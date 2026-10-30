include(WebKitVersion)

# Enable Objective-C / Objective-C++ so .m/.mm sources use the OBJC/OBJCXX
# compile rules and $<COMPILE_LANGUAGE:OBJC/OBJCXX> generator expressions
# match. Without this CMake compiles .mm as CXX, CMAKE_OBJCXX_FLAGS are
# ignored, and WEBKIT_ADD_PREFIX_HEADER produces no OBJCXX precompiled
# header for .mm sources.
WEBKIT_ENABLE_LANGUAGE(OBJC OBJCXX)

WEBKIT_OPTION_BEGIN()

set(ENABLE_UNSAFE_BUFFER_USAGE_WARNING ON)
set(ENABLE_THREAD_SAFETY_WARNING ON)

# Private options shared with other WebKit ports. Add options here only if
# we need a value different from the default defined in WebKitFeatures.cmake.

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_API_TESTS PRIVATE ON)

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MINIBROWSER PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLICATION_MANIFEST PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ASYNC_SCROLLING PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ATTACHMENT_ELEMENT PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_AV1 PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CACHE_PARTITIONING PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CONTENT_EXTENSIONS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CONTENT_FILTERING PRIVATE ON)
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
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_PICTURE_IN_PICTURE_API PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_POINTER_LOCK PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_RESOURCE_USAGE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SANDBOX_EXTENSIONS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SERVICE_CONTROLS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SHAREABLE_RESOURCE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SPEECH_SYNTHESIS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_STREAMING_IPC_IN_LOG_FORWARDING PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_TELEPHONE_NUMBER_DETECTION PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_VARIATION_FONTS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_VIDEO_PRESENTATION_MODE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER_KEYBOARD_INTERACTIONS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER_MOUSE_INTERACTIONS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER_WHEEL_INTERACTIONS PRIVATE ON)
# Keep this in step with HAVE(COMPOSITOR_SERVICES) in PlatformHave.h, which is what the Xcode
# build derives ENABLE(WEBXR) from: visionOS, or macOS with a deployment target of 26 or later.
if (WEBKIT_SDK_IS_XROS OR (WEBKIT_SDK_IS_MACOS AND NOT CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 26.0))
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBXR PRIVATE ON)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBXR_LAYERS PRIVATE ON)
else ()
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBXR PRIVATE OFF)
endif ()
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_API_STATISTICS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_AUTHN PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_CODECS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_RTC PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WIRELESS_PLAYBACK_TARGET PRIVATE ON)

# Xcode enables this via FeatureDefines.xcconfig; defaults OFF in WebKitFeatures.cmake.
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ACCESSIBILITY_ISOLATED_TREE PRIVATE ON)

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBGPU PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBGPU_SWIFT PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_AVIF PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_JPEGXL PRIVATE OFF)
# Cocoa uses ColorSync, not Little CMS; CoreText handles WOFF2 natively.
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_LCMS PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_WOFF2 PRIVATE OFF)

