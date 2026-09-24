#include "Game.h"
#include "Sprites.h"

#include <GLES3/gl3.h>
#include <cstdio>

static Color teamColor(int owner) {
    if (owner == PLAYER) return rgba(.25f, .62f, 1.f);
    if (owner == ENEMY) return rgba(1.f, .32f, .24f);
    return rgba(.4f, .85f, 1.f);
}

static const Color kWhite = {255, 255, 255, 255};
static const Color kBlack = {0, 0, 0, 255};

static std::string clockText(float seconds) {
    char buf[16];
    int s = (int) seconds;
    snprintf(buf, sizeof(buf), "%d:%02d", s / 60, s % 60);
    return buf;
}

// ---------------------------------------------------------------------------------------------
// Shapes
// ---------------------------------------------------------------------------------------------

static Color spriteColor(char ch, Color tc) {
    switch (ch) {
        case 'K': return {18, 20, 28, 255};
        case 'A': return tc;
        case 'a': return shade(tc, 0.6f);
        case 'H': return mix(tc, kWhite, 0.5f);
        case 'V': return {120, 240, 255, 255};
        case 'v': return {40, 150, 190, 255};
        case 'G': return {52, 54, 62, 255};
        case 'g': return {110, 114, 126, 255};
        case 'M': return {107, 112, 122, 255};
        case 'm': return {70, 74, 82, 255};
        case 'L': return {160, 165, 175, 255};
        case 'T': return {34, 35, 40, 255};
        case 't': return {84, 86, 94, 255};
        case 'O': return {214, 150, 58, 255};
        case 'o': return {150, 100, 40, 255};
        case 'S': return {226, 178, 138, 255};
        case 's': return {170, 120, 90, 255};
        case 'Y': return {255, 200, 60, 255};
        case 'W': return {235, 238, 245, 255};
        default: return {255, 0, 255, 255};
    }
}

// Draws a sprite centered on p. Runs of same-colored pixels in a row become one rect.
static void drawSprite(Draw2D &d, const Sprite &sp, Vec2 p, float px, bool flip, Color tc, float flash) {
    float x0 = p.x - sp.w * px * 0.5f, y0 = p.y - sp.h * px * 0.5f;
    for (int y = 0; y < sp.h; ++y) {
        const char *row = sp.rows[y];
        int x = 0;
        while (x < sp.w) {
            char ch = row[x];
            int x2 = x + 1;
            while (x2 < sp.w && row[x2] == ch) ++x2;
            if (ch != '.') {
                Color c = spriteColor(ch, tc);
                if (flash > 0) c = mix(c, kWhite, 0.55f);
                float left = flip ? sp.w - x2 : x;
                d.rect(x0 + left * px, y0 + y * px, (x2 - x) * px, px, c);
            }
            x = x2;
        }
    }
}

void Game::drawUnitShape(Draw2D &d, EType t, int owner, Vec2 p, float s, float facing, bool cargo,
                         float flash, int riders) const {
    const Sprite *sp = unitSprite(t);
    if (!sp) return;
    Color tc = teamColor(owner);
    float px = sp->px * s;
    bool flip = std::cos(facing) < -0.05f;
    float r = kTypes[t].radius * s;
    d.circle(p + Vec2(0, sp->h * px * 0.42f), r * 0.9f, rgba(0, 0, 0, .28f), 14);
    drawSprite(d, *sp, p, px, flip, tc, flash);

    // Extra pixels drawn on top of the sprite, mirrored the same way it is.
    Vec2 origin = p - Vec2(sp->w, sp->h) * (px * 0.5f);
    auto pixels = [&](int x, int y, int w, int h, Color c) {
        float left = flip ? sp->w - (x + w) : x;
        d.rect(origin.x + left * px, origin.y + y * px, w * px, h * px, c);
    };
    if (t == T_WORKER && cargo) {
        pixels(1, 8, 3, 3, spriteColor('K', tc));
        pixels(2, 9, 1, 1, spriteColor('V', tc));
    }
    if (t == T_ROVER) {
        // Helmets of the soldiers riding in the open bed.
        for (int i = 0; i < riders && i < kTransportSlots; ++i) {
            int x = 1 + i * 4;
            pixels(x - 1, 2, 4, 3, spriteColor('K', tc));
            pixels(x, 3, 2, 2, spriteColor('A', tc));
            pixels(x + 1, 3, 1, 1, spriteColor('V', tc));
        }
    }
}

