#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>
#include <string_view>

#include <feature/startup_timeline/startup_timeline.hpp>

using buddy::StartupTimeline;

namespace {

std::string_view sanitized(std::span<char> buf, const char *src) {
    const size_t len = StartupTimeline::sanitize_command(buf, src);
    REQUIRE(len == strlen(buf.data()));
    return std::string_view(buf.data(), len);
}

} // namespace

TEST_CASE("startup_timeline::not_armed", "[startup_timeline]") {
    StartupTimeline tl;
    CHECK(!tl.is_armed());
    CHECK(tl.on_command() == -1);
    CHECK(!tl.is_armed());
}

TEST_CASE("startup_timeline::elapsed", "[startup_timeline]") {
    StartupTimeline tl;

    tl.arm(1000);
    CHECK(tl.is_armed());
    CHECK(tl.elapsed_ms(1250) == 250);

    tl.arm(0xFFFFFF00);
    CHECK(tl.elapsed_ms(0x10) == 0x110);
}

TEST_CASE("startup_timeline::command_cap", "[startup_timeline]") {
    StartupTimeline tl;
    tl.arm(0);

    for (int32_t i = 0; i < StartupTimeline::max_commands - 1; i++) {
        CHECK(tl.on_command() == i);
        CHECK(tl.is_armed());
    }

    CHECK(tl.on_command() == StartupTimeline::max_commands - 1);
    CHECK(!tl.is_armed());
    CHECK(tl.on_command() == -1);
}

TEST_CASE("startup_timeline::disarm_rearm", "[startup_timeline]") {
    StartupTimeline tl;
    tl.arm(0);
    CHECK(tl.on_command() == 0);
    CHECK(tl.on_command() == 1);

    tl.disarm();
    CHECK(!tl.is_armed());
    CHECK(tl.on_command() == -1);

    tl.disarm();
    CHECK(!tl.is_armed());

    tl.arm(5);
    CHECK(tl.on_command() == 0);
}

TEST_CASE("startup_timeline::sanitize_command", "[startup_timeline]") {
    std::array<char, StartupTimeline::max_cmd_chars + 1> buf;

    SECTION("plain") {
        CHECK(StartupTimeline::sanitize_command(buf, "G28") == 3);
        CHECK(std::string_view(buf.data()) == "G28");
    }

    SECTION("leading spaces and comment") {
        CHECK(sanitized(buf, "  M109 R170 ; wait") == "M109 R170");
    }

    SECTION("line end") {
        CHECK(sanitized(buf, "G1 X1\n") == "G1 X1");
        CHECK(sanitized(buf, "G1 X2\r\n") == "G1 X2");
    }

    SECTION("line protocol escapes") {
        CHECK(sanitized(buf, "M117 say \"hi\"\\") == "M117 say 'hi''");
    }

    SECTION("non-printable") {
        CHECK(sanitized(buf, "M117 \x01\xff") == "M117 ??");
    }

    SECTION("truncation") {
        std::array<char, StartupTimeline::max_cmd_chars + 2> guarded;
        guarded.fill(0x55);
        const char *long_cmd = "G29 P9 X208 Y-2.5 W32 H4 C0123456789ABCD";
        REQUIRE(strlen(long_cmd) == 40);

        const size_t len = StartupTimeline::sanitize_command(std::span(guarded.data(), StartupTimeline::max_cmd_chars + 1), long_cmd);
        CHECK(len == StartupTimeline::max_cmd_chars);
        CHECK(std::string_view(guarded.data()) == "G29 P9 X208 Y-2.5 W3");
        CHECK(guarded[StartupTimeline::max_cmd_chars] == '\0');
        CHECK(guarded[StartupTimeline::max_cmd_chars + 1] == 0x55);
    }

    SECTION("empty") {
        CHECK(sanitized(buf, "") == "");
        CHECK(sanitized(buf, "   ") == "");
        CHECK(sanitized(buf, "; only comment") == "");
    }

    SECTION("single byte buffer") {
        std::array<char, 1> tiny { 'x' };
        CHECK(StartupTimeline::sanitize_command(tiny, "G28") == 0);
        CHECK(tiny[0] == '\0');
    }

    SECTION("empty buffer") {
        CHECK(StartupTimeline::sanitize_command(std::span<char>(), "G28") == 0);
    }
}
