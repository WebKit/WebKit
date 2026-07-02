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

function(WEBKIT_ADD_SDK_IMPORTED_LIBRARY _target _library)
    if (NOT TARGET ${_target})
        add_library(${_target} UNKNOWN IMPORTED)
        set_target_properties(${_target} PROPERTIES IMPORTED_LOCATION "${CMAKE_OSX_SYSROOT}/usr/lib/${_library}")
    endif ()
endfunction()

WEBKIT_ADD_SDK_IMPORTED_LIBRARY(SQLite::SQLite3 libsqlite3.tbd)
WEBKIT_ADD_SDK_IMPORTED_LIBRARY(LibXml2::LibXml2 libxml2.tbd)
WEBKIT_ADD_SDK_IMPORTED_LIBRARY(LibXslt::LibXslt libxslt.tbd)
WEBKIT_ADD_SDK_IMPORTED_LIBRARY(LibXslt::LibExslt libexslt.tbd)
WEBKIT_ADD_SDK_IMPORTED_LIBRARY(ZLIB::ZLIB libz.tbd)
if (NOT TARGET SQLite3::SQLite3)
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif ()

find_package(ICU 70.1 REQUIRED COMPONENTS data i18n uc)
set(CMAKE_HAVE_PTHREAD_H 1 CACHE INTERNAL "")
set(CMAKE_HAVE_LIBC_PTHREAD 1 CACHE INTERNAL "")
find_package(Threads REQUIRED)

string(REGEX MATCH "^[0-9]+" _sdk_major "${_sdk_version}")
set(_additions_candidates
    "${CMAKE_SOURCE_DIR}/WebKitLibraries/SDKs/${WEBKIT_SDK_NAME}${_sdk_major}.0-additions.sdk/usr/local/include"
    "${CMAKE_SOURCE_DIR}/WebKitLibraries/SDKs/${WEBKIT_SDK_NAME}${_sdk_major_minor}-additions.sdk/usr/local/include"
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
    file(GLOB _all_additions "${CMAKE_SOURCE_DIR}/WebKitLibraries/SDKs/${WEBKIT_SDK_NAME}*-additions.sdk")
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
unset(_additions_candidates)
unset(_additions)
unset(_additions_found)

if (EXISTS "/usr/local/include/WebKitAdditions" AND NOT EXISTS "/usr/local/include/AppleFeatures/AppleFeatures.h")
    set(_apple_features_stub "${CMAKE_BINARY_DIR}/generated-stubs/AppleFeatures")
    file(MAKE_DIRECTORY "${_apple_features_stub}")
    file(CONFIGURE OUTPUT "${_apple_features_stub}/AppleFeatures.h" CONTENT
        "/* Auto-generated stub -- AppleFeatures not available in this SDK. */\n")
    add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-isystem${CMAKE_BINARY_DIR}/generated-stubs>")
    message(STATUS "AppleFeatures stub generated (WebKitAdditions present, AppleFeatures SDK absent)")
    unset(_apple_features_stub)
endif ()

# FIXME: Audit and reduce these suppressions. https://bugs.webkit.org/show_bug.cgi?id=312034
add_compile_options(
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-Wno-shadow-ivar>"
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-Wno-objc-property-synthesis>"
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-Wno-objc-missing-super-calls>"
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-Wno-objc-duplicate-category-definition>"
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-Wno-objc-signed-char-bool-implicit-float-conversion>"
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-Wno-unused-parameter>"
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
        "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=.>"
        "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-fdebug-prefix-map=${CMAKE_BINARY_DIR}=build>"
        "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.>"
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