void Game::drawBuildingShape(Draw2D &d, EType t, int owner, const Rectf &r, float facing,
                             float flash) const {
    Color tc = teamColor(owner);
    Color metal = rgba(.42f, .44f, .48f), dark = rgba(.2f, .21f, .24f), edge = rgba(.13f, .14f, .16f);
    if (flash > 0) {
        metal = mix(metal, kWhite, 0.5f);
        tc = mix(tc, kWhite, 0.5f);
    }
    float u = std::min(r.w, r.h);
    Vec2 c = r.center();
    if (t == T_MINERAL) {
        Color crystal = rgba(.3f, .8f, 1), light = rgba(.7f, .97f, 1), deep = rgba(.12f, .45f, .75f);
        const Vec2 offs[3] = {{-0.28f, 0.1f}, {0.02f, -0.08f}, {0.3f, 0.12f}};
        const float hs[3] = {0.55f, 0.75f, 0.5f};
        d.rect(r.x + u * 0.1f, r.y + r.h * 0.55f, r.w - u * 0.2f, r.h * 0.35f, rgba(0, 0, 0, .25f));
        for (int i = 0; i < 3; ++i) {
            Vec2 k = c + Vec2(offs[i].x * r.w, offs[i].y * r.h);
            float w = r.w * 0.13f, h = r.h * hs[i];
            Vec2 top = k + Vec2(0, -h * 0.6f), right = k + Vec2(w, 0), bot = k + Vec2(0, h * 0.4f), left = k + Vec2(-w, 0);
            d.quad(top, right, bot, left, crystal);
            d.tri(top, k, left, light);
            d.tri(k, bot, right, deep);
        }
        return;
    }
    if (t == T_TURRET) {
        d.polygon(c + Vec2(u * 0.05f, u * 0.07f), u * 0.46f, 8, kPi / 8, rgba(0, 0, 0, .35f));
        d.polygon(c, u * 0.46f, 8, kPi / 8, metal);
        d.polygon(c, u * 0.34f, 8, kPi / 8, dark);
        Vec2 f = fromAngle(facing);
        d.line(c, c + f * (u * 0.5f), u * 0.11f, rgba(.25f, .25f, .28f));
        d.circle(c, u * 0.22f, tc, 16);
        d.circle(c, u * 0.1f, shade(tc, 0.6f), 10);
        return;
    }
    d.rect(r.x + u * 0.06f, r.y + u * 0.08f, r.w, r.h, rgba(0, 0, 0, .35f));
    d.rect(r, metal);
    switch (t) {
        case T_HQ: {
            float in = u * 0.1f;
            d.rect(r.x + in, r.y + in, r.w - 2 * in, r.h - 2 * in, dark);
            d.circle(c, u * 0.33f, metal, 24);
            d.circle(c, u * 0.27f, shade(tc, 0.55f), 24);
            d.circle(c, u * 0.19f, tc, 20);
            d.ring(c, u * 0.33f, u * 0.025f, edge, 24);
            float k = u * 0.17f;
            const Vec2 corners[4] = {{r.x + k, r.y + k}, {r.x + r.w - k, r.y + k},
                                     {r.x + k, r.y + r.h - k}, {r.x + r.w - k, r.y + r.h - k}};
            for (auto p: corners) d.circle(p, u * 0.045f, tc, 8);
            break;
        }
        case T_DEPOT: {
            float in = u * 0.16f;
            d.rect(r.x + in, r.y + in, r.w - 2 * in, r.h - 2 * in, dark);
            d.line({r.x + in, r.y + r.h - in}, {r.x + r.w - in, r.y + in}, u * 0.12f, tc);
            d.rect(r.x + in, r.y + r.h * 0.46f, r.w - 2 * in, u * 0.08f, metal);
            break;
        }
        case T_BARRACKS: {
            d.rect(r.x, r.y, r.w, r.h * 0.36f, shade(tc, 0.75f));
            d.rect(r.x, r.y + r.h * 0.36f, r.w, u * 0.04f, edge);
            d.rect(c.x - u * 0.16f, r.y + r.h * 0.62f, u * 0.32f, r.h * 0.38f, dark);
            d.rect(r.x + u * 0.1f, r.y + r.h * 0.5f, u * 0.14f, u * 0.1f, rgba(.9f, .85f, .5f));
            d.rect(r.x + r.w - u * 0.24f, r.y + r.h * 0.5f, u * 0.14f, u * 0.1f, rgba(.9f, .85f, .5f));
            break;
        }
        case T_FACTORY: {
            d.rect(r.x + r.w * 0.66f, r.y, r.w * 0.34f, r.h, shade(tc, 0.75f));
            float doorY = r.y + r.h * 0.5f;
            d.rect(r.x + u * 0.08f, doorY, r.w * 0.5f, r.h * 0.5f - u * 0.04f, dark);
            for (int i = 1; i < 4; ++i) {
                d.rect(r.x + u * 0.08f, doorY + i * r.h * 0.11f, r.w * 0.5f, u * 0.025f, edge);
            }
            d.circle({r.x + u * 0.2f, r.y + u * 0.2f}, u * 0.11f, edge, 12);
            d.circle({r.x + u * 0.2f, r.y + u * 0.2f}, u * 0.07f, rgba(.35f, .33f, .3f), 10);
            d.circle({r.x + u * 0.46f, r.y + u * 0.2f}, u * 0.11f, edge, 12);
            d.circle({r.x + u * 0.46f, r.y + u * 0.2f}, u * 0.07f, rgba(.35f, .33f, .3f), 10);
            break;
        }
        default:
            break;
    }
    d.rectOutline(r, std::max(1.f, u * 0.03f), edge);
}

void Game::drawIcon(Draw2D &d, EType t, int owner, Vec2 c, float size) const {
    const TypeDef &td = kTypes[t];
    if (!td.building) {
        const Sprite *sp = unitSprite(t);
        if (!sp) return;
        float s = size / (std::max(sp->w, sp->h) * sp->px);
        drawUnitShape(d, t, owner, c, s, 0, false, 0);
        return;
    }
    float w = size, h = size;
    if (td.tilesW > td.tilesH) h = size * td.tilesH / td.tilesW;
    drawBuildingShape(d, t, owner, {c.x - w * 0.5f, c.y - h * 0.5f, w, h}, -kPi / 4, 0);
}

void Game::drawHealthBar(Draw2D &d, Vec2 center, float width, float frac, float thick) const {
    frac = clampf(frac, 0, 1);
    Color col = frac > 0.6f ? rgba(.3f, .9f, .35f) : (frac > 0.3f ? rgba(1, .8f, .2f) : rgba(1, .25f, .2f));
    float x = center.x - width * 0.5f;
    d.rect(x - 1, center.y - 1, width + 2, thick + 2, rgba(0, 0, 0, .8f));
    d.rect(x, center.y, width * frac, thick, col);
}

// ---------------------------------------------------------------------------------------------
// World
// ---------------------------------------------------------------------------------------------

