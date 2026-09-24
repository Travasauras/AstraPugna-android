#include "Game.h"

// Touch controls:
//   tap own unit/building ........ select (double-tap: select all of that type on screen)
//   tap ground/enemy/minerals .... move / attack / gather with the current selection
//   drag one finger .............. scroll the map
//   hold, then drag .............. box-select units
//   pinch ........................ zoom
//   minimap ...................... tap or drag to jump the camera

// ---------------------------------------------------------------------------------------------
// Layout & camera
// ---------------------------------------------------------------------------------------------

void Game::layout() {
    ui_ = clampf(std::min(screenH_ / 720.f, screenW_ / 1280.f), 0.5f, 4.f);
    topH_ = 46 * ui_;
    panelH_ = 184 * ui_;
    panelY_ = screenH_ - panelH_;
    float pad = 8 * ui_;
    float mmH = panelH_ - 2 * pad;
    minimap_ = {pad, panelY_ + pad, mmH * MAP_W / MAP_H, mmH};
    float bw = 112 * ui_;
    float cardX = screenW_ - pad - 3 * bw - 2 * pad;
    info_ = {minimap_.x + minimap_.w + pad * 2, panelY_ + pad,
             cardX - (minimap_.x + minimap_.w + pad * 4), panelH_ - 2 * pad};
    if (!scaleInit_ && screenH_ > 0) {
        scale_ = 1.35f * ui_;
        scaleInit_ = true;
    }
}

bool Game::inHud(Vec2 s) const {
    return s.y < topH_ || s.y >= panelY_;
}

bool Game::onScreen(Vec2 w) const {
    Vec2 s = toScreen(w);
    return s.x >= 0 && s.x < screenW_ && s.y >= topH_ && s.y < panelY_;
}

void Game::clampCamera() {
    if (screenW_ <= 0) return;
    float minX = -2 * TILE, maxX = MAP_W * TILE - screenW_ / scale_ + 2 * TILE;
    float minY = -topH_ / scale_ - TILE, maxY = MAP_H * TILE - panelY_ / scale_ + TILE;
    cam_.x = maxX < minX ? (minX + maxX) * 0.5f : clampf(cam_.x, minX, maxX);
    cam_.y = maxY < minY ? (minY + maxY) * 0.5f : clampf(cam_.y, minY, maxY);
}

void Game::centerCamera(Vec2 p) {
    cam_ = p - Vec2(screenW_ * 0.5f, (topH_ + panelY_) * 0.5f) / scale_;
    clampCamera();
}

void Game::minimapJump(Vec2 s) {
    Vec2 w{(s.x - minimap_.x) / minimap_.w * MAP_W * TILE, (s.y - minimap_.y) / minimap_.h * MAP_H * TILE};
    centerCamera(w);
}

void Game::titleButtons(Rectf out[3]) const {
    float bw = 220 * ui_, bh = 70 * ui_, gap = 24 * ui_;
    float x0 = screenW_ * 0.5f - (3 * bw + 2 * gap) * 0.5f;
    float y = screenH_ * 0.52f;
    for (int i = 0; i < 3; ++i) out[i] = {x0 + i * (bw + gap), y, bw, bh};
}

// ---------------------------------------------------------------------------------------------
// Command card
// ---------------------------------------------------------------------------------------------

