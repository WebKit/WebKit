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
find_library(CORESERVICES_LIBRARY CoreServices)
find_library(DISKARBITRATION_LIBRARY DiskArbitration)
find_library(IOKIT_LIBRARY IOKit)
find_library(IOSURFACE_LIBRARY IOSurface)
find_library(METAL_LIBRARY Metal)
find_library(OPENGL_LIBRARY OpenGL)
find_library(QUARTZ_LIBRARY Quartz)
find_library(QUARTZCORE_LIBRARY QuartzCore)
find_library(SECURITY_LIBRARY Security)
find_library(SYSTEMCONFIGURATION_LIBRARY SystemConfiguration)
find_library(XML2_LIBRARY XML2)
find_package(Sqlite3 REQUIRED)
find_package(ZLIB REQUIRED)

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
    ${CORESERVICES_LIBRARY}
    ${DISKARBITRATION_LIBRARY}
    ${IOKIT_LIBRARY}
    ${IOSURFACE_LIBRARY}
    ${METAL_LIBRARY}
    ${OPENGL_LIBRARY}
    ${QUARTZ_LIBRARY}
    ${QUARTZCORE_LIBRARY}
    ${SECURITY_LIBRARY}
    ${SQLITE3_LIBRARIES}
    ${SYSTEMCONFIGURATION_LIBRARY}
    ${XML2_LIBRARY}
)

add_definitions(-iframework ${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks)
add_definitions(-iframework ${AVFOUNDATION_LIBRARY}/Versions/Current/Frameworks)
add_definitions(-iframework ${CARBON_LIBRARY}/Versions/Current/Frameworks)
add_definitions(-iframework ${CORESERVICES_LIBRARY}/Versions/Current/Frameworks)
add_definitions(-iframework ${QUARTZ_LIBRARY}/Frameworks)

find_library(COREUI_FRAMEWORK CoreUI HINTS /System/Library/PrivateFrameworks)
if (NOT COREUI_FRAMEWORK-NOTFOUND)
    list(APPEND WebCore_LIBRARIES ${COREUI_FRAMEWORK})
endif ()

find_library(DATADETECTORSCORE_FRAMEWORK DataDetectorsCore HINTS /System/Library/PrivateFrameworks)
if (NOT DATADETECTORSCORE_FRAMEWORK-NOTFOUND)
    list(APPEND WebCore_LIBRARIES ${DATADETECTORSCORE_FRAMEWORK})
endif ()

find_library(LOOKUP_FRAMEWORK Lookup HINTS /System/Library/PrivateFrameworks)
if (NOT LOOKUP_FRAMEWORK-NOTFOUND)
    list(APPEND WebCore_LIBRARIES ${LOOKUP_FRAMEWORK})
endif ()

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/Modules/webauthn/apdu"
    "${WEBCORE_DIR}/Modules/webgpu/WHLSL/Metal"
    "${WEBCORE_DIR}/accessibility/isolatedtree/mac"
    "${WEBCORE_DIR}/accessibility/mac"
    "${WEBCORE_DIR}/bridge/objc"
    "${WEBCORE_DIR}/crypto/mac"
    "${WEBCORE_DIR}/editing/cocoa"
    "${WEBCORE_DIR}/editing/ios"
    "${WEBCORE_DIR}/editing/mac"
    "${WEBCORE_DIR}/html/shadow/cocoa"
    "${WEBCORE_DIR}/layout/tableformatting"
    "${WEBCORE_DIR}/loader/archive/cf"
    "${WEBCORE_DIR}/loader/cf"
    "${WEBCORE_DIR}/loader/mac"
    "${WEBCORE_DIR}/page/cocoa"
    "${WEBCORE_DIR}/page/mac"
    "${WEBCORE_DIR}/page/scrolling/cocoa"
    "${WEBCORE_DIR}/page/scrolling/mac"
    "${WEBCORE_DIR}/platform/audio/cocoa"
    "${WEBCORE_DIR}/platform/audio/mac"
    "${WEBCORE_DIR}/platform/cf"
    "${WEBCORE_DIR}/platform/cocoa"
    "${WEBCORE_DIR}/platform/encryptedmedia/clearkey"
    "${WEBCORE_DIR}/platform/graphics/avfoundation"
    "${WEBCORE_DIR}/platform/graphics/avfoundation/cf"
    "${WEBCORE_DIR}/platform/graphics/avfoundation/objc"
    "${WEBCORE_DIR}/platform/graphics/ca"
    "${WEBCORE_DIR}/platform/graphics/ca/cocoa"
    "${WEBCORE_DIR}/platform/graphics/cocoa"
    "${WEBCORE_DIR}/platform/graphics/cg"
    "${WEBCORE_DIR}/platform/graphics/cv"
    "${WEBCORE_DIR}/platform/graphics/gpu"
    "${WEBCORE_DIR}/platform/graphics/gpu/legacy"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/mac"
    "${WEBCORE_DIR}/platform/mac"
    "${WEBCORE_DIR}/platform/mediacapabilities"
    "${WEBCORE_DIR}/platform/mediarecorder/cocoa"
    "${WEBCORE_DIR}/platform/mediastream/mac"
    "${WEBCORE_DIR}/platform/network/cocoa"
    "${WEBCORE_DIR}/platform/network/cf"
    "${WEBCORE_DIR}/platform/network/ios"
    "${WEBCORE_DIR}/platform/network/mac"
    "${WEBCORE_DIR}/platform/text/cf"
    "${WEBCORE_DIR}/platform/text/mac"
    "${WEBCORE_DIR}/platform/spi/cf"
    "${WEBCORE_DIR}/platform/spi/cg"
    "${WEBCORE_DIR}/platform/spi/cocoa"
    "${WEBCORE_DIR}/platform/spi/ios"
    "${WEBCORE_DIR}/platform/spi/mac"
    "${WEBCORE_DIR}/plugins/mac"
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/html/shadow/mac/imageControlsMac.css
    ${WEBCORE_DIR}/Modules/plugins/QuickTimePluginReplacement.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WEBCORE_DIR}/Modules/plugins/QuickTimePluginReplacement.js
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    "${CMAKE_OSX_SYSROOT}/usr/include/libxslt"
    "${CMAKE_OSX_SYSROOT}/usr/include/libxml2"
)

