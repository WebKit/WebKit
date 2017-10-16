set(WebKit2_OUTPUT_NAME webkit2gtk-${WEBKITGTK_API_VERSION})
set(WebKit2_WebProcess_OUTPUT_NAME WebKitWebProcess)
set(WebKit2_NetworkProcess_OUTPUT_NAME WebKitNetworkProcess)
set(WebKit2_PluginProcess_OUTPUT_NAME WebKitPluginProcess)
set(WebKit2_StorageProcess_OUTPUT_NAME WebKitStorageProcess)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WEBKIT2GTK_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR})

configure_file(UIProcess/API/gtk/WebKitVersion.h.in ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitVersion.h)
configure_file(gtk/webkit2gtk.pc.in ${WebKit2_PKGCONFIG_FILE} @ONLY)
configure_file(gtk/webkit2gtk-web-extension.pc.in ${WebKit2WebExtension_PKGCONFIG_FILE} @ONLY)

add_definitions(-DBUILDING_WEBKIT)
add_definitions(-DWEBKIT2_COMPILATION)
add_definitions(-DWEBKIT_DOM_USE_UNSTABLE_API)

add_definitions(-DLIBEXECDIR="${CMAKE_INSTALL_FULL_LIBEXECDIR}")
add_definitions(-DPKGLIBEXECDIR="${LIBEXEC_INSTALL_DIR}")
add_definitions(-DLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")
add_definitions(-DLIBDIR="${LIB_INSTALL_DIR}")
add_definitions(-DDATADIR="${CMAKE_INSTALL_FULL_DATADIR}")

set(WebKit2_USE_PREFIX_HEADER ON)

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
    Shared/CoordinatedGraphics/threadedcompositor/ThreadedDisplayRefreshMonitor.cpp
    Shared/CoordinatedGraphics/threadedcompositor/ThreadedCompositor.cpp

    Shared/Plugins/Netscape/unix/NetscapePluginModuleUnix.cpp

    Shared/Plugins/unix/PluginSearchPath.cpp

    Shared/cairo/ShareableBitmapCairo.cpp

    Shared/glib/WebContextMenuItemGlib.cpp
    Shared/glib/WebErrorsGlib.cpp

    Shared/gtk/ArgumentCodersGtk.cpp
    Shared/gtk/NativeWebKeyboardEventGtk.cpp
    Shared/gtk/NativeWebMouseEventGtk.cpp
    Shared/gtk/NativeWebTouchEventGtk.cpp
    Shared/gtk/NativeWebWheelEventGtk.cpp
    Shared/gtk/PrintInfoGtk.cpp
    Shared/gtk/ProcessExecutablePathGtk.cpp
    Shared/gtk/WebErrorsGtk.cpp
    Shared/gtk/WebEventFactory.cpp
    Shared/gtk/WebSelectionData.cpp

    Shared/linux/WebMemorySamplerLinux.cpp

    Shared/soup/WebCoreArgumentCodersSoup.cpp
    Shared/soup/WebErrorsSoup.cpp

    Shared/unix/ChildProcessMain.cpp

    StorageProcess/glib/StorageProcessMainGLib.cpp

    UIProcess/AcceleratedDrawingAreaProxy.cpp
    UIProcess/BackingStore.cpp
    UIProcess/DefaultUndoController.cpp
    UIProcess/DrawingAreaProxyImpl.cpp
    UIProcess/LegacySessionStateCodingNone.cpp
    UIProcess/WebResourceLoadStatisticsStore.cpp
    UIProcess/WebResourceLoadStatisticsTelemetry.cpp
    UIProcess/WebTextChecker.cpp
    UIProcess/WebTextCheckerClient.cpp

    UIProcess/API/C/WKGrammarDetail.cpp

    UIProcess/API/C/gtk/WKTextCheckerGtk.cpp
    UIProcess/API/C/gtk/WKView.cpp

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

    UIProcess/API/gtk/PageClientImpl.cpp
    UIProcess/API/gtk/WebKitAuthenticationDialog.cpp
    UIProcess/API/gtk/WebKitColorChooser.cpp
    UIProcess/API/gtk/WebKitColorChooserRequest.cpp
    UIProcess/API/gtk/WebKitOptionMenu.cpp
    UIProcess/API/gtk/WebKitOptionMenuItem.cpp
    UIProcess/API/gtk/WebKitPopupMenu.cpp
    UIProcess/API/gtk/WebKitPrintCustomWidget.cpp
    UIProcess/API/gtk/WebKitPrintOperation.cpp
    UIProcess/API/gtk/WebKitRemoteInspectorProtocolHandler.cpp
    UIProcess/API/gtk/WebKitScriptDialogGtk.cpp
    UIProcess/API/gtk/WebKitVersion.cpp
    UIProcess/API/gtk/WebKitVersion.h.in
    UIProcess/API/gtk/WebKitWebInspector.cpp
    UIProcess/API/gtk/WebKitWebViewBase.cpp
    UIProcess/API/gtk/WebKitWebViewBaseAccessible.cpp
    UIProcess/API/gtk/WebKitWebViewGtk.cpp

    UIProcess/Automation/cairo/WebAutomationSessionCairo.cpp

    UIProcess/Automation/gtk/WebAutomationSessionGtk.cpp

    UIProcess/Launcher/glib/ProcessLauncherGLib.cpp

    UIProcess/linux/MemoryPressureMonitor.cpp

    UIProcess/Plugins/gtk/PluginInfoCache.cpp

    UIProcess/Plugins/unix/PluginInfoStoreUnix.cpp
    UIProcess/Plugins/unix/PluginProcessProxyUnix.cpp

    UIProcess/WebStorage/StorageManager.cpp

    UIProcess/WebsiteData/unix/WebsiteDataStoreUnix.cpp

    UIProcess/cairo/BackingStoreCairo.cpp

    UIProcess/glib/RemoteInspectorClient.cpp

    UIProcess/gstreamer/InstallMissingMediaPluginsPermissionRequest.cpp
    UIProcess/gstreamer/WebPageProxyGStreamer.cpp

    UIProcess/gtk/AcceleratedBackingStore.cpp
    UIProcess/gtk/AcceleratedBackingStoreWayland.cpp
    UIProcess/gtk/AcceleratedBackingStoreX11.cpp
    UIProcess/gtk/DragAndDropHandler.cpp
    UIProcess/gtk/GestureController.cpp
    UIProcess/gtk/HardwareAccelerationManager.cpp
    UIProcess/gtk/InputMethodFilter.cpp
    UIProcess/gtk/KeyBindingTranslator.cpp
    UIProcess/gtk/RemoteWebInspectorProxyGtk.cpp
    UIProcess/gtk/TextCheckerGtk.cpp
    UIProcess/gtk/WaylandCompositor.cpp
    UIProcess/gtk/WebColorPickerGtk.cpp
    UIProcess/gtk/WebContextMenuProxyGtk.cpp
    UIProcess/gtk/WebInspectorProxyGtk.cpp
    UIProcess/gtk/WebKitInspectorWindow.cpp
    UIProcess/gtk/WebPageProxyGtk.cpp
    UIProcess/gtk/WebPasteboardProxyGtk.cpp
    UIProcess/gtk/WebPopupMenuProxyGtk.cpp
    UIProcess/gtk/WebPreferencesGtk.cpp
    UIProcess/gtk/WebProcessPoolGtk.cpp

    UIProcess/soup/WebCookieManagerProxySoup.cpp
    UIProcess/soup/WebProcessPoolSoup.cpp

    WebProcess/Cookies/soup/WebCookieManagerSoup.cpp

    WebProcess/InjectedBundle/API/glib/WebKitConsoleMessage.cpp
    WebProcess/InjectedBundle/API/glib/WebKitExtensionManager.cpp
    WebProcess/InjectedBundle/API/glib/WebKitFrame.cpp
    WebProcess/InjectedBundle/API/glib/WebKitScriptWorld.cpp
    WebProcess/InjectedBundle/API/glib/WebKitWebEditor.cpp
    WebProcess/InjectedBundle/API/glib/WebKitWebExtension.cpp
    WebProcess/InjectedBundle/API/glib/WebKitWebHitTestResult.cpp
    WebProcess/InjectedBundle/API/glib/WebKitWebPage.cpp

    WebProcess/InjectedBundle/API/gtk/DOM/ConvertToUTF8String.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/DOMObjectCache.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/GObjectEventListener.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/GObjectNodeFilterCondition.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/GObjectXPathNSResolver.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMAttr.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMBlob.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCDATASection.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSRule.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSRuleList.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSStyleDeclaration.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSStyleSheet.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSValue.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCharacterData.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMClientRect.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMClientRectList.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMComment.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMImplementation.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMSelection.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMTokenList.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMWindow.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDeprecated.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocument.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentFragment.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentType.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMEvent.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMEventTarget.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMFile.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMFileList.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAnchorElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAppletElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAreaElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBRElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBaseElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBodyElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLButtonElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLCanvasElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLCollection.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDListElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDirectoryElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDivElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDocument.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLEmbedElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFieldSetElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFontElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFormElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFrameElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFrameSetElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHRElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHeadElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHeadingElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHtmlElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLIFrameElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLImageElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLInputElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLIElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLabelElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLegendElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLinkElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMapElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMarqueeElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMenuElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMetaElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLModElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOListElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLObjectElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptGroupElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptionElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptionsCollection.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLParagraphElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLParamElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLPreElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLPrivate.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLQuoteElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLScriptElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLSelectElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLStyleElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableCaptionElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableCellElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableColElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableRowElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableSectionElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTextAreaElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTitleElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLUListElement.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMKeyboardEvent.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMMediaList.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMMouseEvent.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNamedNodeMap.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNode.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeFilter.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeIterator.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeList.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMObject.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMPrivate.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMProcessingInstruction.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMRange.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMStyleSheet.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMStyleSheetList.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMText.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMTreeWalker.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMUIEvent.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMWheelEvent.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathExpression.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathNSResolver.cpp
    WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathResult.cpp

    WebProcess/InjectedBundle/glib/InjectedBundleGlib.cpp

    WebProcess/MediaCache/WebMediaKeyStorageManager.cpp

    WebProcess/Plugins/Netscape/unix/NetscapePluginUnix.cpp
    WebProcess/Plugins/Netscape/unix/PluginProxyUnix.cpp

    WebProcess/Plugins/Netscape/x11/NetscapePluginX11.cpp

    WebProcess/WebCoreSupport/gtk/WebContextMenuClientGtk.cpp
    WebProcess/WebCoreSupport/gtk/WebDragClientGtk.cpp
    WebProcess/WebCoreSupport/gtk/WebEditorClientGtk.cpp
    WebProcess/WebCoreSupport/gtk/WebPopupMenuGtk.cpp

    WebProcess/WebCoreSupport/soup/WebFrameNetworkingContext.cpp

    WebProcess/WebPage/AcceleratedDrawingArea.cpp
    WebProcess/WebPage/AcceleratedSurface.cpp

    WebProcess/WebPage/CoordinatedGraphics/AreaAllocator.cpp
    WebProcess/WebPage/CoordinatedGraphics/CompositingCoordinator.cpp
    WebProcess/WebPage/CoordinatedGraphics/CoordinatedLayerTreeHost.cpp
    WebProcess/WebPage/CoordinatedGraphics/ThreadedCoordinatedLayerTreeHost.cpp
    WebProcess/WebPage/CoordinatedGraphics/UpdateAtlas.cpp

    WebProcess/WebPage/DrawingAreaImpl.cpp

    WebProcess/WebPage/atk/WebPageAccessibilityObjectAtk.cpp

    WebProcess/WebPage/gstreamer/WebPageGStreamer.cpp

    WebProcess/WebPage/gtk/AcceleratedSurfaceWayland.cpp
    WebProcess/WebPage/gtk/AcceleratedSurfaceX11.cpp
    WebProcess/WebPage/gtk/PrinterListGtk.cpp
    WebProcess/WebPage/gtk/WebInspectorUIGtk.cpp
    WebProcess/WebPage/gtk/WebPageGtk.cpp
    WebProcess/WebPage/gtk/WebPrintOperationGtk.cpp

    WebProcess/gtk/WaylandCompositorDisplay.cpp
    WebProcess/gtk/WebProcessMainGtk.cpp

    WebProcess/soup/WebKitSoupRequestInputStream.cpp
    WebProcess/soup/WebProcessSoup.cpp
)

