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
#include <math.h>                           // Required for: sqrtf(), fmodf()

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

//----------------------------------------------------------------------------------
// River network
//----------------------------------------------------------------------------------
// One point of a river network (a DAG). Water flows from each node to its single
// downstream node; junctions are nodes with multiple inflows. The segment to the
// downstream node is subdivided into color samples that advect (shift) downstream
// over time, so color changes travel along the river as a front
#define RIVER_SAMPLE_SPACING 8.0f       // Distance between color samples (pixels)
#define RIVER_FLOW_SPEED 220.0f         // How fast water/dye travels (pixels/second)
#define MAX_SEGMENT_SAMPLES 64

typedef struct RiverNode {
    Vector2 position;
    int downstream;         // Index of node this one flows into, -1 = river mouth
    bool isMain;            // Part of the main river (side rivers may flow into it)
    bool isSource;
    Color sourceColor;      // Fixed color, only used when isSource
    Color color;            // Computed: steady-state color of water leaving this node
    float flow;             // Computed: water volume through this node, 0 = dry
    Color samples[MAX_SEGMENT_SAMPLES]; // Water colors along segment to downstream, head first
    int sampleCount;
} RiverNode;

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

static Vector2 mainPlayerPosition = { (float)screenWidth/2, (float)screenHeight - (float)screenHeight/4 };

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
// River globals
//----------------------------------------------------------------------------------
#define MAX_RIVER_NODES 32
static RiverNode rivers[MAX_RIVER_NODES] = { 0 };
static int riverCount = 0;
static bool riversMerged = false;   // Until true, junctions keep the main river's color
static float riverFlowAccum = 0.0f; // Distance accumulator for advection steps

// Pixel-art scene renderer: the whole scene (floor, walls, water) is generated
// per-pixel into a low-res buffer from a distance field around the river
// segments, then upscaled with point filtering for the pixel-art look
#define RIVER_RES 120                       // Low-res buffer size (720/120 = 6x pixels)
#define RIVER_PIXEL 6.0f                    // World pixels per buffer pixel
static Color riverPixels[RIVER_RES*RIVER_RES] = { 0 };
static Texture2D riverTexture = { 0 };

// Channel cross-section, in world pixels: water | slope | rim | outline
static const float channelHalfWidth = 56.0f;
static const float slopeWidth = 32.0f;
static const float rimWidth = 18.0f;
static const float cornerChamfer = 45.0f;   // 45-degree cut at junction corners

// Metal wall shade ramp, darkest to lightest
static const Color metalRamp[8] = {
    { 13, 15, 18, 255 }, 
    { 34, 38, 44, 255 }, 
    { 57, 64, 74, 255 }, 
    { 74, 81, 92, 255 },
    { 111, 120, 133, 255 }, 
    { 152, 161, 173, 255 }, 
    { 201, 209, 218, 255 }, 
    { 232, 237, 242, 255 }
};
static const Color floorDark = { 38, 41, 47, 255 };
static const Color floorMid = { 43, 46, 53, 255 };
static const Color floorNoise = { 49, 53, 60, 255 };

//----------------------------------------------------------------------------------
// River colors, should be standard across all 
//----------------------------------------------------------------------------------
static const Color riverBlue = { 47, 111, 208, 255 };
static const Color riverRed = { 192, 58, 43, 255 };

//----------------------------------------------------------------------------------
// Witch sprite (3/4 view, ASCII bitmap: each char is one low-res pixel,
// '.' = transparent, letters are palette keys mapped by SpriteColor())
//----------------------------------------------------------------------------------
#define WITCH_SIZE 16
static const char *witchSprite[WITCH_SIZE] = {
    "........T.......",
    ".......TTT......",
    ".......TTT......",
    "......TTTTT.....",
    "......AAAAA.....",
    "...HHHHHHHHHH...",
    ".....FFFFFF.....",
    ".....FEFFEF.....",
    ".....FFFFFF.....",
    ".....RRRRRR.....",
    "....RRRRRRRR....",
    "...SRRRRRRRRS...",
    "YYBBBBBBBBBBBB..",
    "YYY..RRRR.......",
    ".....K..K.......",
    "................"
};
static bool witchFlipX = false;     // True when facing left, so the bristles trail behind

