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

    enable_language(Swift)

    # Match Xcode, which leaves SWIFT_TREAT_WARNINGS_AS_ERRORS unset: every new
    # Swift toolchain adds warnings, and a blanket -warnings-as-errors turns each
    # of them into a build break. Diagnostics that must stay fatal are listed
    # individually as -Werror groups in WEBKIT_SWIFT_FATAL_DIAGNOSTIC_FLAGS.
    set(CMAKE_Swift_COMPILE_OPTIONS_WARNING_AS_ERROR "")

    # -F causes Swift to find C++23 framework headers in implicit module builds.
    if (WEBKIT_SDK_IS_IOS_FAMILY)
        set(CMAKE_Swift_FRAMEWORK_SEARCH_FLAG "-Xcc -iquote")
    endif ()

    if (APPLE)
        WEBKIT_XCRUN(CMAKE_Swift_COMPILER --find swiftc)
    endif ()
    set(ORIGINAL_Swift_COMPILER "${CMAKE_Swift_COMPILER}")
    WEBKIT_QUERY_SWIFT_TARGET_INFO(WEBKIT_SWIFT_TARGET_INFO "${ORIGINAL_Swift_COMPILER}")

    if (WIN32)
        string(JSON _swift_runtime_library_path_count ERROR_VARIABLE _swift_runtime_library_paths_error
            LENGTH "${WEBKIT_SWIFT_TARGET_INFO}" "paths" "runtimeLibraryPaths")
        if (NOT _swift_runtime_library_paths_error STREQUAL "NOTFOUND")
            message(FATAL_ERROR "Could not read Swift runtime library paths: ${_swift_runtime_library_paths_error}")
        endif ()
        set(_swift_runtime_bin_directory "")
        if (_swift_runtime_library_path_count)
            math(EXPR _swift_runtime_library_path_last "${_swift_runtime_library_path_count} - 1")
            # Prefer the installed runtime over the toolchain-adjacent testing fallback.
            foreach (_index RANGE ${_swift_runtime_library_path_last} 0 -1)
                string(JSON _swift_runtime_library_path GET "${WEBKIT_SWIFT_TARGET_INFO}" "paths" "runtimeLibraryPaths" ${_index})
                if (EXISTS "${_swift_runtime_library_path}/swiftCore.dll")
                    set(_swift_runtime_bin_directory "${_swift_runtime_library_path}")
                    break ()
                endif ()
            endforeach ()
        endif ()
        if (NOT _swift_runtime_bin_directory)
            message(FATAL_ERROR "Could not find the Swift runtime bin directory")
        endif ()
        file(WRITE "${CMAKE_BINARY_DIR}/swift-runtime-bin-directory.txt.tmp" "${_swift_runtime_bin_directory}\n")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/swift-runtime-bin-directory.txt.tmp"
            "${CMAKE_BINARY_DIR}/swift-runtime-bin-directory.txt"
            COMMAND_ERROR_IS_FATAL ANY
        )
        file(REMOVE "${CMAKE_BINARY_DIR}/swift-runtime-bin-directory.txt.tmp")
        unset(_swift_runtime_library_path_count)
        unset(_swift_runtime_library_path_last)
        unset(_swift_runtime_library_paths_error)
        unset(_swift_runtime_library_path)
        unset(_swift_runtime_bin_directory)
        unset(_index)

        # Avoid cmd.exe due to command-line length limits.
        set(CMAKE_Swift_COMPILER "${Python_EXECUTABLE}")
        set(CMAKE_Swift_COMPILER_ARG1 "${CMAKE_SOURCE_DIR}/Tools/Scripts/swift/swiftc-wrapper.py")
    else ()
        set(CMAKE_Swift_COMPILER "${CMAKE_SOURCE_DIR}/Tools/Scripts/swift/swiftc-wrapper.py")
    endif ()
    add_compile_options($<$<COMPILE_LANGUAGE:Swift>:--original-swift-compiler=${ORIGINAL_Swift_COMPILER}>)
    add_link_options($<$<LINK_LANGUAGE:Swift>:--original-swift-compiler=${ORIGINAL_Swift_COMPILER}>)

    if (APPLE)
        # When Swift is used as a linker frontend, disable dSYM generation. The
        # objects being linked still contain debug info.
        add_link_options("$<$<LINK_LANGUAGE:Swift>:-gnone>")
    endif ()
    # The static archive rule (<CMAKE_Swift_CREATE_STATIC_LIBRARY>) uses neither
    # <FLAGS> nor link options, so inject the flag directly into the template.
    string(REPLACE "<CMAKE_Swift_COMPILER>"
        "<CMAKE_Swift_COMPILER> --original-swift-compiler=${ORIGINAL_Swift_COMPILER}"
        CMAKE_Swift_CREATE_STATIC_LIBRARY "${CMAKE_Swift_CREATE_STATIC_LIBRARY}")
elseif (WIN32)
    file(REMOVE "${CMAKE_BINARY_DIR}/swift-runtime-bin-directory.txt")
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
