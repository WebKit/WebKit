set(WebCore_LIBRARY_TYPE SHARED)

if ("${CURRENT_OSX_VERSION}" MATCHES "10.9")
set(WEBKITSYSTEMINTERFACE_LIBRARY libWebKitSystemInterfaceMavericks.a)
elif ("${CURRENT_OSX_VERSION}" MATCHES "10.10")
set(WEBKITSYSTEMINTERFACE_LIBRARY libWebKitSystemInterfaceYosemite.a)
else ()
set(WEBKITSYSTEMINTERFACE_LIBRARY libWebKitSystemInterfaceElCapitan.a)
endif ()
link_directories(../../WebKitLibraries)

find_library(ACCELERATE_LIBRARY accelerate)
find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
find_library(AUDIOUNIT_LIBRARY AudioUnit)
find_library(CARBON_LIBRARY Carbon)
find_library(COCOA_LIBRARY Cocoa)
find_library(COREAUDIO_LIBRARY CoreAudio)
find_library(DISKARBITRATION_LIBRARY DiskArbitration)
find_library(IOKIT_LIBRARY IOKit)
find_library(IOSURFACE_LIBRARY IOSurface)
find_library(OPENGL_LIBRARY OpenGL)
find_library(QUARTZ_LIBRARY Quartz)
find_library(QUARTZCORE_LIBRARY QuartzCore)
find_library(SECURITY_LIBRARY Security)
find_library(SYSTEMCONFIGURATION_LIBRARY SystemConfiguration)
find_library(SQLITE3_LIBRARY sqlite3)
find_library(XML2_LIBRARY XML2)
find_package(ZLIB REQUIRED)

list(APPEND WebCore_LIBRARIES
    ${ACCELERATE_LIBRARY}
    ${AUDIOTOOLBOX_LIBRARY}
    ${AUDIOUNIT_LIBRARY}
    ${CARBON_LIBRARY}
    ${COCOA_LIBRARY}
    ${COREAUDIO_LIBRARY}
    ${DISKARBITRATION_LIBRARY}
    ${IOKIT_LIBRARY}
    ${IOSURFACE_LIBRARY}
    ${OPENGL_LIBRARY}
    ${QUARTZ_LIBRARY}
    ${QUARTZCORE_LIBRARY}
    ${SECURITY_LIBRARY}
    ${SQLITE3_LIBRARY}
    ${SYSTEMCONFIGURATION_LIBRARY}
    ${WEBKITSYSTEMINTERFACE_LIBRARY}
    ${XML2_LIBRARY}
    ${ZLIB_LIBRARIES}
)

add_definitions(-iframework ${QUARTZ_LIBRARY}/Frameworks)

find_library(DATADETECTORSCORE_FRAMEWORK DataDetectorsCore HINTS /System/Library/PrivateFrameworks)
if (NOT DATADETECTORSCORE_FRAMEWORK-NOTFOUND)
    list(APPEND WebCore_LIBRARIES ${DATADETECTORSCORE_FRAMEWORK})
endif ()

find_library(LOOKUP_FRAMEWORK Lookup HINTS /System/Library/PrivateFrameworks)
if (NOT LOOKUP_FRAMEWORK-NOTFOUND)
    list(APPEND WebCore_LIBRARIES ${LOOKUP_FRAMEWORK})
endif ()

