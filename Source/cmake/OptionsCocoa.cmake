include(WebKitVersion)

# Enable Objective-C / Objective-C++ so .m/.mm sources use the OBJC/OBJCXX
# compile rules and $<COMPILE_LANGUAGE:OBJC/OBJCXX> generator expressions
# match. Without this CMake compiles .mm as CXX, CMAKE_OBJCXX_FLAGS are
# ignored, and WEBKIT_ADD_PREFIX_HEADER produces no OBJCXX precompiled
# header for .mm sources.
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
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_SESSION PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_SESSION_COORDINATOR PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_SESSION_PLAYLIST PRIVATE ON)
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
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_STREAMING_IPC_IN_LOG_FORWARDING PRIVATE ON)
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

# SDK-gated features that PlatformEnableCocoa.h owns. Bug 312033.
WEBKIT_OPTION_OWNED_BY_PLATFORM_H(
    ENABLE_APPLE_PAY_AUTOMATIC_RELOAD_LINE_ITEM
    ENABLE_APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS
    ENABLE_APPLE_PAY_COUPON_CODE
    ENABLE_APPLE_PAY_DEFERRED_LINE_ITEM
    ENABLE_APPLE_PAY_DEFERRED_PAYMENTS
    ENABLE_APPLE_PAY_DELEGATED_REQUEST
    ENABLE_APPLE_PAY_DISBURSEMENTS
    ENABLE_APPLE_PAY_INSTALLMENTS
    ENABLE_APPLE_PAY_LATER_AVAILABILITY
    ENABLE_APPLE_PAY_MERCHANT_CATEGORY_CODE
    ENABLE_APPLE_PAY_MULTI_MERCHANT_PAYMENTS
    ENABLE_APPLE_PAY_PAYMENT_ORDER_DETAILS
    ENABLE_APPLE_PAY_RECURRING_LINE_ITEM
    ENABLE_APPLE_PAY_RECURRING_PAYMENTS
    ENABLE_APPLE_PAY_SELECTED_SHIPPING_METHOD
    ENABLE_APPLE_PAY_SHIPPING_CONTACT_EDITING_MODE
    ENABLE_APPLE_PAY_SHIPPING_METHOD_DATE_COMPONENTS_RANGE
    ENABLE_MEDIA_SOURCE_IN_WORKERS
    ENABLE_PREDEFINED_COLOR_SPACE_DISPLAY_P3
)

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_OFFSCREEN_CANVAS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_OFFSCREEN_CANVAS_IN_WORKERS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WK_WEB_EXTENSIONS PRIVATE ON)

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MINIBROWSER PUBLIC ON)

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_CONTROLS_CONTEXT_MENUS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MODEL_ELEMENT PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WRITING_TOOLS PRIVATE ON)

# rdar://177360289
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_BACK_FORWARD_LIST_SWIFT PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_IPC_TESTING_SWIFT PRIVATE ON)

# iOS-family feature overrides that differ from the macOS defaults above.
if (WEBKIT_SDK_IS_IOS_FAMILY)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_LAYOUT_TESTS PRIVATE ON)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_AUTOCAPITALIZE PRIVATE ON)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBKIT_TOUCH_CALLOUT_CSS_PROPERTY PRIVATE ON)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_TOUCH_EVENTS PRIVATE ${USE_APPLE_INTERNAL_SDK})
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_IOS_TOUCH_EVENTS PRIVATE ${USE_APPLE_INTERNAL_SDK})
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_IOS_GESTURE_EVENTS PRIVATE ${USE_APPLE_INTERNAL_SDK})
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER_TOUCH_INTERACTIONS PRIVATE ON)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_INSPECTOR_EXTENSIONS PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ACCESSIBILITY_ISOLATED_TREE PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CONTEXT_MENUS PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_BACK_FORWARD_LIST_SWIFT PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MINIBROWSER PUBLIC OFF)
    # Mac-only features absent on the iOS family.
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_AV1 PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CURSOR_VISIBILITY PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_SESSION_COORDINATOR PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_SESSION_PLAYLIST PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MOUSE_CURSOR_SCALE PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_PDF_HUD PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_PDF_PLUGIN PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_PDFKIT_PLUGIN PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_UNIFIED_PDF PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SERVICE_CONTROLS PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER_MOUSE_INTERACTIONS PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER_WHEEL_INTERACTIONS PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_IPC_TESTING_SWIFT PRIVATE OFF)
endif ()