void Game::buildButtons() {
    buttons_.clear();
    if (screen_ != SCR_PLAY) return;
    layout();
    cleanSelection();

    // Top bar shortcuts
    int idle = 0, armyCount = 0;
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (!e.alive || e.owner != PLAYER || isBuilding(e)) continue;
        if (e.type == T_WORKER) idle += e.order == O_IDLE;
        else armyCount++;
    }
    float tb = 150 * ui_, tpad = 6 * ui_;
    buttons_.push_back({{screenW_ - 2 * (tb + tpad), tpad, tb, topH_ - 2 * tpad}, A_IDLE, T_WORKER,
                        "IDLE: " + std::to_string(idle), "", idle > 0, false});
    buttons_.push_back({{screenW_ - (tb + tpad), tpad, tb, topH_ - 2 * tpad}, A_ARMY, T_MARINE,
                        "ARMY: " + std::to_string(armyCount), "", armyCount > 0, false});

    float pad = 8 * ui_, bw = 112 * ui_, bh = (panelH_ - 3 * pad) / 2;
    float cardX = screenW_ - pad - 3 * bw - 2 * pad;
    auto slot = [&](int i) {
        return Rectf{cardX + (i % 3) * (bw + pad), panelY_ + pad + (i / 3) * (bh + pad), bw, bh};
    };
    auto costButton = [&](int i, Action a, EType t) {
        const TypeDef &td = kTypes[t];
        bool reqOk = td.requires == T_COUNT || hasComplete(PLAYER, td.requires);
        std::string sub = reqOk ? std::to_string(td.cost) : std::string("NEEDS ") + kTypes[td.requires].name;
        buttons_.push_back({slot(i), a, t, td.name, sub, reqOk, minerals_[PLAYER] < td.cost});
    };

    if (mode_ != MODE_NORMAL) {
        buttons_.push_back({slot(5), A_CANCEL, T_COUNT, "CANCEL", "", true, false});
        return;
    }
    if (sel_.empty()) return;
    const Entity &f = ents_[sel_[0]];
    if (f.owner != PLAYER) return;

    if (isBuilding(f)) {
        if (!f.complete) return;
        if (f.type == T_HQ) costButton(0, A_TRAIN, T_WORKER);
        if (f.type == T_BARRACKS) {
            costButton(0, A_TRAIN, T_MARINE);
            costButton(1, A_TRAIN, T_SNIPER);
            costButton(2, A_TRAIN, T_OFFICER);
        }
        if (f.type == T_FACTORY) {
            costButton(0, A_TRAIN, T_TANK);
            costButton(1, A_TRAIN, T_ROVER);
        }
        if (!f.queue.empty()) buttons_.push_back({slot(5), A_DEQUEUE, T_COUNT, "CANCEL", "LAST", true, false});
        return;
    }

    bool anyArmy = false;
    for (int id: sel_) anyArmy |= ents_[id].type != T_WORKER;
    if (!anyArmy) {
        costButton(0, A_BUILD, T_DEPOT);
        costButton(1, A_BUILD, T_BARRACKS);
        costButton(2, A_BUILD, T_FACTORY);
        costButton(3, A_BUILD, T_TURRET);
        costButton(4, A_BUILD, T_HQ);
        buttons_.push_back({slot(5), A_STOP, T_COUNT, "STOP", "", true, false});
    } else {
        buttons_.push_back({slot(0), A_ATTACK, T_COUNT, "ATTACK", "MOVE", true, false});
        buttons_.push_back({slot(1), A_STOP, T_COUNT, "STOP", "", true, false});
        buttons_.push_back({slot(2), A_HOLD, T_COUNT, "HOLD", "POSITION", true, false});
        bool anyInfantry = false;
        int riders = 0;
        for (int id: sel_) {
            anyInfantry |= isInfantry(ents_[id].type);
            riders += (int) ents_[id].passengers.size();
        }
        if (anyInfantry) buttons_.push_back({slot(3), A_BOARD, T_COUNT, "BOARD", "VEHICLE", true, false});
        if (riders > 0) {
            buttons_.push_back({slot(4), A_UNLOAD, T_COUNT, "UNLOAD", std::to_string(riders) + " ABOARD", true,
                                false});
        }
    }
}

int Game::buttonAt(Vec2 s) const {
    for (size_t i = 0; i < buttons_.size(); ++i) {
        if (buttons_[i].r.contains(s)) return (int) i;
    }
    return -1;
}

