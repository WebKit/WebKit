list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/accessibility/mac"
    "${WEBCORE_DIR}/page/cocoa"
    "${WEBCORE_DIR}/page/mac"
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
    "${WEBCORE_DIR}/plugins/mac"
)

list(APPEND WebCore_SOURCES
    accessibility/mac/AXObjectCacheMac.mm
    accessibility/mac/AccessibilityObjectMac.mm
    accessibility/mac/WebAccessibilityObjectWrapperBase.mm
    accessibility/mac/WebAccessibilityObjectWrapperMac.mm

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

    platform/graphics/avfoundation/cf/CDMSessionAVFoundationCF.cpp
    platform/graphics/avfoundation/cf/InbandTextTrackPrivateAVCF.cpp
    platform/graphics/avfoundation/cf/InbandTextTrackPrivateLegacyAVCF.cpp
    platform/graphics/avfoundation/cf/WebCoreAVCFResourceLoader.cpp

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
    platform/graphics/opentype/OpenTypeUtilities.cpp
    platform/graphics/opentype/OpenTypeVerticalData.cpp

    platform/graphics/win/DIBPixelData.cpp
    platform/graphics/win/GDIExtras.cpp
    platform/graphics/win/IconWin.cpp
    platform/graphics/win/ImageWin.cpp
    platform/graphics/win/IntPointWin.cpp
    platform/graphics/win/IntRectWin.cpp
    platform/graphics/win/IntSizeWin.cpp

    platform/mac/BlockExceptions.mm
    platform/mac/ContentFilterMac.mm
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
    platform/mac/LocalizedStringsMac.cpp
    platform/mac/LoggingMac.mm
    platform/mac/MIMETypeRegistryMac.mm
    platform/mac/MediaTimeMac.cpp
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
    platform/mac/PurgeableBufferMac.cpp
    platform/mac/SSLKeyGeneratorMac.cpp
    platform/mac/ScrollAnimatorMac.mm
    platform/mac/ScrollElasticityController.mm
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
    platform/mac/WebCoreView.m
    platform/mac/WebFontCache.mm
    platform/mac/WebNSAttributedStringExtras.mm
    platform/mac/WebVideoFullscreenController.mm
    platform/mac/WebVideoFullscreenHUDWindowController.mm
    platform/mac/WebWindowAnimation.mm
    platform/mac/WidgetMac.mm

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

    plugins/mac/PluginPackageMac.cpp
    plugins/mac/PluginViewMac.mm
)
