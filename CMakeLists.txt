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

add_executable(medieval_rpg src/main.cpp)
target_include_directories(medieval_rpg PRIVATE src)
target_link_libraries(medieval_rpg PRIVATE SDL3::SDL3)

# Put SDL3.dll beside the EXE automatically for Windows users.
if(WIN32 AND EXISTS "${SDL3_BUNDLED_PREFIX}/bin/SDL3.dll")
    add_custom_command(TARGET medieval_rpg POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL3_BUNDLED_PREFIX}/bin/SDL3.dll"
            "$<TARGET_FILE_DIR:medieval_rpg>"
        COMMENT "Copying SDL3.dll to game output directory")
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