void Game::doButton(const Button &b) {
    switch (b.action) {
        case A_TRAIN: {
            // Queue on whichever selected producer has the shortest queue.
            Entity *best = nullptr;
            for (int id: sel_) {
                Entity &e = ents_[id];
                if (e.owner != PLAYER || !e.complete || kTypes[b.param].producer != e.type) continue;
                if (!best || e.queue.size() < best->queue.size()) best = &e;
            }
            if (best) tryTrain(*best, b.param);
            break;
        }
        case A_BUILD: {
            const TypeDef &td = kTypes[b.param];
            if (td.requires != T_COUNT && !hasComplete(PLAYER, td.requires)) {
                message(std::string("REQUIRES ") + kTypes[td.requires].name);
            } else if (minerals_[PLAYER] < td.cost) {
                message("NOT ENOUGH MINERALS");
            } else {
                mode_ = MODE_PLACE;
                placeType_ = b.param;
                placeVisible_ = false;
                message(std::string("TAP TO PLACE ") + td.name);
            }
            break;
        }
        case A_STOP:
        case A_HOLD:
            for (int id: selectedUnits()) orderStop(ents_[id], b.action == A_HOLD);
            break;
        case A_ATTACK:
            mode_ = MODE_ATTACK;
            message("TAP A TARGET OR LOCATION");
            break;
        case A_BOARD:
            mode_ = MODE_BOARD;
            message("TAP A TANK OR ROVER");
            break;
        case A_UNLOAD:
            for (int id: sel_) {
                Entity *e = ent(id);
                if (e && e->owner == PLAYER) unload(*e);
            }
            break;
        case A_CANCEL:
            mode_ = MODE_NORMAL;
            placeVisible_ = false;
            break;
        case A_DEQUEUE: {
            if (sel_.empty()) break;
            Entity &e = ents_[sel_[0]];
            if (e.queue.empty()) break;
            minerals_[PLAYER] += kTypes[e.queue.back()].cost;
            e.queue.pop_back();
            if (e.queue.empty()) e.trainT = 0;
            updateSupply();
            break;
        }
        case A_IDLE: {
            std::vector<int> idle;
            for (int id: live_) {
                const Entity &e = ents_[id];
                if (e.alive && e.owner == PLAYER && e.type == T_WORKER && e.order == O_IDLE) idle.push_back(id);
            }
            if (idle.empty()) break;
            int id = idle[idleCycle_++ % idle.size()];
            sel_ = {id};
            centerCamera(ents_[id].pos);
            break;
        }
        case A_ARMY:
            sel_.clear();
            for (int id: live_) {
                const Entity &e = ents_[id];
                if (e.alive && e.owner == PLAYER && !isBuilding(e) && e.type != T_WORKER && !inside(e)) {
                    sel_.push_back(id);
                }
            }
            break;
    }
}

// ---------------------------------------------------------------------------------------------
// Selection & commands
// ---------------------------------------------------------------------------------------------

void Game::cleanSelection() {
    sel_.erase(std::remove_if(sel_.begin(), sel_.end(), [this](int id) {
        const Entity *e = ent(id);
        return !e || inside(*e) || (e->owner != PLAYER && !visibleToPlayer(*e) && !(isBuilding(*e) && e->seen));
    }), sel_.end());
}

std::vector<int> Game::selectedUnits() const {
    std::vector<int> out;
    for (int id: sel_) {
        const Entity *e = ent(id);
        if (e && e->owner == PLAYER && !isBuilding(*e)) out.push_back(id);
    }
    return out;
}

bool Game::selectionHasWorkers() const {
    for (int id: sel_) {
        const Entity *e = ent(id);
        if (e && e->owner == PLAYER && e->type == T_WORKER) return true;
    }
    return false;
}

