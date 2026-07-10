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
    int sideRiverIndex;     // -1 for main river nodes; side source id otherwise
    int damDoor;            // Door blocking this node's outflow, -1 = none
    bool isPool;            // Segment is a dye basin (a door fill): water flowing
                            // through it always blends with standingColor
    Color sourceColor;      // Fixed color, only used when isSource
    Color standingColor;    // Initial fill of the segment to downstream
    Color color;            // Computed: steady-state color of water leaving this node
    float flow;             // Computed: water volume through this node, 0 = dry
    Color samples[MAX_SEGMENT_SAMPLES]; // Water colors along segment to downstream, head first
    int sampleCount;
} RiverNode;

// A door dams one point of a side river: water upstream of it stands still until
// the spell opens it. May carry a color lock (only a matching wand opens it)
typedef struct Door {
    Rectangle rect;
    int sideRiverIndex;
    bool open;              // A door is closed by default
    bool frogged;           // Opened by the spell: a frog sits here now
    Color requiresWandColor;// Lock: alpha 0 = no lock, any spell works
    Color fill;             // Initial standing water just downstream of this door (alpha 0 = dry)
} Door;

typedef struct DoorDef {
    int sideRiverIndex;
    float distFromMouth;    // 0 = at the junction bank; >0 = farther out along the side river
    bool open;
    Color requiresWandColor;
    Color fill;
} DoorDef;

// A button that controls opening of a door tagged by `doorIndex`.
typedef struct Button {
    Vector2 position;
    float radius;
    int doorIndex;      // Which door this button controls
} Button;

typedef struct SideRiverDef {
    Vector2 source;
    Color sourceColor;
    float junctionY;
} SideRiverDef;

// Contains every object of a level.
typedef struct LevelDef {
    const SideRiverDef *sideRivers;
    int sideRiverCount;

    const Button *buttons;
    int buttonCount;

    const DoorDef *doors;
    int doorCount;

    Vector2 playerSpawn;
    Color robotWantedColor;
} LevelDef;

typedef struct JunctionRender {
    Vector2 position;
    float sideDir;
    int upperMainSegment;
    int lowerMainSegment;
    int sideSegment;
} JunctionRender;

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

// Radius of the player.
static const float playerRadius = 10.0f;

static Vector2 mainPlayerPosition = { (float)screenWidth/2, (float)screenHeight - (float)screenHeight/4 };

//----------------------------------------------------------------------------------
// River globals
//----------------------------------------------------------------------------------
#define MAX_RIVER_NODES 32
#define MAX_SIDE_RIVERS 8
#define MAX_LEVEL_DOORS 8
#define MAX_LEVEL_BUTTONS 8
#define MAX_JUNCTIONS MAX_SIDE_RIVERS
static RiverNode rivers[MAX_RIVER_NODES] = { 0 };
static int riverCount = 0;
static bool riversMerged = false;   // Until true, junctions keep the main river's color
static float riverFlowAccum = 0.0f; // Distance accumulator for advection steps
static JunctionRender junctions[MAX_JUNCTIONS] = { 0 };
static int junctionCount = 0;
static Door doors[MAX_LEVEL_DOORS] = { 0 };
static int doorCount = 0;
static Button buttons[MAX_LEVEL_BUTTONS] = { 0 };
static int buttonCount = 0;
static int currentLevelIndex = 0;

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

// 4x4 Bayer matrix for ordered dithering: color transitions blend across a band
// by comparing against these thresholds, giving smooth pixel-art gradients
static const float bayer4[4][4] = {
    { 0.03f, 0.53f, 0.16f, 0.66f },
    { 0.78f, 0.28f, 0.91f, 0.41f },
    { 0.22f, 0.72f, 0.09f, 0.59f },
    { 0.97f, 0.47f, 0.84f, 0.34f }
};

#include "river_colors.h"

//----------------------------------------------------------------------------------
// Witch sprite (3/4 view, ASCII bitmap: each char is one low-res pixel,
// '.' = transparent, letters are palette keys mapped by SpriteColor())
//----------------------------------------------------------------------------------
#define WITCH_SIZE 16
// She holds a wand in her outer hand: 'B' stick angled up-right, 'X' crystal
// tip. The tip renders in the dipped pigment color once the wand is charged
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
// Happy variant of the robot head: same shell, but the screen shows arc eyes
// and a smile. Drawn instead of robotSprite once robotHappy is set
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
// Sad variant for the title screen: drooping outer eyebrows and a frown
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
static const Vector2 robotPosition = { (float)screenWidth/2, 96 };  // Head center, over the river mouth

//----------------------------------------------------------------------------------
// Title screen: story typewriter + the classic raylib logo animation, playing
// together on one screen. ENTER starts the game
//----------------------------------------------------------------------------------
static GameScreen currentScreen = SCREEN_TITLE;

static const char *storyLines[] = {
    "Shinji, the river robot, drinks from",
    "the great river to power his brain.",
    "",
    "But the floodgates rusted shut, only one",
    "witch still remembers how to brew the rivers.",
    "",
    "Casting hex to paint the water.",
    "Save the Shinji.",
};
#define STORY_LINE_COUNT (int)(sizeof(storyLines)/sizeof(storyLines[0]))
static float storyCharsShown = 0.0f;            // Typewriter progress, in characters
static const float storyCharsPerSecond = 40.0f;

// raylib logo animation, half scale (128px), bottom-right badge. Plays through
// its classic states once, then stays put as a "powered by" mark
static int logoState = 0;       // 0 corner blink, 1 top+left bars, 2 bottom+right bars, 3 letters
static int logoFrames = 0;
static int logoLetters = 0;
static int logoTopW = 8, logoLeftH = 8, logoBottomW = 8, logoRightH = 8;
#define LOGO_SIZE 128
#define LOGO_BORDER 8

// Thought bubble shown right of the robot: a snippet of river (two banks with
// water between) in the color he wants. 'C' pixels resolve to robotWantedColor
// at draw time, so the bubble recolors itself per level. Tail dots bottom-left
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

// Mini bubbles trailing between the robot and the thought bubble
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

// Level goal: the robot is happy once water of the color it wants ARRIVES at the
// river mouth (not just when the steady-state color changes on merge)
static Color robotWantedColor = { 0 };  // Target water color, set in BuildRivers()
static bool robotHappy = false;         // Latched true when the wanted color reaches the mouth
static int riverMouthIndex = 0;         // Node index of the main river mouth (under the robot)

//----------------------------------------------------------------------------------
// Spell: pressing R near the gate turns it into a frog, releasing the side river.
// The rivers merge only when the spell actually hits the gate, so the whole chain
// is visible: cast -> frog -> red water flows -> color front reaches the robot
//----------------------------------------------------------------------------------
typedef enum {
    GATE_CLOSED = 0,    // Idle: no spell in flight, gates castable
    SPELL_FLYING,       // Spark travels from the witch to the target gate
    GATE_TRANSFORM      // Poof! Target gate is turning into a frog
} SpellState;

