include(PlatformCocoa.cmake)

if (WEBKIT_ADDITIONS_INCLUDE_PATH AND EXISTS "${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/CMake/TouchEvents.cmake")
    message(STATUS "TouchEvents.cmake: found at ${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/CMake/TouchEvents.cmake")
    include("${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/CMake/TouchEvents.cmake")
else ()
    message(STATUS "TouchEvents.cmake: NOT found (WEBKIT_ADDITIONS_INCLUDE_PATH=${WEBKIT_ADDITIONS_INCLUDE_PATH})")
endif ()
if (WEBKIT_ADDITIONS_INCLUDE_PATH AND EXISTS "${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/CMake/GestureEvents.cmake")
    message(STATUS "GestureEvents.cmake: found at ${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/CMake/GestureEvents.cmake")
    include("${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/CMake/GestureEvents.cmake")
else ()
    message(STATUS "GestureEvents.cmake: NOT found (WEBKIT_ADDITIONS_INCLUDE_PATH=${WEBKIT_ADDITIONS_INCLUDE_PATH})")
endif ()
if (WEBKIT_ADDITIONS_COMPILE_PATH AND EXISTS "${WEBKIT_ADDITIONS_COMPILE_PATH}/WebKitAdditions")
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES "${WEBKIT_ADDITIONS_COMPILE_PATH}/WebKitAdditions")
endif ()

target_compile_options(WebCore PRIVATE -Wno-\#warnings -Wno-abstract-final-class)

set(BUNDLE_VERSION "${MACOSX_FRAMEWORK_BUNDLE_VERSION}")
set(SHORT_VERSION_STRING "${WEBKIT_MAC_VERSION}")
set(PRODUCT_NAME "WebCore")
set(PRODUCT_BUNDLE_IDENTIFIER "com.apple.WebCore")
configure_file(${WEBCORE_DIR}/Info.plist ${CMAKE_CURRENT_BINARY_DIR}/WebCore-Info.plist)

set(_wc_fw "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebCore.framework")

add_custom_command(TARGET WebCore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${WEBCORE_DIR}/en.lproj" "${_wc_fw}/en.lproj"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WEBCORE_DIR}/Resources/ContentFilterBlockedPage.html"
        "${WEBCORE_DIR}/Resources/linearSRGB.icc"
        "${WEBCORE_DIR}/Resources/ListButtonArrow.png"
        "${WEBCORE_DIR}/Resources/ListButtonArrow@2x.png"
        "${WEBCORE_DIR}/Resources/missingImage.png"
        "${WEBCORE_DIR}/Resources/missingImage@2x.png"
        "${WEBCORE_DIR}/Resources/missingImage@3x.png"
        "${WEBCORE_DIR}/Resources/modelDefaultDiffuseData"
        "${WEBCORE_DIR}/Resources/modelDefaultSpecularData"
        "${WEBCORE_DIR}/Resources/textAreaResizeCorner.png"
        "${WEBCORE_DIR}/Resources/textAreaResizeCorner@2x.png"
        "${_wc_fw}/"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_wc_fw}/audio"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WEBCORE_DIR}/platform/audio/resources/Composite.wav"
        "${_wc_fw}/audio/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${WEBCORE_DIR}/Modules/modern-media-controls"
        "${_wc_fw}/modern-media-controls"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${_wc_fw}/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_BINARY_DIR}/WebCore-Info.plist"
        "${_wc_fw}/Info.plist"
    COMMENT "Installing WebCore.framework resources (flat iOS layout)")

