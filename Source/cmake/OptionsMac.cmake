# FIXME: These should line up with versions in Configurations/Version.xcconfig.
# See Source/WebKitLegacy/PlatformWin.cmake for how WebKitVersion.h is generated.
set(WEBKIT_MAC_VERSION 615.1.1)
set(MACOSX_FRAMEWORK_BUNDLE_VERSION 615.1.1+)

# Enable Objective-C / Objective-C++ so .m/.mm sources use the OBJC/OBJCXX
# compile rules and $<COMPILE_LANGUAGE:OBJC/OBJCXX> generator expressions
# match. Without this CMake compiles .mm as CXX, CMAKE_OBJCXX_FLAGS are
# ignored, and the -include flag in ADD_WEBKIT_PREFIX_HEADERS never fires
# for .mm sources.
enable_language(OBJC OBJCXX)

WEBKIT_OPTION_BEGIN()
# Private options shared with other WebKit ports. Add options here only if
# we need a value different from the default defined in WebKitFeatures.cmake.

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_API_TESTS PRIVATE ON)

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MINIBROWSER PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLICATION_MANIFEST PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ASYNC_SCROLLING PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ATTACHMENT_ELEMENT PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_AV1 PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_AVF_CAPTIONS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CACHE_PARTITIONING PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CONTENT_EXTENSIONS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CONTENT_FILTERING PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CURSOR_VISIBILITY PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DARK_MODE_CSS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DATACUE_VALUE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DRAG_SUPPORT PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ENCRYPTED_MEDIA PRIVATE ON)
# FIXME: CSSPaintingAPI static_assert fires when this is ON. https://bugs.webkit.org/show_bug.cgi?id=312028
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_EXPERIMENTAL_FEATURES PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GAMEPAD PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GPU_PROCESS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_INSPECTOR_ALTERNATE_DISPATCHERS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_INSPECTOR_EXTENSIONS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_INSPECTOR_TELEMETRY PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_LEGACY_CUSTOM_PROTOCOL_MANAGER PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_LEGACY_ENCRYPTED_MEDIA PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_SOURCE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_RECORDER PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_STREAM PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEMORY_SAMPLER PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MOUSE_CURSOR_SCALE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_PAYMENT_REQUEST PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_PDF_HUD PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_PDF_PLUGIN PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_PDFKIT_PLUGIN PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_UNIFIED_PDF PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_PERIODIC_MEMORY_MONITOR PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_PICTURE_IN_PICTURE_API PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_POINTER_LOCK PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_RESOURCE_USAGE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SANDBOX_EXTENSIONS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SERVICE_CONTROLS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SHAREABLE_RESOURCE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SPEECH_SYNTHESIS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_TELEPHONE_NUMBER_DETECTION PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_TEXT_AUTOSIZING PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_VARIATION_FONTS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_VIDEO_PRESENTATION_MODE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER_KEYBOARD_INTERACTIONS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER_MOUSE_INTERACTIONS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER_WHEEL_INTERACTIONS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBXR PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_API_STATISTICS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_AUTHN PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_CODECS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_RTC PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WIRELESS_PLAYBACK_TARGET PRIVATE ON)

# Xcode enables this via FeatureDefines.xcconfig; defaults OFF in WebKitFeatures.cmake.
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ACCESSIBILITY_ISOLATED_TREE PRIVATE ON)

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBGPU PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_AVIF PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_JPEGXL PRIVATE OFF)
# Cocoa uses ColorSync, not Little CMS; CoreText handles WOFF2 natively.
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_LCMS PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_WOFF2 PRIVATE OFF)

# FIXME: These derived features are manually mirrored from PlatformEnableCocoa.h because
# IDL/CSS generators don't evaluate it. https://bugs.webkit.org/show_bug.cgi?id=312033
# PlatformEnableCocoa.h-derived: MEDIA_SOURCE && GPU_PROCESS
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_SOURCE_IN_WORKERS PRIVATE ON)

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_OFFSCREEN_CANVAS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_OFFSCREEN_CANVAS_IN_WORKERS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WK_WEB_EXTENSIONS PRIVATE ON)

# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_AUTOMATIC_RELOAD_SUMMARY_ITEM)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_AUTOMATIC_RELOAD_LINE_ITEM PRIVATE ON)
# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_AUTOMATIC_RELOAD_PAYMENTS)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS PRIVATE ON)
# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_COUPON_CODE)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_COUPON_CODE PRIVATE ON)
# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_DEFERRED_SUMMARY_ITEM)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_DEFERRED_LINE_ITEM PRIVATE ON)
# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_DEFERRED_PAYMENTS)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_DEFERRED_PAYMENTS PRIVATE ON)
# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_DISBURSEMENTS), Mac >= 15.0
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_DISBURSEMENTS PRIVATE ON)
# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_INSTALLMENTS)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_INSTALLMENTS PRIVATE ON)
# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_APPLE_PAY_LATER_AVAILABILITY), Mac >= 14.0
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_LATER_AVAILABILITY PRIVATE ON)
# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_MERCHANT_CATEGORY_CODE), Mac >= 15.0
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_MERCHANT_CATEGORY_CODE PRIVATE ON)
# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_MULTI_MERCHANT_PAYMENTS)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_MULTI_MERCHANT_PAYMENTS PRIVATE ON)
# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_PAYMENT_ORDER_DETAILS)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_PAYMENT_ORDER_DETAILS PRIVATE ON)
# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_RECURRING_SUMMARY_ITEM)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_RECURRING_LINE_ITEM PRIVATE ON)
# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_RECURRING_PAYMENTS)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_RECURRING_PAYMENTS PRIVATE ON)
# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_DEFAULT_SHIPPING_METHOD)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_SELECTED_SHIPPING_METHOD PRIVATE ON)
# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_SHIPPING_CONTACT_EDITING_MODE)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_SHIPPING_CONTACT_EDITING_MODE PRIVATE ON)
# PlatformEnableCocoa.h-derived: HAVE(PASSKIT_SHIPPING_METHOD_DATE_COMPONENTS_RANGE)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY_SHIPPING_METHOD_DATE_COMPONENTS_RANGE PRIVATE ON)

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MINIBROWSER PUBLIC ON)

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_CONTROLS_CONTEXT_MENUS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MODEL_ELEMENT PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WRITING_TOOLS PRIVATE ON)

WEBKIT_OPTION_END()

set(SWIFT_REQUIRED ON)

# Flatten output to match Xcode's layout for run-webkit-tests compatibility.
# webkitpy/port/driver.py expects WebKitTestRunner, ImageDiff, and frameworks in the same dir.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Don't relink dependents when a shared library they link against is rebuilt.
# Xcode's EAGER_LINKING achieves the same effect.
set(CMAKE_LINK_DEPENDS_NO_SHARED ON)

