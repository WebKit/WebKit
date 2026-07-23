# Helpers and initialization logic for building with Apple SDKs.
#
# Always pass --sdk to xcrun explicitly -- otherwise, toolchain and SDK
# are not guaranteed to match.

# Sets CMAKE_OSX_SYSROOT and WEBKIT_SDK_VERSION in the caller's scope.
function(WEBKIT_RESOLVE_SDK)
    foreach (_sdk IN LISTS ARGN)
        execute_process(COMMAND xcrun --sdk ${_sdk} --show-sdk-path
            OUTPUT_VARIABLE _sdk_path
            RESULT_VARIABLE _sdk_result
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if (_sdk_result EQUAL 0 AND _sdk_path)
            file(READ "${_sdk_path}/SDKSettings.json" _sdk_settings)
            string(JSON _sdk_version GET ${_sdk_settings} Version)
            set(WEBKIT_SDK_VERSION "${_sdk_version}" PARENT_SCOPE)
            string(JSON _sdk_canonical_name GET ${_sdk_settings} CanonicalName)
            string(JSON _platform_name GET ${_sdk_settings} DefaultProperties PLATFORM_NAME)

            message(STATUS "Xcode SDK: ${_sdk_canonical_name} at ${_sdk_path}")
            set(CMAKE_OSX_SYSROOT "${_sdk_path}" CACHE PATH "" FORCE)
            if (_sdk_path MATCHES "\\.[Ii]nternal.sdk$")
                set(USE_APPLE_INTERNAL_SDK ON CACHE BOOL "" FORCE)
            else ()
                set(USE_APPLE_INTERNAL_SDK OFF CACHE BOOL "" FORCE)
            endif ()

            if (NOT CMAKE_OSX_ARCHITECTURES)
                # Build the supported-archs list from SDKSettings.json's
                # SupportedTargets.<platform>.Archs. Order works out
                # to match our preferred build defaults.
                string(JSON _archs_length LENGTH ${_sdk_settings}
                    SupportedTargets ${_platform_name} Archs)
                set(_supported_archs "")
                math(EXPR _archs_last "${_archs_length} - 1")
                foreach (_i RANGE 0 ${_archs_last})
                    string(JSON _arch_i GET ${_sdk_settings}
                        SupportedTargets ${_platform_name} Archs ${_i})
                    list(APPEND _supported_archs "${_arch_i}")
                endforeach ()

                # FIXME: This is different from what we do in the xcodebuild. For devices,
                # we default to building for all architectures supported by the device 
                # (for iOS devices, just arm64e). For simulators, we query the system for
                # created simulator targets and match their architecture (usually arm64).
                if (PORT STREQUAL "Mac" OR PORT STREQUAL "JSCOnly")
                    # When building for the host machine CMAKE_HOST_SYSTEM_PROCESSOR 
                    # isn't populated until `project()` is called, so consult `uname -m`.
                    # Only set _arch if SDK supports arm64e otherwise let `project()`
                    # configure.
                    execute_process(COMMAND uname -m
                        OUTPUT_VARIABLE _host_arch
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
                    if (_host_arch STREQUAL "arm64" AND USE_APPLE_INTERNAL_SDK
                            AND "arm64e" IN_LIST _supported_archs)
                        set(_arch "arm64e")
                        set(_arch_reason "internal SDK + arm64 host")
                    endif ()
                else ()
                    # When Cross-compiling, just trust the SDKSettings's ordering.
                    list(GET _supported_archs 0 _arch)
                    set(_arch_reason "first SDK-supported arch")
                endif ()

                if (DEFINED _arch)
                    set(CMAKE_OSX_ARCHITECTURES "${_arch}"
                        CACHE STRING "Target architecture" FORCE)
                    message(STATUS "Architecture: ${_arch} (${_arch_reason})")
                endif ()
            endif ()

            return()
        endif ()
    endforeach ()
    message(FATAL_ERROR "xcrun could not locate any SDK in: ${ARGN}")
endfunction()

# Runs `xcrun and capture stdout in OUTPUT_VAR
# (in the caller's scope).
function(WEBKIT_XCRUN OUTPUT_VAR)
    if (NOT CMAKE_OSX_SYSROOT)
        message(FATAL_ERROR "WEBKIT_XCRUN called before WEBKIT_RESOLVE_SDK")
    endif ()
    execute_process(COMMAND xcrun ${ARGN}
        OUTPUT_VARIABLE _out
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    set(${OUTPUT_VAR} "${_out}" PARENT_SCOPE)
endfunction()

function(WEBKIT_RESOLVE_TOOL OUTPUT_VAR _tool)
    if (DEFINED ${OUTPUT_VAR})
        return()
    endif ()
    WEBKIT_XCRUN(_path -f ${_tool})
    if (NOT EXISTS "${_path}")
        message(SEND_ERROR "Cannot find ${_tool} in the active SDK and "
            "toolchain (${CMAKE_OSX_SYSROOT})")
    else ()
        set(${OUTPUT_VAR} "${_path}" CACHE STRING "" FORCE)
    endif ()
endfunction()

# ----------------------------------------------------------------------------
# Initialization logic. Sets CMAKE_OSX_SYSROOT if needed, and pins compiler
# tools from the SDK.
# ----------------------------------------------------------------------------

if (CMAKE_OSX_SYSROOT)
    WEBKIT_RESOLVE_SDK(${CMAKE_OSX_SYSROOT})
elseif (PORT STREQUAL "Mac" OR PORT STREQUAL "JSCOnly" OR NOT PORT)
    WEBKIT_RESOLVE_SDK(macosx.internal macosx)
elseif (PORT STREQUAL "IOS" AND CMAKE_IOS_SIMULATOR)
    WEBKIT_RESOLVE_SDK(iphonesimulator.internal iphonesimulator)
elseif (PORT STREQUAL "IOS" AND NOT CMAKE_IOS_SIMULATOR)
    WEBKIT_RESOLVE_SDK(iphoneos.internal iphoneos)
else ()
    message(FATAL_ERROR "Building for an Apple platform without an SDK "
        "directory (CMAKE_OSX_SYSROOT) or supported PORT variable.")
endif ()

# Cross-compile setup for iOS. CMake doesn't infer CMAKE_SYSTEM_PROCESSOR once
# CMAKE_SYSTEM_NAME is set, so it has to be supplied explicitly. Both must be
# in place before project() runs.
if (PORT STREQUAL "IOS")
    set(CMAKE_SYSTEM_NAME iOS)
    if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        string(REGEX MATCH "^[0-9]+\\.[0-9]+" _ios_deployment_target "${WEBKIT_SDK_VERSION}")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${_ios_deployment_target}" CACHE STRING "Minimum iOS version" FORCE)
    endif ()
    if (NOT CMAKE_SYSTEM_PROCESSOR)
        set(CMAKE_SYSTEM_PROCESSOR "aarch64" CACHE STRING "Target processor" FORCE)
    endif ()
endif ()

# Subsequent use of `xcrun` or any of the system Xcode and BSD tools will
# use the selected SDK and toolchain.
set(ENV{SDKROOT} ${CMAKE_OSX_SYSROOT})

WEBKIT_RESOLVE_TOOL(CMAKE_C_COMPILER "clang")
WEBKIT_RESOLVE_TOOL(CMAKE_ASM_COMPILER "clang")
WEBKIT_RESOLVE_TOOL(CMAKE_CXX_COMPILER "clang++")
WEBKIT_RESOLVE_TOOL(CMAKE_OBJC_COMPILER "clang")
WEBKIT_RESOLVE_TOOL(CMAKE_OBJCXX_COMPILER "clang++")
WEBKIT_RESOLVE_TOOL(CMAKE_Swift_COMPILER "swiftc")
WEBKIT_RESOLVE_TOOL(CMAKE_INSTALL_NAME_TOOL "install_name_tool")
WEBKIT_RESOLVE_TOOL(CMAKE_LINKER "ld")
# FIXME: Move these to proper find modules, as they are not part of any CMake
# language.
WEBKIT_RESOLVE_TOOL(GPERF_EXECUTABLE "gperf")
WEBKIT_RESOLVE_TOOL(Mig_EXECUTABLE "mig")
