// Level data lives here so designing levels does not require digging through
// gameplay, rendering, and river simulation code.

#include "river_colors.h"

static const SideRiverDef level0SideRivers[] = {
    { .source = { 0, 375 }, .sourceColor = RIVER_RED, .junctionY = 375.0f }
};

static const DoorDef level0Doors[] = {
    { .sideRiverIndex = 0, .open = false }
};

static const SideRiverDef level1SideRivers[] = {
    { .source = { 0, 375 }, .sourceColor = RIVER_YELLOW, .junctionY = 375.0f },
    { .source = { 800, 375 }, .sourceColor = RIVER_RED, .junctionY = 375.0f }

};

static const DoorDef level1Doors[] = {
    { .sideRiverIndex = 0, .open = false },
    { .sideRiverIndex = 1, .open = false }
};

// Level 3, the mixing chamber. Right river: yellow dammed behind the OUTER door,
// a blue pool sealed between the two doors. Frog the outer door and yellow pours
// into the pool, brewing green (contained while the inner door stays shut). Dip
// the wand in the green (R over water), unlock the green-locked red gate on the
// left, and red + blue = purple. Opening the inner door spills the pool into the
// main river = fail, hold SPACE to retry
static const SideRiverDef level2SideRivers[] = {
    { .source = { 0, 290 }, .sourceColor = RIVER_RED, .junctionY = 290.0f },
    { .source = { 720, 290 }, .sourceColor = RIVER_YELLOW, .junctionY = 290.0f }
};

static const DoorDef level2Doors[] = {
    { .sideRiverIndex = 0, .requiresWandColor = RIVER_GREEN },              // red gate, green lock
    { .sideRiverIndex = 1 },                                                // inner door: the trap
    { .sideRiverIndex = 1, .distFromMouth = 170.0f, .fill = RIVER_BLUE }    // outer door, blue pool below
};

static const LevelDef levelDefs[] = {
    {
        .sideRivers = level0SideRivers,
        .sideRiverCount = 1,
        .buttons = NULL,
        .buttonCount = 0,
        .doors = level0Doors,
        .doorCount = 1,
        .playerSpawn = { (float)screenWidth/2, (float)screenHeight - (float)screenHeight/4 },
        .robotWantedColor = { 136, 60, 184, 255 }
    },
    {
        .sideRivers = level1SideRivers,
        .sideRiverCount = 2,
        .buttons = NULL,
        .buttonCount = 0,
        .doors = level1Doors,
        .doorCount = 2,
        .playerSpawn = { (float)screenWidth/2, (float)screenHeight - (float)screenHeight/4 },
        .robotWantedColor = { 136, 60, 184, 255 }
    },
    {
        .sideRivers = level2SideRivers,
        .sideRiverCount = 2,
        .buttons = NULL,
        .buttonCount = 0,
        .doors = level2Doors,
        .doorCount = 3,
        .playerSpawn = { (float)screenWidth/2, (float)screenHeight - (float)screenHeight/4 },
        .robotWantedColor = RIVER_PURPLE
    }

};

static const int levelCount = sizeof(levelDefs)/sizeof(levelDefs[0]);
