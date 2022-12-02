include(Headers.cmake)

set(WebKit_USE_PREFIX_HEADER ON)

list(APPEND WebProcess_SOURCES
    WebProcess/EntryPoint/playstation/WebProcessMain.cpp
)
list(APPEND WebProcess_PRIVATE_LIBRARIES
    ${EGL_LIBRARIES}
    ${ProcessLauncher_LIBRARY}
    OpenSSL::Crypto
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/playstation/NetworkProcessMain.cpp
)
list(APPEND NetworkProcess_PRIVATE_LIBRARIES
    ${ProcessLauncher_LIBRARY}
    OpenSSL::Crypto
)

list(APPEND GPUProcess_SOURCES
    GPUProcess/EntryPoint/playstation/GPUProcessMain.cpp
)
list(APPEND GPUProcess_PRIVATE_LIBRARIES
    ${ProcessLauncher_LIBRARY}
    ${EGL_LIBRARIES}
)

list(APPEND WebKit_SOURCES
    GPUProcess/media/playstation/RemoteMediaPlayerProxyPlayStation.cpp

    GPUProcess/playstation/GPUProcessMainPlayStation.cpp
    GPUProcess/playstation/GPUProcessPlayStation.cpp

    NetworkProcess/Classifier/WebResourceLoadStatisticsStore.cpp

    NetworkProcess/Cookies/curl/WebCookieManagerCurl.cpp

    NetworkProcess/cache/NetworkCacheDataCurl.cpp
    NetworkProcess/cache/NetworkCacheIOChannelCurl.cpp

    NetworkProcess/curl/NetworkDataTaskCurl.cpp
    NetworkProcess/curl/NetworkProcessCurl.cpp
    NetworkProcess/curl/NetworkProcessMainCurl.cpp
    NetworkProcess/curl/NetworkSessionCurl.cpp
    NetworkProcess/curl/WebSocketTaskCurl.cpp

    Platform/IPC/unix/ArgumentCodersUnix.cpp
    Platform/IPC/unix/ConnectionUnix.cpp
    Platform/IPC/unix/IPCSemaphoreUnix.cpp

    Platform/classifier/ResourceLoadStatisticsClassifier.cpp

    Platform/unix/LoggingUnix.cpp
    Platform/unix/ModuleUnix.cpp
    Platform/unix/SharedMemoryUnix.cpp

    Shared/API/c/cairo/WKImageCairo.cpp

    Shared/API/c/curl/WKCertificateInfoCurl.cpp

    Shared/API/c/playstation/WKEventPlayStation.cpp

    Shared/Plugins/Netscape/NetscapePluginModuleNone.cpp

    Shared/cairo/ShareableBitmapCairo.cpp

    Shared/curl/WebCoreArgumentCodersCurl.cpp

    Shared/libwpe/NativeWebKeyboardEventLibWPE.cpp
    Shared/libwpe/NativeWebMouseEventLibWPE.cpp
    Shared/libwpe/NativeWebTouchEventLibWPE.cpp
    Shared/libwpe/NativeWebWheelEventLibWPE.cpp
    Shared/libwpe/WebEventFactory.cpp

    Shared/playstation/WebCoreArgumentCodersPlayStation.cpp

    Shared/unix/AuxiliaryProcessMain.cpp

    UIProcess/BackingStore.cpp
    UIProcess/DefaultUndoController.cpp
    UIProcess/LegacySessionStateCodingNone.cpp
    UIProcess/WebGrammarDetail.cpp
    UIProcess/WebMemoryPressureHandler.cpp
    UIProcess/WebViewportAttributes.cpp

    UIProcess/API/C/WKViewportAttributes.cpp

    UIProcess/API/C/curl/WKProtectionSpaceCurl.cpp
    UIProcess/API/C/curl/WKWebsiteDataStoreRefCurl.cpp

    UIProcess/API/C/playstation/WKContextConfigurationPlayStation.cpp
    UIProcess/API/C/playstation/WKPagePrivatePlayStation.cpp
    UIProcess/API/C/playstation/WKRunloop.cpp
    UIProcess/API/C/playstation/WKView.cpp

    UIProcess/Automation/cairo/WebAutomationSessionCairo.cpp

    UIProcess/CoordinatedGraphics/DrawingAreaProxyCoordinatedGraphics.cpp

    UIProcess/Launcher/playstation/ProcessLauncherPlayStation.cpp

    UIProcess/WebsiteData/curl/WebsiteDataStoreCurl.cpp

    UIProcess/WebsiteData/playstation/WebsiteDataStorePlayStation.cpp

    UIProcess/cairo/BackingStoreCairo.cpp

    UIProcess/libwpe/WebPasteboardProxyLibWPE.cpp

    UIProcess/playstation/PageClientImpl.cpp
    UIProcess/playstation/PlayStationWebView.cpp
    UIProcess/playstation/WebPageProxyPlayStation.cpp
    UIProcess/playstation/WebProcessPoolPlayStation.cpp

    WebProcess/GPU/media/playstation/VideoLayerRemotePlayStation.cpp

    WebProcess/InjectedBundle/playstation/InjectedBundlePlayStation.cpp

    WebProcess/WebCoreSupport/curl/WebFrameNetworkingContext.cpp

    WebProcess/WebPage/AcceleratedSurface.cpp

    WebProcess/WebPage/CoordinatedGraphics/CompositingCoordinator.cpp
    WebProcess/WebPage/CoordinatedGraphics/DrawingAreaCoordinatedGraphics.cpp

    WebProcess/WebPage/libwpe/AcceleratedSurfaceLibWPE.cpp

    WebProcess/WebPage/playstation/WebPagePlayStation.cpp

    WebProcess/playstation/WebProcessMainPlayStation.cpp
    WebProcess/playstation/WebProcessPlayStation.cpp
)

