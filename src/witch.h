#ifndef WITCH_H
#define WITCH_H

#include "raylib.h"

void WitchSetFacingFromMovement(float movementX);
void DrawWitch(Vector2 worldPosition, bool shadow, float pixelSize, Color crystalColor);

#endif
