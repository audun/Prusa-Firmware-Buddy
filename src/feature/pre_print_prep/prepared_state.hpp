/// @file
#pragma once

#include <cstdint>
#include <optional>

namespace buddy::pre_print_prep {

/// Why a prepared state ended
enum class ClearReason : uint8_t {
    /// Some axis is no longer precisely homed (motors off, crash, Z align, re-homing, ...)
    trust_lost,
    /// The nozzle target dropped below the target it had when the preparation finished
    nozzle_cooled,
    /// PreparedState::max_age_ms elapsed while idle
    expired,
    /// A print ended; the part is being removed, the bed is no longer known to be clear
    print_finished,
    /// A new preparation is starting
    manual,
};

/// Bookkeeping for the "printer is prepared for a print" state.
///
/// Time is injected so the logic is testable off-target; the firmware glue
/// samples the live printer state and calls update().
class PreparedState {
public:
    /// An idle prepared state is dropped after this long: the user is probably
    /// not about to print and the bed may no longer be clear.
    static constexpr uint32_t max_age_ms = 30 * 60 * 1000;

    /// Enter the prepared state.
    /// @param min_nozzle_target the nozzle target in effect at completion; a
    ///        lower target later (idle cooldown, safety timer) ends the state
    /// @param cleanup_done the preparation ran the nozzle-cleanup taps
    void set(uint32_t now_ms, int16_t min_nozzle_target, bool cleanup_done);

    /// Leave the prepared state. No-op when not prepared.
    void clear(ClearReason reason);

    /// Re-evaluate against the live state. Ends the state when a condition
    /// fails and returns whether it is still prepared.
    /// @param axes_trusted all axes are homed to the precise level
    /// @param nozzle_target current target of the active hotend
    /// @param printing a print is running: the nozzle and age rules are
    ///        suspended because the start G-code owns temperatures and the
    ///        end of the print ends the state anyway
    bool update(uint32_t now_ms, bool axes_trusted, int16_t nozzle_target, bool printing);

    bool is_prepared() const {
        return prepared_;
    }

    /// Prepared, and the preparation ran the nozzle-cleanup taps
    bool is_cleanup_done() const {
        return prepared_ && cleanup_done_;
    }

    int16_t min_nozzle_target() const {
        return min_nozzle_target_;
    }

    uint32_t prepared_since_ms() const {
        return since_ms_;
    }

    /// Reason of the most recent clear() of a prepared state; reset by this
    /// call and by set(). Lets the glue report a clear exactly once.
    std::optional<ClearReason> consume_clear_reason();

private:
    bool prepared_ = false;
    bool cleanup_done_ = false;
    int16_t min_nozzle_target_ = 0;
    uint32_t since_ms_ = 0;
    std::optional<ClearReason> pending_reason_;
};

} // namespace buddy::pre_print_prep