static SpellState spellState = GATE_CLOSED;
static int spellTargetDoor = -1;            // Door the current spell is aimed at

// Wand: R over open water dips it, charging it with that water's color. Locked
// gates only yield to a spell cast with the matching wand color
static Color wandColor = { 0 };
static bool wandCharged = false;
static float wandFlashTimer = 0.0f;         // Crystal flash right after a dip
static const float wandFlashTime = 0.3f;
static float fizzleTimer = 0.0f;            // Gray poof when a locked gate rejects the spell
static Vector2 fizzlePos = { 0 };
static const float fizzleTime = 0.35f;

// Restarting requires holding SPACE; a ring spinner shows the hold progress
static float spaceHoldTime = 0.0f;
static const float restartHoldTime = 0.6f;
static Vector2 spellPos = { 0 };            // Current position of the flying spark
static float spellTimer = 0.0f;             // Timer for the transform poof
static const float spellSpeed = 300.0f;     // Spark travel speed (pixels/second)
static const float transformTime = 0.25f;   // Poof duration (seconds)

// The gate is drawn at door 0, which is derived from its side river mouth; the
// witch must stand within gateCastRadius of it to cast.
static const float gateCastRadius = 60.0f;

// Gate sprite: vertical bars across the side river channel
#define GATE_W 4
#define GATE_H 20
static const char *gateSprite[GATE_H] = {
    "DDDD",
    "DMMD",
    "DMMD",
    "DMMD",
    "DDDD",
    "DMMD",
    "DMMD",
    "DMMD",
    "DMMD",
    "DDDD",
    "DMMD",
    "DMMD",
    "DMMD",
    "DMMD",
    "DDDD",
    "DMMD",
    "DMMD",
    "DMMD",
    "DMMD",
    "DDDD"
};

// Frog, two idle frames (sitting / squashed blink)
#define FROG_W 10
#define FROG_H 7
static const char *frogSprite1[FROG_H] = {
    ".PP....PP.",
    ".PKP..PKP.",
    ".PPPPPPPP.",
    "PPPPPPPPPP",
    "PQQQQQQQQP",
    "PQQQQQQQQP",
    ".PP....PP."
};
static const char *frogSprite2[FROG_H] = {
    "..........",
    ".PP....PP.",
    ".PKPPPPKP.",
    "PPPPPPPPPP",
    "PQQQQQQQQP",
    "PQQQQQQQQP",
    ".PP....PP."
};

//----------------------------------------------------------------------------------
// River layout: the main river always runs vertically through the screen center
// (screenWidth/2). Side rivers are the tweakable part
//----------------------------------------------------------------------------------
static const float mainRiverMouthY = 150.0f;        // Main river ends here (under the robot head)

#include "levels.inc"


//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void UpdateDrawFrame(void);      // Update and Draw one frame
static void LoadLevel(int levelIndex);  // Reset level runtime state and create river network
static void BuildRivers(const LevelDef *level); // Create the river network from level data
static void PropagateRiverColors(void); // Recompute steady-state colors/flow (topological order)
static void UpdateRiverFlow(float dt);  // Advect color samples downstream
static void UpdateRobotMood(void);      // Latch robotHappy once the wanted color arrives at the mouth
static void StartSpell(void);           // Cast if the witch is near a gate (R key)
static void DipWand(void);              // Charge the wand with the water below (T key)
static void UpdateSpell(float dt);      // Spell state machine: fly -> poof -> frog + merge
static void DrawSpellScene(void);       // Draw gate / spark / poof / frog by state
static bool NodeDammed(int i);          // True if a closed door blocks node i's outflow
static Color WaterColorAt(Vector2 p);   // Water color under a point, BLANK if not over water
static bool WandMatchesDoor(const Door *door);  // Does the wand pigment satisfy the door's color lock?
static void RenderRiverPixels(float time);  // Fill the low-res scene buffer
static float RiverDistance(Vector2 p);      // Distance from a point to the river centerline network
// Draw an ASCII sprite snapped to the low-res pixel grid (shadow = flat dark silhouette)
static void DrawSprite(const char **rows, int w, int h, Vector2 worldPos, bool flipX, bool shadow);
static void DrawWitch(Vector2 worldPos, bool shadow);  // Draw the player sprite (or its shadow)
static void DrawRobot(void);                            // Robot head with idle bob + antenna blink
static void DrawRestartSpinner(float progress);         // Hold-to-restart progress ring
static void UpdateDrawTitle(void);                      // Title screen: story + raylib logo badge

//----------------------------------------------------------------------------------
// Audio
//----------------------------------------------------------------------------------
static Music themeMusic = { 0 };        // Looping title screen music, streamed
static Music backgroundMusic = { 0 };   // Looping level music, streamed
static Sound goalSound = { 0 };         // Level complete jingle
static Sound castSound = { 0 };         // Spell cast (R)
static Sound wandSound = { 0 };         // Wand enchantment (T)

