# Darwin-specific options shared by Mac and JSCOnly ports.
# Included from OptionsCommon.cmake when APPLE is true.

# Set deployment target to match SDK version so SPI availability guards
# are satisfied when using the internal SDK.
if (CMAKE_OSX_SYSROOT AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    execute_process(COMMAND xcrun --sdk "${CMAKE_OSX_SYSROOT}" --show-sdk-version
        OUTPUT_VARIABLE _sdk_version
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if (_sdk_version)
        string(REGEX MATCH "^[0-9]+\\.[0-9]+" _sdk_major_minor "${_sdk_version}")
        if (_sdk_major_minor)
            set(CMAKE_OSX_DEPLOYMENT_TARGET "${_sdk_major_minor}" CACHE STRING "Minimum macOS version" FORCE)
        endif ()
    endif ()
endif ()

# Detect internal SDK.
set(WK_USING_INTERNAL_SDK OFF)
if (CMAKE_OSX_SYSROOT AND CMAKE_OSX_SYSROOT MATCHES "[Ii]nternal")
    set(WK_USING_INTERNAL_SDK ON)
endif ()

# Internal-SDK-gated defines (CommonBase.xcconfig:46-50)
if (WK_USING_INTERNAL_SDK)
    add_definitions(-DOS_UNFAIR_LOCK_INLINE=1)
    add_definitions(-D__ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES=0)
endif ()

# Always-on Darwin defines (JSC BaseTarget.xcconfig:36)
add_definitions(-D__STDC_WANT_LIB_EXT1__=1)

# Non-Production developer builds (DebugRelease.xcconfig:56)
if (DEVELOPER_MODE)
    add_definitions(-DENABLE_JSC_RESTRICTED_OPTIONS_BY_DEFAULT=1)
endif ()

# Debug-only (Base.xcconfig:126, Sanitizers.xcconfig:37)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DENABLE_CONJECTURE_ASSERT)
endif ()
