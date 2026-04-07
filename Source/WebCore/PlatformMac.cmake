# The Xcode build doesn't use -Werror=undefined-inline; WebKit's coding pattern
# of declaring inline functions in headers with definitions in separate *Inlines.h
# files triggers this warning in many places on the Mac port.
list(APPEND WebCore_COMPILE_OPTIONS "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-Wno-error=undefined-inline>")

find_library(ACCELERATE_LIBRARY Accelerate)
find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
find_library(AVFOUNDATION_LIBRARY AVFoundation)
find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
find_library(AUDIOUNIT_LIBRARY AudioUnit)
find_library(CARBON_LIBRARY Carbon)
find_library(CFNETWORK_LIBRARY CFNetwork)
find_library(COCOA_LIBRARY Cocoa)
find_library(COMPRESSION_LIBRARY Compression)
find_library(COREAUDIO_LIBRARY CoreAudio)
find_library(COREMEDIA_LIBRARY CoreMedia)
find_library(CORESERVICES_LIBRARY CoreServices)
find_library(DISKARBITRATION_LIBRARY DiskArbitration)
find_library(IOKIT_LIBRARY IOKit)
find_library(IOSURFACE_LIBRARY IOSurface)
find_library(METAL_LIBRARY Metal)
find_library(NETWORKEXTENSION_LIBRARY NetworkExtension)
find_library(OPENGL_LIBRARY OpenGL)
find_library(QUARTZ_LIBRARY Quartz)
find_library(QUARTZCORE_LIBRARY QuartzCore)
find_library(SECURITY_LIBRARY Security)
find_library(SYSTEMCONFIGURATION_LIBRARY SystemConfiguration)
find_library(VIDEOTOOLBOX_LIBRARY VideoToolbox)
find_library(XML2_LIBRARY XML2)

find_package(SQLite3 REQUIRED)
find_package(ZLIB REQUIRED)

if (NOT TARGET SQLite3::SQLite3) # CMake < 4.3
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif ()

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesCocoa.txt"
)

list(APPEND WebCore_LIBRARIES
    ${ACCELERATE_LIBRARY}
    ${AUDIOTOOLBOX_LIBRARY}
    ${AUDIOUNIT_LIBRARY}
    ${AVFOUNDATION_LIBRARY}
    ${CARBON_LIBRARY}
    ${CFNETWORK_LIBRARY}
    ${COCOA_LIBRARY}
    ${COMPRESSION_LIBRARY}
    ${COREAUDIO_LIBRARY}
    ${COREMEDIA_LIBRARY}
    ${CORESERVICES_LIBRARY}
    ${DISKARBITRATION_LIBRARY}
    ${IOKIT_LIBRARY}
    ${IOSURFACE_LIBRARY}
    ${METAL_LIBRARY}
    ${NETWORKEXTENSION_LIBRARY}
    ${OPENGL_LIBRARY}
    ${QUARTZ_LIBRARY}
    ${QUARTZCORE_LIBRARY}
    ${SECURITY_LIBRARY}
    ${SQLITE3_LIBRARIES}
    ${SYSTEMCONFIGURATION_LIBRARY}
    ${VIDEOTOOLBOX_LIBRARY}
    ${XML2_LIBRARY}
    WebGPU
    opus
    vpx
    webm
    webrtc
    yuv
)


find_library(COREIMAGE_LIBRARY CoreImage)
find_library(NETWORK_LIBRARY Network)

list(APPEND WebCore_LIBRARIES
    ${COREIMAGE_LIBRARY}
    ${NETWORK_LIBRARY}
    "-L${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks/FontServices.framework" -lFontParser
    "-L${CMAKE_OSX_SYSROOT}/usr/lib" -lAccessibility
)

# These linker flags must use LINKER: prefix so CMake translates them
# correctly when Swift is the linker driver.
# These linker flags are added via target_link_options in CMakeLists.txt
# (after WEBKIT_FRAMEWORK) so that LINKER: prefix ensures correct translation
# when Swift is the linker driver.
# FIXME: -undefined,dynamic_lookup allows undefined symbols for Swift-dependent
# crypto, iOS-only code, missing libaom, and vpx internal issues until resolved.
set(WebCore_MAC_LINK_OPTIONS
    "LINKER:-undefined,dynamic_lookup"
    "LINKER:-weak-lxslt"
)

find_library(UNIFORMTYPEIDENTIFIERS_LIBRARY UniformTypeIdentifiers)
if (UNIFORMTYPEIDENTIFIERS_LIBRARY)
    list(APPEND WebCore_LIBRARIES ${UNIFORMTYPEIDENTIFIERS_LIBRARY})
endif ()

find_library(ACCESSIBILITY_LIBRARY Accessibility)
if (ACCESSIBILITY_LIBRARY)
    list(APPEND WebCore_LIBRARIES ${ACCESSIBILITY_LIBRARY})
endif ()

# These are Clang-only flags — use WebCore_COMPILE_OPTIONS with generator
# expressions to avoid passing them to Swift.
# Match Xcode's OTHER_CPLUSPLUSFLAGS: -fmodules -fno-cxx-modules
# -fmodules is needed for correct API availability resolution (e.g. CoreLocation enums).
list(APPEND WebCore_COMPILE_OPTIONS
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fmodules>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fno-cxx-modules>"
    "SHELL:$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework ${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks>"
    "SHELL:$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework ${AVFOUNDATION_LIBRARY}/Versions/Current/Frameworks>"
    "SHELL:$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework ${CARBON_LIBRARY}/Versions/Current/Frameworks>"
    "SHELL:$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework ${CORESERVICES_LIBRARY}/Versions/Current/Frameworks>"
    "SHELL:$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework ${QUARTZ_LIBRARY}/Frameworks>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fobjc-weak>"
)