// Resolve a file in assets/ whether the game runs from the repo root or src/
static const char *AssetPath(const char *fileName)
{
    static char path[256];
    snprintf(path, sizeof(path), "%s/%s", DirectoryExists("assets")? "assets" : "../assets", fileName);
    return path;
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

    InitAudioDevice();
    themeMusic = LoadMusicStream(AssetPath("theme.mp3"));
    backgroundMusic = LoadMusicStream(AssetPath("background.mp3"));
    goalSound = LoadSound(AssetPath("goal.mp3"));
    castSound = LoadSound(AssetPath("hex-casting.mp3"));
    wandSound = LoadSound(AssetPath("wandsfx.mp3"));
    themeMusic.looping = true;
    backgroundMusic.looping = true;
    SetMusicVolume(themeMusic, 0.5f);
    SetMusicVolume(backgroundMusic, 0.5f);  // Keep sfx audible over the music
    PlayMusicStream(themeMusic);            // Title screen music; gameplay music starts on ENTER

    // TODO: Load resources / Initialize variables at this point
    LoadLevel(0);

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

    UnloadMusicStream(themeMusic);
    UnloadMusicStream(backgroundMusic);
    UnloadSound(goalSound);
    UnloadSound(castSound);
    UnloadSound(wandSound);
    CloseAudioDevice();

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
    UpdateMusicStream(themeMusic);
    UpdateMusicStream(backgroundMusic);

    if (currentScreen != SCREEN_GAMEPLAY)
    {
        UpdateDrawTitle();
        return;
    }

    float dx = 0.0f, dy = 0.0f;
    if (IsKeyDown(KEY_W)) dy -= 2.0f;
    if (IsKeyDown(KEY_S)) dy += 2.0f;
    if (IsKeyDown(KEY_A)) dx -= 2.0f;
    if (IsKeyDown(KEY_D)) dx += 2.0f;
    // R casts the spell (only works near the gate)
    // R casts at gates, T dips the wand. Pressed, not held: holding T while
    // flying would re-dip the wand in every river crossed, silently replacing
    // the pigment being carried
    if (IsKeyPressed(KEY_R)) StartSpell();
    // Enchanting wand is only available after level 2
    if (currentLevelIndex >= 2 && IsKeyPressed(KEY_T)) DipWand();

    // SPACE: advancing after a win is a simple press, but restarting mid-level
    // must be HELD for a moment so a stray tap doesn't wipe progress
    if (robotHappy)
    {
        spaceHoldTime = 0.0f;
        if (IsKeyPressed(KEY_SPACE)) LoadLevel((currentLevelIndex + 1)%levelCount);
    }
    else if (IsKeyDown(KEY_SPACE))
    {
        spaceHoldTime += GetFrameTime();
        if (spaceHoldTime >= restartHoldTime)
        {
            spaceHoldTime = 0.0f;
            LoadLevel(currentLevelIndex);
        }
    }
    else spaceHoldTime = 0.0f;


    // Face the direction of horizontal travel (sprite flips, bristles trail behind)
    if (dx < 0.0f) witchFlipX = true;
    else if (dx > 0.0f) witchFlipX = false;

    // The witch can only fly above the river network (water + banks). Each axis
    // is tested separately so she slides along the channel edge instead of stopping
    float flyHalfWidth = channelHalfWidth + slopeWidth;
    Vector2 tryX = { mainPlayerPosition.x + dx, mainPlayerPosition.y };
    if (RiverDistance(tryX) <= flyHalfWidth) mainPlayerPosition.x = tryX.x;
    Vector2 tryY = { mainPlayerPosition.x, mainPlayerPosition.y + dy };
    if (RiverDistance(tryY) <= flyHalfWidth) mainPlayerPosition.y = tryY.y;

    // Keep the sprite on screen where the rivers touch the map edges
    mainPlayerPosition.x = Clamp(mainPlayerPosition.x, playerRadius, screenWidth - playerRadius);
    mainPlayerPosition.y = Clamp(mainPlayerPosition.y, playerRadius, screenHeight - playerRadius);

        UpdateRiverFlow(GetFrameTime());
    UpdateSpell(GetFrameTime());
    UpdateRobotMood();


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
        
        // Pixel-art scene: rebuild the low-res buffer and draw it scaled up
        RenderRiverPixels((float)GetTime());
        UpdateTexture(riverTexture, riverPixels);
        DrawTexturePro(riverTexture,
            (Rectangle){ 0, 0, RIVER_RES, RIVER_RES },
            (Rectangle){ 0, 0, (float)screenWidth, (float)screenHeight },
            (Vector2){ 0, 0 }, 0.0f, WHITE);

        if (robotHappy) DrawText("Level complete! Press SPACE to continue", 20, 690, 20, YELLOW);
        else {
            if (currentLevelIndex >= 2) {
                DrawText("Enchan(T) your wand with the river color", 20, 660, 20, RAYWHITE);
            }
            if (currentLevelIndex < 2) {
                DrawText("Press R to hex a gate", 20, 660, 20, RAYWHITE);
            }
            DrawText("Press and hold SPACE to restart", 20, 690, 20, RAYWHITE);
        }

        // Gate / spell spark / poof / frog, depending on the spell state
        DrawSpellScene();

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
            wandCharged? wandColor : LIGHTGRAY
        );



        // Hold-to-restart progress spinner, center screen
        if (spaceHoldTime > 0.0f) DrawRestartSpinner(spaceHoldTime/restartHoldTime);
        // TODO: Draw everything that requires to be drawn at this point, maybe UI?

    EndDrawing();
    //----------------------------------------------------------------------------------
}

//--------------------------------------------------------------------------------------------
// River network functions
//--------------------------------------------------------------------------------------------
static int AddRiverNode(Vector2 position, bool isMain, bool isSource, Color sourceColor, int sideRiverIndex)
{
    if (riverCount >= MAX_RIVER_NODES)
    {
        LOG("Too many river nodes; increase MAX_RIVER_NODES\n");
        return MAX_RIVER_NODES - 1;
    }

    rivers[riverCount] = (RiverNode){
        .position = position,
        .downstream = -1,
        .isMain = isMain,
        .isSource = isSource,
        .sideRiverIndex = sideRiverIndex,
        .damDoor = -1,
        .standingColor = sourceColor,
        .sourceColor = sourceColor,
        .color = sourceColor
    };
    return riverCount++;
}

static void SortJunctionYs(float *ys, int count)
{
    for (int i = 1; i < count; i++)
    {
        float y = ys[i];
        int j = i - 1;
        while ((j >= 0) && (ys[j] > y))
        {
            ys[j + 1] = ys[j];
            j--;
        }
        ys[j + 1] = y;
    }
}

static int FindJunctionY(const float *ys, int count, float y)
{
    for (int i = 0; i < count; i++)
    {
        if (fabsf(ys[i] - y) < 0.5f) return i;
    }
    return -1;
}

static Vector2 DoorCenter(const Door *door)
{
    return (Vector2){
        door->rect.x + door->rect.width*0.5f,
        door->rect.y + door->rect.height*0.5f
    };
}

static void InitializeRiverSamples(void)
{
    for (int i = 0; i < riverCount; i++)
    {
        int d = rivers[i].downstream;
        if (d >= 0)
        {
            float dx = rivers[d].position.x - rivers[i].position.x;
            float dy = rivers[d].position.y - rivers[i].position.y;
            int count = (int)(sqrtf(dx*dx + dy*dy)/RIVER_SAMPLE_SPACING) + 1;
            rivers[i].sampleCount = (count > MAX_SEGMENT_SAMPLES)? MAX_SEGMENT_SAMPLES : count;
            // A dam node's own segment (just downstream of its door) starts as the
            // door's fill pool; everything else starts as its steady color, or dry
            Color c;
            rivers[i].isPool = (rivers[i].damDoor >= 0) && (doors[rivers[i].damDoor].fill.a != 0);
            if (rivers[i].isPool) c = doors[rivers[i].damDoor].fill;
            else c = (rivers[i].flow > 0.0f)? rivers[i].color : BLANK;
            rivers[i].standingColor = (c.a != 0)? c : rivers[i].color;
            for (int k = 0; k < rivers[i].sampleCount; k++) rivers[i].samples[k] = c;
        }
        else rivers[i].sampleCount = 0;
    }
}

// A door dams the node it sits on: while closed, that node's water stops there
static bool NodeDammed(int i)
{
    return (rivers[i].damDoor >= 0) && !doors[rivers[i].damDoor].open;
}

