#include "restart_spinner.h"

#include "raylib.h"

#include <math.h>

#define RESTART_SPINNER_PI 3.14159265358979323846f

void DrawRestartSpinner(float progress, int sceneResolution, float pixelSize)
{
    int centerX = sceneResolution/2;
    int centerY = sceneResolution/2;
    float filled = progress*2.0f*RESTART_SPINNER_PI;

    for (int blockY = centerY - 9; blockY <= centerY + 9; blockY++)
    {
        for (int blockX = centerX - 9; blockX <= centerX + 9; blockX++)
        {
            float dx = (float)(blockX - centerX);
            float dy = (float)(blockY - centerY);
            float distance = sqrtf(dx*dx + dy*dy);
            if ((distance < 4.0f) || (distance > 8.5f)) continue;

            Color color = { 0 };
            if ((distance < 4.8f) || (distance > 7.7f))
                color = (Color){ 13, 15, 18, 255 };
            else
            {
                float angle = atan2f(dx, -dy);
                if (angle < 0.0f) angle += 2.0f*RESTART_SPINNER_PI;
                color = (angle <= filled)? (Color){ 240, 224, 138, 255 } : (Color){ 45, 50, 60, 255 };
            }
            DrawRectangle((int)(blockX*pixelSize), (int)(blockY*pixelSize),
                          (int)pixelSize, (int)pixelSize, color);
        }
    }
}
