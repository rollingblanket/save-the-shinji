#ifndef RIVER_COLORS_H
#define RIVER_COLORS_H

#include "raylib.h"

// Water palette: three primaries, the secondary each pair mixes into, and
// tertiary colors for primary + adjacent secondary mixes.
//
// Each color exists twice: as a brace macro and as a const variable. The
// macros exist because a static const Color is not a compile-time constant
// in C, so static level data (levels.inc) can only be initialized from the
// brace form. Code should keep using the variables.
#define RIVER_BLUE       { 47, 111, 208, 255 }
#define RIVER_RED        { 192, 58, 43, 255 }
#define RIVER_YELLOW     { 232, 180, 40, 255 }
#define RIVER_PURPLE     { 136, 60, 184, 255 } // Blue + Red
#define RIVER_ORANGE     { 230, 126, 34, 255 } // Red + Yellow
#define RIVER_GREEN      { 62, 168, 82, 255 } // Yellow + Blue
#define RIVER_VERMILION  { 0xD3, 0x5C, 0x26, 0xFF } // Red + Orange
#define RIVER_AMBER      { 0xE7, 0x99, 0x25, 0xFF } // Yellow + Orange
#define RIVER_CHARTREUSE { 0x93, 0xAE, 0x3D, 0xFF } // Yellow + Green
#define RIVER_TEAL       { 0x36, 0x8C, 0x91, 0xFF } // Blue + Green
#define RIVER_VIOLET     { 0x5C, 0x56, 0xC4, 0xFF } // Blue + Purple
#define RIVER_MAGENTA    { 0xA4, 0x3B, 0x72, 0xFF } // Red + Purple

static const Color riverBlue = RIVER_BLUE;
static const Color riverRed = RIVER_RED;
static const Color riverYellow = RIVER_YELLOW;
static const Color riverPurple = RIVER_PURPLE;          // red + blue
static const Color riverOrange = RIVER_ORANGE;          // red + yellow
static const Color riverGreen = RIVER_GREEN;            // blue + yellow
static const Color riverVermilion = RIVER_VERMILION;    // red + orange (#D35C26)
static const Color riverAmber = RIVER_AMBER;            // yellow + orange (#E79925)
static const Color riverChartreuse = RIVER_CHARTREUSE;  // yellow + green (#93AE3D)
static const Color riverTeal = RIVER_TEAL;              // blue + green (#368C91)
static const Color riverViolet = RIVER_VIOLET;          // blue + purple (#5C56C4)
static const Color riverMagenta = RIVER_MAGENTA;        // red + purple (#A43B72)

#endif
