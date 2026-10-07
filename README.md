# Dungeon Keys: Rogue Edition

A typing roguelike written in C with raylib. Move through a dungeon and type the
words over enemies to defeat them. Each floor introduces new waves, traps, and
boss encounters.

Between floors, a shop lets you spend gold on upgrades. Bombs, freeze pickups,
and health pickups change how a run plays out. High scores and permanent upgrades
are saved between runs.

## Run locally

Use a C11 compiler and CMake 3.20 or newer. The first build downloads raylib 5.5,
so it needs a network connection and raylib's platform build dependencies.

```sh
cmake -S . -B build
cmake --build build --config Release
```

For a single-configuration build, run `build/typing_roguelike_polished`. With
Visual Studio on Windows, run `build/Release/typing_roguelike_polished.exe`;
with MinGW, run `build/typing_roguelike_polished.exe`. The executable keeps the
original CMake target name.

## Controls and saves

- Enter or Space starts a run.
- Arrow keys or WASD move; typing attacks the words displayed over enemies.
- Number keys select shop upgrades. Enter or Escape leaves the shop.
- Difficulty and other run controls are shown on screen.

The game writes `save.dat` in its working directory. An optional `wordlist.txt`
adds words; built-in lists are used when it is absent. Personal saves are ignored
by Git.

## Implementation

`main.c` contains the game loop, dungeon generation, entity state, combat, drawing,
and save/load code. `CMakeLists.txt` downloads and links raylib. The visuals are
drawn in code, so the game does not need a separate art bundle.

There is no automated test suite yet. Build checks cover compilation; gameplay,
controls, and saving need a manual run.