# Features whose values is controlled by <wtf/Platform.h>. In general, new
# features should go here unless their value needs to be resolved by CMake at
# configure time.
WEBKIT_OPTION_OWNED_BY_PLATFORM_H(
    ENABLE_APPLE_PAY_AUTOMATIC_RELOAD_LINE_ITEM
    ENABLE_APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS
    ENABLE_APPLE_PAY_COUPON_CODE
    ENABLE_APPLE_PAY_DEFERRED_LINE_ITEM
    ENABLE_APPLE_PAY_DEFERRED_PAYMENTS
    ENABLE_APPLE_PAY_DELEGATED_REQUEST
    ENABLE_APPLE_PAY_DISBURSEMENTS
    ENABLE_APPLE_PAY_INSTALLMENTS
    ENABLE_APPLE_PAY_LATER
    ENABLE_APPLE_PAY_LATER_AVAILABILITY
    ENABLE_APPLE_PAY_MERCHANT_CATEGORY_CODE
    ENABLE_APPLE_PAY_MULTI_MERCHANT_PAYMENTS
    ENABLE_APPLE_PAY_PAYMENT_ORDER_DETAILS
    ENABLE_APPLE_PAY_RECURRING_LINE_ITEM
    ENABLE_APPLE_PAY_RECURRING_PAYMENTS
    ENABLE_APPLE_PAY_REFERENCE_SESSION
    ENABLE_APPLE_PAY_SELECTED_SHIPPING_METHOD
    ENABLE_APPLE_PAY_SHIPPING_CONTACT_EDITING_MODE
    ENABLE_APPLE_PAY_SHIPPING_METHOD_DATE_COMPONENTS_RANGE
    ENABLE_AVF_CAPTIONS
    ENABLE_CSS_TAP_HIGHLIGHT_COLOR
    ENABLE_CURSOR_VISIBILITY
    ENABLE_DEVICE_ORIENTATION
    ENABLE_MAC_GESTURE_EVENTS
    ENABLE_MEDIA_CAPTURE
    ENABLE_MEDIA_SOURCE_IN_WORKERS
    ENABLE_NAVIGATOR_STANDALONE
    ENABLE_ORIENTATION_EVENTS
    ENABLE_PDFKIT_PLUGIN
    ENABLE_PDF_HUD
    ENABLE_PDF_PLUGIN
    ENABLE_PHONE_NUMBER_POSSESSION
    ENABLE_PREDEFINED_COLOR_SPACE_DISPLAY_P3
    ENABLE_SMOOTH_SCROLLING
    ENABLE_SPATIAL_PORTAL
    ENABLE_SWIFT_BASE_CLASS_ANNOTATIONS
    ENABLE_UIPROCESS_PERIODIC_MEMORY_MONITOR
    ENABLE_UNIFIED_PDF
    ENABLE_WEBDRIVER_BIDI
    ENABLE_WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY
)

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_OFFSCREEN_CANVAS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_OFFSCREEN_CANVAS_IN_WORKERS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WK_WEB_EXTENSIONS PRIVATE ON)

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_LAYOUT_TESTS PRIVATE ON)
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
    # minidriver only drives the Mac MiniBrowser.
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER PUBLIC OFF)
    # Mac-only features absent on the iOS family.
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_SESSION_COORDINATOR PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_SESSION_PLAYLIST PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MOUSE_CURSOR_SCALE PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SERVICE_CONTROLS PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER_MOUSE_INTERACTIONS PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER_WHEEL_INTERACTIONS PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_IPC_TESTING_SWIFT PRIVATE OFF)
endif ()

# iOS launches the auxiliary processes as ExtensionKit extensions; every other
# platform launches them as XPC services.
if (WEBKIT_SDK_IS_IOS)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_EXTENSIONKIT PRIVATE ON)
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

