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
static const SideRiverDef level3SideRivers[] = {
    { .source = { 0, 290 }, .sourceColor = RIVER_RED, .junctionY = 290.0f },
    { .source = { 720, 290 }, .sourceColor = RIVER_YELLOW, .junctionY = 290.0f }
};

static const DoorDef level3Doors[] = {
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
static const SideRiverDef level2SideRivers[] = {
    { .source = { 0, 290 }, .sourceColor = RIVER_BLUE, .junctionY = 290.0f },
    { .source = { 720, 460 }, .sourceColor = RIVER_YELLOW, .junctionY = 460.0f }
};

static const DoorDef level2Doors[] = {
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



// Level 6, the drought. The main river is YELLOW this time, and 3 seconds in
// its source dries up: a dry front creeps up the channel toward the robot.
// The red side river has a plain gate; the robot wants orange (red + yellow).
// Frog the gate while yellow still flows past the junction and the brew rides
// the last of the water up to the robot. Hesitate, and the junction runs dry:
// pure red arrives instead. Hold SPACE to retry
static const SideRiverDef level5SideRivers[] = {
    { .source = { 0, 375 }, .sourceColor = RIVER_RED, .junctionY = 375.0f }
};

static const DoorDef level5Doors[] = {
    { .sideRiverIndex = 0 }     // Plain gate: the challenge is the clock, not the lock
};

// Level 7, Shinji's body. The spine starts TRANSPARENT (mainDry): the map is a
// body, limbs are rivers, joints are doors. Left arm: blue behind a green door
// (the spine's only clean blue). Right arm: blue through a RED pool = purple key.
// Left leg: red through a YELLOW pool = orange key (purple lock). Right leg:
// yellow through a BLUE pool = green (orange lock at the mouth). Brew purple ->
// orange -> green, dip green to open the left arm (blue fills the spine), re-dip
// orange, release the right leg's green: blue + green = cyan rises into the head
static const SideRiverDef level6SideRivers[] = {
    { .source = { 0, 300 }, .sourceColor = RIVER_BLUE, .junctionY = 300.0f },       // left arm
    { .source = { 720, 240 }, .sourceColor = RIVER_BLUE, .junctionY = 240.0f },     // right arm
    { .source = { 0, 560 }, .sourceColor = RIVER_RED, .junctionY = 560.0f },        // left leg
    { .source = { 720, 500 }, .sourceColor = RIVER_YELLOW, .junctionY = 500.0f }    // right leg
};

static const DoorDef level6Doors[] = {
    { .sideRiverIndex = 0, .requiresWandColor = RIVER_GREEN },                      // left arm: green door
    { .sideRiverIndex = 1 },                                                        // right arm: inner black door
    { .sideRiverIndex = 1, .distFromMouth = 170.0f, .fill = RIVER_RED },            // right arm: outer black door, red pool
    { .sideRiverIndex = 2 },                                                        // left leg: black door at the mouth
    { .sideRiverIndex = 2, .distFromMouth = 170.0f, .requiresWandColor = RIVER_PURPLE, .fill = RIVER_YELLOW }, // left leg: purple door, yellow pool
    { .sideRiverIndex = 3, .requiresWandColor = RIVER_ORANGE },                     // right leg: orange door at the mouth
    { .sideRiverIndex = 3, .distFromMouth = 170.0f, .fill = RIVER_BLUE }            // right leg: black door, blue pool
};

// Level 8, Shinji's body II. Transparent spine again, but every limb is a
// multi-stage refinery and every key is third-generation. Left leg: blue ->
// red pool = purple -> blue pool = VIOLET (its violet-locked mouth is the
// death-twin of the right leg's violet lock: same key, opposite fates). Right
// arm: red -> yellow pool = orange -> red pool = VERMILION (mouth stays shut!).
// Purple opens the arm, orange opens the left arm's outer door (blue -> yellow
// pool = green), vermilion opens its mouth: green fills the spine. Violet opens
// the right leg: yellow joins green = chartreuse in Shinji's head
static const SideRiverDef level7SideRivers[] = {
    { .source = { 0, 300 }, .sourceColor = RIVER_BLUE, .junctionY = 300.0f },       // left arm
    { .source = { 720, 240 }, .sourceColor = RIVER_RED, .junctionY = 240.0f },      // right arm
    { .source = { 0, 560 }, .sourceColor = RIVER_BLUE, .junctionY = 560.0f },       // left leg
    { .source = { 720, 500 }, .sourceColor = RIVER_YELLOW, .junctionY = 500.0f }    // right leg
};

static const DoorDef level7Doors[] = {
    // Left arm: spine <- [vermilion lock](yellow pool)[orange lock] <- blue source
    { .sideRiverIndex = 0, .requiresWandColor = RIVER_VERMILION },
    { .sideRiverIndex = 0, .distFromMouth = 150.0f, .requiresWandColor = RIVER_ORANGE, .fill = RIVER_YELLOW },
    // Right arm: spine <- [black](red pool)[black](yellow pool)[purple lock] <- red source
    { .sideRiverIndex = 1 },                                                        // mouth: the vermilion spill, trap
    { .sideRiverIndex = 1, .distFromMouth = 130.0f, .fill = RIVER_RED },
    { .sideRiverIndex = 1, .distFromMouth = 260.0f, .requiresWandColor = RIVER_PURPLE, .fill = RIVER_YELLOW },
    // Left leg: spine <- [violet lock](blue pool)[black](red pool)[black] <- blue source
    { .sideRiverIndex = 2, .requiresWandColor = RIVER_VIOLET },                     // mouth: the DEAD violet lock
    { .sideRiverIndex = 2, .distFromMouth = 130.0f, .fill = RIVER_BLUE },
    { .sideRiverIndex = 2, .distFromMouth = 260.0f, .fill = RIVER_RED },
    // Right leg: spine <- [violet lock] <- yellow source (the LIVE violet lock)
    { .sideRiverIndex = 3, .requiresWandColor = RIVER_VIOLET }
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
        // Level 4: the two-stage brew (teal)
        .sideRivers = level2SideRivers,
        .sideRiverCount = 2,
        .buttons = NULL,
        .buttonCount = 0,
        .doors = level2Doors,
        .doorCount = 2,
        .playerSpawn = { (float)screenWidth/2, (float)screenHeight - (float)screenHeight/4 },
        .robotWantedColor = RIVER_TEAL
    },
    {
        .sideRivers = level3SideRivers,
        .sideRiverCount = 2,
        .buttons = NULL,
        .buttonCount = 0,
        .doors = level3Doors,
        .doorCount = 3,
        .playerSpawn = { (float)screenWidth/2, (float)screenHeight - (float)screenHeight/4 },
        .robotWantedColor = RIVER_PURPLE
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
    {
        // Level 6: the drought (orange, timed)
        .sideRivers = level5SideRivers,
        .sideRiverCount = 1,
        .buttons = NULL,
        .buttonCount = 0,
        .doors = level5Doors,
        .doorCount = 1,
        .playerSpawn = { (float)screenWidth/2, (float)screenHeight - (float)screenHeight/4 },
        .robotWantedColor = RIVER_ORANGE,
        .mainColor = RIVER_YELLOW,
        .mainDrainDelay = 3.0f
    },
    {
        // Level 7: Shinji's body (cyan, transparent spine)
        .sideRivers = level6SideRivers,
        .sideRiverCount = 4,
        .buttons = NULL,
        .buttonCount = 0,
        .doors = level6Doors,
        .doorCount = 7,
        .playerSpawn = { (float)screenWidth/2, 660 },
        .robotWantedColor = RIVER_TEAL,
        .mainDry = true
    },
    {
        // Level 8: Shinji's body II (chartreuse, transparent spine)
        .sideRivers = level7SideRivers,
        .sideRiverCount = 4,
        .buttons = NULL,
        .buttonCount = 0,
        .doors = level7Doors,
        .doorCount = 9,
        .playerSpawn = { (float)screenWidth/2, 660 },
        .robotWantedColor = RIVER_CHARTREUSE,
        .mainDry = true
    },
};

static const int levelCount = sizeof(levelDefs)/sizeof(levelDefs[0]);
