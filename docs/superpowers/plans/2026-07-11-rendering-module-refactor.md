# Rendering Module Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract sprite, witch, robot, and restart-spinner rendering from `src/raylib_game.c` without changing gameplay or visuals.

**Architecture:** `raylib_game.c` remains the owner of gameplay state and passes explicit rendering inputs to focused C modules. A shared pixel-sprite module owns the ASCII bitmap loop and palette; character modules own their bitmaps and renderer-local animation state.

**Tech Stack:** C99, raylib 6.0, Make, CMake, Jujutsu (`jj`)

## Global Constraints

- Adapt commits `a03fccf5`, `768d5c3a`, and `74b4211f`; do not replay or merge them.
- Preserve the dynamic wand crystal, all three robot moods, thought-bubble animation, and restart-spinner appearance.
- Extracted modules must not read or mutate globals from `raylib_game.c`.
- Preserve unrelated working-copy changes, especially `src/river_colors.h`.
- Use `jj` for every version-control operation.

## File Map

- Create `src/pixel_sprite.c` and `src/pixel_sprite.h`: ASCII sprite palette and drawing loop.
- Create `src/witch.c` and `src/witch.h`: witch bitmap, facing state, and drawing.
- Create `src/robot.c` and `src/robot.h`: robot bitmaps, thought bubbles, and rendering animation.
- Replace `src/restart_spinner.inc` with `src/restart_spinner.c` and create `src/restart_spinner.h`: restart progress rendering.
- Modify `src/raylib_game.c`: consume the modules and remove extracted definitions.
- Modify `src/Makefile`: compile all new source files.
- `src/CMakeLists.txt` requires no edit because it already recursively discovers `.c` and `.h` files.

---

### Task 1: Extract the shared pixel-sprite renderer

**Files:**
- Create: `src/pixel_sprite.h`
- Create: `src/pixel_sprite.c`
- Modify: `src/raylib_game.c:10-12,526-547,1580-1633,1799-1860`
- Modify: `src/Makefile:35`

**Interfaces:**
- Produces: `Color PixelSpriteColor(char key, Color customColor)`
- Produces: `void DrawPixelSprite(const char **rows, int width, int height, Vector2 worldPosition, bool flipX, bool shadow, float pixelSize, Color customColor)`

- [ ] **Step 1: Add the public header and switch main-file consumers to the new API**

Create this header:

```c
#ifndef PIXEL_SPRITE_H
#define PIXEL_SPRITE_H

#include "raylib.h"

Color PixelSpriteColor(char key, Color customColor);
void DrawPixelSprite(const char **rows, int width, int height,
                     Vector2 worldPosition, bool flipX, bool shadow,
                     float pixelSize, Color customColor);

#endif
```

Include it in `raylib_game.c`. Replace every `DrawSprite(...)` call with
`DrawPixelSprite(..., RIVER_PIXEL, customColor)`, using `(Color){ 0 }` for
gate/frog/title calls, `robotWantedColor` for robot and bubble calls, and
`WandCrystalColor()` for witch calls. Add this helper beside `WaterLight`:

```c
static Color WandCrystalColor(void)
{
    Color inactive = { 208, 214, 224, 255 };
    if (wandFlashTimer > 0.0f)
        return WaterLight(WaterLight(wandCharged? wandColor : inactive));
    if (wandCharged) return WaterLight(wandColor);
    return inactive;
}
```

Add `static Color WandCrystalColor(void);` to the module declarations because
the frame renderer calls it before its definition.

Replace `SpriteColor('M')` with `PixelSpriteColor('M', (Color){ 0 })`. Remove
the static `DrawSprite` declaration and implementation, but leave `SpriteColor`
in place until the failing link check.

- [ ] **Step 2: Verify the new interface is not implemented yet**

Run: `cmake --build build`

Expected: FAIL at link time with undefined references to `DrawPixelSprite` and
`PixelSpriteColor`.

- [ ] **Step 3: Implement the shared renderer**

Move the current palette and drawing loop into `pixel_sprite.c`. Rename
`SpriteColor` to `PixelSpriteColor`, make it non-static, and replace its dynamic
cases with:

```c
case 'C':
case 'X': return customColor;
```