list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/NetworkProcess/curl"
    "${WEBKIT_DIR}/Platform/IPC/unix"
    "${WEBKIT_DIR}/Platform/classifier"
    "${WEBKIT_DIR}/Platform/generic"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics/threadedcompositor"
    "${WEBKIT_DIR}/Shared/libwpe"
    "${WEBKIT_DIR}/UIProcess/API/C/cairo"
    "${WEBKIT_DIR}/UIProcess/API/C/curl"
    "${WEBKIT_DIR}/UIProcess/API/C/playstation"
    "${WEBKIT_DIR}/UIProcess/API/playstation"
    "${WEBKIT_DIR}/UIProcess/CoordinatedGraphics"
    "${WEBKIT_DIR}/UIProcess/playstation"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/curl"
    "${WEBKIT_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT_DIR}/WebProcess/WebPage/libwpe"
)

if (ENABLE_GAMEPAD)
    list(APPEND WebKit_SOURCES
        UIProcess/Gamepad/libwpe/UIGamepadProviderLibWPE.cpp
    )
endif ()

if (USE_COORDINATED_GRAPHICS)
    list(APPEND WebKit_SOURCES
        Shared/CoordinatedGraphics/CoordinatedGraphicsScene.cpp
        Shared/CoordinatedGraphics/SimpleViewportController.cpp

        Shared/CoordinatedGraphics/threadedcompositor/CompositingRunLoop.cpp
        Shared/CoordinatedGraphics/threadedcompositor/ThreadedCompositor.cpp
        Shared/CoordinatedGraphics/threadedcompositor/ThreadedDisplayRefreshMonitor.cpp

        WebProcess/WebPage/CoordinatedGraphics/LayerTreeHost.cpp
    )
endif ()

if (USE_GRAPHICS_LAYER_WC)
    list(APPEND WebKit_SOURCES
        GPUProcess/graphics/RemoteGraphicsContextGLWC.cpp

        GPUProcess/graphics/wc/RemoteWCLayerTreeHost.cpp
        GPUProcess/graphics/wc/WCContentBufferManager.cpp
        GPUProcess/graphics/wc/WCScene.cpp
        GPUProcess/graphics/wc/WCSceneContext.cpp

        UIProcess/wc/DrawingAreaProxyWC.cpp

        WebProcess/GPU/graphics/wc/RemoteGraphicsContextGLProxyWC.cpp
        WebProcess/GPU/graphics/wc/RemoteWCLayerTreeHostProxy.cpp

        WebProcess/WebPage/CoordinatedGraphics/LayerTreeHostTextureMapper.cpp

        WebProcess/WebPage/wc/DrawingAreaWC.cpp
        WebProcess/WebPage/wc/GraphicsLayerWC.cpp
        WebProcess/WebPage/wc/WCLayerFactory.cpp
        WebProcess/WebPage/wc/WCTileGrid.cpp
    )

    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBKIT_DIR}/GPUProcess/graphics/wc"
        "${WEBKIT_DIR}/Shared/wc"
        "${WEBKIT_DIR}/UIProcess/wc"
        "${WEBKIT_DIR}/WebProcess/GPU/graphics/wc"
        "${WEBKIT_DIR}/WebProcess/WebPage/wc"
    )

    list(APPEND WebKit_MESSAGES_IN_FILES
        GPUProcess/graphics/wc/RemoteWCLayerTreeHost
    )
endif ()

if (USE_WPE_BACKEND_PLAYSTATION)
    list(APPEND WebKit_SOURCES
        UIProcess/Launcher/libwpe/ProcessProviderLibWPE.cpp

        UIProcess/Launcher/playstation/ProcessProviderPlayStation.cpp
    )
    list(APPEND WebKit_INCLUDE_DIRECTORIES "${WEBKIT_DIR}/UIProcess/Launcher/libwpe")
endif ()

# PlayStation specific
list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
    Shared/API/c/cairo/WKImageCairo.h

    Shared/API/c/curl/WKCertificateInfoCurl.h

    Shared/API/c/playstation/WKBasePlayStation.h
    Shared/API/c/playstation/WKEventPlayStation.h

    UIProcess/API/C/curl/WKProtectionSpaceCurl.h
    UIProcess/API/C/curl/WKWebsiteDataStoreRefCurl.h

    UIProcess/API/C/playstation/WKContextConfigurationPlayStation.h
    UIProcess/API/C/playstation/WKPagePrivatePlayStation.h
    UIProcess/API/C/playstation/WKRunloop.h
    UIProcess/API/C/playstation/WKView.h
    UIProcess/API/C/playstation/WKViewClient.h
)
