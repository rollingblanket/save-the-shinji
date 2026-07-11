#include "robot.h"

#include "pixel_sprite.h"

#include <math.h>

#define ROBOT_W 48
#define ROBOT_H 32

static const char *robotSprite[ROBOT_H] = {
    "..............................AAA...............",
    "..............................AAA...............",
    "..............................AAA...............",
    ".............................D..................",
    "............................D...................",
    "...........................D....................",
    "...........................D....................",
    "...........................D....................",
    ".....DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD....",
    "....DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD....",
    "...DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD...",
    "...DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD...",
    "...DMMMMDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDMMMMD...",
    "...DMMMMDLLLLLLLLLLLLLLLLLLLLLLLLLLLLLGDMMMMD...",
    "...DMMMMDLLLLLLLLLLLLLLLLLLLLLLLLLLLLLGDMMMMD...",
    ".DDDMMMMDLLLLLKKLLLLLLLLLLLLLLLLKKLLLLGDMMMMDDD.",
    "DMMDMMMMDLLLLLLKKLLLLLLLLLLLLLLKKLLLLLGDMMMMDMMD",
    "DMMDMMMMDLLLLLLLKKLLLLLLLLLLLLKKLLLLLLGDMMMMDMMD",
    "DMMDMMMMDLLLLLLKKLLLLLLLLLLLLLLKKLLLLLGDMMMMDMMD",
    "DMMDMMMMDLLLLLKKLLLLLLLLLLLLLLLLKKLLLLGDMMMMDMMD",
    "DMMDMMMMDLLLLLLLLLLLLLLLLLLLLLLLLLLLLLGDMMMMDMMD",
    ".DDDMMMMDLLLLLLLLLLLKLLLLLLKLLLLLLLLLLGDMMMMDDD.",
    "...DMMMMDLLLLLLLLLLLKLLKKLLKLLLLLLLLLLGDMMMMD...",
    "...DMMMMDLLLLLLLLLLLLKKLLKKLLLLLLLLLLLGDMMMMD...",
    "...DMMMMDGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGDMMMMD...",
    "...DMMMMDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDMMMMD...",
    "...DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD...",
    "...DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD...",
    "....DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD....",
    ".....DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD....",
    "................................................",
    "................................................"
};

static const char *robotSpriteHappy[ROBOT_H] = {
    "..............................AAA...............",
    "..............................AAA...............",
    "..............................AAA...............",
    ".............................D..................",
    "............................D...................",
    "...........................D....................",
    "...........................D....................",
    "...........................D....................",
    ".....DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD....",
    "....DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD....",
    "...DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD...",
    "...DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD...",
    "...DMMMMDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDMMMMD...",
    "...DMMMMDLLLLLLLLLLLLLLLLLLLLLLLLLLLLLGDMMMMD...",
    "...DMMMMDLLLLLLLLLLLLLLLLLLLLLLLLLLLLLGDMMMMD...",
    ".DDDMMMMDLLLLLLLLLLLLLLLLLLLLLLLLLLLLLGDMMMMDDD.",
    "DMMDMMMMDLLLLLLKKLLLLLLLLLLLLLLKKLLLLLGDMMMMDMMD",
    "DMMDMMMMDLLLLLKLLKLLLLLLLLLLLLKLLKLLLLGDMMMMDMMD",
    "DMMDMMMMDLLLLLLLLLLLLLLLLLLLLLLLLLLLLLGDMMMMDMMD",
    "DMMDMMMMDLLLLLLLLLLLLLLLLLLLLLLLLLLLLLGDMMMMDMMD",
    "DMMDMMMMDLLLLLLLLLLLLLLLLLLLLLLLLLLLLLGDMMMMDMMD",
    ".DDDMMMMDLLLLLLLLLLKLLLLLLLLKLLLLLLLLLGDMMMMDDD.",
    "...DMMMMDLLLLLLLLLLLKLLLLLLKLLLLLLLLLLGDMMMMD...",
    "...DMMMMDLLLLLLLLLLLLKKKKKKLLLLLLLLLLLGDMMMMD...",
    "...DMMMMDGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGDMMMMD...",
    "...DMMMMDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDMMMMD...",
    "...DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD...",
    "...DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD...",
    "....DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD....",
    ".....DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD....",
    "................................................",
    "................................................"
};

static const char *robotSpriteSad[ROBOT_H] = {
    "..............................AAA...............",
    "..............................AAA...............",
    "..............................AAA...............",
    ".............................D..................",
    "............................D...................",
    "...........................D....................",
    "...........................D....................",
    "...........................D....................",
    ".....DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD....",
    "....DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD....",
    "...DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD...",
    "...DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD...",
    "...DMMMMDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDMMMMD...",
    "...DMMMMDLLLLLLLLLLLLLLLLLLLLLLLLLLLLLGDMMMMD...",
    "...DMMMMDLLLLLLLLLLLLLLLLLLLLLLLLLLLLLGDMMMMD...",
    ".DDDMMMMDLLLLLLLLLLLLLLLLLLLLLLLLLLLLLGDMMMMDDD.",
    "DMMDMMMMDLLLLLLLKKLLLLLLLLLLLLKKLLLLLLGDMMMMDMMD",
    "DMMDMMMMDLLLLLKKLLLLLLLLLLLLLLLLKKLLLLGDMMMMDMMD",
    "DMMDMMMMDLLLLLLLLLLLLLLLLLLLLLLLLLLLLLGDMMMMDMMD",
    "DMMDMMMMDLLLLLLLLLLLLLLLLLLLLLLLLLLLLLGDMMMMDMMD",
    "DMMDMMMMDLLLLLLLLLLLLLLLLLLLLLLLLLLLLLGDMMMMDMMD",
    ".DDDMMMMDLLLLLLLLLLLLKKKKKKLLLLLLLLLLLGDMMMMDDD.",
    "...DMMMMDLLLLLLLLLLLKLLLLLLKLLLLLLLLLLGDMMMMD...",
    "...DMMMMDLLLLLLLLLLKLLLLLLLLKLLLLLLLLLGDMMMMD...",
    "...DMMMMDGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGDMMMMD...",
    "...DMMMMDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDMMMMD...",
    "...DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD...",
    "...DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD...",
    "....DMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD....",
    ".....DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD....",
    "................................................",
    "................................................"
};

