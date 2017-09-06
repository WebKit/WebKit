file(MAKE_DIRECTORY ${DERIVED_SOURCES_WPE_API_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WPE_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WPE_EXTENSION_DIR})

configure_file(wpe/wpe-webkit.pc.in ${CMAKE_BINARY_DIR}/wpe-webkit.pc @ONLY)

add_definitions(-DWEBKIT2_COMPILATION)

add_definitions(-DLIBEXECDIR="${LIBEXEC_INSTALL_DIR}")
add_definitions(-DLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")

set(WebKit2_USE_PREFIX_HEADER ON)

add_custom_target(webkit2wpe-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${WEBKIT2_DIR} --output ${FORWARDING_HEADERS_DIR} --platform wpe --platform soup
)

 # These symbolic link allows includes like #include <wpe/WebkitWebView.h> which simulates installed headers.
add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WPE_DIR}/wpe
    DEPENDS ${WEBKIT2_DIR}/UIProcess/API/wpe
    COMMAND ln -n -s -f ${WEBKIT2_DIR}/UIProcess/API/wpe ${FORWARDING_HEADERS_WPE_DIR}/wpe
)

add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WPE_EXTENSION_DIR}/wpe
    DEPENDS ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/wpe
    COMMAND ln -n -s -f ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/wpe ${FORWARDING_HEADERS_WPE_EXTENSION_DIR}/wpe
)

add_custom_target(webkit2wpe-fake-api-headers
    DEPENDS ${FORWARDING_HEADERS_WPE_DIR}/wpe
            ${FORWARDING_HEADERS_WPE_EXTENSION_DIR}/wpe
)