list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore"
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}"
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector"
    "${JAVASCRIPTCORE_DIR}/replay"
    "${THIRDPARTY_DIR}/ANGLE"
    "${THIRDPARTY_DIR}/ANGLE/include/KHR"
    "${WEBCORE_DIR}/accessibility/mac"
    "${WEBCORE_DIR}/bindings/objc"
    "${WEBCORE_DIR}/bridge/objc"
    "${WEBCORE_DIR}/editing/cocoa"
    "${WEBCORE_DIR}/editing/mac"
    "${WEBCORE_DIR}/ForwardingHeaders"
    "${WEBCORE_DIR}/ForwardingHeaders/bindings"
    "${WEBCORE_DIR}/ForwardingHeaders/bytecode"
    "${WEBCORE_DIR}/ForwardingHeaders/debugger"
    "${WEBCORE_DIR}/ForwardingHeaders/heap"
    "${WEBCORE_DIR}/ForwardingHeaders/inspector"
    "${WEBCORE_DIR}/ForwardingHeaders/interpreter"
    "${WEBCORE_DIR}/ForwardingHeaders/jit"
    "${WEBCORE_DIR}/ForwardingHeaders/masm"
    "${WEBCORE_DIR}/ForwardingHeaders/parser"
    "${WEBCORE_DIR}/ForwardingHeaders/profiler"
    "${WEBCORE_DIR}/ForwardingHeaders/replay"
    "${WEBCORE_DIR}/ForwardingHeaders/runtime"
    "${WEBCORE_DIR}/ForwardingHeaders/yarr"
    "${WEBCORE_DIR}/icu"
    "${WEBCORE_DIR}/loader/archive/cf"
    "${WEBCORE_DIR}/loader/cf"
    "${WEBCORE_DIR}/loader/mac"
    "${WEBCORE_DIR}/page/cocoa"
    "${WEBCORE_DIR}/page/mac"
    "${WEBCORE_DIR}/page/scrolling/mac"
    "${WEBCORE_DIR}/platform/audio/mac"
    "${WEBCORE_DIR}/platform/cf"
    "${WEBCORE_DIR}/platform/cocoa"
    "${WEBCORE_DIR}/platform/graphics/avfoundation"
    "${WEBCORE_DIR}/platform/graphics/avfoundation/cf"
    "${WEBCORE_DIR}/platform/graphics/avfoundation/objc"
    "${WEBCORE_DIR}/platform/graphics/ca"
    "${WEBCORE_DIR}/platform/graphics/ca/cocoa"
    "${WEBCORE_DIR}/platform/graphics/cocoa"
    "${WEBCORE_DIR}/platform/graphics/cg"
    "${WEBCORE_DIR}/platform/graphics/cv"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/mac"
    "${WEBCORE_DIR}/platform/mac"
    "${WEBCORE_DIR}/platform/mediastream/mac"
    "${WEBCORE_DIR}/platform/network/cocoa"
    "${WEBCORE_DIR}/platform/network/cf"
    "${WEBCORE_DIR}/platform/network/mac"
    "${WEBCORE_DIR}/platform/text/cf"
    "${WEBCORE_DIR}/platform/text/mac"
    "${WEBCORE_DIR}/platform/spi/cf"
    "${WEBCORE_DIR}/platform/spi/cg"
    "${WEBCORE_DIR}/platform/spi/cocoa"
    "${WEBCORE_DIR}/platform/spi/ios"
    "${WEBCORE_DIR}/platform/spi/mac"
    "${WEBCORE_DIR}/plugins/mac"
    "${WTF_DIR}"
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/html/shadow/mac/imageControlsMac.css
    ${WEBCORE_DIR}/Modules/plugins/QuickTimePluginReplacement.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WEBCORE_DIR}/Modules/plugins/QuickTimePluginReplacement.js
)

#FIXME: Use ios-encodings.txt once we get CMake working for iOS.
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/CharsetData.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/platform/text/mac/make-charset-table.pl
    DEPENDS platform/text/mac/character-sets.txt
    DEPENDS platform/text/mac/mac-encodings.txt
    COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/platform/text/mac/make-charset-table.pl ${WEBCORE_DIR}/platform/text/mac/character-sets.txt ${WEBCORE_DIR}/platform/text/mac/mac-encodings.txt kTextEncoding > ${DERIVED_SOURCES_WEBCORE_DIR}/CharsetData.cpp
    VERBATIM)

list(APPEND WebCore_SOURCES
    ${DERIVED_SOURCES_WEBCORE_DIR}/CharsetData.cpp
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    "${CMAKE_OSX_SYSROOT}/usr/include/libxslt"
    "${CMAKE_OSX_SYSROOT}/usr/include/libxml2"
)

