#ifndef ASTRAPUGNA_SPRITES_H
#define ASTRAPUGNA_SPRITES_H

// Pixel-art unit sprites, drawn facing right. Each character is one pixel:
//   .  transparent        K  outline
//   A  team color         a  team shadow       H  team highlight
//   V  visor              v  visor shadow
//   G  gunmetal           g  gunmetal light
//   M  metal              m  metal shadow      L  metal light
//   T  tread              t  tread light
//   O  work suit          o  work suit shadow
//   S  skin               s  skin shadow
//   Y  gold / hazard      W  white

struct Sprite {
    int w, h;
    const char *const *rows;
    float px;   // world units per sprite pixel
};

// Returns nullptr for types without a sprite.
const Sprite *unitSprite(int type);

#endif //ASTRAPUGNA_SPRITES_H
