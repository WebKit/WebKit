# Helpers and initialization logic for building with Apple SDKs.
#
# Always pass --sdk to xcrun explicitly -- otherwise, toolchain and SDK
# are not guaranteed to match.

# Sets CMAKE_OSX_SYSROOT, WEBKIT_SDK_NAME, and WEBKIT_SDK_VERSION in the caller's scope.
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
            string(JSON _sdk_canonical_name GET ${_sdk_settings} CanonicalName)
            string(JSON _platform_name GET ${_sdk_settings} DefaultProperties PLATFORM_NAME)

            message(STATUS "Xcode SDK: ${_sdk_canonical_name} at ${_sdk_path}")
            set(WEBKIT_SDK_VERSION "${_sdk_version}" PARENT_SCOPE)
            set(WEBKIT_SDK_NAME "${_platform_name}" PARENT_SCOPE)
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
                if (_platform_name STREQUAL "macosx")
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

            return ()
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
    if (DEFINED ${OUTPUT_VAR} AND EXISTS "${${OUTPUT_VAR}}")
        return ()
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
elseif (PORT STREQUAL "Mac" OR PORT STREQUAL "Cocoa" OR PORT STREQUAL "JSCOnly" OR NOT PORT)
    WEBKIT_RESOLVE_SDK(macosx.internal macosx)
elseif (PORT STREQUAL "IOS" AND CMAKE_IOS_SIMULATOR)
    WEBKIT_RESOLVE_SDK(iphonesimulator.internal iphonesimulator)
elseif (PORT STREQUAL "IOS")
    WEBKIT_RESOLVE_SDK(iphoneos.internal iphoneos)
else ()
    message(FATAL_ERROR "Building for an Apple platform without an SDK "
        "directory (CMAKE_OSX_SYSROOT) or supported PORT variable.")
endif ()

# One entry per SDK the Cocoa port builds for, matching SUPPORTED_PLATFORMS in the
# Xcode configurations. The fields, in order: the platform name Info.plists carry,
# the CMAKE_SYSTEM_NAME to cross-compile with, the OS as a target triple spells it,
# the OS as a Swift module triple spells it, and the clang deployment-target flag
# wtf/Platform.h is evaluated against. macOS is the one platform whose two triple
# spellings differ, and visionOS the one with no -m<os>-version-min flag.
set(_wk_sdk_macosx           "MacOSX"           "Darwin"   "macosx"  "macos"   "-mmacosx-version-min=@VERSION@")
set(_wk_sdk_iphoneos         "iPhoneOS"         "iOS"      "ios"     "ios"     "-miphoneos-version-min=@VERSION@")
set(_wk_sdk_iphonesimulator  "iPhoneSimulator"  "iOS"      "ios"     "ios"     "-mios-simulator-version-min=@VERSION@")
set(_wk_sdk_appletvos        "AppleTVOS"        "tvOS"     "tvos"    "tvos"    "-mtvos-version-min=@VERSION@")
set(_wk_sdk_appletvsimulator "AppleTVSimulator" "tvOS"     "tvos"    "tvos"    "-mtvos-simulator-version-min=@VERSION@")
set(_wk_sdk_watchos          "WatchOS"          "watchOS"  "watchos" "watchos" "-mwatchos-version-min=@VERSION@")
set(_wk_sdk_watchsimulator   "WatchSimulator"   "watchOS"  "watchos" "watchos" "-mwatchos-simulator-version-min=@VERSION@")
set(_wk_sdk_xros             "XROS"             "visionOS" "xros"    "xros"    "-mtargetos=xros@VERSION@")
set(_wk_sdk_xrsimulator      "XRSimulator"      "visionOS" "xros"    "xros"    "-mtargetos=xros@VERSION@-simulator")
set(_wk_sdk_macosx_iosmac    "MacOSX"           "Darwin"   "ios"     "ios"     "-mtargetos=ios@VERSION@-macabi")