list(APPEND WebCore_SOURCES
    Modules/indieui/UIRequestEvent.cpp

    Modules/plugins/QuickTimePluginReplacement.mm
    Modules/plugins/YouTubePluginReplacement.cpp

    accessibility/mac/AXObjectCacheMac.mm
    accessibility/mac/AccessibilityObjectMac.mm
    accessibility/mac/WebAccessibilityObjectWrapperBase.mm
    accessibility/mac/WebAccessibilityObjectWrapperMac.mm

    bindings/js/ScriptControllerMac.mm

    bindings/objc/DOM.mm
    bindings/objc/DOMAbstractView.mm
    bindings/objc/DOMCSS.mm
    bindings/objc/DOMCustomXPathNSResolver.mm
    bindings/objc/DOMEvents.mm
    bindings/objc/DOMHTML.mm
    bindings/objc/DOMInternal.mm
    bindings/objc/DOMObject.mm
    bindings/objc/DOMUIKitExtensions.mm
    bindings/objc/DOMUtility.mm
    bindings/objc/DOMXPath.mm
    bindings/objc/ExceptionHandlers.mm
    bindings/objc/ObjCEventListener.mm
    bindings/objc/ObjCNodeFilterCondition.mm
    bindings/objc/WebScriptObject.mm

    bridge/objc/ObjCRuntimeObject.mm
    bridge/objc/objc_class.mm
    bridge/objc/objc_instance.mm
    bridge/objc/objc_runtime.mm
    bridge/objc/objc_utility.mm

    crypto/CommonCryptoUtilities.cpp
    crypto/CryptoAlgorithm.cpp
    crypto/CryptoAlgorithmDescriptionBuilder.cpp
    crypto/CryptoAlgorithmRegistry.cpp
    crypto/CryptoKey.cpp
    crypto/CryptoKeyPair.cpp
    crypto/SubtleCrypto.cpp

    crypto/algorithms/CryptoAlgorithmAES_CBC.cpp
    crypto/algorithms/CryptoAlgorithmAES_KW.cpp
    crypto/algorithms/CryptoAlgorithmHMAC.cpp
    crypto/algorithms/CryptoAlgorithmRSAES_PKCS1_v1_5.cpp
    crypto/algorithms/CryptoAlgorithmRSASSA_PKCS1_v1_5.cpp
    crypto/algorithms/CryptoAlgorithmRSA_OAEP.cpp
    crypto/algorithms/CryptoAlgorithmSHA1.cpp
    crypto/algorithms/CryptoAlgorithmSHA224.cpp
    crypto/algorithms/CryptoAlgorithmSHA256.cpp
    crypto/algorithms/CryptoAlgorithmSHA384.cpp
    crypto/algorithms/CryptoAlgorithmSHA512.cpp

    crypto/keys/CryptoKeyAES.cpp
    crypto/keys/CryptoKeyDataOctetSequence.cpp
    crypto/keys/CryptoKeyDataRSAComponents.cpp
    crypto/keys/CryptoKeyHMAC.cpp
    crypto/keys/CryptoKeySerializationRaw.cpp

    crypto/mac/CryptoAlgorithmAES_CBCMac.cpp
    crypto/mac/CryptoAlgorithmAES_KWMac.cpp
    crypto/mac/CryptoAlgorithmHMACMac.cpp
    crypto/mac/CryptoAlgorithmRSAES_PKCS1_v1_5Mac.cpp
    crypto/mac/CryptoAlgorithmRSASSA_PKCS1_v1_5Mac.cpp
    crypto/mac/CryptoAlgorithmRSA_OAEPMac.cpp
    crypto/mac/CryptoAlgorithmRegistryMac.cpp
    crypto/mac/CryptoKeyMac.cpp
    crypto/mac/CryptoKeyRSAMac.cpp
    crypto/mac/SerializedCryptoKeyWrapMac.mm

    dom/DataTransferMac.mm
    dom/SlotAssignment.cpp

    editing/SelectionRectGatherer.cpp
    editing/SmartReplaceCF.cpp

    editing/cocoa/DataDetection.mm
    editing/cocoa/EditorCocoa.mm
    editing/cocoa/HTMLConverter.mm

    editing/mac/AlternativeTextUIController.mm
    editing/mac/DictionaryLookup.mm
    editing/mac/EditorMac.mm
    editing/mac/FrameSelectionMac.mm
    editing/mac/TextAlternativeWithRange.mm
    editing/mac/TextUndoInsertionMarkupMac.mm

    fileapi/FileMac.mm

    html/HTMLSlotElement.cpp

    html/shadow/ImageControlsRootElement.cpp
    html/shadow/YouTubeEmbedShadowElement.cpp

    html/shadow/mac/ImageControlsButtonElementMac.cpp
    html/shadow/mac/ImageControlsRootElementMac.cpp

    history/mac/HistoryItemMac.mm

    loader/ResourceLoadInfo.cpp

    loader/archive/cf/LegacyWebArchive.cpp
    loader/archive/cf/LegacyWebArchiveMac.mm

    loader/cocoa/DiskCacheMonitorCocoa.mm
    loader/cocoa/SubresourceLoaderCocoa.mm

    loader/cf/ResourceLoaderCFNet.cpp
    loader/cf/SubresourceLoaderCF.cpp

    loader/mac/DocumentLoaderMac.cpp
    loader/mac/LoaderNSURLExtras.mm
    loader/mac/ResourceLoaderMac.mm

    page/CaptionUserPreferencesMediaAF.cpp
    page/PageDebuggable.cpp

    page/cocoa/UserAgent.mm
    page/cocoa/ResourceUsageOverlayCocoa.mm
    page/cocoa/ResourceUsageThreadCocoa.mm
    page/cocoa/SettingsCocoa.mm

    page/mac/ChromeMac.mm
    page/mac/DragControllerMac.mm
    page/mac/EventHandlerMac.mm
    page/mac/PageMac.mm
    page/mac/ServicesOverlayController.mm
    page/mac/TextIndicatorWindow.mm
    page/mac/UserAgentMac.mm
    page/mac/WheelEventDeltaFilterMac.mm

    page/scrolling/AsyncScrollingCoordinator.cpp

    page/scrolling/cocoa/ScrollingStateNode.mm

    page/scrolling/mac/ScrollingCoordinatorMac.mm
    page/scrolling/mac/ScrollingStateFrameScrollingNodeMac.mm
    page/scrolling/mac/ScrollingThreadMac.mm
    page/scrolling/mac/ScrollingTreeFixedNode.mm
    page/scrolling/mac/ScrollingTreeFrameScrollingNodeMac.mm
    page/scrolling/mac/ScrollingTreeMac.cpp
    page/scrolling/mac/ScrollingTreeStickyNode.mm

    platform/LocalizedStrings.cpp
    platform/ScrollableArea.cpp
    platform/VNodeTracker.cpp

    platform/audio/AudioSession.cpp

    platform/audio/mac/AudioBusMac.mm
    platform/audio/mac/AudioDestinationMac.cpp
    platform/audio/mac/AudioFileReaderMac.cpp
    platform/audio/mac/AudioHardwareListenerMac.cpp
    platform/audio/mac/AudioSessionMac.cpp
    platform/audio/mac/CARingBuffer.cpp
    platform/audio/mac/FFTFrameMac.cpp
    platform/audio/mac/MediaSessionManagerMac.cpp

    platform/cf/CFURLExtras.cpp
    platform/cf/CoreMediaSoftLink.cpp
    platform/cf/FileSystemCF.cpp
    platform/cf/KeyedDecoderCF.cpp
    platform/cf/KeyedEncoderCF.cpp
    platform/cf/MainThreadSharedTimerCF.cpp
    platform/cf/MediaAccessibilitySoftLink.cpp
    platform/cf/RunLoopObserver.cpp
    platform/cf/SharedBufferCF.cpp
    platform/cf/URLCF.cpp

    platform/cocoa/ContentFilterUnblockHandlerCocoa.mm
    platform/cocoa/CoreVideoSoftLink.cpp
    platform/cocoa/DisplaySleepDisablerCocoa.cpp
    platform/cocoa/KeyEventCocoa.mm
    platform/cocoa/LocalizedStringsCocoa.mm
    platform/cocoa/MIMETypeRegistryCocoa.mm
    platform/cocoa/MachSendRight.cpp
    platform/cocoa/MemoryPressureHandlerCocoa.mm
    platform/cocoa/NetworkExtensionContentFilter.mm
    platform/cocoa/ParentalControlsContentFilter.mm
    platform/cocoa/ScrollController.mm
    platform/cocoa/ScrollSnapAnimatorState.mm
    platform/cocoa/SearchPopupMenuCocoa.mm
    platform/cocoa/SystemVersion.mm
    platform/cocoa/TelephoneNumberDetectorCocoa.cpp
    platform/cocoa/ThemeCocoa.cpp
    platform/cocoa/VNodeTrackerCocoa.cpp
    platform/cocoa/WebCoreNSErrorExtras.mm

    platform/crypto/commoncrypto/CryptoDigestCommonCrypto.cpp

    platform/graphics/DisplayRefreshMonitor.cpp
    platform/graphics/DisplayRefreshMonitorManager.cpp
    platform/graphics/FontPlatformData.cpp

    platform/graphics/avfoundation/AVTrackPrivateAVFObjCImpl.mm
    platform/graphics/avfoundation/AudioSourceProviderAVFObjC.mm
    platform/graphics/avfoundation/CDMPrivateMediaSourceAVFObjC.mm
    platform/graphics/avfoundation/InbandMetadataTextTrackPrivateAVF.cpp
    platform/graphics/avfoundation/InbandTextTrackPrivateAVF.cpp
    platform/graphics/avfoundation/MediaPlaybackTargetMac.mm
    platform/graphics/avfoundation/MediaPlayerPrivateAVFoundation.cpp
    platform/graphics/avfoundation/MediaSelectionGroupAVFObjC.mm
    platform/graphics/avfoundation/MediaTimeAVFoundation.cpp

    platform/graphics/avfoundation/objc/AudioTrackPrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/AudioTrackPrivateMediaSourceAVFObjC.cpp
    platform/graphics/avfoundation/objc/CDMSessionAVContentKeySession.mm
    platform/graphics/avfoundation/objc/CDMSessionAVFoundationObjC.mm
    platform/graphics/avfoundation/objc/CDMSessionAVStreamSession.mm
    platform/graphics/avfoundation/objc/CDMSessionMediaSourceAVFObjC.mm
    platform/graphics/avfoundation/objc/InbandTextTrackPrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/InbandTextTrackPrivateLegacyAVFObjC.mm
    platform/graphics/avfoundation/objc/MediaPlayerPrivateAVFoundationObjC.mm
    platform/graphics/avfoundation/objc/MediaPlayerPrivateMediaSourceAVFObjC.mm
    platform/graphics/avfoundation/objc/MediaSourcePrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/SourceBufferPrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/VideoTrackPrivateAVFObjC.cpp
    platform/graphics/avfoundation/objc/VideoTrackPrivateMediaSourceAVFObjC.mm
    platform/graphics/avfoundation/objc/WebCoreAVFResourceLoader.mm

    platform/graphics/ca/GraphicsLayerCA.cpp
    platform/graphics/ca/LayerFlushScheduler.cpp
    platform/graphics/ca/LayerPool.cpp
    platform/graphics/ca/PlatformCAAnimation.cpp
    platform/graphics/ca/PlatformCALayer.cpp
    platform/graphics/ca/TileController.cpp
    platform/graphics/ca/TileCoverageMap.cpp
    platform/graphics/ca/TileGrid.cpp
    platform/graphics/ca/TransformationMatrixCA.cpp

    platform/graphics/ca/cocoa/LayerFlushSchedulerMac.cpp
    platform/graphics/ca/cocoa/PlatformCAAnimationCocoa.mm
    platform/graphics/ca/cocoa/PlatformCAFiltersCocoa.mm
    platform/graphics/ca/cocoa/PlatformCALayerCocoa.mm
    platform/graphics/ca/cocoa/WebSystemBackdropLayer.mm
    platform/graphics/ca/cocoa/WebTiledBackingLayer.mm

    platform/graphics/cg/BitmapImageCG.cpp
    platform/graphics/cg/ColorCG.cpp
    platform/graphics/cg/FloatPointCG.cpp
    platform/graphics/cg/FloatRectCG.cpp
    platform/graphics/cg/FloatSizeCG.cpp
    platform/graphics/cg/GradientCG.cpp
    platform/graphics/cg/GraphicsContext3DCG.cpp
    platform/graphics/cg/GraphicsContextCG.cpp
    platform/graphics/cg/IOSurfacePool.cpp
    platform/graphics/cg/ImageBufferCG.cpp
    platform/graphics/cg/ImageBufferDataCG.cpp
    platform/graphics/cg/ImageCG.cpp
    platform/graphics/cg/ImageSourceCG.cpp
    platform/graphics/cg/ImageSourceCGMac.mm
    platform/graphics/cg/IntPointCG.cpp
    platform/graphics/cg/IntRectCG.cpp
    platform/graphics/cg/IntSizeCG.cpp
    platform/graphics/cg/PDFDocumentImage.cpp
    platform/graphics/cg/PathCG.cpp
    platform/graphics/cg/PatternCG.cpp
    platform/graphics/cg/SubimageCacheWithTimer.cpp
    platform/graphics/cg/TransformationMatrixCG.cpp

    platform/graphics/cocoa/FontCacheCoreText.cpp
    platform/graphics/cocoa/FontCascadeCocoa.mm
    platform/graphics/cocoa/FontCocoa.mm
    platform/graphics/cocoa/FontPlatformDataCocoa.mm
    platform/graphics/cocoa/IOSurface.mm
    platform/graphics/cocoa/IOSurfacePoolCocoa.mm
    platform/graphics/cocoa/WebActionDisablingCALayerDelegate.mm
    platform/graphics/cocoa/WebCoreCALayerExtras.mm

    platform/graphics/cv/PixelBufferConformerCV.cpp
    platform/graphics/cv/TextureCacheCV.mm
    platform/graphics/cv/VideoTextureCopierCV.cpp

    platform/graphics/mac/ColorMac.mm
    platform/graphics/mac/ComplexTextController.cpp
    platform/graphics/mac/ComplexTextControllerCoreText.mm
    platform/graphics/mac/DisplayRefreshMonitorMac.cpp
    platform/graphics/mac/FloatPointMac.mm
    platform/graphics/mac/FloatRectMac.mm
    platform/graphics/mac/FloatSizeMac.mm
    platform/graphics/mac/FontCacheMac.mm
    platform/graphics/mac/FontCustomPlatformData.cpp
    platform/graphics/mac/GlyphPageMac.cpp
    platform/graphics/mac/GraphicsContext3DMac.mm
    platform/graphics/mac/GraphicsContextMac.mm
    platform/graphics/mac/IconMac.mm
    platform/graphics/mac/ImageMac.mm
    platform/graphics/mac/IntPointMac.mm
    platform/graphics/mac/IntRectMac.mm
    platform/graphics/mac/IntSizeMac.mm
    platform/graphics/mac/MediaPlayerPrivateQTKit.mm
    platform/graphics/mac/MediaTimeQTKit.mm
    platform/graphics/mac/PDFDocumentImageMac.mm
    platform/graphics/mac/SimpleFontDataCoreText.cpp
    platform/graphics/mac/WebGLLayer.mm
    platform/graphics/mac/WebLayer.mm

    platform/graphics/opengl/Extensions3DOpenGL.cpp
    platform/graphics/opengl/Extensions3DOpenGLCommon.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGL.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGLCommon.cpp
    platform/graphics/opengl/TemporaryOpenGLSetting.cpp

    platform/graphics/opentype/OpenTypeCG.cpp
    platform/graphics/opentype/OpenTypeMathData.cpp

    platform/mac/BlockExceptions.mm
    platform/mac/CursorMac.mm
    platform/mac/DragDataMac.mm
    platform/mac/DragImageMac.mm
    platform/mac/EventLoopMac.mm
    platform/mac/FileSystemMac.mm
    platform/mac/HIDGamepad.cpp
    platform/mac/HIDGamepadProvider.cpp
    platform/mac/KeyEventMac.mm
    platform/mac/KillRingMac.mm
    platform/mac/Language.mm
    platform/mac/LocalCurrentGraphicsContext.mm
    platform/mac/LoggingMac.mm
    platform/mac/NSScrollerImpDetails.mm
    platform/mac/PasteboardMac.mm
    platform/mac/PlatformClockCA.cpp
    platform/mac/PlatformClockCM.mm
    platform/mac/PlatformEventFactoryMac.mm
    platform/mac/PlatformPasteboardMac.mm
    platform/mac/PlatformScreenMac.mm
    platform/mac/PlatformSpeechSynthesizerMac.mm
    platform/mac/PowerObserverMac.cpp
    platform/mac/PublicSuffixMac.mm
    platform/mac/SSLKeyGeneratorMac.cpp
    platform/mac/ScrollAnimatorMac.mm
    platform/mac/ScrollViewMac.mm
    platform/mac/ScrollbarThemeMac.mm
    platform/mac/SerializedPlatformRepresentationMac.mm
    platform/mac/SharedBufferMac.mm
    platform/mac/SoundMac.mm
    platform/mac/SuddenTermination.mm
    platform/mac/SystemSleepListenerMac.mm
    platform/mac/ThemeMac.mm
    platform/mac/ThreadCheck.mm
    platform/mac/URLMac.mm
    platform/mac/UserActivityMac.mm
    platform/mac/WebCoreFullScreenPlaceholderView.mm
    platform/mac/WebCoreFullScreenWarningView.mm
    platform/mac/WebCoreFullScreenWindow.mm
    platform/mac/WebCoreNSStringExtras.mm
    platform/mac/WebCoreNSURLExtras.mm
    platform/mac/WebCoreObjCExtras.mm
    platform/mac/WebCoreSystemInterface.mm
    platform/mac/WebNSAttributedStringExtras.mm
    platform/mac/WebVideoFullscreenController.mm
    platform/mac/WebVideoFullscreenHUDWindowController.mm
    platform/mac/WebWindowAnimation.mm
    platform/mac/WidgetMac.mm

    platform/mediastream/mac/MockRealtimeVideoSourceMac.mm

    platform/network/cf/AuthenticationCF.cpp
    platform/network/cf/CookieJarCFNet.cpp
    platform/network/cf/CookieStorageCFNet.cpp
    platform/network/cf/CredentialStorageCFNet.cpp
    platform/network/cf/DNSCFNet.cpp
    platform/network/cf/FormDataStreamCFNet.cpp
    platform/network/cf/LoaderRunLoopCF.cpp
    platform/network/cf/NetworkStorageSessionCFNet.cpp
    platform/network/cf/ProxyServerCFNet.cpp
    platform/network/cf/ResourceErrorCF.cpp
    platform/network/cf/ResourceHandleCFNet.cpp
    platform/network/cf/ResourceHandleCFURLConnectionDelegate.cpp
    platform/network/cf/ResourceHandleCFURLConnectionDelegateWithOperationQueue.cpp
    platform/network/cf/ResourceRequestCFNet.cpp
    platform/network/cf/ResourceResponseCFNet.cpp
    platform/network/cf/SocketStreamHandleCFNet.cpp
    platform/network/cf/SynchronousLoaderClientCFNet.cpp
    platform/network/cf/SynchronousResourceHandleCFURLConnectionDelegate.cpp

    platform/network/cocoa/CredentialCocoa.mm
    platform/network/cocoa/ProtectionSpaceCocoa.mm
    platform/network/cocoa/ResourceRequestCocoa.mm
    platform/network/cocoa/ResourceResponseCocoa.mm
    platform/network/cocoa/WebCoreNSURLSession.mm

    platform/network/mac/AuthenticationMac.mm
    platform/network/mac/BlobDataFileReferenceMac.mm
    platform/network/mac/CertificateInfoMac.mm
    platform/network/mac/CookieJarMac.mm
    platform/network/mac/CookieStorageMac.mm
    platform/network/mac/CredentialStorageMac.mm
    platform/network/mac/FormDataStreamMac.mm
    platform/network/mac/NetworkStateNotifierMac.cpp
    platform/network/mac/ResourceErrorMac.mm
    platform/network/mac/ResourceHandleMac.mm
    platform/network/mac/ResourceRequestMac.mm
    platform/network/mac/SynchronousLoaderClient.mm
    platform/network/mac/UTIUtilities.mm
    platform/network/mac/WebCoreResourceHandleAsDelegate.mm
    platform/network/mac/WebCoreResourceHandleAsOperationQueueDelegate.mm
    platform/network/mac/WebCoreURLResponse.mm

    platform/posix/FileSystemPOSIX.cpp

    platform/text/cf/HyphenationCF.cpp

    platform/text/mac/LocaleMac.mm
    platform/text/mac/TextBoundaries.mm
    platform/text/mac/TextBreakIteratorInternalICUMac.mm
    platform/text/mac/TextCodecMac.cpp

    rendering/RenderThemeMac.mm
    rendering/TextAutoSizing.cpp
)