void Game::renderWorld(Draw2D &d) {
    float vw = screenW_ / scale_, vh = screenH_ / scale_;
    int x0 = std::max(0, (int) std::floor(cam_.x / TILE));
    int y0 = std::max(0, (int) std::floor(cam_.y / TILE));
    int x1 = std::min(MAP_W - 1, (int) std::floor((cam_.x + vw) / TILE));
    int y1 = std::min(MAP_H - 1, (int) std::floor((cam_.y + vh) / TILE));

    // Terrain
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            int i = idx(x, y);
            float sh = (shade_[i] / 255.f - 0.5f) * 0.05f;
            float px = x * TILE, py = y * TILE;
            if (terrain_[i] == 0) {
                d.rect(px, py, TILE, TILE, rgba(.30f + sh, .26f + sh, .20f + sh));
                if (shade_[i] > 232) {
                    Vec2 c{px + 6 + (shade_[i] & 7) * 2.5f, py + 8 + ((shade_[i] >> 2) & 7) * 2.f};
                    d.circle(c, 3.5f, rgba(.23f, .2f, .16f), 8);
                    d.circle(c + Vec2(-1, -1), 2.f, rgba(.36f, .32f, .26f), 6);
                } else if (shade_[i] < 10) {
                    d.line({px + 4, py + 10}, {px + 26, py + 20}, 1.5f, rgba(.22f, .19f, .15f));
                }
            } else {
                d.rect(px, py, TILE, TILE, rgba(.15f + sh, .135f + sh, .125f + sh));
                d.circle({px + TILE * 0.5f, py + TILE * 0.45f}, TILE * 0.46f, rgba(.21f + sh, .19f + sh, .17f + sh), 10);
                d.circle({px + TILE * 0.42f, py + TILE * 0.36f}, TILE * 0.22f, rgba(.26f + sh, .235f + sh, .21f + sh), 8);
                if (y + 1 < MAP_H && terrain_[idx(x, y + 1)] == 0) {
                    d.rect(px, py + TILE, TILE, 5, rgba(0, 0, 0, .3f));
                }
            }
        }
    }

    if (mode_ == MODE_PLACE) {
        Color gl = rgba(1, 1, 1, .06f);
        for (int x = x0; x <= x1 + 1; ++x) d.rect(x * TILE, y0 * TILE, 1, (y1 - y0 + 1) * TILE, gl);
        for (int y = y0; y <= y1 + 1; ++y) d.rect(x0 * TILE, y * TILE, (x1 - x0 + 1) * TILE, 1, gl);
    }

    auto inView = [&](Vec2 p, float pad) {
        return p.x > cam_.x - pad && p.y > cam_.y - pad && p.x < cam_.x + vw + pad && p.y < cam_.y + vh + pad;
    };
    std::vector<bool> selected(ents_.size(), false);
    for (int id: sel_) selected[id] = true;

    // Minerals and buildings
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (!e.alive || !isBuilding(e) || !inView(e.pos, 4 * TILE)) continue;
        if (e.type == T_MINERAL && vis_[idx(e.tx, e.ty)] == 0) continue;
        if (e.owner == ENEMY && !e.seen) continue;
        const TypeDef &td = kTypes[e.type];
        Rectf r{e.tx * TILE + 2, e.ty * TILE + 2, td.tilesW * TILE - 4, td.tilesH * TILE - 4};
        if (selected[id]) {
            Color sc = e.owner == PLAYER ? rgba(.3f, 1, .4f) : (e.owner == ENEMY ? rgba(1, .3f, .25f) : rgba(1, 1, .5f));
            d.rectOutline(r.x - 5, r.y - 5, r.w + 10, r.h + 10, 2.5f, sc);
        }
        if (e.type == T_MINERAL) {
            float frac = 0.55f + 0.45f * std::min(1.f, e.amount / 1500.f);
            Rectf rr{r.x + r.w * (1 - frac) * 0.5f, r.y + r.h * (1 - frac), r.w * frac, r.h * frac};
            drawBuildingShape(d, T_MINERAL, NEUTRAL, rr, 0, 0);
        } else if (!e.complete) {
            Color tc = teamColor(e.owner);
            d.rect(r, rgba(.12f, .13f, .15f, .85f));
            float fh = r.h * e.progress;
            d.rect(r.x, r.y + r.h - fh, r.w, fh, rgba(.38f, .4f, .44f));
            for (float x = r.x + 12; x < r.x + r.w; x += 16) d.rect(x, r.y, 2, r.h, rgba(.6f, .55f, .3f, .5f));
            d.rectOutline(r, 2, tc);
        } else {
            drawBuildingShape(d, e.type, e.owner, r, e.facing, e.flash);
        }
    }

    // Rally points
    for (int id: sel_) {
        const Entity *e = ent(id);
        if (!e || e->owner != PLAYER || !e->hasRally) continue;
        d.line(e->pos, e->rally, 1.5f, rgba(.3f, 1, .4f, .5f));
        d.line(e->rally, e->rally + Vec2(0, -18), 2, rgba(.8f, .8f, .8f));
        d.tri(e->rally + Vec2(0, -18), e->rally + Vec2(12, -13), e->rally + Vec2(0, -8), rgba(.3f, 1, .4f));
    }

    // Officer auras, under the units
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (!e.alive || e.type != T_OFFICER || !inView(e.pos, kAuraRadius)) continue;
        if (e.owner != PLAYER && !visibleToPlayer(e)) continue;
        Color tc = teamColor(e.owner);
        d.circle(e.pos, kAuraRadius, fade(tc, selected[id] ? .1f : .05f), 40);
        d.ring(e.pos, kAuraRadius, 1.5f, fade(tc, selected[id] ? .5f : .22f), 40);
    }

    // Units
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (!e.alive || isBuilding(e) || inside(e) || !inView(e.pos, 40)) continue;
        if (e.owner != PLAYER && !visibleToPlayer(e)) continue;
        if (selected[id]) {
            Color sc = e.owner == PLAYER ? rgba(.3f, 1, .4f) : rgba(1, .3f, .25f);
            d.ring(e.pos, unitRadius(e) + 4, 2, sc, 20);
        }
        drawUnitShape(d, e.type, e.owner, e.pos, 1.f, e.facing, e.cargo > 0, e.flash,
                      (int) e.passengers.size());
        if (e.type == T_TANK && !e.passengers.empty()) {
            // Seat pips: how many soldiers are inside.
            float pw = 5, gap = 2, w = kTransportSlots * pw + (kTransportSlots - 1) * gap;
            float x = e.pos.x - w * 0.5f, y = e.pos.y + unitRadius(e) + 4;
            for (int i = 0; i < kTransportSlots; ++i) {
                float px = x + i * (pw + gap);
                d.rect(px - 1, y - 1, pw + 2, pw + 2, rgba(0, 0, 0, .7f));
                if (i < (int) e.passengers.size()) d.rect(px, y, pw, pw, teamColor(e.owner));
            }
        }
    }

    // Projectiles
    for (const Shot &s: shots_) {
        const Entity *tgt = ent(s.target);
        Vec2 to = tgt && !isBuilding(*tgt) ? tgt->pos : s.to;
        Vec2 p = lerp(s.from, to, s.t / s.dur);
        if (s.splash > 0) {
            d.circle(p, 4, rgba(1, .85f, .5f), 8);
        } else {
            Vec2 dir = normalize(to - s.from);
            d.line(p - dir * 10, p, 3, rgba(.6f, .9f, 1));
        }
    }

    // Effects
    for (const Fx &f: fx_) {
        float k = f.t / f.dur;
        switch (f.kind) {
            case FX_TRACER:
                d.line(f.a, f.b, f.size, fade(f.col, 1 - k));
                break;
            case FX_FLASH:
                d.circle(f.a, f.size * (1 - k * 0.5f), fade(f.col, 1 - k), 8);
                break;
            case FX_EXPLODE: {
                float r = f.size * (0.4f + 0.8f * k);
                d.circle(f.a, r, fade(rgba(.25f, .2f, .18f), (1 - k) * 0.6f), 16);
                d.circle(f.a, r * 0.75f, fade(f.col, 1 - k), 16);
                d.circle(f.a, r * 0.4f * (1 - k), fade(rgba(1, 1, .8f), 1 - k), 12);
                break;
            }
            case FX_SPARK: {
                for (int i = 0; i < 3; ++i) {
                    Vec2 o = fromAngle(i * 2.1f + f.a.x) * (f.size * k * 1.5f);
                    d.rect(f.a.x + o.x - 1.5f, f.a.y + o.y - 1.5f, 3, 3, fade(f.col, 1 - k));
                }
                break;
            }
            case FX_MARK:
                d.ring(f.a, f.size * (1 - k * 0.6f), 2.5f, fade(f.col, 1 - k), 20);
                break;
            default:
                break;
        }
    }

    // Placement ghost
    if (mode_ == MODE_PLACE && placeVisible_) {
        const TypeDef &td = kTypes[placeType_];
        int tx, ty;
        placementTile(placePos_, tx, ty);
        bool ok = canPlace(placeType_, tx, ty, PLAYER);
        for (int y = ty; y < ty + td.tilesH; ++y) {
            for (int x = tx; x < tx + td.tilesW; ++x) {
                bool free = inMap(x, y) && !blocked(x, y) && vis_[idx(x, y)] != 0;
                d.rect(x * TILE + 1, y * TILE + 1, TILE - 2, TILE - 2,
                       free && ok ? rgba(.2f, 1, .3f, .35f) : rgba(1, .2f, .2f, .4f));
            }
        }
        Rectf r{tx * TILE + 2, ty * TILE + 2, td.tilesW * TILE - 4, td.tilesH * TILE - 4};
        drawBuildingShape(d, placeType_, PLAYER, r, -kPi / 4, 0);
        d.rect(r, ok ? rgba(.2f, 1, .3f, .25f) : rgba(1, .2f, .2f, .35f));
    }

    // Health bars & progress
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (!e.alive || e.type == T_MINERAL || inside(e) || !inView(e.pos, 4 * TILE)) continue;
        bool vis = e.owner == PLAYER || visibleToPlayer(e);
        if (!vis) continue;
        const TypeDef &td = kTypes[e.type];
        bool show = selected[id] || e.hp < td.hp - 0.01f || !e.complete;
        if (!show) continue;
        float hx, hy;
        halfExtents(e, hx, hy);
        float w = isBuilding(e) ? hx * 1.6f : std::max(20.f, hx * 2.4f);
        Vec2 top{e.pos.x, e.pos.y - hy - 8};
        drawHealthBar(d, top, w, e.hp / td.hp, isBuilding(e) ? 5 : 3.5f);
        if (isBuilding(e) && (!e.complete || !e.queue.empty()) && e.owner == PLAYER) {
            float prog = !e.complete ? e.progress : e.trainT / kTypes[e.queue.front()].buildTime;
            d.rect(top.x - w * 0.5f, top.y + 8, w, 4, rgba(0, 0, 0, .8f));
            d.rect(top.x - w * 0.5f, top.y + 8, w * clampf(prog, 0, 1), 4, rgba(.4f, .8f, 1));
        }
    }

    renderFog(d, x0, y0, x1, y1);
}

