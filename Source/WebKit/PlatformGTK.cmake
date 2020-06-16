include(InspectorGResources.cmake)

set(WebKit_OUTPUT_NAME webkit2gtk-${WEBKITGTK_API_VERSION})
set(WebProcess_OUTPUT_NAME WebKitWebProcess)
set(NetworkProcess_OUTPUT_NAME WebKitNetworkProcess)
set(GPUProcess_OUTPUT_NAME WebKitGPUProcess)
set(PluginProcess_OUTPUT_NAME WebKitPluginProcess)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WEBKIT2GTK_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WEBKIT2GTK_VERSIONED_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR})

configure_file(UIProcess/API/gtk/WebKitVersion.h.in ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitVersion.h)
configure_file(gtk/webkit2gtk.pc.in ${WebKit2_PKGCONFIG_FILE} @ONLY)
configure_file(gtk/webkit2gtk-web-extension.pc.in ${WebKit2WebExtension_PKGCONFIG_FILE} @ONLY)

add_definitions(-DBUILDING_WEBKIT)
add_definitions(-DWEBKIT2_COMPILATION)
add_definitions(-DWEBKIT_DOM_USE_UNSTABLE_API)

add_definitions(-DPKGLIBEXECDIR="${LIBEXEC_INSTALL_DIR}")
add_definitions(-DLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")
add_definitions(-DDATADIR="${CMAKE_INSTALL_FULL_DATADIR}")
add_definitions(-DLIBDIR="${LIB_INSTALL_DIR}")

if (NOT DEVELOPER_MODE AND NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
    WEBKIT_ADD_TARGET_PROPERTIES(WebKit LINK_FLAGS "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/webkitglib-symbols.map")
endif ()

set(WebKit_USE_PREFIX_HEADER ON)

list(APPEND WebKit_UNIFIED_SOURCE_LIST_FILES
    "SourcesGTK.txt"
)

if (NOT USE_GTK4)
    list(APPEND WebKit_SOURCES
        UIProcess/ViewGestureController.cpp

        UIProcess/gtk/ViewGestureControllerGtk.cpp

        WebProcess/WebPage/ViewGestureGeometryCollector.cpp
    )
    list(APPEND WebKit_MESSAGES_IN_FILES
        UIProcess/ViewGestureController

        WebProcess/WebPage/ViewGestureGeometryCollector
    )
endif ()

list(APPEND WebKit_DERIVED_SOURCES
    ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/InspectorGResourceBundle.c
    ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKitResourcesGResourceBundle.c

    ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.cpp
    ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitWebProcessEnumTypes.cpp
)

if (ENABLE_WAYLAND_TARGET)
    list(APPEND WebKit_DERIVED_SOURCES
        ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKitWaylandClientProtocol.c
        ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/pointer-constraints-unstable-v1-protocol.c
        ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/relative-pointer-unstable-v1-protocol.c
    )
endif ()

if (USE_GTK4)
    set(GTK_API_VERSION 4)
else ()
    set(GTK_API_VERSION 3)
endif ()

set(WebKit2GTK_INSTALLED_HEADERS
    ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.h
    ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitVersion.h
    ${WEBKIT_DIR}/UIProcess/API/gtk${GTK_API_VERSION}/WebKitContextMenuItem.h
    ${WEBKIT_DIR}/UIProcess/API/gtk${GTK_API_VERSION}/WebKitInputMethodContext.h
    ${WEBKIT_DIR}/UIProcess/API/gtk${GTK_API_VERSION}/WebKitWebViewBase.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitApplicationInfo.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitAuthenticationRequest.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitAutocleanups.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitAutomationSession.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitBackForwardList.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitBackForwardListItem.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitColorChooserRequest.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitCredential.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitContextMenu.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitContextMenuActions.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitCookieManager.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitDefines.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitDeviceInfoPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitDownload.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitEditingCommands.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitEditorState.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitError.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitFaviconDatabase.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitFileChooserRequest.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitFindController.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitFormSubmissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitForwardDeclarations.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitGeolocationManager.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitGeolocationPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitHitTestResult.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitInstallMissingMediaPluginsPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitJavascriptResult.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitMimeInfo.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitNavigationAction.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitNavigationPolicyDecision.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitNetworkProxySettings.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitNotificationPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitNotification.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitOptionMenu.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitOptionMenuItem.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitPlugin.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitPointerLockPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitPolicyDecision.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitPrintCustomWidget.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitPrintOperation.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitResponsePolicyDecision.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitScriptDialog.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitSecurityManager.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitSecurityOrigin.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitSettings.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitURIRequest.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitURIResponse.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitURISchemeRequest.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitURIUtilities.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitUserContent.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitUserContentFilterStore.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitUserContentManager.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitUserMediaPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitUserMessage.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitWebContext.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitWebInspector.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitWebResource.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitWebView.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitWebViewSessionState.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitWebsiteData.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitWebsiteDataAccessPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitWebsiteDataManager.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitWindowProperties.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitWebsitePolicies.h
    ${WEBKIT_DIR}/UIProcess/API/gtk/webkit2.h
)

set(WebKit2WebExtension_INSTALLED_HEADERS
    ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitWebProcessEnumTypes.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitConsoleMessage.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitFrame.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitScriptWorld.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebEditor.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebExtension.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebExtensionAutocleanups.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebHitTestResult.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebPage.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/webkit-web-extension.h
)

set(WebKitDOM_INSTALLED_HEADERS
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/webkitdomautocleanups.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/webkitdomdefines.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/webkitdom.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMAttr.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMBlob.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCDATASection.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCharacterData.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMClientRect.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMClientRectList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMComment.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSRule.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSRuleList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSStyleDeclaration.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSStyleSheet.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSValue.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCustom.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCustomUnstable.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDeprecated.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentFragment.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentFragmentUnstable.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocument.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentType.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentUnstable.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMImplementation.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMSelection.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMTokenList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMWindow.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMWindowUnstable.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMElementUnstable.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMEvent.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMEventTarget.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMFile.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMFileList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAnchorElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAppletElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAreaElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBaseElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBodyElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBRElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLButtonElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLCanvasElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLCollection.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDirectoryElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDivElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDListElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDocument.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLElementUnstable.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLEmbedElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFieldSetElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFontElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFormElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFrameElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFrameSetElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHeadElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHeadingElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHRElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHtmlElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLIFrameElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLImageElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLInputElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLabelElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLegendElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLIElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLinkElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMapElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMarqueeElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMenuElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMetaElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLModElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLObjectElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOListElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptGroupElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptionElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptionsCollection.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLParagraphElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLParamElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLPreElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLQuoteElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLScriptElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLSelectElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLStyleElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableCaptionElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableCellElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableColElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableRowElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableSectionElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTextAreaElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTitleElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLUListElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMKeyboardEvent.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMMediaList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMMouseEvent.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNamedNodeMap.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeFilter.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNode.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeIterator.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMObject.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMProcessingInstruction.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMRange.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMRangeUnstable.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMStyleSheet.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMStyleSheetList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMText.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMTreeWalker.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMUIEvent.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMWheelEvent.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathExpression.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathNSResolver.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathResult.h
)

set(WebKitDOM_GTKDOC_HEADERS
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMAttr.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMBlob.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCDATASection.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCharacterData.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMClientRect.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMClientRectList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMComment.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSRule.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSRuleList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSStyleDeclaration.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSStyleSheet.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSValue.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCustom.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDeprecated.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentFragment.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocument.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentType.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMImplementation.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMSelection.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMTokenList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMWindow.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMEvent.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMEventTarget.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMFile.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMFileList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAnchorElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAppletElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAreaElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBaseElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBodyElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBRElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLButtonElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLCanvasElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLCollection.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDirectoryElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDivElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDListElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDocument.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLEmbedElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFieldSetElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFontElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFormElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFrameElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFrameSetElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHeadElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHeadingElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHRElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHtmlElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLIFrameElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLImageElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLInputElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLabelElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLegendElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLIElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLinkElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMapElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMarqueeElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMenuElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMetaElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLModElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLObjectElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOListElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptGroupElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptionElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptionsCollection.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLParagraphElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLParamElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLPreElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLQuoteElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLScriptElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLSelectElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLStyleElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableCaptionElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableCellElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableColElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableRowElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableSectionElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTextAreaElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTitleElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLUListElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMKeyboardEvent.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMMediaList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMMouseEvent.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNamedNodeMap.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeFilter.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNode.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeIterator.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMObject.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMProcessingInstruction.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMRange.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMStyleSheet.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMStyleSheetList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMText.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMTreeWalker.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMUIEvent.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMWheelEvent.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathExpression.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathNSResolver.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathResult.h
)

# This is necessary because of a conflict between the GTK+ API WebKitVersion.h and one generated by WebCore.
list(INSERT WebKit_INCLUDE_DIRECTORIES 0
    "${FORWARDING_HEADERS_WEBKIT2GTK_DIR}"
    "${FORWARDING_HEADERS_WEBKIT2GTK_VERSIONED_DIR}"
    "${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}"
    "${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}"
    "${DERIVED_SOURCES_WEBKIT2GTK_DIR}"
)

list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${DERIVED_SOURCES_JAVASCRIPCOREGTK_DIR}"
    "${FORWARDING_HEADERS_DIR}/JavaScriptCore/"
    "${FORWARDING_HEADERS_DIR}/JavaScriptCore/glib"
    "${WEBKIT_DIR}/NetworkProcess/glib"
    "${WEBKIT_DIR}/NetworkProcess/gtk"
    "${WEBKIT_DIR}/NetworkProcess/soup"
    "${WEBKIT_DIR}/Platform/IPC/glib"
    "${WEBKIT_DIR}/Platform/IPC/unix"
    "${WEBKIT_DIR}/Platform/classifier"
    "${WEBKIT_DIR}/Platform/generic"
    "${WEBKIT_DIR}/Shared/API/c/gtk"
    "${WEBKIT_DIR}/Shared/API/glib"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics/threadedcompositor"
    "${WEBKIT_DIR}/Shared/Plugins/unix"
    "${WEBKIT_DIR}/Shared/glib"
    "${WEBKIT_DIR}/Shared/gtk"
    "${WEBKIT_DIR}/Shared/linux"
    "${WEBKIT_DIR}/Shared/soup"
    "${WEBKIT_DIR}/UIProcess/API/C/cairo"
    "${WEBKIT_DIR}/UIProcess/API/C/gtk"
    "${WEBKIT_DIR}/UIProcess/API/glib"
    "${WEBKIT_DIR}/UIProcess/API/gtk${GTK_API_VERSION}"
    "${WEBKIT_DIR}/UIProcess/API/gtk"
    "${WEBKIT_DIR}/UIProcess/CoordinatedGraphics"
    "${WEBKIT_DIR}/UIProcess/Inspector/glib"
    "${WEBKIT_DIR}/UIProcess/Inspector/gtk"
    "${WEBKIT_DIR}/UIProcess/Plugins/gtk"
    "${WEBKIT_DIR}/UIProcess/geoclue"
    "${WEBKIT_DIR}/UIProcess/glib"
    "${WEBKIT_DIR}/UIProcess/gstreamer"
    "${WEBKIT_DIR}/UIProcess/gtk"
    "${WEBKIT_DIR}/UIProcess/linux"
    "${WEBKIT_DIR}/UIProcess/soup"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/DOM"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM"
    "${WEBKIT_DIR}/WebProcess/Inspector/gtk"
    "${WEBKIT_DIR}/WebProcess/Plugins/Netscape/unix"
    "${WEBKIT_DIR}/WebProcess/Plugins/Netscape/x11"
    "${WEBKIT_DIR}/WebProcess/gtk"
    "${WEBKIT_DIR}/WebProcess/soup"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/gtk"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/soup"
    "${WEBKIT_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT_DIR}/WebProcess/WebPage/atk"
    "${WEBKIT_DIR}/WebProcess/WebPage/gtk"
)

list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
    ${ENCHANT_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_PBUTILS_INCLUDE_DIRS}
    ${GTK_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

if (USE_WPE_RENDERER)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBKIT_DIR}/WebProcess/WebPage/libwpe"
    )
    list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
        ${WPEBACKEND_FDO_INCLUDE_DIRS}
    )
