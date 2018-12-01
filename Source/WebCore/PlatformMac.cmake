find_library(ACCELERATE_LIBRARY accelerate)
find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
find_library(AVFOUNDATION_LIBRARY AVFoundation)
find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
find_library(AUDIOUNIT_LIBRARY AudioUnit)
find_library(CARBON_LIBRARY Carbon)
find_library(CFNETWORK_LIBRARY CFNetwork)
find_library(COCOA_LIBRARY Cocoa)
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
find_package(Sqlite REQUIRED)
find_package(ZLIB REQUIRED)

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesCocoa.txt"
    "SourcesMac.txt"
)

list(APPEND WebCore_LIBRARIES
    ${ACCELERATE_LIBRARY}
    ${AUDIOTOOLBOX_LIBRARY}
    ${AUDIOUNIT_LIBRARY}
    ${AVFOUNDATION_LIBRARY}
    ${CARBON_LIBRARY}
    ${CFNETWORK_LIBRARY}
    ${COCOA_LIBRARY}
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
    ${SQLITE_LIBRARIES}
    ${SYSTEMCONFIGURATION_LIBRARY}
    ${XML2_LIBRARY}
    ${ZLIB_LIBRARY}
)

add_definitions(-iframework ${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks)
add_definitions(-iframework ${AVFOUNDATION_LIBRARY}/Versions/Current/Frameworks)
add_definitions(-iframework ${CARBON_LIBRARY}/Versions/Current/Frameworks)
add_definitions(-iframework ${CORESERVICES_LIBRARY}/Versions/Current/Frameworks)
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
    "${THIRDPARTY_DIR}/ANGLE"
    "${THIRDPARTY_DIR}/ANGLE/include/KHR"
    "${WEBCORE_DIR}/accessibility/mac"
    "${WEBCORE_DIR}/bridge/objc"
    "${WEBCORE_DIR}/editing/cocoa"
    "${WEBCORE_DIR}/editing/ios"
    "${WEBCORE_DIR}/editing/mac"
    "${WEBCORE_DIR}/html/shadow/cocoa"
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
    "${WEBCORE_DIR}/platform/graphics/gpu"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/mac"
    "${WEBCORE_DIR}/platform/mac"
    "${WEBCORE_DIR}/platform/mediacapabilities"
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

    accessibility/mac/AXObjectCacheMac.mm
    accessibility/mac/AccessibilityObjectMac.mm
    accessibility/mac/WebAccessibilityObjectWrapperMac.mm

    dom/DataTransferMac.mm
    dom/SlotAssignment.cpp

    editing/ios/AutofillElements.cpp

    editing/mac/AlternativeTextUIController.mm
    editing/mac/DictionaryLookup.mm
    editing/mac/EditorMac.mm
    editing/mac/TextAlternativeWithRange.mm
    editing/mac/TextUndoInsertionMarkupMac.mm
    editing/mac/WebContentReaderMac.mm

    html/HTMLSlotElement.cpp

    html/shadow/mac/ImageControlsButtonElementMac.cpp
    html/shadow/mac/ImageControlsRootElementMac.cpp

    page/PageDebuggable.cpp

    page/mac/EventHandlerMac.mm
    page/mac/ServicesOverlayController.mm
    page/mac/TextIndicatorWindow.mm
    page/mac/UserAgentMac.mm
    page/mac/WheelEventDeltaFilterMac.mm

    page/scrolling/mac/ScrollingCoordinatorMac.mm
    page/scrolling/mac/ScrollingMomentumCalculatorMac.mm
    page/scrolling/mac/ScrollingStateFrameScrollingNodeMac.mm
    page/scrolling/mac/ScrollingTreeFrameScrollingNodeMac.mm
    page/scrolling/mac/ScrollingTreeMac.cpp

    platform/CPUMonitor.cpp
    platform/LocalizedStrings.cpp
    platform/ScrollableArea.cpp

    platform/audio/AudioSession.cpp

    platform/audio/cocoa/MediaSessionManagerCocoa.cpp
    platform/audio/cocoa/WebAudioBufferList.cpp

    platform/audio/mac/CAAudioStreamDescription.cpp

    platform/audio/mac/AudioBusMac.mm
    platform/audio/mac/AudioDestinationMac.cpp
    platform/audio/mac/AudioFileReaderMac.cpp
    platform/audio/mac/AudioHardwareListenerMac.cpp
    platform/audio/mac/AudioSessionMac.cpp
    platform/audio/mac/CARingBuffer.cpp
    platform/audio/mac/FFTFrameMac.cpp
    platform/audio/mac/MediaSessionManagerMac.mm

    platform/cf/FileSystemCF.cpp
    platform/cf/KeyedDecoderCF.cpp
    platform/cf/KeyedEncoderCF.cpp
    platform/cf/MainThreadSharedTimerCF.cpp
    platform/cf/MediaAccessibilitySoftLink.cpp
    platform/cf/RunLoopObserver.cpp
    platform/cf/SharedBufferCF.cpp

    platform/cocoa/ContentFilterUnblockHandlerCocoa.mm
    platform/cocoa/CoreVideoSoftLink.cpp
    platform/cocoa/FileMonitorCocoa.mm
    platform/cocoa/FileSystemCocoa.mm
    platform/cocoa/KeyEventCocoa.mm
    platform/cocoa/LocalizedStringsCocoa.mm
    platform/cocoa/MIMETypeRegistryCocoa.mm
    platform/cocoa/MachSendRight.cpp
    platform/cocoa/NetworkExtensionContentFilter.mm
    platform/cocoa/ParentalControlsContentFilter.mm
    platform/cocoa/PasteboardCocoa.mm
    platform/cocoa/RuntimeApplicationChecksCocoa.mm
    platform/cocoa/ScrollController.mm
    platform/cocoa/ScrollSnapAnimatorState.mm
    platform/cocoa/SearchPopupMenuCocoa.mm
    platform/cocoa/SharedBufferCocoa.mm
    platform/cocoa/SystemVersion.mm
    platform/cocoa/TelephoneNumberDetectorCocoa.cpp
    platform/cocoa/ThemeCocoa.mm
    platform/cocoa/VideoToolboxSoftLink.cpp
    platform/cocoa/WebCoreNSErrorExtras.mm

    platform/gamepad/mac/HIDGamepad.cpp
    platform/gamepad/mac/HIDGamepadProvider.cpp

    platform/graphics/DisplayRefreshMonitor.cpp
    platform/graphics/DisplayRefreshMonitorManager.cpp
    platform/graphics/FourCC.cpp

    platform/graphics/avfoundation/AVTrackPrivateAVFObjCImpl.mm
    platform/graphics/avfoundation/AudioSourceProviderAVFObjC.mm
    platform/graphics/avfoundation/CDMPrivateMediaSourceAVFObjC.mm
    platform/graphics/avfoundation/InbandMetadataTextTrackPrivateAVF.cpp
    platform/graphics/avfoundation/InbandTextTrackPrivateAVF.cpp
    platform/graphics/avfoundation/MediaPlaybackTargetMac.mm
    platform/graphics/avfoundation/MediaPlayerPrivateAVFoundation.cpp
    platform/graphics/avfoundation/MediaSelectionGroupAVFObjC.mm

    platform/graphics/avfoundation/objc/AVAssetTrackUtilities.mm
    platform/graphics/avfoundation/objc/AVFoundationMIMETypeCache.mm
    platform/graphics/avfoundation/objc/AudioTrackPrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/AudioTrackPrivateMediaSourceAVFObjC.cpp
    platform/graphics/avfoundation/objc/CDMSessionAVContentKeySession.mm
    platform/graphics/avfoundation/objc/CDMSessionAVFoundationObjC.mm
    platform/graphics/avfoundation/objc/CDMSessionAVStreamSession.mm
    platform/graphics/avfoundation/objc/CDMSessionMediaSourceAVFObjC.mm
    platform/graphics/avfoundation/objc/ImageDecoderAVFObjC.mm
    platform/graphics/avfoundation/objc/InbandTextTrackPrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/InbandTextTrackPrivateLegacyAVFObjC.mm
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
    platform/graphics/cg/GraphicsContext3DCG.cpp
    platform/graphics/cg/GraphicsContextCG.cpp
    platform/graphics/cg/IOSurfacePool.cpp
    platform/graphics/cg/ImageBufferCG.cpp
    platform/graphics/cg/ImageBufferDataCG.cpp
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

    platform/graphics/cocoa/GraphicsContext3DCocoa.mm
    platform/graphics/cocoa/GraphicsContextCocoa.mm
    platform/graphics/cocoa/FontCacheCoreText.cpp
    platform/graphics/cocoa/FontCascadeCocoa.mm
    platform/graphics/cocoa/FontCocoa.mm
    platform/graphics/cocoa/FontDescriptionCocoa.cpp
    platform/graphics/cocoa/FontFamilySpecificationCoreText.cpp
    platform/graphics/cocoa/FontPlatformDataCocoa.mm
    platform/graphics/cocoa/IOSurface.mm
    platform/graphics/cocoa/IOSurfacePoolCocoa.mm
    platform/graphics/cocoa/WebActionDisablingCALayerDelegate.mm
    platform/graphics/cocoa/WebCoreCALayerExtras.mm
    platform/graphics/cocoa/WebCoreDecompressionSession.mm
    platform/graphics/cocoa/WebGLLayer.mm
    platform/graphics/cocoa/WebGPULayer.mm

    platform/graphics/cv/PixelBufferConformerCV.cpp
    platform/graphics/cv/TextureCacheCV.mm
    platform/graphics/cv/VideoTextureCopierCV.cpp

    platform/graphics/gpu/Texture.cpp
    platform/graphics/gpu/TilingData.cpp

    platform/graphics/mac/ColorMac.mm
    platform/graphics/mac/ComplexTextControllerCoreText.mm
    platform/graphics/mac/DisplayRefreshMonitorMac.cpp
    platform/graphics/mac/FloatPointMac.mm
    platform/graphics/mac/FloatRectMac.mm
    platform/graphics/mac/FloatSizeMac.mm
    platform/graphics/mac/FontCacheMac.mm
    platform/graphics/mac/FontCustomPlatformData.cpp
    platform/graphics/mac/GlyphPageMac.cpp
    platform/graphics/mac/IconMac.mm
    platform/graphics/mac/ImageMac.mm
    platform/graphics/mac/IntPointMac.mm
    platform/graphics/mac/IntRectMac.mm
    platform/graphics/mac/IntSizeMac.mm
    platform/graphics/mac/MediaPlayerPrivateQTKit.mm
    platform/graphics/mac/MediaTimeQTKit.mm
    platform/graphics/mac/PDFDocumentImageMac.mm
    platform/graphics/mac/SimpleFontDataCoreText.cpp
    platform/graphics/mac/WebLayer.mm

    platform/graphics/metal/GPUBufferMetal.mm
    platform/graphics/metal/GPUCommandBufferMetal.mm
    platform/graphics/metal/GPUCommandQueueMetal.mm
    platform/graphics/metal/GPUComputeCommandEncoderMetal.mm
    platform/graphics/metal/GPUComputePipelineStateMetal.mm
    platform/graphics/metal/GPUDepthStencilDescriptorMetal.mm
    platform/graphics/metal/GPUDepthStencilStateMetal.mm
    platform/graphics/metal/GPUDeviceMetal.mm
    platform/graphics/metal/GPUDrawableMetal.mm
    platform/graphics/metal/GPUFunctionMetal.mm
    platform/graphics/metal/GPULibraryMetal.mm
    platform/graphics/metal/GPURenderCommandEncoderMetal.mm
    platform/graphics/metal/GPURenderPassAttachmentDescriptorMetal.mm
    platform/graphics/metal/GPURenderPassColorAttachmentDescriptorMetal.mm
    platform/graphics/metal/GPURenderPassDepthAttachmentDescriptorMetal.mm
    platform/graphics/metal/GPURenderPassDescriptorMetal.mm
    platform/graphics/metal/GPURenderPipelineColorAttachmentDescriptorMetal.mm
    platform/graphics/metal/GPURenderPipelineDescriptorMetal.mm
    platform/graphics/metal/GPURenderPipelineStateMetal.mm
    platform/graphics/metal/GPUTextureDescriptorMetal.mm
    platform/graphics/metal/GPUTextureMetal.mm

    platform/graphics/opengl/Extensions3DOpenGL.cpp
    platform/graphics/opengl/Extensions3DOpenGLCommon.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGL.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGLCommon.cpp
    platform/graphics/opengl/TemporaryOpenGLSetting.cpp

    platform/graphics/opentype/OpenTypeCG.cpp
    platform/graphics/opentype/OpenTypeMathData.cpp

    platform/mac/BlacklistUpdater.mm
    platform/mac/CursorMac.mm
    platform/mac/DragDataMac.mm
    platform/mac/DragImageMac.mm
    platform/mac/EventLoopMac.mm
    platform/mac/FileSystemMac.mm
    platform/mac/KeyEventMac.mm
    platform/mac/LocalCurrentGraphicsContext.mm
    platform/mac/LoggingMac.mm
    platform/mac/MediaRemoteSoftLink.cpp
    platform/mac/NSScrollerImpDetails.mm
    platform/mac/PasteboardMac.mm
    platform/mac/PasteboardWriter.mm
    platform/mac/PlatformEventFactoryMac.mm
    platform/mac/PlatformPasteboardMac.mm
    platform/mac/PlatformScreenMac.mm
    platform/mac/PlatformSpeechSynthesizerMac.mm
    platform/mac/PluginBlacklist.mm
    platform/mac/PowerObserverMac.cpp
    platform/mac/PublicSuffixMac.mm
    platform/mac/RemoteCommandListenerMac.mm
    platform/mac/SSLKeyGeneratorMac.mm
    platform/mac/ScrollAnimatorMac.mm
    platform/mac/ScrollViewMac.mm
    platform/mac/ScrollbarThemeMac.mm
    platform/mac/SerializedPlatformRepresentationMac.mm
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
    platform/mac/WebGLBlacklist.mm
    platform/mac/WebNSAttributedStringExtras.mm
    platform/mac/WebVideoFullscreenController.mm
    platform/mac/WebVideoFullscreenHUDWindowController.mm
    platform/mac/WebWindowAnimation.mm
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

    platform/posix/FileSystemPOSIX.cpp

    platform/text/cf/HyphenationCF.cpp

    platform/text/mac/LocaleMac.mm
    platform/text/mac/TextBoundaries.mm
    platform/text/mac/TextCheckingMac.mm
    platform/text/mac/TextEncodingRegistryMac.mm

    rendering/RenderThemeCocoa.mm
    rendering/RenderThemeMac.mm
    rendering/TextAutoSizing.cpp

    xml/SoftLinkLibxslt.cpp
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
    workers

    workers/service/context

    Modules/applepay
    Modules/applicationmanifest
    Modules/cache
    Modules/geolocation
    Modules/indexeddb
    Modules/mediastream
    Modules/notifications
    Modules/webdatabase
    Modules/websockets

    Modules/indexeddb/client
    Modules/indexeddb/shared
    Modules/indexeddb/server

    bindings/js

    bridge/objc
    bridge/jsc

    css/parser

    editing/cocoa
    editing/mac
    editing/ios

    html/canvas
    html/forms
    html/parser
    html/shadow

    inspector/agents

    loader/appcache
    loader/archive
    loader/cache
    loader/cocoa

    loader/archive/cf

    page/animation
    page/cocoa
    page/csp
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

    platform/audio/cocoa

    platform/gamepad/cocoa
    platform/gamepad/mac

    platform/graphics/ca
    platform/graphics/cocoa
    platform/graphics/cg
    platform/graphics/filters
    platform/graphics/opentype
    platform/graphics/mac
    platform/graphics/transforms

    platform/graphics/ca/cocoa

    platform/mediastream/libwebrtc

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

    workers/service

    workers/service/server

    xml
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

    history/HistoryItem.h
    history/PageCache.h

    html/HTMLMediaElement.h

    loader/appcache/ApplicationCacheStorage.h

    loader/icon/IconDatabase.h
    loader/icon/IconDatabaseBase.h
    loader/icon/IconDatabaseClient.h

    loader/mac/LoaderNSURLExtras.h

    platform/PlatformExportMacros.h

    platform/audio/AudioHardwareListener.h

    platform/cf/RunLoopObserver.h

    platform/cocoa/MachSendRight.h
    platform/cocoa/SoftLinking.h

    platform/graphics/cocoa/IOSurface.h

    platform/graphics/transforms/AffineTransform.h

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

list(APPEND WebCoreTestSupport_LIBRARIES PRIVATE WebCore)
list(APPEND WebCoreTestSupport_SOURCES
    testing/Internals.mm
    testing/MockContentFilter.cpp
    testing/MockContentFilterSettings.cpp
    testing/MockPreviewLoaderClient.cpp

    testing/cocoa/WebArchiveDumpSupport.mm
)

set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} "-compatibility_version 1 -current_version ${WEBKIT_MAC_VERSION}")
