#include <module/temperature/temp_residency.hpp>

#include <catch2/catch_test_macros.hpp>

namespace {
constexpr TempResidency::Config bed_config {
    .window = 1,
    .hysteresis = 3,
    .residency_ms = 5000,
};
} // namespace

TEST_CASE("temp_residency: target 0 is always reached") {
    TempResidency r(bed_config);
    r.update(0, 25, 1000);
    CHECK(r.reached());
    r.update(-1, 25, 2000);
    CHECK(r.reached());
}

TEST_CASE("temp_residency: reached only after the residency time inside the window") {
    TempResidency r(bed_config);
    r.update(60, 40, 1000);
    CHECK_FALSE(r.reached());
    CHECK(r.start_ms() == 0);

    r.update(60, 59.5f, 10000); // enters the window
    CHECK(r.start_ms() == 10000);
    CHECK_FALSE(r.reached());

    r.update(60, 60.2f, 14999);
    CHECK_FALSE(r.reached());

    r.update(60, 60.2f, 15000);
    CHECK(r.reached());

    r.update(60, 59.8f, 60000); // stays reached while inside the band
    CHECK(r.reached());
    CHECK(r.start_ms() == 10000);
}

TEST_CASE("temp_residency: hysteresis keeps the residency, leaving it restarts") {
    TempResidency r(bed_config);
    r.update(60, 60, 1000);
    r.update(60, 60, 6000);
    REQUIRE(r.reached());
    REQUIRE(r.start_ms() == 1000);

    r.update(60, 57.5f, 7000); // 2.5 off, inside hysteresis
    CHECK(r.reached());
    CHECK(r.start_ms() == 1000);

    r.update(60, 56, 8000); // 4 off, outside hysteresis
    CHECK_FALSE(r.reached());
    CHECK(r.start_ms() == 0);

    r.update(60, 58, 9000); // 2 off, back inside hysteresis but not the window
    CHECK_FALSE(r.reached());
    CHECK(r.start_ms() == 0);

    r.update(60, 59.5f, 10000); // back inside the window, timer restarts
    CHECK(r.start_ms() == 10000);
    CHECK_FALSE(r.reached());

    r.update(60, 60, 14999);
    CHECK_FALSE(r.reached());

    r.update(60, 60, 15000);
    CHECK(r.reached());
}

TEST_CASE("temp_residency: reset forgets the residency") {
    TempResidency r(bed_config);
    r.update(60, 60, 1000);
    r.update(60, 60, 6000);
    REQUIRE(r.reached());

    r.reset();
    CHECK_FALSE(r.reached());
    CHECK(r.start_ms() == 0);

    r.update(70, 60, 6001);
    CHECK_FALSE(r.reached());
    CHECK(r.start_ms() == 0);
}

TEST_CASE("temp_residency: reset to target 0 is reached on the next update") {
    TempResidency r(bed_config);
    r.update(60, 60, 1000);
    r.reset();
    CHECK_FALSE(r.reached());

    r.update(0, 60, 1100);
    CHECK(r.reached());
}

TEST_CASE("temp_residency: millis wraparound") {
    TempResidency r(bed_config);
    const uint32_t t0 = 0xFFFFF000u; // 4096 ms before wrap
    r.update(60, 60, t0);
    CHECK(r.start_ms() == t0);
    r.update(60, 60, t0 + 4000); // still before wrap, 4 s in
    CHECK_FALSE(r.reached());
    r.update(60, 60, t0 + 4999); // wrapped past 0, 1 ms short
    CHECK_FALSE(r.reached());
    r.update(60, 60, t0 + 5000);
    CHECK(r.reached());
}

TEST_CASE("temp_residency: zero residency time is reached on entering the window") {
    TempResidency r({ .window = 1, .hysteresis = 3, .residency_ms = 0 });
    r.update(60, 59.5f, 100);
    CHECK(r.reached());
}

TEST_CASE("temp_residency: window equal to hysteresis") {
    TempResidency r({ .window = 3, .hysteresis = 3, .residency_ms = 5000 });
    r.update(60, 57.5f, 1000);
    CHECK(r.start_ms() == 1000);

    r.update(60, 57, 2000); // exactly on the edge keeps the timer
    CHECK(r.start_ms() == 1000);

    r.update(60, 56.5f, 3000);
    CHECK(r.start_ms() == 0);
}
