#ifndef PIXEL_SPRITE_H
#define PIXEL_SPRITE_H

#include "raylib.h"

Color PixelSpriteColor(char key, Color customColor);
void DrawPixelSprite(const char **rows, int width, int height,
                     Vector2 worldPosition, bool flipX, bool shadow,
                     float pixelSize, Color customColor);

#endif
