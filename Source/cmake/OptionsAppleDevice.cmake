# OptionsAppleDevice.cmake — shared configuration for Apple device ports (iOS, visionOS).
#
# Before including this file, the port-specific Options file must set:
#   _sdk_prefix        — Full SDK prefix for xcrun and additions overlay lookup
#                        (e.g. "iphoneos", "iphonesimulator", "xros", "xrsimulator")
#   WEBKIT_PLATFORM_NAME — platform display name (e.g. "iPhoneOS", "XROS")

# Resolve SDK version — bump deployment target to match SDK for SPI header guards.
set(_sdk_name "${_sdk_prefix}.internal")
set(_sdk_name_fallback "${_sdk_prefix}")
execute_process(COMMAND xcrun --sdk ${_sdk_name} --show-sdk-version
    OUTPUT_VARIABLE _sdk_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _sdk_ver_result
    ERROR_QUIET)
if (NOT _sdk_ver_result EQUAL 0 OR NOT _sdk_version)
    execute_process(COMMAND xcrun --sdk ${_sdk_name_fallback} --show-sdk-version
        OUTPUT_VARIABLE _sdk_version
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif ()
unset(_sdk_name)
unset(_sdk_name_fallback)
unset(_sdk_ver_result)
if (_sdk_version)
    string(REGEX MATCH "^[0-9]+\\.[0-9]+" _sdk_major_minor "${_sdk_version}")
    if (_sdk_major_minor AND (NOT CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS _sdk_major_minor))
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${_sdk_major_minor}" CACHE STRING "Minimum deployment target" FORCE)
        message(WARNING "Deployment target auto-set to SDK version: ${CMAKE_OSX_DEPLOYMENT_TARGET} (SPI header guards require this)")
    endif ()
endif ()

include(OptionsCocoa)

enable_language(OBJC OBJCXX)

find_package(ZLIB REQUIRED)

set(WebKit_LIBRARY_TYPE SHARED)

set(bmalloc_LIBRARY_TYPE OBJECT)
set(WTF_LIBRARY_TYPE OBJECT)
set(JavaScriptCore_LIBRARY_TYPE SHARED)
set(WebCore_LIBRARY_TYPE SHARED)

set(_wka_compile_paths
    "${CMAKE_SOURCE_DIR}/../Internal/WebKit"
    "${CMAKE_SOURCE_DIR}/WebKitBuild/Debug/usr/local/include"
    "${CMAKE_SOURCE_DIR}/WebKitBuild/Release/usr/local/include"
)
set(_wka_found FALSE)
foreach (_wka_path IN LISTS _wka_compile_paths)
    if (EXISTS "${_wka_path}/WebKitAdditions" AND NOT _wka_found)
        add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-isystem${_wka_path}>")
        set(WEBKIT_ADDITIONS_COMPILE_PATH "${_wka_path}" CACHE PATH "WebKitAdditions compile include path" FORCE)
        message(STATUS "WebKitAdditions (compile): ${_wka_path}")
        set(_wka_found TRUE)
    endif ()
endforeach ()
if (NOT _wka_found)
    message(WARNING "WebKitAdditions not found -- SPI headers referencing Additions will fail")
endif ()
set(_wka_found FALSE)
set(_wka_cmake_paths
    "${CMAKE_SOURCE_DIR}/../Internal/WebKit"
    "${CMAKE_SOURCE_DIR}/WebKitBuild/Debug/usr/local/include"
    "${CMAKE_SOURCE_DIR}/WebKitBuild/Release/usr/local/include"
)
foreach (_wka_path IN LISTS _wka_cmake_paths)
    if (EXISTS "${_wka_path}/WebKitAdditions" AND NOT _wka_found)
        set(WEBKIT_ADDITIONS_INCLUDE_PATH "${_wka_path}" CACHE PATH "WebKitAdditions include path" FORCE)
        message(STATUS "WebKitAdditions (cmake): ${_wka_path}")
        set(_wka_found TRUE)
    endif ()
endforeach ()
unset(_wka_compile_paths)
unset(_wka_cmake_paths)
unset(_wka_found)

if (CMAKE_OSX_SYSROOT)
    add_link_options("-F${CMAKE_OSX_SYSROOT}/System/Library/Frameworks")
    add_link_options("-F${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks")
    add_link_options("-Wl,-not_for_dyld_shared_cache")
    add_compile_options("$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Fsystem ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks>")
    set(WEBKIT_PRIVATE_FRAMEWORKS_COMPILE_FLAG "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks>")
    add_compile_options(${WEBKIT_PRIVATE_FRAMEWORKS_COMPILE_FLAG})
    if (EXISTS "${CMAKE_OSX_SYSROOT}/usr/local/include")
        add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-isystem${CMAKE_OSX_SYSROOT}/usr/local/include>")
        add_compile_options("$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -isystem${CMAKE_OSX_SYSROOT}/usr/local/include>")
    endif ()
endif ()

# Export macros must be predefined for Swift explicit-module builds (no prefix headers).
add_compile_options(
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -DWEBCORE_EXPORT=>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -DWEBCORE_TESTSUPPORT_EXPORT=>"
)

if (CMAKE_OSX_SYSROOT MATCHES "\\.Internal\\.sdk$")
    add_compile_options("$<$<COMPILE_LANGUAGE:Swift>:-DUSE_APPLE_INTERNAL_SDK>")
endif ()

# VFS overlay: suppress TextInput_Private which uses ICU types without a
# proper module dependency.  The umbrella header drags in TI_NSStringExtras.h
# whose UChar references fail during explicit-module builds of Swift targets.
set(_textinput_private "${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks/TextInput.framework/Modules/module.private.modulemap")
if (EXISTS "${_textinput_private}")
    set(_empty_modulemap "${CMAKE_BINARY_DIR}/empty-module.private.modulemap")
    file(WRITE "${_empty_modulemap}" "")
    set(_vfs_overlay "${CMAKE_BINARY_DIR}/swift-vfs-overlay.yaml")
    file(WRITE "${_vfs_overlay}"
"{
  \"version\": 0,
  \"case-sensitive\": false,
  \"roots\": [
    {
      \"name\": \"${_textinput_private}\",
      \"type\": \"file\",
      \"external-contents\": \"${_empty_modulemap}\"
    }
  ]
}
")
    add_compile_options("$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -ivfsoverlay -Xcc ${_vfs_overlay}>")
endif ()

add_compile_options(
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-shorten-64-to-32>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-sign-conversion>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-conversion>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-float-conversion>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-shadow>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-overloaded-virtual>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-reserved-identifier>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-exit-time-destructors>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-implicit-fallthrough>"
)

add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-error=#warnings>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-objc-method-access>")

add_compile_options(
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fno-common>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fstrict-aliasing>"
)

if (ENABLE_SANITIZERS)
    add_compile_definitions(ENABLE_CONJECTURE_ASSERT=1)
endif ()

set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR ON)
set(JavaScriptCore_INSTALL_NAME_DIR "/System/Library/Frameworks" CACHE STRING "" FORCE)
set(WebKit_INSTALL_NAME_DIR "/System/Library/Frameworks" CACHE STRING "" FORCE)
set(WebCore_INSTALL_NAME_DIR "/System/Library/PrivateFrameworks" CACHE STRING "" FORCE)
set(WebGPU_INSTALL_NAME_DIR "/System/Library/PrivateFrameworks" CACHE STRING "" FORCE)
set(WebKitLegacy_INSTALL_NAME_DIR "/System/Library/PrivateFrameworks" CACHE STRING "" FORCE)
