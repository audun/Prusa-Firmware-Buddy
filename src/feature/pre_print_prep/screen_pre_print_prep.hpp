#pragma once

#include <screen_fsm.hpp>
#include <fsm/pre_print_prep_phases.hpp>

class ScreenPrePrintPrep final : public ScreenFSM {
public:
    ScreenPrePrintPrep();
    ~ScreenPrePrintPrep();

    inline PhasePrePrintPrep get_phase() const {
        return GetEnumFromPhaseIndex<PhasePrePrintPrep>(fsm_base_data.GetPhase());
    }

protected:
    void create_frame() final;
    void destroy_frame() final;
    void update_frame() final;
};