// Robot head (48x32): sits at the end of the main river, which forms its "body".
// Wider (252 world px) than the full channel footprint (212 world px)
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
static const Vector2 robotPosition = { (float)screenWidth/2, 96 };  // Head center, over the river mouth

//----------------------------------------------------------------------------------
// River layout: the main river always runs vertically through the screen center
// (screenWidth/2). Side rivers are the tweakable part
//----------------------------------------------------------------------------------
static const float mainRiverMouthY = 150.0f;        // Main river ends here (under the robot head)
static const float sideRiverJunctionY = 375.0f;     // Where the side river joins the main river
static const Vector2 sideRiverSource = { 0, 375 };  // Side river source (edit freely, diagonals work too)


//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void UpdateDrawFrame(void);      // Update and Draw one frame
static void BuildRivers(void);          // Create the river network
static void PropagateRiverColors(void); // Recompute steady-state colors/flow (topological order)
static void UpdateRiverFlow(float dt);  // Advect color samples downstream
static void RenderRiverPixels(float time);  // Fill the low-res scene buffer
static float RiverDistance(Vector2 p);      // Distance from a point to the river centerline network
// Draw an ASCII sprite snapped to the low-res pixel grid (shadow = flat dark silhouette)
static void DrawSprite(const char **rows, int w, int h, Vector2 worldPos, bool flipX, bool shadow);
static void DrawWitch(Vector2 worldPos, bool shadow);  // Draw the player sprite (or its shadow)
static void DrawRobot(void);                            // Robot head with idle bob + antenna blink

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
    BuildRivers();

    // Low-res scene texture, upscaled with point filtering for crisp pixels
    Image riverImage = GenImageColor(RIVER_RES, RIVER_RES, BLACK);
    riverTexture = LoadTextureFromImage(riverImage);
    UnloadImage(riverImage);
    SetTextureFilter(riverTexture, TEXTURE_FILTER_POINT);

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
    UnloadTexture(riverTexture);
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
    // Interact with the nearest button under the interaction radius.
    if (IsKeyDown(KEY_R)) {
        riversMerged = true;
        PropagateRiverColors();
        for (int i = 0; i < BUTTON_COUNT; i++) {
            if (CheckCollisionPointCircle(buttons[i].position, mainPlayerPosition, interactionRadius)) {
                doors[buttons[i].doorIndex].open = true;
            }
        }
    }

    if (IsKeyPressed(KEY_SPACE)) BuildRivers();     // Reset demo


    // Face the direction of horizontal travel (sprite flips, bristles trail behind)
    if (dx < 0.0f) witchFlipX = true;
    else if (dx > 0.0f) witchFlipX = false;

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

        UpdateRiverFlow(GetFrameTime());


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
        // Pixel-art scene: rebuild the low-res buffer and draw it scaled up
        RenderRiverPixels((float)GetTime());
        UpdateTexture(riverTexture, riverPixels);
        DrawTexturePro(riverTexture,
            (Rectangle){ 0, 0, RIVER_RES, RIVER_RES },
            (Rectangle){ 0, 0, (float)screenWidth, (float)screenHeight },
            (Vector2){ 0, 0 }, 0.0f, WHITE);

        if (!riversMerged) DrawText("Press R to merge the rivers", 20, 690, 20, RAYWHITE);
        else DrawText("Merged! Press SPACE to reset", 20, 690, 20, RAYWHITE);


        for (int i = 0; i < BUTTON_COUNT; i++) {
            bool near = CheckCollisionPointCircle(buttons[i].position, mainPlayerPosition, interactionRadius);
            if (near) {
                DrawCircleV(buttons[i].position, buttons[i].radius + 2.0f, YELLOW);
                DrawCircleV(buttons[i].position, buttons[i].radius - 2.0f, BLUE);
            } else {
                DrawCircleV(buttons[i].position, buttons[i].radius, BLUE);
            }
        }
        // Robot head at the river's end (the river is its body)
        DrawRobot();

        // Player: shadow stays at the true position, the witch bobs above it
        DrawWitch(mainPlayerPosition, true);
        Vector2 witchHover = mainPlayerPosition;
        witchHover.y += sinf((float)GetTime()*5.0f)*4.0f;
        DrawWitch(witchHover, false);
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

