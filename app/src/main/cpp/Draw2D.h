#ifndef ASTRAPUGNA_DRAW2D_H
#define ASTRAPUGNA_DRAW2D_H

#include <GLES3/gl3.h>
#include <string>
#include <vector>

#include "GameMath.h"

struct Color {
    uint8_t r, g, b, a;
};

inline Color rgba(float r, float g, float b, float a = 1.f) {
    auto c = [](float v) { return (uint8_t) (clampf(v, 0.f, 1.f) * 255.f + 0.5f); };
    return {c(r), c(g), c(b), c(a)};
}

// Multiplies the color's alpha by a.
inline Color fade(Color c, float a) {
    c.a = (uint8_t) (c.a * clampf(a, 0.f, 1.f));
    return c;
}

inline Color shade(Color c, float s) {
    auto m = [s](uint8_t v) { return (uint8_t) clampf(v * s, 0.f, 255.f); };
    return {m(c.r), m(c.g), m(c.b), c.a};
}

inline Color mix(Color a, Color b, float t) {
    auto m = [t](uint8_t x, uint8_t y) { return (uint8_t) lerpf(x, y, clampf(t, 0.f, 1.f)); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}

/*!
 * Immediate-mode batched renderer for flat-colored 2D shapes and a built-in bitmap font.
 * Everything is accumulated into one vertex buffer and drawn as triangles on flush().
 */
class Draw2D {
public:
    bool init();
    void shutdown();

    // Sets up a y-down orthographic view. Flushes pending geometry first.
    void setView(float left, float top, float right, float bottom);
    void flush();

    void rect(float x, float y, float w, float h, Color c);
    void rect(const Rectf &r, Color c) { rect(r.x, r.y, r.w, r.h, c); }
    void rectOutline(float x, float y, float w, float h, float thick, Color c);
    void rectOutline(const Rectf &r, float thick, Color c) { rectOutline(r.x, r.y, r.w, r.h, thick, c); }
    void tri(Vec2 a, Vec2 b, Vec2 c, Color col);
    void quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color col);
    void quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color ca, Color cb, Color cc, Color cd);
    void circle(Vec2 c, float r, Color col, int segments = 20);
    void ring(Vec2 c, float r, float thick, Color col, int segments = 24);
    void line(Vec2 a, Vec2 b, float thick, Color col);
    void polygon(Vec2 c, float r, int sides, float rotation, Color col);
    // Rectangle of half-size (hx, hy) rotated to face along dir (unit vector).
    void orientedRect(Vec2 c, Vec2 dir, float hx, float hy, Color col);

    // px is the size of one font pixel; glyphs are 5x7 font pixels with 1 pixel spacing.
    void text(const std::string &s, float x, float y, float px, Color col);
    void textCentered(const std::string &s, float cx, float y, float px, Color col);
    static float textWidth(const std::string &s, float px);

private:
    struct V {
        float x, y;
        uint8_t r, g, b, a;
    };

    void push(float x, float y, Color c) { verts_.push_back({x, y, c.r, c.g, c.b, c.a}); }

    std::vector<V> verts_;
    GLuint program_ = 0, vbo_ = 0, vao_ = 0;
    GLint uMatrix_ = -1;
    float matrix_[16] = {};
};

#endif //ASTRAPUGNA_DRAW2D_H
