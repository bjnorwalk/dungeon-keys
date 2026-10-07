cmake_minimum_required(VERSION 3.20)

project(typing_roguelike_polished C)

set(CMAKE_C_STANDARD 11)

include(FetchContent)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)

FetchContent_MakeAvailable(raylib)

add_executable(typing_roguelike_polished main.c)

target_link_libraries(typing_roguelike_polished PRIVATE raylib)

