include(InspectorGResources.cmake)

set(WebKit_OUTPUT_NAME WPEWebKit)
set(WebKit_WebProcess_OUTPUT_NAME WPEWebProcess)
set(WebKit_NetworkProcess_OUTPUT_NAME WPENetworkProcess)
set(WebKit_StorageProcess_OUTPUT_NAME WPEStorageProcess)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WPE_API_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WPE_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WPE_EXTENSION_DIR})

configure_file(wpe/wpe-webkit.pc.in ${CMAKE_BINARY_DIR}/wpe-webkit.pc @ONLY)

add_definitions(-DWEBKIT2_COMPILATION)

add_definitions(-DLIBEXECDIR="${LIBEXEC_INSTALL_DIR}")
add_definitions(-DLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")

if (NOT DEVELOPER_MODE AND NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
    WEBKIT_ADD_TARGET_PROPERTIES(WebKit LINK_FLAGS "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/webkitglib-symbols.map")
endif ()

# Temporary workaround to allow the build to succeed.
file(REMOVE "${FORWARDING_HEADERS_DIR}/WebCore/Settings.h")

set(WebKit_USE_PREFIX_HEADER ON)

add_custom_target(webkitwpe-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${WEBKIT_DIR} --output ${FORWARDING_HEADERS_DIR} --platform wpe --platform soup
)

 # These symbolic link allows includes like #include <wpe/WebkitWebView.h> which simulates installed headers.
add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WPE_DIR}/wpe
    DEPENDS ${WEBKIT_DIR}/UIProcess/API/wpe
    COMMAND ln -n -s -f ${WEBKIT_DIR}/UIProcess/API/wpe ${FORWARDING_HEADERS_WPE_DIR}/wpe
)

add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WPE_EXTENSION_DIR}/wpe
    DEPENDS ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe
    COMMAND ln -n -s -f ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe ${FORWARDING_HEADERS_WPE_EXTENSION_DIR}/wpe
)

add_custom_target(webkitwpe-fake-api-headers
    DEPENDS ${FORWARDING_HEADERS_WPE_DIR}/wpe
            ${FORWARDING_HEADERS_WPE_EXTENSION_DIR}/wpe
)

set(WEBKIT_EXTRA_DEPENDENCIES
    webkitwpe-fake-api-headers
    webkitwpe-forwarding-headers
)

list(APPEND WebProcess_SOURCES
    WebProcess/EntryPoint/unix/WebProcessMain.cpp
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/unix/NetworkProcessMain.cpp
)

list(APPEND StorageProcess_SOURCES
    StorageProcess/EntryPoint/unix/StorageProcessMain.cpp
)

