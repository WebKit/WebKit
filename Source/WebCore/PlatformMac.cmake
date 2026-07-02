include(PlatformCocoa.cmake)

# Localizable.strings for copyLocalizedString(). Xcode copies via CopyFiles build phase.
# Configure-time -- files rarely change, no build edge needed.
file(COPY "${WEBCORE_DIR}/en.lproj"
     DESTINATION "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebCore.framework/Versions/A/Resources")

# Copy the proper resources over, mirroring Xcode's Resources build phase.
# Listed explicitly (rather than globbed) so null builds stay clean.
set(WebCore_BUNDLE_RESOURCES
    ${WEBCORE_DIR}/Resources/ContentFilterBlockedPage.html
    ${WEBCORE_DIR}/Resources/ListButtonArrow.png
    ${WEBCORE_DIR}/Resources/ListButtonArrow@2x.png
    ${WEBCORE_DIR}/Resources/copyCursor.png
    ${WEBCORE_DIR}/Resources/deleteButtonPressed.tiff
    ${WEBCORE_DIR}/Resources/linearSRGB.icc
    ${WEBCORE_DIR}/Resources/missingImage.png
    ${WEBCORE_DIR}/Resources/missingImage@2x.png
    ${WEBCORE_DIR}/Resources/missingImage@3x.png
    ${WEBCORE_DIR}/Resources/modelDefaultDiffuseData
    ${WEBCORE_DIR}/Resources/modelDefaultSpecularData
    ${WEBCORE_DIR}/Resources/moveCursor.png
    ${WEBCORE_DIR}/Resources/northEastSouthWestResizeCursor.png
    ${WEBCORE_DIR}/Resources/northSouthResizeCursor.png
    ${WEBCORE_DIR}/Resources/northWestSouthEastResizeCursor.png
    ${WEBCORE_DIR}/Resources/nullPlugin.png
    ${WEBCORE_DIR}/Resources/nullPlugin@2x.png
    ${WEBCORE_DIR}/Resources/panIcon.png
    ${WEBCORE_DIR}/Resources/textAreaResizeCorner.png
    ${WEBCORE_DIR}/Resources/textAreaResizeCorner@2x.png
)
WEBKIT_COPY_FILES(WebCore_CopyBundleResources
    DESTINATION "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebCore.framework/Versions/A/Resources"
    FILES ${WebCore_BUNDLE_RESOURCES}
    FLATTENED NO_SYMLINK)
add_dependencies(WebCore WebCore_CopyBundleResources)

