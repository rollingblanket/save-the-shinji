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

// Level 4, the two-stage brew. Same shape as the mixing chamber but the yellow
// river joins LOWER down the main river, and there are no pools: the green the
// lock wants does not exist on the map yet. Open the plain yellow gate so
// yellow merges into the blue main = green flows upstream of that junction.
// Dip the green from the main river itself (T), unlock the green-locked BLUE
// river on the left, and its blue pours into the now-green main:
// green + blue = teal, which is what the robot wants
static const SideRiverDef level3SideRivers[] = {
    { .source = { 0, 290 }, .sourceColor = RIVER_BLUE, .junctionY = 290.0f },
    { .source = { 720, 460 }, .sourceColor = RIVER_YELLOW, .junctionY = 460.0f }
};

static const DoorDef level3Doors[] = {
    { .sideRiverIndex = 0, .requiresWandColor = RIVER_GREEN },  // blue river, green lock
    { .sideRiverIndex = 1 }                                     // yellow river, plain gate
};

// Level 5, the alchemy lab. Two chambers chained by locks. Right-lower: the
// familiar green chamber (yellow + blue pool). Its green unlocks the LEFT
// chamber's outer door, letting red brew purple in a second blue pool. Dipping
// purple (overwriting green!) opens the purple-locked inner door, pouring purple
// into the main river: blue + purple = violet. The right chamber's unlocked
// inner door is still the trap: spilling green downstream ruins everything
static const SideRiverDef level4SideRivers[] = {
    { .source = { 0, 250 }, .sourceColor = RIVER_RED, .junctionY = 250.0f },
    { .source = { 720, 470 }, .sourceColor = RIVER_YELLOW, .junctionY = 470.0f }
};

static const DoorDef level4Doors[] = {
    { .sideRiverIndex = 0, .requiresWandColor = RIVER_PURPLE },                                     // left inner: the finale
    { .sideRiverIndex = 0, .distFromMouth = 200.0f, .requiresWandColor = RIVER_GREEN, .fill = RIVER_BLUE }, // left outer: purple brewery
    { .sideRiverIndex = 1 },                                                                        // right inner: the trap
    { .sideRiverIndex = 1, .distFromMouth = 200.0f, .fill = RIVER_BLUE }                            // right outer: green brewery
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
    },
    {
        // Level 4: the two-stage brew (teal)
        .sideRivers = level3SideRivers,
        .sideRiverCount = 2,
        .buttons = NULL,
        .buttonCount = 0,
        .doors = level3Doors,
        .doorCount = 2,
        .playerSpawn = { (float)screenWidth/2, (float)screenHeight - (float)screenHeight/4 },
        .robotWantedColor = RIVER_TEAL
    },
    {
        // Level 5: the alchemy lab (violet)
        .sideRivers = level4SideRivers,
        .sideRiverCount = 2,
        .buttons = NULL,
        .buttonCount = 0,
        .doors = level4Doors,
        .doorCount = 4,
        .playerSpawn = { (float)screenWidth/2, (float)screenHeight - (float)screenHeight/4 },
        .robotWantedColor = RIVER_VIOLET
    },
};

static const int levelCount = sizeof(levelDefs)/sizeof(levelDefs[0]);
