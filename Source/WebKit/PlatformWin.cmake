set(WebKit_OUTPUT_NAME WebKit2)
set(WebKit_WebProcess_OUTPUT_NAME WebKitWebProcess)
set(WebKit_NetworkProcess_OUTPUT_NAME WebKitNetworkProcess)
set(WebKit_PluginProcess_OUTPUT_NAME WebKitPluginProcess)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT_DIR})

add_definitions(-DBUILDING_WEBKIT)

list(APPEND WebKit_SOURCES
    NetworkProcess/win/NetworkProcessMainWin.cpp

    Platform/IPC/win/AttachmentWin.cpp
    Platform/IPC/win/ConnectionWin.cpp

    Platform/classifier/ResourceLoadStatisticsClassifier.cpp

    Platform/win/LoggingWin.cpp
    Platform/win/ModuleWin.cpp
    Platform/win/SharedMemoryWin.cpp

    Shared/Plugins/Netscape/NetscapePluginModuleNone.cpp

    Shared/win/ChildProcessMainWin.cpp
    Shared/win/NativeWebKeyboardEventWin.cpp
    Shared/win/NativeWebMouseEventWin.cpp
    Shared/win/NativeWebTouchEventWin.cpp
    Shared/win/NativeWebWheelEventWin.cpp
    Shared/win/WebEventFactory.cpp

    UIProcess/AcceleratedDrawingAreaProxy.cpp
    UIProcess/BackingStore.cpp
    UIProcess/DefaultUndoController.cpp
    UIProcess/DrawingAreaProxyImpl.cpp
    UIProcess/LegacySessionStateCodingNone.cpp
    UIProcess/WebGrammarDetail.cpp
    UIProcess/WebResourceLoadStatisticsStore.cpp
    UIProcess/WebResourceLoadStatisticsTelemetry.cpp
    UIProcess/WebViewportAttributes.cpp

    UIProcess/API/C/WKViewportAttributes.cpp

    UIProcess/API/C/curl/WKWebsiteDataStoreRefCurl.cpp

    UIProcess/API/C/win/WKView.cpp

    UIProcess/API/win/APIWebsiteDataStoreWin.cpp

    UIProcess/Launcher/win/ProcessLauncherWin.cpp

    UIProcess/WebStorage/StorageManager.cpp

    UIProcess/WebsiteData/curl/WebsiteDataStoreCurl.cpp

    UIProcess/WebsiteData/win/WebsiteDataStoreWin.cpp

    UIProcess/win/PageClientImpl.cpp
    UIProcess/win/TextCheckerWin.cpp
    UIProcess/win/WebContextMenuProxyWin.cpp
    UIProcess/win/WebInspectorProxyWin.cpp
    UIProcess/win/WebPageProxyWin.cpp
    UIProcess/win/WebPopupMenuProxyWin.cpp
    UIProcess/win/WebPreferencesWin.cpp
    UIProcess/win/WebProcessPoolWin.cpp
    UIProcess/win/WebView.cpp

    WebProcess/InjectedBundle/win/InjectedBundleWin.cpp

    WebProcess/MediaCache/WebMediaKeyStorageManager.cpp

    WebProcess/Plugins/Netscape/NetscapePluginNone.cpp
    WebProcess/Plugins/Netscape/win/PluginProxyWin.cpp

    WebProcess/WebCoreSupport/win/WebContextMenuClientWin.cpp
    WebProcess/WebCoreSupport/win/WebPopupMenuWin.cpp

    WebProcess/WebPage/AcceleratedDrawingArea.cpp
    WebProcess/WebPage/AcceleratedSurface.cpp
    WebProcess/WebPage/DrawingAreaImpl.cpp
    WebProcess/WebPage/LayerTreeHost.cpp

    WebProcess/WebPage/CoordinatedGraphics/CompositingCoordinator.cpp
    WebProcess/WebPage/CoordinatedGraphics/CoordinatedLayerTreeHost.cpp
    WebProcess/WebPage/CoordinatedGraphics/ThreadedCoordinatedLayerTreeHost.cpp

    WebProcess/WebPage/win/WebInspectorUIWin.cpp
    WebProcess/WebPage/win/WebPageWin.cpp

    WebProcess/win/WebProcessMainWin.cpp
    WebProcess/win/WebProcessWin.cpp
)

