# -----------------------------------------------------------------------------
# Determine CMake version and build type.
# -----------------------------------------------------------------------------
# NOTE: cmake_minimum_required() and project() *MUST* be the two first commands
# used, see https://cmake.org/cmake/help/v3.3/command/project.html -- the
# latter in particular handles loading a bunch of shared CMake definitions
# and loading the cross-compilation settings from CMAKE_TOOLCHAIN_FILE.

cmake_minimum_required(VERSION 3.20)
project(WebKit LANGUAGES C CXX)

# -----------------------------------------------------------------------------
# Common configuration
#------------------------------------------------------------------------------
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/Source/cmake")
include(WebKitCommon)

# -----------------------------------------------------------------------------
# Enable Swift - we can't do this until we've checked our configuration,
# which in turn depends on clang-related variables set by the initial
# "project()" line
#------------------------------------------------------------------------------
if (SWIFT_REQUIRED)
    cmake_policy(SET CMP0157 NEW)
    enable_language(Swift)

    # Override Swift compiler to a wrapper script to work around the
    # fact pkg-config feeds CFLAGS even to swiftc.
    # Guard against cache poisoning: if CMAKE_Swift_COMPILER already IS the
    # wrapper (from a previous configure), find the real compiler via 'which'.
    set(_swift_wrapper "${CMAKE_SOURCE_DIR}/Tools/Scripts/swift/swiftc-wrapper.sh")
    if (CMAKE_Swift_COMPILER STREQUAL _swift_wrapper)
        execute_process(COMMAND which swiftc RESULT_VARIABLE _which_retcode OUTPUT_VARIABLE _real_swiftc OUTPUT_STRIP_TRAILING_WHITESPACE)
        if (NOT _which_retcode EQUAL 0)
            message(FATAL_ERROR "swiftc not found in PATH")
        endif ()
    else ()
        set(_real_swiftc "${CMAKE_Swift_COMPILER}")
    endif ()
    set(ORIGINAL_Swift_COMPILER "${_real_swiftc}" CACHE FILEPATH "Original Swift compiler" FORCE)
    set(CMAKE_Swift_COMPILER "${_swift_wrapper}" CACHE FILEPATH "Swift compiler wrapper" FORCE)
    add_compile_options($<$<COMPILE_LANGUAGE:Swift>:--original-swift-compiler=${ORIGINAL_Swift_COMPILER}>)
    unset(_swift_wrapper)
    unset(_real_swiftc)
endif ()

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
