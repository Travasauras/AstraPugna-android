#ifndef ASTRAPUGNA_GAME_H
#define ASTRAPUGNA_GAME_H

#include <deque>
#include <string>
#include <vector>

#include "Draw2D.h"
#include "GameMath.h"

// ---------------------------------------------------------------------------------------------
// Static game data
// ---------------------------------------------------------------------------------------------

enum EType : uint8_t {
    T_WORKER, T_MARINE, T_SNIPER, T_OFFICER, T_TANK, T_ROVER,
    T_HQ, T_DEPOT, T_BARRACKS, T_FACTORY, T_TURRET,
    T_MINERAL,
    T_COUNT
};

struct TypeDef {
    const char *name;
    bool building;
    float radius;          // collision radius for units
    int tilesW, tilesH;    // footprint for buildings
    float hp, armor, speed;
    float range, damage, cooldown, splash;
    float sight;
    int cost, supply, supplyProvided;
    float buildTime;
    EType requires;        // T_COUNT = no requirement
    EType producer;        // building that trains this unit, T_COUNT = none
};

constexpr float TILE = 32.f;
constexpr int MAP_W = 96, MAP_H = 72;
constexpr int PLAYER = 0, ENEMY = 1, NEUTRAL = -1;
constexpr int MAX_SUPPLY = 200;

extern const TypeDef kTypes[T_COUNT];

// Tanks and rovers carry soldiers; a rover's passengers fire from inside it.
constexpr int kTransportSlots = 4;
// Officers boost allies within this radius: more damage, faster fire, +1 armor.
constexpr float kAuraRadius = 5 * TILE;
constexpr float kAuraDamage = 1.25f, kAuraCooldown = 0.75f, kAuraArmor = 1.f;

inline bool isInfantry(EType t) { return t == T_MARINE || t == T_SNIPER || t == T_OFFICER; }
inline bool isTransport(EType t) { return t == T_TANK || t == T_ROVER; }

enum Order : uint8_t {
    O_IDLE, O_MOVE, O_ATTACK_MOVE, O_ATTACK, O_HOLD,
    O_GATHER, O_RETURN, O_BUILD, O_CONSTRUCT, O_BOARD
};

struct Entity {
    int id = -1;
    EType type = T_WORKER;
    int owner = NEUTRAL;
    bool alive = true;
    Vec2 pos;
    float hp = 1, facing = 0, flash = 0, underAttack = 0;

    // Unit orders
    Order order = O_IDLE;
    Vec2 dest;
    int target = -1;
    int mineral = -1;
    int cargo = 0;
    float work = 0, cd = 0, repath = 0, scan = 0;
    std::vector<Vec2> path;
    size_t pathIdx = 0;
    bool moving = false, firing = false;
    float stuckT = 0;
    Vec2 stuckRef;
    int stuckCount = 0, repathCount = 0;
    EType buildType = T_COUNT;
    int bx = 0, by = 0;

    // Transports
    int transport = -1;            // vehicle this unit is riding in, -1 = on foot
    std::vector<int> passengers;   // units riding in this vehicle

    // Buildings
    int tx = 0, ty = 0;
    bool complete = true;
    float progress = 1;
    int builder = -1;
    std::vector<EType> queue;
    float trainT = 0;
    bool hasRally = false;
    Vec2 rally;
    int rallyTarget = -1;
    bool seen = false;       // enemy building has been spotted by the player

    // Minerals
    int amount = 0;
    int miner = -1;
};

inline bool isBuilding(const Entity &e) { return kTypes[e.type].building; }
inline bool inside(const Entity &e) { return e.transport >= 0; }
inline float unitRadius(const Entity &e) { return kTypes[e.type].radius; }

inline void halfExtents(const Entity &e, float &hx, float &hy) {
    const TypeDef &t = kTypes[e.type];
    if (t.building) {
        hx = t.tilesW * TILE * 0.5f;
        hy = t.tilesH * TILE * 0.5f;
    } else {
        hx = hy = t.radius;
    }
}

inline float pointRectDist(Vec2 p, Vec2 c, float hx, float hy) {
    float dx = std::max(std::fabs(p.x - c.x) - hx, 0.f);
    float dy = std::max(std::fabs(p.y - c.y) - hy, 0.f);
    return std::sqrt(dx * dx + dy * dy);
}

// Nearest point on an entity's body to `from`.
inline Vec2 closestPoint(const Entity &t, Vec2 from) {
    if (!isBuilding(t)) return t.pos;
    float hx, hy;
    halfExtents(t, hx, hy);
    return {clampf(from.x, t.pos.x - hx, t.pos.x + hx), clampf(from.y, t.pos.y - hy, t.pos.y + hy)};
}

enum FxKind : uint8_t { FX_TRACER, FX_EXPLODE, FX_SPARK, FX_MARK, FX_FLASH, FX_BOLT };

struct Fx {
    FxKind kind;
    Vec2 a, b;
    float t, dur, size;
    Color col;
};

