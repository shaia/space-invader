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
  src/postfx.cpp
  src/highscores.cpp
  src/platform_paths.cpp
)
if(WIN32)
  enable_language(RC)
  target_sources(space_invader_plus PRIVATE res/app.rc)  # embeds the executable icon
endif()

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
  # Build a .app bundle so Finder/Dock pick up the icon from Resources/.
  set(icns ${CMAKE_CURRENT_SOURCE_DIR}/res/app.icns)
  set_source_files_properties(${icns} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  target_sources(space_invader_plus PRIVATE ${icns})
  set_target_properties(space_invader_plus PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Space Invaders"
    MACOSX_BUNDLE_ICON_FILE app.icns)
endif()

# Linux: install the binary, hicolor icons, and desktop entry into an XDG prefix.
if(UNIX AND NOT APPLE)
  install(TARGETS space_invader_plus RUNTIME DESTINATION bin)
  install(FILES res/space_invader_plus.desktop DESTINATION share/applications)
  foreach(sz 16 32 48 64 128 256 512)
    install(FILES res/linux/hicolor/${sz}x${sz}/apps/space_invader_plus.png
      DESTINATION share/icons/hicolor/${sz}x${sz}/apps)
  endforeach()
endif()
