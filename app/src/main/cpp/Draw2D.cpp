#include "Draw2D.h"

#include <cstddef>

#include "AndroidOut.h"

static const char *kVertexShader = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
uniform mat4 uMatrix;
out vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = uMatrix * vec4(aPos, 0.0, 1.0);
}
)";

static const char *kFragmentShader = R"(#version 300 es
precision mediump float;
in vec4 vColor;
out vec4 outColor;
void main() {
    outColor = vColor;
}
)";

// 5x7 bitmap font. Each glyph is 7 rows of 5 columns, '1' = lit.
struct GlyphDef {
    char c;
    const char *rows;
};

static const GlyphDef kGlyphs[] = {
        {'A', "01110 10001 10001 11111 10001 10001 10001"},
        {'B', "11110 10001 10001 11110 10001 10001 11110"},
        {'C', "01110 10001 10000 10000 10000 10001 01110"},
        {'D', "11110 10001 10001 10001 10001 10001 11110"},
        {'E', "11111 10000 10000 11110 10000 10000 11111"},
        {'F', "11111 10000 10000 11110 10000 10000 10000"},
        {'G', "01110 10001 10000 10111 10001 10001 01111"},
        {'H', "10001 10001 10001 11111 10001 10001 10001"},
        {'I', "01110 00100 00100 00100 00100 00100 01110"},
        {'J', "00111 00010 00010 00010 00010 10010 01100"},
        {'K', "10001 10010 10100 11000 10100 10010 10001"},
        {'L', "10000 10000 10000 10000 10000 10000 11111"},
        {'M', "10001 11011 10101 10101 10001 10001 10001"},
        {'N', "10001 10001 11001 10101 10011 10001 10001"},
        {'O', "01110 10001 10001 10001 10001 10001 01110"},
        {'P', "11110 10001 10001 11110 10000 10000 10000"},
        {'Q', "01110 10001 10001 10001 10101 10010 01101"},
        {'R', "11110 10001 10001 11110 10100 10010 10001"},
        {'S', "01111 10000 10000 01110 00001 00001 11110"},
        {'T', "11111 00100 00100 00100 00100 00100 00100"},
        {'U', "10001 10001 10001 10001 10001 10001 01110"},
        {'V', "10001 10001 10001 10001 10001 01010 00100"},
        {'W', "10001 10001 10001 10101 10101 10101 01010"},
        {'X', "10001 10001 01010 00100 01010 10001 10001"},
        {'Y', "10001 10001 01010 00100 00100 00100 00100"},
        {'Z', "11111 00001 00010 00100 01000 10000 11111"},
        {'0', "01110 10001 10011 10101 11001 10001 01110"},
        {'1', "00100 01100 00100 00100 00100 00100 01110"},
        {'2', "01110 10001 00001 00010 00100 01000 11111"},
        {'3', "11111 00010 00100 00010 00001 10001 01110"},
        {'4', "00010 00110 01010 10010 11111 00010 00010"},
        {'5', "11111 10000 11110 00001 00001 10001 01110"},
        {'6', "00110 01000 10000 11110 10001 10001 01110"},
        {'7', "11111 00001 00010 00100 01000 01000 01000"},
        {'8', "01110 10001 10001 01110 10001 10001 01110"},
        {'9', "01110 10001 10001 01111 00001 00010 01100"},
        {'.', "00000 00000 00000 00000 00000 01100 01100"},
        {',', "00000 00000 00000 00000 01100 00100 01000"},
        {':', "00000 01100 01100 00000 01100 01100 00000"},
        {'/', "00001 00001 00010 00100 01000 10000 10000"},
        {'-', "00000 00000 00000 11111 00000 00000 00000"},
        {'+', "00000 00100 00100 11111 00100 00100 00000"},
        {'!', "00100 00100 00100 00100 00100 00000 00100"},
        {'?', "01110 10001 00001 00010 00100 00000 00100"},
        {'(', "00010 00100 01000 01000 01000 00100 00010"},
        {')', "01000 00100 00010 00010 00010 00100 01000"},
        {'%', "11000 11001 00010 00100 01000 10011 00011"},
        {'\'', "00100 00100 01000 00000 00000 00000 00000"},
        {'<', "00010 00100 01000 10000 01000 00100 00010"},
        {'>', "01000 00100 00010 00001 00010 00100 01000"},
        {'=', "00000 00000 11111 00000 11111 00000 00000"},
        {'#', "01010 01010 11111 01010 11111 01010 01010"},
};

static uint8_t gFont[128][7];
static bool gFontBuilt = false;

static void buildFont() {
    if (gFontBuilt) return;
    for (const auto &g: kGlyphs) {
        int row = 0, bits = 0, col = 0;
        for (const char *p = g.rows; *p && row < 7; ++p) {
            if (*p == ' ') continue;
            bits = (bits << 1) | (*p == '1' ? 1 : 0);
            if (++col == 5) {
                gFont[(int) g.c][row++] = (uint8_t) bits;
                bits = 0;
                col = 0;
            }
        }
    }
    gFontBuilt = true;
}

static GLuint compileShader(GLenum type, const char *src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        aout << "Shader compile failed: " << log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Draw2D::init() {
    buildFont();
    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (!vs || !fs) return false;

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[1024];
        glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
        aout << "Program link failed: " << log << std::endl;
        return false;
    }
    uMatrix_ = glGetUniformLocation(program_, "uMatrix");

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void *) offsetof(V, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(V), (void *) offsetof(V, r));
    verts_.reserve(1 << 16);
    return true;
}