# Modern media controls button icons. RenderThemeCocoa::mediaControlsImageDataForIconNameAndType
# loads these at runtime from WebCore.framework/Resources/modern-media-controls/images/<name>.<type>;
# without them every media-control button loads an empty blob and logs "Button failed to load".
# The macOS variants are copied flat (no platform subdirectory), matching the bundle lookup.
# Listed explicitly (rather than globbed) so null builds stay clean.
set(WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR ${WEBCORE_DIR}/Modules/modern-media-controls/images/macOS)
set(WebCore_MODERN_MEDIA_CONTROLS_IMAGES
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Airplay-fullscreen.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Airplay.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Ellipsis.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/EnterFullscreen.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/ExitFullscreen.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Forward.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/MediaSelector-fullscreen.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/MediaSelector.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Overflow.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Pause.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/PipIn-fullscreen.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/PipIn.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Play.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Rewind.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/SkipBack10.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/SkipBack15.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/SkipForward10.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/SkipForward15.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Volume0-RTL.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Volume0.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Volume1-RTL.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Volume1.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Volume2-RTL.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Volume2.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Volume3-RTL.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/Volume3.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/VolumeMuted-RTL.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/VolumeMuted.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/X.svg
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/airplay-placard@1x.png
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/airplay-placard@2x.png
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/invalid-placard@1x.png
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/invalid-placard@2x.png
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/pip-placard@1x.png
    ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES_DIR}/pip-placard@2x.png
)
file(MAKE_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebCore.framework/Versions/A/Resources/modern-media-controls/images")
WEBKIT_COPY_FILES(WebCore_CopyModernMediaControlsImages
    DESTINATION "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebCore.framework/Versions/A/Resources/modern-media-controls/images"
    FILES ${WebCore_MODERN_MEDIA_CONTROLS_IMAGES}
    FLATTENED NO_SYMLINK)
add_dependencies(WebCore WebCore_CopyModernMediaControlsImages)

find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
find_library(AUDIOUNIT_LIBRARY AudioUnit)
find_library(CARBON_LIBRARY Carbon)
find_library(COCOA_LIBRARY Cocoa)
find_library(CORESERVICES_LIBRARY CoreServices)
find_library(DISKARBITRATION_LIBRARY DiskArbitration)
find_library(OPENGL_LIBRARY OpenGL)
find_library(QUARTZ_LIBRARY Quartz)

list(APPEND WebCore_LIBRARIES
    ${AUDIOUNIT_LIBRARY}
    ${CARBON_LIBRARY}
    ${COCOA_LIBRARY}
    ${CORESERVICES_LIBRARY}
    ${DISKARBITRATION_LIBRARY}
    ${FONTPARSER_LIBRARY}
    ${OPENGL_LIBRARY}
    ${QUARTZ_LIBRARY}
)

add_compile_options(
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${AVFOUNDATION_LIBRARY}/Versions/Current/Frameworks>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CARBON_LIBRARY}/Versions/Current/Frameworks>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CORESERVICES_LIBRARY}/Versions/Current/Frameworks>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${QUARTZ_LIBRARY}/Frameworks>"
)

find_library(LOOKUP_FRAMEWORK Lookup HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
if (NOT LOOKUP_FRAMEWORK-NOTFOUND)
    list(APPEND WebCore_LIBRARIES ${LOOKUP_FRAMEWORK})
endif ()

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/accessibility/isolatedtree/mac"
    "${WEBCORE_DIR}/accessibility/mac"
    "${WEBCORE_DIR}/dom/mac"
    "${WEBCORE_DIR}/editing/mac"
    "${WEBCORE_DIR}/page/mac"
    "${WEBCORE_DIR}/page/scrolling/mac"
    "${WEBCORE_DIR}/platform/audio/mac"
    "${WEBCORE_DIR}/platform/graphics/mac"
    "${WEBCORE_DIR}/platform/graphics/mac/controls"
    "${WEBCORE_DIR}/platform/ios"
    "${WEBCORE_DIR}/platform/mac"
    "${WEBCORE_DIR}/platform/mediastream/mac"
    "${WEBCORE_DIR}/platform/network/mac"
    "${WEBCORE_DIR}/platform/text/mac"
    "${WEBCORE_DIR}/platform/spi/mac"
    "${WEBCORE_DIR}/plugins/mac"
)