list(APPEND WebKit_SOURCES
    NetworkProcess/CustomProtocols/LegacyCustomProtocolManager.cpp

    NetworkProcess/CustomProtocols/soup/LegacyCustomProtocolManagerSoup.cpp

    NetworkProcess/cache/NetworkCacheCodersSoup.cpp
    NetworkProcess/cache/NetworkCacheDataSoup.cpp
    NetworkProcess/cache/NetworkCacheIOChannelSoup.cpp

    NetworkProcess/soup/NetworkDataTaskSoup.cpp
    NetworkProcess/soup/NetworkProcessMainSoup.cpp
    NetworkProcess/soup/NetworkProcessSoup.cpp
    NetworkProcess/soup/NetworkSessionSoup.cpp
    NetworkProcess/soup/RemoteNetworkingContextSoup.cpp

    Platform/IPC/glib/GSocketMonitor.cpp

    Platform/IPC/unix/AttachmentUnix.cpp
    Platform/IPC/unix/ConnectionUnix.cpp

    Platform/classifier/ResourceLoadStatisticsClassifier.cpp

    Platform/glib/ModuleGlib.cpp

    Platform/unix/LoggingUnix.cpp
    Platform/unix/SharedMemoryUnix.cpp

    PluginProcess/unix/PluginControllerProxyUnix.cpp
    PluginProcess/unix/PluginProcessMainUnix.cpp
    PluginProcess/unix/PluginProcessUnix.cpp

    Shared/API/c/cairo/WKImageCairo.cpp

    Shared/API/glib/WebKitContextMenu.cpp
    Shared/API/glib/WebKitContextMenuActions.cpp
    Shared/API/glib/WebKitContextMenuItem.cpp
    Shared/API/glib/WebKitHitTestResult.cpp
    Shared/API/glib/WebKitURIRequest.cpp
    Shared/API/glib/WebKitURIResponse.cpp

    Shared/Authentication/soup/AuthenticationManagerSoup.cpp

    Shared/CoordinatedGraphics/CoordinatedBackingStore.cpp
    Shared/CoordinatedGraphics/CoordinatedGraphicsScene.cpp
    Shared/CoordinatedGraphics/SimpleViewportController.cpp

    Shared/CoordinatedGraphics/threadedcompositor/CompositingRunLoop.cpp
    Shared/CoordinatedGraphics/threadedcompositor/ThreadedCompositor.cpp
    Shared/CoordinatedGraphics/threadedcompositor/ThreadedDisplayRefreshMonitor.cpp

    Shared/Plugins/Netscape/unix/NetscapePluginModuleUnix.cpp

    Shared/cairo/ShareableBitmapCairo.cpp

    Shared/glib/WebContextMenuItemGlib.cpp
    Shared/glib/WebErrorsGlib.cpp

    Shared/linux/WebMemorySamplerLinux.cpp

    Shared/soup/WebCoreArgumentCodersSoup.cpp
    Shared/soup/WebErrorsSoup.cpp

    Shared/unix/ChildProcessMain.cpp

    Shared/wpe/NativeWebKeyboardEventWPE.cpp
    Shared/wpe/NativeWebMouseEventWPE.cpp
    Shared/wpe/NativeWebTouchEventWPE.cpp
    Shared/wpe/NativeWebWheelEventWPE.cpp
    Shared/wpe/ProcessExecutablePathWPE.cpp
    Shared/wpe/WebEventFactory.cpp

    StorageProcess/glib/StorageProcessMainGLib.cpp

    UIProcess/AcceleratedDrawingAreaProxy.cpp
    UIProcess/BackingStore.cpp
    UIProcess/DefaultUndoController.cpp
    UIProcess/LegacySessionStateCodingNone.cpp
    UIProcess/WebResourceLoadStatisticsStore.cpp
    UIProcess/WebResourceLoadStatisticsTelemetry.cpp

    UIProcess/API/C/WKGrammarDetail.cpp

    UIProcess/API/C/wpe/WKView.cpp

    UIProcess/API/glib/APIWebsiteDataStoreGLib.cpp
    UIProcess/API/glib/IconDatabase.cpp
    UIProcess/API/glib/WebKitApplicationInfo.cpp
    UIProcess/API/glib/WebKitAuthenticationRequest.cpp
    UIProcess/API/glib/WebKitAutomationSession.cpp
    UIProcess/API/glib/WebKitBackForwardList.cpp
    UIProcess/API/glib/WebKitBackForwardListItem.cpp
    UIProcess/API/glib/WebKitContextMenuClient.cpp
    UIProcess/API/glib/WebKitCookieManager.cpp
    UIProcess/API/glib/WebKitCredential.cpp
    UIProcess/API/glib/WebKitCustomProtocolManagerClient.cpp
    UIProcess/API/glib/WebKitDownload.cpp
    UIProcess/API/glib/WebKitDownloadClient.cpp
    UIProcess/API/glib/WebKitEditorState.cpp
    UIProcess/API/glib/WebKitError.cpp
    UIProcess/API/glib/WebKitFaviconDatabase.cpp
    UIProcess/API/glib/WebKitFileChooserRequest.cpp
    UIProcess/API/glib/WebKitFindController.cpp
    UIProcess/API/glib/WebKitFormClient.cpp
    UIProcess/API/glib/WebKitFormSubmissionRequest.cpp
    UIProcess/API/glib/WebKitGeolocationPermissionRequest.cpp
    UIProcess/API/glib/WebKitGeolocationProvider.cpp
    UIProcess/API/glib/WebKitInjectedBundleClient.cpp
    UIProcess/API/glib/WebKitInstallMissingMediaPluginsPermissionRequest.cpp
    UIProcess/API/glib/WebKitJavascriptResult.cpp
    UIProcess/API/glib/WebKitMimeInfo.cpp
    UIProcess/API/glib/WebKitNavigationAction.cpp
    UIProcess/API/glib/WebKitNavigationClient.cpp
    UIProcess/API/glib/WebKitNavigationPolicyDecision.cpp
    UIProcess/API/glib/WebKitNetworkProxySettings.cpp
    UIProcess/API/glib/WebKitNotification.cpp
    UIProcess/API/glib/WebKitNotificationPermissionRequest.cpp
    UIProcess/API/glib/WebKitNotificationProvider.cpp
    UIProcess/API/glib/WebKitPermissionRequest.cpp
    UIProcess/API/glib/WebKitPlugin.cpp
    UIProcess/API/glib/WebKitPolicyDecision.cpp
    UIProcess/API/glib/WebKitPrivate.cpp
    UIProcess/API/glib/WebKitResponsePolicyDecision.cpp
    UIProcess/API/glib/WebKitScriptDialog.cpp
    UIProcess/API/glib/WebKitSecurityManager.cpp
    UIProcess/API/glib/WebKitSecurityOrigin.cpp
    UIProcess/API/glib/WebKitSettings.cpp
    UIProcess/API/glib/WebKitUIClient.cpp
    UIProcess/API/glib/WebKitURISchemeRequest.cpp
    UIProcess/API/glib/WebKitUserContent.cpp
    UIProcess/API/glib/WebKitUserContentManager.cpp
    UIProcess/API/glib/WebKitUserMediaPermissionRequest.cpp
    UIProcess/API/glib/WebKitWebContext.cpp
    UIProcess/API/glib/WebKitWebResource.cpp
    UIProcess/API/glib/WebKitWebView.cpp
    UIProcess/API/glib/WebKitWebViewSessionState.cpp
    UIProcess/API/glib/WebKitWebsiteData.cpp
    UIProcess/API/glib/WebKitWebsiteDataManager.cpp
    UIProcess/API/glib/WebKitWindowProperties.cpp

    UIProcess/API/wpe/CompositingManagerProxy.cpp
    UIProcess/API/wpe/PageClientImpl.cpp
    UIProcess/API/wpe/ScrollGestureController.cpp
    UIProcess/API/wpe/WebKitScriptDialogWPE.cpp
    UIProcess/API/wpe/WebKitWebViewBackend.cpp
    UIProcess/API/wpe/WebKitWebViewWPE.cpp
    UIProcess/API/wpe/WPEView.cpp

    UIProcess/Automation/cairo/WebAutomationSessionCairo.cpp

    UIProcess/Launcher/glib/ProcessLauncherGLib.cpp

    UIProcess/Network/CustomProtocols/LegacyCustomProtocolManagerProxy.cpp

    UIProcess/Plugins/unix/PluginInfoStoreUnix.cpp
    UIProcess/Plugins/unix/PluginProcessProxyUnix.cpp

    UIProcess/WebStorage/StorageManager.cpp

    UIProcess/WebsiteData/unix/WebsiteDataStoreUnix.cpp

    UIProcess/cairo/BackingStoreCairo.cpp

    UIProcess/gstreamer/InstallMissingMediaPluginsPermissionRequest.cpp
    UIProcess/gstreamer/WebPageProxyGStreamer.cpp

    UIProcess/linux/MemoryPressureMonitor.cpp

    UIProcess/soup/WebCookieManagerProxySoup.cpp
    UIProcess/soup/WebProcessPoolSoup.cpp

    UIProcess/wpe/TextCheckerWPE.cpp
    UIProcess/wpe/WebInspectorProxyWPE.cpp
    UIProcess/wpe/WebPageProxyWPE.cpp
    UIProcess/wpe/WebPasteboardProxyWPE.cpp
    UIProcess/wpe/WebPreferencesWPE.cpp
    UIProcess/wpe/WebProcessPoolWPE.cpp

    WebProcess/Cookies/soup/WebCookieManagerSoup.cpp

    WebProcess/InjectedBundle/API/glib/WebKitConsoleMessage.cpp
    WebProcess/InjectedBundle/API/glib/WebKitExtensionManager.cpp
    WebProcess/InjectedBundle/API/glib/WebKitFrame.cpp
    WebProcess/InjectedBundle/API/glib/WebKitScriptWorld.cpp
    WebProcess/InjectedBundle/API/glib/WebKitWebEditor.cpp
    WebProcess/InjectedBundle/API/glib/WebKitWebExtension.cpp
    WebProcess/InjectedBundle/API/glib/WebKitWebPage.cpp

    WebProcess/InjectedBundle/glib/InjectedBundleGlib.cpp

    WebProcess/MediaCache/WebMediaKeyStorageManager.cpp

    WebProcess/WebCoreSupport/soup/WebFrameNetworkingContext.cpp

    WebProcess/WebCoreSupport/wpe/WebContextMenuClientWPE.cpp
    WebProcess/WebCoreSupport/wpe/WebEditorClientWPE.cpp
    WebProcess/WebCoreSupport/wpe/WebPopupMenuWPE.cpp

    WebProcess/WebPage/AcceleratedDrawingArea.cpp
    WebProcess/WebPage/AcceleratedSurface.cpp

    WebProcess/WebPage/CoordinatedGraphics/CompositingCoordinator.cpp
    WebProcess/WebPage/CoordinatedGraphics/CoordinatedLayerTreeHost.cpp
    WebProcess/WebPage/CoordinatedGraphics/ThreadedCoordinatedLayerTreeHost.cpp

    WebProcess/WebPage/gstreamer/WebPageGStreamer.cpp

    WebProcess/WebPage/wpe/AcceleratedSurfaceWPE.cpp
    WebProcess/WebPage/wpe/CompositingManager.cpp
    WebProcess/WebPage/wpe/WebInspectorUIWPE.cpp
    WebProcess/WebPage/wpe/WebPageWPE.cpp

    WebProcess/soup/WebKitSoupRequestInputStream.cpp
    WebProcess/soup/WebProcessSoup.cpp

    WebProcess/wpe/WebProcessMainWPE.cpp
)