find_library(COREUI_FRAMEWORK CoreUI HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
if (NOT COREUI_FRAMEWORK-NOTFOUND)
    list(APPEND WebCore_LIBRARIES ${COREUI_FRAMEWORK})
endif ()

find_library(DATADETECTORSCORE_FRAMEWORK DataDetectorsCore HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
if (NOT DATADETECTORSCORE_FRAMEWORK-NOTFOUND)
    list(APPEND WebCore_LIBRARIES ${DATADETECTORSCORE_FRAMEWORK})
endif ()

find_library(LOOKUP_FRAMEWORK Lookup HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
if (NOT LOOKUP_FRAMEWORK-NOTFOUND)
    list(APPEND WebCore_LIBRARIES ${LOOKUP_FRAMEWORK})
endif ()

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}/libwebrtc/PrivateHeaders"
    "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source/third_party/libwebm"
    "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source"
    "${WEBCORE_DIR}/Modules/applepay-ams-ui"
    "${WEBCORE_DIR}/Modules/webauthn/apdu"
    "${WEBCORE_DIR}/accessibility/cocoa"
    "${WEBCORE_DIR}/accessibility/isolatedtree"
    "${WEBCORE_DIR}/accessibility/isolatedtree/mac"
    "${WEBCORE_DIR}/accessibility/mac"
    "${WEBCORE_DIR}/bridge/objc"
    "${WEBCORE_DIR}/crypto/cocoa"
    "${WEBCORE_DIR}/crypto/mac"
    "${WEBCORE_DIR}/dom/mac"
    "${WEBCORE_DIR}/editing/cocoa"
    "${WEBCORE_DIR}/editing/mac"
    "${WEBCORE_DIR}/html/shadow/cocoa"
    "${WEBCORE_DIR}/layout/tableformatting"
    "${WEBCORE_DIR}/loader/archive/cf"
    "${WEBCORE_DIR}/loader/cf"
    "${WEBCORE_DIR}/loader/cocoa"
    "${WEBCORE_DIR}/loader/mac"
    "${WEBCORE_DIR}/page/cocoa"
    "${WEBCORE_DIR}/platform/graphics/cocoa/controls"
    "${WEBCORE_DIR}/platform/graphics/mac/controls"
    "${WEBCORE_DIR}/platform/graphics/re"
    "${WEBCORE_DIR}/platform/image-decoders"
    "${WEBCORE_DIR}/platform/graphics/coretext"
    "${WEBCORE_DIR}/platform/video-codecs/cocoa"
    "${WEBCORE_DIR}/page/writing-tools"
    "${WEBCORE_DIR}/page/mac"
    "${WEBCORE_DIR}/page/scrolling/cocoa"
    "${WEBCORE_DIR}/page/scrolling/mac"
    "${WEBCORE_DIR}/platform/audio/cocoa"
    "${WEBCORE_DIR}/platform/audio/mac"
    "${WEBCORE_DIR}/platform/cf"
    "${WEBCORE_DIR}/platform/cocoa"
    "${WEBCORE_DIR}/platform/gamepad/cocoa"
    "${WEBCORE_DIR}/platform/graphics/angle"
    "${WEBCORE_DIR}/platform/graphics/avfoundation"
    "${WEBCORE_DIR}/platform/graphics/avfoundation/cf"
    "${WEBCORE_DIR}/platform/graphics/avfoundation/objc"
    "${WEBCORE_DIR}/platform/graphics/ca"
    "${WEBCORE_DIR}/platform/graphics/ca/cocoa"
    "${WEBCORE_DIR}/platform/graphics/cocoa"
    "${WEBCORE_DIR}/platform/graphics/coreimage"
    "${WEBCORE_DIR}/platform/graphics/cg"
    "${WEBCORE_DIR}/platform/graphics/cv"
    "${WEBCORE_DIR}/platform/graphics/gpu"
    "${WEBCORE_DIR}/platform/graphics/gpu/cocoa"
    "${WEBCORE_DIR}/platform/graphics/gpu/legacy"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/mac"
    "${WEBCORE_DIR}/platform/mac"
    "${WEBCORE_DIR}/platform/mediacapabilities"
    "${WEBCORE_DIR}/platform/mediarecorder/cocoa"
    "${WEBCORE_DIR}/platform/mediastream/cocoa"
    "${WEBCORE_DIR}/platform/mediastream/mac"
    "${WEBCORE_DIR}/platform/network/cocoa"
    "${WEBCORE_DIR}/platform/network/cf"
    "${WEBCORE_DIR}/platform/network/mac"
    "${WEBCORE_DIR}/platform/text/cf"
    "${WEBCORE_DIR}/platform/text/cocoa"
    "${WEBCORE_DIR}/platform/text/mac"
    "${WEBCORE_DIR}/platform/spi/cf"
    "${WEBCORE_DIR}/platform/spi/cg"
    "${WEBCORE_DIR}/platform/spi/cocoa"
    "${WEBCORE_DIR}/platform/spi/mac"
    "${WEBCORE_DIR}/platform/video-codecs"
    "${CMAKE_SOURCE_DIR}/Source/WebCore/PAL/ThirdParty/dav1d/include"
    "${WEBCORE_DIR}/plugins/mac"
    "${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}"
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    "${CMAKE_OSX_SYSROOT}/usr/include/libxslt"
    "${CMAKE_OSX_SYSROOT}/usr/include/libxml2"
)

list(APPEND WebCore_SOURCES
    Modules/geolocation/cocoa/GeolocationPositionDataCocoa.mm

    Modules/paymentrequest/MerchantValidationEvent.cpp

    Modules/webaudio/MediaStreamAudioSourceCocoa.cpp

    accessibility/isolatedtree/mac/AXIsolatedObjectMac.mm
    accessibility/mac/AXObjectCacheMac.mm
    accessibility/mac/AccessibilityObjectMac.mm
    accessibility/mac/WebAccessibilityObjectWrapperMac.mm

    dom/DataTransferMac.mm
    dom/SlotAssignment.cpp

    editing/cocoa/AlternativeTextUIController.mm
    editing/cocoa/AutofillElements.cpp

    editing/mac/EditorMac.mm
    editing/mac/TextAlternativeWithRange.mm
    editing/mac/TextUndoInsertionMarkupMac.mm
    editing/mac/UniversalAccessZoom.mm

    html/HTMLSlotElement.cpp

    loader/cocoa/PrivateClickMeasurementCocoa.mm

    page/mac/EventHandlerMac.mm
    page/mac/ServicesOverlayController.mm
    page/mac/WheelEventDeltaFilterMac.mm

    page/scrolling/mac/ScrollingCoordinatorMac.mm
    page/scrolling/mac/ScrollingTreeFrameScrollingNodeMac.mm
    page/scrolling/mac/ScrollingTreeMac.mm

    platform/CPUMonitor.cpp
    platform/LocalizedStrings.cpp
    platform/ScrollableArea.cpp

    platform/audio/AudioSession.cpp

    platform/audio/cocoa/AudioBusCocoa.mm
    platform/audio/cocoa/AudioDecoderCocoa.cpp
    platform/audio/cocoa/AudioEncoderCocoa.cpp
    platform/audio/cocoa/FFTFrameCocoa.cpp
    platform/audio/cocoa/WebAudioBufferList.cpp

    platform/audio/mac/AudioHardwareListenerMac.cpp

    platform/cf/KeyedDecoderCF.cpp
    platform/cf/KeyedEncoderCF.cpp
    platform/cf/MainThreadSharedTimerCF.cpp
    platform/cf/MediaAccessibilitySoftLink.cpp
    platform/cf/SharedBufferCF.cpp

    platform/cocoa/ContentFilterUnblockHandlerCocoa.mm
    platform/cocoa/CoreVideoSoftLink.cpp
    platform/cocoa/FileMonitorCocoa.mm
    platform/cocoa/KeyEventCocoa.mm
    platform/cocoa/LocalizedStringsCocoa.mm
    platform/cocoa/LoggingCocoa.mm
    platform/cocoa/MIMETypeRegistryCocoa.mm
    platform/cocoa/MediaRemoteSoftLink.mm
    platform/cocoa/NetworkExtensionContentFilter.mm
    platform/cocoa/ParentalControlsContentFilter.mm
    platform/cocoa/PasteboardCocoa.mm
    platform/cocoa/SearchPopupMenuCocoa.mm
    platform/cocoa/SharedBufferCocoa.mm
    platform/cocoa/SharedMemoryCocoa.mm
    platform/cocoa/StringUtilities.mm
    platform/cocoa/SystemBattery.mm
    platform/cocoa/SystemVersion.mm
    platform/cocoa/TelephoneNumberDetectorCocoa.cpp
    platform/cocoa/ThemeCocoa.mm
    platform/cocoa/VideoToolboxSoftLink.cpp
    platform/cocoa/WebCoreNSErrorExtras.mm
    platform/cocoa/WebCoreNSURLExtras.mm
    platform/cocoa/WebCoreObjCExtras.mm
    platform/cocoa/WebNSAttributedStringExtras.mm

    platform/gamepad/cocoa/GameControllerSoftLink.mm

    platform/gamepad/mac/HIDGamepad.cpp

    platform/graphics/DisplayRefreshMonitor.cpp
    platform/graphics/DisplayRefreshMonitorManager.cpp
    platform/graphics/FourCC.cpp

    platform/graphics/avfoundation/AVTrackPrivateAVFObjCImpl.mm
    platform/graphics/avfoundation/AudioSourceProviderAVFObjC.mm
    platform/graphics/avfoundation/CDMFairPlayStreaming.cpp
    platform/graphics/avfoundation/InbandMetadataTextTrackPrivateAVF.cpp
    platform/graphics/avfoundation/InbandTextTrackPrivateAVF.cpp
    platform/graphics/avfoundation/LegacyCDMPrivateAVFObjC.mm
    platform/graphics/avfoundation/MediaPlaybackTargetCocoa.mm
    platform/graphics/avfoundation/MediaPlayerPrivateAVFoundation.cpp
    platform/graphics/avfoundation/MediaSelectionGroupAVFObjC.mm
    platform/graphics/avfoundation/WebAVSampleBufferListener.mm

    platform/graphics/avfoundation/objc/AVAssetTrackUtilities.mm
    platform/graphics/avfoundation/objc/AudioTrackPrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/AudioTrackPrivateMediaSourceAVFObjC.cpp
    platform/graphics/avfoundation/objc/CDMInstanceFairPlayStreamingAVFObjC.mm
    platform/graphics/avfoundation/objc/CDMSessionAVContentKeySession.mm
    platform/graphics/avfoundation/objc/CDMSessionAVFoundationObjC.mm
    platform/graphics/avfoundation/objc/ImageDecoderAVFObjC.mm
    platform/graphics/avfoundation/objc/InbandTextTrackPrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/MediaPlayerPrivateAVFoundationObjC.mm
    platform/graphics/avfoundation/objc/MediaPlayerPrivateMediaSourceAVFObjC.mm
    platform/graphics/avfoundation/objc/MediaSampleAVFObjC.mm
    platform/graphics/avfoundation/objc/MediaSourcePrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/QueuedVideoOutput.mm
    platform/graphics/avfoundation/objc/SourceBufferPrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/VideoTrackPrivateAVFObjC.cpp
    platform/graphics/avfoundation/objc/VideoTrackPrivateMediaSourceAVFObjC.mm
    platform/graphics/avfoundation/objc/WebCoreAVFResourceLoader.mm

    platform/graphics/ca/GraphicsLayerCA.cpp
    platform/graphics/ca/LayerPool.cpp
    platform/graphics/ca/PlatformCAAnimation.cpp
    platform/graphics/ca/PlatformCALayer.mm
    platform/graphics/ca/TileController.cpp
    platform/graphics/ca/TileCoverageMap.cpp
    platform/graphics/ca/TileGrid.cpp
    platform/graphics/ca/TransformationMatrixCA.cpp

    platform/graphics/ca/cocoa/GraphicsLayerAsyncContentsDisplayDelegateCocoa.mm
    platform/graphics/ca/cocoa/PlatformCAAnimationCocoa.mm
    platform/graphics/ca/cocoa/PlatformCAFiltersCocoa.mm
    platform/graphics/ca/cocoa/PlatformCALayerCocoa.mm
    platform/graphics/ca/cocoa/PlatformDynamicRangeLimitCocoa.mm
    platform/graphics/ca/cocoa/WebSystemBackdropLayer.mm
    platform/graphics/ca/cocoa/WebTiledBackingLayer.mm

    platform/graphics/cg/CGSubimageCacheWithTimer.cpp
    platform/graphics/cg/ColorCG.cpp
    platform/graphics/cg/ColorSpaceCG.cpp
    platform/graphics/cg/FloatPointCG.cpp
    platform/graphics/cg/FloatRectCG.cpp
    platform/graphics/cg/FloatSizeCG.cpp
    platform/graphics/cg/GradientCG.cpp
    platform/graphics/cg/GradientRendererCG.cpp
    platform/graphics/cg/GraphicsContextGLCG.cpp
    platform/graphics/cg/GraphicsContextCG.cpp
    platform/graphics/cg/IOSurfacePool.cpp
    platform/graphics/cg/ImageBufferCGBackend.cpp
    platform/graphics/cg/ImageBufferCGBitmapBackend.cpp
    platform/graphics/cg/ImageBufferIOSurfaceBackend.cpp
    platform/graphics/cg/ImageDecoderCG.cpp
    platform/graphics/cg/IntPointCG.cpp
    platform/graphics/cg/IntRectCG.cpp
    platform/graphics/cg/IntSizeCG.cpp
    platform/graphics/cg/NativeImageCG.cpp
    platform/graphics/cg/PDFDocumentImage.cpp
    platform/graphics/cg/PathCG.cpp
    platform/graphics/cg/PatternCG.cpp
    platform/graphics/cg/TransformationMatrixCG.cpp
    platform/graphics/cg/UTIRegistry.mm

    platform/graphics/cocoa/CMUtilities.mm
    platform/graphics/cocoa/FloatRectCocoa.mm
    platform/graphics/cocoa/FontCacheCoreText.cpp
    platform/graphics/cocoa/FontCocoa.cpp
    platform/graphics/cocoa/FontDatabase.cpp
    platform/graphics/cocoa/FontDescriptionCocoa.cpp
    platform/graphics/cocoa/FontFamilySpecificationCoreText.cpp
    platform/graphics/cocoa/FontFamilySpecificationCoreTextCache.cpp
    platform/graphics/cocoa/FontPlatformDataCocoa.mm
    platform/graphics/cocoa/GraphicsContextCocoa.mm
    platform/graphics/cocoa/GraphicsContextGLCocoa.mm
    platform/graphics/cocoa/IntRectCocoa.mm
    platform/graphics/cocoa/IOSurface.mm
    platform/graphics/cocoa/IOSurfaceDrawingBuffer.cpp
    platform/graphics/cocoa/IOSurfacePoolCocoa.mm
    platform/graphics/cocoa/UnrealizedCoreTextFont.cpp
    platform/graphics/cocoa/WebActionDisablingCALayerDelegate.mm
    platform/graphics/cocoa/WebCoreCALayerExtras.mm
    platform/graphics/cocoa/WebCoreDecompressionSession.mm
    platform/graphics/cocoa/WebLayer.mm
    platform/graphics/cocoa/WebMAudioUtilitiesCocoa.mm
    platform/graphics/cocoa/WebProcessGraphicsContextGLCocoa.mm

    platform/graphics/coretext/ComplexTextControllerCoreText.mm
    platform/graphics/coretext/FontCascadeCoreText.cpp
    platform/graphics/coretext/FontCoreText.cpp
    platform/graphics/coretext/FontCustomPlatformDataCoreText.cpp
    platform/graphics/coretext/FontPlatformDataCoreText.cpp
    platform/graphics/coretext/GlyphPageCoreText.cpp
    platform/graphics/coretext/SimpleFontDataCoreText.cpp

    platform/graphics/cv/CVUtilities.mm
    platform/graphics/cv/GraphicsContextGLCVCocoa.mm
    platform/graphics/cv/ImageRotationSessionVT.mm
    platform/graphics/cv/PixelBufferConformerCV.cpp

    platform/graphics/mac/ColorMac.mm
    platform/graphics/mac/GraphicsChecksMac.cpp
    platform/graphics/mac/IconMac.mm
    platform/graphics/mac/PDFDocumentImageMac.mm

    platform/graphics/opentype/OpenTypeCG.cpp
    platform/graphics/opentype/OpenTypeMathData.cpp

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
    platform/mac/ScrollingEffectsController.mm
    platform/mac/ScrollViewMac.mm
    platform/mac/ScrollbarThemeMac.mm
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

    platform/mediastream/cocoa/MockRealtimeVideoSourceCocoa.mm
    platform/mediastream/cocoa/RealtimeOutgoingVideoSourceCocoa.cpp

    platform/mediastream/libwebrtc/LibWebRTCAudioModule.cpp

    platform/network/cf/CertificateInfoCFNet.cpp
    platform/network/cf/DNSResolveQueueCFNet.cpp
    platform/network/cf/FormDataStreamCFNet.mm
    platform/network/cf/NetworkStorageSessionCFNet.cpp
    platform/network/cf/ResourceRequestCFNet.cpp

    platform/network/cocoa/AuthenticationCocoa.mm
    platform/network/cocoa/BlobDataFileReferenceCocoa.mm
    platform/network/cocoa/CookieCocoa.mm
    platform/network/cocoa/CookieStorageCocoa.mm
    platform/network/cocoa/CookieStorageObserver.mm
    platform/network/cocoa/CredentialCocoa.mm
    platform/network/cocoa/CredentialStorageCocoa.mm
    platform/network/cocoa/FormDataStreamCocoa.mm
    platform/network/cocoa/NetworkLoadMetrics.mm
    platform/network/cocoa/NetworkStorageSessionCocoa.mm
    platform/network/cocoa/ProtectionSpaceCocoa.mm
    platform/network/cocoa/ResourceErrorCocoa.mm
    platform/network/cocoa/ResourceHandleCocoa.mm
    platform/network/cocoa/ResourceRequestCocoa.mm
    platform/network/cocoa/ResourceResponseCocoa.mm
    platform/network/cocoa/SynchronousLoaderClient.mm
    platform/network/cocoa/UTIUtilities.mm
    platform/network/cocoa/WebCoreNSURLSession.mm
    platform/network/cocoa/WebCoreResourceHandleAsOperationQueueDelegate.mm
    platform/network/cocoa/WebCoreURLResponse.mm

    platform/text/cocoa/LocaleCocoa.mm

    platform/text/cf/HyphenationCF.cpp

    platform/text/cocoa/TextBoundaries.mm
    platform/text/mac/TextCheckingMac.mm

    rendering/TextAutoSizing.cpp

    rendering/cocoa/RenderThemeCocoa.mm

    rendering/mac/RenderThemeMac.mm
)

# Sources that the Xcode build compiles individually (not via SourcesCocoa.txt
# unified sources), so they must be listed here for the CMake build.
list(APPEND WebCore_SOURCES
    Modules/webauthn/AuthenticationExtensionsClientOutputs.cpp

    editing/TextListParser.cpp

    platform/DictationCaretAnimator.cpp
    platform/OpacityCaretAnimator.cpp

    platform/audio/cocoa/AudioSessionCocoa.mm

    platform/cocoa/VideoFullscreenCaptions.mm

    platform/gamepad/cocoa/CoreHapticsSoftLink.mm
    platform/gamepad/cocoa/GameControllerHapticEffect.mm
    platform/gamepad/cocoa/GameControllerHapticEngines.mm

    platform/graphics/cocoa/ANGLEUtilitiesCocoa.mm
    platform/graphics/cocoa/MediaPlayerEnumsCocoa.mm
    platform/graphics/cocoa/TextTransformCocoa.cpp

    platform/mediarecorder/MediaRecorderPrivateWriter.cpp

    platform/mediastream/cocoa/CoreAudioCaptureUnit.mm

    platform/mediastream/libwebrtc/LibWebRTCDav1dDecoder.cpp

    workers/service/ServiceWorkerRoute.mm
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WebCore_DERIVED_SOURCES_DIR}/ModernMediaControls.css

    ${WEBCORE_DIR}/html/shadow/mac/imageControlsMac.css
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    Modules/ShapeDetection/Implementation/Cocoa/BarcodeDetectorImplementation.h
    Modules/ShapeDetection/Implementation/Cocoa/FaceDetectorImplementation.h
    Modules/ShapeDetection/Implementation/Cocoa/TextDetectorImplementation.h

    Modules/airplay/WebMediaSessionManager.h
    Modules/airplay/WebMediaSessionManagerClient.h

    Modules/applepay/ApplePayAutomaticReloadPaymentRequest.h
    Modules/applepay/ApplePayCouponCodeUpdate.h
    Modules/applepay/ApplePayDateComponents.h
    Modules/applepay/ApplePayDateComponentsRange.h
    Modules/applepay/ApplePayDeferredPaymentRequest.h
    Modules/applepay/ApplePayDetailsUpdateBase.h
    Modules/applepay/ApplePayError.h
    Modules/applepay/ApplePayErrorCode.h
    Modules/applepay/ApplePayErrorContactField.h
    Modules/applepay/ApplePayFeature.h
    Modules/applepay/ApplePayLineItem.h
    Modules/applepay/ApplePayPaymentMethodUpdate.h
    Modules/applepay/ApplePayPaymentOrderDetails.h
    Modules/applepay/ApplePayPaymentTiming.h
    Modules/applepay/ApplePayPaymentTokenContext.h
    Modules/applepay/ApplePayRecurringPaymentDateUnit.h
    Modules/applepay/ApplePayRecurringPaymentRequest.h
    Modules/applepay/ApplePaySetupConfiguration.h
    Modules/applepay/ApplePaySetupFeatureWebCore.h
    Modules/applepay/ApplePayShippingContactEditingMode.h
    Modules/applepay/ApplePayShippingContactUpdate.h
    Modules/applepay/ApplePayShippingMethod.h
    Modules/applepay/ApplePayShippingMethodUpdate.h
    Modules/applepay/PaymentInstallmentConfigurationWebCore.h
    Modules/applepay/PaymentSessionError.h
    Modules/applepay/PaymentSummaryItems.h

    Modules/encryptedmedia/legacy/LegacyCDM.h
    Modules/encryptedmedia/legacy/LegacyCDMPrivate.h

    Modules/mediasession/MediaPositionState.h
    Modules/mediasession/MediaSession.h
    Modules/mediasession/MediaSessionAction.h
    Modules/mediasession/MediaSessionActionDetails.h
    Modules/mediasession/MediaSessionActionHandler.h
    Modules/mediasession/MediaSessionCoordinator.h
    Modules/mediasession/MediaSessionCoordinatorPrivate.h
    Modules/mediasession/MediaSessionCoordinatorState.h
    Modules/mediasession/MediaSessionPlaybackState.h
    Modules/mediasession/MediaSessionReadyState.h
    Modules/mediasession/NavigatorMediaSession.h

    Modules/webauthn/AuthenticatorAssertionResponse.h
    Modules/webauthn/AuthenticatorAttachment.h
    Modules/webauthn/AuthenticatorAttestationResponse.h
    Modules/webauthn/AuthenticatorResponse.h

    Modules/webauthn/fido/Pin.h

    accessibility/cocoa/CocoaAccessibilityConstants.h
    accessibility/cocoa/WebAccessibilityObjectWrapperBase.h

    accessibility/mac/WebAccessibilityObjectWrapperMac.h

    bridge/objc/WebScriptObject.h
    bridge/objc/WebScriptObjectPrivate.h

    crypto/CryptoAlgorithmIdentifier.h
    crypto/CryptoKey.h
    crypto/CryptoKeyType.h
    crypto/CryptoKeyUsage.h
    crypto/CryptoKeyPair.h
    crypto/CommonCryptoUtilities.h

    crypto/keys/CryptoKeyHMAC.h
    crypto/keys/CryptoAesKeyAlgorithm.h
    crypto/keys/CryptoEcKeyAlgorithm.h
    crypto/keys/CryptoHmacKeyAlgorithm.h
    crypto/keys/CryptoKeyAES.h
    crypto/keys/CryptoKeyAlgorithm.h
    crypto/keys/CryptoRsaHashedKeyAlgorithm.h
    crypto/keys/CryptoRsaKeyAlgorithm.h
    crypto/keys/CryptoKeyEC.h

    dom/EventLoop.h
    dom/WindowEventLoop.h

    editing/cocoa/AlternativeTextContextController.h
    editing/cocoa/AlternativeTextUIController.h
    editing/cocoa/AttributedString.h
    editing/cocoa/AutofillElements.h
    editing/cocoa/DataDetection.h
    editing/cocoa/DataDetectorType.h
    editing/cocoa/EditingHTMLConverter.h
    editing/cocoa/HTMLConverter.h
    editing/cocoa/NodeHTMLConverter.h
    editing/cocoa/TextAttachmentForSerialization.h
    editing/cocoa/WebArchiveResourceFromNSAttributedString.h
    editing/cocoa/WebArchiveResourceWebResourceHandler.h
    editing/cocoa/WebCoreTextAttachment.h

    editing/mac/DictionaryLookup.h
    editing/mac/TextAlternativeWithRange.h
    editing/mac/TextUndoInsertionMarkupMac.h
    editing/mac/UniversalAccessZoom.h

    loader/archive/cf/LegacyWebArchive.h

    loader/cache/CachedRawResource.h

    loader/mac/LoaderNSURLExtras.h

    page/CaptionUserPreferencesMediaAF.h
    page/FrameDestructionObserverInlines.h

    page/cocoa/DataDetectionResultsStorage.h
    page/cocoa/DataDetectorElementInfo.h
    page/cocoa/ImageOverlayDataDetectionResultIdentifier.h

    page/mac/WebCoreFrameView.h

    page/scrolling/ScrollingStateOverflowScrollProxyNode.h

    page/scrolling/cocoa/ScrollingTreeFixedNodeCocoa.h
    page/scrolling/cocoa/ScrollingTreeOverflowScrollProxyNodeCocoa.h
    page/scrolling/cocoa/ScrollingTreePositionedNodeCocoa.h
    page/scrolling/cocoa/ScrollingTreeStickyNodeCocoa.h

    page/scrolling/mac/ScrollerMac.h
    page/scrolling/mac/ScrollerPairMac.h
    page/scrolling/mac/ScrollingCoordinatorMac.h
    page/scrolling/mac/ScrollingTreeFrameScrollingNodeMac.h
    page/scrolling/mac/ScrollingTreeOverflowScrollingNodeMac.h
    page/scrolling/mac/ScrollingTreeScrollingNodeDelegateMac.h

    platform/CaptionPreferencesDelegate.h
    platform/FrameRateMonitor.h
    platform/MainThreadSharedTimer.h
    platform/PictureInPictureSupport.h
    platform/PlatformContentFilter.h
    platform/ScrollAlignment.h
    platform/ScrollAnimation.h
    platform/ScrollSnapAnimatorState.h
    platform/ScrollingEffectsController.h
    platform/SharedTimer.h
    platform/SystemSoundManager.h
    platform/TextRecognitionResult.h

    platform/audio/cocoa/AudioDecoderCocoa.h
    platform/audio/cocoa/AudioDestinationCocoa.h
    platform/audio/cocoa/AudioEncoderCocoa.h
    platform/audio/cocoa/AudioFileReaderCocoa.h
    platform/audio/cocoa/AudioOutputUnitAdaptor.h
    platform/audio/cocoa/AudioSampleBufferConverter.h
    platform/audio/cocoa/AudioSampleBufferList.h
    platform/audio/cocoa/AudioSampleDataConverter.h
    platform/audio/cocoa/AudioSampleDataSource.h
    platform/audio/cocoa/AudioSessionCocoa.h
    platform/audio/cocoa/CAAudioStreamDescription.h
    platform/audio/cocoa/CARingBuffer.h
    platform/audio/cocoa/MediaSessionManagerCocoa.h
    platform/audio/cocoa/PlatformRawAudioDataCocoa.h
    platform/audio/cocoa/SpatialAudioExperienceHelper.h
    platform/audio/cocoa/SpatialAudioPlaybackHelper.h
    platform/audio/cocoa/WebAudioBufferList.h

    platform/audio/mac/AudioHardwareListenerMac.h
    platform/audio/mac/AudioSessionMac.h
    platform/audio/mac/SharedRoutingArbitrator.h

    platform/cf/KeyedDecoderCF.h
    platform/cf/KeyedEncoderCF.h
    platform/cf/MediaAccessibilitySoftLink.h

    platform/cocoa/AppleVisualEffect.h
    platform/cocoa/CocoaView.h
    platform/cocoa/CocoaWritingToolsTypes.h
    platform/cocoa/CoreLocationGeolocationProvider.h
    platform/cocoa/CoreVideoExtras.h
    platform/cocoa/CoreVideoSoftLink.h
    platform/cocoa/EffectiveRateChangedListener.h
    platform/cocoa/KeyEventCocoa.h
    platform/cocoa/LocalCurrentGraphicsContext.h
    platform/cocoa/MediaRemoteSoftLink.h
    platform/cocoa/MediaUtilities.h
    platform/cocoa/NSURLUtilities.h
    platform/cocoa/NetworkExtensionContentFilter.h
    platform/cocoa/ParentalControlsContentFilter.h
    platform/cocoa/ParentalControlsURLFilter.h
    platform/cocoa/ParentalControlsURLFilterParameters.h
    platform/cocoa/PhotosFormatSoftLink.h
    platform/cocoa/PlatformNSAdaptiveImageGlyph.h
    platform/cocoa/PlatformTextAlternatives.h
    platform/cocoa/PlatformViewController.h
    platform/cocoa/PlaybackSessionModel.h
    platform/cocoa/PlaybackSessionModelMediaElement.h
    platform/cocoa/PowerSourceNotifier.h
    platform/cocoa/RemoteCommandListenerCocoa.h
    platform/cocoa/SearchPopupMenuCocoa.h
    platform/cocoa/SharedVideoFrameInfo.h
    platform/cocoa/StringUtilities.h
    platform/cocoa/SystemBattery.h
    platform/cocoa/SystemVersion.h
    platform/cocoa/VideoFullscreenCaptions.h
    platform/cocoa/VideoPresentationLayerProvider.h
    platform/cocoa/VideoPresentationModel.h
    platform/cocoa/VideoPresentationModelVideoElement.h
    platform/cocoa/VideoToolboxSoftLink.h
    platform/cocoa/WebAVPlayerLayer.h
    platform/cocoa/WebAVPlayerLayerView.h
    platform/cocoa/WebCoreNSErrorExtras.h
    platform/cocoa/WebCoreNSURLExtras.h
    platform/cocoa/WebCoreObjCExtras.h
    platform/cocoa/WebNSAttributedStringExtras.h

    platform/gamepad/cocoa/GameControllerGamepadProvider.h
    platform/gamepad/cocoa/GameControllerSPI.h

    platform/gamepad/mac/HIDGamepad.h
    platform/gamepad/mac/HIDGamepadElement.h
    platform/gamepad/mac/HIDGamepadProvider.h
    platform/gamepad/mac/MultiGamepadProvider.h

    platform/graphics/ImageDecoder.h
    platform/graphics/ImageDecoderIdentifier.h
    platform/graphics/ImageUtilities.h
    platform/graphics/MIMETypeCache.h
    platform/graphics/Model.h

    platform/graphics/angle/ANGLEUtilities.h

    platform/graphics/avfoundation/AVTrackPrivateAVFObjCImpl.h
    platform/graphics/avfoundation/AudioSourceProviderAVFObjC.h
    platform/graphics/avfoundation/AudioTrackPrivateAVF.h
    platform/graphics/avfoundation/AudioVideoRendererAVFObjC.h
    platform/graphics/avfoundation/CDMFairPlayStreaming.h
    platform/graphics/avfoundation/ContentKeyGroupDataSource.h
    platform/graphics/avfoundation/FormatDescriptionUtilities.h
    platform/graphics/avfoundation/ISOFairPlayStreamingPsshBox.h
    platform/graphics/avfoundation/InbandMetadataTextTrackPrivateAVF.h
    platform/graphics/avfoundation/InbandTextTrackPrivateAVF.h
    platform/graphics/avfoundation/LegacyCDMPrivateAVFObjC.h
    platform/graphics/avfoundation/MediaPlaybackTargetCocoa.h
    platform/graphics/avfoundation/MediaPlayerPrivateAVFoundation.h
    platform/graphics/avfoundation/MediaSelectionGroupAVFObjC.h
    platform/graphics/avfoundation/SampleBufferDisplayLayer.h
    platform/graphics/avfoundation/VideoTrackPrivateAVF.h
    platform/graphics/avfoundation/WebAVSampleBufferListener.h
    platform/graphics/avfoundation/WebMediaSessionManagerMac.h

    platform/graphics/avfoundation/objc/AVAssetMIMETypeCache.h
    platform/graphics/avfoundation/objc/AVOutputDeviceMenuControllerTargetPicker.h
    platform/graphics/avfoundation/objc/AVPlaybackTargetPicker.h
    platform/graphics/avfoundation/objc/AVRoutePickerViewTargetPicker.h
    platform/graphics/avfoundation/objc/AVStreamDataParserMIMETypeCache.h
    platform/graphics/avfoundation/objc/AudioTrackPrivateAVFObjC.h
    platform/graphics/avfoundation/objc/AudioTrackPrivateMediaSourceAVFObjC.h
    platform/graphics/avfoundation/objc/CDMInstanceFairPlayStreamingAVFObjC.h
    platform/graphics/avfoundation/objc/CDMSessionAVContentKeySession.h
    platform/graphics/avfoundation/objc/CDMSessionAVFoundationObjC.h
    platform/graphics/avfoundation/objc/ContentKeyGroupFactoryAVFObjC.h
    platform/graphics/avfoundation/objc/ImageDecoderAVFObjC.h
    platform/graphics/avfoundation/objc/InbandChapterTrackPrivateAVFObjC.h
    platform/graphics/avfoundation/objc/InbandTextTrackPrivateAVFObjC.h
    platform/graphics/avfoundation/objc/LocalSampleBufferDisplayLayer.h
    platform/graphics/avfoundation/objc/MediaPlayerPrivateAVFoundationObjC.h
    platform/graphics/avfoundation/objc/MediaPlayerPrivateMediaSourceAVFObjC.h
    platform/graphics/avfoundation/objc/MediaPlayerPrivateMediaStreamAVFObjC.h
    platform/graphics/avfoundation/objc/MediaSampleAVFObjC.h
    platform/graphics/avfoundation/objc/MediaSourcePrivateAVFObjC.h
    platform/graphics/avfoundation/objc/OutOfBandTextTrackPrivateAVF.h
    platform/graphics/avfoundation/objc/QueuedVideoOutput.h
    platform/graphics/avfoundation/objc/SourceBufferParserAVFObjC.h
    platform/graphics/avfoundation/objc/SourceBufferPrivateAVFObjC.h
    platform/graphics/avfoundation/objc/TextTrackPrivateMediaSourceAVFObjC.h
    platform/graphics/avfoundation/objc/VideoLayerManagerObjC.h
    platform/graphics/avfoundation/objc/VideoTrackPrivateAVFObjC.h
    platform/graphics/avfoundation/objc/VideoTrackPrivateMediaSourceAVFObjC.h
    platform/graphics/avfoundation/objc/WebAVContentKeyGroup.h
    platform/graphics/avfoundation/objc/WebCoreAVFResourceLoader.h

    platform/graphics/ca/GraphicsLayerCA.h
    platform/graphics/ca/LayerPool.h
    platform/graphics/ca/PlatformCAAnimation.h
    platform/graphics/ca/PlatformCAFilters.h
    platform/graphics/ca/PlatformCALayer.h
    platform/graphics/ca/PlatformCALayerClient.h
    platform/graphics/ca/PlatformCALayerDelegatedContents.h
    platform/graphics/ca/TileController.h
    platform/graphics/ca/TileCoverageMap.h
    platform/graphics/ca/TileGrid.h

    platform/graphics/ca/cocoa/ContentsFormatCocoa.h
    platform/graphics/ca/cocoa/GraphicsLayerAsyncContentsDisplayDelegateCocoa.h
    platform/graphics/ca/cocoa/PlatformCAAnimationCocoa.h
    platform/graphics/ca/cocoa/PlatformCALayerCocoa.h
    platform/graphics/ca/cocoa/PlatformCALayerContentsDelayedReleaser.h
    platform/graphics/ca/cocoa/PlatformDynamicRangeLimitCocoa.h
    platform/graphics/ca/cocoa/WebSystemBackdropLayer.h
    platform/graphics/ca/cocoa/WebTiledBackingLayer.h
    platform/graphics/ca/cocoa/WebVideoContainerLayer.h

    platform/graphics/cg/CGContextStateSaver.h
    platform/graphics/cg/CGSubimageCacheWithTimer.h
    platform/graphics/cg/CGUtilities.h
    platform/graphics/cg/CGWindowUtilities.h
    platform/graphics/cg/ColorSpaceCG.h
    platform/graphics/cg/GradientRendererCG.h
    platform/graphics/cg/GraphicsContextCG.h
    platform/graphics/cg/IOSurfacePool.h
    platform/graphics/cg/IOSurfacePoolIdentifier.h
    platform/graphics/cg/ImageBufferCGBackend.h
    platform/graphics/cg/ImageBufferCGBitmapBackend.h
    platform/graphics/cg/ImageBufferCGPDFDocumentBackend.h
    platform/graphics/cg/ImageBufferIOSurfaceBackend.h
    platform/graphics/cg/ImageDecoderCG.h
    platform/graphics/cg/PDFDocumentImage.h
    platform/graphics/cg/PathCG.h
    platform/graphics/cg/ShareableSpatialImage.h
    platform/graphics/cg/SpatialImageTypes.h
    platform/graphics/cg/UTIRegistry.h

    platform/graphics/cocoa/ANGLEUtilitiesCocoa.h
    platform/graphics/cocoa/AV1UtilitiesCocoa.h
    platform/graphics/cocoa/AudioTrackPrivateWebM.h
    platform/graphics/cocoa/CMUtilities.h
    platform/graphics/cocoa/ColorCocoa.h
    platform/graphics/cocoa/DynamicContentScalingDisplayList.h
    platform/graphics/cocoa/FontCacheCoreText.h
    platform/graphics/cocoa/FontCascadeCocoaInlines.h
    platform/graphics/cocoa/FontCocoa.h
    platform/graphics/cocoa/FontDatabase.h
    platform/graphics/cocoa/FontFamilySpecificationCoreText.h
    platform/graphics/cocoa/FontFamilySpecificationCoreTextCache.h
    platform/graphics/cocoa/FontInterrogation.h
    platform/graphics/cocoa/FontMetricsNormalization.h
    platform/graphics/cocoa/GraphicsContextGLCocoa.h
    platform/graphics/cocoa/H264UtilitiesCocoa.h
    platform/graphics/cocoa/HEVCUtilitiesCocoa.h
    platform/graphics/cocoa/IOSurface.h
    platform/graphics/cocoa/IOSurfaceDrawingBuffer.h
    platform/graphics/cocoa/MediaPlayerEnumsCocoa.h
    platform/graphics/cocoa/MediaPlayerPrivateWebM.h
    platform/graphics/cocoa/MultiRepresentationHEICMetrics.h
    platform/graphics/cocoa/NullPlaybackSessionInterface.h
    platform/graphics/cocoa/NullVideoPresentationInterface.h
    platform/graphics/cocoa/PlatformMediaEngineConfigurationFactoryCocoa.h
    platform/graphics/cocoa/SourceBufferParser.h
    platform/graphics/cocoa/SourceBufferParserWebM.h
    platform/graphics/cocoa/SystemFontDatabaseCoreText.h
    platform/graphics/cocoa/TextTrackRepresentationCocoa.h
    platform/graphics/cocoa/UnrealizedCoreTextFont.h
    platform/graphics/cocoa/VP9UtilitiesCocoa.h
    platform/graphics/cocoa/VP9UtilitiesCocoaInternal.h
    platform/graphics/cocoa/VideoMediaSampleRenderer.h
    platform/graphics/cocoa/VideoTargetFactory.h
    platform/graphics/cocoa/VideoTrackPrivateWebM.h
    platform/graphics/cocoa/WebActionDisablingCALayerDelegate.h
    platform/graphics/cocoa/WebCoreCALayerExtras.h
    platform/graphics/cocoa/WebCoreDecompressionSession.h
    platform/graphics/cocoa/WebLayer.h
    platform/graphics/cocoa/WebMAudioUtilitiesCocoa.h
    platform/graphics/cocoa/WebSampleBufferVideoRendering.h

    platform/graphics/cv/CVUtilities.h
    platform/graphics/cv/GraphicsContextGLCV.h
    platform/graphics/cv/GraphicsContextGLCVCocoa.h
    platform/graphics/cv/ImageRotationSessionVT.h
    platform/graphics/cv/ImageTransferSessionVT.h
    platform/graphics/cv/PixelBufferConformerCV.h
    platform/graphics/cv/VideoFrameCV.h

    platform/graphics/mac/AppKitControlSystemImage.h
    platform/graphics/mac/ColorMac.h
    platform/graphics/mac/GraphicsChecksMac.h
    platform/graphics/mac/LegacyDisplayRefreshMonitorMac.h
    platform/graphics/mac/ScrollbarTrackCornerSystemImageMac.h
    platform/graphics/mac/WebKitNSImageExtras.h

    platform/ios/AbstractPasteboard.h
    platform/ios/DeviceMotionClientIOS.h
    platform/ios/DeviceOrientationClientIOS.h
    platform/ios/DeviceOrientationUpdateProvider.h
    platform/ios/KeyEventCodesIOS.h
    platform/ios/LegacyTileCache.h
    platform/ios/LegacyTileGrid.h
    platform/ios/LegacyTileGridTile.h
    platform/ios/LegacyTileLayer.h
    platform/ios/LegacyTileLayerPool.h
    platform/ios/LocalCurrentTraitCollection.h
    platform/ios/LocalizedDeviceModel.h
    platform/ios/MotionManagerClient.h
    platform/ios/PlatformEventFactoryIOS.h
    platform/ios/PlaybackSessionInterfaceAVKitLegacy.h
    platform/ios/PlaybackSessionInterfaceIOS.h
    platform/ios/PlaybackSessionInterfaceTVOS.h
    platform/ios/QuickLook.h
    platform/ios/ScrollAnimatorIOS.h
    platform/ios/ScrollbarThemeIOS.h
    platform/ios/SystemMemory.h
    platform/ios/TileControllerMemoryHandlerIOS.h
    platform/ios/UIFoundationSoftLink.h
    platform/ios/UIViewControllerUtilities.h
    platform/ios/VideoPresentationInterfaceAVKitLegacy.h
    platform/ios/VideoPresentationInterfaceIOS.h
    platform/ios/VideoPresentationInterfaceTVOS.h
    platform/ios/WebAVPlayerController.h
    platform/ios/WebBackgroundTaskController.h
    platform/ios/WebCoreMotionManager.h
    platform/ios/WebEvent.h
    platform/ios/WebEventPrivate.h
    platform/ios/WebItemProviderPasteboard.h
    platform/ios/WebSQLiteDatabaseTrackerClient.h
    platform/ios/WebVideoFullscreenControllerAVKit.h

    platform/ios/wak/FloatingPointEnvironment.h
    platform/ios/wak/WAKAppKitStubs.h
    platform/ios/wak/WAKClipView.h
    platform/ios/wak/WAKResponder.h
    platform/ios/wak/WAKScrollView.h
    platform/ios/wak/WAKView.h
    platform/ios/wak/WAKViewInternal.h
    platform/ios/wak/WAKWindow.h
    platform/ios/wak/WKContentObservation.h
    platform/ios/wak/WKGraphics.h
    platform/ios/wak/WKTypes.h
    platform/ios/wak/WKUtilities.h
    platform/ios/wak/WKView.h
    platform/ios/wak/WKViewPrivate.h
    platform/ios/wak/WebCoreThread.h
    platform/ios/wak/WebCoreThreadInternal.h
    platform/ios/wak/WebCoreThreadMessage.h
    platform/ios/wak/WebCoreThreadRun.h
    platform/ios/wak/WebCoreThreadSystemInterface.h

    platform/mac/DataDetectorHighlight.h
    platform/mac/HIDDevice.h
    platform/mac/HIDElement.h
    platform/mac/LegacyNSPasteboardTypes.h
    platform/mac/LocalDefaultSystemAppearance.h
    platform/mac/NSScrollerImpDetails.h
    platform/mac/PasteboardWriter.h
    platform/mac/PlatformEventFactoryMac.h
    platform/mac/PlaybackSessionInterfaceMac.h
    platform/mac/PowerObserverMac.h
    platform/mac/RevealUtilities.h
    platform/mac/ScrollAnimationRubberBand.h
    platform/mac/ScrollAnimatorMac.h
    platform/mac/ScrollTypesMac.h
    platform/mac/ScrollbarMac.h
    platform/mac/ScrollbarThemeMac.h
    platform/mac/ScrollbarsControllerMac.h
    platform/mac/ScrollingMomentumCalculatorMac.h
    platform/mac/SerializedPlatformDataCueMac.h
    platform/mac/VideoPresentationInterfaceMac.h
    platform/mac/WebCoreFullScreenPlaceholderView.h
    platform/mac/WebCoreFullScreenWarningView.h
    platform/mac/WebCoreFullScreenWindow.h
    platform/mac/WebCoreNSFontManagerExtras.h
    platform/mac/WebCoreView.h
    platform/mac/WebPlaybackControlsManager.h

    platform/mediarecorder/MediaRecorderPrivateEncoder.h
    platform/mediarecorder/MediaRecorderPrivateOptions.h

    platform/mediarecorder/cocoa/MediaRecorderPrivateWriterAVFObjC.h
    platform/mediarecorder/cocoa/MediaRecorderPrivateWriterWebM.h

    platform/mediastream/AudioMediaStreamTrackRenderer.h
    platform/mediastream/RealtimeIncomingVideoSource.h
    platform/mediastream/RealtimeMediaSourceIdentifier.h

    platform/mediastream/cocoa/AudioMediaStreamTrackRendererInternalUnit.h
    platform/mediastream/cocoa/AudioMediaStreamTrackRendererUnit.h
    platform/mediastream/cocoa/BaseAudioCaptureUnit.h
    platform/mediastream/cocoa/BaseAudioMediaStreamTrackRendererUnit.h
    platform/mediastream/cocoa/CoreAudioCaptureSource.h
    platform/mediastream/cocoa/DisplayCaptureSourceCocoa.h
    platform/mediastream/cocoa/RealtimeIncomingVideoSourceCocoa.h
    platform/mediastream/cocoa/RealtimeVideoUtilities.h
    platform/mediastream/cocoa/ScreenCaptureKitCaptureSource.h
    platform/mediastream/cocoa/ScreenCaptureKitSharingSessionManager.h
    platform/mediastream/cocoa/WebAudioSourceProviderCocoa.h

    platform/mediastream/libwebrtc/LibWebRTCProviderCocoa.h
    platform/mediastream/libwebrtc/VideoFrameLibWebRTC.h

    platform/network/cf/AuthenticationChallenge.h
    platform/network/cf/CertificateInfo.h
    platform/network/cf/DNSResolveQueueCFNet.h
    platform/network/cf/FormDataStreamCFNet.h
    platform/network/cf/ResourceError.h
    platform/network/cf/ResourceRequest.h
    platform/network/cf/ResourceRequestCFNet.h
    platform/network/cf/ResourceResponse.h

    platform/network/cocoa/AuthenticationCocoa.h
    platform/network/cocoa/CookieStorageObserver.h
    platform/network/cocoa/CredentialCocoa.h
    platform/network/cocoa/FormDataStreamCocoa.h
    platform/network/cocoa/HTTPCookieAcceptPolicyCocoa.h
    platform/network/cocoa/ProtectionSpaceCocoa.h
    platform/network/cocoa/RangeResponseGenerator.h
    platform/network/cocoa/UTIUtilities.h
    platform/network/cocoa/WebCoreNSURLSession.h
    platform/network/cocoa/WebCoreResourceHandleAsOperationQueueDelegate.h
    platform/network/cocoa/WebCoreURLResponse.h

    platform/video-codecs/cocoa/WebRTCVideoDecoder.h

    rendering/cocoa/RenderThemeCocoa.h

    rendering/ios/RenderThemeIOS.h

    rendering/mac/RenderThemeMac.h

    testing/MockWebAuthenticationConfiguration.h

    testing/cocoa/WebViewVisualIdentificationOverlay.h
)