void Game::renderFog(Draw2D &d, int x0, int y0, int x1, int y1) {
    auto fogA = [&](int x, int y) {
        x = std::min(std::max(x, 0), MAP_W - 1);
        y = std::min(std::max(y, 0), MAP_H - 1);
        uint8_t v = vis_[idx(x, y)];
        return v == 0 ? 1.f : (v == 1 ? 0.5f : 0.f);
    };
    int cw = x1 - x0 + 2, ch = y1 - y0 + 2;
    if (cw <= 0 || ch <= 0) return;
    std::vector<float> corner((size_t) (cw * ch));
    for (int y = 0; y < ch; ++y) {
        for (int x = 0; x < cw; ++x) {
            int cx = x0 + x, cy = y0 + y;
            corner[y * cw + x] = (fogA(cx - 1, cy - 1) + fogA(cx, cy - 1) + fogA(cx - 1, cy) + fogA(cx, cy)) * 0.25f;
        }
    }
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            int lx = x - x0, ly = y - y0;
            float a = corner[ly * cw + lx], b = corner[ly * cw + lx + 1];
            float c = corner[(ly + 1) * cw + lx + 1], e = corner[(ly + 1) * cw + lx];
            if (a + b + c + e <= 0) continue;
            Vec2 p{x * TILE, y * TILE};
            d.quad(p, p + Vec2(TILE, 0), p + Vec2(TILE, TILE), p + Vec2(0, TILE),
                   rgba(0, 0, 0, a), rgba(0, 0, 0, b), rgba(0, 0, 0, c), rgba(0, 0, 0, e));
        }
    }
}