set(WEBKIT2_EXTRA_DEPENDENCIES
    webkit2wpe-fake-api-headers
    webkit2wpe-forwarding-headers
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

list(APPEND WebKit2_SOURCES
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
    Shared/CoordinatedGraphics/threadedcompositor/ThreadSafeCoordinatedSurface.cpp
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
    UIProcess/API/glib/WebKitIconLoadingClient.cpp
    UIProcess/API/glib/WebKitInjectedBundleClient.cpp
    UIProcess/API/glib/WebKitInstallMissingMediaPluginsPermissionRequest.cpp
    UIProcess/API/glib/WebKitJavascriptResult.cpp
    UIProcess/API/glib/WebKitLoaderClient.cpp
    UIProcess/API/glib/WebKitMimeInfo.cpp
    UIProcess/API/glib/WebKitNavigationAction.cpp
    UIProcess/API/glib/WebKitNavigationPolicyDecision.cpp
    UIProcess/API/glib/WebKitNetworkProxySettings.cpp
    UIProcess/API/glib/WebKitNotification.cpp
    UIProcess/API/glib/WebKitNotificationPermissionRequest.cpp
    UIProcess/API/glib/WebKitNotificationProvider.cpp
    UIProcess/API/glib/WebKitPermissionRequest.cpp
    UIProcess/API/glib/WebKitPlugin.cpp
    UIProcess/API/glib/WebKitPolicyClient.cpp
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
    UIProcess/API/wpe/WebKitWebViewWPE.cpp
    UIProcess/API/wpe/WPEView.cpp

    UIProcess/Automation/cairo/WebAutomationSessionCairo.cpp

    UIProcess/Launcher/wpe/ProcessLauncherWPE.cpp

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

    WebProcess/WebPage/CoordinatedGraphics/AreaAllocator.cpp
    WebProcess/WebPage/CoordinatedGraphics/CompositingCoordinator.cpp
    WebProcess/WebPage/CoordinatedGraphics/CoordinatedLayerTreeHost.cpp
    WebProcess/WebPage/CoordinatedGraphics/ThreadedCoordinatedLayerTreeHost.cpp
    WebProcess/WebPage/CoordinatedGraphics/UpdateAtlas.cpp

    WebProcess/WebPage/gstreamer/WebPageGStreamer.cpp

    WebProcess/WebPage/wpe/AcceleratedSurfaceWPE.cpp
    WebProcess/WebPage/wpe/CompositingManager.cpp
    WebProcess/WebPage/wpe/WebInspectorUIWPE.cpp
    WebProcess/WebPage/wpe/WebPageWPE.cpp

    WebProcess/soup/WebKitSoupRequestInputStream.cpp
    WebProcess/soup/WebProcessSoup.cpp

    WebProcess/wpe/WebProcessMainWPE.cpp
)

list(APPEND WebKit2_MESSAGES_IN_FILES
    UIProcess/API/wpe/CompositingManagerProxy.messages.in
)

list(APPEND WebKit2_DERIVED_SOURCES
    ${DERIVED_SOURCES_WEBKIT2_DIR}/WebKit2ResourcesGResourceBundle.c

    ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.cpp
)

set(WPE_API_INSTALLED_HEADERS
    ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitApplicationInfo.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitAuthenticationRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitAutomationSession.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitBackForwardList.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitBackForwardListItem.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitCredential.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitContextMenu.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitContextMenuActions.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitContextMenuItem.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitCookieManager.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitDefines.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitDownload.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitEditingCommands.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitEditorState.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitError.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitFaviconDatabase.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitFindController.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitFormSubmissionRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitGeolocationPermissionRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitHitTestResult.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitInstallMissingMediaPluginsPermissionRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitJavascriptResult.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitMimeInfo.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitNavigationAction.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitNavigationPolicyDecision.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitNetworkProxySettings.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitNotificationPermissionRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitNotification.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitPermissionRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitPlugin.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitPolicyDecision.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitResponsePolicyDecision.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitSecurityManager.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitSecurityOrigin.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitSettings.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitURIRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitURIResponse.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitURISchemeRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitUserContent.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitUserContentManager.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitUserMediaPermissionRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitWebContext.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitWebResource.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitWebView.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitWebViewSessionState.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitWebsiteData.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitWebsiteDataManager.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitWindowProperties.h
    ${WEBKIT2_DIR}/UIProcess/API/wpe/webkit.h
)

# To generate WebKitEnumTypes.h we want to use all installed headers, except WebKitEnumTypes.h itself.
set(WPE_ENUM_GENERATION_HEADERS ${WPE_API_INSTALLED_HEADERS})
list(REMOVE_ITEM WPE_ENUM_GENERATION_HEADERS ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h
           ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.cpp
    DEPENDS ${WPE_ENUM_GENERATION_HEADERS}

    COMMAND glib-mkenums --template ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitEnumTypes.h.template ${WPE_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ | sed s/WEBKIT_TYPE_KIT/WEBKIT_TYPE/ > ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h

    COMMAND glib-mkenums --template ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKitEnumTypes.cpp.template ${WPE_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ > ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.cpp
    VERBATIM
)

set(WebKit2Resources
)

if (ENABLE_WEB_AUDIO)
    list(APPEND WebKit2Resources
        "        <file alias=\"audio/Composite\">Composite.wav</file>\n"
    )
endif ()

file(WRITE ${DERIVED_SOURCES_WEBKIT2_DIR}/WebKit2ResourcesGResourceBundle.xml
    "<?xml version=1.0 encoding=UTF-8?>\n"
    "<gresources>\n"
    "    <gresource prefix=\"/org/webkitwpe/resources\">\n"
    ${WebKit2Resources}
    "    </gresource>\n"
    "</gresources>\n"
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT2_DIR}/WebKit2ResourcesGResourceBundle.c
    DEPENDS ${DERIVED_SOURCES_WEBKIT2_DIR}/WebKit2ResourcesGResourceBundle.xml
    COMMAND glib-compile-resources --generate --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/Resources --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/platform/audio/resources --target=${DERIVED_SOURCES_WEBKIT2_DIR}/WebKit2ResourcesGResourceBundle.c ${DERIVED_SOURCES_WEBKIT2_DIR}/WebKit2ResourcesGResourceBundle.xml
    VERBATIM
)

list(APPEND WebKit2_INCLUDE_DIRECTORIES
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
    "${WEBKIT2_DIR}/NetworkProcess/CustomProtocols/soup"
    "${WEBKIT2_DIR}/NetworkProcess/Downloads/soup"
    "${WEBKIT2_DIR}/NetworkProcess/soup"
    "${WEBKIT2_DIR}/NetworkProcess/unix"
    "${WEBKIT2_DIR}/Platform/IPC/glib"
    "${WEBKIT2_DIR}/Platform/IPC/unix"
    "${WEBKIT2_DIR}/Platform/classifier"
    "${WEBKIT2_DIR}/Shared/API/c/wpe"
    "${WEBKIT2_DIR}/Shared/API/glib"
    "${WEBKIT2_DIR}/Shared/CoordinatedGraphics"
    "${WEBKIT2_DIR}/Shared/CoordinatedGraphics/threadedcompositor"
    "${WEBKIT2_DIR}/Shared/glib"
    "${WEBKIT2_DIR}/Shared/soup"
    "${WEBKIT2_DIR}/Shared/unix"
    "${WEBKIT2_DIR}/Shared/wpe"
    "${WEBKIT2_DIR}/StorageProcess/unix"
    "${WEBKIT2_DIR}/UIProcess/API/C/cairo"
    "${WEBKIT2_DIR}/UIProcess/API/C/wpe"
    "${WEBKIT2_DIR}/UIProcess/API/glib"
    "${WEBKIT2_DIR}/UIProcess/API/wpe"
    "${WEBKIT2_DIR}/UIProcess/Network/CustomProtocols/soup"
    "${WEBKIT2_DIR}/UIProcess/gstreamer"
    "${WEBKIT2_DIR}/UIProcess/linux"
    "${WEBKIT2_DIR}/UIProcess/soup"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/glib"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/wpe"
    "${WEBKIT2_DIR}/WebProcess/soup"
    "${WEBKIT2_DIR}/WebProcess/unix"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport/soup"
    "${WEBKIT2_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT2_DIR}/WebProcess/WebPage/wpe"
    "${WTF_DIR}/wtf/gtk/"
    "${WTF_DIR}/wtf/gobject"
    "${WTF_DIR}"
    ${CAIRO_INCLUDE_DIRS}
    ${FREETYPE2_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${HARFBUZZ_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${WPE_INCLUDE_DIRS}
)

list(APPEND WebKit2_LIBRARIES
    WebCorePlatformWPE
    ${CAIRO_LIBRARIES}
    ${FREETYPE2_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${HARFBUZZ_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    ${WPE_LIBRARIES}
)

set(InspectorFiles
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/Localizations/en.lproj/localizedStrings.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/*.html
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Base/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Controllers/*.css
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Controllers/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Debug/*.css
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Debug/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/External/CodeMirror/*.css
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/External/CodeMirror/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/External/ESLint/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/External/Esprima/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Images/gtk/*.png
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Images/gtk/*.svg
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Models/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Protocol/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Proxies/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Test/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Views/*.css
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Views/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Workers/Formatter/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Workers/HeapSnapshot/*.js
)

file(GLOB InspectorFilesDependencies
    ${InspectorFiles}
)

# DerivedSources/JavaScriptCore/inspector/InspectorBackendCommands.js is
# expected in DerivedSources/WebInspectorUI/UserInterface/Protocol/.
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
    DEPENDS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendCommands.js
    COMMAND cp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendCommands.js ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.xml
    DEPENDS ${InspectorFilesDependencies}
            ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
            ${TOOLS_DIR}/wpe/generate-inspector-gresource-manifest.py
    COMMAND ${TOOLS_DIR}/wpe/generate-inspector-gresource-manifest.py --output=${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.xml ${InspectorFiles} ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
    VERBATIM
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.c
    DEPENDS ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.xml
    COMMAND glib-compile-resources --generate --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebInspectorUI --sourcedir=${DERIVED_SOURCES_WEBINSPECTORUI_DIR} --target=${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.c ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.xml
    VERBATIM
)

list(APPEND WPEWebInspectorResources_DERIVED_SOURCES
    ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.c
)

list(APPEND WPEWebInspectorResources_LIBRARIES
    ${GLIB_GIO_LIBRARIES}
)

list(APPEND WPEWebInspectorResources_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

add_library(WPEWebInspectorResources SHARED ${WPEWebInspectorResources_DERIVED_SOURCES})
add_dependencies(WPEWebInspectorResources WebKit2)
target_link_libraries(WPEWebInspectorResources ${WPEWebInspectorResources_LIBRARIES})
target_include_directories(WPEWebInspectorResources PUBLIC ${WPEWebInspectorResources_INCLUDE_DIRECTORIES})
install(TARGETS WPEWebInspectorResources DESTINATION "${LIB_INSTALL_DIR}")

add_library(WPEInjectedBundle MODULE "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/glib/WebKitInjectedBundleMain.cpp")
add_webkit2_prefix_header(WPEInjectedBundle)
target_link_libraries(WPEInjectedBundle WebKit2)

if (EXPORT_DEPRECATED_WEBKIT2_C_API)
    set(WPE_INSTALLED_WEBKIT_HEADERS
        ${WEBKIT2_DIR}/Shared/API/c/WKArray.h
        ${WEBKIT2_DIR}/Shared/API/c/WKBase.h
        ${WEBKIT2_DIR}/Shared/API/c/WKData.h
        ${WEBKIT2_DIR}/Shared/API/c/WKDeclarationSpecifiers.h
        ${WEBKIT2_DIR}/Shared/API/c/WKDiagnosticLoggingResultType.h
        ${WEBKIT2_DIR}/Shared/API/c/WKDictionary.h
        ${WEBKIT2_DIR}/Shared/API/c/WKErrorRef.h
        ${WEBKIT2_DIR}/Shared/API/c/WKEvent.h
        ${WEBKIT2_DIR}/Shared/API/c/WKFindOptions.h
        ${WEBKIT2_DIR}/Shared/API/c/WKGeometry.h
        ${WEBKIT2_DIR}/Shared/API/c/WKImage.h
        ${WEBKIT2_DIR}/Shared/API/c/WKMutableArray.h
        ${WEBKIT2_DIR}/Shared/API/c/WKMutableDictionary.h
        ${WEBKIT2_DIR}/Shared/API/c/WKNumber.h
        ${WEBKIT2_DIR}/Shared/API/c/WKPageLoadTypes.h
        ${WEBKIT2_DIR}/Shared/API/c/WKPageVisibilityTypes.h
        ${WEBKIT2_DIR}/Shared/API/c/WKSecurityOriginRef.h
        ${WEBKIT2_DIR}/Shared/API/c/WKSerializedScriptValue.h
        ${WEBKIT2_DIR}/Shared/API/c/WKString.h
        ${WEBKIT2_DIR}/Shared/API/c/WKType.h
        ${WEBKIT2_DIR}/Shared/API/c/WKURL.h
        ${WEBKIT2_DIR}/Shared/API/c/WKURLRequest.h
        ${WEBKIT2_DIR}/Shared/API/c/WKURLResponse.h
        ${WEBKIT2_DIR}/Shared/API/c/WKUserContentInjectedFrames.h
        ${WEBKIT2_DIR}/Shared/API/c/WKUserContentURLPattern.h
        ${WEBKIT2_DIR}/Shared/API/c/WKUserScriptInjectionTime.h

        ${WEBKIT2_DIR}/Shared/API/c/wpe/WKBaseWPE.h

        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundle.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleBackForwardList.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleBackForwardListItem.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleDOMWindowExtension.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleFileHandleRef.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleFrame.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleHitTestResult.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleInitialize.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleInspector.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleNavigationAction.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleNodeHandle.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePage.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageBanner.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageContextMenuClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageEditorClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageFormClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageFullScreenClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageGroup.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageLoaderClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageOverlay.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePagePolicyClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageResourceLoadClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageUIClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleRangeHandle.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleScriptWorld.h

        ${WEBKIT2_DIR}/UIProcess/API/C/WKBackForwardListItemRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKBackForwardListRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKContextConfigurationRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKContextConnectionClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKContextDownloadClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKContextHistoryClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKContextInjectedBundleClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKContext.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKCookieManager.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKCredential.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKCredentialTypes.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKFrame.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKFrameInfoRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKFramePolicyListener.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKHitTestResult.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKNativeEvent.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKNavigationActionRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKNavigationDataRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKNavigationRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKNavigationResponseRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPage.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageConfigurationRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageContextMenuClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageDiagnosticLoggingClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageFindClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageFindMatchesClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageFormClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageGroup.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageInjectedBundleClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageLoaderClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageNavigationClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPagePolicyClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageRenderingProgressEvents.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageUIClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPluginLoadPolicy.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPreferencesRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKSessionStateRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKUserContentControllerRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKUserScriptRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKViewportAttributes.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKWindowFeaturesRef.h

        ${WEBKIT2_DIR}/UIProcess/API/C/wpe/WKView.h
    )

    install(FILES ${WPE_INSTALLED_WEBKIT_HEADERS}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-${WPE_API_VERSION}/WPE/WebKit"
        COMPONENT "Development"
    )

    set(WPE_INSTALLED_HEADERS
        ${WEBKIT2_DIR}/Shared/API/c/wpe/WebKit.h
    )

    install(FILES ${WPE_INSTALLED_HEADERS}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-${WPE_API_VERSION}/WPE"
        COMPONENT "Development"
    )

    install(FILES ${CMAKE_BINARY_DIR}/wpe-webkit.pc
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
        COMPONENT "Development"
    )
endif ()
