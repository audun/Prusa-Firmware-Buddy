#pragma once

#include <cstdint>
#include <cstddef>
#include <span>

namespace buddy {

/// Records the timeline of a print startup: the moment print_start() was
/// called is t0, every top-level G-code command dequeued and every
/// PrintStatusMessage phase change is stamped relative to it.
///
/// Pure state; emission (metrics, log) is done by the firmware hooks in
/// namespace buddy::startup_timeline. All methods are to be called from the
/// Marlin thread only.
class StartupTimeline {
public:
    /// Commands recorded after arm() before the timeline disarms itself.
    static constexpr uint16_t max_commands = 160;

    /// Longest command text that fits into a metric point together with
    /// the other fields (metric_point_t::value_custom is 48 bytes).
    static constexpr size_t max_cmd_chars = 20;

    /// Starts a new timeline with t0 = now_ms. Re-arming restarts it.
    void arm(uint32_t now_ms);

    /// Stops recording. Idempotent.
    void disarm();

    bool is_armed() const {
        return armed_;
    }

    /// Milliseconds since arm(), wrap-around safe. Only meaningful while armed.
    uint32_t elapsed_ms(uint32_t now_ms) const;

    /// Accounts one dequeued top-level command.
    /// \returns the 0-based command index to report, or -1 when nothing
    /// should be reported (not armed). Disarms itself once max_commands is
    /// reached (the command that reaches the cap is still reported).
    int32_t on_command();

    /// Copies \p src into \p dst as a line-protocol-safe string: stops at
    /// NUL, CR, LF or ';', replaces '"' and '\\' with '\'', non-printable
    /// bytes with '?', trims surrounding spaces and NUL-terminates.
    /// Truncates to dst.size() - 1 characters; writes nothing if \p dst is empty.
    /// \returns number of characters written (excluding the NUL).
    static size_t sanitize_command(std::span<char> dst, const char *src);

private:
    uint32_t t0_ms_ = 0;
    uint16_t command_count_ = 0;
    bool armed_ = false;
};

/// Firmware hooks feeding the StartupTimeline singleton and emitting the
/// pstart_* metrics and MarlinServer log lines. When the timeline is not
/// armed, each hook returns after a single check.
namespace startup_timeline {

    /// Marlin-thread singleton used by the hooks.
    StartupTimeline &instance();

    /// Hook for marlin_server::print_start(): arms the timeline when metrics
    /// are enabled (otherwise the whole feature is inert).
    void on_print_start();

    /// Hook for the end of a print (any exit path).
    void on_print_end();

    /// Hook for GcodeSuite::process_next_command(): \p command is the raw
    /// queue buffer entry.
    void on_command_dequeued(const char *command);

    /// Hook for PrintStatusMessageGuard: \p type is the PrintStatusMessage::Type
    /// that is now effectively shown (as an integer to keep this header light).
    void on_phase(uint8_t type);

    /// Free-form marker, e.g. "mesh_verify:ok". Keep it at most 24 chars,
    /// longer text does not fit into the metric point.
    void note(const char *text);

} // namespace startup_timeline

} // namespace buddy
