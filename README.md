## Walking footsteps and ground particles

Outdoor movement now leaves a subtle burst of short-lived pixel particles
beside the hero's boots. The village path uses warm dust; eastern wilderness
mixes grass flecks with dust. The `src/footstep_particles.h` emitter uses a
fixed 64-particle pool, triggers only after real movement (so blocked steps
create no particles), and fades each fragment within about half a second.
It does not spawn on indoor floors. World-space particles scroll correctly
with the village camera and appear behind the character without requiring
any additional PNG assets.

## Village eastern wilderness: new battle map

The image `assets/gablingather.png` now replaces the old outskirts BMP
as the **eastern forest battle background**, fitted to the game's 960×640
logical viewport. Goblins, weapon effects, and the hero remain separate
sprites, not part of the background. The BMP is kept as a fallback if the
PNG cannot be loaded.

`src/east_battle_map.h` defines the initial walkable clearing, stone
obstacles, river, forest boundaries and the **left bridge** passage. Hero
movement, goblin pursuit and sword knockback use the same collision logic.
Press **F3** to inspect approximate terrain boundaries during play. These
obstacles are an initial gameplay approximation of the illustrated artwork
and may need visual tuning after an in-game screenshot.

From the village, follow the east bridge to enter the clearing; walk
left along the clearing's bridge to return. Near the far eastern path,
**E** starts the existing goblin encounter. When all eight are defeated,
press **E** on the victory screen to return to the village while preserving
inventory, equipment, gold and quest progress. **R** still restarts.

## Mouse-controlled sword combat and goblins

The combat scene now uses **left mouse click** to turn toward the cursor and
perform a 120-degree sword sweep in front of the character. **Space** remains
available as a backup attack button. The sweep has a ~72px reach, a short
cooldown/animation, and deals damage from the equipped weapon's ATK minus the
goblin's DEF. One sword swing can hit more than one goblin in the arc.

When struck, goblins flash, stagger, move backward and briefly display damage
numbers. They now **telegraph a close-range hit for 0.46 seconds** with a
visible warning and then have a recovery cooldown. Backing away from the
attack radius during that warning avoids the hit; the player's existing armor
DEF and short hurt invulnerability still apply.

The scene loads `assets/goblin.png` as the goblin walk/strike atlas and
`assets/sword_attack.png` as the sweeping sword effect. Both source sheets
have a dark matte removed during loading. A procedural goblin and sword arc
remain fallback visuals if the optional textures cannot be loaded.

This first stage does not add stamina, dodge rolls, projectile weapons or a
skill tree. Those will come after the mouse-attack and telegraphed melee
combat feel good in play testing. `src/combat.h` isolates attack geometry
and combat timing, with tests in `tests/game_tests.cpp`.

## Eight-direction hero walking (hero.png)

The latest `assets/hero.png` uses an **8 × 10 sprite atlas** with a
pure-white background and no headings. Each of the eight rows is a direction
and each of its ten cells is one walking frame. `src/hero_animation.h`
selects direction and frame (14 FPS), and `asset_pipeline.inc` preloads
80 separate textures, removing the connected near-white matte. The generated
1448×1086 artwork has **irregular row spacing**, so its 80 crops are measured
rather than equally divided; each is placed on a common transparent canvas
and bottom-aligned to avoid cut-off boots, stray neighbouring frames and
walking-frame jumps.

Atlas row order: Down, Down-Left, Left, Up-Left, Up, Up-Right, Right,
Down-Right. WASD combinations select all eight directional animations.
Idle uses frame zero of the direction most recently faced. Character position,
collisions, attack actions and inventory behavior are unchanged.

## Equipment progression and combat stats

Players no longer start with a complete set of powerful demo weapons and
armor. A new game begins with **plain tunic (DEF +1)**, bread, an apple,
**18 gold**, and **bare hands (ATK 2)**. Talk to Father at home to receive a
**worn sword (ATK +3)**, which is placed in the backpack; equip it using
**I/B**, select the sword and press **E/Enter**. Skipping the family dialogue
is supported, so that sword is optional.

Every equipped slot contributes its own attack/defense bonus. Effective hero
stats are calculated from the six actual equipment slots, not from backpack
contents. Both the inventory and the combat HUD display **ATK/DEF**. Physical
damage uses `max(1, attack - defense)`: goblins have 10 HP, 1 DEF, and
14 ATK. A plain tunic reduces an incoming goblin strike by one; more armor
reduces it further. Equipping a stronger weapon decreases the number of hits
needed to kill a goblin.

The general store offers food, healing supplies, an affordable worn sword and
tunic, plus an **80-gold iron sword** for future progression. All prices deduct
real gold and purchases enter the backpack. Item definitions and the attack /
defense formulas are centralized in `src/equipment.h` with standalone C++
regression tests. Existing art icons are reused for the new low-tier items.

## Source code layout

The SDL3 game is no longer implemented in a 2,400-line `src/main.cpp`.
The entry point now handles the **event loop, scene rendering and SDL lifetime**,
while related implementation is grouped in `src/modules/`:

| Source | Responsibility |
| --- | --- |
| `world_state.inc` | Game state, scene/NPC state, asset structures |
| `asset_pipeline.inc` | PNG loading, sprite cropping, transparent textures |
| `world_renderer.inc` | Shared drawing primitives, terrain/map rendering |
| `gameplay.inc` | Core movement, combat, progression updates |
| `characters_scenes.inc` | Character, scene and NPC rendering |
| `hud_dialogue.inc` | HUD, quest notices, fonts, dialogue |
| `scene_transitions.inc` | Scene transitions, quests and interaction |
| `game_over.inc` | Victory and defeat overlay |
| `inventory_shop_ui.inc` | Inventory, equipment, purchase UI and mouse input |

