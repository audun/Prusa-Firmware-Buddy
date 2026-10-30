#pragma once

#include <algorithm>
#include <cmath>

/// Model of how the bed frame absorbs heat from the heatbed.
///
/// The frame temperature estimate slews towards the measured bed
/// temperature at a rate that falls with the bed temperature: ~150 s to
/// settle at 60 °C and ~10 min at 100 °C from a cold start.
namespace bed_frame_soak {

/// The soak is finished when the estimate is this close to the bed target
inline constexpr float finish_threshold = 1.0f; // °C

/// Slew rate of the frame estimate, °C per second, for the given bed temperature
inline constexpr float rate_per_s(float bed_celsius, float speedup = 1.0f) {
    return (0.06f + (100.0f - bed_celsius) * 0.0015f) * speedup;
}

/// Predicted time until the soak finishes
/// \param target bed target temperature
/// \param frame_est current frame temperature estimate
/// \param bed_celsius current bed temperature, drives the rate
/// \param speedup rate multiplier (2 with expansion joints gen 2)
/// \returns seconds; 0 when already within the threshold or the rate is not positive
inline float seconds_remaining(float target, float frame_est, float bed_celsius, float speedup = 1.0f) {
    const float gap = std::fabs(target - frame_est) - finish_threshold;
    const float rate = rate_per_s(bed_celsius, speedup);
    if (gap <= 0.0f || rate <= 0.0f) {
        return 0.0f;
    }
    return gap / rate;
}

} // namespace bed_frame_soak