list(APPEND WebCore_SOURCES
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

    html/HTMLSlotElement.cpp

    html/shadow/mac/ImageControlsButtonElementMac.cpp
    html/shadow/mac/ImageControlsRootElementMac.cpp

    page/PageDebuggable.cpp

    page/mac/EventHandlerMac.mm
    page/mac/ServicesOverlayController.mm
    page/mac/TextIndicatorWindow.mm
    page/mac/WheelEventDeltaFilterMac.mm

    page/scrolling/ScrollingTreeScrollingNodeDelegate.cpp

    page/scrolling/mac/ScrollingCoordinatorMac.mm
    page/scrolling/mac/ScrollingMomentumCalculatorMac.mm
    page/scrolling/mac/ScrollingTreeFrameScrollingNodeMac.mm
    page/scrolling/mac/ScrollingTreeMac.mm

    platform/CPUMonitor.cpp
    platform/LocalizedStrings.cpp
    platform/ScrollableArea.cpp

    platform/audio/AudioSession.cpp

    platform/audio/cocoa/WebAudioBufferList.cpp

    platform/audio/mac/CAAudioStreamDescription.cpp

    platform/audio/mac/AudioBusMac.mm
    platform/audio/mac/AudioDestinationMac.cpp
    platform/audio/mac/AudioFileReaderMac.cpp
    platform/audio/mac/AudioHardwareListenerMac.cpp
    platform/audio/mac/CARingBuffer.cpp
    platform/audio/mac/FFTFrameMac.cpp

    platform/cf/KeyedDecoderCF.cpp
    platform/cf/KeyedEncoderCF.cpp
    platform/cf/MainThreadSharedTimerCF.cpp
    platform/cf/MediaAccessibilitySoftLink.cpp
    platform/cf/RunLoopObserver.cpp
    platform/cf/SharedBufferCF.cpp

    platform/cocoa/ContentFilterUnblockHandlerCocoa.mm
    platform/cocoa/CoreVideoSoftLink.cpp
    platform/cocoa/FileMonitorCocoa.mm
    platform/cocoa/KeyEventCocoa.mm
    platform/cocoa/LocalizedStringsCocoa.mm
    platform/cocoa/MIMETypeRegistryCocoa.mm
    platform/cocoa/NetworkExtensionContentFilter.mm
    platform/cocoa/ParentalControlsContentFilter.mm
    platform/cocoa/PasteboardCocoa.mm
    platform/cocoa/RuntimeApplicationChecksCocoa.mm
    platform/cocoa/ScrollController.mm
    platform/cocoa/ScrollSnapAnimatorState.mm
    platform/cocoa/SearchPopupMenuCocoa.mm
    platform/cocoa/SharedBufferCocoa.mm
    platform/cocoa/SystemBattery.mm
    platform/cocoa/SystemVersion.mm
    platform/cocoa/TelephoneNumberDetectorCocoa.cpp
    platform/cocoa/ThemeCocoa.mm
    platform/cocoa/VideoToolboxSoftLink.cpp
    platform/cocoa/WebCoreNSErrorExtras.mm

    platform/encryptedmedia/clearkey/CDMClearKey.cpp

    platform/gamepad/mac/HIDGamepad.cpp
    platform/gamepad/mac/HIDGamepadProvider.cpp

    platform/graphics/DisplayRefreshMonitor.cpp
    platform/graphics/DisplayRefreshMonitorManager.cpp
    platform/graphics/FourCC.cpp

    platform/graphics/avfoundation/AVTrackPrivateAVFObjCImpl.mm
    platform/graphics/avfoundation/AudioSourceProviderAVFObjC.mm
    platform/graphics/avfoundation/CDMFairPlayStreaming.cpp
    platform/graphics/avfoundation/CDMPrivateMediaSourceAVFObjC.mm
    platform/graphics/avfoundation/InbandMetadataTextTrackPrivateAVF.cpp
    platform/graphics/avfoundation/InbandTextTrackPrivateAVF.cpp
    platform/graphics/avfoundation/MediaPlaybackTargetCocoa.mm
    platform/graphics/avfoundation/MediaPlayerPrivateAVFoundation.cpp
    platform/graphics/avfoundation/MediaSelectionGroupAVFObjC.mm

    platform/graphics/avfoundation/objc/AVAssetTrackUtilities.mm
    platform/graphics/avfoundation/objc/AudioTrackPrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/AudioTrackPrivateMediaSourceAVFObjC.cpp
    platform/graphics/avfoundation/objc/CDMInstanceFairPlayStreamingAVFObjC.mm
    platform/graphics/avfoundation/objc/CDMSessionAVContentKeySession.mm
    platform/graphics/avfoundation/objc/CDMSessionAVFoundationObjC.mm
    platform/graphics/avfoundation/objc/CDMSessionAVStreamSession.mm
    platform/graphics/avfoundation/objc/CDMSessionMediaSourceAVFObjC.mm
    platform/graphics/avfoundation/objc/ImageDecoderAVFObjC.mm
    platform/graphics/avfoundation/objc/InbandTextTrackPrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/MediaPlayerPrivateAVFoundationObjC.mm
    platform/graphics/avfoundation/objc/MediaPlayerPrivateMediaSourceAVFObjC.mm
    platform/graphics/avfoundation/objc/MediaSampleAVFObjC.mm
    platform/graphics/avfoundation/objc/MediaSourcePrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/SourceBufferPrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/VideoTrackPrivateAVFObjC.cpp
    platform/graphics/avfoundation/objc/VideoTrackPrivateMediaSourceAVFObjC.mm
    platform/graphics/avfoundation/objc/WebCoreAVFResourceLoader.mm

    platform/graphics/ca/GraphicsLayerCA.cpp
    platform/graphics/ca/LayerPool.cpp
    platform/graphics/ca/PlatformCAAnimation.cpp
    platform/graphics/ca/PlatformCALayer.cpp
    platform/graphics/ca/TileController.cpp
    platform/graphics/ca/TileCoverageMap.cpp
    platform/graphics/ca/TileGrid.cpp
    platform/graphics/ca/TransformationMatrixCA.cpp

    platform/graphics/ca/cocoa/PlatformCAAnimationCocoa.mm
    platform/graphics/ca/cocoa/PlatformCAFiltersCocoa.mm
    platform/graphics/ca/cocoa/PlatformCALayerCocoa.mm
    platform/graphics/ca/cocoa/WebSystemBackdropLayer.mm
    platform/graphics/ca/cocoa/WebTiledBackingLayer.mm

    platform/graphics/cg/ColorCG.cpp
    platform/graphics/cg/FloatPointCG.cpp
    platform/graphics/cg/FloatRectCG.cpp
    platform/graphics/cg/FloatSizeCG.cpp
    platform/graphics/cg/GradientCG.cpp
    platform/graphics/cg/GraphicsContextGLCG.cpp
    platform/graphics/cg/GraphicsContextCG.cpp
    platform/graphics/cg/IOSurfacePool.cpp
    platform/graphics/cg/ImageBufferCGBackend.cpp
    platform/graphics/cg/ImageBufferCGBitmapBackend.cpp
    platform/graphics/cg/ImageBufferIOSurfaceBackend.cpp
    platform/graphics/cg/ImageBufferUtilitiesCG.cpp
    platform/graphics/cg/ImageDecoderCG.cpp
    platform/graphics/cg/ImageSourceCGMac.mm
    platform/graphics/cg/IntPointCG.cpp
    platform/graphics/cg/IntRectCG.cpp
    platform/graphics/cg/IntSizeCG.cpp
    platform/graphics/cg/NativeImageCG.cpp
    platform/graphics/cg/PDFDocumentImage.cpp
    platform/graphics/cg/PathCG.cpp
    platform/graphics/cg/PatternCG.cpp
    platform/graphics/cg/SubimageCacheWithTimer.cpp
    platform/graphics/cg/TransformationMatrixCG.cpp
    platform/graphics/cg/UTIRegistry.cpp

    platform/graphics/cocoa/FloatRectCocoa.mm
    platform/graphics/cocoa/FontCacheCoreText.cpp
    platform/graphics/cocoa/FontCascadeCocoa.mm
    platform/graphics/cocoa/FontCocoa.mm
    platform/graphics/cocoa/FontDescriptionCocoa.cpp
    platform/graphics/cocoa/FontFamilySpecificationCoreText.cpp
    platform/graphics/cocoa/FontPlatformDataCocoa.mm
    platform/graphics/cocoa/GraphicsContextCocoa.mm
    platform/graphics/cocoa/GraphicsContextGLOpenGLCocoa.mm
    platform/graphics/cocoa/IOSurface.mm
    platform/graphics/cocoa/IOSurfacePoolCocoa.mm
    platform/graphics/cocoa/IntRectCocoa.mm
    platform/graphics/cocoa/WebActionDisablingCALayerDelegate.mm
    platform/graphics/cocoa/WebCoreCALayerExtras.mm
    platform/graphics/cocoa/WebCoreDecompressionSession.mm
    platform/graphics/cocoa/WebGLLayer.mm
    platform/graphics/cocoa/WebGPULayer.mm

    platform/graphics/cv/ImageRotationSessionVT.mm
    platform/graphics/cv/PixelBufferConformerCV.cpp
    platform/graphics/cv/TextureCacheCV.mm
    platform/graphics/cv/VideoTextureCopierCV.cpp

    platform/graphics/gpu/Texture.cpp
    platform/graphics/gpu/TilingData.cpp

    platform/graphics/mac/ColorMac.mm
    platform/graphics/mac/ComplexTextControllerCoreText.mm
    platform/graphics/mac/DisplayRefreshMonitorMac.cpp
    platform/graphics/mac/FloatPointMac.mm
    platform/graphics/mac/FloatSizeMac.mm
    platform/graphics/mac/FontCustomPlatformData.cpp
    platform/graphics/mac/GlyphPageMac.cpp
    platform/graphics/mac/IconMac.mm
    platform/graphics/mac/ImageMac.mm
    platform/graphics/mac/IntPointMac.mm
    platform/graphics/mac/IntSizeMac.mm
    platform/graphics/mac/PDFDocumentImageMac.mm
    platform/graphics/mac/SimpleFontDataCoreText.cpp
    platform/graphics/mac/WebLayer.mm

    platform/graphics/opengl/ExtensionsGLOpenGL.cpp
    platform/graphics/opengl/ExtensionsGLOpenGLCommon.cpp
    platform/graphics/opengl/GraphicsContextGLOpenGLBase.cpp
    platform/graphics/opengl/GraphicsContextGLOpenGLCommon.cpp
    platform/graphics/opengl/TemporaryOpenGLSetting.cpp

    platform/graphics/opentype/OpenTypeCG.cpp
    platform/graphics/opentype/OpenTypeMathData.cpp

    platform/mac/CursorMac.mm
    platform/mac/KeyEventMac.mm
    platform/mac/LocalCurrentGraphicsContextMac.mm
    platform/mac/LoggingMac.mm
    platform/mac/MediaRemoteSoftLink.cpp
    platform/mac/NSScrollerImpDetails.mm
    platform/mac/PasteboardMac.mm
    platform/mac/PasteboardWriter.mm
    platform/mac/PlatformEventFactoryMac.mm
    platform/mac/PlatformPasteboardMac.mm
    platform/mac/PlatformScreenMac.mm
    platform/mac/PlatformSpeechSynthesizerMac.mm
    platform/mac/PowerObserverMac.cpp
    platform/mac/PublicSuffixMac.mm
    platform/mac/RemoteCommandListenerMac.mm
    platform/mac/SSLKeyGeneratorMac.mm
    platform/mac/ScrollAnimatorMac.mm
    platform/mac/ScrollViewMac.mm
    platform/mac/ScrollbarThemeMac.mm
    platform/mac/SerializedPlatformDataCueMac.mm
    platform/mac/StringUtilities.mm
    platform/mac/SuddenTermination.mm
    platform/mac/ThemeMac.mm
    platform/mac/ThreadCheck.mm
    platform/mac/UserActivityMac.mm
    platform/mac/ValidationBubbleMac.mm
    platform/mac/WebCoreFullScreenPlaceholderView.mm
    platform/mac/WebCoreFullScreenWarningView.mm
    platform/mac/WebCoreFullScreenWindow.mm
    platform/mac/WebCoreNSURLExtras.mm
    platform/mac/WebCoreObjCExtras.mm
    platform/mac/WebNSAttributedStringExtras.mm
    platform/mac/WidgetMac.mm

    platform/mediastream/mac/MockRealtimeVideoSourceMac.mm

    platform/network/cf/CertificateInfoCFNet.cpp
    platform/network/cf/DNSResolveQueueCFNet.cpp
    platform/network/cf/FormDataStreamCFNet.cpp
    platform/network/cf/NetworkStorageSessionCFNet.cpp
    platform/network/cf/ProxyServerCFNet.cpp
    platform/network/cf/ResourceRequestCFNet.cpp
    platform/network/cf/SocketStreamHandleImplCFNet.cpp

    platform/network/cocoa/CookieCocoa.mm
    platform/network/cocoa/CookieStorageObserver.mm
    platform/network/cocoa/CredentialCocoa.mm
    platform/network/cocoa/NetworkLoadMetrics.mm
    platform/network/cocoa/NetworkStorageSessionCocoa.mm
    platform/network/cocoa/ProtectionSpaceCocoa.mm
    platform/network/cocoa/ResourceRequestCocoa.mm
    platform/network/cocoa/ResourceResponseCocoa.mm
    platform/network/cocoa/WebCoreNSURLSession.mm

    platform/network/mac/AuthenticationMac.mm
    platform/network/mac/BlobDataFileReferenceMac.mm
    platform/network/mac/CookieStorageMac.mm
    platform/network/mac/CredentialStorageMac.mm
    platform/network/mac/FormDataStreamMac.mm
    platform/network/mac/NetworkStateNotifierMac.cpp
    platform/network/mac/ResourceErrorMac.mm
    platform/network/mac/ResourceHandleMac.mm
    platform/network/mac/SynchronousLoaderClient.mm
    platform/network/mac/UTIUtilities.mm
    platform/network/mac/WebCoreResourceHandleAsOperationQueueDelegate.mm
    platform/network/mac/WebCoreURLResponse.mm

    platform/text/cocoa/LocaleCocoa.mm

    platform/text/cf/HyphenationCF.cpp

    platform/text/mac/TextBoundaries.mm
    platform/text/mac/TextCheckingMac.mm
    platform/text/mac/TextEncodingRegistryMac.mm

    rendering/RenderThemeCocoa.mm
    rendering/RenderThemeMac.mm
    rendering/TextAutoSizing.cpp

    xml/SoftLinkLibxslt.cpp
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    Modules/applepay/PaymentMethodUpdate.h
    Modules/applepay/PaymentSessionError.h
    Modules/applepay/PaymentSummaryItems.h

    accessibility/mac/WebAccessibilityObjectWrapperBase.h
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

    editing/cocoa/AlternativeTextUIController.h
    editing/cocoa/AutofillElements.h
    editing/cocoa/DataDetection.h
    editing/cocoa/HTMLConverter.h

    editing/mac/DictionaryLookup.h
    editing/mac/TextAlternativeWithRange.h
    editing/mac/TextUndoInsertionMarkupMac.h

    loader/archive/cf/LegacyWebArchive.h

    loader/mac/LoaderNSURLExtras.h

    Modules/webauthn/AuthenticatorAssertionResponse.h
    Modules/webauthn/AuthenticatorResponse.h
    Modules/webauthn/AuthenticatorAttestationResponse.h

    Modules/webauthn/fido/Pin.h

    page/mac/TextIndicatorWindow.h
    page/mac/WebCoreFrameView.h

    page/scrolling/ScrollingStateOverflowScrollProxyNode.h

    page/scrolling/cocoa/ScrollingTreeFixedNode.h
    page/scrolling/cocoa/ScrollingTreeOverflowScrollProxyNode.h
    page/scrolling/cocoa/ScrollingTreePositionedNode.h
    page/scrolling/cocoa/ScrollingTreeStickyNode.h

    page/scrolling/mac/ScrollingTreeFrameScrollingNodeMac.h
    page/scrolling/mac/ScrollingTreeOverflowScrollingNodeMac.h
    page/scrolling/mac/ScrollingTreeScrollingNodeDelegateMac.h

    platform/PictureInPictureSupport.h
    platform/PlatformContentFilter.h

    platform/audio/cocoa/MediaSessionManagerCocoa.h
    platform/audio/cocoa/WebAudioBufferList.h

    platform/audio/mac/CAAudioStreamDescription.h
    platform/audio/mac/CARingBuffer.h

    platform/cf/RunLoopObserver.h

    platform/cocoa/LocalCurrentGraphicsContext.h
    platform/cocoa/NetworkExtensionContentFilter.h
    platform/cocoa/PlatformView.h
    platform/cocoa/PlaybackSessionModel.h
    platform/cocoa/PlaybackSessionModelMediaElement.h
    platform/cocoa/ScrollController.h
    platform/cocoa/ScrollSnapAnimatorState.h
    platform/cocoa/SearchPopupMenuCocoa.h
    platform/cocoa/SystemBattery.h
    platform/cocoa/SystemVersion.h
    platform/cocoa/VideoFullscreenChangeObserver.h
    platform/cocoa/VideoFullscreenModel.h
    platform/cocoa/VideoFullscreenModelVideoElement.h
    platform/cocoa/WebKitAvailability.h

    platform/gamepad/cocoa/GameControllerGamepadProvider.h

    platform/gamepad/mac/HIDGamepad.h
    platform/gamepad/mac/HIDGamepadProvider.h

    platform/graphics/MIMETypeCache.h

    platform/graphics/avfoundation/MediaPlaybackTargetCocoa.h
    platform/graphics/avfoundation/WebMediaSessionManagerMac.h

    platform/graphics/avfoundation/objc/AVAssetMIMETypeCache.h

    platform/graphics/ca/GraphicsLayerCA.h
    platform/graphics/ca/LayerPool.h
    platform/graphics/ca/PlatformCAAnimation.h
    platform/graphics/ca/PlatformCAFilters.h
    platform/graphics/ca/PlatformCALayer.h
    platform/graphics/ca/PlatformCALayerClient.h
    platform/graphics/ca/TileController.h

    platform/graphics/ca/cocoa/PlatformCAAnimationCocoa.h
    platform/graphics/ca/cocoa/PlatformCALayerCocoa.h

    platform/graphics/cg/GraphicsContextCG.h
    platform/graphics/cg/IOSurfacePool.h
    platform/graphics/cg/ImageBufferCGBackend.h
    platform/graphics/cg/ImageBufferCGBitmapBackend.h
    platform/graphics/cg/ImageBufferIOSurfaceBackend.h
    platform/graphics/cg/ImageBufferUtilitiesCG.h
    platform/graphics/cg/PDFDocumentImage.h
    platform/graphics/cg/UTIRegistry.h

    platform/graphics/cocoa/FontCacheCoreText.h
    platform/graphics/cocoa/FontFamilySpecificationCoreText.h
    platform/graphics/cocoa/IOSurface.h
    platform/graphics/cocoa/WebActionDisablingCALayerDelegate.h
    platform/graphics/cocoa/WebCoreCALayerExtras.h

    platform/graphics/mac/ColorMac.h
    platform/graphics/mac/SwitchingGPUClient.h
    platform/graphics/mac/WebLayer.h

    platform/mac/DynamicLinkerInterposing.h
    platform/mac/LegacyNSPasteboardTypes.h
    platform/mac/LocalDefaultSystemAppearance.h
    platform/mac/NSScrollerImpDetails.h
    platform/mac/PasteboardWriter.h
    platform/mac/PlatformEventFactoryMac.h
    platform/mac/PlaybackSessionInterfaceMac.h
    platform/mac/ScrollbarThemeMac.h
    platform/mac/StringUtilities.h
    platform/mac/VideoFullscreenInterfaceMac.h
    platform/mac/WebCoreFullScreenPlaceholderView.h
    platform/mac/WebCoreFullScreenWindow.h
    platform/mac/WebCoreNSFontManagerExtras.h
    platform/mac/WebCoreNSURLExtras.h
    platform/mac/WebCoreObjCExtras.h
    platform/mac/WebCoreView.h
    platform/mac/WebNSAttributedStringExtras.h
    platform/mac/WebPlaybackControlsManager.h

    platform/mediastream/RealtimeMediaSourceIdentifier.h

    platform/mediastream/libwebrtc/LibWebRTCProviderCocoa.h

    platform/mediastream/mac/WebAudioSourceProviderAVFObjC.h

    platform/network/cf/AuthenticationCF.h
    platform/network/cf/AuthenticationChallenge.h
    platform/network/cf/CertificateInfo.h
    platform/network/cf/DownloadBundle.h
    platform/network/cf/LoaderRunLoopCF.h
    platform/network/cf/ProtectionSpaceCFNet.h
    platform/network/cf/ResourceError.h
    platform/network/cf/ResourceRequest.h
    platform/network/cf/ResourceRequestCFNet.h
    platform/network/cf/ResourceResponse.h
    platform/network/cf/SocketStreamHandleImpl.h

    platform/network/cocoa/CookieStorageObserver.h
    platform/network/cocoa/CredentialCocoa.h
    platform/network/cocoa/HTTPCookieAcceptPolicyCocoa.h
    platform/network/cocoa/ProtectionSpaceCocoa.h
    platform/network/cocoa/WebCoreNSURLSession.h

    platform/network/mac/AuthenticationMac.h
    platform/network/mac/FormDataStreamMac.h
    platform/network/mac/UTIUtilities.h
    platform/network/mac/WebCoreURLResponse.h

    testing/MockWebAuthenticationConfiguration.h
)

