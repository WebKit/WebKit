find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
find_library(QUARTZ_LIBRARY Quartz)
add_definitions(-iframework ${QUARTZ_LIBRARY}/Frameworks)
add_definitions(-iframework ${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks)

list(APPEND WebKitLegacy_INCLUDE_DIRECTORIES
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}"
    "${CMAKE_SOURCE_DIR}/WebKitLibraries"
    "${WEBKITLEGACY_DIR}/mac"
    "${WEBKITLEGACY_DIR}/mac/Carbon"
    "${WEBKITLEGACY_DIR}/mac/DefaultDelegates"
    "${WEBKITLEGACY_DIR}/mac/DOM"
    "${WEBKITLEGACY_DIR}/mac/History"
    "${WEBKITLEGACY_DIR}/mac/icu"
    "${WEBKITLEGACY_DIR}/mac/Misc"
    "${WEBKITLEGACY_DIR}/mac/Panels"
    "${WEBKITLEGACY_DIR}/mac/Plugins"
    "${WEBKITLEGACY_DIR}/mac/Plugins/Hosted"
    "${WEBKITLEGACY_DIR}/mac/Storage"
    "${WEBKITLEGACY_DIR}/mac/WebCoreSupport"
    "${WEBKITLEGACY_DIR}/mac/WebInspector"
    "${WEBKITLEGACY_DIR}/mac/WebView"
)

