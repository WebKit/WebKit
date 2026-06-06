include(WebKitVersion)

# Enable OBJC/OBJCXX so .mm sources use the OBJCXX compile rules, flags, and PCH;
# otherwise CMake compiles them as CXX and the COMPILE_LANGUAGE expressions miss.
enable_language(OBJC OBJCXX)

WEBKIT_OPTION_BEGIN()
# Override only options whose WebKitFeatures.cmake default differs from what Mac needs.

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
# PlatformEnableCocoa.h enables all three on Mac (WebKitFeatures.cmake defaults them off).
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_SESSION PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_SESSION_COORDINATOR PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_SESSION_PLAYLIST PRIVATE ON)
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

# FIXME: mirrored from PlatformEnableCocoa.h because IDL/CSS generators don't evaluate it
# (MEDIA_SOURCE && GPU_PROCESS). https://bugs.webkit.org/show_bug.cgi?id=312033
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_SOURCE_IN_WORKERS PRIVATE ON)

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_OFFSCREEN_CANVAS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_OFFSCREEN_CANVAS_IN_WORKERS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WK_WEB_EXTENSIONS PRIVATE ON)

# PlatformEnableCocoa.h-derived: gates "display-p3"/"display-p3-linear" in IDL enums (PredefinedColorSpace, WebGL).
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_PREDEFINED_COLOR_SPACE_DISPLAY_P3 PRIVATE ON)

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

# rdar://177360289
SET_AND_EXPOSE_TO_BUILD(ENABLE_BACK_FORWARD_LIST_SWIFT ON)

# The C++<->Swift interop this needs isn't wired up in the CMake build yet; pin it off
# (the OpenSource default) so the WebKit Swift module builds. cmakeconfig.h is -include'd
# before wtf/Platform.h, so this pre-empts the SDK's feature-defines header.
SET_AND_EXPOSE_TO_BUILD(ENABLE_IPC_TESTING_SWIFT OFF)

# This build doesn't compile the Swift class WKTextSelectionController, so turn the
# capability off (PlatformHave.h defaults it on for Mac) and WebViewImpl.mm won't
# reference it. cmakeconfig.h is -include'd first, pre-empting the PlatformHave.h default.
SET_AND_EXPOSE_TO_BUILD(HAVE_WK_TEXT_SELECTION_CONTROLLER OFF)

# -----------------------------------------------------------------------------
# Toolchain / SDK resolution
# -----------------------------------------------------------------------------
include(WebKitXcrun)
# Prefer the internal SDK with a public fallback, matching the top-level
# CMakeLists.txt (the toolchain must match CMAKE_OSX_SYSROOT).
WEBKIT_RESOLVE_SDK(macosx.internal macosx)

# Resolve clang once and pin it for this build tree: faster, and avoids tearing
# between the resolved toolchain and the SDK path/version.
WEBKIT_XCRUN(_clang -f clang)
if (EXISTS "${_clang}")
    set(CMAKE_C_COMPILER "${_clang}")
    set(CMAKE_CXX_COMPILER "${_clang}++")
    set(CMAKE_OBJC_COMPILER "${_clang}")
    set(CMAKE_OBJCXX_COMPILER "${_clang}++")
endif ()

# Deployment target must match SDK version -- PlatformHave.h SPI guards depend on
# __MAC_OS_X_VERSION_MIN_REQUIRED. Auto-bump if the preset floor is below the SDK.
string(REGEX MATCH "^[0-9]+\\.[0-9]+" _sdk_major_minor "${_sdk_version}")
if (_sdk_major_minor AND (NOT CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS _sdk_major_minor))
    set(CMAKE_OSX_DEPLOYMENT_TARGET "${_sdk_major_minor}" CACHE STRING "Minimum macOS version" FORCE)
    message(WARNING "Deployment target auto-set to SDK version: ${CMAKE_OSX_DEPLOYMENT_TARGET} (SPI header guards require this)")
endif ()

set(_sdk_prefix "macosx")
set(WEBKIT_PLATFORM_NAME "MacOSX")

include(OptionsCocoa)

# Swiftc falls back to its built-in deployment target while clang honors
# CMAKE_OSX_DEPLOYMENT_TARGET; the mismatch produces an ld warning per object.
if (CMAKE_OSX_DEPLOYMENT_TARGET)
    list(LENGTH CMAKE_OSX_ARCHITECTURES _arch_count)
    if (_arch_count EQUAL 1)
        set(_swift_arch "${CMAKE_OSX_ARCHITECTURES}")
    elseif (_arch_count EQUAL 0)
        set(_swift_arch "${CMAKE_SYSTEM_PROCESSOR}")
    endif ()
    if (_swift_arch)
        set(CMAKE_Swift_COMPILER_TARGET "${_swift_arch}-apple-macosx${CMAKE_OSX_DEPLOYMENT_TARGET}" CACHE STRING "Swift target triple" FORCE)
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