set(DumpRenderTree_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/DumpRenderTree")
set(WebKitTestRunner_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKitTestRunner")
set(TestRunnerShared_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/TestRunnerShared")

# The code generators don't include wtf/Platform.h; they read its feature
# defines from here. See webkit_generate_platform_feature_defines_file().
set(WEBKIT_PLATFORM_FEATURE_DEFINES_FILE "${CMAKE_BINARY_DIR}/DerivedSources/platform-feature-defines.txt")

SET_AND_EXPOSE_TO_BUILD(USE_LIBWEBRTC TRUE)

set(ENABLE_WEBKIT_LEGACY ON)
set(ENABLE_WEBKIT ON)

# OBJECT libraries don't produce .swiftmodule files.
set(PAL_LIBRARY_TYPE STATIC)

set(CMAKE_LINK_DEPENDS_NO_SHARED ON)

set(USE_ANGLE_EGL ON)

# FIXME: CMake is not used for Production at this moment. https://bugs.webkit.org/show_bug.cgi?id=322112
SET_AND_EXPOSE_TO_BUILD(ENGINEERING_BUILD ON)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR ENABLE_SANITIZERS)
    SET_AND_EXPOSE_TO_BUILD(ENABLE_CONJECTURE_ASSERT ON)
endif ()

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

# Layer WebKit's sparse additions SDK over the iOS family's public SDKs, as
# Configurations/SDKAdditions.xcconfig does for the Xcode build.
if (NOT USE_APPLE_INTERNAL_SDK AND WEBKIT_SDK_IS_IOS_FAMILY)
    string(REGEX MATCH "^[0-9]+" _sdk_major_version "${WEBKIT_SDK_VERSION}")
    set(WEBKIT_SDK_ADDITIONS_DIR "${CMAKE_SOURCE_DIR}/WebKitLibraries/SDKs/${WEBKIT_SDK_NAME}${_sdk_major_version}.0-additions.sdk")
    if (NOT EXISTS "${WEBKIT_SDK_ADDITIONS_DIR}/SDKSettings.plist")
        message(FATAL_ERROR "The public ${WEBKIT_SDK_NAME}${WEBKIT_SDK_VERSION} SDK needs ${WEBKIT_SDK_ADDITIONS_DIR}")
    endif ()

    # Let find_library() see the private framework stubs, since cross-compiles
    # search only CMAKE_FIND_ROOT_PATH.
    list(APPEND CMAKE_FIND_ROOT_PATH "${WEBKIT_SDK_ADDITIONS_DIR}")
    list(APPEND CMAKE_FRAMEWORK_PATH "${WEBKIT_SDK_ADDITIONS_DIR}/System/Library/PrivateFrameworks")
    list(APPEND CMAKE_LIBRARY_PATH "${WEBKIT_SDK_ADDITIONS_DIR}/System/Library/PrivateFrameworks/FontServices.framework")

    # Symlink the other SDKs' headers that the base SDK lacks, as WTF's Xcode
    # build phase does.
    set(WEBKIT_DERIVED_SDK_HEADERS_DIR "${CMAKE_BINARY_DIR}/SDKAdditions")
    foreach (_file_list IN ITEMS SymlinkedHeaders SymlinkedHeaders-output)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${WEBKIT_SDK_ADDITIONS_DIR}/${_file_list}.xcfilelist")
        file(READ "${WEBKIT_SDK_ADDITIONS_DIR}/${_file_list}.xcfilelist" _paths)
        foreach (_sdk IN ITEMS macosx iphoneos)
            if (_paths MATCHES "\\$\\((WK_)?SDK_DIR_${_sdk}\\)")
                WEBKIT_XCRUN(_sdk_dir --sdk ${_sdk} --show-sdk-path)
                if (NOT IS_DIRECTORY "${_sdk_dir}")
                    message(FATAL_ERROR "${WEBKIT_SDK_ADDITIONS_DIR}/${_file_list}.xcfilelist needs the ${_sdk} SDK, which xcrun cannot find")
                endif ()
                string(REGEX REPLACE "\\$\\((WK_)?SDK_DIR_${_sdk}\\)" "${_sdk_dir}" _paths "${_paths}")
            endif ()
        endforeach ()
        string(REPLACE "$(WK_DERIVED_SDK_HEADERS_DIR)" "${WEBKIT_DERIVED_SDK_HEADERS_DIR}" _paths "${_paths}")
        if (_paths MATCHES "\\$\\(([A-Za-z0-9_]+)\\)")
            message(FATAL_ERROR "${WEBKIT_SDK_ADDITIONS_DIR}/${_file_list}.xcfilelist uses unsupported build setting $(${CMAKE_MATCH_1})")
        endif ()
        file(WRITE "${CMAKE_BINARY_DIR}/${_file_list}.xcfilelist" "${_paths}")
    endforeach ()

    # Start from an empty directory so that symlinks the file lists no longer
    # name cannot shadow SDK headers.
    file(REMOVE_RECURSE "${WEBKIT_DERIVED_SDK_HEADERS_DIR}")

    # Create the outputs' parent directories, as Xcode does before a script phase runs.
    file(STRINGS "${CMAKE_BINARY_DIR}/SymlinkedHeaders-output.xcfilelist" _outputs)
    foreach (_output IN LISTS _outputs)
        get_filename_component(_output_dir "${_output}" DIRECTORY)
        file(MAKE_DIRECTORY "${_output_dir}")
    endforeach ()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env
            "SCRIPT_INPUT_FILE_LIST_0=${CMAKE_BINARY_DIR}/SymlinkedHeaders.xcfilelist"
            "SCRIPT_OUTPUT_FILE_LIST_0=${CMAKE_BINARY_DIR}/SymlinkedHeaders-output.xcfilelist"
            "WK_DERIVED_SDK_HEADERS_DIR=${WEBKIT_DERIVED_SDK_HEADERS_DIR}"
            "SDKROOT=${CMAKE_OSX_SYSROOT}"
            "PLATFORM_NAME=${WEBKIT_SDK_NAME}"
            "SDK_DIR_WebKitSDKAdditions_${WEBKIT_SDK_NAME}=${WEBKIT_SDK_ADDITIONS_DIR}"
            ${Python_EXECUTABLE} "${CMAKE_SOURCE_DIR}/Source/WTF/Scripts/symlink-public-sdk-headers-for-embedded-platforms"
        OUTPUT_QUIET
        COMMAND_ERROR_IS_FATAL ANY)

    # Put the symlinks on the search paths that Xcode derives from the additions
    # SDK's SYSTEM_HEADER_SEARCH_PATHS and SYSTEM_FRAMEWORK_SEARCH_PATHS.
    add_compile_options(
        "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-isystem${WEBKIT_DERIVED_SDK_HEADERS_DIR}/usr/include>"
        "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${WEBKIT_DERIVED_SDK_HEADERS_DIR}/System/Library/Frameworks>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -isystem${WEBKIT_DERIVED_SDK_HEADERS_DIR}/usr/include>"
        "$<$<COMPILE_LANGUAGE:Swift>:-F${WEBKIT_DERIVED_SDK_HEADERS_DIR}/System/Library/Frameworks>"
    )

    unset(_file_list)
    unset(_output)
    unset(_output_dir)
    unset(_outputs)
    unset(_paths)
    unset(_sdk)
    unset(_sdk_dir)
    unset(_sdk_major_version)
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
# BOOL is `signed char` on x86_64 and raises additional warnings.
if (WTF_CPU_X86_64)
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-Wno-objc-signed-char-bool-implicit-int-conversion>"
    )
