find_library(QUARTZ_FRAMEWORK Quartz)
add_definitions(-iframework ${QUARTZ_FRAMEWORK}/Frameworks)

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
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/mac"
    "${WEBCORE_DIR}/platform/mac"
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

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentScripts.h ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentScripts.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/Modules/plugins/QuickTimePluginReplacement.js
    DEPENDS Scripts/make-js-file-arrays.py
    COMMAND PYTHONPATH=${WebCore_INSPECTOR_SCRIPTS_DIR} ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/Scripts/make-js-file-arrays.py ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentScripts.h ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentScripts.cpp ${WEBCORE_DIR}/Modules/plugins/QuickTimePluginReplacement.js
    VERBATIM)

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
    ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentScripts.cpp
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    "/usr/include/libxslt"
    "/usr/include/libxml2"
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
    crypto/mac/CryptoDigestMac.cpp
    crypto/mac/CryptoKeyMac.cpp
    crypto/mac/CryptoKeyRSAMac.cpp
    crypto/mac/SerializedCryptoKeyWrapMac.mm

    dom/DataTransferMac.mm

    editing/SelectionRectGatherer.cpp
    editing/SmartReplaceCF.cpp

    editing/cocoa/EditorCocoa.mm
    editing/cocoa/HTMLConverter.mm

    editing/mac/AlternativeTextUIController.mm
    editing/mac/DataDetection.mm
    editing/mac/DictionaryLookup.mm
    editing/mac/EditorMac.mm
    editing/mac/FrameSelectionMac.mm
    editing/mac/TextAlternativeWithRange.mm
    editing/mac/TextUndoInsertionMarkupMac.mm

    fileapi/FileMac.mm

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

    page/mac/ChromeMac.mm
    page/mac/DragControllerMac.mm
    page/mac/EventHandlerMac.mm
    page/mac/PageMac.cpp
    page/mac/ServicesOverlayController.mm
    page/mac/SettingsMac.mm
    page/mac/TextIndicatorWindow.mm
    page/mac/UserAgentMac.mm

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
    platform/cf/MediaAccessibilitySoftLink.cpp
    platform/cf/RunLoopObserver.cpp
    platform/cf/SharedBufferCF.cpp
    platform/cf/SharedTimerCF.cpp
    platform/cf/URLCF.cpp

    platform/cocoa/ContentFilterUnblockHandlerCocoa.mm
    platform/cocoa/DisplaySleepDisablerCocoa.cpp
    platform/cocoa/KeyEventCocoa.mm
    platform/cocoa/LocalizedStringsCocoa.mm
    platform/cocoa/MachSendRight.cpp
    platform/cocoa/MemoryPressureHandlerCocoa.mm
    platform/cocoa/NetworkExtensionContentFilter.mm
    platform/cocoa/ParentalControlsContentFilter.mm
    platform/cocoa/ScrollController.mm
    platform/cocoa/SystemVersion.mm
    platform/cocoa/TelephoneNumberDetectorCocoa.cpp
    platform/cocoa/ThemeCocoa.cpp
    platform/cocoa/VNodeTrackerCocoa.cpp

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
    platform/graphics/avfoundation/objc/CDMSessionAVFoundationObjC.mm
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
    platform/graphics/cg/ImageSourceCGWin.cpp
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

    platform/graphics/opentype/OpenTypeMathData.cpp

    platform/mac/BlockExceptions.mm
    platform/mac/ContextMenuItemMac.mm
    platform/mac/ContextMenuMac.mm
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
    platform/mac/MIMETypeRegistryMac.mm
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
    platform/network/mac/ResourceResponseMac.mm
    platform/network/mac/SynchronousLoaderClient.mm
    platform/network/mac/UTIUtilities.mm
    platform/network/mac/WebCoreResourceHandleAsDelegate.mm
    platform/network/mac/WebCoreResourceHandleAsOperationQueueDelegate.mm
    platform/network/mac/WebCoreURLResponse.mm

    platform/posix/FileSystemPOSIX.cpp
    platform/posix/SharedBufferPOSIX.cpp

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

    platform/animation
    platform/audio
    platform/graphics
    platform/mac
    platform/mediastream
    platform/mock
    platform/network
    platform/sql
    platform/text

    platform/graphics/ca
    platform/graphics/cg
    platform/graphics/filters
    platform/graphics/mac
    platform/graphics/transforms

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

