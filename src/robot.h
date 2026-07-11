#ifndef ROBOT_MODULE_H
#define ROBOT_MODULE_H

#include "raylib.h"

void DrawRobot(float screenWidth, bool happy, Color wantedColor,
               float pixelSize, float flowSpeed);
void DrawHappyRobot(Vector2 worldPosition, float pixelSize);
void DrawSadRobot(Vector2 worldPosition, float pixelSize);

#endif
