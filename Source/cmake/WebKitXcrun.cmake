# Helpers for Apple SDK resolution via xcrun.
#
# Always pass --sdk to xcrun explicitly -- otherwise, toolchain and SDK
# are not guaranteed to match.

# Sets WEBKIT_SDK and WEBKIT_SDK_VERSION as cache vars. Cache (not
# PARENT_SCOPE) so values survive when invoked from inside another function
# (e.g. WEBKIT_RESOLVE_SDK_AND_TOOLCHAIN).
function(WEBKIT_RESOLVE_SDK)
    foreach (_sdk IN LISTS ARGN)
        execute_process(COMMAND xcrun --sdk ${_sdk} --show-sdk-version
            OUTPUT_VARIABLE _xcrun_sdk_version
            RESULT_VARIABLE _sdk_result
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if (_sdk_result EQUAL 0 AND _xcrun_sdk_version)
            set(WEBKIT_SDK "${_sdk}" CACHE STRING "Resolved Apple SDK name" FORCE)
            set(WEBKIT_SDK_VERSION "${_xcrun_sdk_version}" CACHE STRING "Resolved Apple SDK version" FORCE)
            return()
        endif ()
    endforeach ()
    message(FATAL_ERROR "xcrun could not locate any SDK in: ${ARGN}")
endfunction()

# Resolve a tool path via `xcrun <args>` (typically `-f <tool>`) and cache it.
# SDKROOT is in the env (set by WEBKIT_RESOLVE_SDK_AND_TOOLCHAIN), so xcrun's
# tool resolution follows the SDK's preferred toolchain. Caching prevents
# re-running xcrun on every reconfigure (matching find_program semantics).
function(WEBKIT_XCRUN OUTPUT_VAR)
    if (NOT DEFINED ENV{SDKROOT})
        message(FATAL_ERROR "WEBKIT_XCRUN called before SDKROOT was set in env (call WEBKIT_RESOLVE_SDK_AND_TOOLCHAIN first)")
    endif ()
    if (DEFINED CACHE{${OUTPUT_VAR}})
        return()
    endif ()
    execute_process(COMMAND xcrun ${ARGN}
        OUTPUT_VARIABLE _out
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    set(${OUTPUT_VAR} "${_out}" CACHE FILEPATH "Path resolved via xcrun")
endfunction()

# Resolve the Apple SDK, toolchain, deployment target, and architecture for
# the current PORT, before project() runs (project() runs ABI detection tests
# against CMAKE_OSX_SYSROOT, picks the compiler from PATH, and consumes
# CMAKE_OSX_DEPLOYMENT_TARGET).
#
# All outputs are CACHE so the function can call WEBKIT_RESOLVE_SDK without
# manual PARENT_SCOPE plumbing through nested function calls.
#
# Deployment-target policy (precedent: 305611@main): when the sysroot is a
# macOS SDK, use max(host macOS version, SDK version) so PlatformHave.h SPI
# guards keyed off __MAC_OS_X_VERSION_MIN_REQUIRED activate; on iOS we pin to
# a hardcoded floor.
function(WEBKIT_RESOLVE_SDK_AND_TOOLCHAIN)
    if (PORT STREQUAL "Mac")
        # FIXME: add macosx.internal here when the internal-SDK Mac build is green.
        WEBKIT_RESOLVE_SDK(macosx)
        set(_target_key "macosx")
    elseif (PORT STREQUAL "IOS")
        if (CMAKE_IOS_SIMULATOR OR CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
            WEBKIT_RESOLVE_SDK(iphonesimulator.internal iphonesimulator)
            set(_target_key "iphonesimulator")
        else ()
            WEBKIT_RESOLVE_SDK(iphoneos.internal iphoneos)
            set(_target_key "iphoneos")
        endif ()
    else ()
        return()
    endif ()

    string(REGEX MATCH "^[0-9]+\\.[0-9]+" _sdk_major_minor "${WEBKIT_SDK_VERSION}")

    if (NOT CMAKE_OSX_SYSROOT)
        execute_process(COMMAND xcrun --sdk ${WEBKIT_SDK} --show-sdk-path
            OUTPUT_VARIABLE _sysroot_path
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if (NOT _sysroot_path)
            message(FATAL_ERROR "Unable to determine SDK root path for ${WEBKIT_SDK}")
        endif ()
        set(CMAKE_OSX_SYSROOT "${_sysroot_path}" CACHE PATH "SDK path" FORCE)
    endif ()

    # Internal-SDK detection. The Internal SDK changes both arch policy
    # (arm64e on Mac) and SPI surface (compile flags in OptionsCocoa, Swift
    # define in OptionsIOS). Detect both name-form (auto-resolved
    # iphoneos.internal) and path-form (user-supplied iPhoneOS26.6.Internal.sdk).
    if (WEBKIT_SDK MATCHES "\\.internal$" OR CMAKE_OSX_SYSROOT MATCHES "\\.Internal\\.sdk$")
        set(USE_APPLE_INTERNAL_SDK ON CACHE BOOL "Apple-internal SDK is in use" FORCE)
    else ()
        set(USE_APPLE_INTERNAL_SDK OFF CACHE BOOL "Apple-internal SDK is in use" FORCE)
    endif ()

    # Steer all subsequent xcrun-based tool resolution to the SDK's preferred
    # toolchain by setting SDKROOT, and prepend the toolchain's bin to PATH so
    # CMake's find_program-based discovery (CMAKE_AR / CMAKE_RANLIB /
    # CMAKE_LINKER / etc.) lands on toolchain binaries instead of /usr/bin
    # stubs.
    set(ENV{SDKROOT} "${CMAKE_OSX_SYSROOT}")
    execute_process(COMMAND xcrun --sdk "${CMAKE_OSX_SYSROOT}" --show-toolchain-path
        OUTPUT_VARIABLE _toolchain_path
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if (_toolchain_path AND EXISTS "${_toolchain_path}/usr/bin")
        set(ENV{PATH} "${_toolchain_path}/usr/bin:$ENV{PATH}")
        message(STATUS "Using platform toolchain (prepending PATH): ${_toolchain_path}")
    endif ()

    if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        if (PORT STREQUAL "Mac")
            set(_target "${_sdk_major_minor}")
            execute_process(COMMAND sw_vers -productVersion
                OUTPUT_VARIABLE _host_version
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
            string(REGEX MATCH "^[0-9]+\\.[0-9]+" _host_major_minor "${_host_version}")
            if (_host_major_minor AND _host_major_minor VERSION_GREATER _target)
                set(_target "${_host_major_minor}")
            endif ()
            if (_target)
                set(CMAKE_OSX_DEPLOYMENT_TARGET "${_target}" CACHE STRING "Minimum deployment target" FORCE)
            endif ()
        elseif (PORT STREQUAL "IOS")
            set(CMAKE_OSX_DEPLOYMENT_TARGET "18.0" CACHE STRING "Minimum deployment target" FORCE)
        endif ()
    endif ()

    # Pick the default architecture from the SDK's own metadata
    # (SDKSettings.json's SupportedTargets[<target>].Archs), filtered by host
    # arch. The SDK lists archs in its own preferred order (e.g. internal
    # iphoneos lists arm64e first), so the filtered Archs[0] usually does the
    # right thing. Mac is the exception: macosx.internal still lists arm64
    # before arm64e, but WebKit's policy is to build internal Mac binaries as
    # arm64e -- preserve that as an explicit override.
    if (NOT CMAKE_OSX_ARCHITECTURES)
        execute_process(COMMAND uname -m
            OUTPUT_VARIABLE _host_arch
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        set(_pick "")
        set(_sdk_settings "${CMAKE_OSX_SYSROOT}/SDKSettings.json")
        if (EXISTS "${_sdk_settings}")
            file(READ "${_sdk_settings}" _sdk_settings_json)
            string(JSON _archs_count ERROR_VARIABLE _err
                LENGTH "${_sdk_settings_json}" SupportedTargets ${_target_key} Archs)
            if (NOT _err AND _archs_count GREATER 0)
                set(_compat_archs)
                math(EXPR _last "${_archs_count} - 1")
                foreach (_i RANGE 0 ${_last})
                    string(JSON _arch GET "${_sdk_settings_json}"
                        SupportedTargets ${_target_key} Archs ${_i})
                    if ((_host_arch STREQUAL "arm64" AND _arch MATCHES "^arm64") OR
                        (_host_arch STREQUAL "x86_64" AND _arch MATCHES "^x86_64"))
                        list(APPEND _compat_archs "${_arch}")
                    endif ()
                endforeach ()
                if (USE_APPLE_INTERNAL_SDK AND "arm64e" IN_LIST _compat_archs)
                    set(_pick "arm64e")
                elseif (_compat_archs)
                    list(GET _compat_archs 0 _pick)
                endif ()
            endif ()
        endif ()
        if (NOT _pick)
            message(FATAL_ERROR "Could not determine default architecture from ${_sdk_settings} for target '${_target_key}' on ${_host_arch} host. Set CMAKE_OSX_ARCHITECTURES explicitly.")
        endif ()
        set(CMAKE_OSX_ARCHITECTURES "${_pick}" CACHE STRING "Target architecture" FORCE)
    endif ()

    if (PORT STREQUAL "IOS")
        # CMake's cross-compile setup reads CMAKE_SYSTEM_NAME from the
        # directory scope, not the cache, so use PARENT_SCOPE here.
        set(CMAKE_SYSTEM_NAME iOS PARENT_SCOPE)
        if (NOT CMAKE_SYSTEM_PROCESSOR)
            set(CMAKE_SYSTEM_PROCESSOR "aarch64" CACHE STRING "Target processor" FORCE)
        endif ()
    endif ()
endfunction()
