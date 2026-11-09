#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
namespace xoj::radial {
inline double clampCenter(double value, double extent, double size) {
    return std::clamp(value,std::min(extent/2,size/2+8),std::max(extent/2,extent-size/2-8));
}
inline double smooth(double value) { value=std::clamp(value,0.0,1.0);return value*value*(3-2*value); }
// A light approximately 4% overshoot with zero velocity at both ends. Reversing the
// animation reuses the same curve, so quick repeated taps do not jump.
inline double spring(double value) {
    const double t = std::clamp(value, 0.0, 1.0);
    return smooth(t) + 5.5*t*t*(1-t)*(1-t);
}
inline bool isDrag(double dx,double dy) { return dx*dx+dy*dy>=64; }
// A short fan with nearly touching options, rather than distributing them over a full ring.
inline double optionAngle(double parent, size_t index, size_t count, double radius) {
    double step=2*std::asin(std::min(1.0,54.0/(2*std::max(1.0,radius))));
    return parent+(double(index)-(double(count)-1)/2)*step;
}
} // namespace xoj::radial