WEBKIT_OPTION_END()

# -----------------------------------------------------------------------------
# Toolchain / SDK resolution
# -----------------------------------------------------------------------------
# WEBKIT_SDK_NAME, WEBKIT_PLATFORM_NAME, and the deployment target are derived
# centrally from the selected SDK in WebKitXcodeSDK.cmake (before project()).

# ---------------------------------------------------------------------------
# Shared Cocoa configuration.
# ---------------------------------------------------------------------------
set(SWIFT_REQUIRED ON)

# Configure module building
add_compile_options(
    "$<$<COMPILE_LANGUAGE:Swift>:-explicit-module-build>"
    # Needed for compatibility with modules in the (internal) SDK:
    # https://bugs.webkit.org/show_bug.cgi?id=312083
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fexperimental-bounds-safety-attributes>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fexperimental-late-parse-attributes>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-module-cache-path ${CMAKE_BINARY_DIR}/SwiftModuleCache>"
)
set_property(DIRECTORY "${CMAKE_BINARY_DIR}" APPEND PROPERTY
    ADDITIONAL_CLEAN_FILES "${CMAKE_BINARY_DIR}/SwiftModuleCache")

if (WEBKIT_SDK_IS_MACOS AND USE_APPLE_INTERNAL_SDK)
    set(WEBKIT_CODE_SIGN_IDENTITY "Safari Engineering")
    WEBKITADDITIONS_FIND_KEYCHAIN()
else ()
    set(WEBKIT_CODE_SIGN_IDENTITY "-")
endif ()

# Options controlling auxiliary-process entitlement generation. These are read
# by WEBKIT_GENERATE_ENTITLEMENTS (see WebKitEntitlements).
option(USE_RESTRICTED_ENTITLEMENTS "Emit private/restricted entitlements (requires the internal SDK)" ${USE_APPLE_INTERNAL_SDK})
option(USE_FATAL_EXCEPTIONS "Emit the fatal-exceptions entitlements" ON)
option(WEBCONTENT_SERVICE_NEEDS_XPC_DOMAIN_EXTENSION_ENTITLEMENT "Emit the WebContent XPC domain-extension entitlement" OFF)
option(USE_RELOCATABLE_WEBPUSHD "Build a relocatable webpushd (omits the fixed application-groups entitlements)" OFF)

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

# Replace the SDK's availability headers with stubs that defuse availability
# checks.
if (NOT USE_APPLE_INTERNAL_SDK)
    set(_availability_overlay_dir "${CMAKE_SOURCE_DIR}/WebKitLibraries/AvailabilityOverlay")
    set(_availability_overlay_yaml "${CMAKE_BINARY_DIR}/availability-overlay.yaml")
    file(WRITE "${_availability_overlay_yaml}.tmp"
    "{
      \"version\": 0,
      \"case-sensitive\": false,
      \"roots\": [
        {
          \"name\": \"${CMAKE_OSX_SYSROOT}/usr/include/os/availability.h\",
          \"type\": \"file\",
          \"external-contents\": \"${_availability_overlay_dir}/usr/include/os/availability.h\"
        },
        {
          \"name\": \"${CMAKE_OSX_SYSROOT}/usr/include/Availability.h\",
          \"type\": \"file\",
          \"external-contents\": \"${_availability_overlay_dir}/usr/include/Availability.h\"
        }
      ]
    }
    ")
    file(COPY_FILE "${_availability_overlay_yaml}.tmp" ${_availability_overlay_yaml} ONLY_IF_DIFFERENT)
    add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:SHELL:-ivfsoverlay ${_availability_overlay_yaml}>")
    add_compile_options("$<$<COMPILE_LANGUAGE:Swift>:SHELL:-vfsoverlay ${_availability_overlay_yaml}>")
    # For the Platform.h preprocess steps, which don't inherit compile options.
    set(WEBKIT_AVAILABILITY_VFS_OVERLAY_FILE "${_availability_overlay_yaml}")
    unset(_availability_overlay_dir)
    unset(_availability_overlay_yaml)