list(APPEND WebKit2_DERIVED_SOURCES
    ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/InspectorGResourceBundle.c
    ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKit2ResourcesGResourceBundle.c

    ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.cpp
)

if (ENABLE_WAYLAND_TARGET)
    list(APPEND WebKit2_DERIVED_SOURCES
        ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKit2WaylandClientProtocol.c
    )
endif ()

set(WebKit2GTK_INSTALLED_HEADERS
    ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.h
    ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitVersion.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitApplicationInfo.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitAuthenticationRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitAutocleanups.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitAutomationSession.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitBackForwardList.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitBackForwardListItem.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitColorChooserRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitCredential.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitContextMenu.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitContextMenuActions.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitContextMenuItem.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitCookieManager.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitDefines.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitDownload.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitEditingCommands.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitEditorState.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitError.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitFaviconDatabase.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitFileChooserRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitFindController.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitFormSubmissionRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitForwardDeclarations.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitGeolocationPermissionRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitHitTestResult.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitInstallMissingMediaPluginsPermissionRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitJavascriptResult.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitMimeInfo.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitNavigationAction.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitNavigationPolicyDecision.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitNetworkProxySettings.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitNotificationPermissionRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitNotification.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitOptionMenu.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitOptionMenuItem.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitPermissionRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitPlugin.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitPolicyDecision.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitPrintCustomWidget.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitPrintOperation.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitResponsePolicyDecision.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitScriptDialog.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitSecurityManager.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitSecurityOrigin.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitSettings.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitURIRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitURIResponse.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitURISchemeRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitUserContent.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitUserContentManager.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitUserMediaPermissionRequest.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWebContext.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWebInspector.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWebResource.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWebView.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWebViewBase.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWebViewSessionState.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWebsiteData.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWebsiteDataManager.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitWindowProperties.h
    ${WEBKIT2_DIR}/UIProcess/API/gtk/webkit2.h
)