list(APPEND WebCore_IDL_FILES
    Modules/applepay/ApplePayCancelEvent.idl
    Modules/applepay/ApplePayContactField.idl
    Modules/applepay/ApplePayError.idl
    Modules/applepay/ApplePayErrorCode.idl
    Modules/applepay/ApplePayErrorContactField.idl
    Modules/applepay/ApplePayInstallmentItem.idl
    Modules/applepay/ApplePayInstallmentItemType.idl
    Modules/applepay/ApplePayInstallmentConfiguration.idl
    Modules/applepay/ApplePayInstallmentRetailChannel.idl
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
    Modules/applepay/ApplePayPaymentPass.idl
    Modules/applepay/ApplePayPaymentRequest.idl
    Modules/applepay/ApplePayRequestBase.idl
    Modules/applepay/ApplePaySession.idl
    Modules/applepay/ApplePaySessionError.idl
    Modules/applepay/ApplePaySetup.idl
    Modules/applepay/ApplePaySetupConfiguration.idl
    Modules/applepay/ApplePaySetupFeature.idl
    Modules/applepay/ApplePaySetupFeatureState.idl
    Modules/applepay/ApplePaySetupFeatureType.idl
    Modules/applepay/ApplePayShippingContactSelectedEvent.idl
    Modules/applepay/ApplePayShippingContactUpdate.idl
    Modules/applepay/ApplePayShippingMethod.idl
    Modules/applepay/ApplePayShippingMethodSelectedEvent.idl
    Modules/applepay/ApplePayShippingMethodUpdate.idl
    Modules/applepay/ApplePayValidateMerchantEvent.idl

    Modules/applepay/paymentrequest/ApplePayModifier.idl
    Modules/applepay/paymentrequest/ApplePayRequest.idl

    Modules/plugins/QuickTimePluginReplacement.idl
)

