// Level data lives here so designing levels does not require digging through
// gameplay, rendering, and river simulation code.

static const SideRiverDef level0SideRivers[] = {
    { .source = { 0, 375 }, .sourceColor = riverVermilion, .junctionY = 375.0f }
};

static const DoorDef level0Doors[] = {
    { .sideRiverIndex = 0, .open = false }
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
    }
};

static const int levelCount = sizeof(levelDefs)/sizeof(levelDefs[0]);