// ---------------------------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------------------------

void Game::renderHud(Draw2D &d) {
    buildButtons();
    float u = ui_;
    Color accent = rgba(.25f, .62f, 1.f, .7f);

    // Top bar
    d.rect(0, 0, screenW_, topH_, rgba(.05f, .06f, .08f, .92f));
    d.rect(0, topH_ - 2 * u, screenW_, 2 * u, accent);
    float tp = 2.8f * u, ty = (topH_ - 7 * tp) * 0.5f;
    Vec2 mi{22 * u, topH_ * 0.5f};
    d.polygon(mi, 10 * u, 4, 0, rgba(.3f, .8f, 1));
    d.tri(mi + Vec2(0, -10 * u), mi, mi + Vec2(-10 * u, 0), rgba(.75f, .97f, 1));
    d.text(std::to_string(minerals_[PLAYER]), 40 * u, ty, tp, kWhite);

    float sx = 180 * u;
    bool capped = supplyUsed_[PLAYER] >= supplyCap_[PLAYER];
    Color supCol = capped ? rgba(1, .35f, .3f) : rgba(.45f, .95f, .5f);
    d.rect(sx - 10 * u, topH_ * 0.5f - 2 * u, 20 * u, 12 * u, supCol);
    d.tri({sx - 13 * u, topH_ * 0.5f - 2 * u}, {sx, topH_ * 0.5f - 12 * u}, {sx + 13 * u, topH_ * 0.5f - 2 * u}, supCol);
    d.text(std::to_string(supplyUsed_[PLAYER]) + "/" + std::to_string(supplyCap_[PLAYER]), sx + 20 * u, ty, tp,
           capped ? rgba(1, .45f, .4f) : kWhite);

    d.text(clockText(gameTime_), 350 * u, ty, tp, rgba(.75f, .8f, .9f));

    if (msgT_ > 0) {
        float a = std::min(1.f, msgT_ / 0.5f);
        float px = 2.6f * u;
        float w = Draw2D::textWidth(msg_, px);
        d.rect(screenW_ * 0.5f - w * 0.5f - 12 * u, topH_ + 10 * u, w + 24 * u, 7 * px + 16 * u, rgba(0, 0, 0, .55f * a));
        d.textCentered(msg_, screenW_ * 0.5f, topH_ + 18 * u, px, fade(rgba(1, .9f, .5f), a));
    }
    if (mode_ == MODE_PLACE) {
        d.textCentered("TAP THE MAP TO PLACE - DRAG TO ADJUST", screenW_ * 0.5f, panelY_ - 26 * u, 2 * u,
                       rgba(1, 1, 1, .85f));
    } else if (mode_ == MODE_ATTACK) {
        d.textCentered("ATTACK: TAP AN ENEMY OR A LOCATION", screenW_ * 0.5f, panelY_ - 26 * u, 2 * u,
                       rgba(1, .6f, .5f, .9f));
    } else if (mode_ == MODE_BOARD) {
        d.textCentered("BOARD: TAP ONE OF YOUR TANKS OR ROVERS", screenW_ * 0.5f, panelY_ - 26 * u, 2 * u,
                       rgba(.5f, 1, .6f, .9f));
    }

    // Box selection
    if (gesture_ == G_BOX && numTouches_ > 0) {
        Vec2 a = touches_[0].start, b = touches_[0].cur;
        Rectf r{std::min(a.x, b.x), std::min(a.y, b.y), std::fabs(a.x - b.x), std::fabs(a.y - b.y)};
        d.rect(r, rgba(.3f, 1, .4f, .12f));
        d.rectOutline(r, 2 * u, rgba(.3f, 1, .4f, .9f));
    }

    // Bottom panel
    d.rect(0, panelY_, screenW_, panelH_, rgba(.07f, .08f, .1f, .96f));
    d.rect(0, panelY_, screenW_, 2 * u, accent);
    renderMinimap(d);
    renderSelectionInfo(d);

    // Buttons
    for (size_t i = 0; i < buttons_.size(); ++i) {
        const Button &b = buttons_[i];
        bool pressed = (int) i == pressedBtn_ && gesture_ == G_HUD;
        Color bg = pressed ? rgba(.25f, .32f, .42f) : (b.enabled ? rgba(.13f, .16f, .21f) : rgba(.09f, .09f, .11f));
        d.rect(b.r, bg);
        d.rectOutline(b.r, 1.5f * u, b.enabled ? rgba(.3f, .5f, .75f) : rgba(.2f, .2f, .24f));
        Color txt = b.enabled ? kWhite : rgba(.45f, .45f, .5f);
        if (b.action == A_IDLE || b.action == A_ARMY) {
            float px = 2 * u;
            d.textCentered(b.label, b.r.x + b.r.w * 0.5f, b.r.y + (b.r.h - 7 * px) * 0.5f, px, txt);
            continue;
        }
        Vec2 ic{b.r.x + b.r.w * 0.5f, b.r.y + b.r.h * 0.3f};
        float is = b.r.h * 0.38f;
        switch (b.action) {
            case A_TRAIN:
            case A_BUILD:
                drawIcon(d, b.param, PLAYER, ic, is);
                break;
            case A_STOP:
                d.rect(ic.x - is * 0.3f, ic.y - is * 0.3f, is * 0.6f, is * 0.6f, rgba(.9f, .3f, .25f));
                break;
            case A_HOLD:
                d.ring(ic, is * 0.32f, 4 * u, rgba(.9f, .8f, .3f), 20);
                d.rect(ic.x - 2 * u, ic.y - is * 0.2f, 4 * u, is * 0.4f, rgba(.9f, .8f, .3f));
                break;
            case A_ATTACK:
                d.ring(ic, is * 0.32f, 3 * u, rgba(1, .35f, .3f), 20);
                d.rect(ic.x - is * 0.5f, ic.y - 1.5f * u, is, 3 * u, rgba(1, .35f, .3f));
                d.rect(ic.x - 1.5f * u, ic.y - is * 0.5f, 3 * u, is, rgba(1, .35f, .3f));
                break;
            case A_BOARD:
            case A_UNLOAD: {
                // A vehicle bay with an arrow going in (board) or out (unload).
                Color c = rgba(.45f, .95f, .55f);
                d.rectOutline(ic.x - is * 0.4f, ic.y, is * 0.8f, is * 0.35f, 2.5f * u, c);
                float tip = b.action == A_BOARD ? ic.y + is * 0.1f : ic.y - is * 0.45f;
                float tail = b.action == A_BOARD ? ic.y - is * 0.45f : ic.y + is * 0.1f;
                float head = b.action == A_BOARD ? -1.f : 1.f;
                d.rect(ic.x - 1.5f * u, std::min(tip, tail), 3 * u, std::fabs(tail - tip), c);
                d.tri({ic.x - is * 0.18f, tip + head * is * 0.18f}, {ic.x + is * 0.18f, tip + head * is * 0.18f},
                      {ic.x, tip}, c);
                break;
            }
            case A_CANCEL:
            case A_DEQUEUE:
                d.line(ic + Vec2(-is, -is) * 0.3f, ic + Vec2(is, is) * 0.3f, 5 * u, rgba(.9f, .3f, .25f));
                d.line(ic + Vec2(-is, is) * 0.3f, ic + Vec2(is, -is) * 0.3f, 5 * u, rgba(.9f, .3f, .25f));
                break;
            default:
                break;
        }
        float lpx = std::min(1.9f * u, (b.r.w - 8 * u) / (b.label.size() * 6.f));
        float spx = b.sub.empty() ? 0 : std::min(1.6f * u, (b.r.w - 8 * u) / (b.sub.size() * 6.f));
        float ly = b.r.y + b.r.h * 0.56f;
        if (b.sub.empty()) ly += 4 * u;
        d.textCentered(b.label, b.r.x + b.r.w * 0.5f, ly, lpx, txt);
        if (!b.sub.empty()) {
            Color sc = !b.enabled ? rgba(.8f, .45f, .4f) : (b.warn ? rgba(1, .4f, .35f) : rgba(.45f, .85f, 1));
            d.textCentered(b.sub, b.r.x + b.r.w * 0.5f, ly + 7 * lpx + 3 * u, spx, sc);
        }
    }
}

