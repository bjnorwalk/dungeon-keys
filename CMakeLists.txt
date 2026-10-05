cmake_minimum_required(VERSION 3.20)

project(typing_roguelike_polished C)

# This CMake script configures a simple raylib project.  It
# retrieves the raylib source using CMake's FetchContent module
# and links it to our executable.  By using FetchContent the
# project remains self‑contained – no separate installation of
# raylib is required.  CLion will automatically download and
# build raylib the first time you open the project.

set(CMAKE_C_STANDARD 11)

include(FetchContent)

# Fetch the specific tagged version of raylib.  If you wish to
# update to a newer release, change the version string here.
FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)

FetchContent_MakeAvailable(raylib)

add_executable(typing_roguelike_polished main.c)

target_link_libraries(typing_roguelike_polished PRIVATE raylib)

# Copy any required runtime data into the build directory.  In this
# project all assets are generated procedurally so there are no
# additional data files to install, but the directive is kept to
# illustrate where you would copy external resources if needed.