endif ()

add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-cast-align>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-undefined-inline>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-nonportable-include-path>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-missing-field-initializers>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-null-conversion>")
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fobjc-weak>")

# Hide symbols by default in Objective-C and Objective-C++. Otherwise, every
# template instantiation an Objective-C++ file emits is an exported weak
# definition. Inline functions are hidden as well, which is important for
# correctness since an inline function from an ARC translation unit cannot be
# linked into a non-ARC TU.
add_compile_options(
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-fvisibility=hidden>"
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJCXX>:-fvisibility-inlines-hidden>"
)

if (CMAKE_OSX_SYSROOT MATCHES "\\.Internal\\.sdk$")
    webkit_add_compile_definitions(OS_UNFAIR_LOCK_INLINE=1)
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

option(GENERATE_DSYM "Extract debug info from linked binaries into corresponding .dSYM bundles." OFF)

if (GENERATE_DSYM)
    WEBKIT_RESOLVE_TOOL(DSYMUTIL_EXECUTABLE "dsymutil")
    WEBKIT_RESOLVE_TOOL(CMAKE_STRIP "strip")

    # Sanity check that the build will produce debug info.
    string(TOUPPER "${CMAKE_BUILD_TYPE}" _dsym_config)
    if (NOT "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${_dsym_config}}" MATCHES "(^| )-g(line-tables-only)?($| )")
        message(WARNING "GENERATE_DSYM is enabled but ${CMAKE_BUILD_TYPE} emits no debug info.")
    endif ()

    # dsymutil follows the N_OSO stabs back to the object files. Under LTO the
    # linker synthesizes those objects in a temporary directory and deletes them
    # when it exits, so they have to be kept somewhere durable.
    if (LTO_MODE)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/LTO")
        add_link_options("LINKER-Wl,:-object_path_lto,${CMAKE_BINARY_DIR}/LTO")
    endif ()
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
        add_link_options("LINKER:-no_compact_unwind")
    endif ()
endif ()

# FIXME: PCH codegen emits non-exported inline functions from JSC into WebCore,
# so it's incompatible with dead stripping.
if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug" OR USE_PCH_CODEGEN)
    add_link_options("LINKER:-dead_strip")
endif ()
add_link_options("LINKER:-dead_strip_dylibs")