void Game::renderMinimap(Draw2D &d) {
    const Rectf &m = minimap_;
    float sx = m.w / MAP_W, sy = m.h / MAP_H;
    d.rect(m.x - 2, m.y - 2, m.w + 4, m.h + 4, rgba(.2f, .22f, .28f));
    d.rect(m, kBlack);
    // Terrain with fog, run-length encoded per row.
    for (int y = 0; y < MAP_H; ++y) {
        int x = 0;
        while (x < MAP_W) {
            int i = idx(x, y);
            uint8_t v = vis_[i], t = terrain_[i];
            int x2 = x + 1;
            while (x2 < MAP_W && vis_[idx(x2, y)] == v && terrain_[idx(x2, y)] == t) ++x2;
            if (v != 0) {
                float k = v == 2 ? 1.f : 0.55f;
                Color c = t ? rgba(.14f * k, .13f * k, .12f * k) : rgba(.36f * k, .31f * k, .24f * k);
                d.rect(m.x + x * sx, m.y + y * sy, (x2 - x) * sx, sy, c);
            }
            x = x2;
        }
    }
    float dot = std::max(2.f, 1.4f * sx);
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (!e.alive) continue;
        if (isBuilding(e)) {
            if (e.type == T_MINERAL && vis_[idx(e.tx, e.ty)] == 0) continue;
            if (e.owner == ENEMY && !e.seen) continue;
            const TypeDef &td = kTypes[e.type];
            d.rect(m.x + e.tx * sx, m.y + e.ty * sy, td.tilesW * sx, td.tilesH * sy, teamColor(e.owner));
        } else {
            if (inside(e) || (e.owner != PLAYER && !visibleToPlayer(e))) continue;
            d.rect(m.x + e.pos.x / TILE * sx - dot * 0.5f, m.y + e.pos.y / TILE * sy - dot * 0.5f, dot, dot,
                   teamColor(e.owner));
        }
    }
    if (pingT_ > 0) {
        Vec2 p{m.x + pingPos_.x / TILE * sx, m.y + pingPos_.y / TILE * sy};
        float k = std::fmod(pingT_, 1.f);
        d.ring(p, (4 + 14 * (1 - k)) * ui_, 2 * ui_, fade(rgba(1, .3f, .25f), k), 20);
    }
    // Camera frustum
    float cx = m.x + cam_.x / TILE * sx, cy = m.y + (cam_.y + topH_ / scale_) / TILE * sy;
    float cw = screenW_ / scale_ / TILE * sx, chh = (panelY_ - topH_) / scale_ / TILE * sy;
    float x0 = std::max(cx, m.x), y0 = std::max(cy, m.y);
    float x1 = std::min(cx + cw, m.x + m.w), y1 = std::min(cy + chh, m.y + m.h);
    if (x1 > x0 && y1 > y0) d.rectOutline(x0, y0, x1 - x0, y1 - y0, 1.5f * ui_, rgba(1, 1, 1, .9f));
}

