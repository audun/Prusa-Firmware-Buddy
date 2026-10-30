#include "stepper_timeout.hpp"

#include <option/board_is_master_board.h>
#include <Configuration.h>
#include <marlin_server.hpp>
#include <module/stepper/indirection.h>

#include <option/has_pre_print_prep.h>
#if HAS_PRE_PRINT_PREP()
    #include <feature/pre_print_prep/pre_print_prep.hpp>
#endif

namespace buddy {

StepperTimeoutManager &stepper_timeout() {
    static StepperTimeoutManager instance;
    return instance;
}

StepperTimeoutManager::StepperTimeoutManager() {
    set_interval_ms(DEFAULT_STEPPER_DEACTIVE_TIME * 1000);
}

void StepperTimeoutManager::reset() {
    timer_.restart(ticks_ms());
}

void StepperTimeoutManager::step() {
    const auto now = ticks_ms();

    // Any activity - running gcode, motors, ...
    // Never disable motors during print (or print pause, when we're not processing anything)
    // nor while the printer is prepared for a print: disabling XY would drop the position trust
    // the preparation established. The heater safety timer bounds this through the nozzle target.
    if (marlin_server::is_processing() || marlin_server::is_printing()
#if HAS_PRE_PRINT_PREP()
        || buddy::pre_print_prep::is_prepared()
#endif
    ) {
        timer_.restart(now);
    }

    if (enabled_ && timer_.check(now)) {
        trigger();
    }
}

void StepperTimeoutManager::trigger() {
    // In case the trigger was called explicitly from somewhere
    timer_.stop();

#if _DEBUG
    // Report steppers being disabled to the user
    // Skip if position not trusted to avoid warnings when position is not important
    if (axes_home_level != AxesHomeLevel::no_axes_homed) {
        marlin_server::set_warning(WarningType::SteppersTimeout);
    }
#endif

#if (ENABLED(XY_LINKED_ENABLE) && (ENABLED(DISABLE_INACTIVE_X) || ENABLED(DISABLE_INACTIVE_Y)))
    disable_XY();
#else
    #if ENABLED(DISABLE_INACTIVE_X)
    disable_X();
    #endif
    #if ENABLED(DISABLE_INACTIVE_Y)
    disable_Y();
    #endif
#endif
#if ENABLED(DISABLE_INACTIVE_Z)
    disable_Z();
#endif
#if ENABLED(DISABLE_INACTIVE_E)
    disable_e_steppers();
#endif
}

} // namespace buddy
