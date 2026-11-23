# Medieval RPG — The Ashvale Trial

A **playable, self-contained medieval fantasy RPG prototype** built with **C++17 + SDL3 + CMake**, compatible with **GCC / MinGW-w64**. The first milestone focuses on a working gameplay loop, rather than external art assets.

## AI-generated artwork

The game **loads real image textures** when the AI art pack is installed. Download the separate `RpgGameTry_AI_Art_Pack_v02.zip` attached in the chat, and extract it directly into the project root. The resulting files should be:

```text
RpgGameTry/
  assets/
    home.bmp
    village.bmp
    outskirts.bmp
    hero.bmp
    father.bmp
    mother.bmp
    lily.bmp
```

SDL3 loads these images using `SDL_LoadBMP` and `SDL_CreateTextureFromSurface`. You do **not** need SDL3_image. Scene paintings are extracted from the AI-generated concept sheet. They currently contain some baked-in characters and decorative UI from the image, so this is a **visual prototype**, not a clean seamless tileset. Animated player/NPC figures are still drawn above those backdrops; a dedicated character sprite atlas is a later task.

To compile and run in Windows PowerShell:

```powershell
git pull origin main
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j 4
.\\build\\medieval_rpg.exe
```

CMake copies `assets` to `build/assets` when they are present during configuration. The renderer also loads from `./assets`, `../assets`, or `../../assets` depending on where it is launched. Without art files the game gracefully uses its former SDL3 procedural graphics. **The image pack is distributed as a separate chat attachment and is not part of GitHub clone/pull.**

## Features (v0.2)

- 960 × 640 top-down medieval village, houses, inn, trees, stone well, dirt roads and stream
- Story-driven opening: wake up in a countryside cottage, speak with family, visit the village, investigate smoke
- Three scenes with pixel-art-style environments, character NPCs, quest objectives and dialogue panels
- Armored knight controlled with WASD; later, 8 goblins with basic pursuit AI
- Quest-gated directional melee combat and cooldown, enemy health indicators
- Collision against buildings, trees and map bounds
- Health / kill counter HUD, win / game-over screens, restart
- Delta-time movement and simple deterministic procedural decoration
- Pure C++ logic unit tests (CTest); no SDL_ttf or downloadable assets needed

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