struct Shot {
    Vec2 from, to;
    float t, dur;
    int target, owner, attacker;
    float damage, splash;
};

// ---------------------------------------------------------------------------------------------
// Game
// ---------------------------------------------------------------------------------------------

class Game {
public:
    Game();

    void update(float dt);
    void render(Draw2D &d, int width, int height);

    void touchDown(int id, float x, float y);
    void touchMove(int id, float x, float y);
    void touchUp(int id, float x, float y);
    void touchCancel();
    void scrollZoom(float x, float y, float amount);

private:
    // ----- Simulation (Game.cpp)
    void newGame(int difficulty);
    void generateMap(uint32_t seed);
    void setupBase(int owner, int tx, int ty, float mineralAngle);
    void placeMinerals(float cx, float cy, float angle, int count, float radius, int amount);
    int spawn(EType t, int owner, Vec2 pos);
    int spawnBuilding(EType t, int owner, int tx, int ty, bool complete);
    void kill(Entity &e);
    void step(float dt);
    void rebuildLive();
    void updateUnit(Entity &e, float dt);
    void updateWorker(Entity &e, float dt);
    void updatePassenger(Entity &e, float dt);
    void updateBuilding(Entity &e, float dt);
    void updateShots(float dt);
    void fight(Entity &e, Entity &t, float dt, bool canMove);
    void fire(Entity &e, Entity &t);
    void damage(Entity &t, float amount, int attacker);
    void splashDamage(Vec2 p, float radius, float amount, int owner, int attacker);
    int acquire(const Entity &e, float range) const;
    bool canTarget(const Entity &e, const Entity &t) const;
    bool inRange(const Entity &e, const Entity &t) const;
    float gap(const Entity &a, const Entity &b) const;
    bool buffed(const Entity &e) const;
    void board(Entity &e, Entity &vehicle);
    void unload(Entity &vehicle);
    bool follow(Entity &e, float dt);
    void moveDirect(Entity &e, Vec2 goal, float dt);
    void moveToward(Entity &e, const Entity &t, float dt, float repathInterval);
    bool approach(Entity &e, const Entity &t, float within, float dt);
    void setPath(Entity &e, Vec2 goal);
    void resolveCollisions();
    void pushOutOfTerrain(Entity &e);
    void updateFog();
    void updateSupply();
    void checkVictory();
    void spawnTrained(Entity &b, EType t);
    bool tryTrain(Entity &b, EType t);
    bool mineralBusy(const Entity &m, int except) const;
    int findFreeMineral(Vec2 p, float maxDist, int exclude) const;

    // Orders
    void orderMove(Entity &e, Vec2 p, bool attackMove);
    void orderAttack(Entity &e, int target);
    void orderGather(Entity &e, int mineral);
    void orderBuild(Entity &e, EType t, int tx, int ty);
    void orderConstruct(Entity &e, int building);
    void orderStop(Entity &e, bool hold);
    void orderBoard(Entity &e, int vehicle);

    // Map / queries
    bool inMap(int x, int y) const { return x >= 0 && y >= 0 && x < MAP_W && y < MAP_H; }
    int idx(int x, int y) const { return y * MAP_W + x; }
    bool blocked(int x, int y) const {
        return !inMap(x, y) || terrain_[idx(x, y)] != 0 || occ_[idx(x, y)] >= 0;
    }
    bool blockedAt(Vec2 p) const {
        return blocked((int) std::floor(p.x / TILE), (int) std::floor(p.y / TILE));
    }
    static Vec2 tileCenter(int x, int y) { return {(x + 0.5f) * TILE, (y + 0.5f) * TILE}; }
    bool findPath(Vec2 from, Vec2 to, std::vector<Vec2> &out);
    bool nearestFree(int &x, int &y, int prefX, int prefY) const;
    bool lineClear(Vec2 a, Vec2 b, float r) const;
    bool canPlace(EType t, int tx, int ty, int owner) const;
    static Vec2 footprintCenter(EType t, int tx, int ty);
    bool hasComplete(int owner, EType t) const;
    int nearestHQ(int owner, Vec2 p) const;
    int nearestMineral(Vec2 p, float maxDist) const;
    bool visibleToPlayer(const Entity &e) const;
    bool tileVisible(int x, int y) const { return inMap(x, y) && vis_[idx(x, y)] == 2; }
    Entity *ent(int id) { return (id >= 0 && id < (int) ents_.size() && ents_[id].alive) ? &ents_[id] : nullptr; }
    const Entity *ent(int id) const { return (id >= 0 && id < (int) ents_.size() && ents_[id].alive) ? &ents_[id] : nullptr; }
    void addFx(FxKind k, Vec2 a, Vec2 b, float dur, float size, Color c);
    void message(const std::string &m);

    // ----- AI (GameAI.cpp)
    void aiThink();
    bool aiBuild(EType t, Vec2 home);
    bool aiFindSite(EType t, Vec2 home, int &outX, int &outY);

