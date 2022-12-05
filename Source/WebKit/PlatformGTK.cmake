include(GLib.cmake)
include(InspectorGResources.cmake)

if (ENABLE_PDFJS)
    include(PdfJSGResources.cmake)
endif ()

if (ENABLE_MODERN_MEDIA_CONTROLS)
    include(ModernMediaControlsGResources.cmake)
endif ()

set(WebKit_OUTPUT_NAME webkit${WEBKITGTK_API_INFIX}gtk-${WEBKITGTK_API_VERSION})
set(WebProcess_OUTPUT_NAME WebKitWebProcess)
set(NetworkProcess_OUTPUT_NAME WebKitNetworkProcess)
set(GPUProcess_OUTPUT_NAME WebKitGPUProcess)

file(MAKE_DIRECTORY ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit)
file(MAKE_DIRECTORY ${WebKitGTK_FRAMEWORK_HEADERS_DIR})
file(MAKE_DIRECTORY ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkitgtk-${WEBKITGTK_API_VERSION})
file(MAKE_DIRECTORY ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkitgtk-webextension)

configure_file(Shared/glib/BuildRevision.h.in ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/BuildRevision.h)
configure_file(UIProcess/API/gtk/WebKitVersion.h.in ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitVersion.h)
configure_file(gtk/webkitgtk.pc.in ${WebKitGTK_PKGCONFIG_FILE} @ONLY)
configure_file(gtk/webkitgtk-web-extension.pc.in ${WebKitGTKWebExtension_PKGCONFIG_FILE} @ONLY)

if (EXISTS "${TOOLS_DIR}/glib/apply-build-revision-to-files.py")
    add_custom_target(WebKit-build-revision
        ${PYTHON_EXECUTABLE} "${TOOLS_DIR}/glib/apply-build-revision-to-files.py" ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/BuildRevision.h ${WebKitGTK_PKGCONFIG_FILE} ${WebKitGTKWebExtension_PKGCONFIG_FILE}
        DEPENDS ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/BuildRevision.h ${WebKitGTK_PKGCONFIG_FILE} ${WebKitGTKWebExtension_PKGCONFIG_FILE}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} VERBATIM)
    list(APPEND WebKit_DEPENDENCIES
        WebKit-build-revision
    )
endif ()

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

list(APPEND WebKit_MESSAGES_IN_FILES
    UIProcess/ViewGestureController

    WebProcess/gtk/GtkSettingsManagerProxy
    WebProcess/WebPage/ViewGestureGeometryCollector
)