list(APPEND WebKit_MESSAGES_IN_FILES
    NetworkProcess/CustomProtocols/LegacyCustomProtocolManager.messages.in

    UIProcess/API/wpe/CompositingManagerProxy.messages.in

    UIProcess/Network/CustomProtocols/LegacyCustomProtocolManagerProxy.messages.in
)

list(APPEND WebKit_DERIVED_SOURCES
    ${DERIVED_SOURCES_WEBKIT_DIR}/WebKitResourcesGResourceBundle.c

    ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.cpp
)

set(WPE_API_INSTALLED_HEADERS
    ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitApplicationInfo.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitAuthenticationRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitAutomationSession.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitBackForwardList.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitBackForwardListItem.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitCredential.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitContextMenu.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitContextMenuActions.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitContextMenuItem.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitCookieManager.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitDefines.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitDownload.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitEditingCommands.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitEditorState.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitError.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitFaviconDatabase.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitFileChooserRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitFindController.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitFormSubmissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitGeolocationPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitHitTestResult.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitInstallMissingMediaPluginsPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitJavascriptResult.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitMimeInfo.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitNavigationAction.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitNavigationPolicyDecision.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitNetworkProxySettings.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitNotificationPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitNotification.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitPlugin.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitPolicyDecision.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitResponsePolicyDecision.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitScriptDialog.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitSecurityManager.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitSecurityOrigin.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitSettings.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitURIRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitURIResponse.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitURISchemeRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitUserContent.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitUserContentManager.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitUserMediaPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebContext.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebResource.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebView.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebViewBackend.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebViewSessionState.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebsiteData.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebsiteDataManager.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWindowProperties.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/webkit.h
)

