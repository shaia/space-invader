cmake_minimum_required(VERSION 3.24)
project(space_invader_plus CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib)

add_executable(space_invader_plus
  src/main.cpp
  src/game.cpp
  src/screens.cpp
  src/invaders.cpp
  src/player.cpp
  src/bunkers.cpp
  src/powerups.cpp
  src/modifiers.cpp
  src/boss.cpp
  src/particles.cpp
  src/ui_fx.cpp
  src/audio.cpp
  src/render.cpp
  src/highscores.cpp
  src/platform_paths.cpp
)
target_link_libraries(space_invader_plus PRIVATE raylib)

if(MSVC)
  target_compile_options(space_invader_plus PRIVATE /W4 /permissive-)
  # getenv is used deliberately (platform_paths.cpp) to keep windows.h out of the project
  target_compile_definitions(space_invader_plus PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(space_invader_plus PRIVATE -Wall -Wextra)
endif()

if(APPLE)
  target_link_libraries(space_invader_plus PRIVATE
    "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
