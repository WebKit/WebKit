# -----------------------------------------------------------------------------
# Determine CMake version and build type.
# -----------------------------------------------------------------------------
# NOTE: cmake_minimum_required() and project() *MUST* be the two first commands
# used, see https://cmake.org/cmake/help/v3.3/command/project.html -- the
# latter in particular handles loading a bunch of shared CMake definitions
# and loading the cross-compilation settings from CMAKE_TOOLCHAIN_FILE.
#
# cmake_minimum_required is also set inside Source/${FRAMEWORK}/CMakeLists.txt
# in support of the AppleWin internal build so this version should be kept in
# sync across those files.
#
cmake_minimum_required(VERSION 3.16)
project(WebKit)

# Remove this cmake_policy() after upgrading cmake_minimum_required() to 3.20.
if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.20")
    cmake_policy(SET CMP0116 OLD)
endif ()

# -----------------------------------------------------------------------------
# Common configuration
#------------------------------------------------------------------------------
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/Source/cmake")
include(WebKitCommon)

# -----------------------------------------------------------------------------
# Enable API unit tests and create a target for the test runner
# -----------------------------------------------------------------------------
if (ENABLE_API_TESTS)
    enable_testing()
endif ()

# -----------------------------------------------------------------------------
# Add module directories
# -----------------------------------------------------------------------------
add_subdirectory(Source)

# -----------------------------------------------------------------------------
# Add tools
# -----------------------------------------------------------------------------
if (ENABLE_TOOLS)
    add_subdirectory(Tools)
endif ()

if (DEVELOPER_MODE)
    add_subdirectory(PerformanceTests)
endif ()

# -----------------------------------------------------------------------------
# Print the features list last, for maximum visibility.
# -----------------------------------------------------------------------------
PRINT_WEBKIT_OPTIONS()
