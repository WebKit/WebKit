set(_wka_found FALSE)
set(_wka_cmake_paths
    "${CMAKE_SOURCE_DIR}/../Internal/WebKit"
)
foreach (_wka_path IN LISTS _wka_cmake_paths)
    if (EXISTS "${_wka_path}/WebKitAdditions" AND NOT _wka_found)
        set(WEBKIT_ADDITIONS_INCLUDE_PATH "${_wka_path}" CACHE PATH "WebKitAdditions include path" FORCE)
        message(STATUS "WebKitAdditions (cmake): ${_wka_path}")
        set(_wka_found TRUE)
    endif ()
endforeach ()
unset(_wka_cmake_paths)
unset(_wka_found)

set(SWIFT_REQUIRED ON)

# FIXME: AV1 decoding requires dav1d which uses meson. https://bugs.webkit.org/show_bug.cgi?id=314011
SET_AND_EXPOSE_TO_BUILD(ENABLE_AV1 OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(WebKitTestRunner_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKitTestRunner")
set(TestRunnerShared_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/TestRunnerShared")

SET_AND_EXPOSE_TO_BUILD(USE_LIBWEBRTC TRUE)

if (NOT ENABLE_WEBGPU)
    set(_webgpu_fwd "${CMAKE_BINARY_DIR}/WebGPU-stub/WebGPU")
    file(MAKE_DIRECTORY "${_webgpu_fwd}")
    foreach (_h WebGPU.h WebGPUExt.h)
        if (NOT EXISTS "${_webgpu_fwd}/${_h}")
            file(CREATE_LINK "${CMAKE_SOURCE_DIR}/Source/WebGPU/WebGPU/${_h}" "${_webgpu_fwd}/${_h}" SYMBOLIC)
        endif ()
    endforeach ()
    include_directories(SYSTEM "${CMAKE_BINARY_DIR}/WebGPU-stub")
    unset(_webgpu_fwd)
    unset(_h)
else ()
    include_directories(SYSTEM "${CMAKE_BINARY_DIR}/WebGPU/Headers")
endif ()

set(ENABLE_WEBKIT_LEGACY ON)
set(ENABLE_WEBKIT ON)

# OBJECT libraries don't produce .swiftmodule files.
set(PAL_LIBRARY_TYPE STATIC)

set(CMAKE_LINK_DEPENDS_NO_SHARED ON)

set(USE_ANGLE_EGL ON)

find_package(SQLite3 REQUIRED)
if (NOT TARGET SQLite3::SQLite3)
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif ()

find_package(ICU 70.1 REQUIRED COMPONENTS data i18n uc)
find_package(LibXml2 2.8.0 REQUIRED)
find_package(LibXslt 1.1.13 REQUIRED)
find_package(Threads REQUIRED)

# Strip ${SDK}/usr/include from these imported targets; reachable via -isysroot.
foreach (_t SQLite::SQLite3 LibXml2::LibXml2 LibXslt::LibXslt LibXslt::LibExslt)
    if (TARGET ${_t})
        set_target_properties(${_t} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "")
    endif ()
endforeach ()

string(REGEX MATCH "^[0-9]+" _sdk_major "${_sdk_version}")
set(_additions_candidates
    "${CMAKE_SOURCE_DIR}/WebKitLibraries/SDKs/${_sdk_prefix}${_sdk_major}.0-additions.sdk/usr/local/include"
    "${CMAKE_SOURCE_DIR}/WebKitLibraries/SDKs/${_sdk_prefix}${_sdk_major_minor}-additions.sdk/usr/local/include"
)
set(_additions_found FALSE)
foreach (_additions IN LISTS _additions_candidates)
    if (EXISTS "${_additions}/AvailabilityProhibitedInternal.h")
        add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-isystem${_additions}>")
        message(STATUS "SDK additions overlay: ${_additions} (disables API_UNAVAILABLE for SPI code)")
        set(_additions_found TRUE)
        break ()
    endif ()
endforeach ()
if (NOT _additions_found)
    file(GLOB _all_additions "${CMAKE_SOURCE_DIR}/WebKitLibraries/SDKs/${_sdk_prefix}*-additions.sdk")
    list(SORT _all_additions)
    list(REVERSE _all_additions)
    foreach (_additions_sdk IN LISTS _all_additions)
        set(_additions "${_additions_sdk}/usr/local/include")
        if (EXISTS "${_additions}/AvailabilityProhibitedInternal.h")
            add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-isystem${_additions}>")
            message(STATUS "SDK additions overlay (fallback): ${_additions}")
            set(_additions_found TRUE)
            break ()
        endif ()
    endforeach ()
    unset(_all_additions)
    unset(_additions_sdk)
endif ()
if (NOT _additions_found)
    message(WARNING "No SDK additions overlay found -- API_UNAVAILABLE classes (AVAudioSession etc.) will fail to compile")
endif ()
unset(_sdk_version)
unset(_sdk_major)
unset(_sdk_major_minor)
unset(_sdk_prefix)
unset(_additions_candidates)
unset(_additions)
unset(_additions_found)

# An internal WebKitAdditions install provides headers that pull in internal-SDK-only headers. When
# it is present but the build targets a non-internal SDK, those headers are missing; generate minimal
# stubs for them so the build still succeeds, with the corresponding features disabled. Prefer the
# internal SDK (e.g. -DCMAKE_OSX_SYSROOT=macosx.internal), which provides the real headers.
# FIXME: Remove this once building against the internal SDK is fully supported.
if (EXISTS "/usr/local/include/WebKitAdditions"
    AND NOT EXISTS "/usr/local/include/AppleFeatures/AppleFeatures.h"
    AND NOT EXISTS "${CMAKE_OSX_SYSROOT}/usr/local/include/AppleFeatures/AppleFeatures.h")
    set(_stub_dir "${CMAKE_BINARY_DIR}/generated-stubs")

    file(CONFIGURE OUTPUT "${_stub_dir}/AppleFeatures/AppleFeatures.h" CONTENT
        "/* Auto-generated stub. */\n")

    file(CONFIGURE OUTPUT "${_stub_dir}/CoreAnalytics/CoreAnalytics.h" CONTENT
        "/* Auto-generated stub. */\n")

    file(CONFIGURE OUTPUT "${_stub_dir}/os/feature_private.h" CONTENT
"/* Auto-generated stub. */
#pragma once
#ifndef os_feature_enabled
#define os_feature_enabled(NAMESPACE, FEATURE) (0)
#endif
#ifndef os_feature_enabled_simple
#define os_feature_enabled_simple(NAMESPACE, FEATURE, DEFAULT_VALUE) (DEFAULT_VALUE)
#endif
")

    # Mirror the declarations from WTF's OSVariantSPI.h; the symbols resolve at link time.
    file(CONFIGURE OUTPUT "${_stub_dir}/os/variant_private.h" CONTENT
"/* Auto-generated stub. */
#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern \"C\" {
#endif
bool os_variant_allows_internal_security_policies(const char *);
bool os_variant_is_basesystem(const char *);
#ifdef __cplusplus
}
#endif
")

    # Forward to the public dyld header; stub the one notifier the additions header uses.
    file(CONFIGURE OUTPUT "${_stub_dir}/mach-o/dyld_priv.h" CONTENT
"/* Auto-generated stub. */
#pragma once
#include <mach-o/dyld.h>
#ifndef _dyld_register_dlsym_notifier
#define _dyld_register_dlsym_notifier(NOTIFIER) ((void)(NOTIFIER))
#endif
")

    # Apply to C-family and Swift; Swift needs it via -Xcc to reach the Clang importer.
    add_compile_options(
        "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-isystem${_stub_dir}>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -isystem${_stub_dir}>"
    )
    message(STATUS "Using stubs for internal SDK headers absent from the selected SDK")
    unset(_stub_dir)
endif ()

# FIXME: Audit and reduce these suppressions. https://bugs.webkit.org/show_bug.cgi?id=312034
add_compile_options(
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-shadow-ivar>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-objc-property-synthesis>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-objc-missing-super-calls>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-objc-duplicate-category-definition>"
)
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-cast-align>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-undefined-inline>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-nonportable-include-path>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-missing-field-initializers>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-null-conversion>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fobjc-weak>")

# Per-target ObjC visibility; global -fvisibility=hidden hides _OBJC_CLASS_$_ symbols.
add_compile_options(
    "$<$<COMPILE_LANGUAGE:C,CXX>:-fvisibility=hidden>"
    "$<$<COMPILE_LANGUAGE:C,CXX>:-fvisibility-inlines-hidden>"
)

if (CMAKE_OSX_SYSROOT MATCHES "\\.Internal\\.sdk$")
    add_compile_definitions(OS_UNFAIR_LOCK_INLINE=1)
endif ()

if (CMAKE_CXX_COMPILER_LAUNCHER OR CMAKE_C_COMPILER_LAUNCHER)
    string(APPEND CMAKE_C_FLAGS " -fno-record-command-line")
    string(APPEND CMAKE_CXX_FLAGS " -fno-record-command-line")
    string(APPEND CMAKE_OBJC_FLAGS " -fno-record-command-line")
    string(APPEND CMAKE_OBJCXX_FLAGS " -fno-record-command-line")
endif ()

option(RELATIVE_DEBUG_INFO "Write relative paths into DWARF debug info for portable build artifacts." OFF)

if (RELATIVE_DEBUG_INFO)
    add_compile_options(
        "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=.>"
        "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fdebug-prefix-map=${CMAKE_BINARY_DIR}=build>"
        "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.>"
    )
endif ()

if (ENABLE_SANITIZERS)
    add_compile_definitions(RELEASE_WITHOUT_OPTIMIZATIONS)

    string(FIND "${ENABLE_SANITIZERS}" "address" _asan_pos)
    if (NOT _asan_pos EQUAL -1)
        add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fsanitize-address-use-after-return=never>")
        add_link_options("$<$<NOT:$<LINK_LANGUAGE:Swift>>:-fsanitize-address-use-after-return=never>")
    endif ()

    # TSan: ld64 hits "too many personality routines for compact unwind" when
    # the TSan runtime adds its own personality. Mirror Sanitizers.xcconfig
    # (which scopes this to WebCore/WebKit/TestWebKitAPI; applying globally is
    # harmless and avoids per-target plumbing).
    string(FIND "${ENABLE_SANITIZERS}" "thread" _tsan_pos)
    if (NOT _tsan_pos EQUAL -1)
        add_link_options("-Wl,-no_compact_unwind")
    endif ()
endif ()

add_link_options("$<$<NOT:$<CONFIG:Debug>>:-Wl,-dead_strip>")
add_link_options(-Wl,-dead_strip_dylibs)

# Linked globally because PAL has Swift sources that get force-loaded into WebCore,
# and WebCore does not link JavaScriptCore directly on all platforms.
find_library(SWIFTCORE_LIBRARY swiftCore HINTS ${CMAKE_OSX_SYSROOT}/usr/lib/swift REQUIRED)
link_libraries(${SWIFTCORE_LIBRARY})

WEBKIT_XCRUN(_libtool -f libtool)
if (CMAKE_GENERATOR STREQUAL "Ninja")
    set(CMAKE_CXX_ARCHIVE_CREATE "${_libtool} -static -no_warning_for_no_symbols -o <TARGET> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_CREATE "${_libtool} -static -no_warning_for_no_symbols -o <TARGET> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_APPEND "${_libtool} -static -no_warning_for_no_symbols -o <TARGET> <TARGET> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_APPEND "${_libtool} -static -no_warning_for_no_symbols -o <TARGET> <TARGET> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_FINISH true)
    set(CMAKE_C_ARCHIVE_FINISH true)
endif ()

set(CMAKE_STATIC_LINKER_FLAGS "-no_warning_for_no_symbols")

if (CMAKE_EXPORT_COMPILE_COMMANDS AND NOT EXISTS ${CMAKE_SOURCE_DIR}/compile_commands.json)
    file(CREATE_LINK
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
        SYMBOLIC)
endif ()
