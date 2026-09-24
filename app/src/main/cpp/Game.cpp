#include "Game.h"

#include <android/log.h>
#include <ctime>
#include <functional>
#include <queue>

// clang-format off
const TypeDef kTypes[T_COUNT] = {
//   name            bld    rad  tw th  hp    arm  speed  range      dmg  cd     splash sight      cost sup prov time  requires    producer
    {"WORKER",       false, 9,   0, 0,  40,   0,   88,    6,         5,   1.1f,  0,     7 * TILE,  50,  1,  0,   10,  T_COUNT,    T_HQ},
    {"MARINE",       false, 9,   0, 0,  45,   0,   80,    5 * TILE,  6,   0.8f,  0,     9 * TILE,  50,  1,  0,   14,  T_COUNT,    T_BARRACKS},
    {"SHARPSHOOTER", false, 9,   0, 0,  35,   0,   72,    8 * TILE,  24,  2.4f,  0,     10 * TILE, 75,  1,  0,   20,  T_COUNT,    T_BARRACKS},
    {"OFFICER",      false, 9,   0, 0,  60,   1,   80,    4 * TILE,  4,   1.0f,  0,     9 * TILE,  100, 2,  0,   24,  T_FACTORY,  T_BARRACKS},
    {"TANK",         false, 15,  0, 0,  160,  1,   64,    7 * TILE,  28,  2.2f,  40,    10 * TILE, 150, 3,  0,   24,  T_COUNT,    T_FACTORY},
    {"ROVER",        false, 13,  0, 0,  120,  1,   128,   0,         0,   0,     0,     9 * TILE,  100, 2,  0,   18,  T_COUNT,    T_FACTORY},
    {"COMMAND HQ",   true,  0,   4, 4,  1500, 1,   0,     0,         0,   0,     0,     11 * TILE, 400, 0,  10,  60,  T_COUNT,    T_COUNT},
    {"SUPPLY DEPOT", true,  0,   2, 2,  400,  1,   0,     0,         0,   0,     0,     7 * TILE,  100, 0,  8,   18,  T_COUNT,    T_COUNT},
    {"BARRACKS",     true,  0,   3, 3,  1000, 1,   0,     0,         0,   0,     0,     8 * TILE,  150, 0,  0,   35,  T_DEPOT,    T_COUNT},
    {"FACTORY",      true,  0,   3, 3,  1250, 1,   0,     0,         0,   0,     0,     8 * TILE,  200, 0,  0,   40,  T_BARRACKS, T_COUNT},
    {"TURRET",       true,  0,   2, 2,  300,  1,   0,     7 * TILE,  12,  0.9f,  0,     10 * TILE, 100, 0,  0,   22,  T_BARRACKS, T_COUNT},
    {"MINERALS",     true,  0,   2, 1,  1,    0,   0,     0,         0,   0,     0,     0,         0,   0,  0,   0,   T_COUNT,    T_COUNT},
};
// clang-format on

static constexpr float kMineTime = 2.0f;
static constexpr int kCargo = 5;

Game::Game() : rng_((uint32_t) time(nullptr)) {
    terrain_.assign(MAP_W * MAP_H, 0);
    occ_.assign(MAP_W * MAP_H, -1);
    vis_.assign(MAP_W * MAP_H, 0);
    shade_.assign(MAP_W * MAP_H, 0);
    gCost_.assign(MAP_W * MAP_H, 0);
    parent_.assign(MAP_W * MAP_H, -1);
    openGen_.assign(MAP_W * MAP_H, 0);
    closedGen_.assign(MAP_W * MAP_H, 0);
}

// ---------------------------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------------------------

void Game::newGame(int difficulty) {
    difficulty_ = difficulty;
    ents_.clear();
    live_.clear();
    fx_.clear();
    shots_.clear();
    sel_.clear();
    minerals_[0] = minerals_[1] = 50;
    lost_[0] = lost_[1] = 0;
    gameTime_ = simAccum_ = fogTimer_ = aiTimer_ = aiBank_ = 0;
    alertCooldown_ = pingT_ = msgT_ = 0;
    mode_ = MODE_NORMAL;
    gesture_ = G_NONE;
    numTouches_ = 0;

    static const float kIncome[3] = {0.75f, 1.0f, 1.3f};
    static const int kWave[3] = {10, 8, 7};
    static const float kFirst[3] = {360, 250, 170};
    aiIncome_ = kIncome[difficulty];
    aiWaveSize_ = kWave[difficulty];
    aiFirstAttack_ = kFirst[difficulty];
    aiWave_ = 0;
    aiAttacking_ = false;

    generateMap(rng_.next());
    vis_.assign(MAP_W * MAP_H, 0);

    // Main bases sit in opposite corners; everything is point-symmetric for fairness.
    setupBase(PLAYER, 9, 9, -0.75f * kPi);
    setupBase(ENEMY, MAP_W - 9 - 4, MAP_H - 9 - 4, 0.25f * kPi);

    // Expansion mineral fields
    placeMinerals(11, 42, kPi, 7, 6.f, 1200);
    placeMinerals(MAP_W - 11, MAP_H - 42, 0, 7, 6.f, 1200);
    placeMinerals(42, 11, -0.5f * kPi, 7, 6.f, 1200);
    placeMinerals(MAP_W - 42, MAP_H - 11, 0.5f * kPi, 7, 6.f, 1200);

    rebuildLive();
    updateSupply();
    updateFog();
    screen_ = SCR_PLAY;
    needCenter_ = true;
    message("DESTROY ALL ENEMY STRUCTURES");
}

void Game::generateMap(uint32_t seed) {
    struct Zone {
        float x, y, r;
    };
    std::vector<Zone> zones = {{11, 11, 12}, {11, 42, 9}, {42, 11, 9}, {48, 36, 5}};
    size_t base = zones.size();
    for (size_t i = 0; i < base; ++i) {
        zones.push_back({MAP_W - zones[i].x, MAP_H - zones[i].y, zones[i].r});
    }
    const int N = MAP_W * MAP_H;

    for (int attempt = 0; attempt < 64; ++attempt) {
        Rng r(seed + attempt * 7919u);
        terrain_.assign(N, 0);
        occ_.assign(N, -1);

        auto paint = [&](int x, int y) {
            if (!inMap(x, y)) return;
            terrain_[idx(x, y)] = 1;
            terrain_[idx(MAP_W - 1 - x, MAP_H - 1 - y)] = 1;
        };
        auto isProtected = [&](float x, float y, float pad) {
            for (const auto &z: zones) {
                if (dist({x, y}, {z.x, z.y}) < z.r + pad) return true;
            }
            return false;
        };

        for (int x = 0; x < MAP_W; ++x) {
            paint(x, 0);
            paint(x, MAP_H - 1);
        }
        for (int y = 0; y < MAP_H; ++y) {
            paint(0, y);
            paint(MAP_W - 1, y);
        }

        // Rock outcrops
        for (int i = 0; i < 24; ++i) {
            float cx = r.range(3, MAP_W - 3), cy = r.range(3, MAP_H - 3), rad = r.range(1.2f, 3.6f);
            if (isProtected(cx, cy, rad + 1)) continue;
            for (int y = (int) (cy - rad - 1); y <= (int) (cy + rad + 1); ++y) {
                for (int x = (int) (cx - rad - 1); x <= (int) (cx + rad + 1); ++x) {
                    float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
                    if (dx * dx + dy * dy <= rad * rad) paint(x, y);
                }
            }
        }
        // Winding ridges that form chokes
        for (int i = 0; i < 7; ++i) {
            float x = r.range(8, MAP_W - 8), y = r.range(8, MAP_H - 8), ang = r.range(0, 2 * kPi);
            int len = r.irange(6, 16);
            for (int s = 0; s < len; ++s) {
                if (!isProtected(x, y, 2)) {
                    paint((int) x, (int) y);
                    paint((int) x + 1, (int) y);
                    paint((int) x, (int) y + 1);
                    paint((int) x + 1, (int) y + 1);
                }
                ang += r.range(-0.45f, 0.45f);
                x += std::cos(ang);
                y += std::sin(ang);
            }
        }

        // Connectivity: flood from the player main, every zone must be reachable.
        std::vector<uint8_t> reach(N, 0);
        std::vector<int> stack = {idx(11, 11)};
        reach[idx(11, 11)] = 1;
        while (!stack.empty()) {
            int c = stack.back();
            stack.pop_back();
            int cx = c % MAP_W, cy = c / MAP_W;
            const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; ++k) {
                int nx = cx + dx[k], ny = cy + dy[k];
                if (!inMap(nx, ny)) continue;
                int n = idx(nx, ny);
                if (terrain_[n] || reach[n]) continue;
                reach[n] = 1;
                stack.push_back(n);
            }
        }
        bool ok = true;
        for (const auto &z: zones) {
            if (!reach[idx((int) z.x, (int) z.y)]) ok = false;
        }
        if (!ok && attempt < 63) continue;
        for (int i = 0; i < N; ++i) {
            if (!reach[i]) terrain_[i] = 1;
        }
        break;
    }
    Rng sr(seed ^ 0xA5A5A5A5u);
    for (auto &s: shade_) s = (uint8_t) (sr.next() & 0xFF);
}