int Game::pickEntity(Vec2 w, float tol) const {
    int best = -1;
    float bestScore = 1e18f;
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (!e.alive || inside(e)) continue;
        bool buildingLike = isBuilding(e);
        if (e.owner == ENEMY && !visibleToPlayer(e) && !(buildingLike && e.seen)) continue;
        if (e.type == T_MINERAL && vis_[idx(e.tx, e.ty)] == 0) continue;
        float d, score;
        if (buildingLike) {
            float hx, hy;
            halfExtents(e, hx, hy);
            d = pointRectDist(w, e.pos, hx, hy);
            score = d + 1000.f;
        } else {
            d = std::max(0.f, dist(w, e.pos) - unitRadius(e));
            score = d;
        }
        if (d <= tol && score < bestScore) {
            bestScore = score;
            best = id;
        }
    }
    return best;
}

void Game::onTap(Vec2 s) {
    Vec2 w = toWorld(s);
    int hit = pickEntity(w, 14 * ui_ / scale_);

    if (mode_ == MODE_ATTACK) {
        commandAttack(w, hit);
        mode_ = MODE_NORMAL;
        return;
    }
    if (mode_ == MODE_BOARD) {
        commandBoard(hit);
        mode_ = MODE_NORMAL;
        return;
    }

    bool haveUnits = !selectedUnits().empty();
    bool haveProducer = false;
    for (int id: sel_) {
        const Entity *e = ent(id);
        haveProducer |= e && e->owner == PLAYER && e->complete &&
                        (e->type == T_HQ || e->type == T_BARRACKS || e->type == T_FACTORY);
    }

    if (hit >= 0) {
        const Entity &h = ents_[hit];
        if (h.owner == PLAYER) {
            if (isBuilding(h) && !h.complete && selectionHasWorkers()) {
                commandSmart(w, hit);
                return;
            }
            bool doubleTap = realTime_ - lastTapTime_ < 0.4f && lastTapEnt_ >= 0 &&
                             ent(lastTapEnt_) && ents_[lastTapEnt_].type == h.type;
            if (doubleTap) {
                sel_.clear();
                for (int id: live_) {
                    const Entity &e = ents_[id];
                    if (e.alive && e.owner == PLAYER && e.type == h.type && !inside(e) && onScreen(e.pos)) {
                        sel_.push_back(id);
                    }
                }
                lastTapTime_ = -10;
            } else {
                sel_ = {hit};
                lastTapTime_ = realTime_;
                lastTapEnt_ = hit;
            }
            return;
        }
        if (haveUnits || haveProducer) {
            commandSmart(w, hit);
        } else {
            sel_ = {hit};   // inspect enemy / mineral
        }
        return;
    }

    if (haveUnits || haveProducer) commandSmart(w, -1);
    else sel_.clear();
}

void Game::onBoxSelect(Vec2 a, Vec2 b) {
    if (dist(a, b) < 12 * ui_) {
        onTap(b);
        return;
    }
    Vec2 wa = toWorld(a), wb = toWorld(b);
    float x0 = std::min(wa.x, wb.x), x1 = std::max(wa.x, wb.x);
    float y0 = std::min(wa.y, wb.y), y1 = std::max(wa.y, wb.y);
    std::vector<int> picked;
    for (int id: live_) {
        const Entity &e = ents_[id];
        if (!e.alive || e.owner != PLAYER || isBuilding(e) || inside(e)) continue;
        float r = unitRadius(e);
        if (e.pos.x + r >= x0 && e.pos.x - r <= x1 && e.pos.y + r >= y0 && e.pos.y - r <= y1) picked.push_back(id);
    }
    // Prefer soldiers if the box caught both soldiers and workers.
    bool anyArmy = false;
    for (int id: picked) anyArmy |= ents_[id].type != T_WORKER;
    if (anyArmy) {
        picked.erase(std::remove_if(picked.begin(), picked.end(),
                                    [this](int id) { return ents_[id].type == T_WORKER; }), picked.end());
    }
    if (!picked.empty()) sel_ = picked;
}

