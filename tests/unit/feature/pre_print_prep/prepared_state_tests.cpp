#include <feature/pre_print_prep/prepared_state.hpp>

#include <catch2/catch_test_macros.hpp>

using buddy::pre_print_prep::ClearReason;
using buddy::pre_print_prep::PreparedState;

namespace {
constexpr int16_t target = 205;
constexpr uint32_t t0 = 1000;
} // namespace

TEST_CASE("pre_print_prep: fresh state is not prepared") {
    PreparedState state;
    CHECK_FALSE(state.is_prepared());
    CHECK_FALSE(state.update(t0, true, target, false));
    CHECK_FALSE(state.consume_clear_reason().has_value());
}

TEST_CASE("pre_print_prep: prepared state holds while conditions hold") {
    PreparedState state;
    state.set(t0, target);
    CHECK(state.is_prepared());
    CHECK(state.min_nozzle_target() == target);
    CHECK(state.prepared_since_ms() == t0);
    CHECK(state.update(t0 + 1000, true, target, false));
    CHECK_FALSE(state.consume_clear_reason().has_value());
}

TEST_CASE("pre_print_prep: losing position trust ends the state") {
    PreparedState state;
    state.set(t0, target);

    SECTION("idle") {
        CHECK_FALSE(state.update(t0 + 1000, false, target, false));
    }
    SECTION("printing") {
        CHECK_FALSE(state.update(t0 + 1000, false, target, true));
    }

    CHECK_FALSE(state.is_prepared());
    CHECK(state.consume_clear_reason() == ClearReason::trust_lost);
    // Reported exactly once
    CHECK_FALSE(state.consume_clear_reason().has_value());
}

TEST_CASE("pre_print_prep: nozzle rule") {
    PreparedState state;
    state.set(t0, target);

    SECTION("a higher target keeps the state") {
        CHECK(state.update(t0 + 1000, true, target + 10, false));
        CHECK_FALSE(state.consume_clear_reason().has_value());
    }
    SECTION("a lower target while idle ends the state") {
        CHECK_FALSE(state.update(t0 + 1000, true, target - 1, false));
        CHECK(state.consume_clear_reason() == ClearReason::nozzle_cooled);
    }
    SECTION("a lower target while printing is ignored") {
        CHECK(state.update(t0 + 1000, true, 100, true));
        CHECK_FALSE(state.consume_clear_reason().has_value());
    }
}

TEST_CASE("pre_print_prep: expiry") {
    PreparedState state;

    SECTION("idle") {
        state.set(t0, target);
        CHECK(state.update(t0 + PreparedState::max_age_ms - 1, true, target, false));
        CHECK_FALSE(state.update(t0 + PreparedState::max_age_ms, true, target, false));
        CHECK(state.consume_clear_reason() == ClearReason::expired);
    }
    SECTION("tick counter wrap-around") {
        const uint32_t start = 0xFFFFF000;
        state.set(start, target);
        CHECK(state.update(start + 0x2000, true, target, false));
        CHECK_FALSE(state.update(start + PreparedState::max_age_ms, true, target, false));
        CHECK(state.consume_clear_reason() == ClearReason::expired);
    }
    SECTION("suspended while printing") {
        state.set(t0, target);
        CHECK(state.update(t0 + 2 * PreparedState::max_age_ms, true, target, true));
        CHECK_FALSE(state.consume_clear_reason().has_value());
    }
}

TEST_CASE("pre_print_prep: explicit clear") {
    PreparedState state;

    SECTION("clears a prepared state") {
        state.set(t0, target);
        state.clear(ClearReason::print_finished);
        CHECK_FALSE(state.is_prepared());
        CHECK(state.consume_clear_reason() == ClearReason::print_finished);
    }
    SECTION("no-op when not prepared") {
        state.clear(ClearReason::print_finished);
        CHECK_FALSE(state.consume_clear_reason().has_value());
    }
}

TEST_CASE("pre_print_prep: set() discards a pending clear reason") {
    PreparedState state;
    state.set(t0, target);
    state.clear(ClearReason::manual);
    state.set(t0 + 10, target);
    CHECK(state.is_prepared());
    CHECK_FALSE(state.consume_clear_reason().has_value());
}
