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

## Village object layers and moving family NPCs

- `assets/village.png` supplies the **ground and roads** without houses or trees.
- `assets/houseAndTree.png` supplies **separate building and tree sprites**. The renderer extracts individual regions and draws objects either before or after the player based on the object's foot position, so that buildings/tree crowns can obscure the hero from behind.
- Sprite rectangles and world placements are **initial approximate values** in `VILLAGE_PROPS` inside `src/main.cpp`. Generated source sheets are not pixel-perfect production atlases; some background-edge artifacts and scene placement mismatches may remain.
- At home, **Mother, Father and Lily wander short scripted paths**, pause between destinations, face the walking direction, and play the matching walk animation. They stop moving when a dialogue box is open, and E interactions follow their latest location.

No new library is required beyond the existing SDL3 and SDL3_image SDKs.

## Animated hero (hero.png)

The game now loads `assets/hero.png` through SDL3_image and extracts **16 walking frames** (four frames for each of down, left, right and up). WASD picks the facing row and advances the animation; standing still displays the initial pose. The hero sprites are drawn in the home, scrolling village and outskirts. The image is a labeled concept sheet rather than a transparent production atlas, so each frame's connected dark background is automatically removed on load and some edge artifacts may remain. If the PNG is unavailable, the original program-drawn hero is used.

## Expanded village collision and camera

Put your generated **`village.png`** in `assets/` and commit/push it once. The artwork is **not yet in the remote repository** until you add it. When the file exists and SDL3_image is available, the village uses a **2304x1536 world** (the 1536x1024 image enlarged 1.5x), with a smooth camera following the player.

House collision now covers **small foundations only**, and tree collision covers **narrow trunks**, so the hero can walk behind roofs/canopies and become fully hidden by the foreground sprite. Rivers, cliffs, fountain and market stalls remain solid. There are collision exemptions at the bridges. Movement is resolved per axis and subdivided into short steps to prevent fast movement through walls. NPC positions, interactions and east-road transition are mapped in the same scrolling world space. Press **F3** in the village to show collision outlines: **red** = building, **blue** = terrain/water, **green** = bridge passage. The collision shapes are approximate hand-authored polygons and may need adjustment after inspecting the final PNG in game.

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
