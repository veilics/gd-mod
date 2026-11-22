cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(SorrowIntegration VERSION 1.0.0 LANGUAGES C CXX)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please set GEODE_SDK to the SDK root.")
else()
    message(STATUS "Found Geode SDK: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Globed soft-link headers, if the dependency was fetched into the build dir.
# When they are absent the mod still compiles: GlobedLink falls back to polling only.
if (EXISTS ${CMAKE_CURRENT_BINARY_DIR}/geode-deps/dankmeme.globed2/include)
    # The code includes <dankmeme.globed2/include/globed/...>, so the include
    # root must be the PARENT of the dankmeme.globed2 directory.
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/geode-deps)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SORROW_GLOBED_HEADERS=1)
endif()

# Vendored GD-Roulette layout: sources include headers across subdirs by
# bare name ("utils.hpp", "SingletonBase.hpp"), so every dir is on the path.
foreach(DIR utils managers listfetcher roulette layers custom_layers custom_nodes)
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/${DIR})
endforeach()

target_link_libraries(${PROJECT_NAME} geode-sdk)

# RTRP (RobTop request parser) - used by the vendored GD-Roulette code to
# decode GJLevels responses. Pinned upstream tag, fetched at configure time.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake_rtrp.cmake)

# Vendored GD-Roulette expects a build timestamp (used for its cache keys).
string(TIMESTAMP SORROW_BUILD_TIME "%Y%m%d%H%M%S")
target_compile_definitions(${PROJECT_NAME} PRIVATE ROULETTE_BUILD_TIMESTAMP=${SORROW_BUILD_TIME})

# operator""_spr expands ids with GEODE_MOD_ID at compile time. Without the
# macro the runtime fallback returns nullptr and every _spr use segfaults in
# strlen (seen in the field on Android). Define it from mod.json.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/mod.json SORROW_MOD_JSON)
string(JSON MOD_ID GET "${SORROW_MOD_JSON}" id)
target_compile_definitions(${PROJECT_NAME} PRIVATE GEODE_MOD_ID="${MOD_ID}")

setup_geode_mod(${PROJECT_NAME})