void Draw2D::shutdown() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (program_) glDeleteProgram(program_);
    vbo_ = vao_ = program_ = 0;
}

void Draw2D::setView(float left, float top, float right, float bottom) {
    flush();
    for (float &m: matrix_) m = 0;
    matrix_[0] = 2.f / (right - left);
    matrix_[5] = 2.f / (top - bottom);
    matrix_[10] = -1.f;
    matrix_[12] = -(right + left) / (right - left);
    matrix_[13] = -(top + bottom) / (top - bottom);
    matrix_[15] = 1.f;
    glUseProgram(program_);
    glUniformMatrix4fv(uMatrix_, 1, GL_FALSE, matrix_);
}

void Draw2D::flush() {
    if (verts_.empty()) return;
    glUseProgram(program_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (verts_.size() * sizeof(V)), verts_.data(),
                 GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei) verts_.size());
    verts_.clear();
}

void Draw2D::rect(float x, float y, float w, float h, Color c) {
    push(x, y, c);
    push(x + w, y, c);
    push(x + w, y + h, c);
    push(x, y, c);
    push(x + w, y + h, c);
    push(x, y + h, c);
}

void Draw2D::rectOutline(float x, float y, float w, float h, float t, Color c) {
    rect(x, y, w, t, c);
    rect(x, y + h - t, w, t, c);
    rect(x, y + t, t, h - 2 * t, c);
    rect(x + w - t, y + t, t, h - 2 * t, c);
}

void Draw2D::tri(Vec2 a, Vec2 b, Vec2 c, Color col) {
    push(a.x, a.y, col);
    push(b.x, b.y, col);
    push(c.x, c.y, col);
}

void Draw2D::quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color col) {
    quad(a, b, c, d, col, col, col, col);
}

void Draw2D::quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color ca, Color cb, Color cc, Color cd) {
    push(a.x, a.y, ca);
    push(b.x, b.y, cb);
    push(c.x, c.y, cc);
    push(a.x, a.y, ca);
    push(c.x, c.y, cc);
    push(d.x, d.y, cd);
}

void Draw2D::circle(Vec2 c, float r, Color col, int segments) {
    polygon(c, r, segments, 0, col);
}

void Draw2D::polygon(Vec2 c, float r, int sides, float rotation, Color col) {
    Vec2 prev = c + fromAngle(rotation) * r;
    for (int i = 1; i <= sides; ++i) {
        Vec2 next = c + fromAngle(rotation + 2 * kPi * i / sides) * r;
        tri(c, prev, next, col);
        prev = next;
    }
}

void Draw2D::ring(Vec2 c, float r, float thick, Color col, int segments) {
    float r0 = r - thick * 0.5f, r1 = r + thick * 0.5f;
    for (int i = 0; i < segments; ++i) {
        Vec2 d0 = fromAngle(2 * kPi * i / segments), d1 = fromAngle(2 * kPi * (i + 1) / segments);
        quad(c + d0 * r0, c + d0 * r1, c + d1 * r1, c + d1 * r0, col);
    }
}

void Draw2D::line(Vec2 a, Vec2 b, float thick, Color col) {
    Vec2 d = normalize(b - a);
    Vec2 n{-d.y * thick * 0.5f, d.x * thick * 0.5f};
    quad(a + n, b + n, b - n, a - n, col);
}

void Draw2D::orientedRect(Vec2 c, Vec2 dir, float hx, float hy, Color col) {
    Vec2 f = dir * hx, s = Vec2{-dir.y, dir.x} * hy;
    quad(c + f + s, c + f - s, c - f - s, c - f + s, col);
}

void Draw2D::text(const std::string &s, float x, float y, float px, Color col) {
    float cx = x;
    for (char ch: s) {
        if (ch == '\n') {
            cx = x;
            y += px * 10;
            continue;
        }
        if (ch >= 'a' && ch <= 'z') ch = (char) (ch - 32);
        if (ch > 32 && ch < 127) {
            const uint8_t *g = gFont[(int) ch];
            for (int row = 0; row < 7; ++row) {
                uint8_t bits = g[row];
                int c0 = 0;
                while (c0 < 5) {
                    if (bits & (0x10 >> c0)) {
                        int c1 = c0;
                        while (c1 < 5 && (bits & (0x10 >> c1))) ++c1;
                        rect(cx + c0 * px, y + row * px, (c1 - c0) * px, px, col);
                        c0 = c1;
                    } else {
                        ++c0;
                    }
                }
            }
        }
        cx += px * 6;
    }
}

void Draw2D::textCentered(const std::string &s, float cx, float y, float px, Color col) {
    text(s, cx - textWidth(s, px) * 0.5f, y, px, col);
}

float Draw2D::textWidth(const std::string &s, float px) {
    size_t longest = 0, cur = 0;
    for (char ch: s) {
        if (ch == '\n') {
            longest = std::max(longest, cur);
            cur = 0;
        } else {
            ++cur;
        }
    }
    longest = std::max(longest, cur);
    return longest == 0 ? 0 : (longest * 6.f - 1.f) * px;
}
