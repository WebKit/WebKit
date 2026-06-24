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