set(WEBKIT_SDK_VARIANT "" CACHE STRING "SDK variant to build for, as Xcode's SDK_VARIANT names it (iosmac for Mac Catalyst)")
set(_wk_sdk_key ${WEBKIT_SDK_NAME})
set(WEBKIT_SDK_SYSTEM_PREFIX "")
if (WEBKIT_SDK_VARIANT)
    file(READ "${CMAKE_OSX_SYSROOT}/SDKSettings.json" _wk_sdk_settings)
    string(JSON _wk_sdk_variant_settings ERROR_VARIABLE _wk_sdk_variant_error
        GET ${_wk_sdk_settings} SupportedTargets ${WEBKIT_SDK_VARIANT})
    if (_wk_sdk_variant_error)
        message(FATAL_ERROR "The ${WEBKIT_SDK_NAME} SDK at ${CMAKE_OSX_SYSROOT} has no "
            "'${WEBKIT_SDK_VARIANT}' variant (WEBKIT_SDK_VARIANT).")
    endif ()
    string(JSON WEBKIT_SDK_SYSTEM_PREFIX ERROR_VARIABLE _wk_sdk_variant_error
        GET ${_wk_sdk_variant_settings} SystemPrefix)
    if (_wk_sdk_variant_error)
        set(WEBKIT_SDK_SYSTEM_PREFIX "")
    endif ()
    string(JSON _wk_sdk_variant_deployment_target GET ${_wk_sdk_variant_settings} DefaultDeploymentTarget)
    set(_wk_sdk_key ${WEBKIT_SDK_NAME}_${WEBKIT_SDK_VARIANT})
    unset(_wk_sdk_variant_error)
endif ()

# Platform detection, derived once from the resolved SDK's PLATFORM_NAME. All
# downstream CMake branches on these booleans instead of on PORT or an ad-hoc
# CMAKE_IOS_SIMULATOR flag, so the platform is chosen purely by the selected SDK.
if (NOT DEFINED _wk_sdk_${_wk_sdk_key})
    message(FATAL_ERROR "Unsupported Apple SDK '${_wk_sdk_key}'. Add a row for it to "
        "the SDK table in WebKitXcodeSDK.cmake so the platform name, the system name, the "
        "triple spellings, and the deployment-target flag that wtf/Platform.h is evaluated "
        "against are all derived for it.")
endif ()

set(_wk_sdk_entry ${_wk_sdk_${_wk_sdk_key}})
list(GET _wk_sdk_entry 0 WEBKIT_PLATFORM_NAME)
list(GET _wk_sdk_entry 1 WEBKIT_SDK_SYSTEM_NAME)
list(GET _wk_sdk_entry 2 WEBKIT_SDK_TARGET_OS)
list(GET _wk_sdk_entry 3 WEBKIT_SDK_MODULE_OS)
list(GET _wk_sdk_entry 4 _wk_sdk_min_version_flag)
unset(_wk_sdk_entry)

# Cocoa is macOS plus the iOS family, the same split wtf/PlatformLegacy.h makes
# between PLATFORM(MAC) and PLATFORM(IOS_FAMILY), so every SDK above that isn't
# macOS belongs to the family. Mac Catalyst is in the family too, but is not
# PLATFORM(IOS), which wtf/PlatformLegacy.h limits to iPhone and iPad.
set(WEBKIT_SDK_IS_MACOS OFF)
set(WEBKIT_SDK_IS_MACCATALYST OFF)
set(WEBKIT_SDK_IS_IOS_FAMILY OFF)
set(WEBKIT_SDK_IS_IOS OFF)
set(WEBKIT_SDK_IS_TVOS OFF)
set(WEBKIT_SDK_IS_WATCHOS OFF)
set(WEBKIT_SDK_IS_XROS OFF)
set(WEBKIT_SDK_IS_SIMULATOR OFF)
if (_wk_sdk_key STREQUAL "macosx_iosmac")
    set(WEBKIT_SDK_IS_MACCATALYST ON)
    set(WEBKIT_SDK_IS_IOS_FAMILY ON)
elseif (WEBKIT_SDK_NAME STREQUAL "macosx")
    set(WEBKIT_SDK_IS_MACOS ON)
else ()
    set(WEBKIT_SDK_IS_IOS_FAMILY ON)