void Game::commandSmart(Vec2 w, int hit) {
    std::vector<int> units = selectedUnits();
    if (units.empty()) {
        // Production buildings: set rally point.
        for (int id: sel_) {
            Entity *e = ent(id);
            if (!e || e->owner != PLAYER || !isBuilding(*e) || e->type == T_DEPOT || e->type == T_TURRET) continue;
            e->hasRally = true;
            e->rally = hit >= 0 ? ents_[hit].pos : w;
            e->rallyTarget = hit >= 0 && ents_[hit].type == T_MINERAL ? hit : -1;
        }
        addFx(FX_MARK, w, w, 0.5f, 14, rgba(.3f, 1, .4f));
        return;
    }
    if (hit >= 0) {
        const Entity &h = ents_[hit];
        if (h.owner == ENEMY) {
            for (int id: units) orderAttack(ents_[id], hit);
            addFx(FX_MARK, h.pos, h.pos, 0.5f, 18, rgba(1, .3f, .25f));
            return;
        }
        if (h.type == T_MINERAL) {
            std::vector<int> others;
            for (int id: units) {
                if (ents_[id].type == T_WORKER) orderGather(ents_[id], hit);
                else others.push_back(id);
            }
            if (!others.empty()) formationMove(others, h.pos, false);
            addFx(FX_MARK, h.pos, h.pos, 0.5f, 16, rgba(.4f, .9f, 1));
            return;
        }
        if (h.owner == PLAYER && isBuilding(h) && !h.complete) {
            std::vector<int> others;
            bool assigned = false;
            for (int id: units) {
                if (!assigned && ents_[id].type == T_WORKER) {
                    orderConstruct(ents_[id], hit);
                    assigned = true;
                } else {
                    others.push_back(id);
                }
            }
            if (!others.empty()) formationMove(others, w, false);
            addFx(FX_MARK, h.pos, h.pos, 0.5f, 18, rgba(1, .85f, .3f));
            return;
        }
    }
    formationMove(units, w, false);
    addFx(FX_MARK, w, w, 0.5f, 14, rgba(.3f, 1, .4f));
}

void Game::commandAttack(Vec2 w, int hit) {
    std::vector<int> units = selectedUnits();
    if (units.empty()) return;
    if (hit >= 0 && ents_[hit].owner == ENEMY) {
        for (int id: units) orderAttack(ents_[id], hit);
        addFx(FX_MARK, ents_[hit].pos, ents_[hit].pos, 0.5f, 18, rgba(1, .3f, .25f));
        return;
    }
    formationMove(units, w, true);
    addFx(FX_MARK, w, w, 0.5f, 14, rgba(1, .3f, .25f));
}

void Game::commandBoard(int hit) {
    Entity *v = ent(hit);
    if (!v || v->owner != PLAYER || !isTransport(v->type)) {
        message("TAP A TANK OR ROVER");
        return;
    }
    int free = kTransportSlots - (int) v->passengers.size();
    if (free <= 0) {
        message(std::string(kTypes[v->type].name) + " IS FULL");
        return;
    }
    // The closest soldiers claim the open seats.
    std::vector<int> riders;
    for (int id: selectedUnits()) {
        if (isInfantry(ents_[id].type)) riders.push_back(id);
    }
    std::sort(riders.begin(), riders.end(),
              [&](int a, int b) { return distSq(ents_[a].pos, v->pos) < distSq(ents_[b].pos, v->pos); });
    if ((int) riders.size() > free) {
        riders.resize(free);
        message("NOT ENOUGH ROOM - " + std::to_string(free) + " BOARDING");
    }
    for (int id: riders) orderBoard(ents_[id], hit);
    addFx(FX_MARK, v->pos, v->pos, 0.5f, 22, rgba(.3f, 1, .4f));
}

