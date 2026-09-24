#ifndef ASTRAPUGNA_GAMEMATH_H
#define ASTRAPUGNA_GAMEMATH_H

#include <algorithm>
#include <cmath>
#include <cstdint>

constexpr float kPi = 3.14159265358979f;

struct Vec2 {
    float x = 0, y = 0;

    Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2 operator/(float s) const { return {x / s, y / s}; }
    Vec2 &operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    Vec2 &operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
    Vec2 &operator*=(float s) { x *= s; y *= s; return *this; }
};

inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float lengthSq(Vec2 v) { return v.x * v.x + v.y * v.y; }
inline float length(Vec2 v) { return std::sqrt(lengthSq(v)); }
inline float distSq(Vec2 a, Vec2 b) { return lengthSq(a - b); }
inline float dist(Vec2 a, Vec2 b) { return length(a - b); }
inline Vec2 normalize(Vec2 v) {
    float l = length(v);
    return l > 1e-6f ? v / l : Vec2{0, 0};
}
inline Vec2 fromAngle(float a) { return {std::cos(a), std::sin(a)}; }
inline float clampf(float v, float lo, float hi) { return std::min(std::max(v, lo), hi); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline Vec2 lerp(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }

struct Rectf {
    float x = 0, y = 0, w = 0, h = 0;

    bool contains(Vec2 p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
    Vec2 center() const { return {x + w * 0.5f, y + h * 0.5f}; }
};

// Small xorshift RNG so map generation is reproducible from a seed.
struct Rng {
    uint32_t s;

    explicit Rng(uint32_t seed = 1) : s(seed ? seed : 0x9E3779B9u) {}

    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float f() { return (next() & 0xFFFFFF) / 16777216.f; }
    float range(float lo, float hi) { return lo + (hi - lo) * f(); }
    int irange(int lo, int hi) { return lo + (int) (next() % (uint32_t) (hi - lo + 1)); }
};

#endif //ASTRAPUGNA_GAMEMATH_H