# FIXME: We do not need everything from all of these directories.
# Move some to WebCore_FORWARDING_HEADERS_FILES once people start actually maintaining this.
set(WebCore_FORWARDING_HEADERS_DIRECTORIES
    accessibility
    bridge
    contentextensions
    crypto
    css
    dom
    editing
    fileapi
    history
    html
    inspector
    loader
    page
    platform
    plugins
    rendering
    replay
    storage
    style
    svg

    Modules/geolocation
    Modules/indexeddb
    Modules/notifications
    Modules/webdatabase

    Modules/indexeddb/legacy
    Modules/indexeddb/shared
    Modules/indexeddb/server

    bindings/generic
    bindings/js
    bindings/objc

    bridge/jsc

    editing/cocoa
    editing/mac

    html/forms
    html/parser
    html/shadow

    loader/appcache
    loader/archive
    loader/cache
    loader/cocoa

    loader/archive/cf

    page/animation
    page/cocoa
    page/mac
    page/scrolling

    page/scrolling/mac

    platform/animation
    platform/audio
    platform/cf
    platform/cocoa
    platform/graphics
    platform/mac
    platform/mediastream
    platform/mock
    platform/network
    platform/sql
    platform/text

    platform/graphics/ca
    platform/graphics/cocoa
    platform/graphics/cg
    platform/graphics/filters
    platform/graphics/mac
    platform/graphics/transforms

    platform/graphics/ca/cocoa

    platform/network/cf
    platform/network/cocoa
    platform/network/mac

    platform/spi/cf
    platform/spi/cg
    platform/spi/cocoa
    platform/spi/mac

    rendering/line
    rendering/style

    svg/graphics
    svg/properties
)