void Game::placeMinerals(float cx, float cy, float angle, int count, float radius, int amount) {
    for (int k = 0; k < count; ++k) {
        float a = angle + (k - (count - 1) * 0.5f) * 0.34f;
        for (float rr = radius + (k % 2) * 0.8f; rr < radius + 3.5f; rr += 0.5f) {
            float fx = cx + std::cos(a) * rr, fy = cy + std::sin(a) * rr;
            int tx = (int) std::floor(fx - 1.f), ty = (int) std::floor(fy - 0.5f);
            if (tx < 1 || ty < 1 || tx + 2 > MAP_W - 1 || ty + 1 > MAP_H - 1) continue;
            if (blocked(tx, ty) || blocked(tx + 1, ty)) continue;
            int id = spawnBuilding(T_MINERAL, NEUTRAL, tx, ty, true);
            ents_[id].amount = amount;
            break;
        }
    }
}

void Game::setupBase(int owner, int tx, int ty, float mineralAngle) {
    int hq = spawnBuilding(T_HQ, owner, tx, ty, true);
    Vec2 c = ents_[hq].pos;
    placeMinerals(c.x / TILE, c.y / TILE, mineralAngle, 8, 5.8f, 1500);
    rebuildLive();   // orderGather() searches live_ for the HQ and minerals
    for (int i = 0; i < 6; ++i) {
        Vec2 p = c + fromAngle(mineralAngle + (i - 2.5f) * 0.35f) * (3.f * TILE);
        int w = spawn(T_WORKER, owner, p);
        orderGather(ents_[w], -1);
    }
}

int Game::spawn(EType t, int owner, Vec2 pos) {
    Entity e;
    e.id = (int) ents_.size();
    e.type = t;
    e.owner = owner;
    e.pos = pos;
    e.dest = pos;
    e.stuckRef = pos;
    e.hp = kTypes[t].hp;
    e.facing = owner == PLAYER ? 0.25f * kPi : -0.75f * kPi;
    ents_.push_back(std::move(e));
    return (int) ents_.size() - 1;
}

int Game::spawnBuilding(EType t, int owner, int tx, int ty, bool complete) {
    const TypeDef &td = kTypes[t];
    int id = spawn(t, owner, footprintCenter(t, tx, ty));
    Entity &e = ents_[id];
    e.tx = tx;
    e.ty = ty;
    e.complete = complete;
    e.progress = complete ? 1.f : 0.f;
    e.hp = complete ? td.hp : td.hp * 0.1f;
    e.facing = owner == PLAYER ? 0.25f * kPi : -0.75f * kPi;
    for (int y = ty; y < ty + td.tilesH; ++y) {
        for (int x = tx; x < tx + td.tilesW; ++x) {
            if (inMap(x, y)) occ_[idx(x, y)] = id;
        }
    }
    return id;
}

Vec2 Game::footprintCenter(EType t, int tx, int ty) {
    return {(tx + kTypes[t].tilesW * 0.5f) * TILE, (ty + kTypes[t].tilesH * 0.5f) * TILE};
}

void Game::kill(Entity &e) {
    if (!e.alive) return;
    e.alive = false;
    e.hp = 0;
    const TypeDef &td = kTypes[e.type];
    if (Entity *v = ent(e.transport)) {
        v->passengers.erase(std::remove(v->passengers.begin(), v->passengers.end(), e.id), v->passengers.end());
    }
    if (isTransport(e.type)) {
        // Passengers bail out of a wrecked vehicle, and a tank's driver climbs out as a marine.
        unload(e);
        if (e.type == T_TANK && e.owner >= 0) {
            spawn(T_MARINE, e.owner, e.pos + fromAngle(e.facing + kPi) * (td.radius * 0.5f));
        }
    }
    if (td.building) {
        for (int y = e.ty; y < e.ty + td.tilesH; ++y) {
            for (int x = e.tx; x < e.tx + td.tilesW; ++x) {
                if (inMap(x, y) && occ_[idx(x, y)] == e.id) occ_[idx(x, y)] = -1;
            }
        }
    }
    if (e.owner >= 0) lost_[e.owner]++;
    if (e.type == T_MINERAL) return;
    if (td.building) {
        float hx, hy;
        halfExtents(e, hx, hy);
        for (int i = 0; i < 5; ++i) {
            Vec2 p = e.pos + Vec2(rng_.range(-hx, hx), rng_.range(-hy, hy)) * 0.7f;
            addFx(FX_EXPLODE, p, p, 0.6f + rng_.f() * 0.5f, hx * rng_.range(0.6f, 1.1f),
                  rgba(1, .6f, .2f));
        }
    } else {
        addFx(FX_EXPLODE, e.pos, e.pos, 0.45f, td.radius * 2.2f, rgba(1, .55f, .2f));
    }
}

// ---------------------------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------------------------

void Game::update(float dt) {
    realTime_ += dt;
    msgT_ -= dt;
    for (auto &f: fx_) f.t += dt;
    fx_.erase(std::remove_if(fx_.begin(), fx_.end(), [](const Fx &f) { return f.t >= f.dur; }),
              fx_.end());

    if (screen_ == SCR_PLAY) {
        // A finger held still on the map turns into a selection box.
        if (gesture_ == G_TAP && numTouches_ == 1 && realTime_ - touches_[0].t0 > 0.3f) {
            gesture_ = G_BOX;
        }
        const float kStep = 1.f / 60.f;
        simAccum_ += std::min(dt, 0.1f);
#ifdef SIM_FAST
        simAccum_ += std::min(dt, 0.1f) * 7;
#endif
        while (simAccum_ >= kStep && screen_ == SCR_PLAY) {
            step(kStep);
            simAccum_ -= kStep;
        }
    } else if (screen_ == SCR_END) {
        endTimer_ += dt;
    }
}

