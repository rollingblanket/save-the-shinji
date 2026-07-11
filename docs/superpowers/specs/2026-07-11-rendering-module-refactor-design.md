# Rendering Module Refactor Design

## Goal

Reduce the size of `src/raylib_game.c` by adapting the intent of commits
`a03fccf5`, `768d5c3a`, and `74b4211f` to the current game. Preserve gameplay,
rendering, animation, and controls while avoiding a conflict-heavy replay of the
old changes.

## Scope

Extract the shared ASCII sprite renderer, witch rendering, robot rendering, and
restart spinner into normal C modules. Keep river simulation, spell behavior,
level state, title-screen flow, and gate/frog rendering in `raylib_game.c`.

The refactor must retain behavior added after the reference commits:

- The witch's wand crystal reflects its charged color and flash state.
- The title screen renders the sad robot variant.
- The gameplay robot retains normal and happy variants, its thought bubble,
  water animation, bobbing, and antenna blink.
- The restart spinner retains its current position, colors, and fill behavior.

## Module Design

### `pixel_sprite.c` and `pixel_sprite.h`

Own the common ASCII bitmap drawing loop and the static sprite palette. The
renderer accepts pixel size and a caller-provided custom color. Both dynamic
sprite keys used by the extracted modules resolve through that color: `C` for
the robot's requested river color and `X` for the witch's wand crystal. Existing
gate and frog rendering can use the same public drawing and palette helpers from
`raylib_game.c`.

### `witch.c` and `witch.h`

Own the witch bitmap and horizontal facing state. Expose one operation to update
facing from horizontal movement and one to draw the witch. Drawing receives the
position, shadow flag, pixel size, and already-resolved wand crystal color. Wand
gameplay state and water-color calculations remain in `raylib_game.c`.

### `robot.c` and `robot.h`

Own the normal, happy, and sad robot bitmaps and all thought-bubble bitmaps.
Expose gameplay robot drawing with explicit screen width, mood, requested color,
pixel size, and flow speed. Expose sad robot drawing for the title screen. Keep
robot goal state in `raylib_game.c`; keep rendering-only animation calculations
inside the robot module.

### `restart_spinner.c` and `restart_spinner.h`

Replace the textual inclusion of `restart_spinner.inc` with a compiled module.
Expose drawing with progress, low-resolution scene size, and pixel size as
arguments.

## Data Flow

`raylib_game.c` continues to own all gameplay state. Each frame it derives the
wand crystal display color, passes it to the witch module, and passes robot mood
and requested color to the robot module. Extracted modules own only renderer
assets and renderer-local state.

No extracted module reads or mutates game globals from `raylib_game.c`.

## Build Integration

Add the new `.c` files to `PROJECT_SOURCE_FILES` in `src/Makefile`. The existing
recursive glob in `src/CMakeLists.txt` discovers them automatically. Remove
`src/restart_spinner.inc` after its implementation moves to
`src/restart_spinner.c`.

## Verification

Use a compile-first extraction sequence so each new public API is exercised
before its implementation exists, then implement the minimum module needed to
restore the build. Run the full desktop build after every extraction and once
again at the end. Review the final `jj diff` to confirm the change is an
extraction rather than a gameplay rewrite.

Success means the project builds cleanly, the old inline renderer definitions
are absent from `raylib_game.c`, the restart spinner is compiled normally, and
all current rendering inputs remain represented in the new APIs.
