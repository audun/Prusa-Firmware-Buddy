#include <catch2/catch_test_macros.hpp>

#include <array>

#include <mesh_verify_points.hpp>

using namespace mesh_verify;

namespace {

// CORE One+ mesh: mirrors MESH_MIN_* (integer division included), MESH_MAX_* and the 21x21 grid
constexpr int x_bed_size = 250;
constexpr int y_bed_size = 220;
constexpr int grid_points = 21;
constexpr float mesh_min_x = -(x_bed_size / (grid_points - 2 - 1)) + 15;
constexpr float mesh_min_y = -(y_bed_size / (grid_points - 2 - 1)) + 15;
constexpr float mesh_max_x = x_bed_size - mesh_min_x;
constexpr float mesh_max_y = y_bed_size - mesh_min_y;

constexpr GridGeometry coreone {
    .min_x = mesh_min_x,
    .min_y = mesh_min_y,
    .x_dist = (mesh_max_x - mesh_min_x) / (grid_points - 1),
    .y_dist = (mesh_max_y - mesh_min_y) / (grid_points - 1),
    .points_x = grid_points,
    .points_y = grid_points,
    .border = 1,
    .major_step = 3,
};

// Major X positions: 14.3 51.2 88.1 125.0 161.9 198.8 235.7 (indices 1, 4, ..., 19)
// Major Y positions: 13.7 45.8 77.9 110.0 142.1 174.2 206.3

constexpr IndexRange range(int x0, int x1, int y0, int y1) {
    return IndexRange { static_cast<int8_t>(x0), static_cast<int8_t>(x1), static_cast<int8_t>(y0), static_cast<int8_t>(y1) };
}

} // namespace

TEST_CASE("mesh_verify major_index_range", "[mesh_verify]") {
    SECTION("whole bed") {
        CHECK(major_index_range(coreone, { 0, 0, x_bed_size, y_bed_size }) == range(1, 19, 1, 19));
    }
    SECTION("huge rect") {
        CHECK(major_index_range(coreone, { -1e30f, -1e30f, 1e30f, 1e30f }) == range(1, 19, 1, 19));
    }
    SECTION("no major point inside") {
        CHECK(major_index_range(coreone, { 20, 20, 30, 30 }).empty());
    }
    SECTION("major points only on one axis") {
        CHECK(major_index_range(coreone, { 20, 40, 30, 120 }).empty());
    }
    SECTION("2x2") {
        CHECK(major_index_range(coreone, { 85, 75, 130, 115 }) == range(7, 10, 7, 10));
    }
    SECTION("3x3") {
        CHECK(major_index_range(coreone, { 50, 45, 130, 115 }) == range(4, 10, 4, 10));
    }
    SECTION("G29 P1 probe area of a small centred print") {
        // Print area 80..140 enlarged by one major step on each side
        const float dx = coreone.x_dist * coreone.major_step;
        const float dy = coreone.y_dist * coreone.major_step;
        CHECK(major_index_range(coreone, { 80 - dx, 80 - dy, 140 + dx, 140 + dy }) == range(4, 13, 7, 13));
    }
}

TEST_CASE("mesh_verify IndexRange::covers", "[mesh_verify]") {
    const auto r = range(4, 13, 7, 13);
    CHECK(r.covers(r));
    CHECK(range(1, 19, 1, 19).covers(r));
    CHECK_FALSE(r.covers(range(1, 19, 1, 19)));
    CHECK_FALSE(r.covers(range(7, 16, 7, 13)));
    CHECK_FALSE(r.covers(range(4, 13, 4, 10)));
    CHECK(r.covers(IndexRange {}));
    CHECK_FALSE(IndexRange {}.covers(r));
    CHECK_FALSE(IndexRange {}.covers(IndexRange {}));
}

TEST_CASE("mesh_verify select_check_points", "[mesh_verify]") {
    std::array<GridPoint, 5> out {};

    SECTION("2x2, four points in loop order") {
        REQUIRE(select_check_points(range(7, 10, 7, 10), 4, out, 3) == 4);
        CHECK(out[0] == GridPoint { 7, 10 });
        CHECK(out[1] == GridPoint { 10, 10 });
        CHECK(out[2] == GridPoint { 10, 7 });
        CHECK(out[3] == GridPoint { 7, 7 });
    }
    SECTION("three points") {
        REQUIRE(select_check_points(range(7, 10, 7, 10), 3, out, 3) == 3);
        CHECK(out[0] == GridPoint { 7, 10 });
        CHECK(out[1] == GridPoint { 10, 10 });
        CHECK(out[2] == GridPoint { 10, 7 });
    }
    SECTION("2x2 has no distinct centre") {
        CHECK(select_check_points(range(7, 10, 7, 10), 5, out, 3) == 4);
    }
    SECTION("3x3, centre is the middle index") {
        REQUIRE(select_check_points(range(4, 10, 4, 10), 5, out, 3) == 5);
        CHECK(out[4] == GridPoint { 7, 7 });
    }
    SECTION("centre snaps to a major index") {
        // midpoint of 4..13 is 8.5, nearest major index is 7
        REQUIRE(select_check_points(range(4, 13, 7, 13), 5, out, 3) == 5);
        CHECK(out[0] == GridPoint { 4, 13 });
        CHECK(out[1] == GridPoint { 13, 13 });
        CHECK(out[2] == GridPoint { 13, 7 });
        CHECK(out[3] == GridPoint { 4, 7 });
        CHECK(out[4] == GridPoint { 7, 10 });
    }
    SECTION("single column, the centre takes a dropped duplicate's slot") {
        REQUIRE(select_check_points(range(7, 7, 4, 10), 4, out, 3) == 3);
        CHECK(out[0] == GridPoint { 7, 10 });
        CHECK(out[1] == GridPoint { 7, 4 });
        CHECK(out[2] == GridPoint { 7, 7 });
        CHECK(select_check_points(range(7, 7, 4, 10), 5, out, 3) == 3);
        CHECK(select_check_points(range(7, 7, 4, 10), 2, out, 3) == 2);
    }
    SECTION("single column of two points") {
        REQUIRE(select_check_points(range(7, 7, 4, 7), 4, out, 3) == 2);
        CHECK(out[0] == GridPoint { 7, 7 });
        CHECK(out[1] == GridPoint { 7, 4 });
    }
    SECTION("single point") {
        REQUIRE(select_check_points(range(10, 10, 10, 10), 5, out, 3) == 1);
        CHECK(out[0] == GridPoint { 10, 10 });
    }
    SECTION("empty range") {
        CHECK(select_check_points(IndexRange {}, 4, out, 3) == 0);
    }
    SECTION("output smaller than requested") {
        std::array<GridPoint, 2> small {};
        REQUIRE(select_check_points(range(7, 10, 7, 10), 4, small, 3) == 2);
        CHECK(small[0] == GridPoint { 7, 10 });
        CHECK(small[1] == GridPoint { 10, 10 });
    }
}

static_assert(major_index_range(coreone, { 0, 0, x_bed_size, y_bed_size }) == range(1, 19, 1, 19));