list(APPEND WebKit_DERIVED_SOURCES
    ${WebKitGTK_DERIVED_SOURCES_DIR}/InspectorGResourceBundle.c
    ${WebKitGTK_DERIVED_SOURCES_DIR}/WebKitDirectoryInputStreamData.cpp
    ${WebKitGTK_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.c

    ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitEnumTypes.cpp
    ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitWebProcessEnumTypes.cpp
)

if (ENABLE_WAYLAND_TARGET)
    list(APPEND WebKit_DERIVED_SOURCES
        ${WebKitGTK_DERIVED_SOURCES_DIR}/pointer-constraints-unstable-v1-protocol.c
        ${WebKitGTK_DERIVED_SOURCES_DIR}/relative-pointer-unstable-v1-protocol.c
    )
endif ()

if (ENABLE_PDFJS)
    list(APPEND WebKit_DERIVED_SOURCES
        ${WebKitGTK_DERIVED_SOURCES_DIR}/PdfJSGResourceBundle.c
        ${WebKitGTK_DERIVED_SOURCES_DIR}/PdfJSGResourceBundleExtras.c
    )

    WEBKIT_BUILD_PDFJS_GRESOURCES(${WebKitGTK_DERIVED_SOURCES_DIR})
endif ()

if (ENABLE_MODERN_MEDIA_CONTROLS)
    list(APPEND WebKit_DERIVED_SOURCES
        ${WebKitGTK_DERIVED_SOURCES_DIR}/ModernMediaControlsGResourceBundle.c
    )

    WEBKIT_BUILD_MODERN_MEDIA_CONTROLS_GRESOURCES(${WebKitGTK_DERIVED_SOURCES_DIR})
endif ()

set(WebKit_DirectoryInputStream_DATA
    ${WEBKIT_DIR}/NetworkProcess/soup/Resources/directory.css
    ${WEBKIT_DIR}/NetworkProcess/soup/Resources/directory.js
)

add_custom_command(
    OUTPUT ${WebKitGTK_DERIVED_SOURCES_DIR}/WebKitDirectoryInputStreamData.cpp ${WebKitGTK_DERIVED_SOURCES_DIR}/WebKitDirectoryInputStreamData.h
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/make-css-file-arrays.pl
    DEPENDS ${WebKit_DirectoryInputStream_DATA}
    COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/css/make-css-file-arrays.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" ${WebKitGTK_DERIVED_SOURCES_DIR}/WebKitDirectoryInputStreamData.h ${WebKitGTK_DERIVED_SOURCES_DIR}/WebKitDirectoryInputStreamData.cpp ${WebKit_DirectoryInputStream_DATA}
    VERBATIM
)

if (USE_GTK4)
    set(GTK_API_VERSION 4)
    set(GTK_PKGCONFIG_PACKAGE gtk4)
else ()
    set(GTK_API_VERSION 3)
    set(GTK_PKGCONFIG_PACKAGE gtk+-3.0)
endif ()

set(WebKitGTK_HEADER_TEMPLATES
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitApplicationInfo.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitAuthenticationRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitAutocleanups.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitAutomationSession.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitBackForwardList.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitBackForwardListItem.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitCredential.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitContextMenu.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitContextMenuActions.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitContextMenuItem.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitCookieManager.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitDefines.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitDeviceInfoPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitDownload.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitEditingCommands.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitEditorState.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitError.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitFaviconDatabase.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitFileChooserRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitFindController.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitFormSubmissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitGeolocationManager.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitGeolocationPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitHitTestResult.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitInputMethodContext.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitInstallMissingMediaPluginsPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitJavascriptResult.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitMediaKeySystemPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitMemoryPressureSettings.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitMimeInfo.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitNavigationAction.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitNavigationPolicyDecision.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitNetworkProxySettings.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitNotificationPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitNotification.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitOptionMenu.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitOptionMenuItem.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitPlugin.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitPolicyDecision.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitResponsePolicyDecision.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitScriptDialog.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitSecurityManager.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitSecurityOrigin.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitSettings.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitURIRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitURIResponse.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitURISchemeRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitURISchemeResponse.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitURIUtilities.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitUserContent.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitUserContentFilterStore.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitUserContentManager.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitUserMediaPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitUserMessage.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebContext.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebResource.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebView.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebViewSessionState.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebsiteData.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebsiteDataAccessPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebsiteDataManager.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWindowProperties.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebsitePolicies.h.in
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitColorChooserRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitPointerLockPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitPrintCustomWidget.h.in
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitPrintOperation.h.in
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitWebInspector.h.in
    ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitWebViewBase.h.in
)

set(WebKitGTK_INSTALLED_HEADERS
    ${WEBKIT_DIR}/UIProcess/API/gtk/webkit${WEBKITGTK_API_INFIX}.h
    ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitEnumTypes.h
    ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitVersion.h
)

set(WebKitWebExtension_INSTALLED_HEADERS
    ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitWebProcessEnumTypes.h
)

set(WebKitWebExtension_HEADER_TEMPLATES
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitConsoleMessage.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitFrame.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitScriptWorld.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebEditor.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebExtension.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebExtensionAutocleanups.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebFormManager.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebHitTestResult.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebPage.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/webkit-web-extension.h.in
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
    "${WebKitGTK_FRAMEWORK_HEADERS_DIR}"
    "${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkitgtk-${WEBKITGTK_API_VERSION}"
    "${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkitgtk-webextension"
    "${WebKitGTK_DERIVED_SOURCES_DIR}/webkit"
    "${WebKitGTK_DERIVED_SOURCES_DIR}"
)

list(APPEND WebKit_INCLUDE_DIRECTORIES
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
    "${WEBKIT_DIR}/Shared/glib"
    "${WEBKIT_DIR}/Shared/gtk"
    "${WEBKIT_DIR}/Shared/linux"
    "${WEBKIT_DIR}/Shared/soup"
    "${WEBKIT_DIR}/UIProcess/API/C/cairo"
    "${WEBKIT_DIR}/UIProcess/API/C/glib"
    "${WEBKIT_DIR}/UIProcess/API/C/gtk"
    "${WEBKIT_DIR}/UIProcess/API/glib"
    "${WEBKIT_DIR}/UIProcess/API/gtk"
    "${WEBKIT_DIR}/UIProcess/CoordinatedGraphics"
    "${WEBKIT_DIR}/UIProcess/Inspector/glib"
    "${WEBKIT_DIR}/UIProcess/Inspector/gtk"
    "${WEBKIT_DIR}/UIProcess/Notifications/glib/"
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
    "${WEBKIT_DIR}/WebProcess/glib"
    "${WEBKIT_DIR}/WebProcess/gtk"
    "${WEBKIT_DIR}/WebProcess/soup"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/gtk"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/soup"
    "${WEBKIT_DIR}/WebProcess/WebPage/CoordinatedGraphics"
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

if (USE_LIBWEBRTC)
    list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
        "${THIRDPARTY_DIR}/libwebrtc/Source/"
        "${THIRDPARTY_DIR}/libwebrtc/Source/webrtc"
    )
endif ()

if (ENABLE_MEDIA_STREAM)
    list(APPEND WebKit_SOURCES
        UIProcess/glib/UserMediaPermissionRequestManagerProxyGLib.cpp

        WebProcess/glib/UserMediaCaptureManager.cpp
    )
    list(APPEND WebKit_MESSAGES_IN_FILES
        WebProcess/glib/UserMediaCaptureManager
    )
endif ()

GENERATE_API_HEADERS(WebKitGTK_HEADER_TEMPLATES
    ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit
    WebKitGTK_INSTALLED_HEADERS
    "-DWTF_PLATFORM_GTK=1"
    "-DWTF_PLATFORM_WPE=0"
    "-DUSE_GTK4=$<BOOL:${USE_GTK4}>"
    "-DENABLE_2022_GLIB_API=$<BOOL:${ENABLE_2022_GLIB_API}>"
)

GENERATE_API_HEADERS(WebKitWebExtension_HEADER_TEMPLATES
    ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit
    WebKitWebExtension_INSTALLED_HEADERS
    "-DWTF_PLATFORM_GTK=1"
    "-DWTF_PLATFORM_WPE=0"
    "-DUSE_GTK4=$<BOOL:${USE_GTK4}>"
    "-DENABLE_2022_GLIB_API=$<BOOL:${ENABLE_2022_GLIB_API}>"
)

if (USE_GTK4)
    set(WebKitGTK_ENUM_HEADER_TEMPLATE ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitEnumTypesGtk4.h.in)
else ()
    set(WebKitGTK_ENUM_HEADER_TEMPLATE ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitEnumTypesGtk3.h.in)
endif ()

# To generate WebKitEnumTypes.h we want to use all installed headers, except WebKitEnumTypes.h itself.
set(WebKitGTK_ENUM_GENERATION_HEADERS ${WebKitGTK_INSTALLED_HEADERS})
list(REMOVE_ITEM WebKitGTK_ENUM_GENERATION_HEADERS ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitEnumTypes.h)
add_custom_command(
    OUTPUT ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitEnumTypes.h
           ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitEnumTypes.cpp
    DEPENDS ${WebKitGTK_ENUM_GENERATION_HEADERS}

    COMMAND glib-mkenums --template ${WebKitGTK_ENUM_HEADER_TEMPLATE} ${WebKitGTK_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ | sed s/WEBKIT_TYPE_KIT/WEBKIT_TYPE/ > ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitEnumTypes.h

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/UIProcess/API/gtk/WebKitEnumTypes.cpp.in ${WebKitGTK_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ > ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitEnumTypes.cpp
    VERBATIM
)

if (USE_GTK4)
    set(WebKitGTK_WEB_PROCESS_ENUM_HEADER_TEMPLATE ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebProcessEnumTypesGtk4.h.in)
else ()
    set(WebKitGTK_WEB_PROCESS_ENUM_HEADER_TEMPLATE ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebProcessEnumTypesGtk3.h.in)
endif ()

set(WebKitGTK_WEB_PROCESS_ENUM_GENERATION_HEADERS ${WebKitWebExtension_INSTALLED_HEADERS})
list(REMOVE_ITEM WebKitGTK_WEB_PROCESS_ENUM_GENERATION_HEADERS ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitWebProcessEnumTypes.h)
add_custom_command(
    OUTPUT ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitWebProcessEnumTypes.h
           ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitWebProcessEnumTypes.cpp
    DEPENDS ${WebKitGTK_WEB_PROCESS_ENUM_GENERATION_HEADERS}

    COMMAND glib-mkenums --template ${WebKitGTK_WEB_PROCESS_ENUM_HEADER_TEMPLATE} ${WebKitGTK_WEB_PROCESS_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ | sed s/WEBKIT_TYPE_KIT/WEBKIT_TYPE/ > ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitWebProcessEnumTypes.h

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/WebKitWebProcessEnumTypes.cpp.in ${WebKitGTK_WEB_PROCESS_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ > ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitWebProcessEnumTypes.cpp
    VERBATIM
)

WEBKIT_BUILD_INSPECTOR_GRESOURCES(${WebKitGTK_DERIVED_SOURCES_DIR})

set(WebKitResources "")
list(APPEND WebKitResources "<file alias=\"css/gtk-theme.css\">gtk-theme.css</file>\n")
list(APPEND WebKitResources "<file alias=\"images/missingImage\">missingImage.png</file>\n")
list(APPEND WebKitResources "<file alias=\"images/missingImage@2x\">missingImage@2x.png</file>\n")
list(APPEND WebKitResources "<file alias=\"images/missingImage@3x\">missingImage@3x.png</file>\n")
list(APPEND WebKitResources "<file alias=\"images/panIcon\">panIcon.png</file>\n")
list(APPEND WebKitResources "<file alias=\"images/textAreaResizeCorner\">textAreaResizeCorner.png</file>\n")
list(APPEND WebKitResources "<file alias=\"images/textAreaResizeCorner@2x\">textAreaResizeCorner@2x.png</file>\n")

if (ENABLE_WEB_AUDIO)
    list(APPEND WebKitResources
        "        <file alias=\"audio/Composite\">Composite.wav</file>\n"
    )
endif ()

file(WRITE ${WebKitGTK_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.xml
    "<?xml version=1.0 encoding=UTF-8?>\n"
    "<gresources>\n"
    "    <gresource prefix=\"/org/webkitgtk/resources\">\n"
    ${WebKitResources}
    "    </gresource>\n"
    "</gresources>\n"
)

add_custom_command(
    OUTPUT ${WebKitGTK_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.c ${WebKitGTK_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.deps
    DEPENDS ${WebKitGTK_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.xml
    DEPFILE ${WebKitGTK_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.deps
    COMMAND glib-compile-resources --generate --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/Resources --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/platform/audio/resources --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebKit/Resources/gtk --target=${WebKitGTK_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.c --dependency-file=${WebKitGTK_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.deps ${WebKitGTK_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.xml
    VERBATIM
)

if (ENABLE_WAYLAND_TARGET)
    add_custom_command(
        OUTPUT ${WebKitGTK_DERIVED_SOURCES_DIR}/pointer-constraints-unstable-v1-protocol.c
        DEPENDS ${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
        COMMAND ${WAYLAND_SCANNER} private-code ${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml ${WebKitGTK_DERIVED_SOURCES_DIR}/pointer-constraints-unstable-v1-protocol.c
        COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml ${WebKitGTK_DERIVED_SOURCES_DIR}/pointer-constraints-unstable-v1-client-protocol.h
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${WebKitGTK_DERIVED_SOURCES_DIR}/relative-pointer-unstable-v1-protocol.c
        DEPENDS ${WAYLAND_PROTOCOLS_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml
        COMMAND ${WAYLAND_SCANNER} private-code ${WAYLAND_PROTOCOLS_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml ${WebKitGTK_DERIVED_SOURCES_DIR}/relative-pointer-unstable-v1-protocol.c
        COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml ${WebKitGTK_DERIVED_SOURCES_DIR}/relative-pointer-unstable-v1-client-protocol.h
        VERBATIM
    )
endif ()

# Commands for building the built-in injected bundle.
add_library(webkit${WEBKITGTK_API_INFIX}gtkinjectedbundle MODULE "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitInjectedBundleMain.cpp")
ADD_WEBKIT_PREFIX_HEADER(webkit${WEBKITGTK_API_INFIX}gtkinjectedbundle)
target_link_libraries(webkit${WEBKITGTK_API_INFIX}gtkinjectedbundle WebKit)

target_include_directories(webkit${WEBKITGTK_API_INFIX}gtkinjectedbundle PRIVATE
    $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>
    "${DERIVED_SOURCES_DIR}/InjectedBundle"
    "${WebKitGTK_FRAMEWORK_HEADERS_DIR}"
    "${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkit${WEBKITGTK_API_INFIX}gtk-${WEBKITGTK_API_VERSION}"
    "${WebKitGTK_DERIVED_SOURCES_DIR}/webkit"
)

target_include_directories(webkit${WEBKITGTK_API_INFIX}gtkinjectedbundle SYSTEM PRIVATE
    ${WebKit_SYSTEM_INCLUDE_DIRECTORIES}
)

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(webkit${WEBKITGTK_API_INFIX}gtkinjectedbundle -Wno-unused-parameter)
endif ()

# For GTK 3 builds, we have to maintain webkit2/webkit2.h and webkit2/webkit-web-extension.h for API
# compatibility. These are the only headers still installed under webkit2/. Install them manually.
if (NOT USE_GTK4)
    file(MAKE_DIRECTORY ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit2)
    add_custom_command(
        OUTPUT ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit2/webkit2.h ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit2/webkit-web-extension.h
        DEPENDS ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/webkit-web-extension.h
        COMMAND ${CMAKE_COMMAND} -E copy ${WEBKIT_DIR}/UIProcess/API/gtk/webkit2.h ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/webkit-web-extension.h ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit2/
        VERBATIM
    )

    list(APPEND WebKit_SOURCES ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit2/webkit2.h ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit2/webkit-web-extension.h)

    list(REMOVE_ITEM WebKitGTK_INSTALLED_HEADERS ${WEBKIT_DIR}/UIProcess/API/gtk/webkit2.h)
    list(REMOVE_ITEM WebKitWebExtension_INSTALLED_HEADERS ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/webkit-web-extension.h)

    install(FILES ${WEBKIT_DIR}/UIProcess/API/gtk/webkit2.h
                  ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/webkit-web-extension.h
            DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/webkit2"
    )
endif ()

install(TARGETS webkit${WEBKITGTK_API_INFIX}gtkinjectedbundle
        DESTINATION "${LIB_INSTALL_DIR}/webkit${WEBKITGTK_API_INFIX}gtk-${WEBKITGTK_API_VERSION}/injected-bundle"
)
install(FILES "${CMAKE_BINARY_DIR}/Source/WebKit/webkit${WEBKITGTK_API_INFIX}gtk-${WEBKITGTK_API_VERSION}.pc"
              "${CMAKE_BINARY_DIR}/Source/WebKit/webkit${WEBKITGTK_API_INFIX}gtk-web-extension-${WEBKITGTK_API_VERSION}.pc"
        DESTINATION "${LIB_INSTALL_DIR}/pkgconfig"
)
install(FILES ${WebKitGTK_INSTALLED_HEADERS}
              ${WebKitWebExtension_INSTALLED_HEADERS}
        DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/webkit"
)
install(FILES ${WebKitDOM_INSTALLED_HEADERS}
        DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/webkitdom"
)

add_custom_target(WebKit-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${WEBKIT_DIR} --output ${FORWARDING_HEADERS_DIR} --platform gtk --platform soup
)

# These symbolic link allows includes like #include <webkit/WebkitWebView.h>, which simulates installed headers.
add_custom_command(
    OUTPUT ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkit
    DEPENDS ${WEBKIT_DIR}/UIProcess/API/gtk
    COMMAND ln -n -s -f ${WEBKIT_DIR}/UIProcess/API/gtk ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkit
)
add_custom_command(
    OUTPUT ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkitgtk-webextension/webkit
    DEPENDS ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk
    COMMAND ln -n -s -f ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkitgtk-webextension/webkit
)
add_custom_command(
    OUTPUT ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkitgtk-webextension/webkitdom
    DEPENDS ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM
    COMMAND ln -n -s -f ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkitgtk-webextension/webkitdom
)
add_custom_target(WebKit-fake-api-headers
    DEPENDS ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkit
            ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit
            ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkitgtk-webextension/webkit
            ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkitgtk-webextension/webkitdom
)

list(APPEND WebKit_DEPENDENCIES
     WebKit-fake-api-headers
     WebKit-forwarding-headers
)

set(WEBKITGTK_SOURCES_FOR_INTROSPECTION
    UIProcess/API/gtk/WebKitColorChooserRequest.cpp
    UIProcess/API/gtk/WebKitInputMethodContextGtk.cpp
    UIProcess/API/gtk/WebKitPrintCustomWidget.cpp
    UIProcess/API/gtk/WebKitPrintOperation.cpp
    UIProcess/API/gtk/WebKitWebInspector.cpp
    UIProcess/API/gtk/WebKitWebViewGtk.cpp
)

if (USE_GTK4)
    list(APPEND WEBKITGTK_SOURCES_FOR_INTROSPECTION UIProcess/API/gtk/WebKitWebViewGtk4.cpp)
else ()
    list(APPEND WEBKITGTK_SOURCES_FOR_INTROSPECTION UIProcess/API/gtk/WebKitWebViewGtk3.cpp)
endif ()

GI_INTROSPECT(WebKit${WEBKITGTK_API_INFIX} ${WEBKITGTK_API_VERSION} webkit${WEBKITGTK_API_INFIX}/webkit${WEBKITGTK_API_INFIX}.h
    TARGET WebKit
    PACKAGE webkit${WEBKITGTK_API_INFIX}gtk
    IDENTIFIER_PREFIX WebKit
    SYMBOL_PREFIX webkit
    DEPENDENCIES
        JavaScriptCore
        Gtk-${GTK_API_VERSION}.0:${GTK_PKGCONFIG_PACKAGE}
        Soup-${SOUP_API_VERSION}:libsoup-${SOUP_API_VERSION}
    SOURCES
        ${WebKitGTK_INSTALLED_HEADERS}
        ${WEBKITGTK_SOURCES_FOR_INTROSPECTION}
        Shared/API/glib
        UIProcess/API/glib
    NO_IMPLICIT_SOURCES
)
GI_DOCGEN(WebKit${WEBKITGTK_API_INFIX} gtk/webkitgtk.toml.in
    CONTENT_TEMPLATES gtk/urlmap.js
)

GI_INTROSPECT(WebKit${WEBKITGTK_API_INFIX}WebExtension ${WEBKITGTK_API_VERSION} webkit${WEBKITGTK_API_INFIX}/webkit-web-extension.h
    TARGET WebKit
    PACKAGE webkit${WEBKITGTK_API_INFIX}gtk-web-extension
    IDENTIFIER_PREFIX WebKit
    SYMBOL_PREFIX webkit
    DEPENDENCIES
        JavaScriptCore
        Gtk-${GTK_API_VERSION}.0:${GTK_PKGCONFIG_PACKAGE}
        Soup-${SOUP_API_VERSION}:libsoup-${SOUP_API_VERSION}
    SOURCES
        ${WebKitDOM_INSTALLED_HEADERS}
        ${WebKitWebExtension_INSTALLED_HEADERS}
        ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitContextMenu.h
        ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitContextMenuActions.h
        ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitContextMenuItem.h
        ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitHitTestResult.h
        ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitUserMessage.h
        ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitURIRequest.h
        ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/WebKitURIResponse.h
        Shared/API/glib/WebKitContextMenu.cpp
        Shared/API/glib/WebKitContextMenuItem.cpp
        Shared/API/glib/WebKitHitTestResult.cpp
        Shared/API/glib/WebKitUserMessage.cpp
        Shared/API/glib/WebKitURIRequest.cpp
        Shared/API/glib/WebKitURIResponse.cpp
        WebProcess/InjectedBundle/API/glib
        WebProcess/InjectedBundle/API/glib/DOM
    NO_IMPLICIT_SOURCES
)
GI_DOCGEN(WebKit${WEBKITGTK_API_INFIX}WebExtension gtk/webkitgtk-webextension.toml.in
    CONTENT_TEMPLATES gtk/urlmap.js
)
