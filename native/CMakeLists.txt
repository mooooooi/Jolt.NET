cmake_minimum_required(VERSION 3.21)
project(jolt_glue LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Distribution" CACHE STRING "Available build configurations" FORCE)
endif()

set(CMAKE_CXX_FLAGS_DISTRIBUTION "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "Distribution C++ flags" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_DISTRIBUTION "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}" CACHE STRING "Distribution shared linker flags" FORCE)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library" FORCE)
endif()

set(JPH_ROOT "${CMAKE_CURRENT_LIST_DIR}/../lib/JoltPhysics")

option(CROSS_PLATFORM_DETERMINISTIC "Enable deterministic Jolt build flags" ON)
option(GENERATE_DEBUG_SYMBOLS "Generate debug symbols" OFF)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build Jolt as a static library" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "Disable Jolt install target" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "Do not promote Jolt warnings to errors for glue builds" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "Disable LTO for glue builds" FORCE)

add_subdirectory("${JPH_ROOT}/Build" "${CMAKE_BINARY_DIR}/JoltPhysics")

add_library(joltcd SHARED
    jolt_glue.cpp
)

target_include_directories(joltcd
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}"
    PRIVATE
        "${JPH_ROOT}"
)

target_link_libraries(joltcd PRIVATE Jolt)
target_compile_definitions(joltcd PRIVATE JPH_GLUE_BUILD)

set_target_properties(joltcd PROPERTIES
    OUTPUT_NAME "joltcd"
)
