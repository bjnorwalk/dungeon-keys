# Dungeon Keys: Rogue Edition

A typing roguelike written in C with raylib. Move through a dungeon and type the
words over enemies to defeat them. Each floor introduces new waves, traps, and
boss encounters.

Between floors, a shop lets you spend gold on upgrades. Bombs, freeze pickups,
and health pickups change how a run plays out. High scores and permanent upgrades
are saved between runs.

## Play in the browser

[Play Dungeon Keys](https://bjnorwalk.github.io/dungeon_keys/).
Use a desktop keyboard. Click the game to focus it, then press Enter to start.
The browser build keeps upgrades and high scores in local storage when available.
It uses the same C game code as the desktop build.

## Run locally

Use a C11 compiler and CMake 3.20 or newer. The first build downloads raylib 5.5,
so it needs a network connection and raylib's platform build dependencies.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

For a single-configuration build, run `build/typing_roguelike_polished`. With
Visual Studio on Windows, run `build/Release/typing_roguelike_polished.exe`;
with MinGW, run `build/typing_roguelike_polished.exe`. The executable keeps the
original CMake target name.

On macOS, install Xcode Command Line Tools (`xcode-select --install`) and CMake.
On Ubuntu, install the compiler, CMake, and raylib's development libraries:

```sh
sudo apt-get install build-essential cmake libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev
```

GitHub Actions runs a Linux build on pushes and pull requests.

## Build for the web

Install CMake and [Emscripten 6.0.11](https://emscripten.org/docs/getting_started/downloads.html),
then activate the SDK environment in your shell.

```sh
emcmake cmake -S . -B build-web -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF
cmake --build build-web --parallel 2
python3 -m http.server 8000 --directory build-web/web
```

Open `http://localhost:8000`. Serve the generated HTML, JavaScript, and WebAssembly
files together; opening the HTML directly from disk will not load the game.
GitHub Actions compiles this build and publishes it to GitHub Pages. Pull requests
build it without publishing. The hosting branch also publishes while its port is
under review; after merging, `main` handles future deployments.

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
controls, and saving need a manual run. Browser saves belong to the site origin
and are separate from desktop save files. Browsers that block local storage can
still play, but progress is limited to the current session.