endif ()

if (USE_LIBNOTIFY)
list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
    ${LIBNOTIFY_INCLUDE_DIRS}
)
endif ()

set(WebKitCommonIncludeDirectories ${WebKit_INCLUDE_DIRECTORIES})
set(WebKitCommonSystemIncludeDirectories ${WebKit_SYSTEM_INCLUDE_DIRECTORIES})

list(APPEND WebProcess_SOURCES
    WebProcess/EntryPoint/unix/WebProcessMain.cpp
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/unix/NetworkProcessMain.cpp
)

list(APPEND GPUProcess_SOURCES
    GPUProcess/EntryPoint/unix/GPUProcessMain.cpp
)

if (USE_WPE_RENDERER)
    list(APPEND WebKit_LIBRARIES
      WPE::libwpe
      ${WPEBACKEND_FDO_LIBRARIES}
    )
endif ()

if (GTK_UNIX_PRINT_FOUND)
    list(APPEND WebKit_LIBRARIES GTK::UnixPrint)
endif ()

if (LIBNOTIFY_FOUND)
    list(APPEND WebKit_PRIVATE_LIBRARIES
        ${LIBNOTIFY_LIBRARIES}
    )
endif ()

if (USE_LIBWEBRTC)
    list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
        "${THIRDPARTY_DIR}/libwebrtc/Source/"
        "${THIRDPARTY_DIR}/libwebrtc/Source/webrtc"
    )