static void LoadLevel(int levelIndex)
{
    if (levelIndex < 0) levelIndex = 0;
    if (levelIndex >= levelCount) levelIndex = levelCount - 1;
    currentLevelIndex = levelIndex;

    const LevelDef *level = &levelDefs[currentLevelIndex];
    doorCount = (level->doorCount > MAX_LEVEL_DOORS)? MAX_LEVEL_DOORS : level->doorCount;
    for (int i = 0; i < doorCount; i++)
    {
        int sideIndex = level->doors[i].sideRiverIndex;
        // distFromMouth 0 puts the gate at the junction bank; larger values move
        // it outward along the side river. Water upstream of a closed gate stands
        // still; the segment just downstream starts as the door's fill color
        const SideRiverDef *def = &level->sideRivers[sideIndex];
        float mainX = (float)screenWidth/2;
        float dir = (def->source.x < mainX)? -1.0f : 1.0f;
        Vector2 block = { mainX + dir*(channelHalfWidth + level->doors[i].distFromMouth), def->junctionY };
        float w = GATE_W*RIVER_PIXEL;
        float h = GATE_H*RIVER_PIXEL;
        doors[i] = (Door){
            .rect = { block.x - w*0.5f, block.y - h*0.5f, w, h },
            .sideRiverIndex = sideIndex,
            .open = level->doors[i].open,
            .requiresWandColor = level->doors[i].requiresWandColor,
            .fill = level->doors[i].fill
        };
    }

    buttonCount = (level->buttonCount > MAX_LEVEL_BUTTONS)? MAX_LEVEL_BUTTONS : level->buttonCount;
    for (int i = 0; i < buttonCount; i++) buttons[i] = level->buttons[i];

    mainPlayerPosition = level->playerSpawn;
    BuildRivers(level);
}

// Build the rivers from declarative level data. Nodes are created first, then
// downstream links and renderer junction metadata are filled in.
static void BuildRivers(const LevelDef *level)
{
    riverCount = 0;
    junctionCount = 0;

    // Main river (blue): straight vertical channel flowing UPWARD (bottom to top),
    // always centered on the screen
    float mainX = (float)screenWidth/2;
    float junctionYs[MAX_JUNCTIONS] = { 0 };
    int uniqueJunctionCount = 0;
    for (int i = 0; i < level->sideRiverCount; i++)
    {
        if (fabsf(level->sideRivers[i].source.y - level->sideRivers[i].junctionY) > 0.5f)
        {
            LOG("Side river %d is not horizontal; source.y should match junctionY\n", i);
        }
        if (fabsf(level->sideRivers[i].source.x - mainX) <= channelHalfWidth)
        {
            LOG("Side river %d source starts inside the main channel\n", i);
        }
        if (FindJunctionY(junctionYs, uniqueJunctionCount, level->sideRivers[i].junctionY) >= 0) continue;
        if (uniqueJunctionCount >= MAX_JUNCTIONS)
        {
            LOG("Too many river junctions; increase MAX_JUNCTIONS\n");
            break;
        }
        junctionYs[uniqueJunctionCount++] = level->sideRivers[i].junctionY;
    }
    SortJunctionYs(junctionYs, uniqueJunctionCount);
    for (int i = 1; i < uniqueJunctionCount; i++)
    {
        if (junctionYs[i] - junctionYs[i - 1] < channelHalfWidth)
        {
            LOG("River junctions %.1f and %.1f are close enough for arcs to overlap\n",
                junctionYs[i - 1], junctionYs[i]);
        }
    }

    int mouth = AddRiverNode((Vector2){ mainX, mainRiverMouthY }, true, false, BLANK, -1);
    riverMouthIndex = mouth;
    int junctionNodes[MAX_JUNCTIONS] = { 0 };
    for (int i = 0; i < uniqueJunctionCount; i++)
    {
        junctionNodes[i] = AddRiverNode((Vector2){ mainX, junctionYs[i] }, true, false, BLANK, -1);
    }
    int mainSource = AddRiverNode((Vector2){ mainX, (float)screenHeight }, true, true, riverBlue, -1);

    for (int i = 0; i < uniqueJunctionCount; i++)
    {
        rivers[junctionNodes[i]].downstream = (i == 0)? mouth : junctionNodes[i - 1];
    }
    if (uniqueJunctionCount > 0) rivers[mainSource].downstream = junctionNodes[uniqueJunctionCount - 1];
    else rivers[mainSource].downstream = mouth;

    // Side rivers enter their matching main-river junction. Each of the river's
    // doors becomes a dam node along the way, splitting it into sections that
    // hold (or brew) their own water
    for (int i = 0; i < level->sideRiverCount; i++)
    {
        int sortedJunction = FindJunctionY(junctionYs, uniqueJunctionCount, level->sideRivers[i].junctionY);
        if (sortedJunction < 0) continue;

        int junctionNode = junctionNodes[sortedJunction];
        int prev = AddRiverNode(level->sideRivers[i].source, false, true, level->sideRivers[i].sourceColor, i);

        // Walk this river's doors from the outermost (nearest the source) inward
        bool used[MAX_LEVEL_DOORS] = { 0 };
        for (;;)
        {
            int next = -1;
            for (int d = 0; d < doorCount; d++)
            {
                if ((doors[d].sideRiverIndex != i) || used[d]) continue;
                if ((next < 0) || (level->doors[d].distFromMouth > level->doors[next].distFromMouth)) next = d;
            }
            if (next < 0) break;
            used[next] = true;

            int damNode = AddRiverNode(DoorCenter(&doors[next]), false, false, BLANK, i);
            rivers[damNode].damDoor = next;
            rivers[prev].downstream = damNode;
            prev = damNode;
        }
        rivers[prev].downstream = junctionNode;

        if (junctionCount < MAX_JUNCTIONS)
        {
            int lowerMainSegment = (sortedJunction + 1 < uniqueJunctionCount)?
                junctionNodes[sortedJunction + 1] : mainSource;
            junctions[junctionCount++] = (JunctionRender){
                .position = rivers[junctionNode].position,
                .sideDir = (level->sideRivers[i].source.x < mainX)? -1.0f : 1.0f,
                .upperMainSegment = junctionNode,
                .lowerMainSegment = lowerMainSegment,
                .sideSegment = prev
            };
        }
    }

    riversMerged = false;
    riverFlowAccum = 0.0f;
    spellState = GATE_CLOSED;
    spellTargetDoor = -1;
    spellTimer = 0.0f;
    fizzleTimer = 0.0f;
    wandCharged = false;
    wandColor = (Color){ 0 };
    wandFlashTimer = 0.0f;
    robotWantedColor = level->robotWantedColor;
    robotHappy = false;

    PropagateRiverColors();
    InitializeRiverSamples();
}

static bool SameColor(Color a, Color b)
{
    return (a.r == b.r) && (a.g == b.g) && (a.b == b.b);
}