endif ()
if (WEBKIT_SDK_TARGET_OS STREQUAL "ios" AND NOT WEBKIT_SDK_IS_MACCATALYST)
    set(WEBKIT_SDK_IS_IOS ON)
elseif (WEBKIT_SDK_TARGET_OS STREQUAL "tvos")
    set(WEBKIT_SDK_IS_TVOS ON)
elseif (WEBKIT_SDK_TARGET_OS STREQUAL "watchos")
    set(WEBKIT_SDK_IS_WATCHOS ON)
elseif (WEBKIT_SDK_TARGET_OS STREQUAL "xros")
    set(WEBKIT_SDK_IS_XROS ON)
endif ()
if (WEBKIT_SDK_NAME MATCHES "simulator$")
    set(WEBKIT_SDK_IS_SIMULATOR ON)
endif ()

# Xcode's SHALLOW_BUNDLE.
if (WEBKIT_SDK_IS_IOS_FAMILY AND NOT WEBKIT_SDK_IS_MACCATALYST)
    set(WEBKIT_SDK_SHALLOW_BUNDLE ON)
else ()
    set(WEBKIT_SDK_SHALLOW_BUNDLE OFF)
endif ()

# As Configurations/SDKVariant.xcconfig derives it.
if (WEBKIT_SDK_IS_MACCATALYST)
    set(WK_PLATFORM_NAME maccatalyst)
else ()
    set(WK_PLATFORM_NAME ${WEBKIT_SDK_NAME})
endif ()

# The UIDeviceFamily values the SDK supports, most significant first:
# 1=iPhone, 2=iPad, 3=Apple TV, 4=Apple Watch, 6=Mac, 7=Apple Vision.
function(_WEBKIT_SDK_DEVICE_FAMILIES _result)
    if (WEBKIT_SDK_IS_XROS)
        set(${_result} 7 PARENT_SCOPE)
    elseif (WEBKIT_SDK_IS_TVOS)
        set(${_result} 3 PARENT_SCOPE)
    elseif (WEBKIT_SDK_IS_WATCHOS)
        set(${_result} 4 PARENT_SCOPE)
    elseif (WEBKIT_SDK_IS_MACCATALYST)
        set(${_result} 2 6 PARENT_SCOPE)
    else ()
        set(${_result} 1 2 PARENT_SCOPE)
    endif ()
endfunction()

# UIDeviceFamily as Xcode emits it: TARGETED_DEVICE_FAMILY narrowed to the SDK, or the
# SDK's primary family when none is given.
function(WEBKIT_GET_DEVICE_FAMILY _result)
    _WEBKIT_SDK_DEVICE_FAMILIES(_supported)
    if (NOT ARGN)
        list(GET _supported 0 _narrowed)
    else ()
        set(_narrowed "")
        foreach (_family IN LISTS ARGN)
            if (_family IN_LIST _supported)
                list(APPEND _narrowed ${_family})
            endif ()
        endforeach ()
    endif ()
    set(${_result} ${_narrowed} PARENT_SCOPE)
endfunction()