list(APPEND WebCore_SOURCES
    accessibility/isolatedtree/mac/AXIsolatedObjectMac.mm
    accessibility/isolatedtree/mac/AXIsolatedTreeMac.mm
    accessibility/mac/AXObjectCacheMac.mm
    accessibility/mac/AccessibilityObjectMac.mm
    accessibility/mac/WebAccessibilityObjectWrapperMac.mm

    dom/DataTransferMac.mm

    editing/mac/EditorMac.mm
    editing/mac/TextAlternativeWithRange.mm
    editing/mac/TextUndoInsertionMarkupMac.mm

    page/mac/EventHandlerMac.mm
    page/mac/ServicesOverlayController.mm
    page/mac/WheelEventDeltaFilterMac.mm

    page/scrolling/mac/ScrollingCoordinatorMac.mm
    page/scrolling/mac/ScrollingTreeFrameScrollingNodeMac.mm
    page/scrolling/mac/ScrollingTreeMac.mm

    platform/audio/mac/AudioHardwareListenerMac.cpp

    platform/gamepad/mac/HIDGamepad.cpp

    platform/graphics/mac/ColorMac.mm
    platform/graphics/mac/GraphicsChecksMac.cpp
    platform/graphics/mac/IconMac.mm
    platform/graphics/mac/PDFDocumentImageMac.mm

    platform/mac/CursorMac.mm
    platform/mac/KeyEventMac.mm
    platform/mac/LocalCurrentGraphicsContextMac.mm
    platform/mac/NSScrollerImpDetails.mm
    platform/mac/PasteboardMac.mm
    platform/mac/PasteboardWriter.mm
    platform/mac/PlatformEventFactoryMac.mm
    platform/mac/PlatformPasteboardMac.mm
    platform/mac/PlatformScreenMac.mm
    platform/mac/PowerObserverMac.cpp
    platform/mac/RevealUtilities.mm
    platform/mac/ScrollAnimatorMac.mm
    platform/mac/ScrollViewMac.mm
    platform/mac/ScrollbarThemeMac.mm
    platform/mac/ScrollingEffectsController.mm
    platform/mac/SerializedPlatformDataCueMac.mm
    platform/mac/SuddenTermination.mm
    platform/mac/ThemeMac.mm
    platform/mac/ThreadCheck.mm
    platform/mac/UserActivityMac.mm
    platform/mac/ValidationBubbleMac.mm
    platform/mac/WebCoreFullScreenPlaceholderView.mm
    platform/mac/WebCoreFullScreenWarningView.mm
    platform/mac/WebCoreFullScreenWindow.mm
    platform/mac/WidgetMac.mm

    platform/text/mac/TextCheckingMac.mm

    rendering/mac/RenderThemeMac.mm
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/html/shadow/mac/imageControlsMac.css
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    page/mac/CorrectionIndicator.h
    page/mac/WebCoreFrameView.h

    page/scrolling/mac/ScrollerMac.h
    page/scrolling/mac/ScrollerPairMac.h
    page/scrolling/mac/ScrollingCoordinatorMac.h
    page/scrolling/mac/ScrollingTreeFrameScrollingNodeMac.h
    page/scrolling/mac/ScrollingTreeOverflowScrollingNodeMac.h
    page/scrolling/mac/ScrollingTreePluginScrollingNodeMac.h
    page/scrolling/mac/ScrollingTreeScrollingNodeDelegateMac.h

    platform/audio/cocoa/PitchShiftAudioUnit.h

    platform/audio/mac/SharedRoutingArbitrator.h

    platform/gamepad/mac/HIDGamepad.h
    platform/gamepad/mac/HIDGamepadElement.h

    platform/graphics/avfoundation/FormatDescriptionUtilities.h

    platform/graphics/mac/AppKitControlSystemImage.h
    platform/graphics/mac/ColorMac.h
    platform/graphics/mac/GraphicsChecksMac.h
    platform/graphics/mac/ScrollbarTrackCornerSystemImageMac.h

    platform/mac/DataDetectorHighlight.h
    platform/mac/HIDDevice.h
    platform/mac/HIDElement.h
    platform/mac/LocalDefaultSystemAppearance.h
    platform/mac/PowerObserverMac.h
    platform/mac/RevealUtilities.h
    platform/mac/SerializedPlatformDataCueMac.h
    platform/mac/WebCoreFullScreenPlaceholderView.h
    platform/mac/WebPlaybackControlsManager.h

    rendering/mac/RenderThemeMac.h
)

# CSS codegen scripts only see command-line defines, not PlatformHave.h.
# Bare names only (no =1). FIXME: HAVE_CORE_MATERIAL should be gated for Mac
# in PlatformHave.h. https://bugs.webkit.org/show_bug.cgi?id=312061
set(CSS_VALUE_PLATFORM_DEFINES "WTF_PLATFORM_MAC WTF_PLATFORM_COCOA ENABLE_APPLE_PAY_NEW_BUTTON_TYPES HAVE_CORE_MATERIAL HAVE_MATERIAL_HOSTING")