//--------------------------------------------------------------------------------------------
// River network functions
//--------------------------------------------------------------------------------------------
static int AddRiverNode(Vector2 position, int downstream, bool isMain, bool isSource, Color sourceColor)
{
    rivers[riverCount] = (RiverNode){
        .position = position, .downstream = downstream, .isMain = isMain,
        .isSource = isSource, .sourceColor = sourceColor, .color = sourceColor
    };
    return riverCount++;
}

// Build the rivers. Nodes are added mouth-first so downstream indices already exist
static void BuildRivers(void)
{
    riverCount = 0;

    // Main river (blue): straight vertical channel flowing UPWARD (bottom to top),
    // always centered on the screen
    float mainX = (float)screenWidth/2;
    int mouth = AddRiverNode((Vector2){ mainX, mainRiverMouthY }, -1, true, false, BLANK);
    int junction = AddRiverNode((Vector2){ mainX, sideRiverJunctionY }, mouth, true, false, BLANK);
    AddRiverNode((Vector2){ mainX, (float)screenHeight }, junction, true, true, riverBlue);

    // Side river (red): enters the junction from the side
    AddRiverNode(sideRiverSource, junction, false, true, riverRed);

    riversMerged = false;
    riverFlowAccum = 0.0f;

    PropagateRiverColors();

    // Subdivide each segment into samples, filled with the steady-state color
    for (int i = 0; i < riverCount; i++)
    {
        int d = rivers[i].downstream;
        if (d >= 0)
        {
            float dx = rivers[d].position.x - rivers[i].position.x;
            float dy = rivers[d].position.y - rivers[i].position.y;
            int count = (int)(sqrtf(dx*dx + dy*dy)/RIVER_SAMPLE_SPACING) + 1;
            rivers[i].sampleCount = (count > MAX_SEGMENT_SAMPLES)? MAX_SEGMENT_SAMPLES : count;
            for (int k = 0; k < rivers[i].sampleCount; k++) rivers[i].samples[k] = rivers[i].color;
        }
        else rivers[i].sampleCount = 0;
    }
}

// Recompute steady-state color and flow of every node, processing in topological
// order (each node only after all its upstream inputs are resolved)
static void PropagateRiverColors(void)
{
    float r[MAX_RIVER_NODES] = { 0 };       // Accumulated inflow color, flow-weighted
    float g[MAX_RIVER_NODES] = { 0 };
    float b[MAX_RIVER_NODES] = { 0 };
    float inFlow[MAX_RIVER_NODES] = { 0 };
    Color mainColor[MAX_RIVER_NODES] = { 0 };   // Color arriving from the main river, if any
    float mainFlow[MAX_RIVER_NODES] = { 0 };
    bool hasMain[MAX_RIVER_NODES] = { 0 };
    int remaining[MAX_RIVER_NODES] = { 0 };
    int queue[MAX_RIVER_NODES] = { 0 };
    int head = 0;
    int tail = 0;

    for (int i = 0; i < riverCount; i++)
    {
        int d = rivers[i].downstream;
        if (d >= 0) remaining[d]++;
    }

    for (int i = 0; i < riverCount; i++)
    {
        if (remaining[i] == 0) queue[tail++] = i;   // Sources are ready immediately
    }

    while (head < tail)
    {
        int i = queue[head++];

        if (rivers[i].isSource)
        {
            rivers[i].color = rivers[i].sourceColor;
            rivers[i].flow = 1.0f;
        }
        else if (inFlow[i] > 0.0f)
        {
            if (riversMerged || !hasMain[i])
            {
                // Mix everything that flowed in, weighted by flow volume
                rivers[i].color = (Color){
                    (unsigned char)(r[i]/inFlow[i]),
                    (unsigned char)(g[i]/inFlow[i]),
                    (unsigned char)(b[i]/inFlow[i]),
                    255
                };
                rivers[i].flow = inFlow[i];
            }
            else
            {
                // Not merged yet: the main river's water passes through unchanged
                rivers[i].color = mainColor[i];
                rivers[i].flow = mainFlow[i];
            }
        }
        else rivers[i].flow = 0.0f;     // No inflow: dry channel

        int d = rivers[i].downstream;
        if (d >= 0)
        {
            r[d] += rivers[i].color.r*rivers[i].flow;
            g[d] += rivers[i].color.g*rivers[i].flow;
            b[d] += rivers[i].color.b*rivers[i].flow;
            inFlow[d] += rivers[i].flow;
            if (rivers[i].isMain)
            {
                mainColor[d] = rivers[i].color;
                mainFlow[d] = rivers[i].flow;
                hasMain[d] = true;
            }
            remaining[d]--;
            if (remaining[d] == 0) queue[tail++] = d;
        }
    }
}