# Mirrors DYLIB_COMPATIBILITY_VERSION / DYLIB_CURRENT_VERSION in
# Configurations/Version.xcconfig. Set globally rather than per framework so that
# every dylib carries them: clients linked against the Xcode frameworks record a
# required compatibility version of 1.0.0, and dyld refuses to substitute a dylib
# that declares 0.0.0. Shared-only, since ld rejects these flags for executables.
add_link_options(
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:LINKER:-compatibility_version,1.0.0>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:LINKER:-current_version,${WEBKIT_MAC_VERSION}>"
)

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

# The triples everything Apple is built and staged under, taking the first
# architecture of a build for several, as the Platform.h preprocessing does.
if (CMAKE_OSX_DEPLOYMENT_TARGET)
    if (CMAKE_OSX_ARCHITECTURES)
        list(GET CMAKE_OSX_ARCHITECTURES 0 _triple_arch)
    else ()
        set(_triple_arch "${CMAKE_SYSTEM_PROCESSOR}")
    endif ()
    set(_triple_suffix "")
    if (WEBKIT_SDK_IS_SIMULATOR)
        set(_triple_suffix "-simulator")
    endif ()
    set(WEBKIT_SDK_TARGET_TRIPLE
        "${_triple_arch}-apple-${WEBKIT_SDK_TARGET_OS}${CMAKE_OSX_DEPLOYMENT_TARGET}${_triple_suffix}"
        CACHE STRING "Target triple" FORCE
    )
    set(WEBKIT_SWIFT_MODULE_TRIPLE "${_triple_arch}-apple-${WEBKIT_SDK_MODULE_OS}${_triple_suffix}"
        CACHE STRING "Swift module triple" FORCE)

    # Swiftc falls back to its built-in deployment target while clang honors
    # CMAKE_OSX_DEPLOYMENT_TARGET; the mismatch produces an ld warning per object.
    # A build for several architectures has no single target to name, and swiftc
    # is left to work it out.
    list(LENGTH CMAKE_OSX_ARCHITECTURES _arch_count)
    if (_arch_count LESS_EQUAL 1)
        set(CMAKE_Swift_COMPILER_TARGET "${WEBKIT_SDK_TARGET_TRIPLE}" CACHE STRING "Swift target triple" FORCE)
    endif ()
    unset(_triple_suffix)
    unset(_triple_arch)
    unset(_arch_count)
endif ()

# Building x86_64 on an Apple Silicon host means the result only ever runs under Rosetta.
if (CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64" AND WTF_HOST_SYSTEM_MACHINE STREQUAL "arm64")
    add_compile_definitions(HAVE_CPU_TRANSLATION_CAPABILITY=0)
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
set(WebGPU_LIBRARY_TYPE SHARED)
set(WebCoreTestSupport_LIBRARY_TYPE SHARED)

# Large unified-source bundles speed up the macOS build. The iOS family keeps
# the script default (8) so @no-unify-when(bundle<=8) sources (e.g. those that
# pull UIKit -> CoreServices private headers) stay non-unified and avoid clashes.
if (WEBKIT_SDK_IS_MACOS)
    set(WEBKIT_MAX_BUNDLE_SIZE 128)
endif ()

# Frameworks install into the system framework locations so dylib ids resolve at
# runtime, and so that a DYLD_FRAMEWORK_PATH override replaces the dyld shared
# cache copy instead of loading alongside it.
set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR ON)
set(JavaScriptCore_INSTALL_NAME_DIR "/System/Library/Frameworks" CACHE STRING "" FORCE)
set(WebKit_INSTALL_NAME_DIR "/System/Library/Frameworks" CACHE STRING "" FORCE)
set(WebGPU_INSTALL_NAME_DIR "/System/Library/PrivateFrameworks" CACHE STRING "" FORCE)
set(_WebKit_SwiftUI_INSTALL_NAME_DIR "/System/Library/Frameworks" CACHE STRING "" FORCE)

# WebCore and WebKitLegacy ship nested inside WebKit.framework on macOS; the
# iOS family installs them next to the other private frameworks.
if (WEBKIT_SDK_IS_MACOS)
    set(_wk_umbrella_frameworks_dir "/System/Library/Frameworks/WebKit.framework/Versions/A/Frameworks")
else ()
    set(_wk_umbrella_frameworks_dir "/System/Library/PrivateFrameworks")