# To generate WebKitEnumTypes.h we want to use all installed headers, except WebKitEnumTypes.h itself.
set(WPE_ENUM_GENERATION_HEADERS ${WPE_API_INSTALLED_HEADERS})
list(REMOVE_ITEM WPE_ENUM_GENERATION_HEADERS ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h
           ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.cpp
    DEPENDS ${WPE_ENUM_GENERATION_HEADERS}

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitEnumTypes.h.template ${WPE_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ | sed s/WEBKIT_TYPE_KIT/WEBKIT_TYPE/ > ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitEnumTypes.cpp.template ${WPE_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ > ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.cpp
    VERBATIM
)

set(WebKitResources
)

if (ENABLE_WEB_AUDIO)
    list(APPEND WebKitResources
        "        <file alias=\"audio/Composite\">Composite.wav</file>\n"
    )
endif ()

file(WRITE ${DERIVED_SOURCES_WEBKIT_DIR}/WebKitResourcesGResourceBundle.xml
    "<?xml version=1.0 encoding=UTF-8?>\n"
    "<gresources>\n"
    "    <gresource prefix=\"/org/webkitwpe/resources\">\n"
    ${WebKitResources}
    "    </gresource>\n"
    "</gresources>\n"
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT_DIR}/WebKitResourcesGResourceBundle.c
    DEPENDS ${DERIVED_SOURCES_WEBKIT_DIR}/WebKitResourcesGResourceBundle.xml
    COMMAND glib-compile-resources --generate --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/Resources --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/platform/audio/resources --target=${DERIVED_SOURCES_WEBKIT_DIR}/WebKitResourcesGResourceBundle.c ${DERIVED_SOURCES_WEBKIT_DIR}/WebKitResourcesGResourceBundle.xml
    VERBATIM
)

list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${FORWARDING_HEADERS_DIR}"
    "${FORWARDING_HEADERS_WPE_DIR}"
    "${FORWARDING_HEADERS_WPE_EXTENSION_DIR}"
    "${DERIVED_SOURCES_DIR}"
    "${DERIVED_SOURCES_WPE_API_DIR}"
    "${WEBCORE_DIR}/platform/graphics/cairo"
    "${WEBCORE_DIR}/platform/graphics/freetype"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/texmap/coordinated"
    "${WEBCORE_DIR}/platform/network/soup"
    "${WEBKIT_DIR}/NetworkProcess/CustomProtocols/soup"
    "${WEBKIT_DIR}/NetworkProcess/Downloads/soup"
    "${WEBKIT_DIR}/NetworkProcess/soup"
    "${WEBKIT_DIR}/NetworkProcess/unix"
    "${WEBKIT_DIR}/Platform/IPC/glib"
    "${WEBKIT_DIR}/Platform/IPC/unix"
    "${WEBKIT_DIR}/Platform/classifier"
    "${WEBKIT_DIR}/Shared/API/c/wpe"
    "${WEBKIT_DIR}/Shared/API/glib"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics/threadedcompositor"
    "${WEBKIT_DIR}/Shared/glib"
    "${WEBKIT_DIR}/Shared/soup"
    "${WEBKIT_DIR}/Shared/unix"
    "${WEBKIT_DIR}/Shared/wpe"
    "${WEBKIT_DIR}/StorageProcess/unix"
    "${WEBKIT_DIR}/UIProcess/API/C/cairo"
    "${WEBKIT_DIR}/UIProcess/API/C/wpe"
    "${WEBKIT_DIR}/UIProcess/API/glib"
    "${WEBKIT_DIR}/UIProcess/API/wpe"
    "${WEBKIT_DIR}/UIProcess/Network/CustomProtocols/soup"
    "${WEBKIT_DIR}/UIProcess/gstreamer"
    "${WEBKIT_DIR}/UIProcess/linux"
    "${WEBKIT_DIR}/UIProcess/soup"
    "${WEBKIT_DIR}/UIProcess/wpe"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe"
    "${WEBKIT_DIR}/WebProcess/soup"
    "${WEBKIT_DIR}/WebProcess/unix"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/soup"
    "${WEBKIT_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT_DIR}/WebProcess/WebPage/wpe"
    "${WTF_DIR}/wtf/gtk/"
    "${WTF_DIR}/wtf/gobject"
    "${WTF_DIR}"
)