void Game::formationMove(const std::vector<int> &units, Vec2 p, bool attack) {
    if (units.empty()) return;
    if (units.size() == 1) {
        orderMove(ents_[units[0]], p, attack);
        return;
    }
    float maxR = 0;
    Vec2 c{0, 0};
    for (int id: units) {
        maxR = std::max(maxR, unitRadius(ents_[id]));
        c += ents_[id].pos;
    }
    c = c / (float) units.size();
    int n = (int) units.size();
    int cols = (int) std::ceil(std::sqrt((float) n));
    int rows = (n + cols - 1) / cols;
    float sp = maxR * 2 + 6;
    std::vector<Vec2> slots;
    for (int i = 0; i < n; ++i) {
        int col = i % cols, row = i / cols;
        slots.push_back(p + Vec2((col - (cols - 1) * 0.5f) * sp, (row - (rows - 1) * 0.5f) * sp));
    }
    // Closest units claim slots first.
    std::vector<int> order = units;
    std::sort(order.begin(), order.end(),
              [&](int a, int b) { return distSq(ents_[a].pos, p) < distSq(ents_[b].pos, p); });
    std::vector<bool> used(slots.size(), false);
    for (int id: order) {
        int best = 0;
        float bestD = 1e18f;
        for (size_t s = 0; s < slots.size(); ++s) {
            if (used[s]) continue;
            float d = distSq(slots[s], ents_[id].pos - c + p);
            if (d < bestD) {
                bestD = d;
                best = (int) s;
            }
        }
        used[best] = true;
        Vec2 target = blockedAt(slots[best]) ? p : slots[best];
        orderMove(ents_[id], target, attack);
    }
}

void Game::placementTile(Vec2 w, int &tx, int &ty) const {
    const TypeDef &td = kTypes[placeType_];
    tx = (int) std::floor(w.x / TILE - td.tilesW * 0.5f + 0.5f);
    ty = (int) std::floor(w.y / TILE - td.tilesH * 0.5f + 0.5f);
}

void Game::tryPlaceBuilding(Vec2 w) {
    int tx, ty;
    placementTile(w, tx, ty);
    if (!canPlace(placeType_, tx, ty, PLAYER)) {
        message("CAN'T BUILD THERE");
        return;
    }
    Vec2 site = footprintCenter(placeType_, tx, ty);
    int best = -1;
    float bestD = 1e18f;
    for (int id: sel_) {
        const Entity *e = ent(id);
        if (!e || e->owner != PLAYER || e->type != T_WORKER) continue;
        float d = distSq(e->pos, site);
        if (d < bestD) {
            bestD = d;
            best = id;
        }
    }
    if (best < 0) {
        mode_ = MODE_NORMAL;
        return;
    }
    orderBuild(ents_[best], placeType_, tx, ty);
    addFx(FX_MARK, site, site, 0.5f, 20, rgba(1, .85f, .3f));
    mode_ = MODE_NORMAL;
    placeVisible_ = false;
}

// ---------------------------------------------------------------------------------------------
// Raw touch events
// ---------------------------------------------------------------------------------------------

void Game::touchDown(int id, float x, float y) {
    Vec2 p{x, y};
    if (numTouches_ < 5) {
        touches_[numTouches_++] = {id, p, p, p, realTime_};
    }
    if (screen_ != SCR_PLAY) return;
    buildButtons();

    if (numTouches_ == 1) {
        pressedBtn_ = buttonAt(p);
        if (pressedBtn_ >= 0) {
            gesture_ = G_HUD;
        } else if (minimap_.contains(p)) {
            gesture_ = G_MINIMAP;
            minimapJump(p);
        } else if (inHud(p)) {
            gesture_ = G_HUD;
        } else if (mode_ == MODE_PLACE) {
            gesture_ = G_PLACE;
            placePos_ = toWorld(p);
            placeVisible_ = true;
        } else {
            gesture_ = G_TAP;
        }
    } else if (numTouches_ == 2 && gesture_ != G_HUD && gesture_ != G_MINIMAP) {
        gesture_ = G_PINCH;
        pinchDist_ = std::max(1.f, dist(touches_[0].cur, touches_[1].cur));
        pinchMid_ = (touches_[0].cur + touches_[1].cur) * 0.5f;
    }
}

