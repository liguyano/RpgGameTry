# Medieval RPG — The Ashvale Trial

A **playable, self-contained medieval fantasy RPG prototype** built with **C++17 + SDL3 + CMake**, compatible with **GCC / MinGW-w64**. The first milestone focuses on a working gameplay loop, rather than external art assets.

## AI-generated artwork (home.png + family.png)

The game now prefers **PNG** when SDL3_image is present. Your two uploaded files are already in the GitHub `assets/` folder:

```text
RpgGameTry/
  SDL3-3.4.2/x86_64-w64-mingw32/
  SDL3_image-3.4.8/x86_64-w64-mingw32/
  assets/
    home.png             # full cottage background with furniture
    village.png          # large 1536x1024 village map (upload this PNG)
    family.png           # father/mother/Lily idle + 4 directions (3 frames)
    *.bmp                # fallback backgrounds/portraits used in other scenes
```

At startup SDL3_image loads `home.png` instead of the old `home.bmp`. Family NPCs render using crops from `family.png`, with dark sheet backdrop keyed out at load time. NPCs still follow the same scripted interactions; characters are animated with simple idle/talk frames. The spritesheet includes printed labels and a dark backdrop; the cropping is a prototype, not yet a clean production atlas.

**Windows (PowerShell, GCC 64-bit, MinGW Makefiles):**

```powershell
cd E:\code\RpgGameTry
git pull origin main
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j 4
ctest --test-dir build --output-on-failure
.\build\medieval_rpg.exe
```

CMake automatically detects both SDL3 and SDL3_image at the above local SDK paths, and copies their DLLs and the `assets/` directory into `build/`. The `SDL3_image-3.4.8/` SDK is local only and must not be committed. Without SDL3_image the game still builds and uses BMP/procedural artwork.

## SDL3_ttf dialogue typography (3.2.2)

Extract `SDL3_ttf-devel-3.2.2-mingw.tar.gz` alongside the other local SDKs:

```text
RpgGameTry/
  SDL3_ttf-3.2.2/
    x86_64-w64-mingw32/
      include/SDL3_ttf/
      lib/cmake/SDL3_ttf/
      bin/
```

CMake automatically finds the matching 64-bit MinGW SDK, links SDL3_ttf,
and copies the SDK runtime DLLs alongside the executable.

For the dialogue font, optionally put **your own licensed TTF or OTF font** at
`assets/fonts/dialogue.ttf` or `assets/fonts/dialogue.otf`. Without one,
the game tries Windows Georgia, Segoe UI and Arial, then a Linux DejaVu font.
If no font or SDL3_ttf is available, built-in pixel lettering still works.

Dialogue now uses actual lowercase glyphs, UTF-8 capable SDL3_ttf rendering,
word wrapping, and a typewriter reveal at about 34 characters/second.
Press **E or Space** once to show the full line, and again to continue.
The renderer caches dialogue textures rather than recreating the same text
every game frame.

## Village object layers and moving family NPCs

- `assets/village.png` supplies the **ground and roads** without houses or trees.
- `assets/houseAndTree.png` supplies **separate building and tree sprites**. The renderer extracts individual regions and draws objects either before or after the player based on the object's foot position, so that buildings/tree crowns can obscure the hero from behind.
- Sprite rectangles and world placements are **initial approximate values** in `VILLAGE_PROPS` inside `src/main.cpp`. Generated source sheets are not pixel-perfect production atlases; some background-edge artifacts and scene placement mismatches may remain.
- At home, **Mother, Father and Lily wander short scripted paths**, pause between destinations, face the walking direction, and play the matching walk animation. They stop moving when a dialogue box is open, and E interactions follow their latest location.

No new library is required beyond the existing SDL3 and SDL3_image SDKs.

## Animated hero (hero.png)

The game now loads `assets/hero.png` through SDL3_image and extracts **16 walking frames** (four frames for each of down, left, right and up). WASD picks the facing row and advances the animation; standing still displays the initial pose. The hero sprites are drawn in the home, scrolling village and outskirts. The image is a labeled concept sheet rather than a transparent production atlas, so each frame's connected dark background is automatically removed on load and some edge artifacts may remain. If the PNG is unavailable, the original program-drawn hero is used.

## New 12-frame fountain animation

The game now prefers the newly generated 4×3 (12-frame) fountain PNG,
using its **original filename** `水しぶきの噴水アニメーション_spritesheet.png`
or any `assets/*.png` filename containing `ani` (case-insensitive).
There is **no need to rename** the downloaded image. Copy it into
`assets/` and commit/push it to share it with other machines.
CMake copies the asset directory into the executable's output folder when
building. The image plays at 10 FPS, with no added synthetic droplets.
The previous split assets remain available as a **static fallback**, with
`spring.png` as the last-resort static fallback.

## Split fountain sprites (stable basin + moving water)

To eliminate the shaking caused by slightly different stone basins on every
frame of `spring.png`, the game now prefers **two separate transparent PNGs**:

- `assets/fountain_base.png`: a stationary 362×362 RGBA basin.
- `assets/fountain_water_12frames.png`: RGBA water animation, 1448×1086,
  laid out in 4 columns and 3 rows of 362×362 pixels.

Both layers share exactly the same destination rectangle and anchor. The water image now stays on one **full-height jet frame**, while translucent water streaks move continuously **downward** through the central stream and both side falls, with splashes where they hit the basin. The stone basin never moves and the jet never retracts. Keep both files
in `assets/` and run `git add assets/fountain_base.png
assets/fountain_water_12frames.png` followed by a commit and push.
If either file is absent, the old `spring.png` is displayed as a **single,
stationary frame**, rather than shaking.

## Animated fountain in the village

