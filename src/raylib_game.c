/*******************************************************************************************
*
*   raylib gamejam template
*
*   Code licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2022-2026 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include "raymath.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>      // Emscripten library
#endif

#include <stdio.h>                          // Required for: printf()
#include <stdlib.h>                         // Required for: 
#include <string.h>                         // Required for:

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
// Simple log system to avoid printf() calls if required
// NOTE: Avoiding those calls, also avoids const strings memory usage
#define SUPPORT_LOG_INFO
#if defined(SUPPORT_LOG_INFO)
    #define LOG(...) printf(__VA_ARGS__)
#else
    #define LOG(...)
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef enum { 
    SCREEN_LOGO = 0, 
    SCREEN_TITLE, 
    SCREEN_GAMEPLAY, 
    SCREEN_ENDING
} GameScreen;

// TODO: Define your custom data types here

// A door that controls one (or one portion of a) side river.
typedef struct Door {
    Rectangle rect;
    bool open; // A door is closed by defeault
} Door;

// A button that controls opening of a door tagged by `doorIndex`.
typedef struct Button {
    Vector2 position;
    float radius;
    int doorIndex;      // Which door this button controls
} Button;

// Contains every object of a level.
typedef struct Level {
    Button *buttons;
    int buttonCount;

    Door *doors;
    int doorCount;
} Level;

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
//----------------------------------------------------------------------------------
static const int screenWidth = 720;
static const int screenHeight = 720;


static RenderTexture2D target = { 0 };  // Render texture to render our game
static int frameCounter = 0;

// Interaction radius around the player
static const float interactionRadius = 25.0f;

// TODO: Define global variables here, recommended to make them static

// Default radius of a button.
static const float buttonRadius = 12.0f;

// Radius of the player.
static const float playerRadius = 10.0f;

static Vector2 mainPlayerPosition = { (float)screenWidth/2, (float)screenHeight/2 };

// Three walkable rooms — left and right are narrower and shorter than mid.
static const Rectangle midRect   = {240,  60, 240, 600};
static const Rectangle leftRect  = {170, 260,  70, 200};
static const Rectangle rightRect = {480, 260,  70, 200};

// Shared borders between mid and side rooms, and the y-range where the sides overlap mid.
static const float borderL = 240.0f;
static const float borderR = 480.0f;
static const float overlapMinY = 260.0f;
static const float overlapMaxY = 460.0f;

#define DOOR_COUNT 2
static Door doors[DOOR_COUNT] = {
    { { 237.0f, 320.0f, 6.0f, 80.0f }, false },  // left-mid door on borderL
    { { 477.0f, 320.0f, 6.0f, 80.0f }, false },  // mid-right door on borderR
};

#define BUTTON_COUNT 2
static Button buttons[BUTTON_COUNT] = {
    { { 300.0f, 500.0f }, 12.0f, 0 },  // opens left-mid door
    { { 420.0f, 500.0f }, 12.0f, 1 },  // opens mid-right door
};


//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void UpdateDrawFrame(void);      // Update and Draw one frame

// Returns true if the player at playerY can cross the vertical border at borderX
// (an open door covers that y range).
static bool CanCrossBorder(float borderX, float playerY)
{
    if (playerY < overlapMinY || playerY > overlapMaxY) return false;

    for (int i = 0; i < DOOR_COUNT; i++) {
        float doorCenterX = doors[i].rect.x + doors[i].rect.width * 0.5f;
        if (fabsf(doorCenterX - borderX) > 1.0f) continue;
        if (!doors[i].open) continue;

        float dyMin = doors[i].rect.y;
        float dyMax = doors[i].rect.y + doors[i].rect.height;
        if (playerY >= dyMin && playerY <= dyMax) return true;
    }
    return false;
}

// Which room the player's center is currently in.
static Rectangle CurrentRoom(void)
{
    if (mainPlayerPosition.x < borderL) return leftRect;
    if (mainPlayerPosition.x > borderR) return rightRect;
    return midRect;
}

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
#if !defined(_DEBUG)
    SetTraceLogLevel(LOG_NONE);         // Disable raylib trace log messages
#endif



    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(screenWidth, screenHeight, "raylib gamejam template");
    
    // TODO: Load resources / Initialize variables at this point
    
    // Render texture to draw, enables screen scaling
    // NOTE: If screen is scaled, mouse input should be scaled proportionally
    target = LoadRenderTexture(screenWidth, screenHeight);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else
    SetTargetFPS(60);     // Set our game frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button
    {
        UpdateDrawFrame();
    }
#endif

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadRenderTexture(target);
    
    // TODO: Unload all loaded resources at this point

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//--------------------------------------------------------------------------------------------
// Module Functions Definition
//--------------------------------------------------------------------------------------------
// Update and draw frame
void UpdateDrawFrame(void)
{
    // Update
    //----------------------------------------------------------------------------------
    float dx = 0.0f, dy = 0.0f;
    if (IsKeyDown(KEY_W)) dy -= 2.0f;
    if (IsKeyDown(KEY_S)) dy += 2.0f;
    if (IsKeyDown(KEY_A)) dx -= 2.0f;
    if (IsKeyDown(KEY_D)) dx += 2.0f;

    // X axis: block border crossings unless an open door lines up with player.y
    float targetX = mainPlayerPosition.x + dx;
    if (dx > 0.0f) {
        float oldRight = mainPlayerPosition.x + playerRadius;
        float newRight = targetX + playerRadius;
        if (oldRight <= borderL && newRight > borderL && !CanCrossBorder(borderL, mainPlayerPosition.y)) {
            targetX = borderL - playerRadius;
        }
        if (oldRight <= borderR && newRight > borderR && !CanCrossBorder(borderR, mainPlayerPosition.y)) {
            targetX = borderR - playerRadius;
        }
    } else if (dx < 0.0f) {
        float oldLeft = mainPlayerPosition.x - playerRadius;
        float newLeft = targetX - playerRadius;
        if (oldLeft >= borderR && newLeft < borderR && !CanCrossBorder(borderR, mainPlayerPosition.y)) {
            targetX = borderR + playerRadius;
        }
        if (oldLeft >= borderL && newLeft < borderL && !CanCrossBorder(borderL, mainPlayerPosition.y)) {
            targetX = borderL + playerRadius;
        }
    }
    mainPlayerPosition.x = targetX;
    mainPlayerPosition.y += dy;

    // Clamp x to the outer walls and y to whichever room the player is in.
    Rectangle cur = CurrentRoom();
    mainPlayerPosition.x = Clamp(mainPlayerPosition.x,
                                 leftRect.x + playerRadius,
                                 rightRect.x + rightRect.width - playerRadius);
    mainPlayerPosition.y = Clamp(mainPlayerPosition.y,
                                 cur.y + playerRadius,
                                 cur.y + cur.height - playerRadius);

    // Interact with the nearest button under the interaction radius.
    if (IsKeyDown(KEY_R)) {
        for (int i = 0; i < BUTTON_COUNT; i++) {
            if (CheckCollisionPointCircle(buttons[i].position, mainPlayerPosition, interactionRadius)) {
                doors[buttons[i].doorIndex].open = true;
            }
        }
    }

    frameCounter++;
    //----------------------------------------------------------------------------------

    // Draw
    //----------------------------------------------------------------------------------
    // Render game screen to a texture, 
    // it could be useful for scaling or further shader postprocessing
    BeginTextureMode(target);
        ClearBackground(RAYWHITE);
    EndTextureMode();
    
    // Render to screen (main framebuffer)
    BeginDrawing();
        ClearBackground(RAYWHITE);
        
        DrawRectangleLinesEx(midRect,   4, DARKGRAY);
        DrawRectangleLinesEx(leftRect,  4, DARKGRAY);
        DrawRectangleLinesEx(rightRect, 4, DARKGRAY);

        for (int i = 0; i < DOOR_COUNT; i++) {
            if (doors[i].open) {
                // Punch a hole through the wall outline where the door sits.
                DrawRectangleRec(doors[i].rect, RAYWHITE);
            } else {
                DrawRectangleRec(doors[i].rect, GRAY);
            }
        }

        for (int i = 0; i < BUTTON_COUNT; i++) {
            bool near = CheckCollisionPointCircle(buttons[i].position, mainPlayerPosition, interactionRadius);
            if (near) {
                DrawCircleV(buttons[i].position, buttons[i].radius + 2.0f, YELLOW);
                DrawCircleV(buttons[i].position, buttons[i].radius - 2.0f, BLUE);
            } else {
                DrawCircleV(buttons[i].position, buttons[i].radius, BLUE);
            }
        }
        DrawCircleV(mainPlayerPosition, playerRadius, MAROON);
        DrawCircleLines(
            (int)mainPlayerPosition.x,
            (int)mainPlayerPosition.y,
            interactionRadius,
            LIGHTGRAY
        );
        // TODO: Draw everything that requires to be drawn at this point, maybe UI?

    EndDrawing();
    //----------------------------------------------------------------------------------  
}