set(WebKit2WebExtension_INSTALLED_HEADERS
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitConsoleMessage.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitFrame.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitScriptWorld.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebEditor.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebExtension.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebExtensionAutocleanups.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebHitTestResult.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebPage.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/webkit-web-extension.h
)

set(WebKitDOM_INSTALLED_HEADERS
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/webkitdomautocleanups.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/webkitdomdefines.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/webkitdom.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMAttr.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMBlob.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCDATASection.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCharacterData.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMClientRect.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMClientRectList.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMComment.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSRule.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSRuleList.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSStyleDeclaration.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSStyleSheet.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSValue.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCustom.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCustomUnstable.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDeprecated.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentFragment.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentFragmentUnstable.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocument.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentType.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentUnstable.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMImplementation.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMSelection.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMTokenList.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMWindow.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMWindowUnstable.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMElementUnstable.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMEvent.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMEventTarget.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMFile.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMFileList.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAnchorElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAppletElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAreaElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBaseElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBodyElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBRElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLButtonElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLCanvasElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLCollection.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDirectoryElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDivElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDListElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDocument.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLElementUnstable.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLEmbedElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFieldSetElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFontElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFormElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFrameElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFrameSetElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHeadElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHeadingElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHRElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHtmlElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLIFrameElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLImageElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLInputElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLabelElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLegendElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLIElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLinkElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMapElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMarqueeElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMenuElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMetaElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLModElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLObjectElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOListElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptGroupElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptionElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptionsCollection.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLParagraphElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLParamElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLPreElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLQuoteElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLScriptElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLSelectElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLStyleElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableCaptionElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableCellElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableColElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableRowElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableSectionElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTextAreaElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTitleElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLUListElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMKeyboardEvent.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMMediaList.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMMouseEvent.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNamedNodeMap.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeFilter.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNode.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeIterator.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeList.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMObject.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMProcessingInstruction.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMRange.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMRangeUnstable.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMStyleSheet.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMStyleSheetList.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMText.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMTreeWalker.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMUIEvent.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMWheelEvent.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathExpression.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathNSResolver.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathResult.h
)