endif ()

# To generate WebKitEnumTypes.h we want to use all installed headers, except WebKitEnumTypes.h itself.
set(WebKit2GTK_ENUM_GENERATION_HEADERS ${WebKit2GTK_INSTALLED_HEADERS})
list(REMOVE_ITEM WebKit2GTK_ENUM_GENERATION_HEADERS ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.h)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.h
           ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.cpp
    DEPENDS ${WebKit2GTK_ENUM_GENERATION_HEADERS}

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitEnumTypes.h.template ${WebKit2GTK_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ | sed s/WEBKIT_TYPE_KIT/WEBKIT_TYPE/ > ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.h

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitEnumTypes.cpp.template ${WebKit2GTK_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ > ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitEnumTypes.cpp
    VERBATIM
)

set(WebKit2GTK_WEB_PROCESS_ENUM_GENERATION_HEADERS ${WebKit2WebExtension_INSTALLED_HEADERS})
list(REMOVE_ITEM WebKit2GTK_WEB_PROCESS_ENUM_GENERATION_HEADERS ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitWebProcessEnumTypes.h)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitWebProcessEnumTypes.h
           ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitWebProcessEnumTypes.cpp
    DEPENDS ${WebKit2GTK_WEB_PROCESS_ENUM_GENERATION_HEADERS}

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebProcessEnumTypes.h.template ${WebKit2GTK_WEB_PROCESS_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ | sed s/WEBKIT_TYPE_KIT/WEBKIT_TYPE/ > ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitWebProcessEnumTypes.h

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebProcessEnumTypes.cpp.template ${WebKit2GTK_WEB_PROCESS_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ > ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}/WebKitWebProcessEnumTypes.cpp
    VERBATIM
)

WEBKIT_BUILD_INSPECTOR_GRESOURCES(${DERIVED_SOURCES_WEBKIT2GTK_DIR})

set(WebKitResources
    "        <file alias=\"css/gtk-theme.css\">gtk-theme.css</file>\n"
    "        <file alias=\"images/missingImage\">missingImage.png</file>\n"
    "        <file alias=\"images/missingImage@2x\">missingImage@2x.png</file>\n"
    "        <file alias=\"images/panIcon\">panIcon.png</file>\n"
    "        <file alias=\"images/textAreaResizeCorner\">textAreaResizeCorner.png</file>\n"
    "        <file alias=\"images/textAreaResizeCorner@2x\">textAreaResizeCorner@2x.png</file>\n"
)

if (ENABLE_WEB_AUDIO)
    list(APPEND WebKitResources
        "        <file alias=\"audio/Composite\">Composite.wav</file>\n"
    )
endif ()

file(WRITE ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKitResourcesGResourceBundle.xml
    "<?xml version=1.0 encoding=UTF-8?>\n"
    "<gresources>\n"
    "    <gresource prefix=\"/org/webkitgtk/resources\">\n"
    ${WebKitResources}
    "    </gresource>\n"
    "</gresources>\n"
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKitResourcesGResourceBundle.c
    DEPENDS ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKitResourcesGResourceBundle.xml
    COMMAND glib-compile-resources --generate --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/Resources --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/platform/audio/resources --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebKit/Resources/gtk --target=${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKitResourcesGResourceBundle.c ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKitResourcesGResourceBundle.xml
    VERBATIM
)

if (ENABLE_WAYLAND_TARGET)
    # Wayland protocol extension.
    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKitWaylandClientProtocol.c
        DEPENDS ${WEBKIT_DIR}/Shared/gtk/WebKitWaylandProtocol.xml
        COMMAND ${WAYLAND_SCANNER} server-header ${WEBKIT_DIR}/Shared/gtk/WebKitWaylandProtocol.xml ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKitWaylandServerProtocol.h
        COMMAND ${WAYLAND_SCANNER} client-header ${WEBKIT_DIR}/Shared/gtk/WebKitWaylandProtocol.xml ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKitWaylandClientProtocol.h
        COMMAND ${WAYLAND_SCANNER} private-code ${WEBKIT_DIR}/Shared/gtk/WebKitWaylandProtocol.xml ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/WebKitWaylandClientProtocol.c
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/pointer-constraints-unstable-v1-protocol.c
        DEPENDS ${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
        COMMAND ${WAYLAND_SCANNER} private-code ${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/pointer-constraints-unstable-v1-protocol.c
        COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/pointer-constraints-unstable-v1-client-protocol.h
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/relative-pointer-unstable-v1-protocol.c
        DEPENDS ${WAYLAND_PROTOCOLS_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml
        COMMAND ${WAYLAND_SCANNER} private-code ${WAYLAND_PROTOCOLS_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/relative-pointer-unstable-v1-protocol.c
        COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/relative-pointer-unstable-v1-client-protocol.h
        VERBATIM
    )
endif ()

# GTK3 PluginProcess
list(APPEND PluginProcess_SOURCES
    PluginProcess/EntryPoint/unix/PluginProcessMain.cpp
)

# Commands for building the built-in injected bundle.
add_library(webkit2gtkinjectedbundle MODULE "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitInjectedBundleMain.cpp")
ADD_WEBKIT_PREFIX_HEADER(webkit2gtkinjectedbundle)
target_link_libraries(webkit2gtkinjectedbundle WebKit)

target_include_directories(webkit2gtkinjectedbundle PRIVATE
    ${WebKit_INCLUDE_DIRECTORIES}
    ${WebKit_PRIVATE_INCLUDE_DIRECTORIES}
    "${DERIVED_SOURCES_DIR}/InjectedBundle"
    "${FORWARDING_HEADERS_DIR}"
    "${FORWARDING_HEADERS_WEBKIT2GTK_DIR}"
    "${FORWARDING_HEADERS_WEBKIT2GTK_VERSIONED_DIR}"
    "${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}"
)

target_include_directories(webkit2gtkinjectedbundle SYSTEM PRIVATE
    ${WebKit_SYSTEM_INCLUDE_DIRECTORIES}
)

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

if (ENABLE_INTROSPECTION)
    string(COMPARE EQUAL "${PREV_LOADER_LIBRARY_PATH}" "" ld_library_path_does_not_exist)
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
        DEPENDS WebKit
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
            -I${WEBKIT_DIR}
            -I${DERIVED_SOURCES_DIR}
            -I${DERIVED_SOURCES_WEBKIT2GTK_DIR}
            -I${DERIVED_SOURCES_JAVASCRIPCOREGTK_DIR}
            -I${FORWARDING_HEADERS_DIR}
            -I${FORWARDING_HEADERS_DIR}/JavaScriptCore/glib
            -I${FORWARDING_HEADERS_WEBKIT2GTK_DIR}
            -I${FORWARDING_HEADERS_WEBKIT2GTK_VERSIONED_DIR}
            ${WebKit2GTK_INSTALLED_HEADERS}
            ${WEBKIT_DIR}/Shared/API/glib/*.cpp
            ${WEBKIT_DIR}/UIProcess/API/glib/*.cpp
            ${WEBKIT_DIR}/UIProcess/API/gtk/*.cpp
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
            -I${WEBKIT_DIR}
            -I${DERIVED_SOURCES_DIR}
            -I${DERIVED_SOURCES_WEBKIT2GTK_DIR}
            -I${DERIVED_SOURCES_JAVASCRIPCOREGTK_DIR}
            -I${FORWARDING_HEADERS_DIR}
            -I${FORWARDING_HEADERS_DIR}/JavaScriptCore/glib
            -I${FORWARDING_HEADERS_WEBKIT2GTK_DIR}
            -I${FORWARDING_HEADERS_WEBKIT2GTK_VERSIONED_DIR}
            -I${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}
            -I${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk
            ${WebKitDOM_INSTALLED_HEADERS}
            ${WebKit2WebExtension_INSTALLED_HEADERS}
            ${WEBKIT_DIR}/Shared/API/glib/WebKitContextMenu.cpp
            ${WEBKIT_DIR}/Shared/API/glib/WebKitContextMenuItem.cpp
            ${WEBKIT_DIR}/Shared/API/glib/WebKitHitTestResult.cpp
            ${WEBKIT_DIR}/Shared/API/glib/WebKitUserMessage.cpp
            ${WEBKIT_DIR}/Shared/API/glib/WebKitURIRequest.cpp
            ${WEBKIT_DIR}/Shared/API/glib/WebKitURIResponse.cpp
            ${WEBKIT_DIR}/UIProcess/API/gtk${GTK_API_VERSION}/WebKitContextMenuItem.h
            ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitContextMenu.h
            ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitContextMenuActions.h
            ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitHitTestResult.h
            ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitUserMessage.h
            ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitURIRequest.h
            ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitURIResponse.h
            ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/*.cpp
            ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/DOM/*.cpp
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
endif ()

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
    "decorator=WEBKIT_API|WEBKIT_DEPRECATED|WEBKIT_DEPRECATED_FOR\\(.+\\)\n"
    "deprecation_guard=WEBKIT_DISABLE_DEPRECATED\n"
    "namespace=webkit\n"
    "cflags=-I${CMAKE_SOURCE_DIR}/Source\n"
    "       -I${WEBKIT_DIR}/Shared/API/glib\n"
    "       -I${WEBKIT_DIR}/UIProcess/API/glib\n"
    "       -I${WEBKIT_DIR}/UIProcess/API/gtk\n"
    "       -I${DERIVED_SOURCES_WEBKIT2GTK_DIR}\n"
    "       -I${FORWARDING_HEADERS_WEBKIT2GTK_DIR}\n"
    "       -I${FORWARDING_HEADERS_WEBKIT2GTK_VERSIONED_DIR}\n"
    "doc_dir=${WEBKIT_DIR}/UIProcess/API/gtk/docs\n"
    "source_dirs=${WEBKIT_DIR}/Shared/API/glib\n"
    "            ${WEBKIT_DIR}/UIProcess/API/glib\n"
    "            ${WEBKIT_DIR}/UIProcess/API/gtk${GTK_API_VERSION}\n"
    "            ${WEBKIT_DIR}/UIProcess/API/gtk\n"
    "            ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib\n"
    "            ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk\n"
    "            ${DERIVED_SOURCES_WEBKIT2GTK_API_DIR}\n"
    "headers=${WebKit2GTK_ENUM_GENERATION_HEADERS} ${WebKit2WebExtension_INSTALLED_HEADERS}\n"
    "main_sgml_file=webkit2gtk-docs.sgml\n"
)

file(WRITE ${CMAKE_BINARY_DIR}/gtkdoc-webkitdom.cfg
    "[webkitdomgtk-${WEBKITGTK_API_VERSION}]\n"
    "pkgconfig_file=${WebKit2_PKGCONFIG_FILE}\n"
    "decorator=WEBKIT_API|WEBKIT_DEPRECATED|WEBKIT_DEPRECATED_FOR\\(.+\\)\n"
    "deprecation_guard=WEBKIT_DISABLE_DEPRECATED\n"
    "namespace=webkit_dom\n"
    "cflags=-I${CMAKE_SOURCE_DIR}/Source\n"
    "       -I${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM\n"
    "       -I${DERIVED_SOURCES_WEBKIT2GTK_DIR}\n"
    "       -I${FORWARDING_HEADERS_WEBKIT2GTK_DIR}\n"
    "       -I${FORWARDING_HEADERS_WEBKIT2GTK_VERSIONED_DIR}\n"
    "doc_dir=${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/docs\n"
    "source_dirs=${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/DOM\n"
    "            ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM\n"
    "headers=${WebKitDOM_GTKDOC_HEADERS}\n"
    "main_sgml_file=webkitdomgtk-docs.sgml\n"
)

add_custom_target(WebKit-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${WEBKIT_DIR} --output ${FORWARDING_HEADERS_DIR} --platform gtk --platform soup
)

# These symbolic link allows includes like #include <webkit2/WebkitWebView.h> which simulates installed headers.
add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WEBKIT2GTK_DIR}/webkit2
    DEPENDS ${WEBKIT_DIR}/UIProcess/API/gtk
    COMMAND ln -n -s -f ${WEBKIT_DIR}/UIProcess/API/gtk ${FORWARDING_HEADERS_WEBKIT2GTK_DIR}/webkit2
)
add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WEBKIT2GTK_VERSIONED_DIR}/webkit2
    DEPENDS ${WEBKIT_DIR}/UIProcess/API/gtk${GTK_API_VERSION}
    COMMAND ln -n -s -f ${WEBKIT_DIR}/UIProcess/API/gtk${GTK_API_VERSION} ${FORWARDING_HEADERS_WEBKIT2GTK_VERSIONED_DIR}/webkit2
)
add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}/webkit2
    DEPENDS ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk
    COMMAND ln -n -s -f ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}/webkit2
)
add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}/webkitdom
    DEPENDS ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM
    COMMAND ln -n -s -f ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}/webkitdom
)
add_custom_target(WebKit-fake-api-headers
    DEPENDS ${FORWARDING_HEADERS_WEBKIT2GTK_DIR}/webkit2
            ${FORWARDING_HEADERS_WEBKIT2GTK_VERSIONED_DIR}/webkit2
            ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}/webkit2
            ${FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR}/webkitdom
)

list(APPEND WebKit_DEPENDENCIES
     WebKit-fake-api-headers
     WebKit-forwarding-headers
)
