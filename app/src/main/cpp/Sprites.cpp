#include "Sprites.h"

#include "Game.h"

// clang-format off
static const char *const kWorker[] = {
    ".....KKKKK.......",
    "....KYYYYYK......",
    "...KYYWYYYYK.....",
    "....KVVVSSK......",
    "....KvvSSSK......",
    ".....KSSSK.......",
    "...KKOOAOOKK.....",
    "..KOOOOAOOOOK....",
    "..KOoKOAOKoOKKK..",
    "..KOK.KOOOKMMLLK.",
    "..KSK.KoooKKKKMK.",
    "...K..KOKOK...K..",
    ".....KOK.KOK.....",
    ".....KOK.KOK.....",
    "....KGGK.KGGK....",
    "....KKKK.KKKK....",
};

static const char *const kMarine[] = {
    ".....KKKKK.......",
    "....KAHHAAK......",
    "....KAAAVVVK.....",
    ".KK.KaAAVvvK.....",
    "KgGKKKaaaaKK.....",
    "KGKHHAAKaaKK.....",
    "KKHAAAAAKKKKKKK..",
    ".KAAAAAaKgggggggK",
    ".KaAAAaKAAGGGGKK.",
    "..KaaaKaAaKGK....",
    "...KKKaAaaK.K....",
    "....KaaKKaaK.....",
    "....KAAK.KAAK....",
    "...KAAAK.KAAAK...",
    "...KKKKK.KKKKK...",
};

static const char *const kSniper[] = {
    ".....KKKK..............",
    "....KaAAAK.............",
    "....KAAVVK.............",
    "....KaAvvK.............",
    ".....KKKK...KKKKKK.....",
    "...KKaAAaK.KKVVVVKK....",
    "..KaAAAAAaKKGGGGGGKKKKK",
    "..KAAAAAAKKgggggggggggK",
    "..KaAAAAKAAGKKKKKKKKKK.",
    "...KaAAAKaaK...........",
    "...KaaaaaKK............",
    "...KaK.KaK.............",
    "...KaK.KaK.............",
    "...KAK.KAAK............",
    "..KAAK.KAAAK...........",
    "..KKKK.KKKKK...........",
};

static const char *const kOfficer[] = {
    "..KK..............",
    "..KAAAAK..........",
    "..KAHAAAK.........",
    "..KAAAAK..........",
    "..K.KKKKKK........",
    "..K.KaaaaaaK......",
    "..K.KYaaaaaaK.....",
    "..K.KSSSSSK.......",
    "..K.KSSKSSK.......",
    "..K..KsSSK........",
    "..KKKaAYAAKK......",
    "..KaAAAAAAaAKKKK..",
    "...KaAAAAAKSGGGK..",
    "...KaaAAAaKKKKK...",
    "....KaKKKaK.......",
    "....KaK.KaK.......",
    "...KGGK.KGGK......",
    "...KKKK.KKKK......",
};

static const char *const kTank[] = {
    "...........KKKKKKK..........",
    "..........KaAAAAHAK.........",
    "..........KAAAAAAAKKKKKKKKKK",
    ".........KKAAAAAAAKggggggggK",
    ".........KaaAAAAAaKKKKKKKKKK",
    "....KKKKKKKKKKKKKKKKKKKK....",
    "...KMLLLLLLLLLLLLLLLLLLMK...",
    "..KMAAAAAAAAAAAAAAAAAAAAMK..",
    "..KMaaaaaaaaaaaaaaaaaaaaMMK.",
    ".KTTTTTTTTTTTTTTTTTTTTTTTTK.",
    ".KTtKTtKTtKTtKTtKTtKTtKTtTK.",
    ".KTttTTttTTttTTttTTttTTttTK.",
    "..KTTTTTTTTTTTTTTTTTTTTTTK..",
    "...KKKKKKKKKKKKKKKKKKKKKK...",
};

static const char *const kRover[] = {
    ".................K........",
    "................KVK.......",
    "...KK...........KVK.......",
    "...KMK..........KVVK......",
    "...KMK.........KKKKKKKK...",
    "KKKKMKKKKKKKKKKKAAAAAAAKK.",
    "KLLLLLLLLLLLLLLLAHHAAAAAAK",
    "KAAAAAAAAAAAAAAAAAAAAAAAYK",
    "KaaaaaaaaaaaaaaaaaaaaaaaaK",
    "KKKKGGGGKKKKKKKKKKKGGGGKKK",
    "...KGggtGK.......KGggtGK..",
    "...KGtggGK.......KGtggGK..",
    "....KGGGK.........KGGGK...",
    ".....KKK...........KKK....",
};
// clang-format on

template<int H>
static Sprite make(const char *const (&rows)[H], float px) {
    int w = 0;
    while (rows[0][w]) ++w;
    return {w, H, rows, px};
}

const Sprite *unitSprite(int type) {
    static const Sprite worker = make(kWorker, 1.25f);
    static const Sprite marine = make(kMarine, 1.3f);
    static const Sprite sniper = make(kSniper, 1.3f);
    static const Sprite officer = make(kOfficer, 1.2f);
    static const Sprite tank = make(kTank, 1.35f);
    static const Sprite rover = make(kRover, 1.35f);
    switch (type) {
        case T_WORKER: return &worker;
        case T_MARINE: return &marine;
        case T_SNIPER: return &sniper;
        case T_OFFICER: return &officer;
        case T_TANK: return &tank;
        case T_ROVER: return &rover;
        default: return nullptr;
    }
}