    // ----- Input & commands (GameInput.cpp)
    enum Action {
        A_TRAIN, A_BUILD, A_STOP, A_HOLD, A_ATTACK, A_CANCEL, A_DEQUEUE, A_IDLE, A_ARMY, A_BOARD, A_UNLOAD
    };
    struct Button {
        Rectf r;
        Action action;
        EType param;
        std::string label, sub;
        bool enabled;
        bool warn;     // draw cost in red (can't afford right now)
    };
    void layout();
    void buildButtons();
    int buttonAt(Vec2 s) const;
    void doButton(const Button &b);
    void onTap(Vec2 s);
    void onBoxSelect(Vec2 a, Vec2 b);
    int pickEntity(Vec2 w, float tol) const;
    void commandSmart(Vec2 w, int hit);
    void commandAttack(Vec2 w, int hit);
    void commandBoard(int hit);
    void formationMove(const std::vector<int> &units, Vec2 p, bool attack);
    void tryPlaceBuilding(Vec2 w);
    void placementTile(Vec2 w, int &tx, int &ty) const;
    void cleanSelection();
    std::vector<int> selectedUnits() const;
    bool selectionHasWorkers() const;
    void clampCamera();
    void centerCamera(Vec2 p);
    void minimapJump(Vec2 s);
    Vec2 toWorld(Vec2 s) const { return cam_ + s / scale_; }
    Vec2 toScreen(Vec2 w) const { return (w - cam_) * scale_; }
    bool onScreen(Vec2 w) const;
    bool inHud(Vec2 s) const;
    void titleButtons(Rectf out[3]) const;

    // ----- Rendering (GameRender.cpp)
    void renderTitle(Draw2D &d);
    void renderWorld(Draw2D &d);
    void renderFog(Draw2D &d, int x0, int y0, int x1, int y1);
    void renderHud(Draw2D &d);
    void renderMinimap(Draw2D &d);
    void renderSelectionInfo(Draw2D &d);
    void renderEnd(Draw2D &d);
    void drawUnitShape(Draw2D &d, EType t, int owner, Vec2 p, float s, float facing, bool cargo,
                       float flash, int riders = 0) const;
    void drawBuildingShape(Draw2D &d, EType t, int owner, const Rectf &r, float facing,
                           float flash) const;
    void drawIcon(Draw2D &d, EType t, int owner, Vec2 c, float size) const;
    void drawHealthBar(Draw2D &d, Vec2 center, float width, float frac, float thick) const;

    // ----- State
    enum Screen { SCR_TITLE, SCR_PLAY, SCR_END } screen_ = SCR_TITLE;
    int difficulty_ = 1;
    bool victory_ = false;
    float realTime_ = 0, gameTime_ = 0, endTimer_ = 0, simAccum_ = 0;
    Rng rng_;

    std::deque<Entity> ents_;   // deque: references stay valid while spawning
    std::vector<int> live_;
    std::vector<uint8_t> terrain_, vis_, shade_;
    std::vector<int> occ_;
    int minerals_[2] = {};
    int supplyUsed_[2] = {}, supplyCap_[2] = {};
    int lost_[2] = {};
    std::vector<Fx> fx_;
    std::vector<Shot> shots_;
    float fogTimer_ = 0;

    // Pathfinding scratch
    std::vector<float> gCost_;
    std::vector<int> parent_;
    std::vector<uint32_t> openGen_, closedGen_;
    uint32_t searchGen_ = 0;

    // AI
    float aiTimer_ = 0, aiIncome_ = 1, aiBank_ = 0, aiFirstAttack_ = 240, aiWaveStart_ = 0;
    int aiWaveSize_ = 8, aiWave_ = 0;
    bool aiAttacking_ = false;

    // Alerts / messages
    float alertCooldown_ = 0, pingT_ = 0, msgT_ = 0;
    Vec2 pingPos_;
    std::string msg_;

    // Camera & input
    float screenW_ = 0, screenH_ = 0, ui_ = 1;
    float topH_ = 0, panelH_ = 0, panelY_ = 0;
    Rectf minimap_, info_;
    bool needCenter_ = true, scaleInit_ = false;
    Vec2 cam_;
    float scale_ = 1.3f;
    std::vector<int> sel_;
    enum Mode { MODE_NORMAL, MODE_PLACE, MODE_ATTACK, MODE_BOARD } mode_ = MODE_NORMAL;
    EType placeType_ = T_COUNT;
    Vec2 placePos_;
    bool placeVisible_ = false;

    struct Touch {
        int id = -1;
        Vec2 start, cur, prev;
        float t0 = 0;
    };
    Touch touches_[5];
    int numTouches_ = 0;
    enum Gesture { G_NONE, G_TAP, G_PAN, G_BOX, G_PINCH, G_HUD, G_MINIMAP, G_PLACE } gesture_ = G_NONE;
    float pinchDist_ = 0;
    Vec2 pinchMid_;
    int pressedBtn_ = -1;
    float lastTapTime_ = -10;
    int lastTapEnt_ = -1;
    int idleCycle_ = 0;
    std::vector<Button> buttons_;
};

#endif //ASTRAPUGNA_GAME_H
