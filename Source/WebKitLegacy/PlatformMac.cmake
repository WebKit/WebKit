include(PlatformCocoa.cmake)

find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
find_library(QUARTZ_LIBRARY Quartz)
find_library(SECURITYINTERFACE_LIBRARY SecurityInterface)
add_definitions(-iframework ${QUARTZ_LIBRARY}/Frameworks)
add_definitions(-iframework ${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks)

list(APPEND WebKitLegacy_PRIVATE_LIBRARIES
    ${SECURITYINTERFACE_LIBRARY}
)

list(APPEND WebKitLegacy_SOURCES
    mac/DefaultDelegates/WebDefaultEditingDelegate.m

    mac/History/WebURLsWithTitles.m

    mac/Misc/WebKitErrors.m
    mac/Misc/WebKitLogging.m
    mac/Misc/WebKitStatistics.m
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

    mac/WebCoreSupport/WebJavaScriptTextInputPanel.m

    mac/WebView/WebFeature.m
    mac/WebView/WebFormDelegate.m
)


# WebKit reexports WebKitLegacy, so the legacy ObjC API is part of WebKit's
# API.
set(WebKitLegacy_FORWARDED_PUBLIC_HEADERS
    ${WEBCORE_DIR}/bridge/objc/WebScriptObject.h
    ${WEBCORE_DIR}/platform/cocoa/WebKitAvailability.h
    mac/DOM/DOM.h
    mac/DOM/DOMAbstractView.h
    mac/DOM/DOMAttr.h
    mac/DOM/DOMBlob.h
    mac/DOM/DOMCDATASection.h
    mac/DOM/DOMCSS.h
    mac/DOM/DOMCSSCharsetRule.h
    mac/DOM/DOMCSSFontFaceRule.h
    mac/DOM/DOMCSSImportRule.h
    mac/DOM/DOMCSSMediaRule.h
    mac/DOM/DOMCSSPageRule.h
    mac/DOM/DOMCSSPrimitiveValue.h
    mac/DOM/DOMCSSRule.h
    mac/DOM/DOMCSSRuleList.h
    mac/DOM/DOMCSSStyleDeclaration.h
    mac/DOM/DOMCSSStyleRule.h
    mac/DOM/DOMCSSStyleSheet.h
    mac/DOM/DOMCSSUnknownRule.h
    mac/DOM/DOMCSSValue.h
    mac/DOM/DOMCSSValueList.h
    mac/DOM/DOMCharacterData.h
    mac/DOM/DOMComment.h
    mac/DOM/DOMCore.h
    mac/DOM/DOMCounter.h
    mac/DOM/DOMDocument.h
    mac/DOM/DOMDocumentFragment.h
    mac/DOM/DOMDocumentType.h
    mac/DOM/DOMElement.h
    mac/DOM/DOMEntity.h
    mac/DOM/DOMEntityReference.h
    mac/DOM/DOMEvent.h
    mac/DOM/DOMEventException.h
    mac/DOM/DOMEventListener.h
    mac/DOM/DOMEventTarget.h
    mac/DOM/DOMEvents.h
    mac/DOM/DOMException.h
    mac/DOM/DOMExtensions.h
    mac/DOM/DOMFile.h
    mac/DOM/DOMFileList.h
    mac/DOM/DOMHTML.h
    mac/DOM/DOMHTMLAnchorElement.h
    mac/DOM/DOMHTMLAppletElement.h
    mac/DOM/DOMHTMLAreaElement.h
    mac/DOM/DOMHTMLBRElement.h
    mac/DOM/DOMHTMLBaseElement.h
    mac/DOM/DOMHTMLBaseFontElement.h
    mac/DOM/DOMHTMLBodyElement.h
    mac/DOM/DOMHTMLButtonElement.h
    mac/DOM/DOMHTMLCollection.h
    mac/DOM/DOMHTMLDListElement.h
    mac/DOM/DOMHTMLDirectoryElement.h
    mac/DOM/DOMHTMLDivElement.h
    mac/DOM/DOMHTMLDocument.h
    mac/DOM/DOMHTMLElement.h
    mac/DOM/DOMHTMLEmbedElement.h
    mac/DOM/DOMHTMLFieldSetElement.h
    mac/DOM/DOMHTMLFontElement.h
    mac/DOM/DOMHTMLFormElement.h
    mac/DOM/DOMHTMLFrameElement.h
    mac/DOM/DOMHTMLFrameSetElement.h
    mac/DOM/DOMHTMLHRElement.h
    mac/DOM/DOMHTMLHeadElement.h
    mac/DOM/DOMHTMLHeadingElement.h
    mac/DOM/DOMHTMLHtmlElement.h
    mac/DOM/DOMHTMLIFrameElement.h
    mac/DOM/DOMHTMLImageElement.h
    mac/DOM/DOMHTMLInputElement.h
    mac/DOM/DOMHTMLLIElement.h
    mac/DOM/DOMHTMLLabelElement.h
    mac/DOM/DOMHTMLLegendElement.h
    mac/DOM/DOMHTMLLinkElement.h
    mac/DOM/DOMHTMLMapElement.h
    mac/DOM/DOMHTMLMarqueeElement.h
    mac/DOM/DOMHTMLMenuElement.h
    mac/DOM/DOMHTMLMetaElement.h
    mac/DOM/DOMHTMLModElement.h
    mac/DOM/DOMHTMLOListElement.h
    mac/DOM/DOMHTMLObjectElement.h
    mac/DOM/DOMHTMLOptGroupElement.h
    mac/DOM/DOMHTMLOptionElement.h
    mac/DOM/DOMHTMLOptionsCollection.h
    mac/DOM/DOMHTMLParagraphElement.h
    mac/DOM/DOMHTMLParamElement.h
    mac/DOM/DOMHTMLPreElement.h
    mac/DOM/DOMHTMLQuoteElement.h
    mac/DOM/DOMHTMLScriptElement.h
    mac/DOM/DOMHTMLSelectElement.h
    mac/DOM/DOMHTMLStyleElement.h
    mac/DOM/DOMHTMLTableCaptionElement.h
    mac/DOM/DOMHTMLTableCellElement.h
    mac/DOM/DOMHTMLTableColElement.h
    mac/DOM/DOMHTMLTableElement.h
    mac/DOM/DOMHTMLTableRowElement.h
    mac/DOM/DOMHTMLTableSectionElement.h
    mac/DOM/DOMHTMLTextAreaElement.h
    mac/DOM/DOMHTMLTitleElement.h
    mac/DOM/DOMHTMLUListElement.h
    mac/DOM/DOMImplementation.h
    mac/DOM/DOMKeyboardEvent.h
    mac/DOM/DOMMediaList.h
    mac/DOM/DOMMouseEvent.h
    mac/DOM/DOMMutationEvent.h
    mac/DOM/DOMNamedNodeMap.h
    mac/DOM/DOMNode.h
    mac/DOM/DOMNodeFilter.h
    mac/DOM/DOMNodeIterator.h
    mac/DOM/DOMNodeList.h
    mac/DOM/DOMObject.h
    mac/DOM/DOMOverflowEvent.h
    mac/DOM/DOMProcessingInstruction.h
    mac/DOM/DOMProgressEvent.h
    mac/DOM/DOMRGBColor.h
    mac/DOM/DOMRange.h
    mac/DOM/DOMRangeException.h
    mac/DOM/DOMRanges.h
    mac/DOM/DOMRect.h
    mac/DOM/DOMStyleSheet.h
    mac/DOM/DOMStyleSheetList.h
    mac/DOM/DOMStylesheets.h
    mac/DOM/DOMText.h
    mac/DOM/DOMTraversal.h
    mac/DOM/DOMTreeWalker.h
    mac/DOM/DOMUIEvent.h
    mac/DOM/DOMViews.h
    mac/DOM/DOMWheelEvent.h
    mac/DOM/DOMXPath.h
    mac/DOM/DOMXPathException.h
    mac/DOM/DOMXPathExpression.h
    mac/DOM/DOMXPathNSResolver.h
    mac/DOM/DOMXPathResult.h
    mac/DOM/WebDOMOperations.h

    mac/History/WebBackForwardList.h
    mac/History/WebHistory.h
    mac/History/WebHistoryItem.h

    mac/Misc/WebDownload.h
    mac/Misc/WebKitErrors.h

    mac/Plugins/WebPlugin.h
    mac/Plugins/WebPluginContainer.h
    mac/Plugins/WebPluginViewFactory.h

    mac/WebView/WebArchive.h
    mac/WebView/WebDataSource.h
    mac/WebView/WebDocument.h
    mac/WebView/WebEditingDelegate.h
    mac/WebView/WebFrame.h
    mac/WebView/WebFrameLoadDelegate.h
    mac/WebView/WebFrameView.h
    mac/WebView/WebPolicyDelegate.h
    mac/WebView/WebPreferences.h
    mac/WebView/WebResource.h
    mac/WebView/WebResourceLoadDelegate.h
    mac/WebView/WebUIDelegate.h
    mac/WebView/WebView.h
)

set(WebKitLegacy_FORWARDED_PRIVATE_HEADERS
    mac/DOM/DOMDocumentFragmentPrivate.h
    mac/DOM/DOMDocumentPrivate.h
    mac/DOM/DOMHTMLCanvasElement.h
    mac/DOM/DOMHTMLElementPrivate.h
    mac/DOM/DOMHTMLInputElementPrivate.h
    mac/DOM/DOMHTMLMediaElement.h
    mac/DOM/DOMHTMLVideoElement.h
    mac/DOM/DOMMediaError.h
    mac/DOM/DOMNodePrivate.h
    mac/DOM/DOMPrivate.h
    mac/DOM/DOMTextEvent.h
    mac/DOM/DOMTimeRanges.h
    mac/DOM/WebAutocapitalizeTypes.h
    mac/DOM/WebDOMOperationsPrivate.h

    mac/DefaultDelegates/WebDefaultPolicyDelegate.h

    mac/History/WebBackForwardListPrivate.h
    mac/History/WebHistoryItemPrivate.h
    mac/History/WebHistoryPrivate.h
    mac/History/WebURLsWithTitles.h

    mac/Misc/WebCache.h
    mac/Misc/WebCoreStatistics.h
    mac/Misc/WebIconDatabase.h
    mac/Misc/WebKitErrorsPrivate.h
    mac/Misc/WebKitNSStringExtras.h
    mac/Misc/WebKitStatistics.h
    mac/Misc/WebLocalizableStrings.h
    mac/Misc/WebNSEventExtras.h
    mac/Misc/WebNSFileManagerExtras.h
    mac/Misc/WebNSPasteboardExtras.h
    mac/Misc/WebNSURLExtras.h
    mac/Misc/WebNSUserDefaultsExtras.h
    mac/Misc/WebNSViewExtras.h
    mac/Misc/WebNSWindowExtras.h
    mac/Misc/WebQuotaManager.h
    mac/Misc/WebStringTruncator.h
    mac/Misc/WebUserContentURLPattern.h

    mac/Panels/WebPanelAuthenticationHandler.h

    mac/Plugins/WebPluginContainerPrivate.h
    mac/Plugins/WebPluginDatabase.h
    mac/Plugins/WebPluginPackagePrivate.h
    mac/Plugins/WebPluginViewFactoryPrivate.h

    mac/Storage/WebDatabaseManagerPrivate.h
    mac/Storage/WebDatabaseQuotaManager.h
    mac/Storage/WebStorageManagerPrivate.h

    mac/WebCoreSupport/WebCreateFragmentInternal.h
    mac/WebCoreSupport/WebJavaScriptTextInputPanel.h
    mac/WebCoreSupport/WebSecurityOriginPrivate.h

    mac/WebInspector/WebInspector.h
    mac/WebInspector/WebInspectorPrivate.h

    mac/WebView/WebAllowDenyPolicyListener.h
    mac/WebView/WebDataSourcePrivate.h
    mac/WebView/WebDeviceOrientation.h
    mac/WebView/WebDeviceOrientationProvider.h
    mac/WebView/WebDeviceOrientationProviderMock.h
    mac/WebView/WebDocumentPrivate.h
    mac/WebView/WebDynamicScrollBarsView.h
    mac/WebView/WebEditingDelegatePrivate.h
    mac/WebView/WebFeature.h
    mac/WebView/WebFormDelegate.h
    mac/WebView/WebFormDelegatePrivate.h
    mac/WebView/WebFrameLoadDelegatePrivate.h
    mac/WebView/WebFramePrivate.h
    mac/WebView/WebFrameViewPrivate.h
    mac/WebView/WebGeolocationPosition.h
    mac/WebView/WebHTMLRepresentation.h
    mac/WebView/WebHTMLRepresentationPrivate.h
    mac/WebView/WebHTMLView.h
    mac/WebView/WebHTMLViewPrivate.h
    mac/WebView/WebNavigationData.h
    mac/WebView/WebNotification.h
    mac/WebView/WebPolicyDelegatePrivate.h
    mac/WebView/WebPreferenceKeysPrivate.h
    mac/WebView/WebPreferencesPrivate.h
    mac/WebView/WebResourceLoadDelegatePrivate.h
    mac/WebView/WebResourcePrivate.h
    mac/WebView/WebScriptDebugDelegate.h
    mac/WebView/WebScriptWorld.h
    mac/WebView/WebTextIterator.h
    mac/WebView/WebUIDelegatePrivate.h
    mac/WebView/WebViewPrivate.h
)

# Make the above also available from <WebKitLegacy/X.h> imports.
set(WebKitLegacy_PRIVATE_FRAMEWORK_HEADERS
    ${WebKitLegacy_FORWARDED_PUBLIC_HEADERS}
    ${WebKitLegacy_FORWARDED_PRIVATE_HEADERS})

list(APPEND WebKitLegacy_INTERFACE_DEPENDENCIES WebKitLegacy_ForwardHeaders WebKitLegacy_ForwardPrivateHeaders)

WEBKIT_COPY_FILES(WebKitLegacy_ForwardHeaders
    DESTINATION ${WebKit_HEADERS_DIR}
    FILES ${WebKitLegacy_FORWARDED_PUBLIC_HEADERS}
    COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/forward-headers-cmake.pl
    FLATTENED
)
WEBKIT_COPY_FILES(WebKitLegacy_ForwardPrivateHeaders
    DESTINATION ${WebKit_PRIVATE_HEADERS_DIR}
    FILES ${WebKitLegacy_FORWARDED_PRIVATE_HEADERS}
    COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/forward-headers-cmake.pl
    FLATTENED
)

# FIXME: Eventually add all non-installed headers to this list, so that the
# per-directory -I paths can be removed.
set(WebKitLegacy_PROJECT_HEADERS
    Storage/InProcessIDBServer.h
    Storage/StorageAreaImpl.h
    Storage/StorageAreaSync.h
    Storage/StorageNamespaceImpl.h
    Storage/StorageSyncManager.h
    Storage/StorageThread.h
    Storage/StorageTracker.h
    Storage/StorageTrackerClient.h
    Storage/WebDatabaseProvider.h
    Storage/WebStorageNamespaceProvider.h

    mac/DOM/DOMAbstractViewFrame.h
    mac/DOM/DOMAbstractViewInternal.h
    mac/DOM/DOMAttrInternal.h
    mac/DOM/DOMBlobInternal.h
    mac/DOM/DOMCDATASectionInternal.h
    mac/DOM/DOMCSSPrimitiveValueInternal.h
    mac/DOM/DOMCSSRuleInternal.h
    mac/DOM/DOMCSSRuleListInternal.h
    mac/DOM/DOMCSSStyleDeclarationInternal.h
    mac/DOM/DOMCSSStyleSheetInternal.h
    mac/DOM/DOMCSSValueInternal.h
    mac/DOM/DOMCommentInternal.h
    mac/DOM/DOMCounterInternal.h
    mac/DOM/DOMCustomXPathNSResolver.h
    mac/DOM/DOMDocumentFragmentInternal.h
    mac/DOM/DOMDocumentInternal.h
    mac/DOM/DOMDocumentTypeInternal.h
    mac/DOM/DOMElementInternal.h
    mac/DOM/DOMEventInternal.h
    mac/DOM/DOMFileInternal.h
    mac/DOM/DOMFileListInternal.h
    mac/DOM/DOMHTMLAreaElementInternal.h
    mac/DOM/DOMHTMLCollectionInternal.h
    mac/DOM/DOMHTMLDocumentInternal.h
    mac/DOM/DOMHTMLElementInternal.h
    mac/DOM/DOMHTMLFormElementInternal.h
    mac/DOM/DOMHTMLHeadElementInternal.h
    mac/DOM/DOMHTMLImageElementInternal.h
    mac/DOM/DOMHTMLInputElementInternal.h
    mac/DOM/DOMHTMLLinkElementInternal.h
    mac/DOM/DOMHTMLOptionElementInternal.h
    mac/DOM/DOMHTMLOptionsCollectionInternal.h
    mac/DOM/DOMHTMLScriptElementInternal.h
    mac/DOM/DOMHTMLSelectElementInternal.h
    mac/DOM/DOMHTMLStyleElementInternal.h
    mac/DOM/DOMHTMLTableCaptionElementInternal.h
    mac/DOM/DOMHTMLTableCellElementInternal.h
    mac/DOM/DOMHTMLTableColElementInternal.h
    mac/DOM/DOMHTMLTableSectionElementInternal.h
    mac/DOM/DOMHTMLTextAreaElementInternal.h
    mac/DOM/DOMImplementationInternal.h
    mac/DOM/DOMInternal.h
    mac/DOM/DOMMediaErrorInternal.h
    mac/DOM/DOMMediaListInternal.h
    mac/DOM/DOMNamedNodeMapInternal.h
    mac/DOM/DOMNodeInternal.h
    mac/DOM/DOMNodeIteratorInternal.h
    mac/DOM/DOMNodeListInternal.h
    mac/DOM/DOMProcessingInstructionInternal.h
    mac/DOM/DOMRGBColorInternal.h
    mac/DOM/DOMRangeInternal.h
    mac/DOM/DOMRectInternal.h
    mac/DOM/DOMStyleSheetInternal.h
    mac/DOM/DOMStyleSheetListInternal.h
    mac/DOM/DOMTextInternal.h
    mac/DOM/DOMTimeRangesInternal.h
    mac/DOM/DOMTokenList.h
    mac/DOM/DOMTokenListInternal.h
    mac/DOM/DOMTreeWalkerInternal.h
    mac/DOM/DOMWheelEventInternal.h
    mac/DOM/DOMXPathExpressionInternal.h
    mac/DOM/DOMXPathResultInternal.h
    mac/DOM/ExceptionHandlers.h
    mac/DOM/ObjCEventListener.h
    mac/DOM/ObjCNodeFilterCondition.h
    mac/DOM/WebDOMOperationsInternal.h

    mac/DefaultDelegates/WebDefaultContextMenuDelegate.h
    mac/DefaultDelegates/WebDefaultEditingDelegate.h
    mac/DefaultDelegates/WebDefaultUIDelegate.h

    mac/History/BackForwardList.h
    mac/History/BinaryPropertyList.h
    mac/History/HistoryPropertyList.h
    mac/History/WebBackForwardListInternal.h
    mac/History/WebHistoryInternal.h
    mac/History/WebHistoryItemInternal.h

    mac/Misc/WebElementDictionary.h
    mac/Misc/WebKitLogging.h
    mac/Misc/WebKitStatisticsPrivate.h
    mac/Misc/WebKitVersionChecks.h
    mac/Misc/WebLocalizableStringsInternal.h
    mac/Misc/WebNSControlExtras.h
    mac/Misc/WebNSDataExtras.h
    mac/Misc/WebNSDictionaryExtras.h
    mac/Misc/WebNSImageExtras.h
    mac/Misc/WebNSObjectExtras.h
    mac/Misc/WebNSPrintOperationExtras.h
    mac/Misc/WebNSURLRequestExtras.h
    mac/Misc/WebSharingServicePickerController.h

    mac/Panels/WebAuthenticationPanel.h

    mac/Plugins/WebBasePluginPackage.h
    mac/Plugins/WebPluginContainerCheck.h
    mac/Plugins/WebPluginController.h
    mac/Plugins/WebPluginPackage.h

    mac/Storage/WebDatabaseManagerClient.h
    mac/Storage/WebDatabaseManagerInternal.h
    mac/Storage/WebStorageManagerInternal.h
    mac/Storage/WebStorageTrackerClient.h

    mac/WebCoreSupport/CorrectionPanel.h
    mac/WebCoreSupport/LegacyHistoryItemClient.h
    mac/WebCoreSupport/PopupMenuMac.h
    mac/WebCoreSupport/SearchPopupMenuMac.h
    mac/WebCoreSupport/TextIndicatorWindow.h
    mac/WebCoreSupport/WebAlternativeTextClient.h
    mac/WebCoreSupport/WebCachedFramePlatformData.h
    mac/WebCoreSupport/WebChromeClient.h
    mac/WebCoreSupport/WebContextMenuClient.h
    mac/WebCoreSupport/WebDragClient.h
    mac/WebCoreSupport/WebEditorClient.h
    mac/WebCoreSupport/WebFrameLoaderClient.h
    mac/WebCoreSupport/WebFrameNetworkingContext.h
    mac/WebCoreSupport/WebGeolocationClient.h
    mac/WebCoreSupport/WebInspectorClient.h
    mac/WebCoreSupport/WebKitFullScreenListener.h
    mac/WebCoreSupport/WebMediaKeySystemClient.h
    mac/WebCoreSupport/WebNotificationClient.h
    mac/WebCoreSupport/WebOpenPanelResultListener.h
    mac/WebCoreSupport/WebPaymentCoordinatorClient.h
    mac/WebCoreSupport/WebPlatformStrategies.h
    mac/WebCoreSupport/WebPluginInfoProvider.h
    mac/WebCoreSupport/WebProgressTrackerClient.h
    mac/WebCoreSupport/WebSecurityOriginInternal.h
    mac/WebCoreSupport/WebSelectionServiceController.h
    mac/WebCoreSupport/WebValidationMessageClient.h
    mac/WebCoreSupport/WebVisitedLinkStore.h

    mac/WebInspector/WebInspectorFrontend.h
    mac/WebInspector/WebNodeHighlight.h
    mac/WebInspector/WebNodeHighlightView.h
    mac/WebInspector/WebNodeHighlighter.h

    mac/WebView/WebArchiveInternal.h
    mac/WebView/WebClipView.h
    mac/WebView/WebDataSourceInternal.h
    mac/WebView/WebDelegateImplementationCaching.h
    mac/WebView/WebDeviceOrientationInternal.h
    mac/WebView/WebDeviceOrientationProviderMockInternal.h
    mac/WebView/WebDocumentInternal.h
    mac/WebView/WebDocumentLoaderMac.h
    mac/WebView/WebDynamicScrollBarsViewInternal.h
    mac/WebView/WebFeatureInternal.h
    mac/WebView/WebFrameInternal.h
    mac/WebView/WebFrameViewInternal.h
    mac/WebView/WebFullScreenController.h
    mac/WebView/WebGeolocationPositionInternal.h
    mac/WebView/WebHTMLViewForTestingMac.h
    mac/WebView/WebHTMLViewInternal.h
    mac/WebView/WebHistoryDelegate.h
    mac/WebView/WebImmediateActionController.h
    mac/WebView/WebIndicateLayer.h
    mac/WebView/WebJSPDFDoc.h
    mac/WebView/WebMediaPlaybackTargetPicker.h
    mac/WebView/WebNotificationInternal.h
    mac/WebView/WebPDFDocumentExtras.h
    mac/WebView/WebPDFRepresentation.h
    mac/WebView/WebPDFView.h
    mac/WebView/WebPreferencesDefaultValues.h
    mac/WebView/WebPreferencesInternal.h
    mac/WebView/WebResourceInternal.h
    mac/WebView/WebScriptDebugger.h
    mac/WebView/WebScriptWorldInternal.h
    mac/WebView/WebTextCompletionController.h
    mac/WebView/WebViewData.h
    mac/WebView/WebViewInternal.h
    mac/WebView/WebViewRenderingUpdateScheduler.h
)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -compatibility_version 1 -current_version ${WEBKIT_MAC_VERSION} -framework SecurityInterface")