These files are **implementation fragments included in order** inside the
original anonymous namespace. They are listed as header-only sources in
`CMakeLists.txt` for IDE navigation; they are **not** separately compiled
translation units. This is an incremental, behavior-preserving reorganization,
not a full public-header / multiple-`.cpp` architecture yet. Keep new
inter-module dependencies in the include order, or introduce declarations
before using helpers from a later module.

Existing headers `game_logic.h`, `village_map.h` and `shop_map.h` continue
to host reusable geometry and collision logic tested by `tests/game_tests.cpp`.

## Seamless doors and right-side bridge access

- The upper-right bridge and lower mill bridge now allow uninterrupted
  movement across the river and both approaches; F3 displays the collision
  geometry. Crossing tests cover the full upper bridge and the lower path.
- After the opening scene, walk **south through the cottage door** to enter
  the village without speaking to every family member or pressing E. The
  quest progression is brought forward to the village chapter when needed.
- Walk **north into the original cottage's doorway** (near village image
  position 405, 362) to go home again. Inventory, gold, and quest state are
  preserved. Walk south out again whenever you wish.
- Walk **east over the mill bridge** toward the right-bank path (near village
  image position 1395, 751) to enter the outskirts automatically. No E prompt
  or forced dialogue is needed. While outside the battle, moving west to the
  outskirts edge returns you to the village.
- The general-store exit is also walk-through; shopping with the merchant
  still uses E or the mouse. Existing E interactions remain for conversation.

## General store: separate room, furniture and buying

The village now has an optional shop interior built from two image assets:
`assets/shopback.png` fills the 960×640 room and individual props cropped from
`assets/shopitems.png` create shelves, wooden counters, tables and barrels.
Furniture uses shape-specific collision footprints (including the full upper
body of barrels and crates) and its front/back draw sorting uses the same
feet depth. F3 shows the collision rectangles. The shopkeeper reuses the existing merchant character graphics.

In the large village, walk south of the shop near image coordinate (1104, 731)
and press **E** when the **E SHOP** marker appears. The shop may be visited
without advancing or resetting the chapter quest. Inside, walk toward the
counter and press **E** to open the general-store purchase menu. Use
**Mouse hover** to highlight a product and **left-click** to buy; click the top-right **X** to close. **Up/Down**, **Enter/E**, and **Esc/B** still work.
Purchases deduct the displayed gold price and put an item in the first open
backpack slot; out-of-gold and full-inventory conditions show messages.
Walk back to the entrance and press **E** to return to the village.

The six test goods are health potion, herbs, bread, apple, shield and boots.
This is a first playable store; buying is implemented, but selling, restocking
and persistent saves are not yet present. Inventory demo items are unchanged.
If the new images are missing, the shop retains functional layout with a
fallback drawn interior or invisible furniture footprints.

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

## Fading quest banners and persistent health

The original large top chapter/quest banner and bottom control tips now fade
in (0.4 seconds), remain for 2.8 seconds, then fade out (1 second) instead
of permanently covering the scene. They replay when story progress changes,
when entering another scene, or after a restart. Press **F1** to show the
two prompts again at any time.

A separate always-visible **HP / 100** bar is placed at the upper left below
the former quest-banner area. It updates with combat damage or healing in
the inventory/quickbar. The battle-specific kill counter remains available.

## RPG inventory UI (ui.png + items.png)

The playable backpack reads `assets/ui.png` (stone-and-gold UI panels) and
`assets/items.png` (item icon atlas), with procedural slot frames as a
fallback. It starts with a sample set of weapons, armor, consumables and gold
for testing. These demo items are not yet connected to the game world's loot
or merchant economy.

- **B / I**: open or close backpack. **Esc** closes it.
- **Arrow keys**: select one of 24 backpack slots.
- **E / Enter**: equip the selected weapon or armor, or use a consumable
  (health recovery only if HP is not full).
- **Mouse left-click**: select a backpack item, or click an equipped item
  to move it into the first empty bag slot.
- **1–8**: with the backpack open, bind the selected item to that hotbar key;
  outside the backpack, select the hotbar key and use a matching consumable.
- A quickbar remains visible in normal gameplay, except during dialogue.
  Movement is paused while the inventory is open.

Equipment slots: weapon, shield, helmet, armor, boots and accessory/other.
Clicking a backpack slot selects it; E transfers equipment into its slot and
swaps an existing equipped item back into the backpack. The original item
textures are cropped from the generated concept sheet, so some margins may
need adjustment after an in-game visual check.

## Animated village citizens (`villager.png`)

The uploaded `assets/villager.png` now supplies eight character styles, with
four facing directions and four walking poses per direction (128 cropped
textures). The loader removes dark backgrounds connected to the edge of each
sprite crop so the labels and the sheet background are not displayed.

Elder Rowan, the merchant and the guard use illustrated sprites without
changing their existing chapter progression. Eight additional townsfolk roam
short routes near their homes, pause, turn to walk and can be spoken to with
**E** at close range. NPC walking respects village obstacle collisions; their
animation and placement use the expanded village map. If the PNG is unavailable,
procedural NPC drawings remain as a fallback. Collision and portrait cropping
are approximate because the supplied illustration is a labelled concept sheet.

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
