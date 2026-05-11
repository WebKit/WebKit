# -----------------------------------------------------------------------------
# Determine CMake version and build type.
# -----------------------------------------------------------------------------
# NOTE: cmake_minimum_required() and project() *MUST* be the two first commands
# used, see https://cmake.org/cmake/help/v3.3/command/project.html -- the
# latter in particular handles loading a bunch of shared CMake definitions
# and loading the cross-compilation settings from CMAKE_TOOLCHAIN_FILE.

cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_SOURCE_DIR}/Source/cmake/AppleDeviceCrossCompile.cmake)

if (PORT STREQUAL "IOS")
    webkit_setup_apple_device_cross_compile(
        SYSTEM_NAME iOS
        SDK_NAME "iphoneos"
        SDK_NAME_SIMULATOR "iphonesimulator"
        DEFAULT_DEPLOYMENT_TARGET "18.0"
        SUPPORTS_ARM64E
    )
elseif (PORT STREQUAL "XROS")
    webkit_setup_apple_device_cross_compile(
        SYSTEM_NAME visionOS
        SDK_NAME "xros"
        SDK_NAME_SIMULATOR "xrsimulator"
        DEFAULT_DEPLOYMENT_TARGET "2.0"
        SUPPORTS_ARM64E
    )
endif ()

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

    # CMake omits -target for swiftc during cross-compilation to Apple devices.
    if (PORT STREQUAL "IOS" OR PORT STREQUAL "XROS")
        if (PORT STREQUAL "IOS")
            set(_swift_os_name "ios")
        elseif (PORT STREQUAL "XROS")
            set(_swift_os_name "xros")
        endif ()

        if (NOT CMAKE_Swift_COMPILER_TARGET)
            if (CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
                set(CMAKE_Swift_COMPILER_TARGET "${CMAKE_OSX_ARCHITECTURES}-apple-${_swift_os_name}${CMAKE_OSX_DEPLOYMENT_TARGET}-simulator" CACHE STRING "Swift target triple" FORCE)
            else ()
                set(CMAKE_Swift_COMPILER_TARGET "${CMAKE_OSX_ARCHITECTURES}-apple-${_swift_os_name}${CMAKE_OSX_DEPLOYMENT_TARGET}" CACHE STRING "Swift target triple" FORCE)
            endif ()
        endif ()

        execute_process(COMMAND xcrun --show-sdk-platform-path
            OUTPUT_VARIABLE _platform_path
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if (_platform_path)
            execute_process(COMMAND find "${_platform_path}" -name swiftc -path "*/xctoolchain/*" -type f -print -quit
                OUTPUT_VARIABLE _platform_swiftc
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
            if (_platform_swiftc AND NOT _platform_swiftc STREQUAL CMAKE_Swift_COMPILER)
                set(CMAKE_Swift_COMPILER "${_platform_swiftc}" CACHE FILEPATH "Swift compiler" FORCE)
                message(STATUS "${PORT}: using platform-specific Swift compiler: ${_platform_swiftc}")
            endif ()
        endif ()
        unset(_platform_path)
        unset(_platform_swiftc)
        unset(_swift_os_name)
    endif ()

    enable_language(Swift)

    # -F causes Swift to find C++23 framework headers in implicit module builds.
    if (PORT STREQUAL "IOS" OR PORT STREQUAL "XROS")
        set(CMAKE_Swift_FRAMEWORK_SEARCH_FLAG "-Xcc -iquote")
    endif ()

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