endif ()

if (NOT USE_APPLE_INTERNAL_SDK AND EXISTS "/usr/local/include/WebKitAdditions" AND NOT EXISTS "/usr/local/include/AppleFeatures/AppleFeatures.h")
    set(_apple_features_stub "${CMAKE_BINARY_DIR}/generated-stubs/AppleFeatures")
    file(MAKE_DIRECTORY "${_apple_features_stub}")
    file(CONFIGURE OUTPUT "${_apple_features_stub}/AppleFeatures.h" CONTENT
        "/* Auto-generated stub -- AppleFeatures not available in this SDK. */\n")
    add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-isystem${CMAKE_BINARY_DIR}/generated-stubs>")
    set(WEBKIT_GENERATED_STUBS_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated-stubs")
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

# Profile-Guided Optimization for the Cocoa ports. The Internal overlay resolves
# the WebKitAdditions PGO profiles and defines WEBKIT_TARGET_USE_PGO,
# which the relevant framework CMakeLists call.
if (USE_APPLE_INTERNAL_SDK)
    include(OptionsPGO)
endif ()

# Swiftc falls back to its built-in deployment target while clang honors
# CMAKE_OSX_DEPLOYMENT_TARGET; the mismatch produces an ld warning per object.
# The iOS-family Swift triple is set in the top-level CMakeLists.txt instead.
if (WEBKIT_SDK_IS_MACOS AND CMAKE_OSX_DEPLOYMENT_TARGET)
    list(LENGTH CMAKE_OSX_ARCHITECTURES _arch_count)
    if (_arch_count EQUAL 1)
        set(_swift_arch "${CMAKE_OSX_ARCHITECTURES}")
    elseif (_arch_count EQUAL 0)
        set(_swift_arch "${CMAKE_SYSTEM_PROCESSOR}")
    endif ()
    if (_swift_arch)
        set(CMAKE_Swift_COMPILER_TARGET "${_swift_arch}-apple-macosx${CMAKE_OSX_DEPLOYMENT_TARGET}" CACHE STRING "Swift target triple" FORCE)
        set(WEBKIT_SWIFT_MODULE_TRIPLE "${_swift_arch}-apple-macos" CACHE STRING "Swift module triple" FORCE)
    endif ()
    unset(_swift_arch)
    unset(_arch_count)
endif ()

# Fail loudly if an ASan build dir lost its CMakeCache.txt and reconfigured
# without ENABLE_SANITIZERS — otherwise the tree silently rebuilds uninstrumented.
get_filename_component(_bindir_name "${CMAKE_BINARY_DIR}" NAME)
if (_bindir_name STREQUAL "ASan" AND NOT ENABLE_SANITIZERS MATCHES "address")
    message(FATAL_ERROR
        "Build directory '${CMAKE_BINARY_DIR}' is an ASan tree but ENABLE_SANITIZERS='${ENABLE_SANITIZERS}'. "
        "CMakeCache.txt was likely deleted or never configured via the preset. Re-run: cmake --preset mac-asan")
endif ()
if (_bindir_name STREQUAL "TSan" AND NOT ENABLE_SANITIZERS MATCHES "thread")
    message(FATAL_ERROR
        "Build directory '${CMAKE_BINARY_DIR}' is a TSan tree but ENABLE_SANITIZERS='${ENABLE_SANITIZERS}'. "
        "CMakeCache.txt was likely deleted or never configured via the preset. Re-run: cmake --preset mac-tsan")
endif ()
unset(_bindir_name)

set(bmalloc_LIBRARY_TYPE OBJECT)
set(WTF_LIBRARY_TYPE OBJECT)
set(JavaScriptCore_LIBRARY_TYPE SHARED)
set(WebCore_LIBRARY_TYPE SHARED)
set(WebKit_LIBRARY_TYPE SHARED)

# Large unified-source bundles speed up the macOS build. The iOS family keeps
# the script default (8) so @no-unify-when(bundle<=8) sources (e.g. those that
# pull UIKit -> CoreServices private headers) stay non-unified and avoid clashes.
if (WEBKIT_SDK_IS_MACOS)
    set(WEBKIT_MAX_BUNDLE_SIZE 128)
endif ()

# iOS-family framework install names. macOS relies on defaults; the iOS family
# installs into the system framework locations so dylib ids resolve at runtime.
if (WEBKIT_SDK_IS_IOS_FAMILY)
    set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR ON)
    set(JavaScriptCore_INSTALL_NAME_DIR "/System/Library/Frameworks" CACHE STRING "" FORCE)
    set(WebKit_INSTALL_NAME_DIR "/System/Library/Frameworks" CACHE STRING "" FORCE)
    set(WebCore_INSTALL_NAME_DIR "/System/Library/PrivateFrameworks" CACHE STRING "" FORCE)
    set(WebGPU_INSTALL_NAME_DIR "/System/Library/PrivateFrameworks" CACHE STRING "" FORCE)
    set(WebKitLegacy_INSTALL_NAME_DIR "/System/Library/PrivateFrameworks" CACHE STRING "" FORCE)

    # Local dev builds are not part of the dyld shared cache. System-path install
    # names would otherwise mark some frameworks "shared-cache eligible" and the
    # linker rejects eligible->ineligible links between them. Opt every dylib out.
    add_link_options("-Wl,-not_for_dyld_shared_cache")

    # Define USE_APPLE_INTERNAL_SDK for the Swift Clang-module importer. Module
    # PCMs (e.g. WebKitLegacy consumed by WebKit's Swift) are built from
    # command-line flags only and don't see wtf/PlatformUse.h's definition, so
    # SPI headers like WebDownload.h would take their non-internal stub branch and
    # clash with the real SDK type (NSURLDownload in Foundation_Private).
    if (USE_APPLE_INTERNAL_SDK)
        add_compile_options("$<$<COMPILE_LANGUAGE:Swift>:-DUSE_APPLE_INTERNAL_SDK>")
        add_compile_options("$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -DUSE_APPLE_INTERNAL_SDK=1>")
    endif ()

    # Bare "-framework <name>" link flags (e.g. AuthKit) resolve private
    # frameworks from the SDK; add its search paths at link time. macOS resolves
    # its private frameworks via find_library(HINTS ...) so it doesn't need this.
    if (CMAKE_OSX_SYSROOT)
        add_link_options("-F${CMAKE_OSX_SYSROOT}/System/Library/Frameworks")
        add_link_options("-F${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks")
    endif ()
endif ()

if (CMAKE_OSX_SYSROOT)
    add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-F${CMAKE_BINARY_DIR};-iframework${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks>")
    add_compile_options("$<$<COMPILE_LANGUAGE:Swift>:-F${CMAKE_BINARY_DIR};-Fsystem;${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks>")
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

if (WEBKIT_ADDITIONS_INCLUDE_PATH AND EXISTS "${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/CMake/OptionsCocoa.cmake")
    message(STATUS "WebKitAdditions CMake: ${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/CMake/OptionsCocoa.cmake")
    include("${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/CMake/OptionsCocoa.cmake")
endif ()

set(MiniBrowser_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/MiniBrowser")