list(APPEND WebKitLegacy_SOURCES
    cf/WebCoreSupport/WebInspectorClientCF.cpp

    mac/Carbon/CarbonUtils.m
    mac/Carbon/CarbonWindowAdapter.mm
    mac/Carbon/CarbonWindowContentView.m
    mac/Carbon/CarbonWindowFrame.m
    mac/Carbon/HIViewAdapter.m
    mac/Carbon/HIWebView.mm

    mac/DOM/DOM.mm
    mac/DOM/DOMAbstractView.mm
    mac/DOM/DOMAttr.mm
    mac/DOM/DOMBlob.mm
    mac/DOM/DOMCDATASection.mm
    mac/DOM/DOMCharacterData.mm
    mac/DOM/DOMComment.mm
    mac/DOM/DOMCounter.mm
    mac/DOM/DOMCSS.mm
    mac/DOM/DOMCSSCharsetRule.mm
    mac/DOM/DOMCSSFontFaceRule.mm
    mac/DOM/DOMCSSImportRule.mm
    mac/DOM/DOMCSSMediaRule.mm
    mac/DOM/DOMCSSPageRule.mm
    mac/DOM/DOMCSSPrimitiveValue.mm
    mac/DOM/DOMCSSRule.mm
    mac/DOM/DOMCSSRuleList.mm
    mac/DOM/DOMCSSStyleDeclaration.mm
    mac/DOM/DOMCSSStyleRule.mm
    mac/DOM/DOMCSSStyleSheet.mm
    mac/DOM/DOMCSSUnknownRule.mm
    mac/DOM/DOMCSSValue.mm
    mac/DOM/DOMCSSValueList.mm
    mac/DOM/DOMCustomXPathNSResolver.mm
    mac/DOM/DOMDocument.mm
    mac/DOM/DOMDocumentFragment.mm
    mac/DOM/DOMDocumentType.mm
    mac/DOM/DOMElement.mm
    mac/DOM/DOMEntityReference.mm
    mac/DOM/DOMEvent.mm
    mac/DOM/DOMEvents.mm
    mac/DOM/DOMFile.mm
    mac/DOM/DOMFileList.mm
    mac/DOM/DOMHTML.mm
    mac/DOM/DOMHTMLAnchorElement.mm
    mac/DOM/DOMHTMLAppletElement.mm
    mac/DOM/DOMHTMLAreaElement.mm
    mac/DOM/DOMHTMLBRElement.mm
    mac/DOM/DOMHTMLBaseElement.mm
    mac/DOM/DOMHTMLBaseFontElement.mm
    mac/DOM/DOMHTMLBodyElement.mm
    mac/DOM/DOMHTMLButtonElement.mm
    mac/DOM/DOMHTMLCanvasElement.mm
    mac/DOM/DOMHTMLCollection.mm
    mac/DOM/DOMHTMLDListElement.mm
    mac/DOM/DOMHTMLDirectoryElement.mm
    mac/DOM/DOMHTMLDivElement.mm
    mac/DOM/DOMHTMLDocument.mm
    mac/DOM/DOMHTMLElement.mm
    mac/DOM/DOMHTMLEmbedElement.mm
    mac/DOM/DOMHTMLFieldSetElement.mm
    mac/DOM/DOMHTMLFontElement.mm
    mac/DOM/DOMHTMLFormElement.mm
    mac/DOM/DOMHTMLFrameElement.mm
    mac/DOM/DOMHTMLFrameSetElement.mm
    mac/DOM/DOMHTMLHRElement.mm
    mac/DOM/DOMHTMLHeadElement.mm
    mac/DOM/DOMHTMLHeadingElement.mm
    mac/DOM/DOMHTMLHtmlElement.mm
    mac/DOM/DOMHTMLIFrameElement.mm
    mac/DOM/DOMHTMLImageElement.mm
    mac/DOM/DOMHTMLInputElement.mm
    mac/DOM/DOMHTMLLIElement.mm
    mac/DOM/DOMHTMLLabelElement.mm
    mac/DOM/DOMHTMLLegendElement.mm
    mac/DOM/DOMHTMLLinkElement.mm
    mac/DOM/DOMHTMLMapElement.mm
    mac/DOM/DOMHTMLMarqueeElement.mm
    mac/DOM/DOMHTMLMediaElement.mm
    mac/DOM/DOMHTMLMenuElement.mm
    mac/DOM/DOMHTMLMetaElement.mm
    mac/DOM/DOMHTMLModElement.mm
    mac/DOM/DOMHTMLOListElement.mm
    mac/DOM/DOMHTMLObjectElement.mm
    mac/DOM/DOMHTMLOptGroupElement.mm
    mac/DOM/DOMHTMLOptionElement.mm
    mac/DOM/DOMHTMLOptionsCollection.mm
    mac/DOM/DOMHTMLParagraphElement.mm
    mac/DOM/DOMHTMLParamElement.mm
    mac/DOM/DOMHTMLPreElement.mm
    mac/DOM/DOMHTMLQuoteElement.mm
    mac/DOM/DOMHTMLScriptElement.mm
    mac/DOM/DOMHTMLSelectElement.mm
    mac/DOM/DOMHTMLStyleElement.mm
    mac/DOM/DOMHTMLTableCaptionElement.mm
    mac/DOM/DOMHTMLTableCellElement.mm
    mac/DOM/DOMHTMLTableColElement.mm
    mac/DOM/DOMHTMLTableElement.mm
    mac/DOM/DOMHTMLTableRowElement.mm
    mac/DOM/DOMHTMLTableSectionElement.mm
    mac/DOM/DOMHTMLTextAreaElement.mm
    mac/DOM/DOMHTMLTitleElement.mm
    mac/DOM/DOMHTMLUListElement.mm
    mac/DOM/DOMHTMLVideoElement.mm
    mac/DOM/DOMInternal.mm
    mac/DOM/DOMImplementation.mm
    mac/DOM/DOMKeyboardEvent.mm
    mac/DOM/DOMMediaError.mm
    mac/DOM/DOMMediaList.mm
    mac/DOM/DOMMouseEvent.mm
    mac/DOM/DOMMutationEvent.mm
    mac/DOM/DOMNamedNodeMap.mm
    mac/DOM/DOMNode.mm
    mac/DOM/DOMNodeIterator.mm
    mac/DOM/DOMNodeList.mm
    mac/DOM/DOMObject.mm
    mac/DOM/DOMOverflowEvent.mm
    mac/DOM/DOMProcessingInstruction.mm
    mac/DOM/DOMProgressEvent.mm
    mac/DOM/DOMRGBColor.mm
    mac/DOM/DOMRange.mm
    mac/DOM/DOMRect.mm
    mac/DOM/DOMStyleSheet.mm
    mac/DOM/DOMStyleSheetList.mm
    mac/DOM/DOMText.mm
    mac/DOM/DOMTextEvent.mm
    mac/DOM/DOMTimeRanges.mm
    mac/DOM/DOMTokenList.mm
    mac/DOM/DOMTreeWalker.mm
    mac/DOM/DOMUIEvent.mm
    mac/DOM/DOMUIKitExtensions.mm
    mac/DOM/DOMUtility.mm
    mac/DOM/DOMWheelEvent.mm
    mac/DOM/DOMXPath.mm
    mac/DOM/DOMXPathExpression.mm
    mac/DOM/DOMXPathResult.mm
    mac/DOM/ExceptionHandlers.mm
    mac/DOM/ObjCEventListener.mm
    mac/DOM/ObjCNodeFilterCondition.mm

    mac/DefaultDelegates/WebDefaultContextMenuDelegate.mm
    mac/DefaultDelegates/WebDefaultEditingDelegate.m
    mac/DefaultDelegates/WebDefaultPolicyDelegate.m
    mac/DefaultDelegates/WebDefaultUIDelegate.mm

    mac/History/BackForwardList.mm
    mac/History/BinaryPropertyList.cpp
    mac/History/HistoryPropertyList.mm
    mac/History/WebBackForwardList.mm
    mac/History/WebHistory.mm
    mac/History/WebHistoryItem.mm
    mac/History/WebURLsWithTitles.m

    mac/Misc/WebCache.mm
    mac/Misc/WebCoreStatistics.mm
    mac/Misc/WebDownload.mm
    mac/Misc/WebElementDictionary.mm
    mac/Misc/WebIconDatabase.mm
    mac/Misc/WebKitErrors.m
    mac/Misc/WebKitLogging.m
    mac/Misc/WebKitNSStringExtras.mm
    mac/Misc/WebKitStatistics.m
    mac/Misc/WebKitVersionChecks.mm
    mac/Misc/WebLocalizableStrings.mm
    mac/Misc/WebLocalizableStringsInternal.mm
    mac/Misc/WebNSControlExtras.m
    mac/Misc/WebNSDataExtras.mm
    mac/Misc/WebNSDictionaryExtras.m
    mac/Misc/WebNSEventExtras.m
    mac/Misc/WebNSFileManagerExtras.mm
    mac/Misc/WebNSImageExtras.m
    mac/Misc/WebNSObjectExtras.mm
    mac/Misc/WebNSPasteboardExtras.mm
    mac/Misc/WebNSPrintOperationExtras.m
    mac/Misc/WebNSURLExtras.mm
    mac/Misc/WebNSURLRequestExtras.m
    mac/Misc/WebNSUserDefaultsExtras.mm
    mac/Misc/WebNSViewExtras.m
    mac/Misc/WebNSWindowExtras.m
    mac/Misc/WebSharingServicePickerController.mm
    mac/Misc/WebStringTruncator.mm
    mac/Misc/WebUserContentURLPattern.mm

    mac/Panels/WebAuthenticationPanel.m
    mac/Panels/WebPanelAuthenticationHandler.m

    mac/Plugins/WebBaseNetscapePluginView.mm
    mac/Plugins/WebBasePluginPackage.mm
    mac/Plugins/WebNetscapePluginEventHandler.mm
    mac/Plugins/WebNetscapePluginEventHandlerCarbon.mm
    mac/Plugins/WebNetscapePluginEventHandlerCocoa.mm
    mac/Plugins/WebNetscapePluginPackage.mm
    mac/Plugins/WebNetscapePluginStream.mm
    mac/Plugins/WebNetscapePluginView.mm
    mac/Plugins/WebPluginContainerCheck.mm
    mac/Plugins/WebPluginController.mm
    mac/Plugins/WebPluginDatabase.mm
    mac/Plugins/WebPluginPackage.mm
    mac/Plugins/WebPluginRequest.m
    mac/Plugins/npapi.mm

    mac/Plugins/Hosted/HostedNetscapePluginStream.mm
    mac/Plugins/Hosted/NetscapePluginHostManager.mm
    mac/Plugins/Hosted/NetscapePluginHostProxy.mm
    mac/Plugins/Hosted/NetscapePluginInstanceProxy.mm
    mac/Plugins/Hosted/ProxyInstance.mm
    mac/Plugins/Hosted/ProxyRuntimeObject.mm
    mac/Plugins/Hosted/WebHostedNetscapePluginView.mm
    mac/Plugins/Hosted/WebKitPluginAgent.defs
    mac/Plugins/Hosted/WebKitPluginAgentReply.defs
    mac/Plugins/Hosted/WebKitPluginClient.defs
    mac/Plugins/Hosted/WebKitPluginHost.defs
    mac/Plugins/Hosted/WebKitPluginHostTypes.defs
    mac/Plugins/Hosted/WebTextInputWindowController.m

    mac/Storage/WebDatabaseManager.mm
    mac/Storage/WebDatabaseManagerClient.mm
    mac/Storage/WebDatabaseProvider.mm
    mac/Storage/WebDatabaseQuotaManager.mm
    mac/Storage/WebStorageManager.mm
    mac/Storage/WebStorageTrackerClient.mm

    mac/WebCoreSupport/CorrectionPanel.mm
    mac/WebCoreSupport/PopupMenuMac.mm
    mac/WebCoreSupport/SearchPopupMenuMac.mm
    mac/WebCoreSupport/WebAlternativeTextClient.mm
    mac/WebCoreSupport/WebApplicationCache.mm
    mac/WebCoreSupport/WebApplicationCacheQuotaManager.mm
    mac/WebCoreSupport/WebChromeClient.mm
    mac/WebCoreSupport/WebContextMenuClient.mm
    mac/WebCoreSupport/WebDeviceOrientationClient.mm
    mac/WebCoreSupport/WebDragClient.mm
    mac/WebCoreSupport/WebEditorClient.mm
    mac/WebCoreSupport/WebFrameLoaderClient.mm
    mac/WebCoreSupport/WebFrameNetworkingContext.mm
    mac/WebCoreSupport/WebGeolocationClient.mm
    mac/WebCoreSupport/WebInspectorClient.mm
    mac/WebCoreSupport/WebJavaScriptTextInputPanel.m
    mac/WebCoreSupport/WebKitFullScreenListener.mm
    mac/WebCoreSupport/WebNotificationClient.mm
    mac/WebCoreSupport/WebOpenPanelResultListener.mm
    mac/WebCoreSupport/WebPlatformStrategies.mm
    mac/WebCoreSupport/WebPluginInfoProvider.mm
    mac/WebCoreSupport/WebProgressTrackerClient.mm
    mac/WebCoreSupport/WebSecurityOrigin.mm
    mac/WebCoreSupport/WebSelectionServiceController.mm
    mac/WebCoreSupport/WebValidationMessageClient.mm
    mac/WebCoreSupport/WebVisitedLinkStore.mm

    mac/WebInspector/WebInspector.mm
    mac/WebInspector/WebInspectorFrontend.mm
    mac/WebInspector/WebNodeHighlight.mm
    mac/WebInspector/WebNodeHighlightView.mm
    mac/WebInspector/WebNodeHighlighter.mm

    mac/WebView/WebArchive.mm
    mac/WebView/WebClipView.mm
    mac/WebView/WebDashboardRegion.mm
    mac/WebView/WebDataSource.mm
    mac/WebView/WebDelegateImplementationCaching.mm
    mac/WebView/WebDeviceOrientation.mm
    mac/WebView/WebDeviceOrientationProviderMock.mm
    mac/WebView/WebDocumentLoaderMac.mm
    mac/WebView/WebDynamicScrollBarsView.mm
    mac/WebView/WebFormDelegate.m
    mac/WebView/WebFrame.mm
    mac/WebView/WebFrameView.mm
    mac/WebView/WebFullScreenController.mm
    mac/WebView/WebGeolocationPosition.mm
    mac/WebView/WebHTMLRepresentation.mm
    mac/WebView/WebHTMLView.mm
    mac/WebView/WebImmediateActionController.mm
    mac/WebView/WebIndicateLayer.mm
    mac/WebView/WebJSPDFDoc.mm
    mac/WebView/WebNavigationData.mm
    mac/WebView/WebNotification.mm
    mac/WebView/WebPDFDocumentExtras.mm
    mac/WebView/WebPDFRepresentation.mm
    mac/WebView/WebPDFView.mm
    mac/WebView/WebPolicyDelegate.mm
    mac/WebView/WebPreferences.mm
    mac/WebView/WebResource.mm
    mac/WebView/WebScriptDebugDelegate.mm
    mac/WebView/WebScriptDebugger.mm
    mac/WebView/WebScriptWorld.mm
    mac/WebView/WebTextCompletionController.mm
    mac/WebView/WebTextIterator.mm
    mac/WebView/WebView.mm
    mac/WebView/WebViewData.mm
)

