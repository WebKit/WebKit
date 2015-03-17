list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/accessibility/mac"
    "${WEBCORE_DIR}/bindings/objc"
    "${WEBCORE_DIR}/bridge/objc"
    "${WEBCORE_DIR}/editing/mac"
    "${WEBCORE_DIR}/loader/archive/cf"
    "${WEBCORE_DIR}/loader/cf"
    "${WEBCORE_DIR}/loader/mac"
    "${WEBCORE_DIR}/page/cocoa"
    "${WEBCORE_DIR}/page/mac"
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

    "/usr/include/libxslt"
    "/usr/include/libxml2"
)

list(APPEND WebCore_SOURCES
    accessibility/mac/AXObjectCacheMac.mm
    accessibility/mac/AccessibilityObjectMac.mm
    accessibility/mac/WebAccessibilityObjectWrapperBase.mm
    accessibility/mac/WebAccessibilityObjectWrapperMac.mm

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

    platform/cocoa/DisplaySleepDisablerCocoa.cpp
    platform/cocoa/KeyEventCocoa.mm
    platform/cocoa/MemoryPressureHandlerCocoa.mm
    platform/cocoa/SystemVersion.mm
    platform/cocoa/TelephoneNumberDetectorCocoa.cpp

    platform/graphics/avfoundation/AVTrackPrivateAVFObjCImpl.mm
    platform/graphics/avfoundation/InbandMetadataTextTrackPrivateAVF.cpp
    platform/graphics/avfoundation/InbandTextTrackPrivateAVF.cpp
    platform/graphics/avfoundation/MediaPlayerPrivateAVFoundation.cpp

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
    platform/mac/SharedTimerMac.mm
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

    platform/network/cocoa/CredentialCocoa.mm
    platform/network/cocoa/ProtectionSpaceCocoa.mm
    platform/network/cocoa/ResourceRequestCocoa.mm

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

    platform/text/cf/HyphenationCF.cpp

    platform/text/mac/LocaleMac.mm
    platform/text/mac/TextBoundaries.mm
    platform/text/mac/TextBreakIteratorInternalICUMac.mm
    platform/text/mac/TextCodecMac.cpp
)

set(WebCore_FORWARDING_HEADERS_DIRECTORIES
    html
    bindings/objc
    platform
    platform/mac
    platform/network/cf
)

set(WebCore_FORWARDING_HEADERS_FILES
    bindings/js/SerializedScriptValue.h
    bindings/objc/WebKitAvailability.h

    bridge/IdentifierRep.h
    bridge/npruntime_internal.h

    contentextensions/CompiledContentExtension.h

    editing/FindOptions.h

    html/HTMLMediaElement.h

    loader/FrameLoaderTypes.h
    loader/LoaderStrategy.h
    loader/ResourceLoaderOptions.h

    Modules/indexeddb/IDBKeyData.h
    Modules/indexeddb/IDBKeyPath.h
    Modules/webdatabase/DatabaseDetails.h

    page/ContextMenuContext.h
    page/SecurityOrigin.h
    page/SessionID.h
    page/TextIndicator.h
    page/UserScript.h
    page/UserStyleSheet.h

    platform/PlatformExportMacros.h
    platform/DisplaySleepDisabler.h

    platform/audio/AudioHardwareListener.h

    platform/cocoa/MachSendRight.h

    platform/graphics/Color.h
    platform/graphics/FloatPoint.h
    platform/graphics/FloatRect.h
    platform/graphics/FloatSize.h
    platform/graphics/GraphicsContext.h
    platform/graphics/GraphicsLayer.h
    platform/graphics/IntPoint.h
    platform/graphics/IntRect.h
    platform/graphics/IntSize.h
    platform/graphics/NativeImagePtr.h

    platform/graphics/cocoa/IOSurface.h

    platform/graphics/transforms/AffineTransform.h

    platform/mac/SoftLinking.h
    platform/mac/WebCoreSystemInterface.h

    platform/network/BlobDataFileReference.h
    platform/network/BlobRegistryImpl.h
    platform/network/HTTPHeaderMap.h
    platform/network/NetworkStorageSession.h
    platform/network/ResourceHandle.h

    platform/network/cf/CertificateInfo.h
    platform/network/cf/ResourceResponse.h

    platform/spi/cg/CoreGraphicsSPI.h

    plugins/PluginData.h
    plugins/npruntime.h
)

WEBKIT_CREATE_FORWARDING_HEADERS(WebCore DIRECTORIES ${WebCore_FORWARDING_HEADERS_DIRECTORIES} FILES ${WebCore_FORWARDING_HEADERS_FILES})

# FIXME: Get Objective C bindings working.
#set(FEATURE_DEFINES_OBJECTIVE_C "LANGUAGE_OBJECTIVE_C=1 ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}")
#set(ADDITIONAL_BINDINGS_DEPENDENCIES
#    ${WINDOW_CONSTRUCTORS_FILE}
#    ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
#    ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
#)
#GENERATE_BINDINGS(WebCore_SOURCES
#    "${WebCore_NON_SVG_IDL_FILES}"
#    "${WEBCORE_DIR}"
#    "${IDL_INCLUDES}"
#    "${FEATURE_DEFINES_OBJECTIVE_C}"
#    ${DERIVED_SOURCES_WEBCORE_DIR} DOM ObjC mm
#    ${IDL_ATTRIBUTES_FILE}
#    ${SUPPLEMENTAL_DEPENDENCY_FILE}
#    ${ADDITIONAL_BINDINGS_DEPENDENCIES})
