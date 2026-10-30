#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

/// Grid index math for G29 P11 mesh verification, independent of the printer configuration.
namespace mesh_verify {

/// Mesh geometry (MESH_MIN_*, MESH_*_DIST, GRID_* of the printer config).
struct GridGeometry {
    float min_x, min_y;
    float x_dist, y_dist;
    uint8_t points_x, points_y;
    uint8_t border; ///< GRID_BORDER
    uint8_t major_step; ///< GRID_MAJOR_STEP

    constexpr float xpos(uint8_t i) const { return min_x + i * x_dist; }
    constexpr float ypos(uint8_t i) const { return min_y + i * y_dist; }
};

/// Axis-aligned rectangle, normalized (x1 <= x2, y1 <= y2).
struct Rect {
    float x1, y1, x2, y2;
};

struct GridPoint {
    uint8_t x, y;

    bool operator==(const GridPoint &) const = default;
};

/// Inclusive range of MAJOR grid indices (border + k * major_step) whose
/// positions lie inside a rectangle. empty() when there is none.
struct IndexRange {
    int8_t x0 = 1, x1 = 0, y0 = 1, y1 = 0;

    constexpr bool empty() const { return x1 < x0 || y1 < y0; }

    /// True when every major point of other is also in this range.
    /// An empty range covers nothing; an empty other is covered by any non-empty range.
    constexpr bool covers(const IndexRange &other) const {
        return !empty() && (other.empty() || (x0 <= other.x0 && other.x1 <= x1 && y0 <= other.y0 && other.y1 <= y1));
    }

    bool operator==(const IndexRange &) const = default;
};

/// Major grid points inside r. Mirrors the point set probe_major_points() visits
/// (indices border, border + step, ... below points - border), ignoring probe reachability.
constexpr IndexRange major_index_range(const GridGeometry &g, const Rect &r) {
    IndexRange out;
    bool any_x = false, any_y = false;
    for (int i = g.border; i < g.points_x - g.border; i += g.major_step) {
        const float x = g.xpos(i);
        if (x < r.x1 || x > r.x2) {
            continue;
        }
        if (!any_x) {
            out.x0 = i;
            any_x = true;
        }
        out.x1 = i;
    }
    for (int j = g.border; j < g.points_y - g.border; j += g.major_step) {
        const float y = g.ypos(j);
        if (y < r.y1 || y > r.y2) {
            continue;
        }
        if (!any_y) {
            out.y0 = j;
            any_y = true;
        }
        out.y1 = j;
    }
    if (!any_x || !any_y) {
        return IndexRange {};
    }
    return out;
}

/// Check points for a range: the far-left, far-right, near-right and near-left
/// corners (a loop without crossings), then the major point nearest to the centre.
/// Duplicates (single row or column ranges) are dropped, so fewer points than
/// requested can be returned. requested is clamped to out.size().
/// @returns number of points written to out.
constexpr size_t select_check_points(const IndexRange &r, uint8_t requested, std::span<GridPoint> out, uint8_t major_step) {
    if (r.empty() || out.empty() || major_step == 0) {
        return 0;
    }
    const auto snap = [&](int v, int lo, int hi) {
        const int i = lo + ((v - lo + major_step / 2) / major_step) * major_step;
        return static_cast<uint8_t>(std::clamp(i, lo, hi));
    };
    const GridPoint candidates[] = {
        { static_cast<uint8_t>(r.x0), static_cast<uint8_t>(r.y1) },
        { static_cast<uint8_t>(r.x1), static_cast<uint8_t>(r.y1) },
        { static_cast<uint8_t>(r.x1), static_cast<uint8_t>(r.y0) },
        { static_cast<uint8_t>(r.x0), static_cast<uint8_t>(r.y0) },
        { snap((r.x0 + r.x1) / 2, r.x0, r.x1), snap((r.y0 + r.y1) / 2, r.y0, r.y1) },
    };
    size_t n = 0;
    const size_t limit = std::min<size_t>(requested, out.size());
    for (const auto &c : candidates) {
        if (n >= limit) {
            break;
        }
        if (std::find(out.begin(), out.begin() + n, c) != out.begin() + n) {
            continue;
        }
        out[n++] = c;
    }
    return n;
}

} // namespace mesh_verify