// Color of the water leaving node i right now, based on the water that has
// actually ARRIVED at it (the tail samples of its inflow segments)
static Color RiverNodeOutputColor(int i)
{
    if (rivers[i].isSource) return rivers[i].sourceColor;

    float r = 0.0f, g = 0.0f, b = 0.0f, f = 0.0f;
    Color mainCol = rivers[i].color;
    bool hasMain = false;

    for (int j = 0; j < riverCount; j++)
    {
        if ((rivers[j].downstream == i) && (rivers[j].flow > 0.0f) && (rivers[j].sampleCount > 0))
        {
            Color c = rivers[j].samples[rivers[j].sampleCount - 1];     // Water arriving now
            r += c.r*rivers[j].flow;
            g += c.g*rivers[j].flow;
            b += c.b*rivers[j].flow;
            f += rivers[j].flow;
            if (rivers[j].isMain)
            {
                mainCol = c;
                hasMain = true;
            }
        }
    }

    if (f <= 0.0f) return rivers[i].color;
    if (!riversMerged && hasMain) return mainCol;   // Side inflows don't tint the main river yet
    return (Color){ (unsigned char)(r/f), (unsigned char)(g/f), (unsigned char)(b/f), 255 };
}

// Advect: every RIVER_SAMPLE_SPACING pixels of travel, shift all segment
// samples one step downstream and inject each node's current output at the head
static void UpdateRiverFlow(float dt)
{
    riverFlowAccum += RIVER_FLOW_SPEED*dt;

    while (riverFlowAccum >= RIVER_SAMPLE_SPACING)
    {
        riverFlowAccum -= RIVER_SAMPLE_SPACING;

        // Snapshot outputs first so a step moves water by exactly one sample
        Color out[MAX_RIVER_NODES] = { 0 };
        for (int i = 0; i < riverCount; i++) out[i] = RiverNodeOutputColor(i);

        for (int i = 0; i < riverCount; i++)
        {
            for (int k = rivers[i].sampleCount - 1; k > 0; k--) rivers[i].samples[k] = rivers[i].samples[k - 1];
            if (rivers[i].sampleCount > 0) rivers[i].samples[0] = out[i];
        }
    }
}

//--------------------------------------------------------------------------------------------
// Pixel-art scene renderer
//--------------------------------------------------------------------------------------------
static float SegmentDistance(Vector2 p, Vector2 a, Vector2 b, float *tOut)
{
    float abx = b.x - a.x;
    float aby = b.y - a.y;
    float len2 = abx*abx + aby*aby;
    float t = ((p.x - a.x)*abx + (p.y - a.y)*aby)/len2;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float cx = a.x + abx*t;
    float cy = a.y + aby*t;
    *tOut = t;
    return sqrtf((p.x - cx)*(p.x - cx) + (p.y - cy)*(p.y - cy));
}


/* Return c lightened 35% toward white, with full opacity. */
static Color WaterLight(Color c)
{
    return (Color){ c.r + (255 - c.r)*35/100, c.g + (255 - c.g)*35/100, c.b + (255 - c.b)*35/100, 255 };
}


