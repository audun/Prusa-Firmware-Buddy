#include "startup_timeline.hpp"

namespace buddy {

void StartupTimeline::arm(uint32_t now_ms) {
    t0_ms_ = now_ms;
    command_count_ = 0;
    armed_ = true;
}

void StartupTimeline::disarm() {
    armed_ = false;
}

uint32_t StartupTimeline::elapsed_ms(uint32_t now_ms) const {
    return now_ms - t0_ms_;
}

int32_t StartupTimeline::on_command() {
    if (!armed_) {
        return -1;
    }

    const int32_t index = command_count_++;
    if (command_count_ >= max_commands) {
        armed_ = false;
    }
    return index;
}

size_t StartupTimeline::sanitize_command(std::span<char> dst, const char *src) {
    if (dst.empty()) {
        return 0;
    }

    while (*src == ' ') {
        src++;
    }

    size_t len = 0;
    for (; len + 1 < dst.size(); src++) {
        const unsigned char c = *src;
        if (c == '\0' || c == '\n' || c == '\r' || c == ';') {
            break;
        }

        if (c == '"' || c == '\\') {
            dst[len++] = '\'';
        } else if (c < 0x20 || c > 0x7e) {
            dst[len++] = '?';
        } else {
            dst[len++] = static_cast<char>(c);
        }
    }

    while (len > 0 && dst[len - 1] == ' ') {
        len--;
    }

    dst[len] = '\0';
    return len;
}

} // namespace buddy
