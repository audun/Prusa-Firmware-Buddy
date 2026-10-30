#include "pre_print_prep.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstring>

#include <bsod/bsod.h>
#include <client_response.hpp>
#include <common/fsm_base_types.hpp>
#include <feature/bedlevel/bedlevel.h>
#include <feature/gcode_exception/gcode_exception.hpp>
#include <feature/print_area.h>
#include <feature/print_status_message/print_status_message_mgr.hpp>
#include <feature/startup_timeline/startup_timeline.hpp>
#include <fsm/pre_print_prep_phases.hpp>
#include <Marlin/src/gcode/gcode.h>
#include <logging/log.hpp>
#include <mapi/parking.hpp>
#include <marlin_server.hpp>
#include <marlin_vars.hpp>
#include <metric.h>
#include <module/motion.h>
#include <module/probe.h>
#include <option/has_auto_retract.h>
#include <option/has_loadcell.h>
#include <option/has_print_sheet_detection.h>
#include <printers.h>
#include <timing.h>
#include <tool/hotend/hotend.hpp>
#include <tool_index.hpp>
#include <utils/publisher.hpp>

static_assert(HAS_LOADCELL());
static_assert(HAS_PRINT_SHEET_DETECTION());
// cleanup_probe() retracts before the taps (pre_nozzle_cleaning_retraction_enable), which
// replaces the slicer's "M302 S155; G1 E-2" that precedes G29 P9 in the start G-code
static_assert(HAS_AUTO_RETRACT());

LOG_COMPONENT_DEF(PrePrintPrep, logging::Severity::info);

namespace buddy::pre_print_prep {

namespace {

    METRIC_DEF(metric_prep, "pre_print_prep", METRIC_VALUE_CUSTOM, 0, METRIC_ENABLED);

    /// Z lift after a Stop [mm]
    constexpr float stop_z_raise = 5.0f;

#if PRINTER_IS_PRUSA_COREONE()
    /// Nozzle-cleanup taps as sent by the PrusaSlicer CORE One start G-code
    constexpr const char *cleanup_gcode = "G29 P9 X208 Y-2.5 W32 H4";
#elif PRINTER_IS_PRUSA_MK4()
    /// Nozzle-cleanup taps as sent by the PrusaSlicer MK4(S) start G-code
    constexpr const char *cleanup_gcode = "G29 P9 X10 Y-4 W32 H4";
#else
    #error "No slicer nozzle-cleanup rectangle known for this printer"
#endif

    PreparedState &prepared_state() {
        static PreparedState instance;
        return instance;
    }

    const char *reason_name(ClearReason reason) {
        switch (reason) {
        case ClearReason::trust_lost:
            return "trust_lost";
        case ClearReason::nozzle_cooled:
            return "nozzle_cooled";
        case ClearReason::expired:
            return "expired";
        case ClearReason::print_finished:
            return "print_finished";
        case ClearReason::manual:
            return "manual";
        }
        bsod_unreachable();
    }

    int16_t active_nozzle_target() {
        const auto tool = PhysicalToolIndex::currently_selected_opt();
        return tool ? Hotend::for_tool(*tool).nozzle_target_temp() : 0;
    }

    bool sample_and_update() {
        return prepared_state().update(ticks_ms(),
            axes_home_level.is_homed(AxisHomeLevel::full),
            active_nozzle_target(),
            marlin_server::is_printing());
    }

    void report_clear_if_any() {
        if (const auto reason = prepared_state().consume_clear_reason()) {
            log_info(PrePrintPrep, "prepared state ended (%s)", reason_name(*reason));
            metric_record_custom(&metric_prep, " s=\"cleared\",r=\"%s\"", reason_name(*reason));
            ubl.clear_mesh_fresh();
        }
    }

    using Step = PrePrintPrepData::Step;

    class Preparation {
    public:
        explicit Preparation(const RunArgs &args)
            : args_(args) {}

        void run() {
            if (args_.ask_bed_clear && !ask()) {
                return;
            }

            // Whatever was prepared before is void the moment we move again
            pre_print_prep::clear(ClearReason::manual);

            switch (execute()) {

            case Outcome::success:
                prepared_state().set(ticks_ms(), active_nozzle_target(), args_.cleanup);
                log_info(PrePrintPrep, "prepared (nozzle target %d)", active_nozzle_target());
                note("done");
                return;

            case Outcome::stopped:
                note("stopped");
                show_final(PhasePrePrintPrep::stopped);
                return;

            case Outcome::failed:
                note("failed");
                show_final(PhasePrePrintPrep::failed);
                return;
            }
        }

    private:
        enum class Outcome {
            success,
            stopped,
            failed,
        };

        bool ask() {
            fsm_change(PhasePrePrintPrep::ask_bed_clear);
            return marlin_server::wait_for_response(PhasePrePrintPrep::ask_bed_clear) == Response::Yes;
        }

        void show_final(PhasePrePrintPrep phase) {
            fsm_change(phase);
            marlin_server::wait_for_response(phase);
        }

        void fsm_change(PhasePrePrintPrep phase, fsm::PhaseData data = {}) {
            marlin_server::fsm_change(phase, data);
        }