set(WebKitLegacy_FORWARDING_HEADERS_DIRECTORIES
    mac/DOM
    mac/DefaultDelegates
    mac/History
    mac/Misc
    mac/Panels
    mac/Plugins
    mac/Storage
    mac/WebCoreSupport
    mac/WebInspector
    mac/WebView
)

set(WebKitLegacy_FORWARDING_HEADERS_FILES
    mac/DOM/WebDOMOperations.h

    mac/History/WebHistory.h
    mac/History/WebHistoryItem.h

    mac/Misc/WebNSURLExtras.h

    mac/Panels/WebPanelAuthenticationHandler.h

    mac/Plugins/WebBasePluginPackage.h

    mac/Storage/WebDatabaseManagerPrivate.h

    mac/WebCoreSupport/WebKeyGenerator.h

    mac/WebInspector/WebInspector.h

    mac/WebView/WebFrame.h
    mac/WebView/WebView.h

    ${WEBCORE_DIR}/plugins/npfunctions.h
)

add_definitions("-include WebKitPrefix.h")

set(C99_FILES
    ${WEBKITLEGACY_DIR}/mac/Carbon/CarbonUtils.m
    ${WEBKITLEGACY_DIR}/mac/Carbon/CarbonWindowContentView.m
    ${WEBKITLEGACY_DIR}/mac/Carbon/CarbonWindowFrame.m
    ${WEBKITLEGACY_DIR}/mac/Carbon/HIViewAdapter.m

    mac/DefaultDelegates/WebDefaultEditingDelegate.m
    mac/DefaultDelegates/WebDefaultPolicyDelegate.m
    mac/DefaultDelegates/WebDefaultUIDelegate.m

    mac/Misc/WebKitErrors.m
    mac/Misc/WebKitLogging.m
    mac/Misc/WebKitStatistics.m
    mac/Misc/WebKitSystemBits.m
    mac/Misc/WebNSArrayExtras.m
    mac/Misc/WebNSControlExtras.m
    mac/Misc/WebNSDictionaryExtras.m
    mac/Misc/WebNSEventExtras.m
    mac/Misc/WebNSImageExtras.m
    mac/Misc/WebNSPrintOperationExtras.m
    mac/Misc/WebNSURLRequestExtras.m
    mac/Misc/WebNSViewExtras.m
    mac/Misc/WebNSWindowExtras.m

    mac/Panels/WebAuthenticationPanel.m
    mac/Panels/WebPanelAuthenticationHandler.m

    mac/Plugins/WebPluginRequest.m
    mac/Plugins/WebPluginsPrivate.m

    mac/Plugins/Hosted/WebTextInputWindowController.m

    mac/WebCoreSupport/WebJavaScriptTextInputPanel.m

    mac/WebView/WebFormDelegate.m
)