# Additional private headers needed by the WebKit target (present in Xcode
# build's WebCore.framework/PrivateHeaders but missing from CMake).
list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    accessibility/ios/AXRemoteTokenIOS.h

    editing/ICUSearcher.h

    html/track/WebVTTParser.h

    page/cocoa/ContentChangeObserver.h
    page/cocoa/DOMTimerHoldingTank.h
    page/cocoa/WebTextIndicatorLayer.h

    page/ios/WebEventRegion.h

    page/mac/CorrectionIndicator.h

    page/scrolling/mac/ScrollingTreePluginScrollingNodeMac.h

    page/writing-tools/TextEffectController.h

    platform/WebCoreMainThread.h

    platform/audio/cocoa/AudioUtilitiesCocoa.h

    platform/audio/ios/MediaDeviceRouteController.h
    platform/audio/ios/MediaDeviceRouteLoadURLResult.h
    platform/audio/ios/MediaSessionHelperIOS.h
    platform/audio/ios/MediaSessionManagerIOS.h

    platform/cocoa/PlaybackSessionModel.serialization.in
    platform/cocoa/ThemeCocoa.h
    platform/cocoa/WebKitAvailability.h

    platform/gamepad/cocoa/GameControllerSoftLink.h

    platform/graphics/FontInlines.h
    platform/graphics/MediaPlaybackTargetWirelessPlayback.h
    platform/graphics/MediaSourceTypeSupportedCache.h

    platform/mediastream/cocoa/AVVideoCaptureSource.h
    platform/mediastream/cocoa/CoreAudioCaptureDeviceManager.h
    platform/mediastream/cocoa/CoreAudioCaptureUnit.h

    platform/mediastream/ios/AVAudioSessionCaptureDeviceManager.h

    platform/network/ios/LegacyPreviewLoaderClient.h
    platform/network/ios/WebCoreURLResponseIOS.h

    platform/text/cocoa/CoreTextCompositionEngine.h

    platform/xr/cocoa/PlatformXRPose.h
)