set(WEBKIT_MAX_BUNDLE_SIZE 128)

# Framework search paths (mirrors Base.xcconfig / OptionsIOS.cmake): the internal SDK
# ships SPI as PrivateFrameworks, needed by clang and swiftc.
if (CMAKE_OSX_SYSROOT)
    add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks>")
    add_link_options("-F${CMAKE_OSX_SYSROOT}/System/Library/Frameworks")
    add_link_options("-F${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks")
    add_compile_options("$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Fsystem ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks>")
    set(WEBKIT_PRIVATE_FRAMEWORKS_COMPILE_FLAG "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks>")
endif ()

# Swift can't auto-derive USE(APPLE_INTERNAL_SDK) (no __has_include), so define it
# explicitly; the -Xcc arm keeps the Swift clang-importer consistent. Mirrors OptionsIOS.cmake.
if (USE_APPLE_INTERNAL_SDK)
    add_compile_options("$<$<COMPILE_LANGUAGE:Swift>:-DUSE_APPLE_INTERNAL_SDK>")
    add_compile_options("$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -DUSE_APPLE_INTERNAL_SDK>")
endif ()

# Mask TextInput_Private's modulemap with an empty one: its umbrella drags in ICU
# types that break explicit-module Swift builds (via AppKit_Private <- SwiftUI).
# macOS frameworks are versioned, so remap every modulemap path variant.
set(_textinput_fw "${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks/TextInput.framework")
set(_textinput_private "${_textinput_fw}/Modules/module.private.modulemap")
if (EXISTS "${_textinput_private}")
    set(_empty_modulemap "${CMAKE_BINARY_DIR}/empty-module.private.modulemap")
    file(WRITE "${_empty_modulemap}" "")

    get_filename_component(_textinput_private_real "${_textinput_private}" REALPATH)
    set(_textinput_private_current "${_textinput_fw}/Versions/Current/Modules/module.private.modulemap")
    set(_ti_paths "${_textinput_private}")
    if (NOT _textinput_private_current STREQUAL _textinput_private)
        list(APPEND _ti_paths "${_textinput_private_current}")
    endif ()
    if (NOT _textinput_private_real IN_LIST _ti_paths)
        list(APPEND _ti_paths "${_textinput_private_real}")
    endif ()

    set(_ti_objs "")
    foreach (_ti_path IN LISTS _ti_paths)
        list(APPEND _ti_objs "    { \"name\": \"${_ti_path}\", \"type\": \"file\", \"external-contents\": \"${_empty_modulemap}\" }")
    endforeach ()
    list(JOIN _ti_objs ",\n" _ti_roots)

    set(_vfs_overlay "${CMAKE_BINARY_DIR}/mac-swift-vfs-overlay.yaml")
    file(WRITE "${_vfs_overlay}" "{\n  \"version\": 0,\n  \"case-sensitive\": false,\n  \"roots\": [\n${_ti_roots}\n  ]\n}\n")
    add_compile_options("$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -ivfsoverlay -Xcc ${_vfs_overlay}>")
    unset(_textinput_private_real)
    unset(_textinput_private_current)
    unset(_ti_paths)
    unset(_ti_objs)
    unset(_ti_roots)
    unset(_vfs_overlay)
    unset(_empty_modulemap)
endif ()
unset(_textinput_private)
unset(_textinput_fw)

# The internal SDK ships additional WebKit SPI headers under usr/local/include.
# Add it for clang and the Swift clang importer. Mirrors OptionsIOS.cmake.
if (CMAKE_OSX_SYSROOT AND EXISTS "${CMAKE_OSX_SYSROOT}/usr/local/include")
    add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-isystem${CMAKE_OSX_SYSROOT}/usr/local/include>")
    add_compile_options("$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -isystem${CMAKE_OSX_SYSROOT}/usr/local/include>")
endif ()

# Regenerate the Xcode debug wrapper at configure time so its scheme/lldbinit paths
# track this build dir, without adding a build action.
if (EXISTS ${TOOLS_DIR}/Scripts/generate-cmake-xcode-project)
    execute_process(
        COMMAND ${Python_EXECUTABLE}
                ${TOOLS_DIR}/Scripts/generate-cmake-xcode-project
                ${CMAKE_BINARY_DIR}
        OUTPUT_QUIET)
endif ()

if (WEBKIT_ADDITIONS_INCLUDE_PATH AND EXISTS "${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/CMake/OptionsMac.cmake")
    message(STATUS "WebKitAdditions CMake: ${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/CMake/OptionsMac.cmake")
    include("${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/CMake/OptionsMac.cmake")
endif ()

set(MiniBrowser_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/MiniBrowser")
