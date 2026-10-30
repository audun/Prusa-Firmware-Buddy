/// @file
#pragma once

#include <option/has_pre_print_prep.h>
#include "prepared_state.hpp"

static_assert(HAS_PRE_PRINT_PREP());

/// Pre-print preparation (M1706): homing with sheet detection, nozzle
/// cleanup and mesh verification while idle, so that a print started soon
/// afterwards has nothing left to do before the purge line.
///
/// All functions are to be called from the Marlin thread only.
namespace buddy::pre_print_prep {

struct RunArgs {
    /// Show the "bed clear and sheet in place?" question first
    bool ask_bed_clear = true;
    /// Run the slicer's nozzle-cleanup taps before verifying the mesh
    bool cleanup = true;
    /// Also tap the Z homing point during the mesh verification (G29 P11 Z)
    bool include_home_point = false;
    /// Tap the check points even when the mesh is fresh (G29 P11 M)
    bool force_measure = false;
};

/// Runs the preparation (question, homing, cleanup, mesh verification) in
/// its own FSM with a Stop button. Blocks until done.
/// On success the printer is left in the prepared state.
void run(const RunArgs &args);

/// Re-evaluates the state against position trust, nozzle target and age
/// first, so a caller never sees a stale "prepared".
bool is_prepared();

/// Ends the prepared state (no-op when not prepared). Also clears the mesh
/// freshness so that a stale mesh cannot be reused without verification.
void clear(ClearReason reason);

/// Periodic re-evaluation, called from the marlin server loop.
void step();

} // namespace buddy::pre_print_prep