set(WebKitDOM_GTKDOC_HEADERS
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMAttr.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMBlob.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCDATASection.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCharacterData.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMClientRect.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMClientRectList.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMComment.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSRule.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSRuleList.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSStyleDeclaration.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSStyleSheet.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSValue.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCustom.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDeprecated.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentFragment.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocument.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentType.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMImplementation.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMSelection.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMTokenList.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMWindow.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMEvent.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMEventTarget.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMFile.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMFileList.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAnchorElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAppletElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAreaElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBaseElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBodyElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBRElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLButtonElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLCanvasElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLCollection.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDirectoryElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDivElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDListElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDocument.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLEmbedElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFieldSetElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFontElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFormElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFrameElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFrameSetElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHeadElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHeadingElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHRElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHtmlElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLIFrameElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLImageElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLInputElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLabelElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLegendElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLIElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLinkElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMapElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMarqueeElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMenuElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMetaElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLModElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLObjectElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOListElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptGroupElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptionElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptionsCollection.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLParagraphElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLParamElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLPreElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLQuoteElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLScriptElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLSelectElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLStyleElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableCaptionElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableCellElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableColElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableRowElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableSectionElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTextAreaElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTitleElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLUListElement.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMKeyboardEvent.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMMediaList.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMMouseEvent.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNamedNodeMap.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeFilter.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNode.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeIterator.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeList.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMObject.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMProcessingInstruction.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMRange.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMStyleSheet.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMStyleSheetList.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMText.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMTreeWalker.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMUIEvent.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMWheelEvent.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathExpression.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathNSResolver.h
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathResult.h
)