list(APPEND WebCore_IDL_FILES
    Modules/applepay/ApplePayAutomaticReloadPaymentRequest.idl
    Modules/applepay/ApplePayCancelEvent.idl
    Modules/applepay/ApplePayContactField.idl
    Modules/applepay/ApplePayCouponCodeChangedEvent.idl
    Modules/applepay/ApplePayCouponCodeDetails.idl
    Modules/applepay/ApplePayCouponCodeUpdate.idl
    Modules/applepay/ApplePayDateComponents.idl
    Modules/applepay/ApplePayDateComponentsRange.idl
    Modules/applepay/ApplePayDeferredPaymentRequest.idl
    Modules/applepay/ApplePayDetailsUpdateBase.idl
    Modules/applepay/ApplePayDisbursementRequest.idl
    Modules/applepay/ApplePayError.idl
    Modules/applepay/ApplePayErrorCode.idl
    Modules/applepay/ApplePayErrorContactField.idl
    Modules/applepay/ApplePayFeature.idl
    Modules/applepay/ApplePayInstallmentItem.idl
    Modules/applepay/ApplePayInstallmentItemType.idl
    Modules/applepay/ApplePayInstallmentConfiguration.idl
    Modules/applepay/ApplePayInstallmentRetailChannel.idl
    Modules/applepay/ApplePayLaterAvailability.idl
    Modules/applepay/ApplePayLineItem.idl
    Modules/applepay/ApplePayMerchantCapability.idl
    Modules/applepay/ApplePayPayment.idl
    Modules/applepay/ApplePayPaymentAuthorizationResult.idl
    Modules/applepay/ApplePayPaymentAuthorizedEvent.idl
    Modules/applepay/ApplePayPaymentContact.idl
    Modules/applepay/ApplePayPaymentMethod.idl
    Modules/applepay/ApplePayPaymentMethodSelectedEvent.idl
    Modules/applepay/ApplePayPaymentMethodType.idl
    Modules/applepay/ApplePayPaymentMethodUpdate.idl
    Modules/applepay/ApplePayPaymentOrderDetails.idl
    Modules/applepay/ApplePayPaymentPass.idl
    Modules/applepay/ApplePayPaymentRequest.idl
    Modules/applepay/ApplePayPaymentTiming.idl
    Modules/applepay/ApplePayPaymentTokenContext.idl
    Modules/applepay/ApplePayRecurringPaymentDateUnit.idl
    Modules/applepay/ApplePayRecurringPaymentRequest.idl
    Modules/applepay/ApplePayRequestBase.idl
    Modules/applepay/ApplePaySession.idl
    Modules/applepay/ApplePaySessionError.idl
    Modules/applepay/ApplePaySetup.idl
    Modules/applepay/ApplePaySetupConfiguration.idl
    Modules/applepay/ApplePaySetupFeature.idl
    Modules/applepay/ApplePaySetupFeatureState.idl
    Modules/applepay/ApplePaySetupFeatureType.idl
    Modules/applepay/ApplePayShippingContactEditingMode.idl
    Modules/applepay/ApplePayShippingContactSelectedEvent.idl
    Modules/applepay/ApplePayShippingContactUpdate.idl
    Modules/applepay/ApplePayShippingMethod.idl
    Modules/applepay/ApplePayShippingMethodSelectedEvent.idl
    Modules/applepay/ApplePayShippingMethodUpdate.idl
    Modules/applepay/ApplePayValidateMerchantEvent.idl

    Modules/applepay/paymentrequest/ApplePayModifier.idl
    Modules/applepay/paymentrequest/ApplePayPaymentCompleteDetails.idl
    Modules/applepay/paymentrequest/ApplePayRequest.idl

    Modules/applepay-ams-ui/ApplePayAMSUIRequest.idl
)

