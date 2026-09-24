#include "Game.h"

// A simple build-order AI: saturate minerals, keep supply ahead, tech up, then attack in
// growing waves. It defends its base when the player's units show up nearby.

void Game::aiThink() {
    const int me = ENEMY;
    int hq = nearestHQ(me, {MAP_W * TILE, MAP_H * TILE});
    Vec2 home{(MAP_W - 11) * TILE, (MAP_H - 11) * TILE};
    if (hq >= 0) {
        home = ents_[hq].pos;
    } else {
        for (int id: live_) {
            const Entity &e = ents_[id];
            if (e.alive && e.owner == me && isBuilding(e)) {
                home = e.pos;
                break;
            }
        }
    }

    std::vector<int> workers, army, incomplete, producers;
    int count[T_COUNT] = {}, pending[T_COUNT] = {};
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (!e.alive || e.owner != me) continue;
        if (e.type == T_WORKER) {
            workers.push_back(id);
            if (e.order == O_BUILD) pending[e.buildType]++;
        } else if (!isBuilding(e)) {
            count[e.type]++;
            if (!inside(e)) army.push_back(id);
        } else {
            count[e.type]++;
            for (EType q: e.queue) count[q]++;
            if (!e.complete) incomplete.push_back(id);
            else if (e.type == T_BARRACKS || e.type == T_FACTORY) producers.push_back(id);
        }
    }

    // Put idle workers back to work.
    for (int id: workers) {
        Entity &w = ents_[id];
        if (w.order == O_IDLE) orderGather(w, nearestMineral(home, 16 * TILE));
    }
    // Resume construction that lost its builder.
    for (int id: incomplete) {
        Entity &b = ents_[id];
        const Entity *bl = ent(b.builder);
        if (bl && bl->order == O_CONSTRUCT && bl->target == b.id) continue;
        int best = -1;
        float bestD = 1e18f;
        for (int w: workers) {
            const Entity &we = ents_[w];
            if (we.order == O_BUILD || we.order == O_CONSTRUCT) continue;
            float d = distSq(we.pos, b.pos);
            if (d < bestD) {
                bestD = d;
                best = w;
            }
        }
        if (best >= 0) orderConstruct(ents_[best], b.id);
    }

    // Economy
    int queuedWorkers = 0;
    if (hq >= 0) {
        for (EType q: ents_[hq].queue) queuedWorkers += q == T_WORKER;
    }
    int nWorkers = (int) workers.size() + queuedWorkers;
    if (hq >= 0 && ents_[hq].queue.empty() && nWorkers < 20) tryTrain(ents_[hq], T_WORKER);

    // Supply: stay ahead of production.
    int prodCount = count[T_BARRACKS] + count[T_FACTORY] + 1;
    bool depotBusy = pending[T_DEPOT] > 0;
    for (int id: incomplete) depotBusy |= ents_[id].type == T_DEPOT;
    bool wantBuilding = false;
    if (supplyCap_[me] < MAX_SUPPLY && supplyUsed_[me] + 2 + prodCount * 2 >= supplyCap_[me] &&
        !depotBusy) {
        wantBuilding = true;
        aiBuild(T_DEPOT, home);
    } else {
        int wantBarracks = nWorkers >= 15 ? 2 : (nWorkers >= 10 ? 1 : 0);
        if (difficulty_ == 2 && nWorkers >= 18) wantBarracks = 3;
        if (count[T_BARRACKS] + pending[T_BARRACKS] < wantBarracks && hasComplete(me, T_DEPOT)) {
            wantBuilding = true;
            aiBuild(T_BARRACKS, home);
        } else if (hasComplete(me, T_BARRACKS) && nWorkers >= 14 &&
                   count[T_FACTORY] + pending[T_FACTORY] < 1) {
            wantBuilding = true;
            aiBuild(T_FACTORY, home);
        } else if (hasComplete(me, T_FACTORY) && count[T_TURRET] + pending[T_TURRET] < 2) {
            wantBuilding = true;
            aiBuild(T_TURRET, home);
        }
    }

    // Army production (hold back some minerals when a building is wanted)
    int reserve = wantBuilding ? 150 : 0;
    for (int id: producers) {
        Entity &b = ents_[id];
        if (b.queue.size() >= 2) continue;
        EType u = T_TANK;
        if (b.type == T_BARRACKS) {
            // Mostly marines, some sharpshooters, and an officer or two once teched up.
            int wantOfficers = 1 + difficulty_ / 2;
            u = T_MARINE;
            if (hasComplete(me, T_FACTORY) && count[T_OFFICER] < wantOfficers) u = T_OFFICER;
            else if (rng_.f() < 0.3f) u = T_SNIPER;
        }
        if (minerals_[me] - reserve >= kTypes[u].cost) tryTrain(b, u);
    }

    // ----- Military
    // Defend: player units near home pull the whole army back.
    int threat = -1;
    float threatD = 22 * TILE;
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (!e.alive || e.owner != PLAYER || isBuilding(e)) continue;
        float d = dist(e.pos, home);
        if (d < threatD) {
            threatD = d;
            threat = id;
        }
    }
    Vec2 center{MAP_W * TILE * 0.5f, MAP_H * TILE * 0.5f};
    Vec2 rally = home + normalize(center - home) * (7 * TILE);

    auto nearestPlayerBuilding = [&](Vec2 from) {
        int best = -1;
        float bestD = 1e18f;
        for (int id: live_) {
            const Entity &e = ents_[id];
            if (!e.alive || e.owner != PLAYER || !isBuilding(e)) continue;
            float d = distSq(from, e.pos);
            if (d < bestD) {
                bestD = d;
                best = id;
            }
        }
        return best;
    };

    if (threat >= 0) {
        Vec2 p = ents_[threat].pos;
        for (int id: army) {
            Entity &u = ents_[id];
            bool busy = u.order == O_ATTACK_MOVE && u.target >= 0;
            if (!busy && dist(u.dest, p) > 4 * TILE) orderMove(u, p, true);
        }
        return;
    }

    if (!aiAttacking_) {
        if (gameTime_ >= aiFirstAttack_ && (int) army.size() >= aiWaveSize_) {
            int target = nearestPlayerBuilding(home);
            if (target >= 0) {
                aiAttacking_ = true;
                aiWaveStart_ = gameTime_;
                aiWave_++;
                aiWaveSize_ = std::min(36, aiWaveSize_ + 3 + difficulty_);
                for (int id: army) orderMove(ents_[id], ents_[target].pos, true);
                return;
            }
        }
        for (int id: army) {
            Entity &u = ents_[id];
            if (u.order == O_IDLE && dist(u.pos, rally) > 4 * TILE) orderMove(u, rally, true);
        }
        return;
    }

    // Attacking: keep pushing to the next structure; end the wave when the army is spent.
    int awayCount = 0;
    for (int id: army) {
        Entity &u = ents_[id];
        bool away = dist(u.pos, home) > 25 * TILE;
        awayCount += away;
        if (u.order != O_IDLE) continue;
        if (away) {
            int target = nearestPlayerBuilding(u.pos);
            if (target >= 0) orderMove(u, ents_[target].pos, true);
        } else if (dist(u.pos, rally) > 4 * TILE) {
            orderMove(u, rally, true);
        }
    }
    if (awayCount == 0 && gameTime_ - aiWaveStart_ > 25.f) aiAttacking_ = false;
}