// Mix two water colors: primary pairs produce their defined secondary,
// anything else falls back to an RGB average
static Color MixWaterColors(Color a, Color b)
{
    if (SameColor(a, b)) return a;

    bool hasRed = SameColor(a, riverRed) || SameColor(b, riverRed);
    bool hasBlue = SameColor(a, riverBlue) || SameColor(b, riverBlue);
    bool hasYellow = SameColor(a, riverYellow) || SameColor(b, riverYellow);
    bool hasPurple = SameColor(a, riverPurple) || SameColor(b, riverPurple);
    bool hasOrange = SameColor(a, riverOrange) || SameColor(b, riverOrange);
    bool hasGreen = SameColor(a, riverGreen) || SameColor(b, riverGreen);

    if (hasRed && hasBlue) return riverPurple;
    if (hasRed && hasYellow) return riverOrange;
    if (hasBlue && hasYellow) return riverGreen;
    if (hasRed && hasOrange) return riverVermilion;
    if (hasYellow && hasOrange) return riverAmber;
    if (hasYellow && hasGreen) return riverChartreuse;
    if (hasBlue && hasGreen) return riverTeal;
    if (hasBlue && hasPurple) return riverViolet;
    if (hasRed && hasPurple) return riverMagenta;

    return (Color){ (a.r + b.r)/2, (a.g + b.g)/2, (a.b + b.b)/2, 255 };
}

