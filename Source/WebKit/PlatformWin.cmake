set(WebKit_OUTPUT_NAME WebKit2)
set(WebProcess_OUTPUT_NAME WebKitWebProcess)
set(NetworkProcess_OUTPUT_NAME WebKitNetworkProcess)
set(GPUProcess_OUTPUT_NAME WebKitGPUProcess)
set(PluginProcess_OUTPUT_NAME WebKitPluginProcess)

include(Headers.cmake)

list(APPEND WebKit_SOURCES
    GPUProcess/graphics/RemoteGraphicsContextGLWC.cpp

    GPUProcess/graphics/wc/RemoteWCLayerTreeHost.cpp
    GPUProcess/graphics/wc/WCContentBufferManager.cpp
    GPUProcess/graphics/wc/WCScene.cpp
    GPUProcess/graphics/wc/WCSceneContext.cpp

    GPUProcess/media/win/RemoteMediaPlayerProxyWin.cpp

    GPUProcess/win/GPUProcessMainWin.cpp
    GPUProcess/win/GPUProcessWin.cpp

    NetworkProcess/Classifier/WebResourceLoadStatisticsStore.cpp

    Platform/IPC/win/ArgumentCodersWin.cpp
    Platform/IPC/win/ConnectionWin.cpp
    Platform/IPC/win/IPCSemaphoreWin.cpp

    Platform/classifier/ResourceLoadStatisticsClassifier.cpp

    Platform/win/LoggingWin.cpp
    Platform/win/ModuleWin.cpp
    Platform/win/SharedMemoryWin.cpp

    Shared/Plugins/Netscape/NetscapePluginModuleNone.cpp

    Shared/win/AuxiliaryProcessMainWin.cpp
    Shared/win/NativeWebKeyboardEventWin.cpp
    Shared/win/NativeWebMouseEventWin.cpp
    Shared/win/NativeWebTouchEventWin.cpp
    Shared/win/NativeWebWheelEventWin.cpp
    Shared/win/WebCoreArgumentCodersWin.cpp
    Shared/win/WebEventFactory.cpp

    UIProcess/BackingStore.cpp
    UIProcess/DefaultUndoController.cpp
    UIProcess/LegacySessionStateCodingNone.cpp
    UIProcess/WebGrammarDetail.cpp
    UIProcess/WebViewportAttributes.cpp

    UIProcess/API/C/WKViewportAttributes.cpp

    UIProcess/API/C/win/WKView.cpp

    UIProcess/CoordinatedGraphics/DrawingAreaProxyCoordinatedGraphics.cpp

    UIProcess/Inspector/win/InspectorResourceURLSchemeHandler.cpp
    UIProcess/Inspector/win/WebInspectorUIProxyWin.cpp

    UIProcess/Launcher/win/ProcessLauncherWin.cpp

    UIProcess/WebsiteData/win/WebsiteDataStoreWin.cpp

    UIProcess/wc/DrawingAreaProxyWC.cpp

    UIProcess/win/PageClientImpl.cpp
    UIProcess/win/WebContextMenuProxyWin.cpp
    UIProcess/win/WebPageProxyWin.cpp
    UIProcess/win/WebPopupMenuProxyWin.cpp
    UIProcess/win/WebProcessPoolWin.cpp
    UIProcess/win/WebView.cpp

    WebProcess/GPU/graphics/wc/RemoteGraphicsContextGLProxyWC.cpp
    WebProcess/GPU/graphics/wc/RemoteWCLayerTreeHostProxy.cpp

    WebProcess/GPU/media/win/VideoLayerRemoteWin.cpp

    WebProcess/InjectedBundle/win/InjectedBundleWin.cpp

    WebProcess/Inspector/win/RemoteWebInspectorUIWin.cpp
    WebProcess/Inspector/win/WebInspectorUIWin.cpp

    WebProcess/MediaCache/WebMediaKeyStorageManager.cpp

    WebProcess/WebCoreSupport/win/WebPopupMenuWin.cpp

    WebProcess/WebPage/AcceleratedSurface.cpp

    WebProcess/WebPage/CoordinatedGraphics/CompositingCoordinator.cpp
    WebProcess/WebPage/CoordinatedGraphics/DrawingAreaCoordinatedGraphics.cpp
    WebProcess/WebPage/CoordinatedGraphics/LayerTreeHostTextureMapper.cpp

    WebProcess/WebPage/wc/DrawingAreaWC.cpp
    WebProcess/WebPage/wc/GraphicsLayerWC.cpp
    WebProcess/WebPage/wc/WCLayerFactory.cpp
    WebProcess/WebPage/wc/WCTileGrid.cpp

    WebProcess/WebPage/win/WebPageWin.cpp

    WebProcess/win/WebProcessMainWin.cpp
    WebProcess/win/WebProcessWin.cpp
)

