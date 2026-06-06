# -----------------------------------------------------------------------------
# Determine CMake version and build type.
# -----------------------------------------------------------------------------
# NOTE: cmake_minimum_required() and project() *MUST* be the two first commands
# used, see https://cmake.org/cmake/help/v3.3/command/project.html -- the
# latter in particular handles loading a bunch of shared CMake definitions
# and loading the cross-compilation settings from CMAKE_TOOLCHAIN_FILE.

cmake_minimum_required(VERSION 3.20)

if (PORT STREQUAL "IOS")
    set(CMAKE_SYSTEM_NAME iOS)
    if (CMAKE_IOS_SIMULATOR)
        set(_sdk_name "iphonesimulator.internal")
        set(_sdk_name_fallback "iphonesimulator")
    else ()
        set(_sdk_name "iphoneos.internal")
        set(_sdk_name_fallback "iphoneos")
    endif ()
    execute_process(COMMAND xcrun --sdk ${_sdk_name} --show-sdk-path
        OUTPUT_VARIABLE _ios_sysroot
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _internal_sdk_result
        ERROR_QUIET)
    if (NOT _internal_sdk_result EQUAL 0 OR NOT _ios_sysroot)
        set(_ios_sysroot "")
        if (CMAKE_OSX_SYSROOT AND EXISTS "${CMAKE_OSX_SYSROOT}")
            set(_ios_sysroot "${CMAKE_OSX_SYSROOT}")
        elseif (DEFINED ENV{SDKROOT} AND EXISTS "$ENV{SDKROOT}" AND "$ENV{SDKROOT}" MATCHES "iPhone")
            set(_ios_sysroot "$ENV{SDKROOT}")
        else ()
            execute_process(COMMAND xcrun --sdk ${_sdk_name_fallback} --show-sdk-path
                OUTPUT_VARIABLE _ios_sysroot
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
        endif ()
    endif ()
    if (_ios_sysroot)
        set(CMAKE_OSX_SYSROOT "${_ios_sysroot}" CACHE PATH "iOS SDK path" FORCE)
    endif ()
    unset(_sdk_name)
    unset(_sdk_name_fallback)
    unset(_internal_sdk_result)
    unset(_ios_sysroot)

    if (NOT CMAKE_OSX_ARCHITECTURES)
        if (CMAKE_OSX_SYSROOT MATCHES "\\.Internal\\.sdk$" AND NOT CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
            execute_process(COMMAND uname -m
                OUTPUT_VARIABLE _host_arch
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            if (_host_arch STREQUAL "arm64")
                set(CMAKE_OSX_ARCHITECTURES "arm64e" CACHE STRING "Target architecture" FORCE)
                message(STATUS "iOS: arm64e enabled (internal device SDK detected)")
            endif ()
        else ()
            set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architecture" FORCE)
        endif ()
    endif ()

    if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "18.0" CACHE STRING "Minimum iOS version" FORCE)
    endif ()

    if (NOT CMAKE_SYSTEM_PROCESSOR)
        set(CMAKE_SYSTEM_PROCESSOR "aarch64" CACHE STRING "Target processor" FORCE)
    endif ()
endif ()

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT CMAKE_OSX_SYSROOT)
    # Set CMAKE_OSX_SYSROOT before the project() call below. project() runs ABI
    # detection tests whose results are cached; having the wrong sysroot there
    # would cause those cached results to refer to a different SDK than actual
    # builds use. OptionsMac.cmake (which sets this via WEBKIT_XCRUN) is loaded
    # only after project() via include(WebKitCommon), so it is too late.
    # WEBKIT_XCRUN is also unavailable here because WebKitXcrun.cmake is
    # included by OptionsMac.cmake.
    #
    # Prefer the internal SDK (matching Xcode) with a public fallback; keep in sync
    # with WEBKIT_RESOLVE_SDK(macosx.internal macosx) in OptionsMac.cmake.
    execute_process(COMMAND xcrun --sdk macosx.internal --show-sdk-path
        OUTPUT_VARIABLE _macos_sysroot
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _internal_sdk_result
        ERROR_QUIET)
    if (NOT _internal_sdk_result EQUAL 0 OR NOT _macos_sysroot)
        execute_process(COMMAND xcrun --sdk macosx --show-sdk-path
            OUTPUT_VARIABLE _macos_sysroot
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    endif ()
    if (_macos_sysroot)
        set(CMAKE_OSX_SYSROOT "${_macos_sysroot}" CACHE PATH "macOS SDK path" FORCE)
    endif ()
    unset(_macos_sysroot)
    unset(_internal_sdk_result)
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
    if (POLICY CMP0157)
        cmake_policy(SET CMP0157 NEW)
    endif ()

    # CMake omits -target for swiftc during iOS cross-compilation.
    if (PORT STREQUAL "IOS")
        if (CMAKE_IOS_SIMULATOR OR CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
            set(CMAKE_Swift_COMPILER_TARGET "${CMAKE_OSX_ARCHITECTURES}-apple-ios${CMAKE_OSX_DEPLOYMENT_TARGET}-simulator" CACHE STRING "Swift target triple" FORCE)
        else ()
            set(CMAKE_Swift_COMPILER_TARGET "${CMAKE_OSX_ARCHITECTURES}-apple-ios${CMAKE_OSX_DEPLOYMENT_TARGET}" CACHE STRING "Swift target triple" FORCE)
        endif ()
    endif ()

    enable_language(Swift)

    # -F causes Swift to find C++23 framework headers in implicit module builds.
    if (PORT STREQUAL "IOS")
        set(CMAKE_Swift_FRAMEWORK_SEARCH_FLAG "-Xcc -iquote")
    endif ()

    if (APPLE)
        WEBKIT_XCRUN(ORIGINAL_Swift_COMPILER --find swiftc)
    else ()
        set(ORIGINAL_Swift_COMPILER "${CMAKE_Swift_COMPILER}")
    endif ()
    set(CMAKE_Swift_COMPILER "${CMAKE_SOURCE_DIR}/Tools/Scripts/swift/swiftc-wrapper.sh")
    add_compile_options($<$<COMPILE_LANGUAGE:Swift>:--original-swift-compiler=${ORIGINAL_Swift_COMPILER}>)
    add_link_options($<$<LINK_LANGUAGE:Swift>:--original-swift-compiler=${ORIGINAL_Swift_COMPILER}>)
    # The static archive rule (<CMAKE_Swift_CREATE_STATIC_LIBRARY>) uses neither
    # <FLAGS> nor link options, so inject the flag directly into the template.
    string(REPLACE "<CMAKE_Swift_COMPILER>"
        "<CMAKE_Swift_COMPILER> --original-swift-compiler=${ORIGINAL_Swift_COMPILER}"
        CMAKE_Swift_CREATE_STATIC_LIBRARY "${CMAKE_Swift_CREATE_STATIC_LIBRARY}")
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
    if (EXISTS "${CMAKE_SOURCE_DIR}/PerformanceTests")
        add_subdirectory(PerformanceTests)
    endif ()
endif ()

# -----------------------------------------------------------------------------
# Print the features list last, for maximum visibility.
# -----------------------------------------------------------------------------
PRINT_WEBKIT_OPTIONS()