set(OBJC_BINDINGS_IDL_FILES
    dom/EventListener.idl
    ${WebCore_NON_SVG_IDL_FILES}
)

WEBKIT_CREATE_FORWARDING_HEADERS(WebCore DIRECTORIES ${WebCore_FORWARDING_HEADERS_DIRECTORIES} FILES ${WebCore_FORWARDING_HEADERS_FILES})

set(FEATURE_DEFINES_OBJECTIVE_C "LANGUAGE_OBJECTIVE_C=1 ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}")
set(ADDITIONAL_BINDINGS_DEPENDENCIES
    ${WINDOW_CONSTRUCTORS_FILE}
    ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
    ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
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
    "${OBJC_BINDINGS_IDL_FILES}"
    "${WEBCORE_DIR}"
    "${IDL_INCLUDES}"
    "${FEATURE_DEFINES_OBJECTIVE_C}"
    ${DERIVED_SOURCES_WEBCORE_DIR} DOM ObjC mm
    ${IDL_ATTRIBUTES_FILE}
    ${SUPPLEMENTAL_DEPENDENCY_FILE}
    ${ADDITIONAL_BINDINGS_DEPENDENCIES})

list(APPEND WebCore_SOURCES
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMAttr.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMBeforeLoadEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCDATASection.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCharacterData.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMComment.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCounter.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSCharsetRule.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSFontFaceRule.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSImportRule.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSKeyframeRule.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSKeyframesRule.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSMediaRule.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSPageRule.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSPrimitiveValue.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSRule.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSRuleList.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSStyleDeclaration.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSStyleRule.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSStyleSheet.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSSupportsRule.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSUnknownRule.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSValue.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCSSValueList.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDocument.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDocumentFragment.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDocumentType.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMImplementation.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMNamedFlowCollection.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMTokenList.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMEntity.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMEntityReference.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMFile.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMFileList.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLAnchorElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLAppletElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLAreaElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLBaseElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLBaseFontElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLBodyElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLBRElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLButtonElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLCanvasElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLCollection.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLDirectoryElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLDivElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLDListElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLDocument.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLEmbedElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLFieldSetElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLFontElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLFormElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLFrameElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLFrameSetElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLHeadElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLHeadingElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLHRElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLHtmlElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLIFrameElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLImageElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLInputElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLLabelElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLLegendElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLLIElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLLinkElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLMapElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLMarqueeElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLMenuElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLMetaElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLModElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLObjectElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLOListElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLOptGroupElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLOptionElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLOptionsCollection.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLParagraphElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLParamElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLPreElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLQuoteElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLScriptElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLSelectElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLStyleElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLTableCaptionElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLTableCellElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLTableColElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLTableElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLTableRowElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLTableSectionElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLTextAreaElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLTitleElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLUListElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMKeyboardEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMMediaList.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMMessageEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMMessagePort.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMMouseEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMMutationEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMNamedNodeMap.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMNodeIterator.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMNodeList.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMOverflowEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMProcessingInstruction.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMProgressEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMRange.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMRect.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMRGBColor.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMStyleSheet.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMStyleSheetList.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMText.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMTextEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMTreeWalker.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMUIEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMValidityState.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWebKitCSSFilterValue.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWebKitCSSRegionRule.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWebKitCSSTransformValue.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWebKitNamedFlow.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWheelEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMXPathExpression.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMXPathResult.mm
)