void Game::rebuildLive() {
    live_.clear();
    for (size_t i = 0; i < ents_.size(); ++i) {
        if (ents_[i].alive) live_.push_back((int) i);
    }
}

void Game::step(float dt) {
    gameTime_ += dt;
    alertCooldown_ -= dt;
    pingT_ -= dt;

    rebuildLive();
    size_t n = live_.size();
    for (size_t k = 0; k < n; ++k) {
        Entity &e = ents_[live_[k]];
        if (!e.alive) continue;
        if (isBuilding(e)) {
            if (e.type != T_MINERAL) updateBuilding(e, dt);
        } else {
            updateUnit(e, dt);
        }
    }
    updateShots(dt);
    rebuildLive();
    resolveCollisions();
    updateSupply();

    fogTimer_ -= dt;
    if (fogTimer_ <= 0) {
        updateFog();
        fogTimer_ = 0.1f;
    }
    aiTimer_ -= dt;
    if (aiTimer_ <= 0) {
        aiThink();
        aiTimer_ = 0.5f;
    }
    checkVictory();
#ifdef SIM_FAST
    if ((int) (gameTime_ / 30) != (int) ((gameTime_ - dt) / 30)) {
        int u[2] = {0, 0}, b[2] = {0, 0}, w[2] = {0, 0};
        for (int id: live_) {
            const Entity &e = ents_[id];
            if (!e.alive || e.owner < 0) continue;
            if (isBuilding(e)) b[e.owner]++; else if (e.type == T_WORKER) w[e.owner]++; else u[e.owner]++;
        }
        __android_log_print(ANDROID_LOG_INFO, "SIMTEST",
                            "t=%d P[min=%d sup=%d/%d w=%d army=%d bld=%d] E[min=%d sup=%d/%d w=%d army=%d bld=%d] wave=%d attacking=%d fx=%d",
                            (int) gameTime_, minerals_[0], supplyUsed_[0], supplyCap_[0], w[0], u[0], b[0],
                            minerals_[1], supplyUsed_[1], supplyCap_[1], w[1], u[1], b[1], aiWave_, aiAttacking_, (int) fx_.size());
    }
#endif
}

void Game::checkVictory() {
    int buildings[2] = {0, 0};
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (e.alive && e.owner >= 0 && isBuilding(e)) buildings[e.owner]++;
    }
    if (buildings[PLAYER] == 0 || buildings[ENEMY] == 0) {
        victory_ = buildings[ENEMY] == 0 && buildings[PLAYER] > 0;
        screen_ = SCR_END;
        endTimer_ = 0;
        mode_ = MODE_NORMAL;
    }
}

void Game::updateSupply() {
    for (int s = 0; s < 2; ++s) supplyUsed_[s] = supplyCap_[s] = 0;
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (!e.alive || e.owner < 0) continue;
        const TypeDef &td = kTypes[e.type];
        if (!td.building) {
            supplyUsed_[e.owner] += td.supply;
        } else {
            if (e.complete) supplyCap_[e.owner] += td.supplyProvided;
            for (EType q: e.queue) supplyUsed_[e.owner] += kTypes[q].supply;
        }
    }
    for (int &c: supplyCap_) c = std::min(c, MAX_SUPPLY);
}

void Game::updateFog() {
    for (auto &v: vis_) {
        if (v == 2) v = 1;
    }
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (!e.alive || e.owner != PLAYER) continue;
        float sight = kTypes[e.type].sight / TILE;
        if (isBuilding(e) && !e.complete) sight = std::max(3.f, sight * 0.5f);
        int R = (int) std::ceil(sight);
        int cx = (int) (e.pos.x / TILE), cy = (int) (e.pos.y / TILE);
        float r2 = sight * sight;
        for (int dy = -R; dy <= R; ++dy) {
            for (int dx = -R; dx <= R; ++dx) {
                if (dx * dx + dy * dy > r2 || !inMap(cx + dx, cy + dy)) continue;
                vis_[idx(cx + dx, cy + dy)] = 2;
            }
        }
    }
    for (int id: live_) {
        Entity &e = ents_[id];
        if (e.alive && e.owner == ENEMY && isBuilding(e) && !e.seen && visibleToPlayer(e)) {
            e.seen = true;
        }
    }
}

bool Game::visibleToPlayer(const Entity &e) const {
    if (e.owner == PLAYER) return true;
    if (isBuilding(e)) {
        const TypeDef &td = kTypes[e.type];
        for (int y = e.ty; y < e.ty + td.tilesH; ++y) {
            for (int x = e.tx; x < e.tx + td.tilesW; ++x) {
                if (tileVisible(x, y)) return true;
            }
        }
        return false;
    }
    return tileVisible((int) (e.pos.x / TILE), (int) (e.pos.y / TILE));
}

void Game::addFx(FxKind k, Vec2 a, Vec2 b, float dur, float size, Color c) {
    if (fx_.size() > 600) return;
    fx_.push_back({k, a, b, 0, dur, size, c});
}

void Game::message(const std::string &m) {
    msg_ = m;
    msgT_ = 2.5f;
}

// ---------------------------------------------------------------------------------------------
// Units
// ---------------------------------------------------------------------------------------------

