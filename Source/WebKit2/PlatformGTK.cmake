file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT2_DIR})
file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WEBKIT2GTK_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR})

configure_file(UIProcess/API/gtk/WebKitVersion.h.in ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitVersion.h)
configure_file(webkit2gtk.pc.in ${WebKit2_PKGCONFIG_FILE} @ONLY)
configure_file(webkit2gtk-web-extension.pc.in ${WebKit2WebExtension_PKGCONFIG_FILE} @ONLY)

add_definitions(-DWEBKIT2_COMPILATION)
add_definitions(-DLIBEXECDIR="${CMAKE_INSTALL_FULL_LIBEXECDIR}")
add_definitions(-DPACKAGE_LOCALE_DIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")
add_definitions(-DLIBDIR="${CMAKE_INSTALL_FULL_LIBDIR}")

set(WebKit2_USE_PREFIX_HEADER ON)

list(APPEND WebKit2_SOURCES
    ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKit2InspectorGResourceBundle.c
    ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/InspectorGResourceBundle.c

    ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.cpp
    ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitMarshal.cpp

    NetworkProcess/soup/NetworkProcessSoup.cpp
    NetworkProcess/soup/NetworkResourceLoadSchedulerSoup.cpp
    NetworkProcess/soup/RemoteNetworkingContextSoup.cpp

    NetworkProcess/unix/NetworkProcessMainUnix.cpp

    Platform/IPC/unix/AttachmentUnix.cpp
    Platform/IPC/unix/ConnectionUnix.cpp

    Platform/gtk/LoggingGtk.cpp
    Platform/gtk/ModuleGtk.cpp
    Platform/gtk/WorkQueueGtk.cpp

    Platform/unix/SharedMemoryUnix.cpp

    PluginProcess/unix/PluginControllerProxyUnix.cpp
    PluginProcess/unix/PluginProcessMainUnix.cpp
    PluginProcess/unix/PluginProcessUnix.cpp

    Shared/API/c/cairo/WKImageCairo.cpp

    Shared/Downloads/gtk/DownloadSoupErrorsGtk.cpp

    Shared/Downloads/soup/DownloadSoup.cpp

    Shared/Network/CustomProtocols/soup/CustomProtocolManagerImpl.cpp
    Shared/Network/CustomProtocols/soup/CustomProtocolManagerSoup.cpp

    Shared/Plugins/Netscape/x11/NetscapePluginModuleX11.cpp

    Shared/cairo/ShareableBitmapCairo.cpp

    Shared/gtk/ArgumentCodersGtk.cpp
    Shared/gtk/LayerTreeContextGtk.cpp
    Shared/gtk/NativeWebKeyboardEventGtk.cpp
    Shared/gtk/NativeWebMouseEventGtk.cpp
    Shared/gtk/NativeWebTouchEventGtk.cpp
    Shared/gtk/NativeWebWheelEventGtk.cpp
    Shared/gtk/PrintInfoGtk.cpp
    Shared/gtk/ProcessExecutablePathGtk.cpp
    Shared/gtk/WebEventFactory.cpp

    Shared/linux/WebMemorySamplerLinux.cpp

    Shared/linux/SeccompFilters/OpenSyscall.cpp
    Shared/linux/SeccompFilters/SeccompBroker.cpp
    Shared/linux/SeccompFilters/SeccompFilters.cpp
    Shared/linux/SeccompFilters/SigactionSyscall.cpp
    Shared/linux/SeccompFilters/SigprocmaskSyscall.cpp
    Shared/linux/SeccompFilters/Syscall.cpp
    Shared/linux/SeccompFilters/SyscallPolicy.cpp

    Shared/soup/WebCoreArgumentCodersSoup.cpp

    UIProcess/DefaultUndoController.cpp
    UIProcess/DrawingAreaProxyImpl.cpp

    UIProcess/API/C/cairo/WKIconDatabaseCairo.cpp

    UIProcess/API/C/gtk/WKFullScreenClientGtk.cpp
    UIProcess/API/C/gtk/WKInspectorClientGtk.cpp
    UIProcess/API/C/gtk/WKView.cpp

    UIProcess/API/C/soup/WKCookieManagerSoup.cpp
    UIProcess/API/C/soup/WKSoupCustomProtocolRequestManager.cpp

    UIProcess/API/gtk/PageClientImpl.cpp
    UIProcess/API/gtk/PageClientImpl.h
    UIProcess/API/gtk/WebKitAuthenticationDialog.cpp
    UIProcess/API/gtk/WebKitAuthenticationDialog.h
    UIProcess/API/gtk/WebKitAuthenticationRequest.cpp
    UIProcess/API/gtk/WebKitAuthenticationRequest.h
    UIProcess/API/gtk/WebKitBackForwardList.cpp
    UIProcess/API/gtk/WebKitBackForwardList.h
    UIProcess/API/gtk/WebKitBackForwardListItem.cpp
    UIProcess/API/gtk/WebKitBackForwardListItem.h
    UIProcess/API/gtk/WebKitBackForwardListPrivate.h
    UIProcess/API/gtk/WebKitCertificateInfo.cpp
    UIProcess/API/gtk/WebKitCertificateInfo.h
    UIProcess/API/gtk/WebKitCertificateInfoPrivate.h
    UIProcess/API/gtk/WebKitContextMenu.cpp
    UIProcess/API/gtk/WebKitContextMenu.h
    UIProcess/API/gtk/WebKitContextMenuActions.cpp
    UIProcess/API/gtk/WebKitContextMenuActions.h
    UIProcess/API/gtk/WebKitContextMenuActionsPrivate.h
    UIProcess/API/gtk/WebKitContextMenuClient.cpp
    UIProcess/API/gtk/WebKitContextMenuClient.h
    UIProcess/API/gtk/WebKitContextMenuItem.cpp
    UIProcess/API/gtk/WebKitContextMenuItem.h
    UIProcess/API/gtk/WebKitContextMenuItemPrivate.h
    UIProcess/API/gtk/WebKitContextMenuPrivate.h
    UIProcess/API/gtk/WebKitCookieManager.cpp
    UIProcess/API/gtk/WebKitCookieManager.h
    UIProcess/API/gtk/WebKitCookieManagerPrivate.h
    UIProcess/API/gtk/WebKitCredential.cpp
    UIProcess/API/gtk/WebKitCredential.h
    UIProcess/API/gtk/WebKitDefines.h
    UIProcess/API/gtk/WebKitDownload.cpp
    UIProcess/API/gtk/WebKitDownload.h
    UIProcess/API/gtk/WebKitDownloadClient.cpp
    UIProcess/API/gtk/WebKitDownloadClient.h
    UIProcess/API/gtk/WebKitDownloadPrivate.h
    UIProcess/API/gtk/WebKitEditingCommands.h
    UIProcess/API/gtk/WebKitError.cpp
    UIProcess/API/gtk/WebKitError.h
    UIProcess/API/gtk/WebKitFaviconDatabase.cpp
    UIProcess/API/gtk/WebKitFaviconDatabase.h
    UIProcess/API/gtk/WebKitFaviconDatabasePrivate.h
    UIProcess/API/gtk/WebKitFileChooserRequest.cpp
    UIProcess/API/gtk/WebKitFileChooserRequest.h
    UIProcess/API/gtk/WebKitFileChooserRequestPrivate.h
    UIProcess/API/gtk/WebKitFindController.cpp
    UIProcess/API/gtk/WebKitFindController.h
    UIProcess/API/gtk/WebKitFormClient.cpp
    UIProcess/API/gtk/WebKitFormClient.h
    UIProcess/API/gtk/WebKitFormSubmissionRequest.cpp
    UIProcess/API/gtk/WebKitFormSubmissionRequest.h
    UIProcess/API/gtk/WebKitFormSubmissionRequestPrivate.h
    UIProcess/API/gtk/WebKitForwardDeclarations.h
    UIProcess/API/gtk/WebKitFullscreenClient.cpp
    UIProcess/API/gtk/WebKitFullscreenClient.h
    UIProcess/API/gtk/WebKitGeolocationPermissionRequest.cpp
    UIProcess/API/gtk/WebKitGeolocationPermissionRequest.h
    UIProcess/API/gtk/WebKitGeolocationPermissionRequestPrivate.h
    UIProcess/API/gtk/WebKitGeolocationProvider.cpp
    UIProcess/API/gtk/WebKitGeolocationProvider.h
    UIProcess/API/gtk/WebKitHitTestResult.cpp
    UIProcess/API/gtk/WebKitHitTestResult.h
    UIProcess/API/gtk/WebKitHitTestResultPrivate.h
    UIProcess/API/gtk/WebKitInjectedBundleClient.cpp
    UIProcess/API/gtk/WebKitInjectedBundleClient.h
    UIProcess/API/gtk/WebKitJavascriptResult.cpp
    UIProcess/API/gtk/WebKitJavascriptResult.h
    UIProcess/API/gtk/WebKitJavascriptResultPrivate.h
    UIProcess/API/gtk/WebKitLoaderClient.cpp
    UIProcess/API/gtk/WebKitLoaderClient.h
    UIProcess/API/gtk/WebKitMimeInfo.cpp
    UIProcess/API/gtk/WebKitMimeInfo.h
    UIProcess/API/gtk/WebKitMimeInfoPrivate.h
    UIProcess/API/gtk/WebKitNavigationPolicyDecision.cpp
    UIProcess/API/gtk/WebKitNavigationPolicyDecision.h
    UIProcess/API/gtk/WebKitNavigationPolicyDecisionPrivate.h
    UIProcess/API/gtk/WebKitPermissionRequest.cpp
    UIProcess/API/gtk/WebKitPermissionRequest.h
    UIProcess/API/gtk/WebKitPlugin.cpp
    UIProcess/API/gtk/WebKitPlugin.h
    UIProcess/API/gtk/WebKitPluginPrivate.h
    UIProcess/API/gtk/WebKitPolicyClient.cpp
    UIProcess/API/gtk/WebKitPolicyClient.h
    UIProcess/API/gtk/WebKitPolicyDecision.cpp
    UIProcess/API/gtk/WebKitPolicyDecision.h
    UIProcess/API/gtk/WebKitPolicyDecisionPrivate.h
    UIProcess/API/gtk/WebKitPrintOperation.cpp
    UIProcess/API/gtk/WebKitPrintOperation.h
    UIProcess/API/gtk/WebKitPrintOperationPrivate.h
    UIProcess/API/gtk/WebKitPrivate.cpp
    UIProcess/API/gtk/WebKitPrivate.h
    UIProcess/API/gtk/WebKitRequestManagerClient.cpp
    UIProcess/API/gtk/WebKitRequestManagerClient.h
    UIProcess/API/gtk/WebKitResponsePolicyDecision.cpp
    UIProcess/API/gtk/WebKitResponsePolicyDecision.h
    UIProcess/API/gtk/WebKitResponsePolicyDecisionPrivate.h
    UIProcess/API/gtk/WebKitScriptDialog.cpp
    UIProcess/API/gtk/WebKitScriptDialog.h
    UIProcess/API/gtk/WebKitScriptDialogPrivate.h
    UIProcess/API/gtk/WebKitSecurityManager.cpp
    UIProcess/API/gtk/WebKitSecurityManager.h
    UIProcess/API/gtk/WebKitSecurityManagerPrivate.h
    UIProcess/API/gtk/WebKitSettings.cpp
    UIProcess/API/gtk/WebKitSettings.h
    UIProcess/API/gtk/WebKitSettingsPrivate.h
    UIProcess/API/gtk/WebKitTextChecker.cpp
    UIProcess/API/gtk/WebKitTextChecker.h
    UIProcess/API/gtk/WebKitUIClient.cpp
    UIProcess/API/gtk/WebKitUIClient.h
    UIProcess/API/gtk/WebKitURIRequest.cpp
    UIProcess/API/gtk/WebKitURIRequest.h
    UIProcess/API/gtk/WebKitURIRequestPrivate.h
    UIProcess/API/gtk/WebKitURIResponse.cpp
    UIProcess/API/gtk/WebKitURIResponse.h
    UIProcess/API/gtk/WebKitURIResponsePrivate.h
    UIProcess/API/gtk/WebKitURISchemeRequest.cpp
    UIProcess/API/gtk/WebKitURISchemeRequest.h
    UIProcess/API/gtk/WebKitURISchemeRequestPrivate.h
    UIProcess/API/gtk/WebKitVersion.cpp
    UIProcess/API/gtk/WebKitVersion.h.in
    UIProcess/API/gtk/WebKitWebContext.cpp
    UIProcess/API/gtk/WebKitWebContext.h
    UIProcess/API/gtk/WebKitWebContextPrivate.h
    UIProcess/API/gtk/WebKitWebInspector.cpp
    UIProcess/API/gtk/WebKitWebInspector.h
    UIProcess/API/gtk/WebKitWebInspectorPrivate.h
    UIProcess/API/gtk/WebKitWebResource.cpp
    UIProcess/API/gtk/WebKitWebResource.h
    UIProcess/API/gtk/WebKitWebResourcePrivate.h
    UIProcess/API/gtk/WebKitWebView.cpp
    UIProcess/API/gtk/WebKitWebView.h
    UIProcess/API/gtk/WebKitWebViewBase.cpp
    UIProcess/API/gtk/WebKitWebViewBase.h
    UIProcess/API/gtk/WebKitWebViewBaseAccessible.cpp
    UIProcess/API/gtk/WebKitWebViewBaseAccessible.h
    UIProcess/API/gtk/WebKitWebViewBasePrivate.h
    UIProcess/API/gtk/WebKitWebViewGroup.cpp
    UIProcess/API/gtk/WebKitWebViewGroup.h
    UIProcess/API/gtk/WebKitWebViewGroupPrivate.h
    UIProcess/API/gtk/WebKitWebViewPrivate.h
    UIProcess/API/gtk/WebKitWindowProperties.cpp
    UIProcess/API/gtk/WebKitWindowProperties.h
    UIProcess/API/gtk/WebKitWindowPropertiesPrivate.h
    UIProcess/API/gtk/WebViewBaseInputMethodFilter.cpp
    UIProcess/API/gtk/WebViewBaseInputMethodFilter.h
    UIProcess/API/gtk/webkit2.h

    UIProcess/InspectorServer/gtk/WebInspectorServerGtk.cpp

    UIProcess/InspectorServer/soup/WebSocketServerSoup.cpp

    UIProcess/Launcher/gtk/ProcessLauncherGtk.cpp

    UIProcess/Network/CustomProtocols/soup/CustomProtocolManagerProxySoup.cpp
    UIProcess/Network/CustomProtocols/soup/WebSoupCustomProtocolRequestManagerClient.cpp
    UIProcess/Network/CustomProtocols/soup/WebSoupCustomProtocolRequestManager.cpp

    UIProcess/Plugins/gtk/PluginInfoCache.cpp

    UIProcess/Plugins/unix/PluginInfoStoreUnix.cpp
    UIProcess/Plugins/unix/PluginProcessProxyUnix.cpp

    UIProcess/Storage/StorageManager.cpp

    UIProcess/cairo/BackingStoreCairo.cpp

    UIProcess/gtk/ExperimentalFeatures.cpp
    UIProcess/gtk/TextCheckerGtk.cpp
    UIProcess/gtk/WebContextGtk.cpp
    UIProcess/gtk/WebContextMenuProxyGtk.cpp
    UIProcess/gtk/WebFullScreenClientGtk.cpp
    UIProcess/gtk/WebInspectorClientGtk.cpp
    UIProcess/gtk/WebInspectorProxyGtk.cpp
    UIProcess/gtk/WebPageProxyGtk.cpp
    UIProcess/gtk/WebPopupMenuProxyGtk.cpp
    UIProcess/gtk/WebPreferencesGtk.cpp
    UIProcess/gtk/WebProcessProxyGtk.cpp

    UIProcess/Network/soup/NetworkProcessProxySoup.cpp
    UIProcess/soup/WebContextSoup.cpp
    UIProcess/soup/WebCookieManagerProxySoup.cpp

    WebProcess/Cookies/soup/WebCookieManagerSoup.cpp
    WebProcess/Cookies/soup/WebKitSoupCookieJarSqlite.cpp

    WebProcess/InjectedBundle/API/gtk/WebKitFrame.cpp
    WebProcess/InjectedBundle/API/gtk/WebKitScriptWorld.cpp
    WebProcess/InjectedBundle/API/gtk/WebKitWebExtension.cpp
    WebProcess/InjectedBundle/API/gtk/WebKitWebPage.cpp

    WebProcess/InjectedBundle/gtk/InjectedBundleGtk.cpp

    WebProcess/Plugins/Netscape/unix/PluginProxyUnix.cpp

    WebProcess/Plugins/Netscape/x11/NetscapePluginX11.cpp

    WebProcess/WebCoreSupport/gtk/WebContextMenuClientGtk.cpp
    WebProcess/WebCoreSupport/gtk/WebDragClientGtk.cpp
    WebProcess/WebCoreSupport/gtk/WebEditorClientGtk.cpp
    WebProcess/WebCoreSupport/gtk/WebErrorsGtk.cpp
    WebProcess/WebCoreSupport/gtk/WebPopupMenuGtk.cpp

    WebProcess/WebCoreSupport/soup/WebFrameNetworkingContext.cpp

    WebProcess/WebPage/DrawingAreaImpl.cpp

    WebProcess/WebPage/atk/WebPageAccessibilityObjectAtk.cpp

    WebProcess/WebPage/gtk/LayerTreeHostGtk.cpp
    WebProcess/WebPage/gtk/PrinterListGtk.cpp
    WebProcess/WebPage/gtk/WebInspectorGtk.cpp
    WebProcess/WebPage/gtk/WebPageGtk.cpp
    WebProcess/WebPage/gtk/WebPrintOperationGtk.cpp

    WebProcess/gtk/WebGtkExtensionManager.cpp
    WebProcess/gtk/WebGtkInjectedBundleMain.cpp
    WebProcess/gtk/WebProcessMainGtk.cpp

    WebProcess/soup/WebKitSoupRequestGeneric.cpp
    WebProcess/soup/WebKitSoupRequestInputStream.cpp
    WebProcess/soup/WebProcessSoup.cpp
)

set(WebKit2GTK_INSTALLED_HEADERS
    ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.h
    ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitVersion.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitAuthenticationRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitBackForwardList.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitBackForwardListItem.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitCertificateInfo.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitCredential.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitContextMenu.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitContextMenuActions.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitContextMenuItem.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitCookieManager.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitDefines.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitDownload.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitEditingCommands.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitError.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitFaviconDatabase.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitFileChooserRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitFindController.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitFormSubmissionRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitForwardDeclarations.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitGeolocationPermissionRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitHitTestResult.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitJavascriptResult.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitMimeInfo.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitNavigationPolicyDecision.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitPermissionRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitPlugin.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitPolicyDecision.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitPrintOperation.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitResponsePolicyDecision.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitScriptDialog.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitSecurityManager.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitSettings.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitURIRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitURIResponse.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitURISchemeRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWebContext.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWebInspector.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWebResource.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWebView.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWebViewBase.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWebViewGroup.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWindowProperties.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/webkit2.h
)

set(WebKit2WebExtension_INSTALLED_HEADERS
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitFrame.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitScriptWorld.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebExtension.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebPage.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/webkit-web-extension.h
)

file(GLOB InspectorFiles
    ${CMAKE_SOURCE_DIR}/Source/Localizations/en.lproj/localizedStrings.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/*.html
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Base/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Controllers/*.css
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Controllers/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/External/CodeMirror/*
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Models/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Protocol/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Views/*.css
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Views/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Images/*.png
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Images/*.svg
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSBackendCommands.js
    ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorWebBackendCommands.js
)

# This is necessary because of a conflict between the GTK+ API WebKitVersion.h and one generated by WebCore.
list(INSERT WebKit2_INCLUDE_DIRECTORIES 0
    "${FORWARDING_HEADERS_WEBKIT2GTK_DIR}"
    "${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}"
    "${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}"
    "${DERIVED_SOURCES_WEBKIT2GTK_DIR}"
)

list(APPEND WebKit2_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/cairo"
    "${WEBCORE_DIR}/platform/gtk"
    "${WEBCORE_DIR}/platform/graphics/cairo"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/network/soup"
    "${WEBCORE_DIR}/platform/text/enchant"
    "${WEBKIT2_DIR}/Shared/API/c/gtk"
    "${WEBKIT2_DIR}/Shared/Network/CustomProtocols/soup"
    "${WEBKIT2_DIR}/Shared/Downloads/soup"
    "${WEBKIT2_DIR}/Shared/gtk"
    "${WEBKIT2_DIR}/Shared/soup"
    "${WEBKIT2_DIR}/NetworkProcess/unix"
    "${WEBKIT2_DIR}/UIProcess/API/C/cairo"
    "${WEBKIT2_DIR}/UIProcess/API/C/gtk"
    "${WEBKIT2_DIR}/UIProcess/API/C/soup"
    "${WEBKIT2_DIR}/UIProcess/API/cpp/gtk"
    "${WEBKIT2_DIR}/UIProcess/API/gtk"
    "${WEBKIT2_DIR}/UIProcess/Network/CustomProtocols/soup"
    "${WEBKIT2_DIR}/UIProcess/Plugins/gtk"
    "${WEBKIT2_DIR}/UIProcess/gtk"
    "${WEBKIT2_DIR}/UIProcess/soup"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk"
    "${WEBKIT2_DIR}/WebProcess/gtk"
    "${WEBKIT2_DIR}/WebProcess/soup"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport/gtk"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport/soup"
    "${WEBKIT2_DIR}/WebProcess/WebPage/atk"
    "${WEBKIT2_DIR}/WebProcess/WebPage/gtk"
    "${WTF_DIR}/wtf/gtk/"
    "${WTF_DIR}/wtf/gobject"
    ${WTF_DIR}
    ${CAIRO_INCLUDE_DIRS}
    ${ENCHANT_INCLUDE_DIRS}
    ${GEOCLUE_INCLUDE_DIRS}
    ${HARFBUZZ_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

set(WebKit2CommonIncludeDirectories ${WebKit2_INCLUDE_DIRECTORIES})

list(APPEND WebKit2_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${GTK_INCLUDE_DIRS}
    ${GTK_UNIX_PRINT_INCLUDE_DIRS}
)

list(APPEND WebProcess_SOURCES
    gtk/MainGtk.cpp
)

list(APPEND NetworkProcess_SOURCES
    unix/NetworkMainUnix.cpp
)

set(SharedWebKit2Libraries
    ${WebKit2_LIBRARIES}
)

list(APPEND WebKit2_LIBRARIES
    GObjectDOMBindings
    WebCorePlatformGTK
    ${GTK_UNIX_PRINT_LIBRARIES}
)
ADD_WHOLE_ARCHIVE_TO_LIBRARIES(WebKit2_LIBRARIES)

set(WebKit2_MARSHAL_LIST ${WEBKIT2_DIR}/UIProcess/API/gtk/webkit2marshal.list)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitMarshal.cpp
           ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitMarshal.h
    MAIN_DEPENDENCY ${WebKit2_MARSHAL_LIST}

    COMMAND echo extern \"C\" { > ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitMarshal.cpp
    COMMAND glib-genmarshal --prefix=webkit_marshal ${WebKit2_MARSHAL_LIST} --body >> ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitMarshal.cpp
    COMMAND echo } >> ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitMarshal.cpp

    COMMAND glib-genmarshal --prefix=webkit_marshal ${WebKit2_MARSHAL_LIST} --header > ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitMarshal.h
    VERBATIM)

# To generate WebKitEnumTypes.h we want to use all installed headers, except WebKitEnumTypes.h itself.
set(WebKit2GTK_ENUM_GENERATION_HEADERS ${WebKit2GTK_INSTALLED_HEADERS})
list(REMOVE_ITEM WebKit2GTK_ENUM_GENERATION_HEADERS ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.h)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.h
           ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.cpp
    DEPENDS ${WebKit2GTK_ENUM_GENERATION_HEADERS}

    COMMAND glib-mkenums --template ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitEnumTypes.h.template ${WebKit2GTK_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ | sed s/WEBKIT_TYPE_KIT/WEBKIT_TYPE/ > ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.h

    COMMAND glib-mkenums --template ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitEnumTypes.cpp.template ${WebKit2GTK_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ > ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.cpp
    VERBATIM)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/InspectorGResourceBundle.xml
    DEPENDS ${InspectorFiles}
            ${TOOLS_DIR}/gtk/generate-inspector-gresource-manifest.py
    COMMAND ${TOOLS_DIR}/gtk/generate-inspector-gresource-manifest.py ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/InspectorGResourceBundle.xml
    VERBATIM
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/InspectorGResourceBundle.c
    DEPENDS ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/InspectorGResourceBundle.xml
            ${InspectorFiles}
    COMMAND glib-compile-resources --generate --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebInspectorUI --target=${DERIVED_SOURCES_WEBKIT2GTK_DIR}/InspectorGResourceBundle.c ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/InspectorGResourceBundle.xml
    VERBATIM
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKit2InspectorGResourceBundle.c
    DEPENDS ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKit2InspectorGResourceBundle.xml
            ${WEBKIT2_DIR}/UIProcess/InspectorServer/front-end/inspectorPageIndex.html
    COMMAND glib-compile-resources --generate --sourcedir=${WEBKIT2_DIR}/UIProcess/InspectorServer/front-end --target=${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKit2InspectorGResourceBundle.c ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKit2InspectorGResourceBundle.xml
    VERBATIM
)

add_custom_target(webkit2gtk-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT2_DIR} ${FORWARDING_HEADERS_DIR} gtk
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT2_DIR} ${FORWARDING_HEADERS_DIR} soup

    # These symbolic link allows includes like #include <webkit2/WebkitWebView.h> which simulates installed headers.
    COMMAND ln -n -s -f ${WEBKIT2_DIR}/UIProcess/API/gtk ${FORWARDING_HEADERS_WEBKIT2GTK_DIR}/webkit2
    COMMAND ln -n -s -f ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}/webkit2
)

set(WEBKIT2_EXTRA_DEPENDENCIES
     webkit2gtk-forwarding-headers
)

if (ENABLE_PLUGIN_PROCESS)
    set(PluginProcess_EXECUTABLE_NAME WebKitPluginProcess)
    list(APPEND PluginProcess_INCLUDE_DIRECTORIES
        "${WEBKIT2_DIR}/PluginProcess/unix"
    )

    include_directories(${PluginProcess_INCLUDE_DIRECTORIES})

    # FIXME: We should figure out a way to avoid compiling files that are common between the plugin
    # process and WebKit2 only once instead of recompiling them for the plugin process.
    list(APPEND PluginProcess_SOURCES
        Platform/Logging.cpp
        Platform/Module.cpp
        Platform/WorkQueue.cpp

        Platform/IPC/ArgumentCoders.cpp
        Platform/IPC/ArgumentDecoder.cpp
        Platform/IPC/ArgumentEncoder.cpp
        Platform/IPC/Attachment.cpp
        Platform/IPC/Connection.cpp
        Platform/IPC/DataReference.cpp
        Platform/IPC/MessageDecoder.cpp
        Platform/IPC/MessageEncoder.cpp
        Platform/IPC/MessageReceiverMap.cpp
        Platform/IPC/MessageSender.cpp
        Platform/IPC/StringReference.cpp

        Platform/IPC/unix/AttachmentUnix.cpp
        Platform/IPC/unix/ConnectionUnix.cpp

        Platform/gtk/LoggingGtk.cpp
        Platform/gtk/ModuleGtk.cpp
        Platform/gtk/WorkQueueGtk.cpp

        Platform/unix/SharedMemoryUnix.cpp

        PluginProcess/PluginControllerProxy.cpp
        PluginProcess/PluginCreationParameters.cpp
        PluginProcess/PluginProcess.cpp
        PluginProcess/WebProcessConnection.cpp

        PluginProcess/unix/PluginControllerProxyUnix.cpp
        PluginProcess/unix/PluginProcessMainUnix.cpp
        PluginProcess/unix/PluginProcessUnix.cpp

        Shared/ActivityAssertion.cpp
        Shared/ChildProcess.cpp
        Shared/ChildProcessProxy.cpp
        Shared/ConnectionStack.cpp
        Shared/ShareableBitmap.cpp
        Shared/WebCoreArgumentCoders.cpp
        Shared/WebEvent.cpp
        Shared/WebKeyboardEvent.cpp
        Shared/WebKit2Initialize.cpp
        Shared/WebMouseEvent.cpp
        Shared/WebPlatformTouchPoint.cpp
        Shared/WebTouchEvent.cpp
        Shared/WebWheelEvent.cpp

        Shared/Plugins/NPIdentifierData.cpp
        Shared/Plugins/NPObjectMessageReceiver.cpp
        Shared/Plugins/NPObjectProxy.cpp
        Shared/Plugins/NPRemoteObjectMap.cpp
        Shared/Plugins/NPVariantData.cpp
        Shared/Plugins/PluginProcessCreationParameters.cpp

        Shared/Plugins/Netscape/NetscapePluginModule.cpp
        Shared/Plugins/Netscape/NetscapePluginModuleNone.cpp

        Shared/Plugins/Netscape/x11/NetscapePluginModuleX11.cpp

        Shared/cairo/ShareableBitmapCairo.cpp

        Shared/gtk/NativeWebKeyboardEventGtk.cpp
        Shared/gtk/NativeWebMouseEventGtk.cpp
        Shared/gtk/NativeWebTouchEventGtk.cpp
        Shared/gtk/NativeWebWheelEventGtk.cpp
        Shared/gtk/ProcessExecutablePathGtk.cpp
        Shared/gtk/WebEventFactory.cpp

        Shared/soup/WebCoreArgumentCodersSoup.cpp

        UIProcess/Launcher/ProcessLauncher.cpp

        UIProcess/Launcher/gtk/ProcessLauncherGtk.cpp

        UIProcess/Plugins/unix/PluginProcessProxyUnix.cpp

        WebProcess/Plugins/Plugin.cpp

        WebProcess/Plugins/Netscape/NPRuntimeUtilities.cpp
        WebProcess/Plugins/Netscape/NetscapeBrowserFuncs.cpp
        WebProcess/Plugins/Netscape/NetscapePlugin.cpp
        WebProcess/Plugins/Netscape/NetscapePluginNone.cpp
        WebProcess/Plugins/Netscape/NetscapePluginStream.cpp

        WebProcess/Plugins/Netscape/x11/NetscapePluginX11.cpp

        unix/PluginMainUnix.cpp
    )

    list(APPEND PluginProcess_MESSAGES_IN_FILES
        PluginProcess/PluginControllerProxy.messages.in
        PluginProcess/PluginProcess.messages.in
        PluginProcess/WebProcessConnection.messages.in

        Shared/Plugins/NPObjectMessageReceiver.messages.in
    )
    GENERATE_WEBKIT2_MESSAGE_SOURCES(PluginProcess_SOURCES "${PluginProcess_MESSAGES_IN_FILES}")

    add_executable(WebKitPluginProcess ${PluginProcess_SOURCES})
    add_webkit2_prefix_header(WebKitPluginProcess)

    # We need ENABLE_PLUGIN_PROCESS for all targets in this directory, but
    # we only want GTK_API_VERSION_2 for the plugin process target.
    add_definitions(-DENABLE_PLUGIN_PROCESS=1)
    set_property(
        TARGET WebKitPluginProcess
        APPEND
        PROPERTY COMPILE_DEFINITIONS GTK_API_VERSION_2=1
    )
    set_property(
        TARGET WebKitPluginProcess
        APPEND
        PROPERTY INCLUDE_DIRECTORIES
            ${WebKit2CommonIncludeDirectories}
            ${GTK2_INCLUDE_DIRS}
            ${GDK2_INCLUDE_DIRS}
    )

    set(WebKitPluginProcess_LIBRARIES
        ${SharedWebKit2Libraries}
        WebCorePlatformGTK2
    )
    ADD_WHOLE_ARCHIVE_TO_LIBRARIES(WebKitPluginProcess_LIBRARIES)
    target_link_libraries(WebKitPluginProcess ${WebKitPluginProcess_LIBRARIES})

    add_dependencies(WebKitPluginProcess WebKit2)

    install(TARGETS WebKitPluginProcess DESTINATION "${LIBEXEC_INSTALL_DIR}")
endif () # ENABLE_PLUGIN_PROCESS

# Commands for building the built-in injected bundle.
include_directories(
    "${WEBKIT2_DIR}/Platform"
    "${WEBKIT2_DIR}/Shared"
    "${WEBKIT2_DIR}/Shared/API/c"
    "${WEBKIT2_DIR}/UIProcess/API/C"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c"
    "${DERIVED_SOURCES_DIR}"
    "${DERIVED_SOURCES_DIR}/InjectedBundle"
    "${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}"
    "${FORWARDING_HEADERS_DIR}"
    "${FORWARDING_HEADERS_WEBKIT2GTK_DIR}"
)

add_library(webkit2gtkinjectedbundle MODULE "${WEBKIT2_DIR}/WebProcess/gtk/WebGtkInjectedBundleMain.cpp")
add_dependencies(webkit2gtkinjectedbundle GObjectDOMBindings)
add_webkit2_prefix_header(webkit2gtkinjectedbundle)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/WebKit2-${WEBKITGTK_API_VERSION}.gir
    DEPENDS WebKit2
    DEPENDS JavaScriptCore-3-gir
    COMMAND CC=${CMAKE_C_COMPILER} CFLAGS=-Wno-deprecated-declarations
        ${INTROSPECTION_SCANNER}
        --quiet
        --warn-all
        --symbol-prefix=webkit
        --identifier-prefix=WebKit
        --namespace=WebKit2
        --nsversion=${WEBKITGTK_API_VERSION}
        --include=GObject-2.0
        --include=Gtk-${WEBKITGTK_API_VERSION}
        --include=Soup-2.4
        --include-uninstalled=${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir
        --library=webkit2gtk-${WEBKITGTK_API_VERSION}
        --library=javascriptcoregtk-${WEBKITGTK_API_VERSION}
        -L${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        --no-libtool
        --pkg=gobject-2.0
        --pkg=gtk+-${WEBKITGTK_API_VERSION}
        --pkg=libsoup-2.4
        --pkg-export=webkit2gtk-${WEBKITGTK_API_VERSION}
        --output=${CMAKE_BINARY_DIR}/WebKit2-${WEBKITGTK_API_VERSION}.gir
        --c-include="webkit2/webkit2.h"
        -DBUILDING_WEBKIT
        -DWEBKIT2_COMPILATION
        -I${CMAKE_SOURCE_DIR}/Source
        -I${WEBKIT2_DIR}
        -I${JAVASCRIPTCORE_DIR}/ForwardingHeaders
        -I${DERIVED_SOURCES_DIR}
        -I${DERIVED_SOURCES_WEBKIT2GTK_DIR}
        -I${FORWARDING_HEADERS_WEBKIT2GTK_DIR}
        ${WebKit2GTK_INSTALLED_HEADERS}
        ${WEBKIT2_DIR}/UIProcess/API/gtk/*.cpp
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/WebKit2WebExtension-${WEBKITGTK_API_VERSION}.gir
    DEPENDS JavaScriptCore-3-gir
    DEPENDS ${CMAKE_BINARY_DIR}/WebKit2-${WEBKITGTK_API_VERSION}.gir
    COMMAND CC=${CMAKE_C_COMPILER} CFLAGS=-Wno-deprecated-declarations
        ${INTROSPECTION_SCANNER}
        --quiet
        --warn-all
        --symbol-prefix=webkit
        --identifier-prefix=WebKit
        --namespace=WebKit2WebExtension
        --nsversion=${WEBKITGTK_API_VERSION}
        --include=GObject-2.0
        --include=Gtk-${WEBKITGTK_API_VERSION}
        --include=Soup-2.4
        --include-uninstalled=${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir
        --library=webkit2gtk-${WEBKITGTK_API_VERSION}
        --library=javascriptcoregtk-${WEBKITGTK_API_VERSION}
        -L${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        --no-libtool
        --pkg=gobject-2.0
        --pkg=gtk+-${WEBKITGTK_API_VERSION}
        --pkg=libsoup-2.4
        --pkg-export=webkit2gtk-web-extension-${WEBKITGTK_API_VERSION}
        --output=${CMAKE_BINARY_DIR}/WebKit2WebExtension-${WEBKITGTK_API_VERSION}.gir
        --c-include="webkit2/webkit-web-extension.h"
        -DBUILDING_WEBKIT
        -DWEBKIT2_COMPILATION
        -I${CMAKE_SOURCE_DIR}/Source
        -I${WEBKIT2_DIR}
        -I${JAVASCRIPTCORE_DIR}/ForwardingHeaders
        -I${DERIVED_SOURCES_DIR}
        -I${DERIVED_SOURCES_WEBKIT2GTK_DIR}
        -I${FORWARDING_HEADERS_DIR}
        -I${FORWARDING_HEADERS_WEBKIT2GTK_DIR}
        -I${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}
        -I${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk
        ${GObjectDOMBindings_GIR_HEADERS}
        ${WebKit2WebExtension_INSTALLED_HEADERS}
        ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitURIRequest.h
        ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitURIResponse.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/*.cpp
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/WebKit2-${WEBKITGTK_API_VERSION}.typelib
    DEPENDS ${CMAKE_BINARY_DIR}/WebKit2-${WEBKITGTK_API_VERSION}.gir
    COMMAND ${INTROSPECTION_COMPILER} --includedir=${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/WebKit2-${WEBKITGTK_API_VERSION}.gir -o ${CMAKE_BINARY_DIR}/WebKit2-${WEBKITGTK_API_VERSION}.typelib
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/WebKit2WebExtension-${WEBKITGTK_API_VERSION}.typelib
    DEPENDS ${CMAKE_BINARY_DIR}/WebKit2WebExtension-${WEBKITGTK_API_VERSION}.gir
    COMMAND ${INTROSPECTION_COMPILER} --includedir=${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/WebKit2WebExtension-${WEBKITGTK_API_VERSION}.gir -o ${CMAKE_BINARY_DIR}/WebKit2WebExtension-${WEBKITGTK_API_VERSION}.typelib
)

ADD_TYPELIB(${CMAKE_BINARY_DIR}/WebKit2-${WEBKITGTK_API_VERSION}.typelib)
ADD_TYPELIB(${CMAKE_BINARY_DIR}/WebKit2WebExtension-${WEBKITGTK_API_VERSION}.typelib)

install(TARGETS webkit2gtkinjectedbundle
        DESTINATION "${LIB_INSTALL_DIR}/webkit2gtk-${WEBKITGTK_API_VERSION}/injected-bundle"
)
install(FILES "${CMAKE_BINARY_DIR}/Source/WebKit2/webkit2gtk-${WEBKITGTK_API_VERSION}.pc"
              "${CMAKE_BINARY_DIR}/Source/WebKit2/webkit2gtk-web-extension-${WEBKITGTK_API_VERSION}.pc"
        DESTINATION "${LIB_INSTALL_DIR}/pkgconfig"
)
install(FILES ${WebKit2GTK_INSTALLED_HEADERS}
              ${WebKit2WebExtension_INSTALLED_HEADERS}
        DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/webkit2"
)
install(FILES ${CMAKE_BINARY_DIR}/WebKit2-${WEBKITGTK_API_VERSION}.gir
              ${CMAKE_BINARY_DIR}/WebKit2WebExtension-${WEBKITGTK_API_VERSION}.gir
        DESTINATION ${INTROSPECTION_INSTALL_GIRDIR}
)
install(FILES ${CMAKE_BINARY_DIR}/WebKit2-${WEBKITGTK_API_VERSION}.typelib
              ${CMAKE_BINARY_DIR}/WebKit2WebExtension-${WEBKITGTK_API_VERSION}.typelib
        DESTINATION ${INTROSPECTION_INSTALL_TYPELIBDIR}
)

file(WRITE ${CMAKE_BINARY_DIR}/gtkdoc-webkit2gtk.cfg
    "[webkit2gtk]\n"
    "pkgconfig_file=${WebKit2_PKGCONFIG_FILE}\n"
    "namespace=webkit\n"
    "cflags=-I${CMAKE_SOURCE_DIR}/Source\n"
    "       -I${WEBKIT2_DIR}/UIProcess/API/gtk\n"
    "       -I${DERIVED_SOURCES_WEBKIT2GTK_DIR}\n"
    "       -I${FORWARDING_HEADERS_WEBKIT2GTK_DIR}\n"
    "doc_dir=${WEBKIT2_DIR}/UIProcess/API/gtk/docs\n"
    "source_dirs=${WEBKIT2_DIR}/UIProcess/API/gtk\n"
    "            ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk\n"
    "            ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}\n"
    "headers=${WebKit2GTK_ENUM_GENERATION_HEADERS} ${WebKit2WebExtension_INSTALLED_HEADERS}\n"
)