void Game::touchMove(int id, float x, float y) {
    Touch *t = nullptr;
    for (int i = 0; i < numTouches_; ++i) {
        if (touches_[i].id == id) t = &touches_[i];
    }
    if (!t) return;
    t->prev = t->cur;
    t->cur = {x, y};
    if (screen_ != SCR_PLAY) return;

    switch (gesture_) {
        case G_TAP:
            if (dist(t->cur, t->start) > 14 * ui_) {
                gesture_ = G_PAN;
                cam_ -= (t->cur - t->start) / scale_;
                clampCamera();
            }
            break;
        case G_PAN:
            cam_ -= (t->cur - t->prev) / scale_;
            clampCamera();
            break;
        case G_PINCH: {
            if (numTouches_ < 2) break;
            float nd = std::max(1.f, dist(touches_[0].cur, touches_[1].cur));
            Vec2 mid = (touches_[0].cur + touches_[1].cur) * 0.5f;
            Vec2 anchor = toWorld(pinchMid_);
            scale_ = clampf(scale_ * nd / pinchDist_, 0.55f * ui_, 2.8f * ui_);
            cam_ = anchor - mid / scale_;
            clampCamera();
            pinchDist_ = nd;
            pinchMid_ = mid;
            break;
        }
        case G_MINIMAP:
            minimapJump(t->cur);
            break;
        case G_PLACE:
            placePos_ = toWorld(t->cur);
            break;
        default:
            break;
    }
}

void Game::touchUp(int id, float x, float y) {
    int k = -1;
    for (int i = 0; i < numTouches_; ++i) {
        if (touches_[i].id == id) k = i;
    }
    if (k < 0) return;
    Touch t = touches_[k];
    t.cur = {x, y};
    touches_[k] = touches_[numTouches_ - 1];
    numTouches_--;

    if (screen_ == SCR_TITLE) {
        Rectf r[3];
        titleButtons(r);
        for (int i = 0; i < 3; ++i) {
            if (r[i].contains(t.cur) && r[i].contains(t.start)) newGame(i);
        }
        return;
    }
    if (screen_ == SCR_END) {
        if (endTimer_ > 1.5f) screen_ = SCR_TITLE;
        return;
    }

    buildButtons();
    switch (gesture_) {
        case G_TAP:
            onTap(t.cur);
            break;
        case G_BOX:
            onBoxSelect(t.start, t.cur);
            break;
        case G_HUD:
            if (pressedBtn_ >= 0 && pressedBtn_ < (int) buttons_.size() &&
                buttons_[pressedBtn_].r.contains(t.cur)) {
                Button b = buttons_[pressedBtn_];
                if (b.enabled) doButton(b);
                else if (b.action == A_BUILD || b.action == A_TRAIN) message(b.sub);
            }
            break;
        case G_PLACE:
            if (!inHud(t.cur)) tryPlaceBuilding(toWorld(t.cur));
            break;
        default:
            break;
    }
    pressedBtn_ = -1;
    if (numTouches_ == 0) {
        gesture_ = G_NONE;
    } else if (gesture_ == G_PINCH) {
        gesture_ = G_PAN;   // keep scrolling with the remaining finger
    }
}

void Game::touchCancel() {
    numTouches_ = 0;
    gesture_ = G_NONE;
    pressedBtn_ = -1;
}

void Game::scrollZoom(float x, float y, float amount) {
    if (screen_ != SCR_PLAY) return;
    Vec2 s{x, y};
    Vec2 anchor = toWorld(s);
    scale_ = clampf(scale_ * (amount > 0 ? 1.12f : 1 / 1.12f), 0.55f * ui_, 2.8f * ui_);
    cam_ = anchor - s / scale_;
    clampCamera();
}