void Game::updateUnit(Entity &e, float dt) {
    if (inside(e)) {
        updatePassenger(e, dt);
        return;
    }
    const TypeDef &td = kTypes[e.type];
    const bool armed = td.damage > 0;
    e.cd -= dt;
    e.repath -= dt;
    e.scan -= dt;
    e.flash = std::max(0.f, e.flash - dt);
    e.underAttack -= dt;
    e.moving = false;
    e.firing = false;

    bool workerJob = e.type == T_WORKER &&
                     (e.order == O_GATHER || e.order == O_RETURN || e.order == O_BUILD ||
                      e.order == O_CONSTRUCT);
    if (workerJob) {
        updateWorker(e, dt);
    } else {
        switch (e.order) {
            case O_IDLE:
                if (e.type != T_WORKER && armed && e.scan <= 0) {
                    e.scan = 0.25f;
                    int t = acquire(e, td.sight);
                    if (t >= 0) {
                        e.order = O_ATTACK_MOVE;
                        e.dest = e.pos;
                        e.target = t;
                        e.path.clear();
                        e.pathIdx = 0;
                    }
                }
                break;
            case O_HOLD: {
                if (!armed) break;
                Entity *t = ent(e.target);
                if (!t || !canTarget(e, *t) || !inRange(e, *t)) {
                    t = nullptr;
                    e.target = -1;
                    if (e.scan <= 0) {
                        e.scan = 0.2f;
                        e.target = acquire(e, td.range);
                        t = ent(e.target);
                    }
                }
                if (t) fight(e, *t, dt, false);
                break;
            }
            case O_MOVE:
                if (follow(e, dt)) e.order = O_IDLE;
                break;
            case O_ATTACK_MOVE: {
                // Unarmed vehicles just drive; their passengers do the shooting.
                Entity *t = armed ? ent(e.target) : nullptr;
                if (t && (!canTarget(e, *t) || gap(e, *t) > td.sight * 1.25f)) {
                    t = nullptr;
                    e.target = -1;
                    e.path.clear();
                    e.pathIdx = 0;
                }
                if (!t && armed && e.scan <= 0) {
                    e.scan = 0.2f;
                    e.target = acquire(e, td.sight);
                    t = ent(e.target);
                    if (t) {
                        e.path.clear();
                        e.pathIdx = 0;
                    }
                }
                if (t) {
                    fight(e, *t, dt, true);
                    break;
                }
                if (e.pathIdx >= e.path.size()) {
                    if (dist(e.pos, e.dest) <= TILE * 1.5f) {
                        e.order = O_IDLE;
                        break;
                    }
                    if (e.repath <= 0) {
                        setPath(e, e.dest);
                        e.repath = 1.f;
                    }
                }
                follow(e, dt);
                break;
            }
            case O_ATTACK: {
                Entity *t = ent(e.target);
                if (!t || !canTarget(e, *t)) {
                    e.order = O_IDLE;
                    e.target = -1;
                    e.path.clear();
                    e.pathIdx = 0;
                    break;
                }
                if (armed) fight(e, *t, dt, true);
                else approach(e, *t, 3.5f * TILE, dt);   // close in so passengers can fire
                break;
            }
            case O_BOARD: {
                Entity *v = ent(e.target);
                if (!v || v->owner != e.owner || !isTransport(v->type) ||
                    (int) v->passengers.size() >= kTransportSlots) {
                    orderStop(e, false);
                    break;
                }
                if (approach(e, *v, 6.f, dt)) board(e, *v);
                break;
            }
            default:
                e.order = O_IDLE;
                break;
        }
    }

    // Stuck detection: if a moving unit barely makes progress, repath or give up.
    if (e.moving) {
        e.stuckT += dt;
        if (e.stuckT >= 0.5f) {
            float moved = dist(e.pos, e.stuckRef);
            e.stuckCount = moved < td.speed * 0.5f * 0.25f ? e.stuckCount + 1 : 0;
            e.stuckRef = e.pos;
            e.stuckT = 0;
            if (e.stuckCount >= 3) {
                e.stuckCount = 0;
                if (e.order == O_MOVE || (e.order == O_ATTACK_MOVE && e.target < 0)) {
                    if (dist(e.pos, e.dest) < TILE * 4 || ++e.repathCount > 3) {
                        e.order = O_IDLE;
                        e.path.clear();
                        e.pathIdx = 0;
                    } else {
                        setPath(e, e.dest);
                    }
                } else {
                    e.repath = 0;
                    e.path.clear();
                    e.pathIdx = 0;
                }
            }
        }
    } else {
        e.stuckT = 0;
        e.stuckCount = 0;
        e.stuckRef = e.pos;
    }
}

void Game::updatePassenger(Entity &e, float dt) {
    Entity *v = ent(e.transport);
    if (!v) {
        e.transport = -1;
        orderStop(e, false);
        return;
    }
    e.pos = v->pos;
    e.dest = v->pos;
    e.cd -= dt;
    e.scan -= dt;
    e.flash = std::max(0.f, e.flash - dt);
    e.moving = e.firing = false;
    if (v->type != T_ROVER || kTypes[e.type].damage <= 0) return;

    // Rover passengers shoot on the move, favoring whatever the driver was told to attack.
    Entity *t = nullptr;
    if (v->order == O_ATTACK) {
        t = ent(v->target);
        if (t && (!canTarget(e, *t) || !inRange(e, *t))) t = nullptr;
    }
    if (!t) {
        t = ent(e.target);
        if (t && (!canTarget(e, *t) || !inRange(e, *t))) t = nullptr;
    }
    if (!t && e.scan <= 0) {
        e.scan = 0.2f;
        t = ent(acquire(e, kTypes[e.type].range));
    }
    e.target = t ? t->id : -1;
    if (t) fight(e, *t, dt, false);
}

void Game::board(Entity &e, Entity &vehicle) {
    vehicle.passengers.push_back(e.id);
    e.transport = vehicle.id;
    e.pos = vehicle.pos;
    e.order = O_IDLE;
    e.target = -1;
    e.path.clear();
    e.pathIdx = 0;
}

void Game::unload(Entity &vehicle) {
    float r = unitRadius(vehicle) + 10.f;
    for (size_t i = 0; i < vehicle.passengers.size(); ++i) {
        Entity *p = ent(vehicle.passengers[i]);
        if (!p) continue;
        p->transport = -1;
        p->pos = vehicle.pos + fromAngle(vehicle.facing + kPi * 0.5f + i * kPi * 0.5f) * r;
        orderStop(*p, false);
        p->cd = 0.3f;
        pushOutOfTerrain(*p);
    }
    vehicle.passengers.clear();
}

bool Game::mineralBusy(const Entity &m, int except) const {
    const Entity *w = ent(m.miner);
    return w && w->id != except && w->order == O_GATHER && w->mineral == m.id && w->work > 0;
}

int Game::findFreeMineral(Vec2 p, float maxDist, int exclude) const {
    int best = -1;
    float bestD = maxDist;
    for (int id: live_) {
        const Entity &m = ents_[id];
        if (!m.alive || m.type != T_MINERAL || id == exclude || mineralBusy(m, -1)) continue;
        float d = dist(p, m.pos);
        if (d < bestD) {
            bestD = d;
            best = id;
        }
    }
    return best;
}