/* Return c darkened to 72% brightness, with full opacity. */
static Color WaterDark(Color c)
{
    return (Color){ c.r*72/100, c.g*72/100, c.b*72/100, 255 };
}

// Fill the low-res buffer: for every pixel, distance to the nearest river
// segment decides what it is (water / slope / rim / outline / floor)
static void RenderRiverPixels(float time)
{
    for (int py = 0; py < RIVER_RES; py++)
    {
        for (int px = 0; px < RIVER_RES; px++)
        {
            Vector2 p = { (px + 0.5f)*RIVER_PIXEL, (py + 0.5f)*RIVER_PIXEL };

            // Nearest and second-nearest segments (second one is for the chamfer)
            float best = 1e9f, second = 1e9f, bestT = 0.0f;
            int bestSeg = -1;
            for (int i = 0; i < riverCount; i++)
            {
                int ds = rivers[i].downstream;
                if (ds < 0) continue;
                float t = 0.0f;
                float dd = SegmentDistance(p, rivers[i].position, rivers[ds].position, &t);
                if (dd < best) { second = best; best = dd; bestSeg = i; bestT = t; }
                else if (dd < second) second = dd;
            }

            float d = best;
            float chamfered = (best + second - cornerChamfer)*0.7071f;  // 45-degree corner cut
            if (chamfered < d) d = chamfered;

            Color out;
            if ((bestSeg >= 0) && (d <= channelHalfWidth + slopeWidth + rimWidth + 11.0f))
            {
                RiverNode *seg = &rivers[bestSeg];
                Vector2 a = seg->position;
                Vector2 bp = rivers[seg->downstream].position;
                float segLen = sqrtf((bp.x - a.x)*(bp.x - a.x) + (bp.y - a.y)*(bp.y - a.y));
                float along = bestT*segLen;

                if (d <= channelHalfWidth)
                {
                    // Water: color comes from the advected samples along the segment
                    int count = seg->sampleCount;
                    Color w = seg->color;
                    if (count > 1)
                    {
                        float kf = bestT*(count - 1) + (((px + py) & 1)? 0.5f : 0.0f);  // Dithered
                        int k = (int)kf;
                        if (k > count - 1) k = count - 1;
                        w = seg->samples[k];
                    }
                    // Flow streaks moving downstream
                    float sp = fmodf(along - time*RIVER_FLOW_SPEED, 90.0f);
                    if (sp < 0.0f) sp += 90.0f;
                    if ((sp < 15.0f) && (d < channelHalfWidth - 16.0f) && (((px*7 + py*3)%5) < 3)) w = WaterLight(w);
                    if (d > channelHalfWidth - 9.0f) w = WaterDark(w);  // Waterline edge
                    out = w;
                }
                else if (d <= channelHalfWidth + slopeWidth)
                {
                    // Angled bank: gradient down to the water, directional light, hatch ridges
                    Vector2 cpx = { a.x + (bp.x - a.x)*bestT, a.y + (bp.y - a.y)*bestT };
                    float ox = (p.x - cpx.x)/best;
                    float oy = (p.y - cpx.y)/best;
                    float lit = ox*-0.7071f + oy*-0.7071f;              // Sun from top-left
                    int dirShade = (lit > 0.25f)? -1 : (lit < -0.25f)? 1 : 0;

                    float gd = (d - channelHalfWidth)/slopeWidth;
                    int idx = 3 + (int)(gd*2.0f + 0.5f) + dirShade;
                    float hatch = fmodf(along, 75.0f);
                    if (hatch < 0.0f) hatch += 75.0f;
                    if (hatch < 10.0f) idx -= 1;
                    if (idx < 1) idx = 1;
                    if (idx > 6) idx = 6;
                    out = metalRamp[idx];
                }
                else if (d <= channelHalfWidth + slopeWidth + rimWidth)
                {
                    // Flat top rim, with a darker seam where it meets the slope
                    out = (d < channelHalfWidth + slopeWidth + 6.0f)? metalRamp[4] : metalRamp[6];
                }
                else out = metalRamp[1];    // Outer outline
            }
            else
            {
                // Floor: checkered tiles with sparse noise
                int tx = (int)(p.x/48.0f);
                int ty = (int)(p.y/48.0f);
                out = ((tx + ty) & 1)? floorDark : floorMid;
                if (((px*13 + py*29)%41) == 0) out = floorNoise;
            }

            riverPixels[py*RIVER_RES + px] = out;
        }
    }
}