foreach (_file ${WebKitLegacy_SOURCES})
    list(FIND C99_FILES ${_file} _c99_index)
    if (${_c99_index} EQUAL -1)
        set_source_files_properties(${_file} PROPERTIES COMPILE_FLAGS "-ObjC++ -std=c++17")
    else ()
        set_source_files_properties(${_file} PROPERTIES COMPILE_FLAGS -std=c99)
    endif ()
endforeach ()

file(COPY
    mac/Plugins/Hosted/WebKitPluginAgent.defs
    mac/Plugins/Hosted/WebKitPluginAgentReply.defs
    mac/Plugins/Hosted/WebKitPluginClient.defs
    mac/Plugins/Hosted/WebKitPluginHost.defs
    mac/Plugins/Hosted/WebKitPluginHostTypes.defs
    mac/Plugins/Hosted/WebKitPluginHostTypes.h
DESTINATION ${DERIVED_SOURCES_WEBKITLEGACY_DIR})

add_custom_command(
    OUTPUT
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginAgentReplyServer.c
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginAgentReplyUser.c
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginAgentServer.c
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginAgentUser.c
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginHostServer.c
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginHostUser.c
    DEPENDS mac/Plugins/Hosted/WebKitPluginAgent.defs mac/Plugins/Hosted/WebKitPluginHost.defs
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBKITLEGACY_DIR}
    COMMAND mig -I.. WebKitPluginAgent.defs WebKitPluginAgentReply.defs WebKitPluginHost.defs
    VERBATIM)