Rename `DrawSprite` to `DrawPixelSprite`; replace `RIVER_PIXEL` with the
`pixelSize` parameter and resolve colors with
`PixelSpriteColor(key, customColor)`. Remove the old palette from
`raylib_game.c`. Add `pixel_sprite.c` to `PROJECT_SOURCE_FILES`.

- [ ] **Step 4: Verify the extraction builds**

Run: `mkdir -p /tmp/raylib-refactor-build && cmake --build build && make -B -C src PROJECT_BUILD_PATH=/tmp/raylib-refactor-build`

Expected: both commands exit 0 with no undefined symbols.

- [ ] **Step 5: Commit only Task 1 files**

Run:

```bash
jj commit src/pixel_sprite.c src/pixel_sprite.h src/raylib_game.c src/Makefile \
  -m "refactor: extract pixel sprite rendering"
```

### Task 2: Extract witch rendering with current wand behavior

**Files:**
- Create: `src/witch.h`
- Create: `src/witch.c`
- Modify: `src/raylib_game.c:12,205-231,526-545,690-695,1634-1638`
- Modify: `src/Makefile:35`

**Interfaces:**
- Consumes: `DrawPixelSprite(...)`
- Produces: `void WitchSetFacingFromMovement(float movementX)`
- Produces: `void DrawWitch(Vector2 worldPosition, bool shadow, float pixelSize, Color crystalColor)`

- [ ] **Step 1: Declare and call the missing witch module**

Create `witch.h` with the two signatures above. Include it in `raylib_game.c`,
replace the inline facing mutation with `WitchSetFacingFromMovement(dx)`, and
pass `WandCrystalColor()` to both witch draw calls. Remove the inline
`DrawWitch` declaration and definition, but leave the bitmap and facing state
in main and do not create `witch.c` yet.

- [ ] **Step 2: Verify the module is missing**

Run: `cmake --build build`

Expected: FAIL with undefined references to `WitchSetFacingFromMovement` and
the four-argument `DrawWitch`.

- [ ] **Step 3: Implement the witch module**

Move `WITCH_SIZE`, `witchSprite`, and `witchFlipX` verbatim into `witch.c`.
Implement facing with the current `< 0.0f`/`> 0.0f` behavior. Implement drawing
as:

```c
void DrawWitch(Vector2 worldPosition, bool shadow, float pixelSize, Color crystalColor)
{
    DrawPixelSprite(witchSprite, WITCH_SIZE, WITCH_SIZE, worldPosition,
                    witchFlipX, shadow, pixelSize, crystalColor);
}
```

Remove the moved definitions from `raylib_game.c` and add `witch.c` to the Make
source list.

- [ ] **Step 4: Verify and commit**

Run: `cmake --build build && make -B -C src PROJECT_BUILD_PATH=/tmp/raylib-refactor-build`

Expected: both commands exit 0.

Run:

```bash
jj commit src/witch.c src/witch.h src/raylib_game.c src/Makefile \
  -m "refactor: extract witch rendering"
```

### Task 3: Extract all robot rendering variants

**Files:**
- Create: `src/robot.h`
- Create: `src/robot.c`
- Modify: `src/raylib_game.c:12,233-420,526-547,739,1640-1696,1740`
- Modify: `src/Makefile:35`

**Interfaces:**
- Consumes: `DrawPixelSprite(...)`
- Produces: `void DrawRobot(float screenWidth, bool happy, Color wantedColor, float pixelSize, float flowSpeed)`
- Produces: `void DrawSadRobot(Vector2 worldPosition, float pixelSize)`

- [ ] **Step 1: Switch callers to the missing robot module**

Create `robot.h` with the signatures above and include it. Replace gameplay
drawing with:

```c
DrawRobot((float)screenWidth, robotHappy, robotWantedColor,
          RIVER_PIXEL, RIVER_FLOW_SPEED);
```

Replace the title-screen sad sprite call with:

```c
DrawSadRobot(robotPos, RIVER_PIXEL);
```

Remove the static `DrawRobot` declaration, but do not create `robot.c` yet.

- [ ] **Step 2: Verify the module is missing**

Run: `cmake --build build`

Expected: FAIL with undefined references to `DrawRobot` and `DrawSadRobot`.

- [ ] **Step 3: Implement the robot module**

