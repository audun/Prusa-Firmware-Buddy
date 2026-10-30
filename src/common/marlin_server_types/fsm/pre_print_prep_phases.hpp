/// @file
#pragma once

#include <marlin_server_types/client_response.hpp>

enum class PhasePrePrintPrep : PhaseUnderlyingType {
    ask_bed_clear,
    running,
    stopped,
    failed,
    _last = failed,
};

constexpr inline ClientFSM client_fsm_from_phase(PhasePrePrintPrep) { return ClientFSM::PrePrintPrep; }

namespace ClientResponses {
inline constexpr EnumArray<PhasePrePrintPrep, PhaseResponses, CountPhases<PhasePrePrintPrep>()> pre_print_prep_responses {
    { PhasePrePrintPrep::ask_bed_clear, { Response::Yes, Response::No } },
    { PhasePrePrintPrep::running, { Response::Stop } },
    { PhasePrePrintPrep::stopped, { Response::Continue } },
    { PhasePrePrintPrep::failed, { Response::Continue } },
};
}

/// Data passed with PhasePrePrintPrep::running
struct PrePrintPrepData {
    enum class Step : uint8_t {
        homing,
        cleaning,
        verifying,
    };

    Step step = Step::homing;

    /// Incremented whenever the print status message changes, so that the
    /// GUI re-reads it (the FSM is only redrawn when its data changes)
    uint8_t status_seq = 0;

    bool operator==(const PrePrintPrepData &) const = default;
};
