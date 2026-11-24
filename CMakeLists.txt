cmake_minimum_required(VERSION 3.16)
project(MedievalRPG VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Auto-detect SDL3 extracted into the project, using the appropriate GCC architecture.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(SDL3_MINGW_ARCH x86_64-w64-mingw32)
else()
    set(SDL3_MINGW_ARCH i686-w64-mingw32)
endif()
set(SDL3_BUNDLED_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/SDL3-3.4.2/${SDL3_MINGW_ARCH}")
if(WIN32 AND EXISTS "${SDL3_BUNDLED_PREFIX}/lib/cmake/SDL3/SDL3Config.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "${SDL3_BUNDLED_PREFIX}")
    message(STATUS "Using local SDL3: ${SDL3_BUNDLED_PREFIX}")
endif()

find_package(SDL3 CONFIG QUIET)
if(NOT TARGET SDL3::SDL3)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL3 QUIET IMPORTED_TARGET sdl3)
        if(TARGET PkgConfig::SDL3)
            add_library(SDL3::SDL3 ALIAS PkgConfig::SDL3)
        endif()
    endif()
endif()
if(NOT TARGET SDL3::SDL3)
    message(FATAL_ERROR
        "SDL3 not found. Extract SDL3-devel-3.4.2-mingw.zip to "
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL3-3.4.2 so x86_64-w64-mingw32/lib/cmake/SDL3 exists.")
endif()

# SDL3_image handles PNG artwork. Look for the user's local MinGW SDK first.
set(SDL3_IMAGE_BUNDLED_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/SDL3_image-3.4.8/${SDL3_MINGW_ARCH}")
if(WIN32 AND EXISTS "${SDL3_IMAGE_BUNDLED_PREFIX}/lib/cmake/SDL3_image/SDL3_imageConfig.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "${SDL3_IMAGE_BUNDLED_PREFIX}")
    message(STATUS "Using local SDL3_image: ${SDL3_IMAGE_BUNDLED_PREFIX}")
endif()
find_package(SDL3_image CONFIG QUIET)

# Optional SDL3_ttf 3.2.2 development SDK alongside SDL3 and SDL3_image.
set(SDL3_TTF_BUNDLED_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/SDL3_ttf-3.2.2/${SDL3_MINGW_ARCH}")
if(WIN32 AND EXISTS "${SDL3_TTF_BUNDLED_PREFIX}/lib/cmake/SDL3_ttf/SDL3_ttfConfig.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "${SDL3_TTF_BUNDLED_PREFIX}")
    message(STATUS "Using local SDL3_ttf: ${SDL3_TTF_BUNDLED_PREFIX}")
endif()
find_package(SDL3_ttf CONFIG QUIET)

add_executable(medieval_rpg src/main.cpp)
# Implementation fragments are included (not independently compiled) by main.cpp.
# List them for IDE navigation without adding duplicate object definitions.
target_sources(medieval_rpg PRIVATE
    src/modules/world_state.inc
    src/modules/asset_pipeline.inc
    src/modules/world_renderer.inc
    src/modules/gameplay.inc
    src/modules/characters_scenes.inc
    src/modules/hud_dialogue.inc
    src/modules/scene_transitions.inc
    src/modules/game_over.inc
    src/modules/inventory_shop_ui.inc
)
set_source_files_properties(
    src/modules/world_state.inc
    src/modules/asset_pipeline.inc
    src/modules/world_renderer.inc
    src/modules/gameplay.inc
    src/modules/characters_scenes.inc
    src/modules/hud_dialogue.inc
    src/modules/scene_transitions.inc
    src/modules/game_over.inc
    src/modules/inventory_shop_ui.inc
    PROPERTIES HEADER_FILE_ONLY TRUE
)
target_include_directories(medieval_rpg PRIVATE src)
target_link_libraries(medieval_rpg PRIVATE SDL3::SDL3)
if(TARGET SDL3_image::SDL3_image)
    target_link_libraries(medieval_rpg PRIVATE SDL3_image::SDL3_image)
    target_compile_definitions(medieval_rpg PRIVATE RPG_HAS_SDL3_IMAGE=1)
    message(STATUS "PNG artwork support enabled")
else()
    target_compile_definitions(medieval_rpg PRIVATE RPG_HAS_SDL3_IMAGE=0)
    message(WARNING "SDL3_image not found: game runs with legacy BMP and procedural fallback only")
endif()
if(TARGET SDL3_ttf::SDL3_ttf)
    target_link_libraries(medieval_rpg PRIVATE SDL3_ttf::SDL3_ttf)
    target_compile_definitions(medieval_rpg PRIVATE RPG_HAS_SDL3_TTF=1)
    message(STATUS "SDL3_ttf font rendering enabled")
else()
    target_compile_definitions(medieval_rpg PRIVATE RPG_HAS_SDL3_TTF=0)
    message(WARNING "SDL3_ttf not found; using built-in pixel text")
endif()
if(WIN32 AND TARGET SDL3_ttf::SDL3_ttf AND EXISTS "${SDL3_TTF_BUNDLED_PREFIX}/bin")
    add_custom_command(TARGET medieval_rpg POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SDL3_TTF_BUNDLED_PREFIX}/bin"
            "$<TARGET_FILE_DIR:medieval_rpg>"
        COMMENT "Copying SDL3_ttf DLLs and dependencies")
endif()
if(WIN32 AND EXISTS "${SDL3_IMAGE_BUNDLED_PREFIX}/bin/SDL3_image.dll")
    add_custom_command(TARGET medieval_rpg POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL3_IMAGE_BUNDLED_PREFIX}/bin/SDL3_image.dll"
            "$<TARGET_FILE_DIR:medieval_rpg>"
        COMMENT "Copying SDL3_image.dll to game output directory")
endif()

# Put SDL3.dll beside the EXE automatically for Windows users.
if(WIN32 AND EXISTS "${SDL3_BUNDLED_PREFIX}/bin/SDL3.dll")
    add_custom_command(TARGET medieval_rpg POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL3_BUNDLED_PREFIX}/bin/SDL3.dll"
            "$<TARGET_FILE_DIR:medieval_rpg>"
        COMMENT "Copying SDL3.dll to game output directory")
endif()

# Copy optional AI-generated BMP art pack into the build output.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET medieval_rpg POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:medieval_rpg>/assets"
        COMMENT "Copying AI-generated scene and portrait assets")
endif()

if(MSVC)
    target_compile_options(medieval_rpg PRIVATE /W4)
else()
    target_compile_options(medieval_rpg PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(game_tests tests/game_tests.cpp)
    target_include_directories(game_tests PRIVATE src)
    add_test(NAME game_logic COMMAND game_tests)
endif()