void Game::updateWorker(Entity &e, float dt) {
    switch (e.order) {
        case O_GATHER: {
            if (e.cargo >= kCargo) {
                e.order = O_RETURN;
                e.path.clear();
                e.pathIdx = 0;
                return;
            }
            Entity *m = ent(e.mineral);
            if (!m || m->type != T_MINERAL) {
                int hq = nearestHQ(e.owner, e.pos);
                e.mineral = nearestMineral(hq >= 0 ? ents_[hq].pos : e.pos, 14 * TILE);
                m = ent(e.mineral);
                e.path.clear();
                e.pathIdx = 0;
                if (!m) {
                    e.order = e.cargo > 0 ? O_RETURN : O_IDLE;
                    return;
                }
            }
            if (!approach(e, *m, 5.f, dt)) {
                e.work = 0;
                return;
            }
            Vec2 d = m->pos - e.pos;
            e.facing = std::atan2(d.y, d.x);
            if (mineralBusy(*m, e.id)) {
                // Someone else is mining this patch; hop to a free one nearby if there is one.
                int alt = findFreeMineral(e.pos, 5 * TILE, m->id);
                if (alt >= 0) {
                    e.mineral = alt;
                    e.path.clear();
                    e.pathIdx = 0;
                }
                e.work = 0;
                return;
            }
            m->miner = e.id;
            e.work += dt;
            if (rng_.f() < dt * 6) {
                Vec2 p = closestPoint(*m, e.pos);
                addFx(FX_SPARK, p, p, 0.12f, 4, rgba(.5f, .95f, 1));
            }
            if (e.work >= kMineTime) {
                int take = std::min(kCargo, m->amount);
                m->amount -= take;
                e.cargo = take;
                e.work = 0;
                m->miner = -1;
                if (m->amount <= 0) kill(*m);
                e.order = O_RETURN;
                e.path.clear();
                e.pathIdx = 0;
            }
            break;
        }
        case O_RETURN: {
            int h = nearestHQ(e.owner, e.pos);
            if (h < 0) {
                e.order = O_IDLE;
                return;
            }
            if (!approach(e, ents_[h], 6.f, dt)) return;
            if (e.owner == ENEMY) {
                aiBank_ += e.cargo * aiIncome_;
                int whole = (int) aiBank_;
                minerals_[ENEMY] += whole;
                aiBank_ -= whole;
            } else {
                minerals_[e.owner] += e.cargo;
            }
            e.cargo = 0;
            e.order = O_GATHER;
            e.path.clear();
            e.pathIdx = 0;
            break;
        }
        case O_BUILD: {
            const TypeDef &bt = kTypes[e.buildType];
            Vec2 c = footprintCenter(e.buildType, e.bx, e.by);
            float reach = std::max(bt.tilesW, bt.tilesH) * TILE * 0.5f + 20.f;
            if (dist(e.pos, c) > reach) {
                if (e.pathIdx >= e.path.size()) {
                    if (e.repath <= 0) {
                        setPath(e, c);
                        e.repath = 1.f;
                    } else {
                        moveDirect(e, c, dt);
                    }
                }
                follow(e, dt);
                return;
            }
            bool isPlayer = e.owner == PLAYER;
            if (!canPlace(e.buildType, e.bx, e.by, e.owner)) {
                if (isPlayer) message("CAN'T BUILD THERE");
                e.order = O_IDLE;
                return;
            }
            if (bt.requires != T_COUNT && !hasComplete(e.owner, bt.requires)) {
                if (isPlayer) message(std::string("REQUIRES ") + kTypes[bt.requires].name);
                e.order = O_IDLE;
                return;
            }
            if (minerals_[e.owner] < bt.cost) {
                if (isPlayer) message("NOT ENOUGH MINERALS");
                e.order = O_IDLE;
                return;
            }
            minerals_[e.owner] -= bt.cost;
            int b = spawnBuilding(e.buildType, e.owner, e.bx, e.by, false);
            orderConstruct(e, b);
            break;
        }
        case O_CONSTRUCT: {
            Entity *b = ent(e.target);
            if (!b || b->complete) {
                orderGather(e, -1);
                return;
            }
            if (b->builder != e.id) {
                const Entity *other = ent(b->builder);
                if (other && other->order == O_CONSTRUCT && other->target == b->id) {
                    orderGather(e, -1);
                    return;
                }
                b->builder = e.id;
            }
            if (!approach(e, *b, 10.f, dt)) return;
            const TypeDef &bt = kTypes[b->type];
            float inc = dt / bt.buildTime;
            b->progress += inc;
            b->hp = std::min(bt.hp, b->hp + bt.hp * 0.9f * inc);
            Vec2 p = closestPoint(*b, e.pos);
            Vec2 d = p - e.pos;
            e.facing = std::atan2(d.y, d.x);
            if (rng_.f() < dt * 10) addFx(FX_SPARK, p, p, 0.15f, 5, rgba(1, .85f, .4f));
            if (b->progress >= 1.f) {
                b->progress = 1.f;
                b->complete = true;
                if (b->owner == PLAYER) message(std::string(bt.name) + " COMPLETE");
                orderGather(e, -1);
            }
            break;
        }
        default:
            break;
    }
}

// ---------------------------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------------------------

void Game::setPath(Entity &e, Vec2 goal) {
    findPath(e.pos, goal, e.path);
    e.pathIdx = 0;
    e.stuckCount = 0;
    e.stuckT = 0;
    e.stuckRef = e.pos;
}

bool Game::follow(Entity &e, float dt) {
    const float speed = kTypes[e.type].speed;
    while (e.pathIdx < e.path.size()) {
        Vec2 wp = e.path[e.pathIdx];
        Vec2 d = wp - e.pos;
        float l = length(d);
        bool last = e.pathIdx + 1 == e.path.size();
        if (l <= (last ? 2.f : TILE * 0.45f)) {
            e.pathIdx++;
            continue;
        }
        float step = speed * dt;
        e.pos += d * (std::min(step, l) / l);
        e.facing = std::atan2(d.y, d.x);
        e.moving = true;
        return false;
    }
    return true;
}

void Game::moveDirect(Entity &e, Vec2 goal, float dt) {
    Vec2 d = goal - e.pos;
    float l = length(d);
    if (l < 0.5f) return;
    float step = kTypes[e.type].speed * dt;
    e.pos += d * (std::min(step, l) / l);
    e.facing = std::atan2(d.y, d.x);
    e.moving = true;
}

void Game::moveToward(Entity &e, const Entity &t, float dt, float repathInterval) {
    float r = unitRadius(e);
    Vec2 goal = t.pos;
    if (isBuilding(t)) {
        Vec2 cp = closestPoint(t, e.pos);
        goal = cp + normalize(e.pos - cp) * (r + 2.f);
    }
    if (lineClear(e.pos, goal, r * 0.7f)) {
        e.path.clear();
        e.pathIdx = 0;
        moveDirect(e, goal, dt);
        return;
    }
    if (e.repath <= 0 || e.pathIdx >= e.path.size()) {
        if (e.repath <= 0) {
            setPath(e, t.pos);
            e.repath = repathInterval;
        }
    }
    follow(e, dt);
}

bool Game::approach(Entity &e, const Entity &t, float within, float dt) {
    if (gap(e, t) <= within) {
        e.path.clear();
        e.pathIdx = 0;
        return true;
    }
    moveToward(e, t, dt, isBuilding(t) ? 1.f : 0.4f);
    return false;
}

void Game::resolveCollisions() {
    // Unit-vs-unit separation. Mining workers ghost through other units, like the classics.
    auto ghost = [](const Entity &e) {
        return e.type == T_WORKER && (e.order == O_GATHER || e.order == O_RETURN);
    };
    auto weight = [](const Entity &e) {
        if (e.order == O_HOLD || e.firing) return 0.15f;
        return e.moving ? 0.6f : 1.f;
    };
    std::vector<int> units;
    units.reserve(live_.size());
    for (int id: live_) {
        if (ents_[id].alive && !isBuilding(ents_[id]) && !inside(ents_[id])) units.push_back(id);
    }
    for (size_t i = 0; i < units.size(); ++i) {
        Entity &a = ents_[units[i]];
        if (ghost(a)) continue;
        float ra = unitRadius(a);
        for (size_t j = i + 1; j < units.size(); ++j) {
            Entity &b = ents_[units[j]];
            if (ghost(b)) continue;
            float minD = ra + unitRadius(b);
            Vec2 d = b.pos - a.pos;
            if (std::fabs(d.x) >= minD || std::fabs(d.y) >= minD) continue;
            float d2 = lengthSq(d);
            if (d2 >= minD * minD) continue;
            float l = std::sqrt(d2);
            Vec2 n = l > 1e-3f ? d / l : fromAngle((float) (units[i] * 7 + units[j]));
            float overlap = minD - l;
            float wa = weight(a), wb = weight(b);
            a.pos -= n * (overlap * wa / (wa + wb));
            b.pos += n * (overlap * wb / (wa + wb));
        }
    }
    for (int id: units) pushOutOfTerrain(ents_[id]);
}