void Game::renderSelectionInfo(Draw2D &d) {
    const Rectf &r = info_;
    float u = ui_;
    d.rect(r, rgba(.05f, .06f, .08f));
    d.rectOutline(r, 1.5f * u, rgba(.18f, .22f, .3f));
    float px = 2.f * u, pad = 10 * u;

    if (sel_.empty()) {
        d.text("TAP A UNIT TO SELECT IT\nTAP THE GROUND TO MOVE OR ATTACK\nHOLD, THEN DRAG TO BOX SELECT\n"
               "DRAG TO SCROLL - PINCH TO ZOOM\nDOUBLE TAP: SELECT ALL OF A TYPE",
               r.x + pad, r.y + pad, 1.6f * u, rgba(.55f, .6f, .7f));
        return;
    }

    if (sel_.size() == 1) {
        const Entity &e = ents_[sel_[0]];
        const TypeDef &td = kTypes[e.type];
        d.text(td.name, r.x + pad, r.y + pad, 2.6f * u, e.owner == ENEMY ? rgba(1, .5f, .45f) : kWhite);
        float iconS = std::min(64 * u, r.h - 50 * u);
        Vec2 ic{r.x + pad + iconS * 0.5f, r.y + 34 * u + iconS * 0.5f};
        d.rect(ic.x - iconS * 0.5f, ic.y - iconS * 0.5f, iconS, iconS, rgba(.1f, .12f, .15f));
        drawIcon(d, e.type, e.owner, ic, iconS * 0.8f);
        float tx = r.x + pad * 2 + iconS, tyy = r.y + 36 * u, tw = r.w - (tx - r.x) - pad;
        Color dim = rgba(.7f, .75f, .85f);

        if (e.type == T_MINERAL) {
            d.text("REMAINING: " + std::to_string(e.amount), tx, tyy, px, rgba(.5f, .9f, 1));
            return;
        }
        drawHealthBar(d, {tx + tw * 0.5f, tyy}, tw, e.hp / td.hp, 8 * u);
        d.text("HP " + std::to_string((int) std::ceil(e.hp)) + "/" + std::to_string((int) td.hp), tx,
               tyy + 16 * u, px, dim);
        float ly = tyy + 40 * u;
        if (!td.building) {
            if (td.damage > 0) {
                char buf[64];
                snprintf(buf, sizeof(buf), "DMG %d  RANGE %d  ARMOR %d", (int) td.damage,
                         std::max(1, (int) (td.range / TILE + 0.5f)), (int) td.armor);
                d.text(buf, tx, ly, 1.7f * u, dim);
                ly += 22 * u;
            }
            const char *status = "IDLE";
            switch (e.order) {
                case O_MOVE: status = "MOVING"; break;
                case O_ATTACK_MOVE: status = e.target >= 0 ? "ENGAGING" : "ATTACK MOVING"; break;
                case O_ATTACK: status = "ATTACKING"; break;
                case O_HOLD: status = "HOLDING POSITION"; break;
                case O_GATHER: status = "GATHERING MINERALS"; break;
                case O_RETURN: status = "RETURNING CARGO"; break;
                case O_BUILD: status = "GOING TO BUILD SITE"; break;
                case O_CONSTRUCT: status = "CONSTRUCTING"; break;
                case O_BOARD: status = "BOARDING"; break;
                default: break;
            }
            if (e.owner == PLAYER) {
                d.text(status, tx, ly, 1.7f * u, rgba(.5f, .9f, 1));
                ly += 22 * u;
            }
            if (e.type == T_OFFICER) {
                d.text("AURA: +25% DMG, FIRE RATE, +1 ARMOR", tx, ly, 1.5f * u, rgba(1, .85f, .4f));
            } else if (isTransport(e.type)) {
                std::string seats = "PASSENGERS " + std::to_string(e.passengers.size()) + "/" +
                                    std::to_string(kTransportSlots);
                if (e.type == T_ROVER) seats += " - THEY FIRE FROM INSIDE";
                d.text(seats, tx, ly, 1.5f * u, rgba(.45f, .95f, .55f));
            }
            return;
        }
        if (!e.complete) {
            d.text("UNDER CONSTRUCTION " + std::to_string((int) (e.progress * 100)) + "%", tx, ly, 1.7f * u,
                   rgba(1, .85f, .4f));
            return;
        }
        if (td.supplyProvided > 0) {
            d.text("PROVIDES " + std::to_string(td.supplyProvided) + " SUPPLY", tx, ly, 1.7f * u, dim);
            ly += 22 * u;
        }
        if (td.damage > 0) {
            d.text("DMG " + std::to_string((int) td.damage) + "  RANGE " + std::to_string((int) (td.range / TILE)),
                   tx, ly, 1.7f * u, dim);
        }
        if (e.owner == PLAYER && !e.queue.empty()) {
            float prog = e.trainT / kTypes[e.queue.front()].buildTime;
            d.text(std::string("TRAINING ") + kTypes[e.queue.front()].name, tx, ly, 1.7f * u, rgba(.5f, .9f, 1));
            d.rect(tx, ly + 16 * u, tw, 6 * u, rgba(0, 0, 0, .8f));
            d.rect(tx, ly + 16 * u, tw * clampf(prog, 0, 1), 6 * u, rgba(.4f, .8f, 1));
            float cs = std::min(34 * u, (tw - 4 * 4 * u) / 5);
            for (int i = 0; i < 5; ++i) {
                Rectf c{tx + i * (cs + 4 * u), ly + 28 * u, cs, cs};
                d.rect(c, rgba(.1f, .12f, .15f));
                d.rectOutline(c, 1, rgba(.25f, .3f, .4f));
                if (i < (int) e.queue.size()) drawIcon(d, e.queue[i], PLAYER, c.center(), cs * 0.8f);
            }
        }
        return;
    }

    // Multi-selection grid
    float cell = 42 * u;
    int cols = std::max(1, (int) ((r.w - pad) / cell));
    int rows = std::max(1, (int) ((r.h - pad) / cell));
    int maxShown = cols * rows;
    int n = (int) sel_.size();
    for (int i = 0; i < n && i < maxShown; ++i) {
        const Entity &e = ents_[sel_[i]];
        Rectf c{r.x + pad * 0.5f + (i % cols) * cell, r.y + pad * 0.5f + (i / cols) * cell, cell - 4 * u,
                cell - 4 * u};
        if (i == maxShown - 1 && n > maxShown) {
            d.textCentered("+" + std::to_string(n - maxShown + 1), c.x + c.w * 0.5f, c.y + c.h * 0.35f, 2 * u, kWhite);
            break;
        }
        d.rect(c, rgba(.1f, .12f, .15f));
        drawIcon(d, e.type, e.owner, {c.x + c.w * 0.5f, c.y + c.h * 0.42f}, c.w * 0.6f);
        drawHealthBar(d, {c.x + c.w * 0.5f, c.y + c.h - 6 * u}, c.w - 8 * u, e.hp / kTypes[e.type].hp, 3 * u);
    }
}