list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
    ${FREETYPE2_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${HARFBUZZ_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${WPE_INCLUDE_DIRS}
)

list(APPEND WebKit_LIBRARIES
    WebCore
    ${CAIRO_LIBRARIES}
    ${FREETYPE2_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${HARFBUZZ_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    ${WPE_LIBRARIES}
)

WEBKIT_BUILD_INSPECTOR_GRESOURCES(${DERIVED_SOURCES_WEBINSPECTORUI_DIR})
list(APPEND WPEWebInspectorResources_DERIVED_SOURCES
    ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.c
)

list(APPEND WPEWebInspectorResources_LIBRARIES
    ${GLIB_GIO_LIBRARIES}
)

list(APPEND WPEWebInspectorResources_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

add_library(WPEWebInspectorResources SHARED ${WPEWebInspectorResources_DERIVED_SOURCES})
add_dependencies(WPEWebInspectorResources WebKit)
target_link_libraries(WPEWebInspectorResources ${WPEWebInspectorResources_LIBRARIES})
target_include_directories(WPEWebInspectorResources SYSTEM PUBLIC ${WPEWebInspectorResources_SYSTEM_INCLUDE_DIRECTORIES})
install(TARGETS WPEWebInspectorResources DESTINATION "${LIB_INSTALL_DIR}")

add_library(WPEInjectedBundle MODULE "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitInjectedBundleMain.cpp")
ADD_WEBKIT_PREFIX_HEADER(WPEInjectedBundle)
target_link_libraries(WPEInjectedBundle WebKit)

install(FILES "${CMAKE_BINARY_DIR}/wpe-webkit.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT "Development"
)

install(FILES ${WPE_API_INSTALLED_HEADERS}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-${WPE_API_VERSION}/WPE/wpe"
    COMPONENT "Development"
)