set(InspectorFiles
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
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Models/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Protocol/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Proxies/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Test/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Views/*.css
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Views/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Workers/Formatter/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Workers/HeapSnapshot/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Images/gtk/*.png
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Images/gtk/*.svg
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/Localizations/en.lproj/localizedStrings.js
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

# This is necessary because of a conflict between the GTK+ API WebKitVersion.h and one generated by WebCore.
list(INSERT WebKit2_INCLUDE_DIRECTORIES 0
    "${FORWARDING_HEADERS_WEBKIT2GTK_DIR}"
    "${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}"
    "${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}"
    "${DERIVED_SOURCES_WEBKIT2GTK_DIR}"
)

list(APPEND WebKit2_INCLUDE_DIRECTORIES
    "${WEBKIT2_DIR}/PluginProcess/unix"
    "${WEBKIT2_DIR}/NetworkProcess/CustomProtocols/soup"
    "${WEBKIT2_DIR}/NetworkProcess/Downloads/soup"
    "${WEBKIT2_DIR}/NetworkProcess/gtk"
    "${WEBKIT2_DIR}/NetworkProcess/soup"
    "${WEBKIT2_DIR}/NetworkProcess/unix"
    "${WEBKIT2_DIR}/Platform/IPC/glib"
    "${WEBKIT2_DIR}/Platform/IPC/unix"
    "${WEBKIT2_DIR}/Platform/classifier"
    "${WEBKIT2_DIR}/Shared/API/c/gtk"
    "${WEBKIT2_DIR}/Shared/API/glib"
    "${WEBKIT2_DIR}/Shared/CoordinatedGraphics"
    "${WEBKIT2_DIR}/Shared/CoordinatedGraphics/threadedcompositor"
    "${WEBKIT2_DIR}/Shared/Plugins/unix"
    "${WEBKIT2_DIR}/Shared/glib"
    "${WEBKIT2_DIR}/Shared/gtk"
    "${WEBKIT2_DIR}/Shared/linux"
    "${WEBKIT2_DIR}/Shared/soup"
    "${WEBKIT2_DIR}/Shared/unix"
    "${WEBKIT2_DIR}/StorageProcess/unix"
    "${WEBKIT2_DIR}/UIProcess/API/C/cairo"
    "${WEBKIT2_DIR}/UIProcess/API/C/gtk"
    "${WEBKIT2_DIR}/UIProcess/API/glib"
    "${WEBKIT2_DIR}/UIProcess/API/gtk"
    "${WEBKIT2_DIR}/UIProcess/Network/CustomProtocols/soup"
    "${WEBKIT2_DIR}/UIProcess/Plugins/gtk"
    "${WEBKIT2_DIR}/UIProcess/glib"
    "${WEBKIT2_DIR}/UIProcess/gstreamer"
    "${WEBKIT2_DIR}/UIProcess/gtk"
    "${WEBKIT2_DIR}/UIProcess/linux"
    "${WEBKIT2_DIR}/UIProcess/soup"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/glib"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM"
    "${WEBKIT2_DIR}/WebProcess/Plugins/Netscape/unix"
    "${WEBKIT2_DIR}/WebProcess/Plugins/Netscape/x11"
    "${WEBKIT2_DIR}/WebProcess/gtk"
    "${WEBKIT2_DIR}/WebProcess/soup"
    "${WEBKIT2_DIR}/WebProcess/unix"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport/gtk"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport/soup"
    "${WEBKIT2_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT2_DIR}/WebProcess/WebPage/atk"
    "${WEBKIT2_DIR}/WebProcess/WebPage/gtk"
)

list(APPEND WebKit2_SYSTEM_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
    ${ENCHANT_INCLUDE_DIRS}
    ${GEOCLUE_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_PBUTILS_INCLUDE_DIRS}
    ${HARFBUZZ_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

if (USE_LIBNOTIFY)
list(APPEND WebKit2_SYSTEM_INCLUDE_DIRECTORIES
    ${LIBNOTIFY_INCLUDE_DIRS}
)
endif ()

set(WebKit2CommonIncludeDirectories ${WebKit2_INCLUDE_DIRECTORIES})
set(WebKit2CommonSystemIncludeDirectories ${WebKit2_SYSTEM_INCLUDE_DIRECTORIES})

list(APPEND WebKit2_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${GTK_INCLUDE_DIRS}
    ${GTK_UNIX_PRINT_INCLUDE_DIRS}
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

set(SharedWebKit2Libraries
    ${WebKit2_LIBRARIES}
)

list(APPEND WebKit2_LIBRARIES
    WebCorePlatformGTK
    ${GTK_UNIX_PRINT_LIBRARIES}
)

if (LIBNOTIFY_FOUND)
list(APPEND WebKit2_LIBRARIES
    ${LIBNOTIFY_LIBRARIES}
)
endif ()

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
    DEPENDS ${InspectorFilesDependencies}
            ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
            ${TOOLS_DIR}/gtk/generate-inspector-gresource-manifest.py
    COMMAND ${PYTHON_EXECUTABLE} ${TOOLS_DIR}/gtk/generate-inspector-gresource-manifest.py --output=${DERIVED_SOURCES_WEBKIT2GTK_DIR}/InspectorGResourceBundle.xml ${InspectorFiles} ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
    VERBATIM
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/InspectorGResourceBundle.c
    DEPENDS ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/InspectorGResourceBundle.xml
    COMMAND glib-compile-resources --generate --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebInspectorUI --sourcedir=${DERIVED_SOURCES_WEBINSPECTORUI_DIR} --target=${DERIVED_SOURCES_WEBKIT2GTK_DIR}/InspectorGResourceBundle.c ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/InspectorGResourceBundle.xml
    VERBATIM
)

set(WebKit2Resources
    "        <file alias=\"images/missingImage\">missingImage.png</file>\n"
    "        <file alias=\"images/missingImage@2x\">missingImage@2x.png</file>\n"
    "        <file alias=\"images/panIcon\">panIcon.png</file>\n"
    "        <file alias=\"images/textAreaResizeCorner\">textAreaResizeCorner.png</file>\n"
    "        <file alias=\"images/textAreaResizeCorner@2x\">textAreaResizeCorner@2x.png</file>\n"
)

if (ENABLE_ICONDATABASE)
    list(APPEND WebKit2Resources
        "        <file alias=\"images/urlIcon\">urlIcon.png</file>\n"
    )
endif ()

if (ENABLE_WEB_AUDIO)
    list(APPEND WebKit2Resources
        "        <file alias=\"audio/Composite\">Composite.wav</file>\n"
    )
endif ()

file(WRITE ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKit2ResourcesGResourceBundle.xml
    "<?xml version=1.0 encoding=UTF-8?>\n"
    "<gresources>\n"
    "    <gresource prefix=\"/org/webkitgtk/resources\">\n"
    ${WebKit2Resources}
    "    </gresource>\n"
    "</gresources>\n"
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKit2ResourcesGResourceBundle.c
    DEPENDS ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKit2ResourcesGResourceBundle.xml
    COMMAND glib-compile-resources --generate --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/Resources --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/platform/audio/resources --target=${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKit2ResourcesGResourceBundle.c ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKit2ResourcesGResourceBundle.xml
    VERBATIM
)

if (ENABLE_WAYLAND_TARGET)
    # Wayland protocol extension.
    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKit2WaylandClientProtocol.c
        DEPENDS ${WEBKIT2_DIR}/Shared/gtk/WebKit2WaylandProtocol.xml
        COMMAND wayland-scanner server-header < ${WEBKIT2_DIR}/Shared/gtk/WebKit2WaylandProtocol.xml > ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKit2WaylandServerProtocol.h
        COMMAND wayland-scanner client-header < ${WEBKIT2_DIR}/Shared/gtk/WebKit2WaylandProtocol.xml > ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKit2WaylandClientProtocol.h
        COMMAND wayland-scanner code < ${WEBKIT2_DIR}/Shared/gtk/WebKit2WaylandProtocol.xml > ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKit2WaylandClientProtocol.c
    )
endif ()

if (ENABLE_PLUGIN_PROCESS_GTK2)
    set(PluginProcessGTK2_EXECUTABLE_NAME WebKitPluginProcess2)

    # FIXME: We should figure out a way to avoid compiling files that are common between the plugin
    # process and WebKit2 only once instead of recompiling them for the plugin process.
    list(APPEND PluginProcessGTK2_SOURCES
        Platform/Logging.cpp
        Platform/Module.cpp

        Platform/IPC/ArgumentCoders.cpp
        Platform/IPC/Attachment.cpp
        Platform/IPC/Connection.cpp
        Platform/IPC/DataReference.cpp
        Platform/IPC/Decoder.cpp
        Platform/IPC/Encoder.cpp
        Platform/IPC/MessageReceiverMap.cpp
        Platform/IPC/MessageSender.cpp
        Platform/IPC/StringReference.cpp

        Platform/IPC/glib/GSocketMonitor.cpp
        Platform/IPC/unix/AttachmentUnix.cpp
        Platform/IPC/unix/ConnectionUnix.cpp

        Platform/glib/ModuleGlib.cpp

        Platform/unix/LoggingUnix.cpp
        Platform/unix/SharedMemoryUnix.cpp

        PluginProcess/PluginControllerProxy.cpp
        PluginProcess/PluginCreationParameters.cpp
        PluginProcess/PluginProcess.cpp
        PluginProcess/WebProcessConnection.cpp

        PluginProcess/EntryPoint/unix/PluginProcessMain.cpp

        PluginProcess/unix/PluginControllerProxyUnix.cpp
        PluginProcess/unix/PluginProcessMainUnix.cpp
        PluginProcess/unix/PluginProcessUnix.cpp

        Shared/ActivityAssertion.cpp
        Shared/BlobDataFileReferenceWithSandboxExtension.cpp
        Shared/ChildProcess.cpp
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

        Shared/Plugins/Netscape/unix/NetscapePluginModuleUnix.cpp

        Shared/cairo/ShareableBitmapCairo.cpp

        Shared/gtk/NativeWebKeyboardEventGtk.cpp
        Shared/gtk/NativeWebMouseEventGtk.cpp
        Shared/gtk/NativeWebTouchEventGtk.cpp
        Shared/gtk/NativeWebWheelEventGtk.cpp
        Shared/gtk/ProcessExecutablePathGtk.cpp
        Shared/gtk/WebEventFactory.cpp

        Shared/soup/WebCoreArgumentCodersSoup.cpp

        Shared/unix/ChildProcessMain.cpp

        UIProcess/Launcher/ProcessLauncher.cpp

        UIProcess/Launcher/glib/ProcessLauncherGLib.cpp

        UIProcess/Plugins/unix/PluginProcessProxyUnix.cpp

        WebProcess/Plugins/Plugin.cpp

        WebProcess/Plugins/Netscape/NPRuntimeUtilities.cpp
        WebProcess/Plugins/Netscape/NetscapeBrowserFuncs.cpp
        WebProcess/Plugins/Netscape/NetscapePlugin.cpp
        WebProcess/Plugins/Netscape/NetscapePluginNone.cpp
        WebProcess/Plugins/Netscape/NetscapePluginStream.cpp

        WebProcess/Plugins/Netscape/unix/NetscapePluginUnix.cpp
        WebProcess/Plugins/Netscape/x11/NetscapePluginX11.cpp

        ${DERIVED_SOURCES_WEBKIT2_DIR}/PluginControllerProxyMessageReceiver.cpp
        ${DERIVED_SOURCES_WEBKIT2_DIR}/PluginProcessMessageReceiver.cpp
        ${DERIVED_SOURCES_WEBKIT2_DIR}/WebProcessConnectionMessageReceiver.cpp
        ${DERIVED_SOURCES_WEBKIT2_DIR}/NPObjectMessageReceiverMessageReceiver.cpp
        ${DERIVED_SOURCES_WEBKIT2_DIR}/ChildProcessMessageReceiver.cpp
    )

    add_executable(WebKitPluginProcess2 ${PluginProcessGTK2_SOURCES})
    add_webkit2_prefix_header(WebKitPluginProcess2)

    # We need ENABLE_PLUGIN_PROCESS for all targets in this directory, but
    # we only want GTK_API_VERSION_2 for the plugin process target.
    set_property(
        TARGET WebKitPluginProcess2
        APPEND
        PROPERTY COMPILE_DEFINITIONS GTK_API_VERSION_2=1
    )
    target_include_directories(WebKitPluginProcess2 PRIVATE
        ${WebKit2CommonIncludeDirectories}
    )
    target_include_directories(WebKitPluginProcess2 SYSTEM PRIVATE
         ${WebKit2CommonSystemIncludeDirectories}
         ${GTK2_INCLUDE_DIRS}
         ${GDK2_INCLUDE_DIRS}
    )

    set(WebKitPluginProcess2_LIBRARIES
        ${SharedWebKit2Libraries}
        WebCorePlatformGTK2
    )
    ADD_WHOLE_ARCHIVE_TO_LIBRARIES(WebKitPluginProcess2_LIBRARIES)
    target_link_libraries(WebKitPluginProcess2 ${WebKitPluginProcess2_LIBRARIES})

    add_dependencies(WebKitPluginProcess2 WebKit2)

    install(TARGETS WebKitPluginProcess2 DESTINATION "${LIBEXEC_INSTALL_DIR}")

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(WebKitPluginProcess2 -Wno-unused-parameter)
    endif ()
endif () # ENABLE_PLUGIN_PROCESS_GTK2

# GTK3 PluginProcess
list(APPEND PluginProcess_SOURCES
    PluginProcess/EntryPoint/unix/PluginProcessMain.cpp
)

# Commands for building the built-in injected bundle.
include_directories(
    "${WEBKIT2_DIR}/Platform"
    "${WEBKIT2_DIR}/Shared"
    "${WEBKIT2_DIR}/Shared/API/c"
    "${WEBKIT2_DIR}/UIProcess/API/C"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c"
    "${DERIVED_SOURCES_DIR}/InjectedBundle"
    "${FORWARDING_HEADERS_DIR}"
    "${FORWARDING_HEADERS_WEBKIT2GTK_DIR}"
)

add_library(webkit2gtkinjectedbundle MODULE "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/glib/WebKitInjectedBundleMain.cpp")
add_webkit2_prefix_header(webkit2gtkinjectedbundle)
target_link_libraries(webkit2gtkinjectedbundle WebKit2)

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(webkit2gtkinjectedbundle -Wno-unused-parameter)
endif ()

# Add ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} to LD_LIBRARY_PATH or DYLD_LIBRARY_PATH
if (APPLE)
    set(LOADER_LIBRARY_PATH_VAR "DYLD_LIBRARY_PATH")
    set(PREV_LOADER_LIBRARY_PATH "$ENV{DYLD_LIBRARY_PATH}")
else ()
    set(LOADER_LIBRARY_PATH_VAR "LD_LIBRARY_PATH")
    set(PREV_LOADER_LIBRARY_PATH "$ENV{LD_LIBRARY_PATH}")
endif ()

string(COMPARE EQUAL "${PREV_LOADER_LIBRARY_PATH}" "" ld_library_path_not_exist)
if (ld_library_path_does_not_exist)
    set(INTROSPECTION_ADDITIONAL_LIBRARY_PATH
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    )
else ()
    set(INTROSPECTION_ADDITIONAL_LIBRARY_PATH
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}:${PREV_LOADER_LIBRARY_PATH}"
    )
endif ()

# Add required -L flags from ${CMAKE_SHARED_LINKER_FLAGS} for g-ir-scanner
string(REGEX MATCHALL "-L[^ ]*"
    INTROSPECTION_ADDITIONAL_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/WebKit2-${WEBKITGTK_API_VERSION}.gir
    DEPENDS WebKit2
    DEPENDS ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir
    COMMAND CC=${CMAKE_C_COMPILER} CFLAGS=-Wno-deprecated-declarations LDFLAGS=
        ${LOADER_LIBRARY_PATH_VAR}="${INTROSPECTION_ADDITIONAL_LIBRARY_PATH}"
        ${INTROSPECTION_SCANNER}
        --quiet
        --warn-all
        --symbol-prefix=webkit
        --identifier-prefix=WebKit
        --namespace=WebKit2
        --nsversion=${WEBKITGTK_API_VERSION}
        --include=GObject-2.0
        --include=Gtk-3.0
        --include=Soup-2.4
        --include-uninstalled=${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir
        --library=webkit2gtk-${WEBKITGTK_API_VERSION}
        --library=javascriptcoregtk-${WEBKITGTK_API_VERSION}
        -L${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        ${INTROSPECTION_ADDITIONAL_LINKER_FLAGS}
        --no-libtool
        --pkg=gobject-2.0
        --pkg=gtk+-3.0
        --pkg=libsoup-2.4
        --pkg-export=webkit2gtk-${WEBKITGTK_API_VERSION}
        --output=${CMAKE_BINARY_DIR}/WebKit2-${WEBKITGTK_API_VERSION}.gir
        --c-include="webkit2/webkit2.h"
        -DBUILDING_WEBKIT
        -DWEBKIT2_COMPILATION
        -I${CMAKE_SOURCE_DIR}/Source
        -I${WEBKIT2_DIR}
        -I${JAVASCRIPTCORE_DIR}
        -I${JAVASCRIPTCORE_DIR}/ForwardingHeaders
        -I${DERIVED_SOURCES_DIR}
        -I${DERIVED_SOURCES_WEBKIT2GTK_DIR}
        -I${FORWARDING_HEADERS_WEBKIT2GTK_DIR}
        ${WebKit2GTK_INSTALLED_HEADERS}
        ${WEBKIT2_DIR}/Shared/API/glib/*.cpp
        ${WEBKIT2_DIR}/UIProcess/API/glib/*.cpp
        ${WEBKIT2_DIR}/UIProcess/API/gtk/*.cpp
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/WebKit2WebExtension-${WEBKITGTK_API_VERSION}.gir
    DEPENDS ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir
    DEPENDS ${CMAKE_BINARY_DIR}/WebKit2-${WEBKITGTK_API_VERSION}.gir
    COMMAND CC=${CMAKE_C_COMPILER} CFLAGS=-Wno-deprecated-declarations
        LDFLAGS="${INTROSPECTION_ADDITIONAL_LDFLAGS}"
        ${LOADER_LIBRARY_PATH_VAR}="${INTROSPECTION_ADDITIONAL_LIBRARY_PATH}"
        ${INTROSPECTION_SCANNER}
        --quiet
        --warn-all
        --symbol-prefix=webkit
        --identifier-prefix=WebKit
        --namespace=WebKit2WebExtension
        --nsversion=${WEBKITGTK_API_VERSION}
        --include=GObject-2.0
        --include=Gtk-3.0
        --include=Soup-2.4
        --include-uninstalled=${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir
        --library=webkit2gtk-${WEBKITGTK_API_VERSION}
        --library=javascriptcoregtk-${WEBKITGTK_API_VERSION}
        ${INTROSPECTION_ADDITIONAL_LIBRARIES}
        -L${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        ${INTROSPECTION_ADDITIONAL_LINKER_FLAGS}
        --no-libtool
        --pkg=gobject-2.0
        --pkg=gtk+-3.0
        --pkg=libsoup-2.4
        --pkg-export=webkit2gtk-web-extension-${WEBKITGTK_API_VERSION}
        --output=${CMAKE_BINARY_DIR}/WebKit2WebExtension-${WEBKITGTK_API_VERSION}.gir
        --c-include="webkit2/webkit-web-extension.h"
        -DBUILDING_WEBKIT
        -DWEBKIT2_COMPILATION
        -I${CMAKE_SOURCE_DIR}/Source
        -I${WEBKIT2_DIR}
        -I${JAVASCRIPTCORE_DIR}
        -I${JAVASCRIPTCORE_DIR}/ForwardingHeaders
        -I${DERIVED_SOURCES_DIR}
        -I${DERIVED_SOURCES_WEBKIT2GTK_DIR}
        -I${FORWARDING_HEADERS_DIR}
        -I${FORWARDING_HEADERS_WEBKIT2GTK_DIR}
        -I${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}
        -I${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk
        ${WebKitDOM_INSTALLED_HEADERS}
        ${WebKit2WebExtension_INSTALLED_HEADERS}
        ${WEBKIT2_DIR}/Shared/API/glib/WebKitContextMenu.cpp
        ${WEBKIT2_DIR}/Shared/API/glib/WebKitContextMenuItem.cpp
        ${WEBKIT2_DIR}/Shared/API/glib/WebKitHitTestResult.cpp
        ${WEBKIT2_DIR}/Shared/API/glib/WebKitURIRequest.cpp
        ${WEBKIT2_DIR}/Shared/API/glib/WebKitURIResponse.cpp
        ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitContextMenu.h
        ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitContextMenuActions.h
        ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitContextMenuItem.h
        ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitHitTestResult.h
        ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitURIRequest.h
        ${WEBKIT2_DIR}/UIProcess/API/gtk/WebKitURIResponse.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/glib/*.cpp
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
install(FILES "${CMAKE_BINARY_DIR}/Source/WebKit/webkit2gtk-${WEBKITGTK_API_VERSION}.pc"
              "${CMAKE_BINARY_DIR}/Source/WebKit/webkit2gtk-web-extension-${WEBKITGTK_API_VERSION}.pc"
        DESTINATION "${LIB_INSTALL_DIR}/pkgconfig"
)
install(FILES ${WebKit2GTK_INSTALLED_HEADERS}
              ${WebKit2WebExtension_INSTALLED_HEADERS}
        DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/webkit2"
)
install(FILES ${WebKitDOM_INSTALLED_HEADERS}
        DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/webkitdom"
)

if (ENABLE_INTROSPECTION)
    install(FILES ${CMAKE_BINARY_DIR}/WebKit2-${WEBKITGTK_API_VERSION}.gir
                  ${CMAKE_BINARY_DIR}/WebKit2WebExtension-${WEBKITGTK_API_VERSION}.gir
            DESTINATION ${INTROSPECTION_INSTALL_GIRDIR}
    )
    install(FILES ${CMAKE_BINARY_DIR}/WebKit2-${WEBKITGTK_API_VERSION}.typelib
                  ${CMAKE_BINARY_DIR}/WebKit2WebExtension-${WEBKITGTK_API_VERSION}.typelib
            DESTINATION ${INTROSPECTION_INSTALL_TYPELIBDIR}
    )
endif ()

file(WRITE ${CMAKE_BINARY_DIR}/gtkdoc-webkit2gtk.cfg
    "[webkit2gtk-${WEBKITGTK_API_VERSION}]\n"
    "pkgconfig_file=${WebKit2_PKGCONFIG_FILE}\n"
    "namespace=webkit\n"
    "cflags=-I${CMAKE_SOURCE_DIR}/Source\n"
    "       -I${WEBKIT2_DIR}/Shared/API/glib\n"
    "       -I${WEBKIT2_DIR}/UIProcess/API/glib\n"
    "       -I${WEBKIT2_DIR}/UIProcess/API/gtk\n"
    "       -I${DERIVED_SOURCES_WEBKIT2GTK_DIR}\n"
    "       -I${FORWARDING_HEADERS_WEBKIT2GTK_DIR}\n"
    "doc_dir=${WEBKIT2_DIR}/UIProcess/API/gtk/docs\n"
    "source_dirs=${WEBKIT2_DIR}/Shared/API/glib"
    "            ${WEBKIT2_DIR}/UIProcess/API/glib\n"
    "            ${WEBKIT2_DIR}/UIProcess/API/gtk\n"
    "            ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/glib\n"
    "            ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk\n"
    "            ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}\n"
    "headers=${WebKit2GTK_ENUM_GENERATION_HEADERS} ${WebKit2WebExtension_INSTALLED_HEADERS}\n"
    "main_sgml_file=webkit2gtk-docs.sgml\n"
)

file(WRITE ${CMAKE_BINARY_DIR}/gtkdoc-webkitdom.cfg
    "[webkitdomgtk-${WEBKITGTK_API_VERSION}]\n"
    "pkgconfig_file=${WebKit2_PKGCONFIG_FILE}\n"
    "namespace=webkit_dom\n"
    "cflags=-I${CMAKE_SOURCE_DIR}/Source\n"
    "       -I${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM\n"
    "       -I${DERIVED_SOURCES_WEBKIT2GTK_DIR}\n"
    "       -I${FORWARDING_HEADERS_WEBKIT2GTK_DIR}\n"
    "doc_dir=${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/docs\n"
    "source_dirs=${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM\n"
    "headers=${WebKitDOM_GTKDOC_HEADERS}\n"
    "main_sgml_file=webkitdomgtk-docs.sgml\n"
)

add_custom_target(WebKit2-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${WEBKIT2_DIR} --output ${FORWARDING_HEADERS_DIR} --platform gtk --platform soup
)

# These symbolic link allows includes like #include <webkit2/WebkitWebView.h> which simulates installed headers.
add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WEBKIT2GTK_DIR}/webkit2
    DEPENDS ${WEBKIT2_DIR}/UIProcess/API/gtk
    COMMAND ln -n -s -f ${WEBKIT2_DIR}/UIProcess/API/gtk ${FORWARDING_HEADERS_WEBKIT2GTK_DIR}/webkit2
)
add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}/webkit2
    DEPENDS ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk
    COMMAND ln -n -s -f ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}/webkit2
)
add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}/webkitdom
    DEPENDS ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM
    COMMAND ln -n -s -f ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/gtk/DOM ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}/webkitdom
)
add_custom_target(WebKit2-fake-api-headers
    DEPENDS ${FORWARDING_HEADERS_WEBKIT2GTK_DIR}/webkit2
            ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}/webkit2
            ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}/webkitdom
)

set(WEBKIT2_EXTRA_DEPENDENCIES
     WebKit2-fake-api-headers
     WebKit2-forwarding-headers
)