Move `ROBOT_W`, `ROBOT_H`, all three robot bitmaps, `BUBBLE_W`, `BUBBLE_H`, and
all bubble bitmaps verbatim into `robot.c`. Adapt the reference commit's robot
renderer to use only its arguments and a local position:

```c
Vector2 robotPosition = { screenWidth/2.0f, 96.0f };
```

Keep the current bob, thought-bubble water, mini-bubble, and antenna timing.
Implement title rendering as:

```c
void DrawSadRobot(Vector2 worldPosition, float pixelSize)
{
    DrawPixelSprite(robotSpriteSad, ROBOT_W, ROBOT_H, worldPosition,
                    false, false, pixelSize, (Color){ 0 });
}
```

Define private `RobotWaterLight` and `RobotWaterDark` helpers using the same
channel adjustments as current `WaterLight` and `WaterDark`. Remove all moved
assets and the inline `DrawRobot` from main; add `robot.c` to the Make sources.

- [ ] **Step 4: Verify and commit**

Run: `cmake --build build && make -B -C src PROJECT_BUILD_PATH=/tmp/raylib-refactor-build`

Expected: both commands exit 0.

Run:

```bash
jj commit src/robot.c src/robot.h src/raylib_game.c src/Makefile \
  -m "refactor: extract robot rendering"
```

### Task 4: Compile the restart spinner as a module

**Files:**
- Replace: `src/restart_spinner.inc` with `src/restart_spinner.c`
- Create: `src/restart_spinner.h`
- Modify: `src/raylib_game.c:12,546,768,1797`
- Modify: `src/Makefile:35`

**Interfaces:**
- Produces: `void DrawRestartSpinner(float progress, int sceneResolution, float pixelSize)`

- [ ] **Step 1: Switch main to the missing compiled API**

Create the header with the signature above, include it, remove the static
declaration and textual `#include "restart_spinner.inc"`, and call:

```c
DrawRestartSpinner(spaceHoldTime/restartHoldTime, RIVER_RES, RIVER_PIXEL);
```

- [ ] **Step 2: Verify the implementation is not linked**

Run: `cmake --build build`

Expected: FAIL with an undefined reference to the three-argument spinner.

- [ ] **Step 3: Adapt the implementation**

Use `apply_patch` to add `src/restart_spinner.c` with the existing spinner body
and delete `src/restart_spinner.inc`. Include `restart_spinner.h`, `raylib.h`,
and `math.h`; remove `static`; replace `RIVER_RES` with `sceneResolution` and
`RIVER_PIXEL` with `pixelSize`. Add the new `.c` file to the Make sources and
remove `restart_spinner.inc` from the `raylib_game.o` dependency rule.

- [ ] **Step 4: Verify and commit**

Run: `cmake --build build && make -B -C src PROJECT_BUILD_PATH=/tmp/raylib-refactor-build`

Expected: both commands exit 0.

Run:

```bash
jj commit src/restart_spinner.c src/restart_spinner.h src/restart_spinner.inc \
  src/raylib_game.c src/Makefile -m "refactor: extract restart spinner"
```

### Task 5: Final behavior-preservation audit

**Files:**
- Inspect: `src/raylib_game.c`
- Inspect: `src/pixel_sprite.c`
- Inspect: `src/witch.c`
- Inspect: `src/robot.c`
- Inspect: `src/restart_spinner.c`

- [ ] **Step 1: Run fresh full verification**

Run:

```bash
cmake --build build --clean-first
make -B -C src PROJECT_BUILD_PATH=/tmp/raylib-refactor-build
```

Expected: all commands exit 0 with no compiler or linker errors.

- [ ] **Step 2: Confirm the extraction boundaries**

Run:

```bash
rg -n 'witchSprite|robotSprite|bubbleSprite|static void DrawRestartSpinner|restart_spinner\.inc' src/raylib_game.c
wc -l src/raylib_game.c
jj log -r 'cf4d55b9::@' --stat
jj st
```

Expected: `rg` returns no matches; `raylib_game.c` is substantially below 1,905
lines; the diff contains renderer extraction and build wiring only; the user's
unrelated `src/river_colors.h` change remains present and uncommitted.

- [ ] **Step 3: Report verification evidence**

Report exact build commands and exit status, the final main-file line count,
the module list, and any warning that remains. Do not claim visual equivalence
beyond what compilation and structural comparison establish.