set(WebCore_FORWARDING_HEADERS_FILES
    Modules/webdatabase/DatabaseDetails.h

    bridge/IdentifierRep.h
    bridge/npruntime_impl.h
    bridge/npruntime_internal.h

    contentextensions/CompiledContentExtension.h

    editing/EditAction.h
    editing/EditingBehaviorTypes.h
    editing/EditingBoundary.h
    editing/FindOptions.h
    editing/FrameSelection.h
    editing/TextAffinity.h

    editing/mac/TextAlternativeWithRange.h

    history/BackForwardList.h
    history/HistoryItem.h
    history/PageCache.h

    html/HTMLMediaElement.h

    loader/appcache/ApplicationCacheStorage.h

    loader/icon/IconDatabase.h
    loader/icon/IconDatabaseBase.h
    loader/icon/IconDatabaseClient.h

    loader/mac/LoaderNSURLExtras.h

    platform/DisplaySleepDisabler.h
    platform/PlatformExportMacros.h

    platform/audio/AudioHardwareListener.h

    platform/cf/RunLoopObserver.h

    platform/cocoa/MachSendRight.h

    platform/graphics/cocoa/IOSurface.h

    platform/graphics/transforms/AffineTransform.h

    platform/mac/SoftLinking.h
    platform/mac/WebCoreSystemInterface.h

    platform/network/cf/CertificateInfo.h
    platform/network/cf/ResourceResponse.h

    platform/network/mac/AuthenticationMac.h

    platform/sql/SQLiteDatabase.h

    rendering/style/RenderStyleConstants.h
)