# Avoid collision between flat-output executables and DerivedSources dirs of the same name.
set(WebKitTestRunner_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKitTestRunner")
set(TestRunnerShared_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/TestRunnerShared")
set(MiniBrowser_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/MiniBrowser")

SET_AND_EXPOSE_TO_BUILD(USE_LIBWEBRTC TRUE)

# Forward WebGPU headers when Source/WebGPU is not built.
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
    # WebGPU framework copies headers here; WebCore needs <WebGPU/WebGPU.h>.
    include_directories(SYSTEM "${CMAKE_BINARY_DIR}/WebGPU/Headers")
endif ()

set(ENABLE_WEBKIT_LEGACY ON)
set(ENABLE_WEBKIT ON)

set(bmalloc_LIBRARY_TYPE OBJECT)
# bmalloc and WTF objects are absorbed into the JavaScriptCore dylib
# (mirrors Xcode's -force_load libWTF.a / libbmalloc.a). Downstream
# frameworks link JavaScriptCore only; WEBKIT_FRAMEWORK's LINKED_INTO
# tracking redirects WTF/bmalloc references there.
# PAL must be STATIC (not OBJECT) because it has Swift CryptoKit sources and
# OBJECT libraries don't produce .swiftmodule files.
set(WTF_LIBRARY_TYPE OBJECT)
set(JavaScriptCore_LIBRARY_TYPE SHARED)
set(PAL_LIBRARY_TYPE STATIC)
set(WebCore_LIBRARY_TYPE SHARED)

set(USE_ANGLE_EGL ON)

find_package(ICU 70.1 REQUIRED COMPONENTS data i18n uc)
find_package(LibXml2 2.8.0 REQUIRED)
find_package(LibXslt 1.1.13 REQUIRED)
find_package(Threads REQUIRED)

# -----------------------------------------------------------------------------
# SDK resolution
# -----------------------------------------------------------------------------
# Ask xcrun directly; CMake's default sysroot discovery can lag Xcode versions.
if (NOT CMAKE_OSX_SYSROOT)
    execute_process(COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE CMAKE_OSX_SYSROOT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif ()

# Deployment target must match SDK version -- PlatformHave.h SPI guards depend on
# __MAC_OS_X_VERSION_MIN_REQUIRED. Auto-bump if the preset floor is below the SDK.
execute_process(COMMAND xcrun --show-sdk-version
    OUTPUT_VARIABLE _sdk_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if (_sdk_version)
    string(REGEX MATCH "^[0-9]+\\.[0-9]+" _sdk_major_minor "${_sdk_version}")
    if (_sdk_major_minor AND (NOT CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS _sdk_major_minor))
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${_sdk_major_minor}" CACHE STRING "Minimum macOS version" FORCE)
        message(WARNING "Deployment target auto-set to SDK version: ${CMAKE_OSX_DEPLOYMENT_TARGET} (SPI header guards require this)")
    endif ()
endif ()

# CMake's Swift link rule passes -sdk but not -target, so swiftc falls back to
# its built-in default deployment target while clang honors
# CMAKE_OSX_DEPLOYMENT_TARGET. That mismatch produces an ld warning per object.
# Pass -target explicitly so the Swift driver and clang agree.
if (CMAKE_OSX_DEPLOYMENT_TARGET)
    list(LENGTH CMAKE_OSX_ARCHITECTURES _arch_count)
    if (_arch_count EQUAL 1)
        set(_swift_arch "${CMAKE_OSX_ARCHITECTURES}")
    elseif (_arch_count EQUAL 0)
        set(_swift_arch "${CMAKE_SYSTEM_PROCESSOR}")
    endif ()
    if (_swift_arch)
        string(APPEND CMAKE_Swift_FLAGS " -target ${_swift_arch}-apple-macosx${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif ()
endif ()

# -----------------------------------------------------------------------------
# SDK additions overlay (AvailabilityProhibitedInternal.h)
# -----------------------------------------------------------------------------
# SDK additions overlay: puts AvailabilityProhibitedInternal.h on the include path so
# API_UNAVAILABLE soft-linked classes compile. Xcode uses ADDITIONAL_SDKS for this.
# Look for <major>.0 first, then fall back to the highest available version.
string(REGEX MATCH "^[0-9]+" _sdk_major "${_sdk_version}")
set(_additions_candidates
    "${CMAKE_SOURCE_DIR}/WebKitLibraries/SDKs/macosx${_sdk_major}.0-additions.sdk/usr/local/include"
    "${CMAKE_SOURCE_DIR}/WebKitLibraries/SDKs/macosx${_sdk_major_minor}-additions.sdk/usr/local/include"
)
set(_additions_found FALSE)
foreach (_additions IN LISTS _additions_candidates)
    if (EXISTS "${_additions}/AvailabilityProhibitedInternal.h")
        # -isystem so SDK-addition headers are treated like real SDK headers.
        add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-isystem${_additions}>")
        message(STATUS "SDK additions overlay: ${_additions} (disables API_UNAVAILABLE for SPI code)")
        set(_additions_found TRUE)
        break ()
    endif ()
endforeach ()
# Fallback: scan for highest available macosx*-additions.sdk
if (NOT _additions_found)
    file(GLOB _all_additions "${CMAKE_SOURCE_DIR}/WebKitLibraries/SDKs/macosx*-additions.sdk")
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
endif ()
if (NOT _additions_found)
    message(WARNING "No SDK additions overlay found -- API_UNAVAILABLE classes (AVAudioSession etc.) will fail to compile")
endif ()
unset(_sdk_version)
unset(_sdk_major)
unset(_sdk_major_minor)
unset(_additions_candidates)
unset(_additions)

# Stub AppleFeatures.h when WebKitAdditions is present but the full Internal SDK is not.
if (EXISTS "/usr/local/include/WebKitAdditions" AND NOT EXISTS "/usr/local/include/AppleFeatures/AppleFeatures.h")
    set(_apple_features_stub "${CMAKE_BINARY_DIR}/generated-stubs/AppleFeatures")
    file(MAKE_DIRECTORY "${_apple_features_stub}")
    file(WRITE "${_apple_features_stub}/AppleFeatures.h"
        "/* Auto-generated stub -- AppleFeatures not available in this SDK. */\n")
    add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-isystem${CMAKE_BINARY_DIR}/generated-stubs>")
    message(STATUS "AppleFeatures stub generated (WebKitAdditions present, AppleFeatures SDK absent)")
    unset(_apple_features_stub)
endif ()
# Add PrivateFrameworks to framework search path (mirrors Base.xcconfig).
if (CMAKE_OSX_SYSROOT)
    add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks>")
endif ()

# FIXME: Audit and reduce these suppressions. https://bugs.webkit.org/show_bug.cgi?id=312034
# WebKitCompilerFlags.cmake enables -Wextra; Xcode doesn't. These match Xcode's defaults.
# Guard with NOT Swift -- swiftc doesn't understand Clang -W flags.
add_compile_options(
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-shadow-ivar>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-objc-property-synthesis>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-objc-missing-super-calls>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-objc-duplicate-category-definition>"
)
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-cast-align>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-undefined-inline>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-nonportable-include-path>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-unused-parameter>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-missing-field-initializers>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-null-conversion>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fobjc-weak>")

# Strip absolute paths from .o files so ccache hits are path-independent.
# Safe for debugging: use `lldb settings set target.source-map` to restore.
if (CMAKE_CXX_COMPILER_LAUNCHER OR CMAKE_C_COMPILER_LAUNCHER)
    add_compile_options(
        "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=.>"
        "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fdebug-prefix-map=${CMAKE_BINARY_DIR}=build>"
        "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.>"
    )
    # -frecord-command-line embeds absolute paths; use CMAKE_*_FLAGS for all languages.
    string(APPEND CMAKE_C_FLAGS " -fno-record-command-line")
    string(APPEND CMAKE_CXX_FLAGS " -fno-record-command-line")
    string(APPEND CMAKE_OBJC_FLAGS " -fno-record-command-line")
    string(APPEND CMAKE_OBJCXX_FLAGS " -fno-record-command-line")
endif ()

# The mac-asan preset's binaryDir is .../ASan. If CMakeCache.txt is lost and
# ninja's auto-reconfigure bootstraps a fresh cache, ENABLE_SANITIZERS silently
# defaults to empty and the whole tree rebuilds without instrumentation. Fail
# loudly instead so the recovery command is obvious.
get_filename_component(_bindir_name "${CMAKE_BINARY_DIR}" NAME)
if (_bindir_name STREQUAL "ASan" AND NOT ENABLE_SANITIZERS MATCHES "address")
    message(FATAL_ERROR
        "Build directory '${CMAKE_BINARY_DIR}' is an ASan tree but ENABLE_SANITIZERS='${ENABLE_SANITIZERS}'. "
        "CMakeCache.txt was likely deleted or never configured via the preset. Re-run: cmake --preset mac-asan")
endif ()

# Mac-specific sanitizer flags — mirror Configurations/Sanitizers.xcconfig.
if (ENABLE_SANITIZERS)
    # Prevents wtf/Compiler.h macros like ALWAYS_INLINE from interfering with
    # sanitizer instrumentation in optimized builds.
    add_compile_definitions(RELEASE_WITHOUT_OPTIMIZATIONS)

    # Disable ASan's "fake stack" (use-after-return detection) — it breaks JSC
    # garbage collection by keeping stack frames alive that the GC expects to be
    # dead. See Sanitizers.xcconfig.
    string(FIND "${ENABLE_SANITIZERS}" "address" _asan_pos)
    if (NOT _asan_pos EQUAL -1)
        add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fsanitize-address-use-after-return=never>")
        add_link_options("$<$<NOT:$<LINK_LANGUAGE:Swift>>:-fsanitize-address-use-after-return=never>")
    endif ()
endif ()

# Dead-strip unused symbols and dylibs. Mirrors Xcode's DEAD_CODE_STRIPPING,
# which is YES for release configs and NO for Debug.
add_link_options("$<$<NOT:$<CONFIG:Debug>>:-Wl,-dead_strip>")
add_link_options(-Wl,-dead_strip_dylibs)

if (CMAKE_GENERATOR STREQUAL "Ninja")
    set(CMAKE_CXX_ARCHIVE_CREATE "xcrun libtool -static -no_warning_for_no_symbols -o <TARGET> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_CREATE "xcrun libtool -static -no_warning_for_no_symbols -o <TARGET> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_APPEND "xcrun libtool -static -no_warning_for_no_symbols -o <TARGET> <TARGET> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_APPEND "xcrun libtool -static -no_warning_for_no_symbols -o <TARGET> <TARGET> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_FINISH true)
    set(CMAKE_C_ARCHIVE_FINISH true)
endif ()

# Suppress "has no symbols" warnings from Swift's internal libtool invocation (e.g. PAL).
set(CMAKE_STATIC_LINKER_FLAGS "-no_warning_for_no_symbols")

# Apple Silicon handles more concurrent links than the default pool size.
if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64" AND NOT DEFINED ENV{WEBKIT_NINJA_LINK_MAX})
    set(ENV{WEBKIT_NINJA_LINK_MAX} 6)
endif ()

# Symlink compile_commands.json to source root for clangd.
if (CMAKE_EXPORT_COMPILE_COMMANDS AND NOT EXISTS ${CMAKE_SOURCE_DIR}/compile_commands.json)
    file(CREATE_LINK
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
        SYMBOLIC)
endif ()

# Regenerate the Xcode debug wrapper on every (re)configure so its scheme paths
# and lldbinit source-map track this binary directory. Runs at configure time
# (not as a build target) to keep the no-op ninja build at zero actions.
if (EXISTS ${TOOLS_DIR}/Scripts/generate-cmake-xcode-project)
    execute_process(
        COMMAND ${Python_EXECUTABLE}
                ${TOOLS_DIR}/Scripts/generate-cmake-xcode-project
                ${CMAKE_BINARY_DIR}
        OUTPUT_QUIET)
endif ()
