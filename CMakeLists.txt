cmake_minimum_required(VERSION 3.20)

project(typing_roguelike_polished C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

if(EMSCRIPTEN)
    set_target_properties(typing_roguelike_polished PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/web"
        OUTPUT_NAME index
        SUFFIX ".html"
    )
    target_link_options(typing_roguelike_polished PRIVATE
        "-sUSE_GLFW=3"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFORCE_FILESYSTEM=1"
        # raylib's audio bridge reads the heap through Module.
        "-sEXPORTED_RUNTIME_METHODS=HEAPF32"
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    )
endif()
