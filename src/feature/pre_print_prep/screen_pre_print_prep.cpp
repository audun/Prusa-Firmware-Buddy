#include "screen_pre_print_prep.hpp"

#include <array>
#include <cstring>

#include <common/fsm_base_types.hpp>
#include <feature/print_status_message/print_status_message_formatter_buddy.hpp>
#include <feature/print_status_message/print_status_message_mgr.hpp>
#include <guiconfig/GuiDefaults.hpp>
#include <i18n.h>
#include <img_resources.hpp>
#include <standard_frame/frame_prompt.hpp>
#include <standard_frame/frame_text_prompt.hpp>
#include <string_view_utf8.hpp>
#include <utils/string_builder.hpp>

namespace {

constexpr auto txt_title = N_("Prepare for Print");
constexpr auto txt_ask = N_("Is the bed clear and the steel sheet in place?\n\nThe printer will home, detect the sheet, clean the nozzle and check the bed mesh so that the next print can start right away.");
constexpr auto txt_homing = N_("Homing");
constexpr auto txt_cleaning = N_("Nozzle cleaning");
constexpr auto txt_verifying = N_("Checking bed mesh");
constexpr auto txt_stopped = N_("Preparation stopped. The position is no longer known; the next print will home and probe as usual.");
constexpr auto txt_failed = N_("Preparation did not finish. The next print will home and probe as usual.");

/// Running phase: title, the live print status message (homing, detecting
/// the steel sheet, nozzle cleaning, probing, ...) and the Stop button
class FrameRunning : public FramePrompt {
public:
    FrameRunning(window_frame_t *parent, FSMAndPhase fsm_phase, const char *title)
        : FramePrompt(parent, fsm_phase, _(title), string_view_utf8::MakeNULLSTR()) {}

    void update(fsm::PhaseData data) {
        const auto prep_data = fsm::deserialize_data<PrePrintPrepData>(data);

        decltype(text_) new_text {};
        StringBuilder builder { new_text };
        PrintStatusMessageFormatterBuddy::format(builder, print_status_message().current_message().message);

        if (new_text[0] == '\0') {
            // Between the G-codes there is no status message, show the step instead
            text_[0] = '\0';
            info.SetText(_(step_text(prep_data.step)));
            return;
        }

        if (strcmp(text_.data(), new_text.data()) != 0) {
            text_ = new_text;
            // The buffer is recycled, so SetText alone would not notice the change
            info.SetText(string_view_utf8::MakeRAM(text_.data()));
            info.Invalidate();
        }
    }

private:
    static const char *step_text(PrePrintPrepData::Step step) {
        switch (step) {
        case PrePrintPrepData::Step::homing:
            return txt_homing;
        case PrePrintPrepData::Step::cleaning:
            return txt_cleaning;
        case PrePrintPrepData::Step::verifying:
            return txt_verifying;
        }
        return txt_homing;
    }

    std::array<char, 128> text_ {};
};

using Frames = FrameDefinitionList<ScreenPrePrintPrep::FrameStorage,
    FrameDefinition<PhasePrePrintPrep::ask_bed_clear, FramePrompt, PhasePrePrintPrep::ask_bed_clear, txt_title, txt_ask>,
    FrameDefinition<PhasePrePrintPrep::running, FrameRunning, PhasePrePrintPrep::running, txt_title>,
    FrameDefinition<PhasePrePrintPrep::stopped, FrameTextPrompt, PhasePrePrintPrep::stopped, txt_stopped>,
    FrameDefinition<PhasePrePrintPrep::failed, FrameTextPrompt, PhasePrePrintPrep::failed, txt_failed>>;

} // namespace

ScreenPrePrintPrep::ScreenPrePrintPrep()
    : ScreenFSM { N_("PREPARE FOR PRINT"), GuiDefaults::RectScreenNoHeader } {
    header.SetIcon(&img::home_shape_16x16);
    CaptureNormalWindow(inner_frame);
    create_frame();
}

ScreenPrePrintPrep::~ScreenPrePrintPrep() {
    destroy_frame();
}

void ScreenPrePrintPrep::create_frame() {
    Frames::create_frame(frame_storage, get_phase(), &inner_frame);
}

void ScreenPrePrintPrep::destroy_frame() {
    Frames::destroy_frame(frame_storage, get_phase());
}

void ScreenPrePrintPrep::update_frame() {
    Frames::update_frame(frame_storage, get_phase(), fsm_base_data.GetData());
}
