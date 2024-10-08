include(Headers.cmake)
include(Platform/Curl.cmake)

set(WebKit_USE_PREFIX_HEADER ON)

WEBKIT_ADD_TARGET_CXX_FLAGS(WebKit -Wno-unused-lambda-capture)

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

    NetworkProcess/NetworkDataTaskDataURL.cpp

    NetworkProcess/Classifier/WebResourceLoadStatisticsStore.cpp

    Platform/IPC/unix/ArgumentCodersUnix.cpp
    Platform/IPC/unix/ConnectionUnix.cpp
    Platform/IPC/unix/IPCSemaphoreUnix.cpp

    Platform/classifier/ResourceLoadStatisticsClassifier.cpp

    Platform/unix/LoggingUnix.cpp
    Platform/unix/ModuleUnix.cpp

    Shared/API/c/playstation/WKEventPlayStation.cpp

    Shared/libwpe/NativeWebKeyboardEventLibWPE.cpp
    Shared/libwpe/NativeWebMouseEventLibWPE.cpp
    Shared/libwpe/NativeWebTouchEventLibWPE.cpp
    Shared/libwpe/NativeWebWheelEventLibWPE.cpp
    Shared/libwpe/WebEventFactory.cpp

    Shared/unix/AuxiliaryProcessMain.cpp

    UIProcess/DefaultUndoController.cpp
    UIProcess/LegacySessionStateCodingNone.cpp
    UIProcess/WebGrammarDetail.cpp
    UIProcess/WebMemoryPressureHandler.cpp
    UIProcess/WebViewportAttributes.cpp

    UIProcess/API/C/WKUserScriptRef.cpp
    UIProcess/API/C/WKViewportAttributes.cpp

    UIProcess/API/C/playstation/WKContextConfigurationPlayStation.cpp
    UIProcess/API/C/playstation/WKPagePrivatePlayStation.cpp
    UIProcess/API/C/playstation/WKRunloop.cpp
    UIProcess/API/C/playstation/WKView.cpp

    UIProcess/CoordinatedGraphics/DrawingAreaProxyCoordinatedGraphics.cpp

    UIProcess/Launcher/playstation/ProcessLauncherPlayStation.cpp

    UIProcess/WebsiteData/playstation/WebsiteDataStorePlayStation.cpp

    UIProcess/libwpe/WebPasteboardProxyLibWPE.cpp

    UIProcess/playstation/DisplayLinkPlayStation.cpp
    UIProcess/playstation/PageClientImpl.cpp
    UIProcess/playstation/PlayStationWebView.cpp
    UIProcess/playstation/WebPageProxyPlayStation.cpp
    UIProcess/playstation/WebProcessPoolPlayStation.cpp

    WebProcess/GPU/media/playstation/VideoLayerRemotePlayStation.cpp

    WebProcess/InjectedBundle/playstation/InjectedBundlePlayStation.cpp

    WebProcess/WebPage/AcceleratedSurface.cpp

    WebProcess/WebPage/CoordinatedGraphics/DrawingAreaCoordinatedGraphics.cpp

    WebProcess/WebPage/libwpe/AcceleratedSurfaceLibWPE.cpp

    WebProcess/WebPage/playstation/WebPagePlayStation.cpp

    WebProcess/playstation/WebProcessMainPlayStation.cpp
    WebProcess/playstation/WebProcessPlayStation.cpp
)

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/Platform/IPC/unix"
    "${WEBKIT_DIR}/Platform/classifier"
    "${WEBKIT_DIR}/Platform/generic"
    "${WEBKIT_DIR}/Shared/libwpe"
    "${WEBKIT_DIR}/UIProcess/API/C/playstation"
    "${WEBKIT_DIR}/UIProcess/API/libwpe"
    "${WEBKIT_DIR}/UIProcess/API/playstation"
    "${WEBKIT_DIR}/UIProcess/CoordinatedGraphics"
    "${WEBKIT_DIR}/UIProcess/playstation"
    "${WEBKIT_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT_DIR}/WebProcess/WebPage/libwpe"
)

if (ENABLE_GAMEPAD)
    list(APPEND WebKit_SOURCES
        UIProcess/Gamepad/libwpe/UIGamepadProviderLibWPE.cpp
    )
endif ()

if (ENABLE_WEBDRIVER AND USE_WPE_BACKEND_PLAYSTATION)
    list(APPEND WebKit_SOURCES
        UIProcess/Automation/libwpe/WebAutomationSessionLibWPE.cpp
    )
endif ()

if (USE_CAIRO)
    list(APPEND WebKit_SOURCES
        Shared/API/c/cairo/WKImageCairo.cpp

        Shared/freetype/WebCoreArgumentCodersFreeType.cpp

        UIProcess/Automation/cairo/WebAutomationSessionCairo.cpp

        UIProcess/cairo/BackingStoreCairo.cpp
    )

    list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBKIT_DIR}/UIProcess/API/C/cairo"
    )

    list(APPEND WebKit_LIBRARIES
        Cairo::Cairo
        Freetype::Freetype
    )

    list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
        Shared/API/c/cairo/WKImageCairo.h
    )
elseif (USE_SKIA)
    include(Platform/Skia.cmake)

    list(APPEND WebKit_SOURCES
        UIProcess/skia/BackingStoreSkia.cpp
    )
endif ()

if (USE_COORDINATED_GRAPHICS)
    list(APPEND WebKit_SOURCES
        WebProcess/WebPage/CoordinatedGraphics/CompositingRunLoop.cpp
        WebProcess/WebPage/CoordinatedGraphics/CoordinatedGraphicsScene.cpp
        WebProcess/WebPage/CoordinatedGraphics/LayerTreeHost.cpp
        WebProcess/WebPage/CoordinatedGraphics/SimpleViewportController.cpp
        WebProcess/WebPage/CoordinatedGraphics/ThreadedCompositor.cpp
        WebProcess/WebPage/CoordinatedGraphics/ThreadedDisplayRefreshMonitor.cpp
    )
endif ()

if (USE_GRAPHICS_LAYER_WC)
    include(Platform/WC.cmake)
endif ()

if (USE_WPE_BACKEND_PLAYSTATION)
    list(APPEND WebKit_SOURCES
        UIProcess/API/libwpe/TouchGestureController.cpp

        UIProcess/Launcher/libwpe/ProcessProviderLibWPE.cpp

        UIProcess/Launcher/playstation/ProcessProviderPlayStation.cpp
    )
    list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES "${WEBKIT_DIR}/UIProcess/Launcher/libwpe")
endif ()

# PlayStation specific
list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
    Shared/API/c/playstation/WKBasePlayStation.h
    Shared/API/c/playstation/WKEventPlayStation.h

    UIProcess/API/C/playstation/WKContextConfigurationPlayStation.h
    UIProcess/API/C/playstation/WKPagePrivatePlayStation.h
    UIProcess/API/C/playstation/WKRunloop.h
    UIProcess/API/C/playstation/WKView.h
    UIProcess/API/C/playstation/WKViewClient.h
)