# Building for macOS defaults uses the host system's deployment target. Other
# platforms deploy to the SDK version. Mac Catalyst deploys to the iOS version
# the SDK pairs with the macOS one.
if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    execute_process(COMMAND sw_vers -productVersion
        OUTPUT_VARIABLE _host_version
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY)
    if (WEBKIT_SDK_IS_MACCATALYST)
        set(_deployment_target ${_wk_sdk_variant_deployment_target})
        if (_host_version VERSION_LESS WEBKIT_SDK_VERSION)
            string(REGEX MATCH "^[0-9]+\\.[0-9]+" _host_version "${_host_version}")
            string(JSON _deployment_target ERROR_VARIABLE _wk_sdk_version_map_error
                GET ${_wk_sdk_settings} VersionMap macOS_iOSMac ${_host_version})
            if (_wk_sdk_version_map_error)
                message(FATAL_ERROR "The ${WEBKIT_SDK_NAME} SDK maps no Mac Catalyst version to "
                    "macOS ${_host_version}; set CMAKE_OSX_DEPLOYMENT_TARGET to an iOS version.")
            endif ()
        endif ()
    elseif (WEBKIT_SDK_IS_MACOS AND (_host_version VERSION_LESS WEBKIT_SDK_VERSION))
        set(_deployment_target ${_host_version})
    else ()
        set(_deployment_target ${WEBKIT_SDK_VERSION})
    endif ()

    # Host OS version could be three components, but deployment target is
    # always a two-component X.Y string.
    string(REGEX MATCH "^[0-9]+\\.[0-9]+" _deployment_target "${_deployment_target}")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "${_deployment_target}" CACHE STRING "Deployment target" FORCE)

    unset(_host_version)
    unset(_deployment_target)
    message(STATUS "Deployment target: ${WK_PLATFORM_NAME}${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif ()

if (CMAKE_OSX_DEPLOYMENT_TARGET)
    string(REPLACE "@VERSION@" "${CMAKE_OSX_DEPLOYMENT_TARGET}"
        WEBKIT_SDK_MIN_VERSION_FLAG "${_wk_sdk_min_version_flag}")
endif ()

if (WEBKIT_SDK_IS_MACCATALYST)
    string(JSON WEBKIT_MACCATALYST_MACOS_DEPLOYMENT_TARGET ERROR_VARIABLE _wk_sdk_version_map_error
        GET ${_wk_sdk_settings} VersionMap iOSMac_macOS ${CMAKE_OSX_DEPLOYMENT_TARGET})
    if (_wk_sdk_version_map_error)
        message(FATAL_ERROR "The ${WEBKIT_SDK_NAME} SDK maps no macOS version to Mac Catalyst "
            "${CMAKE_OSX_DEPLOYMENT_TARGET} (CMAKE_OSX_DEPLOYMENT_TARGET).")
    endif ()
    unset(_wk_sdk_version_map_error)
endif ()
unset(_wk_sdk_min_version_flag)

# Cross-compile setup for the iOS family (device or simulator). CMake doesn't
# infer CMAKE_SYSTEM_PROCESSOR once CMAKE_SYSTEM_NAME is set, so it has to be
# supplied explicitly. Both must be in place before project() runs. Device vs.
# simulator is inferred by CMake from the sysroot path, so only the system name
# differs from a macOS build here.
# Mac Catalyst stays a Darwin build, with the compilers pointed at the -macabi
# triple.
if (WEBKIT_SDK_IS_MACCATALYST)
    if (NOT CMAKE_OSX_ARCHITECTURES)
        execute_process(COMMAND uname -m OUTPUT_VARIABLE _wk_sdk_arch OUTPUT_STRIP_TRAILING_WHITESPACE)
        set(CMAKE_OSX_ARCHITECTURES "${_wk_sdk_arch}" CACHE STRING "Target architecture" FORCE)
    endif ()
    list(LENGTH CMAKE_OSX_ARCHITECTURES _wk_sdk_arch_count)
    if (_wk_sdk_arch_count GREATER 1)
        message(FATAL_ERROR "Mac Catalyst builds one architecture at a time; "
            "CMAKE_OSX_ARCHITECTURES is '${CMAKE_OSX_ARCHITECTURES}'.")
    endif ()
    foreach (_lang IN ITEMS C CXX OBJC OBJCXX ASM)
        set(CMAKE_${_lang}_COMPILER_TARGET "${CMAKE_OSX_ARCHITECTURES}-apple-ios${CMAKE_OSX_DEPLOYMENT_TARGET}-macabi")
    endforeach ()
    unset(_lang)
    unset(_wk_sdk_arch)
    unset(_wk_sdk_arch_count)
elseif (WEBKIT_SDK_IS_IOS_FAMILY)
    set(CMAKE_SYSTEM_NAME ${WEBKIT_SDK_SYSTEM_NAME})
    if (NOT CMAKE_SYSTEM_PROCESSOR)
        set(CMAKE_SYSTEM_PROCESSOR "aarch64" CACHE STRING "Target processor" FORCE)
    endif ()
endif ()
unset(_wk_sdk_key)
unset(_wk_sdk_settings)
unset(_wk_sdk_variant_settings)
unset(_wk_sdk_variant_deployment_target)

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
