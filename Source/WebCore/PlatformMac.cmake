list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/accessibility/mac"
    "${WEBCORE_DIR}/bindings/objc"
    "${WEBCORE_DIR}/bridge/objc"
    "${WEBCORE_DIR}/editing/cocoa"
    "${WEBCORE_DIR}/editing/mac"
    "${WEBCORE_DIR}/icu"
    "${WEBCORE_DIR}/loader/archive/cf"
    "${WEBCORE_DIR}/loader/cf"
    "${WEBCORE_DIR}/loader/mac"
    "${WEBCORE_DIR}/page/cocoa"
    "${WEBCORE_DIR}/page/mac"
    "${WEBCORE_DIR}/platform/audio/mac"
    "${WEBCORE_DIR}/platform/cf"
    "${WEBCORE_DIR}/platform/cocoa"
    "${WEBCORE_DIR}/platform/graphics/avfoundation"
    "${WEBCORE_DIR}/platform/graphics/avfoundation/cf"
    "${WEBCORE_DIR}/platform/graphics/avfoundation/objc"
    "${WEBCORE_DIR}/platform/graphics/ca"
    "${WEBCORE_DIR}/platform/graphics/ca/mac"
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
    "${WEBCORE_DIR}/platform/spi/mac"
    "${WEBCORE_DIR}/plugins/mac"
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    "/usr/include/libxslt"
    "/usr/include/libxml2"
)

list(APPEND WebCore_SOURCES
    accessibility/mac/AXObjectCacheMac.mm
    accessibility/mac/AccessibilityObjectMac.mm
    accessibility/mac/WebAccessibilityObjectWrapperBase.mm
    accessibility/mac/WebAccessibilityObjectWrapperMac.mm

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

    editing/cocoa/HTMLConverter.mm

    editing/mac/AlternativeTextUIController.mm
    editing/mac/DataDetection.mm
    editing/mac/DictionaryLookup.mm
    editing/mac/EditorMac.mm
    editing/mac/FrameSelectionMac.mm
    editing/mac/TextAlternativeWithRange.mm
    editing/mac/TextUndoInsertionMarkupMac.mm

    history/mac/HistoryItemMac.mm

    loader/archive/cf/LegacyWebArchive.cpp
    loader/archive/cf/LegacyWebArchiveMac.mm

    loader/cf/ResourceLoaderCFNet.cpp
    loader/cf/SubresourceLoaderCF.cpp

    page/cocoa/UserAgent.mm

    page/mac/ChromeMac.mm
    page/mac/DragControllerMac.mm
    page/mac/EventHandlerMac.mm
    page/mac/PageMac.cpp
    page/mac/SettingsMac.mm
    page/mac/UserAgentMac.mm

    platform/LocalizedStrings.cpp

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
    platform/cocoa/MemoryPressureHandlerCocoa.mm
    platform/cocoa/SystemVersion.mm
    platform/cocoa/TelephoneNumberDetectorCocoa.cpp

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

    platform/graphics/ca/mac/LayerFlushSchedulerMac.cpp
    platform/graphics/ca/mac/PlatformCAAnimationMac.mm
    platform/graphics/ca/mac/PlatformCAFiltersMac.mm
    platform/graphics/ca/mac/PlatformCALayerMac.mm
    platform/graphics/ca/mac/WebTiledBackingLayer.mm

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
    platform/mac/PlatformSpeechSynthesisMac.mm
    platform/mac/PlatformSpeechSynthesizerMac.mm
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

    loader/archive/cf

    page/animation
    page/cocoa
    page/mac
    page/scrolling

    platform/animation
    platform/audio
    platform/graphics
    platform/mac
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
GENERATE_BINDINGS(WebCore_SOURCES
    "${OBJC_BINDINGS_IDL_FILES}"
    "${WEBCORE_DIR}"
    "${IDL_INCLUDES}"
    "${FEATURE_DEFINES_OBJECTIVE_C}"
    ${DERIVED_SOURCES_WEBCORE_DIR} DOM ObjC mm
    ${IDL_ATTRIBUTES_FILE}
    ${SUPPLEMENTAL_DEPENDENCY_FILE}
    ${ADDITIONAL_BINDINGS_DEPENDENCIES})

list(REMOVE_ITEM WebCore_SOURCES
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMAbstractView.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMAbstractWorker.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMAnalyserNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMAudioBuffer.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMAudioBufferSourceNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMAudioContext.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMAudioDestinationNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMAudioNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMAudioParam.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMBiquadFilterNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCanvasRenderingContext2D.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMChannelMergerNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMChannelSplitterNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMChildNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCommandLineAPIHost.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMConvolverNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCoordinates.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCrypto.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCryptoKey.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMCustomEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDataCue.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDataTransfer.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDedicatedWorkerGlobalScope.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDelayNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMCoreException.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMFormData.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMPath.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMSettableTokenList.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMStringMap.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMURL.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMURLMediaSource.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMURLMediaStream.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMWindow.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMWindowIndexedDatabase.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMWindowNotifications.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMWindowSpeechSynthesis.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDOMWindowWebDatabase.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMDynamicsCompressorNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMEventListener.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMEventTarget.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMFileException.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMFileReader.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMFileReaderSync.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMGainNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMGeolocation.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHistory.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLFormControlsCollection.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLMediaElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLMediaElementMediaSession.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLMediaElementMediaStream.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLTrackElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMHTMLVideoElement.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBAny.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBCursor.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBCursor.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBCursorWithValue.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBCursorWithValue.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBDatabase.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBDatabase.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBFactory.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBFactory.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBIndex.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBIndex.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBKeyRange.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBKeyRange.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBObjectStore.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBObjectStore.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBOpenDBRequest.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBOpenDBRequest.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBRequest.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBRequest.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBTransaction.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBTransaction.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBVersionChangeEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMIDBVersionChangeEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMInspectorFrontendHost.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMLocation.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMMediaControlsHost.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMMediaElementAudioSourceNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMMediaSource.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMMutationObserver.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMNavigator.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMNavigatorBattery.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMNavigatorContentUtils.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMNavigatorGamepad.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMNavigatorGeolocation.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMNavigatorMediaDevices.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMNavigatorUserMedia.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMNavigatorVibration.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMNodeFilter.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMNotification.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMOfflineAudioContext.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMOscillatorNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMPannerNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMPopStateEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMRadioNodeList.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMReadableStream.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMReadableStreamReader.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMScriptProcessorNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMScriptProfile.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMScriptProfileNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMSourceBuffer.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMSourceBufferList.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMSQLError.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMSQLException.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMSQLResultSetRowList.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMSQLStatementErrorCallback.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMSQLTransaction.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMSQLTransactionErrorCallback.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMSubtleCrypto.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMTrackEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMUIRequestEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMURLUtils.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMVTTCue.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWaveShaperNode.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWebGL2RenderingContext.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWebGLRenderingContext.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWebGLRenderingContextBase.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWebSocket.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWindowBase64.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWindowIndexedDatabase.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWindowTimers.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWorker.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWorkerGlobalScope.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWorkerGlobalScopeIndexedDatabase.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWorkerGlobalScopeIndexedDatabase.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWorkerGlobalScopeNotifications.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMXMLHttpRequest.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMXMLHttpRequestProgressEvent.mm
    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMXPathNSResolver.mm
)