#define BUBBLE_W 24
#define BUBBLE_H 15

static const char *bubbleSprite[BUBBLE_H] = {
    "......KKKKKKKKKK........",
    "....KKLLLLLLLLLLKK......",
    "...KLLLLLLLLLLLLLLLK....",
    "..KLLLLLLLLLLLLLLLLLK...",
    ".KLLLLLLLLLLLLLLLLLLLK..",
    ".KLLKKKKKKKKKKKKKKKLLK..",
    ".KLLCCCCCCCCCCCCCCCLLK..",
    ".KLLCCCCCCCCCCCCCCCLLK..",
    ".KLLCCCCCCCCCCCCCCCLLK..",
    ".KLLKKKKKKKKKKKKKKKLLK..",
    ".KLLLLLLLLLLLLLLLLLLLK..",
    "..KLLLLLLLLLLLLLLLLLK...",
    "...KLLLLLLLLLLLLLLLK....",
    "....KKLLLLLLLLLLKK......",
    "......KKKKKKKKKK........"
};

static const char *miniBubbleBig[4] = {
    ".KK.",
    "KLLK",
    "KLLK",
    ".KK."
};

static const char *miniBubbleSmall[3] = {
    ".K.",
    "KLK",
    ".K."
};

static Color RobotWaterLight(Color color)
{
    return (Color){ color.r + (255 - color.r)*35/100,
                    color.g + (255 - color.g)*35/100,
                    color.b + (255 - color.b)*35/100, 255 };
}

static Color RobotWaterDark(Color color)
{
    return (Color){ color.r*72/100, color.g*72/100, color.b*72/100, 255 };
}

void DrawRobot(float screenWidth, bool happy, Color wantedColor,
               float pixelSize, float flowSpeed)
{
    float time = (float)GetTime();
    Vector2 robotPosition = { screenWidth/2.0f, 96.0f };
    Vector2 position = robotPosition;
    position.y += sinf(time*1.6f)*pixelSize;

    DrawPixelSprite(happy? robotSpriteHappy : robotSprite, ROBOT_W, ROBOT_H,
                    position, false, false, pixelSize, wantedColor);

    if (!happy)
    {
        Vector2 bubblePosition = { robotPosition.x + 216,
            robotPosition.y - 48 + sinf(time*1.6f + 1.0f)*pixelSize };
        DrawPixelSprite(bubbleSprite, BUBBLE_W, BUBBLE_H, bubblePosition,
                        false, false, pixelSize, wantedColor);

        int left = (int)roundf(bubblePosition.x/pixelSize) - BUBBLE_W/2;
        int top = (int)roundf(bubblePosition.y/pixelSize) - BUBBLE_H/2;
        for (int y = 6; y <= 8; y++)
        {
            for (int x = 4; x <= 18; x++)
            {
                Color water = wantedColor;
                if ((y == 6) || (y == 8)) water = RobotWaterDark(water);
                else
                {
                    float streak = fmodf(x*pixelSize + time*flowSpeed, 90.0f);
                    if ((streak < 24.0f) && (((x*7 + y*3)%5) < 3)) water = RobotWaterLight(water);
                }
                DrawRectangle((int)((left + x)*pixelSize), (int)((top + y)*pixelSize),
                              (int)pixelSize, (int)pixelSize, water);
            }
        }

        Vector2 miniBig = { robotPosition.x + 168,
            robotPosition.y - 6 + sinf(time*1.6f + 2.2f)*pixelSize };
        Vector2 miniSmall = { robotPosition.x + 132,
            robotPosition.y + 24 + sinf(time*1.6f + 3.4f)*pixelSize };
        DrawPixelSprite(miniBubbleBig, 4, 4, miniBig, false, false, pixelSize, wantedColor);
        DrawPixelSprite(miniBubbleSmall, 3, 3, miniSmall, false, false, pixelSize, wantedColor);
    }

    if (((int)(time*(happy? 6.0f : 2.0f)))%2 == 0)
    {
        int left = (int)roundf(position.x/pixelSize) - ROBOT_W/2;
        int top = (int)roundf(position.y/pixelSize) - ROBOT_H/2;
        DrawRectangle((int)((left + 30)*pixelSize), (int)(top*pixelSize),
                      (int)(3*pixelSize), (int)(3*pixelSize),
                      (Color){ 240, 224, 138, 255 });
    }
}

void DrawSadRobot(Vector2 worldPosition, float pixelSize)
{
    DrawPixelSprite(robotSpriteSad, ROBOT_W, ROBOT_H, worldPosition,
                    false, false, pixelSize, (Color){ 0 });
}
