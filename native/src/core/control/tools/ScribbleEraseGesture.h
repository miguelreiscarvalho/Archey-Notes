#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "model/Point.h"

namespace xoj::input {
/** Deliberate repeated scrubbing, measured in document coordinates, not pixels. */
struct ScribbleEraseGesture {
    double axisX = 1, axisY = 0, span = 0;
    std::vector<std::pair<size_t, size_t>> passes;

    static std::optional<ScribbleEraseGesture> detect(const std::vector<Point>& points) {
        if (points.size() < 6 || points.size() > 12000) return std::nullopt;
        double mx = 0, my = 0;
        for (const auto& p : points) {
            if (!std::isfinite(p.x) || !std::isfinite(p.y)) return std::nullopt;
            mx += p.x; my += p.y;
        }
        const double sampleCount = static_cast<double>(points.size());
        mx /= sampleCount; my /= sampleCount;
        double xx = 0, xy = 0, yy = 0;
        for (const auto& p : points) {
            const auto x = p.x - mx, y = p.y - my;
            xx += x*x; xy += x*y; yy += y*y;
        }
        // Principal direction allows diagonal/vertical scratch-out gestures too.
        const double angle = .5 * std::atan2(2*xy, xx-yy);
        ScribbleEraseGesture result;
        result.axisX = std::cos(angle); result.axisY = std::sin(angle);
        const auto project = [&](const Point& p) { return p.x*result.axisX + p.y*result.axisY; };
        const auto across = [&](const Point& p) { return -p.x*result.axisY + p.y*result.axisX; };
        double lo = project(points.front()), hi = lo;
        double minorLo = across(points.front()), minorHi = minorLo, length = 0;
        for (size_t i = 0; i < points.size(); ++i) {
            lo = std::min(lo, project(points[i])); hi = std::max(hi, project(points[i]));
            minorLo = std::min(minorLo, across(points[i])); minorHi = std::max(minorHi, across(points[i]));
            if (i) length += std::hypot(points[i].x-points[i-1].x, points[i].y-points[i-1].y);
        }
        result.span = hi-lo;
        const double breadth = minorHi-minorLo;
        if (!std::isfinite(length) || !std::isfinite(result.span) || result.span < 20 || result.span > 1500 ||
            breadth < 2 || breadth > result.span*.65 || length < result.span*4.5)
            return std::nullopt;
        const double minimumRun = std::max(6.0, result.span*.4);
        size_t start = 0, extremeIndex = 0;
        double extreme = project(points.front());
        int direction = 0;
        for (size_t i = 1; i < points.size(); ++i) {
            const auto value = project(points[i]);
            if (!direction) {
                if (std::abs(value-project(points[start])) < minimumRun) continue;
                direction = value > project(points[start]) ? 1 : -1;
                extreme = value; extremeIndex = i;
            } else if ((value-extreme)*direction > 0) {
                extreme = value; extremeIndex = i;
            } else if ((extreme-value)*direction >= minimumRun) {
                result.passes.emplace_back(start, extremeIndex);
                start = extremeIndex; direction = -direction; extreme = value; extremeIndex = i;
            }
        }
        if (std::abs(extreme-project(points[start])) >= minimumRun)
            result.passes.emplace_back(start, extremeIndex);
        if (result.passes.size() < 5) return std::nullopt;
        return result;
    }
};
}