set(FEATURE_DEFINES_OBJECTIVE_C "LANGUAGE_OBJECTIVE_C=1 ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}")
set(ADDITIONAL_BINDINGS_DEPENDENCIES
    ${WINDOW_CONSTRUCTORS_FILE}
    ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
    ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
)
set(CSS_VALUE_PLATFORM_DEFINES "WTF_PLATFORM_MAC=1 HAVE_OS_DARK_MODE_SUPPORT=1")

add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/WHLSLStandardLibraryFunctionMap.cpp
    MAIN_DEPENDENCY Modules/webgpu/WHLSL/WHLSLStandardLibrary.txt
    DEPENDS Modules/webgpu/WHLSL/WHLSLBuildStandardLibraryFunctionMap.py
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/Modules/webgpu/WHLSL/WHLSLBuildStandardLibraryFunctionMap.py ${WEBCORE_DIR}/Modules/webgpu/WHLSL/WHLSLStandardLibrary.txt ${WebCore_DERIVED_SOURCES_DIR}/WHLSLStandardLibraryFunctionMap.cpp
    VERBATIM)
add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/WHLSLStandardLibrary.h
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${WEBCORE_DIR}/Modules/webgpu/WHLSL/WHLSLStandardLibrary.txt
    COMMAND gzip -cn ${WEBCORE_DIR}/Modules/webgpu/WHLSL/WHLSLStandardLibrary.txt > ${WebCore_DERIVED_SOURCES_DIR}/WHLSLStandardLibrary.gz
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl WHLSLStandardLibrary ${WebCore_DERIVED_SOURCES_DIR}/WHLSLStandardLibrary.gz ${WebCore_DERIVED_SOURCES_DIR}/WHLSLStandardLibrary.h
    VERBATIM)
list(APPEND WebCore_SOURCES
    ${WebCore_DERIVED_SOURCES_DIR}/WHLSLStandardLibrary.h
    ${WebCore_DERIVED_SOURCES_DIR}/WHLSLStandardLibraryFunctionMap.cpp
)

list(APPEND WebCoreTestSupport_LIBRARIES PRIVATE WebCore)
list(APPEND WebCoreTestSupport_SOURCES
    testing/Internals.mm
    testing/MockContentFilter.cpp
    testing/MockContentFilterSettings.cpp
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
    testing/MockWebAuthenticationConfiguration.idl
)

set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} "-compatibility_version 1 -current_version ${WEBKIT_MAC_VERSION}")