endif ()
set(WebCore_INSTALL_NAME_DIR "${_wk_umbrella_frameworks_dir}" CACHE STRING "" FORCE)
set(WebKitLegacy_INSTALL_NAME_DIR "${_wk_umbrella_frameworks_dir}" CACHE STRING "" FORCE)
unset(_wk_umbrella_frameworks_dir)

# Local dev builds are not part of the dyld shared cache. System-path install
# names would otherwise mark some frameworks "shared-cache eligible" and the
# linker rejects eligible->ineligible links between them. Opt every dylib out.
add_link_options("LINKER:-not_for_dyld_shared_cache")

if (WEBKIT_SDK_IS_IOS_FAMILY)
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
    # The build directory comes first: WebKit, WebCore and friends are in the SDK
    # too, and its stubs export none of the SPI this build links against.
    # Search the SDK additions' stubs last so they never shadow a real framework.
    if (CMAKE_OSX_SYSROOT)
        add_link_options("-F${CMAKE_BINARY_DIR}")
        add_link_options("-F${CMAKE_OSX_SYSROOT}/System/Library/Frameworks")
        add_link_options("-F${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks")
        if (WEBKIT_SDK_ADDITIONS_DIR)
            add_link_options("-F${WEBKIT_SDK_ADDITIONS_DIR}/System/Library/PrivateFrameworks")
        endif ()
    endif ()
endif ()

if (CMAKE_OSX_SYSROOT)
    add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-F${CMAKE_BINARY_DIR};-iframework${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks>")
    # -F rather than -Fsystem for SDK frameworks, cf. rdar://problem/30939744
    add_compile_options("$<$<COMPILE_LANGUAGE:Swift>:-F${CMAKE_BINARY_DIR};-F${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks>")
endif ()

# WebKit.xcodeproj and its lldbinit, on request: `ninja xcode-project`. Xcode
# is only a debugger frontend here, so generating it from a configure or from
# the default build would spend every engineer's build on a file most of them
# never open. For the macOS SDK alone, because its schemes launch their
# product on the build machine.
if (WEBKIT_SDK_NAME STREQUAL "macosx"
        AND EXISTS ${TOOLS_DIR}/Scripts/generate-cmake-xcode-project)
    # Deferred: the executables it depends on, so that it runs after they are
    # linked, are not defined where this file is included.
    function(WEBKIT_ADD_XCODE_PROJECT_TARGET)
        set(_executables)
        set(_directories ${CMAKE_SOURCE_DIR})
        while (_directories)
            list(POP_FRONT _directories _directory)
            get_property(_subdirectories DIRECTORY ${_directory}
                PROPERTY SUBDIRECTORIES)
            list(APPEND _directories ${_subdirectories})
            get_property(_targets DIRECTORY ${_directory}
                PROPERTY BUILDSYSTEM_TARGETS)
            foreach (_target IN LISTS _targets)
                get_target_property(_type ${_target} TYPE)
                # The schemes cover what the default build produces, so an
                # EXCLUDE_FROM_ALL executable such as an ANGLE test binary is
                # not something this target should build either.
                get_target_property(_excluded ${_target} EXCLUDE_FROM_ALL)
                if (_type STREQUAL "EXECUTABLE" AND NOT _excluded)
                    list(APPEND _executables ${_target})
                endif ()
            endforeach ()
        endwhile ()

        add_custom_target(xcode-project
            COMMAND ${Python_EXECUTABLE}
                    ${TOOLS_DIR}/Scripts/generate-cmake-xcode-project
                    --quiet
                    ${CMAKE_BINARY_DIR}
            COMMENT "Generating ${CMAKE_BINARY_DIR}/WebKit.xcodeproj"
            VERBATIM)
        # Its schemes name built products, so build them first.
        add_dependencies(xcode-project ${_executables})
    endfunction()

    cmake_language(DEFER CALL WEBKIT_ADD_XCODE_PROJECT_TARGET)
endif ()

if (WEBKIT_ADDITIONS_INCLUDE_PATH AND EXISTS "${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/CMake/OptionsCocoa.cmake")
    message(STATUS "WebKitAdditions CMake: ${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/CMake/OptionsCocoa.cmake")
    include("${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/CMake/OptionsCocoa.cmake")
endif ()

set(MiniBrowser_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/MiniBrowser")

