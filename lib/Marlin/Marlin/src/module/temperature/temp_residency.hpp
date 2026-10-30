/// @file
#pragma once

#include <cmath>
#include <cstdint>

/// Tracks whether a heater has settled on its target: the temperature has
/// stayed within the window for the residency time.
///
/// The timer starts when the temperature first comes within `window` of the
/// target and restarts only when it leaves the wider `hysteresis` band, so a
/// heater oscillating around the target keeps its residency. A target of 0
/// or below counts as reached. Drive it from the thermal tick with
/// `update()` and reset it whenever the target changes.
class TempResidency {

public:
    struct Config {
        float window; ///< (°C) distance from target that starts the timer
        float hysteresis; ///< (°C) distance from target that restarts the timer
        uint32_t residency_ms; ///< time the temperature has to stay inside the band
    };

    constexpr explicit TempResidency(const Config &config)
        : config_(config) {}

    /// Call on every temperature reading.
    void update(float target, float current, uint32_t now_ms) {
        const float diff = std::abs(target - current);

        if (!start_ms_ && diff < config_.window) {
            start_ms_ = now_ms;
        } else if (diff > config_.hysteresis) {
            start_ms_ = 0;
        }

        // Same as Marlin's ELAPSED(), wraparound-safe
        reached_ = target <= 0
            || (start_ms_ && static_cast<int32_t>(now_ms - (start_ms_ + config_.residency_ms)) >= 0);
    }

    /// Call when the target changes; the residency has to be earned again.
    void reset() {
        start_ms_ = 0;
        reached_ = false;
    }

    bool reached() const {
        return reached_;
    }

    /// Timestamp when the temperature entered the window, 0 while outside it.
    uint32_t start_ms() const {
        return start_ms_;
    }

private:
    Config config_;

    /// 0 while outside the window. 0 is also a valid timestamp; if the window is
    /// entered at exactly 0 ms the next tick re-arms it, which is harmless.
    uint32_t start_ms_ = 0;
    bool reached_ = false;
};