//--------------------------------------------------------------------------------------------
// Player sprite
//--------------------------------------------------------------------------------------------
// Shared palette for all ASCII sprites (witch, robot, ...)
static Color SpriteColor(char c)
{
    switch (c)
    {
        case 'T': return (Color){ 106, 64, 156, 255 };  // Hat cone
        case 'A': return (Color){ 217, 168, 60, 255 };  // Gold (hat band, antenna ball)
        case 'H': return (Color){ 58, 32, 92, 255 };    // Brim
        case 'E': return (Color){ 36, 26, 48, 255 };    // Eyes
        case 'R': return (Color){ 84, 48, 128, 255 };   // Robe
        case 'B': return (Color){ 138, 90, 43, 255 };   // Broom stick
        case 'Y': return (Color){ 201, 151, 77, 255 };  // Bristles
        case 'K': return (Color){ 42, 32, 48, 255 };    // Near-black (boots, robot face)
        case 'D': return (Color){ 57, 64, 74, 255 };    // Dark metal outline
        case 'M': return (Color){ 152, 161, 173, 255 }; // Metal
        case 'L': return (Color){ 201, 209, 218, 255 }; // Screen light
        case 'G': return (Color){ 168, 178, 192, 255 }; // Screen shade
        default: return (Color){ 232, 184, 138, 255 };  // 'F'/'S' skin
    }
}

// Draw an ASCII sprite (or its drop shadow) snapped to the low-res pixel grid.
// Each sprite pixel is drawn as one RIVER_PIXEL-sized rectangle on top of the
// scene texture, so it stays on the same grid as the pixel-art scene
static void DrawSprite(const char **rows, int w, int h, Vector2 worldPos, bool flipX, bool shadow)
{
    // Round (don't truncate) so sprites center correctly on half-pixel positions
    int left = (int)roundf(worldPos.x/RIVER_PIXEL) - w/2 + (shadow? 2 : 0);
    int top = (int)roundf(worldPos.y/RIVER_PIXEL) - h/2 + (shadow? 3 : 0);

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            char c = rows[y][flipX? (w - 1 - x) : x];
            if (c == '.') continue;

            int bx = left + x;
            int by = top + y;
            Color col = shadow? (Color){ 10, 10, 20, 115 } : SpriteColor(c);
            DrawRectangle((int)(bx*RIVER_PIXEL), (int)(by*RIVER_PIXEL),
                          (int)RIVER_PIXEL, (int)RIVER_PIXEL, col);
        }
    }
}

static void DrawWitch(Vector2 worldPos, bool shadow)
{
    DrawSprite(witchSprite, WITCH_SIZE, WITCH_SIZE, worldPos, witchFlipX, shadow);
}

// Robot head idle animation: gentle one-pixel bob plus blinking antenna ball
static void DrawRobot(void)
{
    float t = (float)GetTime();
    Vector2 pos = robotPosition;
    pos.y += sinf(t*1.6f)*RIVER_PIXEL;      // Rounds to a 1 low-res pixel bob

    DrawSprite(robotSprite, ROBOT_W, ROBOT_H, pos, false, false);

    // Blink: overdraw the 3x3 antenna ball (sprite cells x30..32, y0..2) brighter
    if (((int)(t*2.0f))%2 == 0)
    {
        int left = (int)roundf(pos.x/RIVER_PIXEL) - ROBOT_W/2;
        int top = (int)roundf(pos.y/RIVER_PIXEL) - ROBOT_H/2;
        DrawRectangle((int)((left + 30)*RIVER_PIXEL), (int)(top*RIVER_PIXEL),
                      (int)(3*RIVER_PIXEL), (int)(3*RIVER_PIXEL), (Color){ 240, 224, 138, 255 });
    }
}