list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/GPUProcess/graphics/wc"
    "${WEBKIT_DIR}/Platform/IPC/win"
    "${WEBKIT_DIR}/Platform/classifier"
    "${WEBKIT_DIR}/Platform/generic"
    "${WEBKIT_DIR}/PluginProcess/win"
    "${WEBKIT_DIR}/Shared/API/c/win"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics/threadedcompositor"
    "${WEBKIT_DIR}/Shared/Plugins/win"
    "${WEBKIT_DIR}/Shared/wc"
    "${WEBKIT_DIR}/Shared/win"
    "${WEBKIT_DIR}/UIProcess/API/C/win"
    "${WEBKIT_DIR}/UIProcess/API/cpp/win"
    "${WEBKIT_DIR}/UIProcess/API/win"
    "${WEBKIT_DIR}/UIProcess/CoordinatedGraphics"
    "${WEBKIT_DIR}/UIProcess/Inspector/socket"
    "${WEBKIT_DIR}/UIProcess/Inspector/win"
    "${WEBKIT_DIR}/UIProcess/Plugins/win"
    "${WEBKIT_DIR}/UIProcess/wc"
    "${WEBKIT_DIR}/UIProcess/win"
    "${WEBKIT_DIR}/WebProcess/GPU/graphics/wc"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/win"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/win/DOM"
    "${WEBKIT_DIR}/WebProcess/Inspector/win"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/win"
    "${WEBKIT_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT_DIR}/WebProcess/WebPage/wc"
    "${WEBKIT_DIR}/WebProcess/WebPage/win"
    "${WEBKIT_DIR}/win"
)

list(APPEND WebKit_MESSAGES_IN_FILES
    GPUProcess/graphics/wc/RemoteWCLayerTreeHost
)

set(WebKitCommonIncludeDirectories ${WebKit_INCLUDE_DIRECTORIES})
set(WebKitCommonSystemIncludeDirectories ${WebKit_SYSTEM_INCLUDE_DIRECTORIES})

list(APPEND WebProcess_SOURCES
    WebProcess/EntryPoint/win/WebProcessMain.cpp
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/win/NetworkProcessMain.cpp
)

list(APPEND GPUProcess_SOURCES
    GPUProcess/EntryPoint/win/GPUProcessMain.cpp
)

if (${WTF_PLATFORM_WIN_CAIRO})
    list(APPEND WebKit_SOURCES
        NetworkProcess/Cookies/curl/WebCookieManagerCurl.cpp

        NetworkProcess/cache/NetworkCacheDataCurl.cpp
        NetworkProcess/cache/NetworkCacheIOChannelCurl.cpp

        NetworkProcess/curl/NetworkDataTaskCurl.cpp
        NetworkProcess/curl/NetworkProcessCurl.cpp
        NetworkProcess/curl/NetworkProcessMainCurl.cpp
        NetworkProcess/curl/NetworkSessionCurl.cpp
        NetworkProcess/curl/WebSocketTaskCurl.cpp

        Shared/API/c/cairo/WKImageCairo.cpp

        Shared/API/c/curl/WKCertificateInfoCurl.cpp

        Shared/cairo/ShareableBitmapCairo.cpp

        Shared/curl/WebCoreArgumentCodersCurl.cpp

        UIProcess/API/C/curl/WKProtectionSpaceCurl.cpp
        UIProcess/API/C/curl/WKWebsiteDataStoreRefCurl.cpp

        UIProcess/Automation/cairo/WebAutomationSessionCairo.cpp

        UIProcess/WebsiteData/curl/WebsiteDataStoreCurl.cpp

        UIProcess/cairo/BackingStoreCairo.cpp

        WebProcess/WebCoreSupport/curl/WebFrameNetworkingContext.cpp
    )

    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBKIT_DIR}/NetworkProcess/curl"
        "${WEBKIT_DIR}/UIProcess/API/C/cairo"
        "${WEBKIT_DIR}/UIProcess/API/C/curl"
        "${WEBKIT_DIR}/WebProcess/WebCoreSupport/curl"
    )

    list(APPEND WebKit_PRIVATE_LIBRARIES
        comctl32
    )
endif ()

if (ENABLE_REMOTE_INSPECTOR)
    list(APPEND WebKit_SOURCES
        UIProcess/Inspector/socket/RemoteInspectorClient.cpp
        UIProcess/Inspector/socket/RemoteInspectorProtocolHandler.cpp

        UIProcess/Inspector/win/RemoteWebInspectorUIProxyWin.cpp
    )

    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBKIT_DIR}/UIProcess/socket"
    )
endif ()

# Windows specific
list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
    Shared/API/c/win/WKBaseWin.h

    UIProcess/API/C/win/WKView.h
)

if (${WTF_PLATFORM_WIN_CAIRO})
    list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
        Shared/API/c/cairo/WKImageCairo.h

        Shared/API/c/curl/WKCertificateInfoCurl.h

        UIProcess/API/C/curl/WKProtectionSpaceCurl.h
        UIProcess/API/C/curl/WKWebsiteDataStoreRefCurl.h
    )
endif ()
