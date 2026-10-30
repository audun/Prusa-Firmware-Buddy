#include "print_status_message_guard.hpp"
#include "print_status_message_mgr.hpp"

#include <optional>

#include <marlin_server.hpp>
#include <bsod/bsod.h>
#include <feature/startup_timeline/startup_timeline.hpp>

PrintStatusMessageGuard::PrintStatusMessageGuard(bool clear_temporary_msg) {
    debug_assert(marlin_server::is_marlin_server_thread());

    auto &psm = print_status_message();
    std::scoped_lock guard(psm.mutex_);

    parent_guard_ = psm.active_guard_;
    record_.id = psm.id_counter_++;

    if (clear_temporary_msg) {
        psm.temporary_message_ = {};
    }

    psm.active_guard_ = this;
}

PrintStatusMessageGuard::~PrintStatusMessageGuard() {
    auto &psm = print_status_message();
    std::optional<PrintStatusMessage::Type> shown_after;

    {
        std::scoped_lock guard(psm.mutex_);

        debug_assert(psm.active_guard_ == this);
        psm.active_guard_ = parent_guard_;

        if (record_.message.type != PrintStatusMessage::none) {
            PrintStatusMessage::Type effective = PrintStatusMessage::none;
            for (auto g = parent_guard_; g; g = g->parent_guard_) {
                if (g->record_.message.type != PrintStatusMessage::none) {
                    effective = g->record_.message.type;
                    break;
                }
            }
            shown_after = effective;
        }
    }

    // Outside of the lock - emitting metrics and logs must not block readers of the message
    if (shown_after) {
        buddy::startup_timeline::on_phase(*shown_after);
    }
}

void PrintStatusMessageGuard::update(const Message &msg) {
    auto &psm = print_status_message();

    // record_ is only written from this thread, reading it without the lock is fine
    const bool type_changed = (msg.type != record_.message.type);

    {
        std::scoped_lock guard(psm.mutex_);

        record_.message = msg;
        psm.add_history_item_nolock(record_);
    }

    if (type_changed) {
        buddy::startup_timeline::on_phase(msg.type);
    }
}
