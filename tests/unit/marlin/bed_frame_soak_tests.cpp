#include <module/temperature/bed_frame_soak.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using namespace bed_frame_soak;

TEST_CASE("bed_frame_soak rate") {
    CHECK_THAT(rate_per_s(60), WithinAbs(0.12f, 1e-6f));
    CHECK_THAT(rate_per_s(100), WithinAbs(0.06f, 1e-6f));
    CHECK_THAT(rate_per_s(60, 2.0f), WithinAbs(0.24f, 1e-6f));
}

TEST_CASE("bed_frame_soak seconds_remaining") {
    SECTION("within threshold is done") {
        CHECK(seconds_remaining(60, 59.5f, 60) == 0.0f);
        CHECK(seconds_remaining(60, 60.5f, 60) == 0.0f);
    }
    SECTION("linear slew at 60 C") {
        // 2.4 C above the threshold at 0.12 C/s is 20 s
        CHECK_THAT(seconds_remaining(60, 60 - 1 - 2.4f, 60), WithinAbs(20.0f, 0.01f));
        // from a cold frame, roughly the documented 150 s class
        CHECK_THAT(seconds_remaining(60, 25, 60), WithinAbs((35.0f - 1.0f) / 0.12f, 0.01f));
    }
    SECTION("speedup halves the time") {
        CHECK_THAT(seconds_remaining(60, 25, 60, 2.0f), WithinAbs((35.0f - 1.0f) / 0.24f, 0.01f));
    }
    SECTION("non-positive rate never predicts a negative or infinite time") {
        CHECK(seconds_remaining(60, 25, 200) == 0.0f);
    }
}
