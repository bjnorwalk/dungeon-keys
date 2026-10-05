# Dungeon Keys: Rogue Edition

A typing roguelike by William Norwalk, written in C with raylib. Move through procedural dungeons and type enemy words to survive increasingly difficult waves.

## Gameplay

- Procedural floors, enemy waves, and boss encounters.
- Traps, health pickups, bombs, freeze powerups, and a between-floor shop.
- Persistent upgrades and high scores saved through C file I/O.
- Combo scoring, fog of war, particles, HUD feedback, and screen shake.

## Build and run

Requirements: CMake 3.20+, a C11 compiler, and internet access on the first build to download raylib 5.5. Platform-specific raylib build dependencies may also be required.

```sh
cmake -S . -B build
cmake --build build --config Release
```

Run `build/typing_roguelike_polished` on a single-configuration build. On Windows, the executable is typically `build/Release/typing_roguelike_polished.exe` with Visual Studio, or `build/typing_roguelike_polished.exe` with MinGW. The executable retains the original internal project name.

## Controls and persistence

- Enter or Space starts a run from the title screen.
- Arrow keys or WASD move; type the words displayed over enemies.
- Number keys select shop upgrades; Enter or Escape leaves the shop.
- Follow the on-screen prompts for difficulty and run actions.

`save.dat` is created in the working directory. An optional `wordlist.txt` can supply words; the source contains fallback word lists. Personal saves are excluded from this repository.

## Implementation

`main.c` contains the game loop, entity state, dungeon generation, combat, UI, and save/load routines. `CMakeLists.txt` fetches and links raylib. Game visuals are drawn procedurally, so no separate art bundle is required.