set(FEATURE_DEFINES_OBJECTIVE_C "LANGUAGE_OBJECTIVE_C=1 ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}")
set(ADDITIONAL_BINDINGS_DEPENDENCIES
    ${WINDOW_CONSTRUCTORS_FILE}
    ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
    ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
)
set(CSS_VALUE_PLATFORM_DEFINES "WTF_PLATFORM_MAC WTF_PLATFORM_COCOA ENABLE_APPLE_PAY_NEW_BUTTON_TYPES HAVE_CORE_MATERIAL HAVE_CORE_ANIMATION_SEPARATED_LAYERS HAVE_MATERIAL_HOSTING")

set(WebCore_USER_AGENT_SCRIPTS ${WebCore_DERIVED_SOURCES_DIR}/ModernMediaControls.js)

list(APPEND WebCoreTestSupport_LIBRARIES PRIVATE WebCore)
list(APPEND WebCoreTestSupport_PRIVATE_HEADERS testing/cocoa/WebArchiveDumpSupport.h)
list(APPEND WebCoreTestSupport_SOURCES
    testing/Internals.mm
    testing/MockApplePaySetupFeature.cpp
    testing/MockContentFilter.cpp
    testing/MockContentFilterSettings.cpp
    testing/MockMediaSessionCoordinator.cpp
    testing/MockPaymentCoordinator.cpp
    testing/MockPreviewLoaderClient.cpp
    testing/ServiceWorkerInternals.mm

    testing/cocoa/WebArchiveDumpSupport.mm
)
list(APPEND WebCoreTestSupport_IDL_FILES
    testing/MockPaymentAddress.idl
    testing/MockPaymentContactFields.idl
    testing/MockPaymentCoordinator.idl
    testing/MockPaymentError.idl
)

if (NOT EXISTS ${CMAKE_BINARY_DIR}/WebCore/WebKitAvailability.h)
    file(COPY platform/cocoa/WebKitAvailability.h DESTINATION ${CMAKE_BINARY_DIR}/WebCore)
endif ()
