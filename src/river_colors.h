#ifndef RIVER_COLORS_H
#define RIVER_COLORS_H

#include "raylib.h"

// Water palette: three primaries, the secondary each pair mixes into, and
// tertiary colors for primary + adjacent secondary mixes.
static const Color riverBlue = { 47, 111, 208, 255 };
static const Color riverRed = { 192, 58, 43, 255 };
static const Color riverYellow = { 232, 180, 40, 255 };
static const Color riverPurple = { 136, 60, 184, 255 };  // red + blue
static const Color riverOrange = { 230, 126, 34, 255 };  // red + yellow
static const Color riverGreen = { 62, 168, 82, 255 };    // blue + yellow
static const Color riverVermilion = { 0xD3, 0x5C, 0x26, 0xFF };  // red + orange (#D35C26)
static const Color riverAmber = { 0xE7, 0x99, 0x25, 0xFF };      // yellow + orange (#E79925)
static const Color riverChartreuse = { 0x93, 0xAE, 0x3D, 0xFF }; // yellow + green (#93AE3D)
static const Color riverTeal = { 0x36, 0x8C, 0x91, 0xFF };       // blue + green (#368C91)
static const Color riverViolet = { 0x5C, 0x56, 0xC4, 0xFF };     // blue + purple (#5C56C4)
static const Color riverMagenta = { 0xA4, 0x3B, 0x72, 0xFF };    // red + purple (#A43B72)

#endif