The uploaded `assets/spring.png` is loaded as a **4-column × 3-row, 12-frame** fountain animation (about **9 FPS**) with SDL3_image. Its water jet, splash and basin are rendered at the village plaza with camera offset, and the entire fountain switches to foreground when the hero walks behind it. The pool uses a small **circular collision**, rather than a large legacy rectangle. Press **F3** to see the golden fountain collision outline. The original generated sheet has a dark matte; the loader removes only near-black pixels connected to each frame's outer edge. If the image is missing, the water sprite simply is not rendered.

## Air-wall cleanup (current village ground map)

The former village's fountain, stalls, decorative wall boxes and oversize old forest/cliff polygons were removed from collision because those objects no longer exist in the **new** `village.png`. House/tree colliders still follow their separate placed props, and water/river collision remains. Press **F3** to inspect real building and water boundaries. Collision shapes are hand-authored approximations; cliff and coast detail will need new collision data based on the current ground image.

## Expanded village collision and camera

Put your generated **`village.png`** in `assets/` and commit/push it once. The artwork is **not yet in the remote repository** until you add it. When the file exists and SDL3_image is available, the village uses a **2304x1536 world** (the 1536x1024 image enlarged 1.5x), with a smooth camera following the player.

House and tree collision now covers the **lower half of each placed sprite** (85% width for houses, 65% width for trees). Upper roofs and tree crowns remain visually layered, and the player can pass behind their upper portions and be obscured. Rivers, cliffs, fountain and market stalls remain solid. There are collision exemptions at the bridges. Movement is resolved per axis and subdivided into short steps to prevent fast movement through walls. NPC positions, interactions and east-road transition are mapped in the same scrolling world space. Press **F3** in the village to show collision outlines: **red** = building, **blue** = terrain/water, **green** = bridge passage. The collision shapes are approximate hand-authored polygons and may need adjustment after inspecting the final PNG in game.

If the PNG has not been uploaded, the game keeps displaying the existing `village.bmp` and its old smaller map behavior.

## Features (v0.2)

- 960 × 640 top-down medieval village, houses, inn, trees, stone well, dirt roads and stream
- Story-driven opening: wake up in a countryside cottage, speak with family, visit the village, investigate smoke
- Three scenes with pixel-art-style environments, character NPCs, quest objectives and dialogue panels
- Armored knight controlled with WASD; later, 8 goblins with basic pursuit AI
- Quest-gated directional melee combat and cooldown, enemy health indicators
- Collision against buildings, trees and map bounds
- Health / kill counter HUD, win / game-over screens, restart
- Delta-time movement and simple deterministic procedural decoration
- Pure C++ logic unit tests (CTest); no SDL_ttf required

## Controls

| Key | Action |
| --- | --- |
| WASD | Move |
| E | Talk / interact with NPCs and places |
| Space | Continue dialogue, or attack after combat begins |
| R | Restart |
| Esc | Quit |

**Chapter 1:** Speak with Mother, Father and Lily at home, leave the cottage, meet Elder Rowan, the Merchant and the Guard, inspect the east road and the smoke. Combat only begins after investigating the ruins. Defeat **8 goblins** without losing all your HP to win.

## Build on Windows with your local SDL3 SDK (recommended)

Extract the **SDL3 3.4.2 MinGW development ZIP** into the repository root:

```text
RpgGameTry/
  CMakeLists.txt
  SDL3-3.4.2/
    x86_64-w64-mingw32/
      include/SDL3/SDL.h
      lib/cmake/SDL3/SDL3Config.cmake
      bin/SDL3.dll
```

The CMake project **automatically detects this SDL3 folder** and copies `SDL3.dll` next to `medieval_rpg.exe`. Do not manually copy DLLs or set `CMAKE_PREFIX_PATH`.

From **PowerShell**, assuming GCC/MinGW-w64 and CMake are already on PATH:

```powershell
cd E:\code\RpgGameTry
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j 4
ctest --test-dir build --output-on-failure
.\build\medieval_rpg.exe
```

If you previously configured `build` with another generator or SDK, remove the build folder first: `Remove-Item -Recurse -Force .\build`.

Alternatively, from **MSYS2 UCRT64**, install the GCC, CMake and Ninja packages (SDL3 is already bundled locally):

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
cmake -S . -B build -G Ninja
cmake --build build
./build/medieval_rpg.exe
```

**Important:** Use 64-bit GCC for `x86_64-w64-mingw32`. SDL3 3.4.2 is not committed into Git; keep your existing local SDK folder when pulling.

## Build on Linux (Debian/Ubuntu)

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libsdl3-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/medieval_rpg
```

## Project structure

```text
RpgGameTry/
├── CMakeLists.txt
├── src/
│   ├── main.cpp         # SDL3 window, world, rendering and game loop
│   └── game_logic.h     # reusable math, collision and damage helpers
├── tests/
│   └── game_tests.cpp   # CTest unit tests
└── .github/workflows/
    └── build.yml        # Windows MinGW GCC CI
```

## Roadmap

v0.2: texture atlas and animations, character sprites, camera and larger tile map.  
v0.3: NPC conversations, quests, inventory, equipment and item pickup.  
v0.4: multiple areas, persistence, audio and combat upgrades.

> Note: This is an initial playable prototype, not yet a full story-driven RPG. Art is generated by SDL3 drawing primitives to keep the initial build self-contained.

### Family sprite sheet calibration

`family.png` is a 1448x1086 labeled concept sheet, not a uniform grid. The extracted sprites are now calibrated against its real 18-column layout (idle 3, down/left/right/up walk 3 each, talk 3) and the three actual character rows. Walking direction selects the correct directional frames, standing uses separate idle poses, and conversation uses the talk poses. Do not replace `family.png` with a differently arranged sheet without updating the crop coordinates in `src/main.cpp`.
