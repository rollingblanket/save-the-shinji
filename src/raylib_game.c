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

// Whether a door is open
static bool doorOpen = false;




// TODO: Define global variables here, recommended to make them static

// Default radius of a button.
static const float buttonRadius = 12.0f;

Vector2 mainPlayerPosition = { (float)screenWidth/2, (float)screenHeight/2 };

Rectangle mapRect = {240, 60, 240, 600};

Vector2 buttonPosition = { (float)screenWidth/3, (float)screenHeight/3 };
Vector2 buttonSize = { 10, 20 };

Vector2 doorPosition = { (float)screenWidth/3, (float)screenHeight/4 };
Vector2 doorSize = { 100, 20 };


//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void UpdateDrawFrame(void);      // Update and Draw one frame

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
    // TODO: Update variables / Implement example logic at this point
        
        // Check if player is near a button
        bool playerNearButton = CheckCollisionPointCircle(
            buttonPosition,
            mainPlayerPosition,
            interactionRadius
        );

        if (IsKeyDown(KEY_W)) mainPlayerPosition.y -= 2;
        if (IsKeyDown(KEY_S)) mainPlayerPosition.y += 2;
        if (IsKeyDown(KEY_A)) mainPlayerPosition.x -= 2;
        if (IsKeyDown(KEY_D)) mainPlayerPosition.x += 2;
        if (IsKeyDown(KEY_R)) {
            if (playerNearButton) {
                doorOpen = true;
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
        
        DrawRectangleLinesEx(mapRect, 4, DARKGRAY);
        // Highlight button if player is near it
        if (playerNearButton) {
            DrawCircleV(buttonPosition, 14, YELLOW);
            DrawCircleV(buttonPosition, 10, BLUE);
        } else {
            DrawCircleV(buttonPosition, 12, BLUE);
        }

        if (!doorOpen) {
            DrawRectangleV(doorPosition, doorSize, GRAY);
        }
        DrawCircleV(mainPlayerPosition, 10, MAROON);
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
