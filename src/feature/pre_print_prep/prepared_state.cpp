#include "prepared_state.hpp"

#include <utility>

namespace buddy::pre_print_prep {

void PreparedState::set(uint32_t now_ms, int16_t min_nozzle_target, bool cleanup_done) {
    prepared_ = true;
    cleanup_done_ = cleanup_done;
    since_ms_ = now_ms;
    min_nozzle_target_ = min_nozzle_target;
    pending_reason_.reset();
}

void PreparedState::clear(ClearReason reason) {
    if (!prepared_) {
        return;
    }
    prepared_ = false;
    cleanup_done_ = false;
    pending_reason_ = reason;
}

bool PreparedState::update(uint32_t now_ms, bool axes_trusted, int16_t nozzle_target, bool printing) {
    if (!prepared_) {
        return false;
    }

    if (!axes_trusted) {
        clear(ClearReason::trust_lost);
    } else if (!printing && nozzle_target < min_nozzle_target_) {
        clear(ClearReason::nozzle_cooled);
    } else if (!printing && now_ms - since_ms_ >= max_age_ms) {
        clear(ClearReason::expired);
    }
    return prepared_;
}

std::optional<ClearReason> PreparedState::consume_clear_reason() {
    return std::exchange(pending_reason_, std::nullopt);
}

} // namespace buddy::pre_print_prep
