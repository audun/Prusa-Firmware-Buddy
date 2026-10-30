#include "startup_timeline.hpp"

#include <inttypes.h>

#include <bsod/bsod.h>
#include <feature/print_status_message/print_status_message.hpp>
#include <logging/log.hpp>
#include <marlin_server.hpp>
#include <metric.h>
#include <metric_handlers.h>
#include <timing.h>

LOG_COMPONENT_REF(MarlinServer);

namespace buddy::startup_timeline {

namespace {

    /// Short, untranslated phase names for the metric and the log.
    /// Each name must be at most 20 characters to fit into the metric point.
    const char *phase_name(PrintStatusMessage::Type type) {
        switch (type) {
        case PrintStatusMessage::none:
            return "none";
        case PrintStatusMessage::custom:
            return "custom";
        case PrintStatusMessage::homing:
            return "homing";
        case PrintStatusMessage::homing_retrying:
            return "homing_retrying";
        case PrintStatusMessage::homing_refining:
            return "homing_refining";
        case PrintStatusMessage::recalibrating_home:
            return "recalibrating_home";
        case PrintStatusMessage::calibrating_axis:
            return "calibrating_axis";
        case PrintStatusMessage::probing_bed:
            return "probing_bed";
        case PrintStatusMessage::additional_probing:
            return "additional_probing";
        case PrintStatusMessage::dwelling:
            return "dwelling";
        case PrintStatusMessage::absorbing_heat:
            return "absorbing_heat";
        case PrintStatusMessage::waiting_for_hotend_temp:
            return "wait_hotend";
        case PrintStatusMessage::waiting_for_bed_temp:
            return "wait_bed";
#if ENABLED(PROBE_CLEANUP_SUPPORT)
        case PrintStatusMessage::nozzle_cleaning:
            return "nozzle_cleaning";
#endif
#if HAS_PRINT_SHEET_DETECTION()
        case PrintStatusMessage::detecting_steel_sheet:
            return "sheet_detect";
#endif
#if HAS_SPOOL_JOIN()
        case PrintStatusMessage::spool_joined:
            return "spool_joined";
        case PrintStatusMessage::joining_spool:
            return "joining_spool";
#endif
#if HAS_CHAMBER_API()
        case PrintStatusMessage::waiting_for_chamber_temp:
            return "wait_chamber";
#endif
#if HAS_AUTO_RETRACT()
        case PrintStatusMessage::auto_retracting:
            return "auto_retract";
#endif
#if HAS_CHAMBER_VENTS()
        case PrintStatusMessage::opening_chamber_vents:
            return "vents_open";
        case PrintStatusMessage::closing_chamber_vents:
            return "vents_close";
#endif
#if HAS_TOOL_OFFSET_SENSOR()
        case PrintStatusMessage::tool_offset_calibrating:
            return "tool_offset_cal";
#endif
#if HAS_NOZZLE_CLEANER()
        case PrintStatusMessage::nozzle_cleaner:
            return "nozzle_cleaner";
#endif
#if HAS_NOZZLE_CLEANER_LITE()
        case PrintStatusMessage::nozzle_cleaner_lite:
            return "cleaner_lite";
#endif
        case PrintStatusMessage::_cnt:
            break;
        }
        return "?";
    }

} // namespace

StartupTimeline &instance() {
    static StartupTimeline instance;
    return instance;
}

void on_print_start() {
    debug_assert(marlin_server::is_marlin_server_thread());

    if (!are_metrics_enabled()) {
        instance().disarm();
        return;
    }

    instance().arm(ticks_ms());
    log_info(MarlinServer, "pstart +0ms armed");
}

void on_print_end() {
    instance().disarm();
}

void on_command_dequeued(const char *command) {
    auto &tl = instance();
    if (!tl.is_armed()) {
        return;
    }

    const uint32_t t = tl.elapsed_ms(ticks_ms());
    const int32_t index = tl.on_command();
    if (index < 0) {
        return;
    }

    METRIC_DEF(metric, "pstart_gcode", METRIC_VALUE_CUSTOM, 0, METRIC_ENABLED);
    // Sanitizing is the only non-trivial work here, skip it when the point would be dropped anyway
    if (are_metrics_enabled() && metric_record_is_due(&metric)) {
        char buf[StartupTimeline::max_cmd_chars + 1];
        StartupTimeline::sanitize_command(buf, command);
        metric_record_custom(&metric, " t=%" PRIu32 "i,i=%" PRIi32 "i,c=\"%s\"", t, index, buf);
    }
    log_info(MarlinServer, "pstart +%" PRIu32 "ms gcode %" PRIi32 " %s", t, index, command);
}

void on_phase(uint8_t type_raw) {
    auto &tl = instance();
    if (!tl.is_armed()) {
        return;
    }

    const uint32_t t = tl.elapsed_ms(ticks_ms());
    const char *name = phase_name(static_cast<PrintStatusMessage::Type>(type_raw));

    METRIC_DEF(metric, "pstart_phase", METRIC_VALUE_CUSTOM, 0, METRIC_ENABLED);
    metric_record_custom(&metric, " t=%" PRIu32 "i,p=%ui,n=\"%s\"", t, unsigned(type_raw), name);
    log_info(MarlinServer, "pstart +%" PRIu32 "ms phase %u %s", t, unsigned(type_raw), name);
}

void note(const char *text) {
    auto &tl = instance();
    if (!tl.is_armed()) {
        return;
    }

    const uint32_t t = tl.elapsed_ms(ticks_ms());

    METRIC_DEF(metric, "pstart_note", METRIC_VALUE_CUSTOM, 0, METRIC_ENABLED);
    metric_record_custom(&metric, " t=%" PRIu32 "i,n=\"%s\"", t, text);
    log_info(MarlinServer, "pstart +%" PRIu32 "ms note %s", t, text);
}

} // namespace buddy::startup_timeline