find_library(APPSUPPORT_LIBRARY AppSupport HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(IOSURFACEACCELERATOR_LIBRARY IOSurfaceAccelerator HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(IMAGEIO_LIBRARY ImageIO)
find_library(CORETEXT_LIBRARY CoreText)
find_library(COREIMAGE_LIBRARY CoreImage)
find_library(COREVIDEO_LIBRARY CoreVideo)
find_library(GRAPHICSSERVICES_LIBRARY GraphicsServices HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(MOBILEGESTALT_LIBRARY MobileGestalt HINTS ${CMAKE_OSX_SYSROOT}/usr/lib)
find_library(MOBILECORESERVICES_LIBRARY MobileCoreServices)
find_library(UIKIT_LIBRARY UIKit)

list(APPEND WebCore_LIBRARIES
    ${COREIMAGE_LIBRARY}
    ${CORETEXT_LIBRARY}
    ${COREVIDEO_LIBRARY}
    ${IMAGEIO_LIBRARY}
    ${MOBILECORESERVICES_LIBRARY}
    ${UIKIT_LIBRARY}
)

if (APPSUPPORT_LIBRARY)
    list(APPEND WebCore_LIBRARIES ${APPSUPPORT_LIBRARY})
endif ()
if (FONTPARSER_LIBRARY)
    list(APPEND WebCore_LIBRARIES ${FONTPARSER_LIBRARY})
endif ()
if (GRAPHICSSERVICES_LIBRARY)
    list(APPEND WebCore_LIBRARIES ${GRAPHICSSERVICES_LIBRARY})
endif ()
if (MOBILEGESTALT_LIBRARY)
    list(APPEND WebCore_LIBRARIES ${MOBILEGESTALT_LIBRARY})
endif ()

if (IOSURFACEACCELERATOR_LIBRARY)
    list(APPEND WebCore_LIBRARIES ${IOSURFACEACCELERATOR_LIBRARY})
endif ()

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_SOURCE_DIR}/Source/WebCore/accessibility/ios"
    "${WEBCORE_DIR}/platform/mac"
    "${WEBCORE_DIR}/platform/graphics/mac"
    "${WEBCORE_DIR}/editing/mac"
    "${WEBCORE_DIR}/loader/ios"
    "${WEBCORE_DIR}/page/mac"
    "${WEBCORE_DIR}/Modules/system-preview"
    "${WEBCORE_DIR}/editing/ios"
    "${WEBCORE_DIR}/page/ios"
    "${WEBCORE_DIR}/platform/audio/ios"
    "${WEBCORE_DIR}/platform/graphics/ios"
    "${WEBCORE_DIR}/platform/graphics/ios/controls"
    "${WEBCORE_DIR}/platform/ios"
    "${WEBCORE_DIR}/platform/ios/wak"
    "${WEBCORE_DIR}/platform/mediastream/ios"
    "${WEBCORE_DIR}/platform/network/ios"
    "${WEBCORE_DIR}/rendering/ios"
)

list(APPEND WebCore_SOURCES
    accessibility/AccessibilityMediaHelpers.cpp

    accessibility/ios/AXObjectCacheIOS.mm
    accessibility/ios/AccessibilityObjectIOS.mm
    accessibility/ios/WebAccessibilityObjectWrapperIOS.mm

    editing/ios/EditorIOS.mm

    page/ios/EventHandlerIOS.mm
    page/ios/FrameIOS.mm

    platform/cocoa/WebAVPlayerLayerView.mm

    platform/graphics/ios/IconIOS.mm

    platform/ios/ColorIOS.mm
    platform/ios/DragImageIOS.mm
    platform/ios/KeyEventIOS.mm
    platform/ios/LocalCurrentGraphicsContextIOS.mm
    platform/ios/LocalCurrentTraitCollection.mm
    platform/ios/LocalizedDeviceModel.mm
    platform/ios/PasteboardIOS.mm
    platform/ios/PlatformEventFactoryIOS.mm
    platform/ios/PlatformPasteboardIOS.mm
    platform/ios/PlatformScreenIOS.mm
    platform/ios/ScrollAnimatorIOS.mm
    platform/ios/ScrollViewIOS.mm
    platform/ios/ScrollbarThemeIOS.mm
    platform/ios/ThemeIOS.mm
    platform/ios/UIFoundationSoftLink.mm
    platform/ios/UserAgentIOS.mm
    platform/ios/ValidationBubbleIOS.mm
    platform/ios/WidgetIOS.mm

    platform/ios/wak/WAKAppKitStubs.mm
    platform/ios/wak/WAKClipView.mm
    platform/ios/wak/WAKResponder.mm
    platform/ios/wak/WKUtilities.cpp

    platform/mediastream/ios/MediaCaptureStatusBarManager.mm

    rendering/ios/RenderThemeIOS.mm
)

set_source_files_properties(
    ${WEBCORE_DIR}/platform/ios/wak/WAKAppKitStubs.mm
    ${WEBCORE_DIR}/platform/ios/wak/WAKClipView.mm
    ${WEBCORE_DIR}/platform/ios/wak/WAKResponder.mm
    PROPERTIES COMPILE_FLAGS -fno-objc-arc
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    dom/TouchEvent.h

    page/mac/WebCoreFrameView.h

    platform/audio/ios/MediaDeviceRouteController.h
    platform/audio/ios/MediaDeviceRouteLoadURLResult.h

    platform/graphics/mac/ColorMac.h

    platform/ios/AbstractPasteboard.h

    platform/ios/wak/WAKView.h
)

set(CSS_VALUE_PLATFORM_DEFINES "WTF_PLATFORM_IOS WTF_PLATFORM_IOS_FAMILY WTF_PLATFORM_COCOA ENABLE_APPLE_PAY_NEW_BUTTON_TYPES HAVE_CORE_MATERIAL HAVE_MATERIAL_HOSTING")

list(APPEND WebCoreTestSupport_PRIVATE_INCLUDE_DIRECTORIES "${WEBCORE_DIR}/testing/cocoa")
list(APPEND WebCoreTestSupport_SOURCES
    testing/MockMediaDeviceRoute.mm
    testing/MockMediaDeviceRouteController.mm

    testing/cocoa/WebMockMediaDeviceRoute.mm
)