# DerivedSources/JavaScriptCore/inspector/InspectorBackendCommands.js is
# expected in DerivedSources/WebInspectorUI/UserInterface/Protocol/.
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
    DEPENDS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendCommands.js
    COMMAND cp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendCommands.js ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
)

list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/NetworkProcess/win"
    "${WEBKIT_DIR}/Platform/classifier"
    "${WEBKIT_DIR}/PluginProcess/win"
    "${WEBKIT_DIR}/Shared/API/c/win"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics/threadedcompositor"
    "${WEBKIT_DIR}/Shared/Plugins/win"
    "${WEBKIT_DIR}/Shared/unix"
    "${WEBKIT_DIR}/Shared/win"
    "${WEBKIT_DIR}/UIProcess/API/C/cairo"
    "${WEBKIT_DIR}/UIProcess/API/C/curl"
    "${WEBKIT_DIR}/UIProcess/API/C/win"
    "${WEBKIT_DIR}/UIProcess/API/cpp/win"
    "${WEBKIT_DIR}/UIProcess/API/win"
    "${WEBKIT_DIR}/UIProcess/Plugins/win"
    "${WEBKIT_DIR}/UIProcess/win"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/win"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/win/DOM"
    "${WEBKIT_DIR}/WebProcess/win"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/win"
    "${WEBKIT_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT_DIR}/WebProcess/WebPage/win"
    "${WEBKIT_DIR}/win"
)

list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
)

set(WebKitCommonIncludeDirectories ${WebKit_INCLUDE_DIRECTORIES})
set(WebKitCommonSystemIncludeDirectories ${WebKit_SYSTEM_INCLUDE_DIRECTORIES})

list(APPEND WebProcess_SOURCES
    WebProcess/EntryPoint/win/WebProcessMain.cpp
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/win/NetworkProcessMain.cpp
)

if (${ENABLE_PLUGIN_PROCESS})
    list(APPEND PluginProcess_SOURCES
    )
endif ()

if (${WTF_PLATFORM_WIN_CAIRO})
    add_definitions(-DUSE_CAIRO=1 -DUSE_CURL=1)

    list(APPEND WebKit_SOURCES
        NetworkProcess/Cookies/curl/WebCookieManagerCurl.cpp

        NetworkProcess/cache/NetworkCacheDataCurl.cpp
        NetworkProcess/cache/NetworkCacheIOChannelCurl.cpp

        NetworkProcess/curl/NetworkDataTaskCurl.cpp
        NetworkProcess/curl/NetworkProcessCurl.cpp
        NetworkProcess/curl/NetworkSessionCurl.cpp
        NetworkProcess/curl/RemoteNetworkingContextCurl.cpp

        Shared/API/c/cairo/WKImageCairo.cpp

        Shared/cairo/ShareableBitmapCairo.cpp

        Shared/curl/WebCoreArgumentCodersCurl.cpp

        UIProcess/Automation/cairo/WebAutomationSessionCairo.cpp

        UIProcess/cairo/BackingStoreCairo.cpp

        WebProcess/WebCoreSupport/curl/WebFrameNetworkingContext.cpp
    )

    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/network/curl"
        "${WEBKIT_DIR}/NetworkProcess/curl"
        "${WEBKIT_DIR}/WebProcess/WebCoreSupport/curl"
    )

    list(APPEND WebKit_LIBRARIES
        PRIVATE
            ${OPENSSL_LIBRARIES}
            mfuuid.lib
            strmiids.lib
    )
endif ()

set(SharedWebKitLibraries
    ${WebKit_LIBRARIES}
)

WEBKIT_WRAP_SOURCELIST(${WebKit_SOURCES})

set(WebKit_FORWARDING_HEADERS_DIRECTORIES
    Shared/API/c

    Shared/API/c/cairo
    Shared/API/c/cf
    Shared/API/c/win

    UIProcess/API/C
    UIProcess/API/cpp

    UIProcess/API/C/curl
    UIProcess/API/C/win

    WebProcess/InjectedBundle/API/c
)

WEBKIT_MAKE_FORWARDING_HEADERS(WebKit
    DIRECTORIES ${WebKit_FORWARDING_HEADERS_DIRECTORIES}
    FLATTENED
)