list(APPEND WebCore_IDL_FILES
    Modules/plugins/QuickTimePluginReplacement.idl
)

WEBKIT_CREATE_FORWARDING_HEADERS(WebCore DIRECTORIES ${WebCore_FORWARDING_HEADERS_DIRECTORIES} FILES ${WebCore_FORWARDING_HEADERS_FILES})

set(FEATURE_DEFINES_OBJECTIVE_C "LANGUAGE_OBJECTIVE_C=1 ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}")
set(ADDITIONAL_BINDINGS_DEPENDENCIES
    ${WINDOW_CONSTRUCTORS_FILE}
    ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
    ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
)

set(ObjC_Bindings_IDL_FILES
    css/CSSCharsetRule.idl
    css/CSSFontFaceRule.idl
    css/CSSImportRule.idl
    css/CSSKeyframeRule.idl
    css/CSSKeyframesRule.idl
    css/CSSMediaRule.idl
    css/CSSPageRule.idl
    css/CSSPrimitiveValue.idl
    css/CSSRule.idl
    css/CSSRuleList.idl
    css/CSSStyleDeclaration.idl
    css/CSSStyleRule.idl
    css/CSSStyleSheet.idl
    css/CSSSupportsRule.idl
    css/CSSUnknownRule.idl
    css/CSSValue.idl
    css/CSSValueList.idl
    css/Counter.idl
    css/MediaList.idl
    css/RGBColor.idl
    css/Rect.idl
    css/StyleSheet.idl
    css/StyleSheetList.idl
    css/WebKitCSSFilterValue.idl
    css/WebKitCSSRegionRule.idl
    css/WebKitCSSTransformValue.idl

    dom/Attr.idl
    dom/BeforeLoadEvent.idl
    dom/CDATASection.idl
    dom/CharacterData.idl
    dom/Comment.idl
    dom/DOMImplementation.idl
    dom/DOMNamedFlowCollection.idl
    dom/Document.idl
    dom/DocumentFragment.idl
    dom/DocumentType.idl
    dom/Element.idl
    dom/Entity.idl
    dom/EntityReference.idl
    dom/Event.idl
    dom/EventListener.idl
    dom/EventTarget.idl
    dom/KeyboardEvent.idl
    dom/MessageEvent.idl
    dom/MessagePort.idl
    dom/MouseEvent.idl
    dom/MutationEvent.idl
    dom/NamedNodeMap.idl
    dom/Node.idl
    dom/NodeFilter.idl
    dom/NodeIterator.idl
    dom/NodeList.idl
    dom/OverflowEvent.idl
    dom/ProcessingInstruction.idl
    dom/ProgressEvent.idl
    dom/Range.idl
    dom/Text.idl
    dom/TextEvent.idl
    dom/TreeWalker.idl
    dom/UIEvent.idl
    dom/WebKitNamedFlow.idl
    dom/WheelEvent.idl

    fileapi/Blob.idl
    fileapi/File.idl
    fileapi/FileList.idl

    html/DOMTokenList.idl
    html/HTMLAnchorElement.idl
    html/HTMLAppletElement.idl
    html/HTMLAreaElement.idl
    html/HTMLBRElement.idl
    html/HTMLBaseElement.idl
    html/HTMLBaseFontElement.idl
    html/HTMLBodyElement.idl
    html/HTMLButtonElement.idl
    html/HTMLCanvasElement.idl
    html/HTMLCollection.idl
    html/HTMLDListElement.idl
    html/HTMLDirectoryElement.idl
    html/HTMLDivElement.idl
    html/HTMLDocument.idl
    html/HTMLElement.idl
    html/HTMLEmbedElement.idl
    html/HTMLFieldSetElement.idl
    html/HTMLFontElement.idl
    html/HTMLFormElement.idl
    html/HTMLFrameElement.idl
    html/HTMLFrameSetElement.idl
    html/HTMLHRElement.idl
    html/HTMLHeadElement.idl
    html/HTMLHeadingElement.idl
    html/HTMLHtmlElement.idl
    html/HTMLIFrameElement.idl
    html/HTMLImageElement.idl
    html/HTMLInputElement.idl
    html/HTMLLIElement.idl
    html/HTMLLabelElement.idl
    html/HTMLLegendElement.idl
    html/HTMLLinkElement.idl
    html/HTMLMapElement.idl
    html/HTMLMarqueeElement.idl
    html/HTMLMenuElement.idl
    html/HTMLMediaElement.idl
    html/HTMLMetaElement.idl
    html/HTMLModElement.idl
    html/HTMLOListElement.idl
    html/HTMLObjectElement.idl
    html/HTMLOptGroupElement.idl
    html/HTMLOptionElement.idl
    html/HTMLOptionsCollection.idl
    html/HTMLParagraphElement.idl
    html/HTMLParamElement.idl
    html/HTMLPreElement.idl
    html/HTMLQuoteElement.idl
    html/HTMLScriptElement.idl
    html/HTMLSelectElement.idl
    html/HTMLStyleElement.idl
    html/HTMLTableCaptionElement.idl
    html/HTMLTableCellElement.idl
    html/HTMLTableColElement.idl
    html/HTMLTableElement.idl
    html/HTMLTableRowElement.idl
    html/HTMLTableSectionElement.idl
    html/HTMLTextAreaElement.idl
    html/HTMLTitleElement.idl
    html/HTMLUListElement.idl
    html/HTMLVideoElement.idl
    html/ValidityState.idl

    page/AbstractView.idl

    xml/XPathExpression.idl
    xml/XPathNSResolver.idl
    xml/XPathResult.idl
)