// Recompute steady-state color and flow of every node, processing in topological
// order (each node only after all its upstream inputs are resolved)
static void PropagateRiverColors(void)
{
    Color mixColor[MAX_RIVER_NODES] = { 0 };    // Folded mix of all inflow colors
    bool hasColor[MAX_RIVER_NODES] = { 0 };
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
            rivers[i].flow = 1.0f;  // Closed doors block at the junction, not at the source
        }
        else if (inFlow[i] > 0.0f)
        {
            if (riversMerged || !hasMain[i])
            {
                // Mix everything that flowed in (primary pairs -> secondary)
                rivers[i].color = mixColor[i];
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
            // A closed door dams its node: the water upstream stays put and
            // contributes nothing downstream
            bool dammed = NodeDammed(i);

            if (!dammed)
            {
                if (rivers[i].flow > 0.0f)
                {
                    mixColor[d] = hasColor[d]? MixWaterColors(mixColor[d], rivers[i].color) : rivers[i].color;
                    hasColor[d] = true;
                }
                inFlow[d] += rivers[i].flow;
            }
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

    Color mixed = { 0 };
    bool hasColor = false;
    Color mainCol = rivers[i].color;
    bool hasMain = false;

    for (int j = 0; j < riverCount; j++)
    {
        if ((rivers[j].downstream == i) && (rivers[j].flow > 0.0f) && (rivers[j].sampleCount > 0))
        {
            // Dammed nodes contribute nothing until their door opens
            if (NodeDammed(j)) continue;

            Color c = rivers[j].samples[rivers[j].sampleCount - 1];     // Water arriving now
            if (c.a == 0) continue;    // Channel is still refilling: nothing has arrived yet
            mixed = hasColor? MixWaterColors(mixed, c) : c;
            hasColor = true;
            if (rivers[j].isMain)
            {
                mainCol = c;
                hasMain = true;
            }
        }
    }

    if (!hasColor) return rivers[i].color;
    if (!riversMerged && hasMain) return mainCol;   // Side inflows don't tint the main river yet
    return mixed;
}

// True if every RGB channel of a is within tol of b (mixing rounds channels,
// so an exact == comparison would be off by one and never match)
static bool ColorNear(Color a, Color b, int tol)
{
    return (abs(a.r - b.r) <= tol) && (abs(a.g - b.g) <= tol) && (abs(a.b - b.b) <= tol);
}

// Latch robotHappy once water of the wanted color physically arrives at the
// river mouth (checked against the advected samples, so the face changes only
// when the merged color front reaches the robot, not when R is pressed)
static void UpdateRobotMood(void)
{
    if (robotHappy || !riversMerged) return;
    if (ColorNear(RiverNodeOutputColor(riverMouthIndex), robotWantedColor, 10))
    {
        robotHappy = true;
        PlaySound(goalSound);
    }
}

// T: dip the wand into the water below the witch, charging it with that
// water's pigment. Works anywhere over water, gates nearby or not
static void DipWand(void)
{
    Color w = WaterColorAt(mainPlayerPosition);
    if (w.a != 0)
    {
        // The crystal flashes when it picks up a NEW pigment
        if (!wandCharged || !SameColor(w, wandColor)) {
            wandFlashTimer = wandFlashTime;
            PlaySound(wandSound);
        }
        wandColor = w;
        wandCharged = true;
    }
}

// R: cast at the nearest closed gate in range, honoring its color lock
static void StartSpell(void)
{
    if (spellState != GATE_CLOSED) return;

    int targetDoor = -1;
    float targetDist = gateCastRadius;
    for (int i = 0; i < doorCount; i++)
    {
        if (doors[i].open) continue;
        float dist = Vector2Distance(mainPlayerPosition, DoorCenter(&doors[i]));
        if (dist <= targetDist)
        {
            targetDist = dist;
            targetDoor = i;
        }
    }

    if (targetDoor < 0) return;

    Door *door = &doors[targetDoor];
    if (!WandMatchesDoor(door))
    {
        // Locked: the spell fizzles against the gate's rune
        fizzleTimer = fizzleTime;
        fizzlePos = DoorCenter(door);
        return;
    }

    spellTargetDoor = targetDoor;
    spellState = SPELL_FLYING;
    spellPos = mainPlayerPosition;
    PlaySound(castSound);
}

// Advance the spell: spark flies to the gate, poofs, then the frog appears and
// the rivers actually merge (this is where the old R-key merge code moved to)
static void UpdateSpell(float dt)
{
    if (fizzleTimer > 0.0f) fizzleTimer -= dt;
    if (wandFlashTimer > 0.0f) wandFlashTimer -= dt;

    if ((spellTargetDoor < 0) || (spellTargetDoor >= doorCount)) return;
    Vector2 gatePosition = DoorCenter(&doors[spellTargetDoor]);

    if (spellState == SPELL_FLYING)
    {
        Vector2 toGate = Vector2Subtract(gatePosition, spellPos);
        float dist = Vector2Length(toGate);
        float step = spellSpeed*dt;
        if (step >= dist)
        {
            spellPos = gatePosition;
            spellState = GATE_TRANSFORM;
            spellTimer = 0.0f;
        }
        else spellPos = Vector2Add(spellPos, Vector2Scale(toGate, step/dist));
    }
    else if (spellState == GATE_TRANSFORM)
    {
        spellTimer += dt;
        if (spellTimer >= transformTime)
        {
            doors[spellTargetDoor].open = true;
            doors[spellTargetDoor].frogged = true;
            riversMerged = true;
            PropagateRiverColors();
            spellState = GATE_CLOSED;   // Idle again: the witch can cast at the next gate
            spellTargetDoor = -1;
        }
    }
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
            if (rivers[i].sampleCount <= 0) continue;
            if (NodeDammed(i)) continue;    // Standing water behind a closed gate: frozen

            for (int k = rivers[i].sampleCount - 1; k > 0; k--) rivers[i].samples[k] = rivers[i].samples[k - 1];

            // A dye basin blends everything flowing through it with its base
            // color: yellow into a blue pool brews green, dammed or not
            Color inj = out[i];
            if ((inj.a != 0) && rivers[i].isPool) inj = MixWaterColors(inj, rivers[i].standingColor);
            rivers[i].samples[0] = inj;
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

// Distance from a point to the nearest river centerline segment. Used as the
// witch's movement bound: she may only fly where this stays within the channel
static float RiverDistance(Vector2 p)
{
    float best = 1e9f;
    for (int i = 0; i < riverCount; i++)
    {
        int ds = rivers[i].downstream;
        if (ds < 0) continue;
        float t = 0.0f;
        float d = SegmentDistance(p, rivers[i].position, rivers[ds].position, &t);
        if (d < best) best = d;
    }
    return best;
}

// Water color under a point: the nearest segment's advected sample. BLANK when
// the point is not over water, or the channel there is dry
static Color WaterColorAt(Vector2 p)
{
    float best = 1e9f, bestT = 0.0f;
    int bestSeg = -1;
    for (int i = 0; i < riverCount; i++)
    {
        int ds = rivers[i].downstream;
        if (ds < 0) continue;
        float t = 0.0f;
        float d = SegmentDistance(p, rivers[i].position, rivers[ds].position, &t);
        if (d < best) { best = d; bestT = t; bestSeg = i; }
    }
    if ((bestSeg < 0) || (best > channelHalfWidth)) return BLANK;
    if (rivers[bestSeg].sampleCount <= 0) return BLANK;

    int k = (int)(bestT*(rivers[bestSeg].sampleCount - 1));
    return rivers[bestSeg].samples[k];  // Dry channels read as BLANK naturally
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

// Does the wand pigment satisfy a door's color lock? Colorless doors (alpha 0)
// accept any spell. The comparison is tolerant rather than exact: mixing
// rounds color channels, and the lock should also accept the lightened
// display shade of its color in case a dip ever captures that variant
static bool WandMatchesDoor(const Door *door)
{
    if (door->requiresWandColor.a == 0) return true;
    if (!wandCharged) return false;
    return ColorNear(wandColor, door->requiresWandColor, 12) ||
           ColorNear(wandColor, WaterLight(door->requiresWandColor), 12);
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
                    // Decide whose water this pixel shows. Inside the main channel
                    // main water always wins: side rivers END at the main channel's
                    // bank. At the junction the hand-off
                    // between upstream and merged water follows an arc, like the
                    // entering water being swept upward by the main flow
                    int waterSeg = bestSeg;
                    float waterT = bestT;
                    float mainX = (float)screenWidth/2;

                    if ((fabsf(p.x - mainX) <= channelHalfWidth) && (junctionCount > 0))
                    {
                        int nearestJunction = 0;
                        float nearest = fabsf(p.y - junctions[0].position.y);
                        for (int j = 1; j < junctionCount; j++)
                        {
                            float dj = fabsf(p.y - junctions[j].position.y);
                            if (dj < nearest)
                            {
                                nearest = dj;
                                nearestJunction = j;
                            }
                        }

                        JunctionRender *junction = &junctions[nearestJunction];

                        // Curved hand-off boundary: one quarter-circle arc per mouth.
                        // Several side rivers may share one junction (same y), so take
                        // the highest boundary among the co-located mouths - each
                        // entering river sweeps upward from its own bank
                        float arcR = 2.0f*channelHalfWidth;
                        float boundaryY = junction->position.y + channelHalfWidth - arcR;
                        for (int j = 0; j < junctionCount; j++)
                        {
                            if (fabsf(junctions[j].position.y - junction->position.y) > 1.0f) continue;
                            float sideBankX = junctions[j].position.x + junctions[j].sideDir*channelHalfWidth;
                            float ax = (p.x - sideBankX)*-junctions[j].sideDir;
                            float bY = junctions[j].position.y + channelHalfWidth - arcR;
                            if ((ax >= 0.0f) && (ax < arcR)) bY += sqrtf(arcR*arcR - ax*ax);
                            if (bY > boundaryY) boundaryY = bY;
                        }

                        // Ordered-dither blend across a band instead of a hard edge
                        float blend = (boundaryY - p.y)/24.0f + 0.5f;   // 1 = merged side, 0 = upstream
                        waterSeg = (blend > bayer4[py & 3][px & 3])? junction->upperMainSegment : junction->lowerMainSegment;
                        SegmentDistance(p, rivers[waterSeg].position,
                                        rivers[rivers[waterSeg].downstream].position, &waterT);
                    }

                    RiverNode *wseg = &rivers[waterSeg];
                    Vector2 wa = wseg->position;
                    Vector2 wb = rivers[wseg->downstream].position;
                    float wLen = sqrtf((wb.x - wa.x)*(wb.x - wa.x) + (wb.y - wa.y)*(wb.y - wa.y));
                    float wAlong = waterT*wLen;

                    // Water color comes from the advected samples along the segment
                    int count = wseg->sampleCount;
                    Color w = wseg->color;
                    if (count > 1)
                    {
                        // Ordered dither spreads each sample boundary over ~2 samples,
                        // so the traveling color front has a soft blended edge
                        float kf = waterT*(count - 1) + bayer4[py & 3][px & 3]*2.0f - 0.5f;
                        int k = (int)kf;
                        if (k < 0) k = 0;
                        if (k > count - 1) k = count - 1;
                        w = wseg->samples[k];
                    }

                    if ((wseg->flow <= 0.0f) || (w.a == 0))
                    {
                        // Dry bed: no flow, or the water refilling this channel
                        // hasn't reached this stretch yet (alpha 0 samples)
                        out = (d > channelHalfWidth - 9.0f)? metalRamp[1] : metalRamp[2];
                    }
                    else
                    {
                        // Flow streaks moving downstream
                        float sp = fmodf(wAlong - time*RIVER_FLOW_SPEED, 90.0f);
                        if (sp < 0.0f) sp += 90.0f;
                        if ((sp < 15.0f) && (d < channelHalfWidth - 16.0f) && (((px*7 + py*3)%5) < 3)) w = WaterLight(w);
                        if (d > channelHalfWidth - 9.0f) w = WaterDark(w);  // Waterline edge
                        out = w;
                    }
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
        case 'X':   // Wand crystal tip: shows the dipped pigment when charged.
                    // Drawn one shade LIGHTER than the river water so it stays
                    // visible while flying over water of the same color
            if (wandFlashTimer > 0.0f) return WaterLight(WaterLight(wandCharged? wandColor : (Color){ 208, 214, 224, 255 }));
            if (wandCharged) return WaterLight(wandColor);
            return (Color){ 208, 214, 224, 255 };       // Inert pale crystal
        case 'K': return (Color){ 42, 32, 48, 255 };    // Near-black (boots, robot face)
        case 'D': return (Color){ 57, 64, 74, 255 };    // Dark metal outline
        case 'M': return (Color){ 152, 161, 173, 255 }; // Metal
        case 'L': return (Color){ 201, 209, 218, 255 }; // Screen light
        case 'G': return (Color){ 168, 178, 192, 255 }; // Screen shade
        case 'P': return (Color){ 95, 180, 86, 255 };   // Frog green
        case 'Q': return (Color){ 178, 222, 146, 255 }; // Frog belly
        case 'C': return robotWantedColor;              // Thought-bubble river water
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

    DrawSprite(robotHappy? robotSpriteHappy : robotSprite, ROBOT_W, ROBOT_H, pos, false, false);

    // Thought bubble on the right showing the river color he wants,
    // floating with the same bob (slightly out of phase). Gone once happy
    if (!robotHappy)
    {
        Vector2 bubblePos = { robotPosition.x + 216, robotPosition.y - 48 + sinf(t*1.6f + 1.0f)*RIVER_PIXEL };
        DrawSprite(bubbleSprite, BUBBLE_W, BUBBLE_H, bubblePos, false, false);

        // The snippet flows like a real river: same waterline + moving streaks
        // treatment as RenderRiverPixels, applied to the bubble's 'C' cells
        int left = (int)roundf(bubblePos.x/RIVER_PIXEL) - BUBBLE_W/2;
        int top = (int)roundf(bubblePos.y/RIVER_PIXEL) - BUBBLE_H/2;
        for (int y = 6; y <= 8; y++)
        {
            for (int x = 4; x <= 18; x++)
            {
                Color w = robotWantedColor;
                if ((y == 6) || (y == 8)) w = WaterDark(w);     // Waterline edges
                else
                {
                    // Streaks drift toward the robot, same speed as real water
                    float sp = fmodf(x*RIVER_PIXEL + t*RIVER_FLOW_SPEED, 90.0f);
                    if ((sp < 24.0f) && (((x*7 + y*3)%5) < 3)) w = WaterLight(w);
                }
                DrawRectangle((int)((left + x)*RIVER_PIXEL), (int)((top + y)*RIVER_PIXEL),
                              (int)RIVER_PIXEL, (int)RIVER_PIXEL, w);
            }
        }

        // Mini bubbles leading from the robot up to the thought bubble,
        // each bobbing on its own phase
        Vector2 miniBig = { robotPosition.x + 168, robotPosition.y - 6 + sinf(t*1.6f + 2.2f)*RIVER_PIXEL };
        Vector2 miniSmall = { robotPosition.x + 132, robotPosition.y + 24 + sinf(t*1.6f + 3.4f)*RIVER_PIXEL };
        DrawSprite(miniBubbleBig, 4, 4, miniBig, false, false);
        DrawSprite(miniBubbleSmall, 3, 3, miniSmall, false, false);
    }

    // Blink: overdraw the 3x3 antenna ball (sprite cells x30..32, y0..2) brighter.
    // Blinks faster when happy
    if (((int)(t*(robotHappy? 6.0f : 2.0f)))%2 == 0)
    {
        int left = (int)roundf(pos.x/RIVER_PIXEL) - ROBOT_W/2;
        int top = (int)roundf(pos.y/RIVER_PIXEL) - ROBOT_H/2;
        DrawRectangle((int)((left + 30)*RIVER_PIXEL), (int)(top*RIVER_PIXEL),
                      (int)(3*RIVER_PIXEL), (int)(3*RIVER_PIXEL), (Color){ 240, 224, 138, 255 });
    }
}

//--------------------------------------------------------------------------------------------
// Title screen
//--------------------------------------------------------------------------------------------
static void UpdateDrawTitle(void)
{
    float t = (float)GetTime();

    // Typewriter progress; total character count of the story
    int totalChars = 0;
    for (int i = 0; i < STORY_LINE_COUNT; i++) totalChars += (int)strlen(storyLines[i]);
    storyCharsShown += GetFrameTime()*storyCharsPerSecond;
    bool storyDone = (storyCharsShown >= totalChars);

    // ENTER starts the game once the story is out; any key first fast-forwards it
    if (IsKeyPressed(KEY_ENTER) && storyDone)
    {
        currentScreen = SCREEN_GAMEPLAY;
        StopMusicStream(themeMusic);
        PlayMusicStream(backgroundMusic);
        return;
    }
    if ((GetKeyPressed() != 0) && !storyDone) storyCharsShown = (float)totalChars;

    // raylib logo animation: classic four states at half scale
    logoFrames++;
    if (logoState == 0)
    {
        if (logoFrames >= 60) { logoState = 1; logoFrames = 0; }
    }
    else if (logoState == 1)
    {
        logoTopW += 2; logoLeftH += 2;
        if (logoTopW >= LOGO_SIZE) { logoState = 2; logoFrames = 0; }
    }
    else if (logoState == 2)
    {
        logoBottomW += 2; logoRightH += 2;
        if (logoBottomW >= LOGO_SIZE) { logoState = 3; logoFrames = 0; }
    }
    else if ((logoLetters < 6) && (logoFrames%10 == 0)) logoLetters++;

    BeginDrawing();
        ClearBackground((Color){ 24, 26, 31, 255 });

        // Title
        const char *title = "SAVE THE SHINJI";
        DrawText(title, (screenWidth - MeasureText(title, 44))/2, 64, 44, RAYWHITE);

        // Sad Shinji, bobbing gently
        Vector2 robotPos = { (float)screenWidth/2, 216 + sinf(t*1.6f)*RIVER_PIXEL };
        DrawSprite(robotSpriteSad, ROBOT_W, ROBOT_H, robotPos, false, false);

        // Story, revealed character by character. Each line is centered at its
        // final position so the text doesn't shift while typing
        int budget = (int)storyCharsShown;
        int y = 370;
        for (int i = 0; i < STORY_LINE_COUNT; i++)
        {
            int len = (int)strlen(storyLines[i]);
            int show = (budget < len)? budget : len;
            budget -= show;
            if (show > 0)
            {
                int x = (screenWidth - MeasureText(storyLines[i], 20))/2;
                DrawText(TextSubtext(storyLines[i], 0, show), x, y, 20, (Color){ 201, 209, 218, 255 });
            }
            y += (len == 0)? 14 : 28;
        }

        // Blinking prompt once the story is fully out
        if (storyDone && (((int)(t*2.0f))%2 == 0))
        {
            const char *prompt = "- Press ENTER -";
            DrawText(prompt, (screenWidth - MeasureText(prompt, 24))/2, 620, 24, YELLOW);
        }

        // raylib logo badge, bottom-right
        int lx = screenWidth - LOGO_SIZE - 24;
        int ly = screenHeight - LOGO_SIZE - 24;
        if (logoState == 0)
        {
            if ((logoFrames/10)%2 == 0)
                DrawRectangle(lx, ly, LOGO_BORDER*2, LOGO_BORDER*2, RAYWHITE);
        }
        else
        {
            DrawRectangle(lx, ly, logoTopW, LOGO_BORDER, RAYWHITE);
            DrawRectangle(lx, ly, LOGO_BORDER, logoLeftH, RAYWHITE);
            if (logoState >= 2)
            {
                DrawRectangle(lx + LOGO_SIZE - LOGO_BORDER, ly, LOGO_BORDER, logoRightH, RAYWHITE);
                DrawRectangle(lx, ly + LOGO_SIZE - LOGO_BORDER, logoBottomW, LOGO_BORDER, RAYWHITE);
            }
            if (logoState >= 3)
                DrawText(TextSubtext("raylib", 0, logoLetters), lx + 42, ly + 88, 25, RAYWHITE);
        }

    EndDrawing();
}

#include "restart_spinner.inc"

// Draw a gate with its bars in the given color: metal for plain gates, the lock
// color for color-locked ones (the whole door shows what wand it wants)
static void DrawGate(Vector2 worldPos, Color barColor)
{
    int left = (int)roundf(worldPos.x/RIVER_PIXEL) - GATE_W/2;
    int top = (int)roundf(worldPos.y/RIVER_PIXEL) - GATE_H/2;

    for (int y = 0; y < GATE_H; y++)
    {
        for (int x = 0; x < GATE_W; x++)
        {
            char c = gateSprite[y][x];
            if (c == '.') continue;
            Color col = (c == 'M')? barColor : SpriteColor(c);
            DrawRectangle((int)((left + x)*RIVER_PIXEL), (int)((top + y)*RIVER_PIXEL),
                          (int)RIVER_PIXEL, (int)RIVER_PIXEL, col);
        }
    }
}

// Color a gate's bars should show: its lock color, or plain metal when unlocked
static Color GateBarColor(const Door *door)
{
    return (door->requiresWandColor.a != 0)? door->requiresWandColor : SpriteColor('M');
}

// Gates / spark / poof / frogs, drawn on the same low-res pixel grid as the scene
static void DrawSpellScene(void)
{
    float t = (float)GetTime();

    // Every closed door shows its gate; every spell-opened door shows its frog
    for (int i = 0; i < doorCount; i++)
    {
        Vector2 doorPosition = DoorCenter(&doors[i]);

        if (!doors[i].open)
        {
            // The transform effect draws the target gate itself (lingering half-poof)
            if ((spellState == GATE_TRANSFORM) && (i == spellTargetDoor)) continue;

            // Color-locked gates are painted entirely in their lock color
            DrawGate(doorPosition, GateBarColor(&doors[i]));

            // Cast-range hint: pulsing ring while the witch is close enough. Gray
            // when the gate's lock does not match the wand
            if ((spellState == GATE_CLOSED) &&
                (Vector2Distance(mainPlayerPosition, doorPosition) <= gateCastRadius))
            {
                bool wandFits = WandMatchesDoor(&doors[i]);
                DrawCircleLines((int)doorPosition.x, (int)doorPosition.y,
                                gateCastRadius + sinf(t*6.0f)*3.0f,
                                wandFits? YELLOW : (Color){ 130, 130, 140, 255 });
            }
        }
        else if (doors[i].frogged)
        {
            const char **frame = (((int)(t*2.0f))%2 == 0)? frogSprite1 : frogSprite2;
            DrawSprite(frame, FROG_W, FROG_H, doorPosition, false, false);
        }
    }

    // Fizzle: a fading gray puff where a locked gate rejected the spell
    if (fizzleTimer > 0.0f)
    {
        unsigned char a = (unsigned char)(255*fizzleTimer/fizzleTime);
        Color gray = { 120, 120, 130, a };
        DrawRectangle((int)(fizzlePos.x - 9), (int)(fizzlePos.y - 9), 18, 18, gray);
        DrawRectangle((int)(fizzlePos.x - 21), (int)(fizzlePos.y - 3), 6, 6, gray);
        DrawRectangle((int)(fizzlePos.x + 15), (int)(fizzlePos.y - 3), 6, 6, gray);
    }

    if ((spellTargetDoor < 0) || (spellTargetDoor >= doorCount)) return;
    Vector2 gatePosition = DoorCenter(&doors[spellTargetDoor]);

    if (spellState == SPELL_FLYING)
    {
        // Spark: bright core with cross arms in the wand's color (gold by default)
        float sx = roundf(spellPos.x/RIVER_PIXEL)*RIVER_PIXEL;
        float sy = roundf(spellPos.y/RIVER_PIXEL)*RIVER_PIXEL;
        Color core = { 255, 244, 180, 255 };
        Color gold = wandCharged? wandColor : (Color){ 217, 168, 60, 255 };
        DrawRectangle((int)(sx - RIVER_PIXEL), (int)(sy - RIVER_PIXEL),
                      (int)(2*RIVER_PIXEL), (int)(2*RIVER_PIXEL), core);
        DrawRectangle((int)(sx - 2*RIVER_PIXEL), (int)(sy - RIVER_PIXEL/2), (int)RIVER_PIXEL, (int)RIVER_PIXEL, gold);
        DrawRectangle((int)(sx + RIVER_PIXEL), (int)(sy - RIVER_PIXEL/2), (int)RIVER_PIXEL, (int)RIVER_PIXEL, gold);
        DrawRectangle((int)(sx - RIVER_PIXEL/2), (int)(sy - 2*RIVER_PIXEL), (int)RIVER_PIXEL, (int)RIVER_PIXEL, gold);
        DrawRectangle((int)(sx - RIVER_PIXEL/2), (int)(sy + RIVER_PIXEL), (int)RIVER_PIXEL, (int)RIVER_PIXEL, gold);
    }
    else if (spellState == GATE_TRANSFORM)
    {
        // Poof: gate lingers for the first half, white flash expands and fades
        if (spellTimer < transformTime*0.5f) DrawGate(gatePosition, GateBarColor(&doors[spellTargetDoor]));

        float f = spellTimer/transformTime;     // 0 -> 1
        unsigned char a = (unsigned char)(255*(1.0f - f));
        float spread = 12.0f + f*30.0f;
        DrawRectangle((int)(gatePosition.x - 12), (int)(gatePosition.y - 12), 24, 24, (Color){ 255, 255, 255, a });
        DrawRectangle((int)(gatePosition.x - spread - 4), (int)(gatePosition.y - 4), 8, 8, (Color){ 255, 244, 180, a });
        DrawRectangle((int)(gatePosition.x + spread - 4), (int)(gatePosition.y - 4), 8, 8, (Color){ 255, 244, 180, a });
        DrawRectangle((int)(gatePosition.x - 4), (int)(gatePosition.y - spread - 4), 8, 8, (Color){ 255, 244, 180, a });
        DrawRectangle((int)(gatePosition.x - 4), (int)(gatePosition.y + spread - 4), 8, 8, (Color){ 255, 244, 180, a });
    }
}