void Game::pushOutOfTerrain(Entity &e) {
    float r = unitRadius(e);
    int tx = (int) std::floor(e.pos.x / TILE), ty = (int) std::floor(e.pos.y / TILE);
    if (blocked(tx, ty)) {
        int fx = tx, fy = ty;
        if (nearestFree(fx, fy, tx, ty)) {
            float lo = std::min(r, TILE * 0.5f);
            e.pos.x = clampf(e.pos.x, fx * TILE + lo, (fx + 1) * TILE - lo);
            e.pos.y = clampf(e.pos.y, fy * TILE + lo, (fy + 1) * TILE - lo);
        }
        return;
    }
    int x0 = (int) std::floor((e.pos.x - r) / TILE), x1 = (int) std::floor((e.pos.x + r) / TILE);
    int y0 = (int) std::floor((e.pos.y - r) / TILE), y1 = (int) std::floor((e.pos.y + r) / TILE);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (!blocked(x, y)) continue;
            Vec2 q{clampf(e.pos.x, x * TILE, (x + 1) * TILE), clampf(e.pos.y, y * TILE, (y + 1) * TILE)};
            Vec2 d = e.pos - q;
            float l2 = lengthSq(d);
            if (l2 < r * r && l2 > 1e-4f) {
                float l = std::sqrt(l2);
                e.pos += d * ((r - l) / l);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------------------------

float Game::gap(const Entity &a, const Entity &b) const {
    bool ab = isBuilding(a), bb = isBuilding(b);
    if (!ab && !bb) return dist(a.pos, b.pos) - unitRadius(a) - unitRadius(b);
    float ahx, ahy, bhx, bhy;
    halfExtents(a, ahx, ahy);
    halfExtents(b, bhx, bhy);
    if (!ab) return pointRectDist(a.pos, b.pos, bhx, bhy) - unitRadius(a);
    if (!bb) return pointRectDist(b.pos, a.pos, ahx, ahy) - unitRadius(b);
    float dx = std::max(std::fabs(a.pos.x - b.pos.x) - ahx - bhx, 0.f);
    float dy = std::max(std::fabs(a.pos.y - b.pos.y) - ahy - bhy, 0.f);
    return std::sqrt(dx * dx + dy * dy);
}

bool Game::inRange(const Entity &e, const Entity &t) const {
    return gap(e, t) <= kTypes[e.type].range;
}

bool Game::canTarget(const Entity &e, const Entity &t) const {
    if (!t.alive || t.owner == e.owner || t.owner == NEUTRAL || inside(t)) return false;
    return e.owner != PLAYER || visibleToPlayer(t);
}

int Game::acquire(const Entity &e, float range) const {
    int best = -1;
    float bestScore = 1e9f;
    for (int id: live_) {
        const Entity &t = ents_[id];
        if (!t.alive || t.owner == e.owner || t.owner == NEUTRAL || inside(t)) continue;
        float g = gap(e, t);
        if (g > range) continue;
        if (e.owner == PLAYER && !visibleToPlayer(t)) continue;
        // Prefer things that shoot back, then workers, then structures.
        float score = g;
        bool shootsBack = kTypes[t.type].damage > 0 || (t.type == T_ROVER && !t.passengers.empty());
        if (t.type == T_WORKER) score += 150.f;
        else if (!shootsBack) score += 400.f;
        if (score < bestScore) {
            bestScore = score;
            best = id;
        }
    }
    return best;
}

bool Game::buffed(const Entity &e) const {
    if (isBuilding(e)) return false;
    for (int id: live_) {
        const Entity &o = ents_[id];
        if (o.alive && o.type == T_OFFICER && o.owner == e.owner && o.id != e.id &&
            distSq(o.pos, e.pos) <= kAuraRadius * kAuraRadius) {
            return true;
        }
    }
    return false;
}

void Game::fight(Entity &e, Entity &t, float dt, bool canMove) {
    if (inRange(e, t)) {
        e.path.clear();
        e.pathIdx = 0;
        Vec2 d = t.pos - e.pos;
        e.facing = std::atan2(d.y, d.x);
        e.firing = true;
        if (e.cd <= 0) {
            fire(e, t);
            e.cd = kTypes[e.type].cooldown * (buffed(e) ? kAuraCooldown : 1.f);
        }
    } else if (canMove) {
        moveToward(e, t, dt, 0.5f);
    }
}

void Game::fire(Entity &e, Entity &t) {
    const TypeDef &td = kTypes[e.type];
    Vec2 aim = isBuilding(t) ? closestPoint(t, e.pos) : t.pos;
    Vec2 dir = normalize(aim - e.pos);
    float off = isBuilding(e) ? 16.f : td.radius + 7.f;
    Vec2 muzzle = e.pos + dir * off;
    float d = dist(muzzle, aim);
    float dmg = td.damage * (buffed(e) ? kAuraDamage : 1.f);
    switch (e.type) {
        case T_WORKER:
            addFx(FX_SPARK, aim, aim, 0.15f, 6, rgba(1, .9f, .4f));
            damage(t, dmg, e.id);
            break;
        case T_MARINE:
        case T_OFFICER: {
            Vec2 jitter{rng_.range(-4, 4), rng_.range(-4, 4)};
            addFx(FX_TRACER, muzzle, aim + jitter, 0.07f, 1.5f, rgba(1, .95f, .55f));
            addFx(FX_FLASH, muzzle, muzzle, 0.05f, 4, rgba(1, .9f, .5f));
            damage(t, dmg, e.id);
            break;
        }
        case T_SNIPER:
            addFx(FX_TRACER, muzzle, aim, 0.18f, 2.2f, rgba(.75f, .95f, 1));
            addFx(FX_FLASH, muzzle, muzzle, 0.08f, 6, rgba(.8f, .95f, 1));
            addFx(FX_SPARK, aim, aim, 0.15f, 6, rgba(.8f, .95f, 1));
            damage(t, dmg, e.id);
            break;
        case T_TANK:
            shots_.push_back({muzzle, aim, 0, std::max(0.12f, d / 650.f), -1, e.owner, e.id,
                              dmg, td.splash});
            addFx(FX_FLASH, muzzle, muzzle, 0.1f, 9, rgba(1, .8f, .4f));
            break;
        case T_TURRET:
            shots_.push_back({muzzle, aim, 0, std::max(0.08f, d / 900.f), t.id, e.owner, e.id,
                              dmg, 0});
            addFx(FX_FLASH, muzzle, muzzle, 0.06f, 6, rgba(.6f, .9f, 1));
            break;
        default:
            break;
    }
}

void Game::updateShots(float dt) {
    for (size_t i = 0; i < shots_.size();) {
        Shot &s = shots_[i];
        s.t += dt;
        if (s.t < s.dur) {
            ++i;
            continue;
        }
        Shot hit = s;
        shots_[i] = shots_.back();
        shots_.pop_back();
        Entity *tgt = ent(hit.target);
        if (hit.splash > 0) {
            addFx(FX_EXPLODE, hit.to, hit.to, 0.4f, hit.splash, rgba(1, .7f, .25f));
            splashDamage(hit.to, hit.splash, hit.damage, hit.owner, hit.attacker);
        } else if (tgt) {
            Vec2 p = isBuilding(*tgt) ? hit.to : tgt->pos;
            addFx(FX_EXPLODE, p, p, 0.2f, 7, rgba(.6f, .9f, 1));
            damage(*tgt, hit.damage, hit.attacker);
        }
    }
}

void Game::splashDamage(Vec2 p, float radius, float amount, int owner, int attacker) {
    for (int id: live_) {
        Entity &e = ents_[id];
        if (!e.alive || e.owner == owner || e.owner == NEUTRAL || inside(e)) continue;
        float d;
        if (isBuilding(e)) {
            float hx, hy;
            halfExtents(e, hx, hy);
            d = pointRectDist(p, e.pos, hx, hy);
        } else {
            d = std::max(0.f, dist(p, e.pos) - unitRadius(e));
        }
        if (d <= radius) damage(e, d <= radius * 0.4f ? amount : amount * 0.5f, attacker);
    }
}

void Game::damage(Entity &t, float amount, int attacker) {
    if (!t.alive) return;
    float armor = kTypes[t.type].armor + (buffed(t) ? kAuraArmor : 0.f);
    t.hp -= std::max(0.5f, amount - armor);
    t.flash = 0.12f;
    t.underAttack = 3.f;
    if (t.owner == PLAYER && alertCooldown_ <= 0 && !onScreen(t.pos)) {
        message(isBuilding(t) ? "YOUR BASE IS UNDER ATTACK" : "YOUR FORCES ARE UNDER ATTACK");
        pingPos_ = t.pos;
        pingT_ = 3.f;
        alertCooldown_ = 12.f;
    }
    if (t.hp <= 0) {
        kill(t);
        return;
    }
    // Idle soldiers shot from beyond their sight walk toward the shooter.
    const Entity *a = ent(attacker);
    if (a && !isBuilding(t) && t.type != T_WORKER && kTypes[t.type].damage > 0 && t.order == O_IDLE) {
        t.order = O_ATTACK_MOVE;
        t.dest = a->pos;
        t.target = -1;
        t.path.clear();
        t.pathIdx = 0;
        t.repath = 0;
    }
}

// ---------------------------------------------------------------------------------------------
// Buildings
// ---------------------------------------------------------------------------------------------

void Game::updateBuilding(Entity &e, float dt) {
    const TypeDef &td = kTypes[e.type];
    e.flash = std::max(0.f, e.flash - dt);
    e.underAttack -= dt;
    if (!e.complete) return;

    if (td.damage > 0) {
        e.cd -= dt;
        e.scan -= dt;
        Entity *t = ent(e.target);
        if (!t || !canTarget(e, *t) || !inRange(e, *t)) {
            t = nullptr;
            e.target = -1;
            if (e.scan <= 0) {
                e.scan = 0.2f;
                e.target = acquire(e, td.range);
                t = ent(e.target);
            }
        }
        if (t) {
            Vec2 d = t->pos - e.pos;
            e.facing = std::atan2(d.y, d.x);
            if (e.cd <= 0) {
                fire(e, *t);
                e.cd = td.cooldown;
            }
        }
    }

    if (!e.queue.empty()) {
        EType u = e.queue.front();
        e.trainT += dt;
        if (e.trainT >= kTypes[u].buildTime) {
            e.queue.erase(e.queue.begin());
            e.trainT = 0;
            spawnTrained(e, u);
        }
    }
}

void Game::spawnTrained(Entity &b, EType t) {
    const TypeDef &bt = kTypes[b.type];
    Vec2 want = b.hasRally ? b.rally : b.pos + Vec2(0, bt.tilesH * TILE);
    int bestX = -1, bestY = -1;
    float bestD = 1e18f;
    for (int y = b.ty - 1; y <= b.ty + bt.tilesH; ++y) {
        for (int x = b.tx - 1; x <= b.tx + bt.tilesW; ++x) {
            bool edge = x == b.tx - 1 || x == b.tx + bt.tilesW || y == b.ty - 1 || y == b.ty + bt.tilesH;
            if (!edge || blocked(x, y)) continue;
            float d = distSq(tileCenter(x, y), want);
            if (d < bestD) {
                bestD = d;
                bestX = x;
                bestY = y;
            }
        }
    }
    Vec2 p = bestX >= 0 ? tileCenter(bestX, bestY) : b.pos + Vec2(0, bt.tilesH * TILE * 0.5f + 12);
    int owner = b.owner;
    bool hasRally = b.hasRally;
    Vec2 rally = b.rally;
    int rallyTarget = b.rallyTarget;
    int id = spawn(t, owner, p);
    Entity &n = ents_[id];
    Vec2 d = want - p;
    n.facing = std::atan2(d.y, d.x);
    if (t == T_WORKER) {
        const Entity *m = ent(rallyTarget);
        if (hasRally && !(m && m->type == T_MINERAL)) orderMove(n, rally, false);
        else orderGather(n, m && m->type == T_MINERAL ? rallyTarget : -1);
    } else if (hasRally) {
        orderMove(n, rally, false);
    }
}

bool Game::tryTrain(Entity &b, EType t) {
    const TypeDef &td = kTypes[t];
    bool isPlayer = b.owner == PLAYER;
    if (!b.complete) return false;
    if (b.queue.size() >= 5) {
        if (isPlayer) message("QUEUE IS FULL");
        return false;
    }
    if (minerals_[b.owner] < td.cost) {
        if (isPlayer) message("NOT ENOUGH MINERALS");
        return false;
    }
    if (supplyUsed_[b.owner] + td.supply > supplyCap_[b.owner]) {
        if (isPlayer) {
            message(supplyCap_[b.owner] >= MAX_SUPPLY ? "SUPPLY LIMIT REACHED"
                                                      : "NOT ENOUGH SUPPLY - BUILD DEPOTS");
        }
        return false;
    }
    minerals_[b.owner] -= td.cost;
    supplyUsed_[b.owner] += td.supply;
    b.queue.push_back(t);
    return true;
}

// ---------------------------------------------------------------------------------------------
// Orders
// ---------------------------------------------------------------------------------------------

void Game::orderMove(Entity &e, Vec2 p, bool attackMove) {
    e.order = attackMove ? O_ATTACK_MOVE : O_MOVE;
    e.dest = p;
    e.target = -1;
    e.repathCount = 0;
    e.repath = 1.f;
    setPath(e, p);
}

void Game::orderAttack(Entity &e, int target) {
    e.order = O_ATTACK;
    e.target = target;
    e.path.clear();
    e.pathIdx = 0;
    e.repath = 0;
}

void Game::orderGather(Entity &e, int mineral) {
    if (e.type != T_WORKER) return;
    if (mineral < 0) {
        int hq = nearestHQ(e.owner, e.pos);
        mineral = nearestMineral(hq >= 0 ? ents_[hq].pos : e.pos, 16 * TILE);
    }
    e.order = mineral >= 0 || e.cargo > 0 ? O_GATHER : O_IDLE;
    e.mineral = mineral;
    e.target = -1;
    e.work = 0;
    e.repath = 0;
    e.path.clear();
    e.pathIdx = 0;
}

void Game::orderBuild(Entity &e, EType t, int tx, int ty) {
    e.order = O_BUILD;
    e.buildType = t;
    e.bx = tx;
    e.by = ty;
    e.target = -1;
    e.repath = 0;
    e.path.clear();
    e.pathIdx = 0;
}

void Game::orderConstruct(Entity &e, int building) {
    e.order = O_CONSTRUCT;
    e.target = building;
    e.repath = 0;
    e.path.clear();
    e.pathIdx = 0;
    ents_[building].builder = e.id;
}

void Game::orderBoard(Entity &e, int vehicle) {
    if (!isInfantry(e.type) || inside(e)) return;
    e.order = O_BOARD;
    e.target = vehicle;
    e.repath = 0;
    e.path.clear();
    e.pathIdx = 0;
}

void Game::orderStop(Entity &e, bool hold) {
    e.order = hold ? O_HOLD : O_IDLE;
    e.target = -1;
    e.path.clear();
    e.pathIdx = 0;
    e.dest = e.pos;
}

// ---------------------------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------------------------

bool Game::hasComplete(int owner, EType t) const {
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (e.alive && e.owner == owner && e.type == t && e.complete) return true;
    }
    return false;
}

int Game::nearestHQ(int owner, Vec2 p) const {
    int best = -1;
    float bestD = 1e18f;
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (!e.alive || e.owner != owner || e.type != T_HQ || !e.complete) continue;
        float d = distSq(p, e.pos);
        if (d < bestD) {
            bestD = d;
            best = id;
        }
    }
    return best;
}

