# AppleDeviceCrossCompile.cmake — shared pre-project() setup for Apple device ports.
#
# Called before project() to configure CMAKE_SYSTEM_NAME, CMAKE_OSX_SYSROOT,
# CMAKE_OSX_ARCHITECTURES, CMAKE_OSX_DEPLOYMENT_TARGET, and CMAKE_SYSTEM_PROCESSOR
# for cross-compiling to an Apple embedded OS (iOS, visionOS, etc.).
#
# Usage (before project()):
#   include(${CMAKE_CURRENT_SOURCE_DIR}/Source/cmake/AppleDeviceCrossCompile.cmake)
#   webkit_setup_apple_device_cross_compile(
#       SYSTEM_NAME iOS
#       SDK_NAME "iphoneos"
#       SDK_NAME_SIMULATOR "iphonesimulator"
#       DEFAULT_DEPLOYMENT_TARGET "18.0"
#       SUPPORTS_ARM64E
#   )

macro(webkit_setup_apple_device_cross_compile)
    cmake_parse_arguments(_ACC "SUPPORTS_ARM64E" "SYSTEM_NAME;SDK_NAME;SDK_NAME_SIMULATOR;DEFAULT_DEPLOYMENT_TARGET" "" ${ARGN})

    set(CMAKE_SYSTEM_NAME ${_ACC_SYSTEM_NAME})

    if (NOT CMAKE_OSX_SYSROOT)
        if (DEFINED ENV{SDKROOT} AND EXISTS "$ENV{SDKROOT}")
            set(CMAKE_OSX_SYSROOT "$ENV{SDKROOT}" CACHE PATH "${_ACC_SYSTEM_NAME} SDK path" FORCE)
        else ()
            if (CMAKE_IOS_SIMULATOR)
                set(_acc_sdk_name "${_ACC_SDK_NAME_SIMULATOR}.internal")
                set(_acc_sdk_fallback "${_ACC_SDK_NAME_SIMULATOR}")
            else ()
                set(_acc_sdk_name "${_ACC_SDK_NAME}.internal")
                set(_acc_sdk_fallback "${_ACC_SDK_NAME}")
            endif ()
            execute_process(COMMAND xcrun --sdk ${_acc_sdk_name} --show-sdk-path
                OUTPUT_VARIABLE _acc_sysroot
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE _acc_sdk_result
                ERROR_QUIET)
            if (NOT _acc_sdk_result EQUAL 0 OR NOT _acc_sysroot)
                execute_process(COMMAND xcrun --sdk ${_acc_sdk_fallback} --show-sdk-path
                    OUTPUT_VARIABLE _acc_sysroot
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET)
            endif ()
            if (_acc_sysroot)
                set(CMAKE_OSX_SYSROOT "${_acc_sysroot}" CACHE PATH "${_ACC_SYSTEM_NAME} SDK path" FORCE)
            endif ()
            unset(_acc_sdk_name)
            unset(_acc_sdk_fallback)
            unset(_acc_sysroot)
            unset(_acc_sdk_result)
        endif ()
    endif ()

    if (NOT CMAKE_OSX_ARCHITECTURES)
        if (_ACC_SUPPORTS_ARM64E AND CMAKE_OSX_SYSROOT MATCHES "\\.Internal\\.sdk$" AND NOT CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
            execute_process(COMMAND uname -m
                OUTPUT_VARIABLE _acc_host_arch
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            if (_acc_host_arch STREQUAL "arm64")
                set(CMAKE_OSX_ARCHITECTURES "arm64e" CACHE STRING "Target architecture" FORCE)
                message(STATUS "${_ACC_SYSTEM_NAME}: arm64e enabled (internal device SDK detected)")
            endif ()
            unset(_acc_host_arch)
        else ()
            set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architecture" FORCE)
        endif ()
    endif ()

    if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${_ACC_DEFAULT_DEPLOYMENT_TARGET}" CACHE STRING "Minimum ${_ACC_SYSTEM_NAME} version" FORCE)
    endif ()

    if (NOT CMAKE_SYSTEM_PROCESSOR)
        set(CMAKE_SYSTEM_PROCESSOR "aarch64" CACHE STRING "Target processor" FORCE)
    endif ()
endmacro()