set(ObjC_BINDINGS_NO_MM
    AbstractView
    AbstractWorker
    ChildNode
    DOMURLMediaSource
    DOMURLMediaStream
    DOMWindowIndexedDatabase
    DOMWindowNotifications
    DOMWindowSpeechSynthesis
    DOMWindowWebDatabase
    EventListener
    EventTarget
    GlobalEventHandlers
    HTMLMediaElementMediaSession
    HTMLMediaElementMediaStream
    NavigatorBattery
    NavigatorContentUtils
    NavigatorGamepad
    NavigatorGeolocation
    NavigatorMediaDevices
    NavigatorUserMedia
    NavigatorVibration
    NodeFilter
    NonDocumentTypeChildNode
    NonElementParentNode
    ParentNode
    URLUtils
    WindowBase64
    WindowEventHandlers
    WindowTimers
    WorkerGlobalScopeIndexedDatabase
    WorkerGlobalScopeNotifications
    XPathNSResolver
)

GENERATE_BINDINGS(WebCore_SOURCES
    "${ObjC_Bindings_IDL_FILES}"
    "${WEBCORE_DIR}"
    "${IDL_INCLUDES}"
    "${FEATURE_DEFINES_OBJECTIVE_C}"
    ${DERIVED_SOURCES_WEBCORE_DIR} DOM ObjC mm
    ${IDL_ATTRIBUTES_FILE}
    ${SUPPLEMENTAL_DEPENDENCY_FILE}
    ${ADDITIONAL_BINDINGS_DEPENDENCIES})