int Game::nearestMineral(Vec2 p, float maxDist) const {
    int best = -1;
    float bestD = maxDist * maxDist;
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (!e.alive || e.type != T_MINERAL) continue;
        float d = distSq(p, e.pos);
        if (d < bestD) {
            bestD = d;
            best = id;
        }
    }
    return best;
}

bool Game::canPlace(EType t, int tx, int ty, int owner) const {
    const TypeDef &td = kTypes[t];
    if (tx < 1 || ty < 1 || tx + td.tilesW > MAP_W - 1 || ty + td.tilesH > MAP_H - 1) return false;
    for (int y = ty; y < ty + td.tilesH; ++y) {
        for (int x = tx; x < tx + td.tilesW; ++x) {
            if (blocked(x, y)) return false;
            if (owner == PLAYER && vis_[idx(x, y)] == 0) return false;
        }
    }
    // Enemy units standing on the site block it; friendly ones get shoved aside.
    Vec2 c = footprintCenter(t, tx, ty);
    float hx = td.tilesW * TILE * 0.5f, hy = td.tilesH * TILE * 0.5f;
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (!e.alive || isBuilding(e) || e.owner == owner || inside(e)) continue;
        if (pointRectDist(e.pos, c, hx, hy) < unitRadius(e)) return false;
    }
    if (t == T_HQ) {
        // Keep command centers a few tiles away from mineral fields.
        for (int y = ty - 3; y < ty + td.tilesH + 3; ++y) {
            for (int x = tx - 3; x < tx + td.tilesW + 3; ++x) {
                if (!inMap(x, y)) continue;
                int o = occ_[idx(x, y)];
                if (o >= 0 && ents_[o].type == T_MINERAL) return false;
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// Pathfinding: A* over the tile grid with 8-way moves, then string-pulled.
// ---------------------------------------------------------------------------------------------

bool Game::nearestFree(int &x, int &y, int prefX, int prefY) const {
    if (inMap(x, y) && !blocked(x, y)) return true;
    for (int r = 1; r < 24; ++r) {
        int bx = -1, by = -1;
        float bd = 1e18f;
        for (int yy = y - r; yy <= y + r; ++yy) {
            for (int xx = x - r; xx <= x + r; ++xx) {
                if (std::abs(xx - x) != r && std::abs(yy - y) != r) continue;
                if (blocked(xx, yy)) continue;
                float d = (float) ((xx - prefX) * (xx - prefX) + (yy - prefY) * (yy - prefY));
                if (d < bd) {
                    bd = d;
                    bx = xx;
                    by = yy;
                }
            }
        }
        if (bx >= 0) {
            x = bx;
            y = by;
            return true;
        }
    }
    return false;
}

bool Game::lineClear(Vec2 a, Vec2 b, float r) const {
    Vec2 d = b - a;
    float L = length(d);
    if (L < 1.f) return !blockedAt(b);
    Vec2 n = d / L;
    Vec2 perp{-n.y * r, n.x * r};
    int steps = (int) (L / (TILE * 0.25f)) + 1;
    for (int i = 0; i <= steps; ++i) {
        Vec2 p = a + d * ((float) i / steps);
        if (blockedAt(p)) return false;
        if (r > 0 && (blockedAt(p + perp) || blockedAt(p - perp))) return false;
    }
    return true;
}

bool Game::findPath(Vec2 from, Vec2 to, std::vector<Vec2> &out) {
    out.clear();
    to.x = clampf(to.x, 1.f, MAP_W * TILE - 1.f);
    to.y = clampf(to.y, 1.f, MAP_H * TILE - 1.f);
    int sx = (int) clampf(from.x / TILE, 0.f, MAP_W - 1.f);
    int sy = (int) clampf(from.y / TILE, 0.f, MAP_H - 1.f);
    int gx = (int) (to.x / TILE), gy = (int) (to.y / TILE);
    if (blocked(sx, sy) && !nearestFree(sx, sy, sx, sy)) return false;
    bool exact = true;
    if (blocked(gx, gy)) {
        exact = false;
        if (!nearestFree(gx, gy, sx, sy)) return false;
    }
    Vec2 goalPos = exact ? to : tileCenter(gx, gy);
    if ((sx == gx && sy == gy) || lineClear(from, goalPos, 10.f)) {
        out.push_back(goalPos);
        return true;
    }

    if (++searchGen_ == 0) {
        std::fill(openGen_.begin(), openGen_.end(), 0);
        std::fill(closedGen_.begin(), closedGen_.end(), 0);
        searchGen_ = 1;
    }
    auto heur = [gx, gy](int x, int y) {
        float dx = (float) std::abs(x - gx), dy = (float) std::abs(y - gy);
        return dx + dy - 0.5858f * std::min(dx, dy);
    };
    using Node = std::pair<float, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    int s = idx(sx, sy), g = idx(gx, gy);
    gCost_[s] = 0;
    parent_[s] = -1;
    openGen_[s] = searchGen_;
    open.push({heur(sx, sy), s});
    int best = s;
    float bestH = heur(sx, sy);
    int expanded = 0;
    static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};

    while (!open.empty()) {
        int cur = open.top().second;
        open.pop();
        if (closedGen_[cur] == searchGen_) continue;
        closedGen_[cur] = searchGen_;
        if (cur == g) {
            best = cur;
            break;
        }
        int cx = cur % MAP_W, cy = cur / MAP_W;
        float h = heur(cx, cy);
        if (h < bestH) {
            bestH = h;
            best = cur;
        }
        if (++expanded > 9000) break;
        for (int k = 0; k < 8; ++k) {
            int nx = cx + dx[k], ny = cy + dy[k];
            if (blocked(nx, ny)) continue;
            if (k >= 4 && (blocked(cx + dx[k], cy) || blocked(cx, cy + dy[k]))) continue;
            int n = idx(nx, ny);
            if (closedGen_[n] == searchGen_) continue;
            float ng = gCost_[cur] + (k < 4 ? 1.f : 1.41421f);
            if (openGen_[n] != searchGen_ || ng < gCost_[n]) {
                openGen_[n] = searchGen_;
                gCost_[n] = ng;
                parent_[n] = cur;
                open.push({ng + heur(nx, ny), n});
            }
        }
    }

    std::vector<Vec2> raw;
    for (int c = best; c != -1 && c != s; c = parent_[c]) raw.push_back(tileCenter(c % MAP_W, c / MAP_W));
    if (raw.empty()) return false;
    std::reverse(raw.begin(), raw.end());
    if (best == g) raw.back() = goalPos;

    // String-pull: skip waypoints that have a clear straight line.
    Vec2 cur = from;
    size_t i = 0;
    while (i < raw.size()) {
        size_t j = std::min(raw.size() - 1, i + 14);
        while (j > i && !lineClear(cur, raw[j], 10.f)) --j;
        out.push_back(raw[j]);
        cur = raw[j];
        i = j + 1;
    }
    return true;
}
