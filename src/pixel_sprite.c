#include "pixel_sprite.h"

#include <math.h>

Color PixelSpriteColor(char key, Color customColor)
{
    switch (key)
    {
        case 'T': return (Color){ 106, 64, 156, 255 };
        case 'A': return (Color){ 217, 168, 60, 255 };
        case 'H': return (Color){ 58, 32, 92, 255 };
        case 'E': return (Color){ 36, 26, 48, 255 };
        case 'R': return (Color){ 84, 48, 128, 255 };
        case 'B': return (Color){ 138, 90, 43, 255 };
        case 'Y': return (Color){ 201, 151, 77, 255 };
        case 'X':
        case 'C': return customColor;
        case 'K': return (Color){ 42, 32, 48, 255 };
        case 'D': return (Color){ 57, 64, 74, 255 };
        case 'M': return (Color){ 152, 161, 173, 255 };
        case 'L': return (Color){ 201, 209, 218, 255 };
        case 'G': return (Color){ 168, 178, 192, 255 };
        case 'P': return (Color){ 95, 180, 86, 255 };
        case 'Q': return (Color){ 178, 222, 146, 255 };
        default: return (Color){ 232, 184, 138, 255 };
    }
}

void DrawPixelSprite(const char **rows, int width, int height,
                     Vector2 worldPosition, bool flipX, bool shadow,
                     float pixelSize, Color customColor)
{
    int left = (int)roundf(worldPosition.x/pixelSize) - width/2 + (shadow? 2 : 0);
    int top = (int)roundf(worldPosition.y/pixelSize) - height/2 + (shadow? 3 : 0);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            char key = rows[y][flipX? (width - 1 - x) : x];
            if (key == '.') continue;

            int blockX = left + x;
            int blockY = top + y;
            Color color = shadow? (Color){ 10, 10, 20, 115 } : PixelSpriteColor(key, customColor);
            DrawRectangle((int)(blockX*pixelSize), (int)(blockY*pixelSize),
                          (int)pixelSize, (int)pixelSize, color);
        }
    }
}