// ---------------------------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------------------------

void Game::renderTitle(Draw2D &d) {
    float u = ui_;
    d.rect(0, 0, screenW_, screenH_, rgba(.02f, .03f, .07f));
    Rng r(1234);
    for (int i = 0; i < 220; ++i) {
        float x = r.f() * screenW_, y = r.f() * screenH_, s = r.range(1, 2.6f) * u;
        float tw = 0.55f + 0.45f * std::sin(realTime_ * r.range(0.5f, 2.5f) + i);
        d.rect(x, y, s, s, rgba(.8f, .85f, 1, tw));
    }
    Vec2 planet{screenW_ * 0.84f, screenH_ * 1.05f};
    float pr = screenH_ * 0.55f;
    d.circle(planet, pr * 1.04f, rgba(.3f, .5f, 1, .12f), 64);
    d.circle(planet, pr, rgba(.22f, .15f, .12f), 64);
    d.circle(planet + Vec2(-pr * 0.1f, -pr * 0.1f), pr * 0.88f, rgba(.32f, .22f, .16f), 64);
    d.circle(planet + Vec2(-pr * 0.3f, -pr * 0.35f), pr * 0.2f, rgba(.26f, .18f, .13f), 32);

    float tpx = std::min(10 * u, screenW_ / (13 * 6.f + 4));
    d.textCentered("ASTRA PUGNA", screenW_ * 0.5f + 4 * u, screenH_ * 0.16f + 4 * u, tpx, rgba(0, 0, 0, .6f));
    d.textCentered("ASTRA PUGNA", screenW_ * 0.5f, screenH_ * 0.16f, tpx, rgba(.4f, .75f, 1));
    d.textCentered("A REAL-TIME STRATEGY GAME", screenW_ * 0.5f, screenH_ * 0.16f + 9 * tpx, 2.4f * u,
                   rgba(.75f, .8f, .9f));

    Rectf b[3];
    titleButtons(b);
    d.textCentered("CHOOSE DIFFICULTY", screenW_ * 0.5f, b[0].y - 34 * u, 2.2f * u, rgba(.7f, .75f, .85f));
    const char *names[3] = {"EASY", "NORMAL", "HARD"};
    const Color cols[3] = {rgba(.3f, .85f, .45f), rgba(.35f, .65f, 1), rgba(1, .4f, .3f)};
    for (int i = 0; i < 3; ++i) {
        bool pressed = numTouches_ > 0 && b[i].contains(touches_[0].cur);
        d.rect(b[i], pressed ? shade(cols[i], 0.5f) : rgba(.08f, .1f, .14f, .9f));
        d.rectOutline(b[i], 3 * u, cols[i]);
        d.textCentered(names[i], b[i].x + b[i].w * 0.5f, b[i].y + (b[i].h - 21 * u) * 0.5f, 3 * u, kWhite);
    }
    d.textCentered("MINE CRYSTALS - BUILD AN ARMY - DESTROY EVERY ENEMY STRUCTURE\n", screenW_ * 0.5f,
                   screenH_ * 0.76f, 1.9f * u, rgba(.85f, .85f, .9f));
    d.textCentered("TAP: SELECT / COMMAND    HOLD + DRAG: BOX SELECT    DRAG: SCROLL    PINCH: ZOOM",
                   screenW_ * 0.5f, screenH_ * 0.76f + 30 * u, 1.6f * u, rgba(.6f, .65f, .75f));
}

void Game::renderEnd(Draw2D &d) {
    float u = ui_;
    float a = std::min(1.f, endTimer_);
    d.rect(0, 0, screenW_, screenH_, rgba(0, 0, 0, .65f * a));
    const char *title = victory_ ? "VICTORY" : "DEFEAT";
    Color col = victory_ ? rgba(.4f, 1, .5f) : rgba(1, .35f, .3f);
    float tpx = 12 * u;
    d.textCentered(title, screenW_ * 0.5f, screenH_ * 0.26f, tpx, fade(col, a));
    std::string stats = "GAME TIME " + clockText(gameTime_) + "\nYOUR LOSSES " + std::to_string(lost_[PLAYER]) +
                        "\nENEMY LOSSES " + std::to_string(lost_[ENEMY]);
    d.textCentered(stats, screenW_ * 0.5f, screenH_ * 0.26f + 12 * tpx, 2.6f * u, fade(kWhite, a));
    if (endTimer_ > 1.5f && std::fmod(realTime_, 1.2f) < 0.8f) {
        d.textCentered("TAP TO CONTINUE", screenW_ * 0.5f, screenH_ * 0.72f, 2.4f * u, rgba(.8f, .85f, 1));
    }
}

void Game::render(Draw2D &d, int width, int height) {
    screenW_ = (float) width;
    screenH_ = (float) height;
    layout();
    glClearColor(.02f, .02f, .03f, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    if (screen_ == SCR_TITLE) {
        d.setView(0, 0, screenW_, screenH_);
        renderTitle(d);
        d.flush();
        return;
    }
    if (needCenter_) {
        int hq = nearestHQ(PLAYER, {0, 0});
        if (hq >= 0) centerCamera(ents_[hq].pos);
        needCenter_ = false;
    }
    clampCamera();
    d.setView(cam_.x, cam_.y, cam_.x + screenW_ / scale_, cam_.y + screenH_ / scale_);
    renderWorld(d);
    d.setView(0, 0, screenW_, screenH_);
    renderHud(d);
    if (screen_ == SCR_END) renderEnd(d);
    d.flush();
}