add_custom_command(
    OUTPUT
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginClientServer.c
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginClientUser.c
    DEPENDS mac/Plugins/Hosted/WebKitPluginClient.defs
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBKITLEGACY_DIR}
    COMMAND mig -I.. -sheader WebKitPluginClientServer.h WebKitPluginClient.defs
    VERBATIM)
list(APPEND WebKitLegacy_SOURCES
    ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginAgentUser.c
    ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginClientServer.c
    ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginHostUser.c
)

WEBKIT_CREATE_FORWARDING_HEADERS(WebKitLegacy DIRECTORIES ${WebKitLegacy_FORWARDING_HEADERS_DIRECTORIES} FILES ${WebKitLegacy_FORWARDING_HEADERS_FILES})

# FIXME: Forwarding headers should be copies of actual headers.
file(GLOB ObjCHeaders ${WEBCORE_DIR}/plugins/*.h)
list(APPEND ObjCHeaders
    WebKitAvailability.h
    WebScriptObject.h
)
foreach (_file ${ObjCHeaders})
    get_filename_component(_name ${_file} NAME)
    if (NOT EXISTS ${FORWARDING_HEADERS_DIR}/WebKitLegacy/${_name})
        file(WRITE ${FORWARDING_HEADERS_DIR}/WebKitLegacy/${_name} "#import <WebCore/${_name}>")
    endif ()
endforeach ()

set(WebKitLegacy_OUTPUT_NAME WebKitLegacy)

set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} "-compatibility_version 1 -current_version ${WEBKIT_MAC_VERSION}")