        void note(const char *step) {
            const uint32_t elapsed_ms = ticks_ms() - start_ms_;
            log_info(PrePrintPrep, "%s +%" PRIu32 "ms", step, elapsed_ms);
            metric_record_custom(&metric_prep, " s=\"%s\",t=%" PRIu32 "i", step, elapsed_ms);

            // Only recorded when a print start armed the timeline
            char timeline_note[24];
            snprintf(timeline_note, sizeof(timeline_note), "prep:%s", step);
            buddy::startup_timeline::note(timeline_note);
        }

        /// Republishes the running phase when the step or the print status
        /// message changed, so that the GUI shows what is being done
        void refresh_running() {
            const auto status = print_status_message().current_message();
            if (status != last_status_) {
                last_status_ = status;
                ++data_.status_seq;
            }
            data_.step = step_;
            fsm_change(PhasePrePrintPrep::running, fsm::serialize_data(data_));
        }

        Outcome execute() {
            start_ms_ = ticks_ms();
            note("begin");
            refresh_running();

            GCodeExceptionHandler abort_handler {
                GCEHandlerExtent::any_move,
                [] {
                    // The quick stop may have skipped steps
                    set_all_unhomed();
                    // A Stop during a tap may have left the hot nozzle on the sheet
                    mapi::park({ .z = mapi::ParkingPosition::Relative { stop_z_raise } });
                },
            };
            Subscriber idle_watcher { marlin_server::idle_publisher, [this, &abort_handler] {
                                         if (marlin_server::get_response_from_phase(PhasePrePrintPrep::running) == Response::Stop) {
                                             gcode_exceptions().throw_at(&abort_handler);
                                             return;
                                         }
                                         refresh_running();
                                     } };

            // 1. Full precise homing with steel-sheet detection, as the slicer's bare G28 does
            const bool homed = GcodeSuite::G28_no_parser(true, true, true,
                G28Flags {
                    .only_if_needed = false,
                    .can_calibrate = true,
                    .precise = true,
                    .check_sheet = true,
                });
            if (gcode_exceptions().is_unwinding()) {
                return Outcome::stopped;
            }
            if (!homed || !axes_home_level.is_homed(AxisHomeLevel::full)) {
                return Outcome::failed;
            }
            note("homed");

            // 2. Nozzle-cleanup taps, exactly as the start G-code does them
            if (args_.cleanup) {
                step_ = Step::cleaning;
                refresh_running();
                process(cleanup_gcode);
                if (gcode_exceptions().is_unwinding()) {
                    return Outcome::stopped;
                }
                // Outside of a print, Abort in the nozzle-cleaning-failed wizard does not
                // stop anything; it only leaves this flag set (Ignore clears it)
                if (ubl.g29_nozzle_cleaning_failed || !axes_home_level.is_homed(AxisHomeLevel::full)) {
                    return Outcome::failed;
                }
                note("cleaned");
            }

            // 3. Mesh verification over the whole bed, full probe when the mesh is not valid
            step_ = Step::verifying;
            refresh_running();

            // Print init resets Live-Z before every print. The mesh records the probe offset it was
            // taken with, so verify with the offset the print will have or it is invalid there again.
            probe_offset.z = 0;
            marlin_vars().z_offset = 0;

            // The file is not chosen yet; covering the whole bed makes the mesh valid for any print area
            print_area.reset_bounding_rect();

            char verify_gcode[16];
            snprintf(verify_gcode, sizeof(verify_gcode), "G29 P11%s%s", args_.include_home_point ? " Z" : "", args_.force_measure ? " M" : "");
            process(verify_gcode);
            if (gcode_exceptions().is_unwinding()) {
                return Outcome::stopped;
            }
            // Fresh is set by a successful verification or a complete probe only; it stays clear
            // when the probing failed and the user declined to retry
            if (!ubl.mesh_fresh || !axes_home_level.is_homed(AxisHomeLevel::full)) {
                return Outcome::failed;
            }

            for (const char *gcode : { "G29 P3.2", "G29 P3.13", "G29 A" }) {
                process(gcode);
                if (gcode_exceptions().is_unwinding()) {
                    return Outcome::stopped;
                }
            }
            note("mesh_ok");
            return Outcome::success;
        }

        static void process(const char *gcode) {
            // The parser works in place
            char buffer[32];
            strlcpy(buffer, gcode, sizeof(buffer));
            GcodeSuite::process_subcommands_now(buffer);
        }

        const RunArgs args_;
        uint32_t start_ms_ = 0;
        Step step_ = Step::homing;
        PrePrintPrepData data_;
        PrintStatusMessageManager::Record last_status_;
        marlin_server::FSM_Holder holder_ { ClientFSM::PrePrintPrep };
    };

} // namespace

void run(const RunArgs &args) {
    Preparation preparation { args };
    preparation.run();
}

bool is_prepared() {
    const bool result = sample_and_update();
    report_clear_if_any();
    return result;
}

bool is_cleanup_done() {
    return is_prepared() && prepared_state().is_cleanup_done();
}

void clear(ClearReason reason) {
    prepared_state().clear(reason);
    report_clear_if_any();
}

void step() {
    sample_and_update();
    report_clear_if_any();
}

} // namespace buddy::pre_print_prep
