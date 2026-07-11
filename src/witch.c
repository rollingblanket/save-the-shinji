#include "witch.h"

#include "pixel_sprite.h"

#define WITCH_SIZE 16

static const char *witchSprite[WITCH_SIZE] = {
    "........T.......",
    ".......TTT......",
    ".......TTT......",
    "......TTTTT.....",
    "......AAAAA.....",
    "...HHHHHHHHHH...",
    ".....FFFFFF....X",
    ".....FEFFEF....X",
    ".....FFFFFF....B",
    ".....RRRRRR...B.",
    "....RRRRRRRR.B..",
    "...SRRRRRRRRS...",
    "YYBBBBBBBBBBBB..",
    "YYY..RRRR.......",
    ".....K..K.......",
    "................"
};

static bool witchFlipX = false;

void WitchSetFacingFromMovement(float movementX)
{
    if (movementX < 0.0f) witchFlipX = true;
    else if (movementX > 0.0f) witchFlipX = false;
}

void DrawWitch(Vector2 worldPosition, bool shadow, float pixelSize, Color crystalColor)
{
    DrawPixelSprite(witchSprite, WITCH_SIZE, WITCH_SIZE, worldPosition,
                    witchFlipX, shadow, pixelSize, crystalColor);
}