bool Game::aiBuild(EType t, Vec2 home) {
    const TypeDef &td = kTypes[t];
    if (minerals_[ENEMY] < td.cost) return false;
    int tx, ty;
    if (!aiFindSite(t, home, tx, ty)) return false;
    Vec2 site = footprintCenter(t, tx, ty);
    int best = -1;
    float bestD = 1e18f;
    for (int id: live_) {
        const Entity &w = ents_[id];
        if (!w.alive || w.owner != ENEMY || w.type != T_WORKER) continue;
        if (w.order == O_BUILD || w.order == O_CONSTRUCT) continue;
        float d = distSq(w.pos, site) + (w.cargo > 0 ? 1e5f : 0.f);
        if (d < bestD) {
            bestD = d;
            best = id;
        }
    }
    if (best < 0) return false;
    orderBuild(ents_[best], t, tx, ty);
    return true;
}

bool Game::aiFindSite(EType t, Vec2 home, int &outX, int &outY) {
    const TypeDef &td = kTypes[t];
    int hx = (int) (home.x / TILE), hy = (int) (home.y / TILE);

    // Average mineral position near home, so we keep the mining line clear.
    Vec2 mineralDir{0, 0};
    for (int id: live_) {
        const Entity &m = ents_[id];
        if (m.alive && m.type == T_MINERAL && dist(m.pos, home) < 10 * TILE) mineralDir += m.pos - home;
    }
    mineralDir = normalize(mineralDir);
    Vec2 center{MAP_W * TILE * 0.5f, MAP_H * TILE * 0.5f};
    Vec2 prefer = t == T_TURRET ? home + normalize(center - home) * (6 * TILE) : home;

    float bestScore = 1e18f;
    bool found = false;
    for (int y = hy - 14; y <= hy + 14; ++y) {
        for (int x = hx - 14; x <= hx + 14; ++x) {
            if (!canPlace(t, x, y, ENEMY)) continue;
            // Leave a one-tile corridor around every structure.
            bool crowded = false;
            for (int yy = y - 1; yy <= y + td.tilesH && !crowded; ++yy) {
                for (int xx = x - 1; xx <= x + td.tilesW; ++xx) {
                    if (inMap(xx, yy) && occ_[idx(xx, yy)] >= 0) {
                        crowded = true;
                        break;
                    }
                }
            }
            if (crowded) continue;
            Vec2 c = footprintCenter(t, x, y);
            Vec2 off = c - home;
            float d = length(off);
            if (d < 4 * TILE) continue;
            if (length(mineralDir) > 0 && dot(normalize(off), mineralDir) > 0.2f) continue;
            float score = dist(c, prefer) + rng_.range(0, 2 * TILE);
            if (score < bestScore) {
                bestScore = score;
                outX = x;
                outY = y;
                found = true;
            }
        }
    }
    return found;
}
