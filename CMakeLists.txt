# -----------------------------------------------------------------------------
# Determine CMake version and build type.
# -----------------------------------------------------------------------------
# NOTE: cmake_minimum_required() and project() *MUST* be the two first commands
# used, see https://cmake.org/cmake/help/v3.3/command/project.html -- the
# latter in particular handles loading a bunch of shared CMake definitions
# and loading the cross-compilation settings from CMAKE_TOOLCHAIN_FILE.

cmake_minimum_required(VERSION 3.20)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/Source/cmake")

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    # Resolve the SDK and pin tools before the call to project().
    include(WebKitXcodeSDK)
endif ()

project(WebKit LANGUAGES C CXX)

# -----------------------------------------------------------------------------
# Common configuration
#------------------------------------------------------------------------------
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

    # Derive the Swift triple from the C compiler's target when cross-compiling
    # (Yocto bakes --target=<sys> into CC, not CMAKE_C_COMPILER_TARGET).
    if ((PORT STREQUAL "WPE" OR PORT STREQUAL "GTK") AND CMAKE_CROSSCOMPILING AND NOT CMAKE_Swift_COMPILER_TARGET)
        set(_swift_target "")
        if (CMAKE_C_COMPILER_TARGET)
            set(_swift_target "${CMAKE_C_COMPILER_TARGET}")
        elseif (CMAKE_C_COMPILER_ARG1 MATCHES "--target=([^ \t]+)")
            set(_swift_target "${CMAKE_MATCH_1}")
        endif ()
        if (_swift_target)
            set(CMAKE_Swift_COMPILER_TARGET "${_swift_target}" CACHE STRING "Swift target triple" FORCE)
        elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(CMAKE_Swift_COMPILER_TARGET "aarch64-unknown-linux-gnu" CACHE STRING "Swift target triple" FORCE)
        elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^armv7" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
            set(CMAKE_Swift_COMPILER_TARGET "armv7-unknown-linux-gnueabihf" CACHE STRING "Swift target triple" FORCE)
        else ()
            message(FATAL_ERROR
                "Cannot derive a Swift target triple for "
                "CMAKE_SYSTEM_PROCESSOR='${CMAKE_SYSTEM_PROCESSOR}'. "
                "Pass -DCMAKE_Swift_COMPILER_TARGET=<triple> explicitly.")
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
    # CMake drops the bare -fuse-ld driver flag when forwarding USE_LD_LLD's
    # CMAKE_*_LINKER_FLAGS to swiftc (only -Wl,/LINKER: items survive), so Swift
    # links fall back to the default ld and miss lld's tighter layout
    # (webkit.org/b/317573). Re-add it. The swiftc-wrapper maps -fuse-ld=* through,
    # and lld is target-agnostic, so this works for cross-builds too.
    if (USE_LD_LLD)
        add_link_options($<$<LINK_LANGUAGE:Swift>:-fuse-ld=lld>)
    endif ()
    # The static archive rule (<CMAKE_Swift_CREATE_STATIC_LIBRARY>) uses neither
    # <FLAGS> nor link options, so inject the flag directly into the template.
    string(REPLACE "<CMAKE_Swift_COMPILER>"
        "<CMAKE_Swift_COMPILER> --original-swift-compiler=${ORIGINAL_Swift_COMPILER}"
        CMAKE_Swift_CREATE_STATIC_LIBRARY "${CMAKE_Swift_CREATE_STATIC_LIBRARY}")
else ()
    # The Yocto cross-toolchain env injects -DCMAKE_Swift_FLAGS_INIT whenever the
    # SDK ships swiftc, but only enable_language(Swift) above consumes it. Without
    # Swift, touch it here so CMake doesn't warn that it went unused.
    set(_unused_swift_flags_init "${CMAKE_Swift_FLAGS_INIT}")
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
