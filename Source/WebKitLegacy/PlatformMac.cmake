find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
find_library(QUARTZ_LIBRARY Quartz)
find_library(SECURITYINTERFACE_LIBRARY SecurityInterface)
add_definitions(-iframework ${QUARTZ_LIBRARY}/Frameworks)
add_definitions(-iframework ${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks)
add_definitions(-DJSC_CLASS_AVAILABLE\\\(...\\\)=)
add_definitions(-fobjc-weak)

list(APPEND WebKitLegacy_PRIVATE_LIBRARIES
    ${SECURITYINTERFACE_LIBRARY}
)

list(APPEND WebKitLegacy_PRIVATE_INCLUDE_DIRECTORIES
    "${PAL_FRAMEWORK_HEADERS_DIR}"
    "${WEBKITLEGACY_DIR}"
    "${WEBKITLEGACY_DIR}/mac"
    "${WEBKITLEGACY_DIR}/mac/Misc"
    "${WEBKITLEGACY_DIR}/mac/WebView"
    "${WEBKITLEGACY_DIR}/mac/WebCoreSupport"
    "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}"
    "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}/WebKitLegacy"
    "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source"
    "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source/third_party/abseil-cpp"
    "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source/webrtc"
)

list(APPEND WebKitLegacy_UNIFIED_SOURCE_LIST_FILES
    SourcesCocoa.txt
)
WEBKIT_COMPUTE_SOURCES(WebKitLegacy)

list(APPEND WebKitLegacy_SOURCES
    cf/WebCoreSupport/WebInspectorClientCF.cpp

    mac/DefaultDelegates/WebDefaultEditingDelegate.m
    mac/DefaultDelegates/WebDefaultPolicyDelegate.m
    mac/DefaultDelegates/WebDefaultUIDelegate.mm

    mac/History/BackForwardList.mm
    mac/History/BinaryPropertyList.cpp
    mac/History/HistoryPropertyList.mm
    mac/History/WebHistory.mm
    mac/History/WebHistoryItem.mm
    mac/History/WebURLsWithTitles.m

    mac/Misc/WebCache.mm
    mac/Misc/WebCoreStatistics.mm
    mac/Misc/WebDownload.mm
    mac/Misc/WebElementDictionary.mm
    mac/Misc/WebIconDatabase.mm
    mac/Misc/WebKitErrors.m
    mac/Misc/WebKitLogInitialization.mm
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
    mac/Misc/WebStringTruncator.mm
    mac/Misc/WebUserContentURLPattern.mm

    mac/Panels/WebAuthenticationPanel.m
    mac/Panels/WebPanelAuthenticationHandler.m

    mac/Plugins/WebPluginPackage.mm

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
    mac/WebCoreSupport/WebChromeClient.mm
    mac/WebCoreSupport/WebContextMenuClient.mm
    mac/WebCoreSupport/WebDeviceOrientationClient.mm
    mac/WebCoreSupport/WebDragClient.mm
    mac/WebCoreSupport/WebEditorClient.mm
    mac/WebCoreSupport/WebFrameNetworkingContext.mm
    mac/WebCoreSupport/WebGeolocationClient.mm
    mac/WebCoreSupport/WebInspectorClient.mm
    mac/WebCoreSupport/WebJavaScriptTextInputPanel.m
    mac/WebCoreSupport/WebKitFullScreenListener.mm
    mac/WebCoreSupport/WebMediaKeySystemClient.mm
    mac/WebCoreSupport/WebNotificationClient.mm
    mac/WebCoreSupport/WebOpenPanelResultListener.mm
    mac/WebCoreSupport/WebPaymentCoordinatorClient.mm
    mac/WebCoreSupport/WebPlatformStrategies.mm
    mac/WebCoreSupport/WebPluginInfoProvider.mm
    mac/WebCoreSupport/WebProgressTrackerClient.mm
    mac/WebCoreSupport/WebSecurityOrigin.mm
    mac/WebCoreSupport/WebSelectionServiceController.mm
    mac/WebCoreSupport/WebSwitchingGPUClient.cpp
    mac/WebCoreSupport/WebValidationMessageClient.mm
    mac/WebCoreSupport/WebVisitedLinkStore.mm

    mac/WebInspector/WebInspectorFrontend.mm
    mac/WebInspector/WebNodeHighlight.mm
    mac/WebInspector/WebNodeHighlightView.mm
    mac/WebInspector/WebNodeHighlighter.mm

    mac/WebView/WebArchive.mm
    mac/WebView/WebDashboardRegion.mm
    mac/WebView/WebDelegateImplementationCaching.mm
    mac/WebView/WebDeviceOrientation.mm
    mac/WebView/WebDeviceOrientationProviderMock.mm
    mac/WebView/WebDocumentLoaderMac.mm
    mac/WebView/WebDynamicScrollBarsView.mm
    mac/WebView/WebFeature.m
    mac/WebView/WebFormDelegate.m
    mac/WebView/WebGeolocationPosition.mm
    mac/WebView/WebHTMLRepresentation.mm
    mac/WebView/WebIndicateLayer.mm
    mac/WebView/WebJSPDFDoc.mm
    mac/WebView/WebMediaPlaybackTargetPicker.mm
    mac/WebView/WebNavigationData.mm
    mac/WebView/WebNotification.mm
    mac/WebView/WebPDFDocumentExtras.mm
    mac/WebView/WebPolicyDelegate.mm
    mac/WebView/WebPreferences.mm
    mac/WebView/WebPreferencesDefaultValues.mm
    mac/WebView/WebResource.mm
    mac/WebView/WebTextCompletionController.mm
    mac/WebView/WebTextIterator.mm
    mac/WebView/WebVideoFullscreenController.mm
    mac/WebView/WebViewData.mm
    mac/WebView/WebWindowAnimation.mm
)

set(WebKitLegacy_LEGACY_FORWARDING_HEADERS_FILES
    mac/DOM/DOMHTMLHeadingElement.h
    mac/DOM/DOMHTMLBaseFontElement.h
    mac/DOM/DOMCSSUnknownRule.h
    mac/DOM/DOMHTMLCollection.h
    mac/DOM/DOMHTMLDivElement.h
    mac/DOM/DOMHTMLFormElement.h
    mac/DOM/DOMXPathExpressionInternal.h
    mac/DOM/DOMHTMLHeadElement.h
    mac/DOM/DOMImplementation.h
    mac/DOM/DOMCSSStyleRule.h
    mac/DOM/DOMCSSRule.h
    mac/DOM/DOMEvents.h
    mac/DOM/DOMHTMLImageElementInternal.h
    mac/DOM/DOMMouseEvent.h
    mac/DOM/DOMElement.h
    mac/DOM/DOMMediaListInternal.h
    mac/DOM/DOMHTMLMapElement.h
    mac/DOM/DOMCSSRuleInternal.h
    mac/DOM/DOMMediaList.h
    mac/DOM/DOMCSSRuleListInternal.h
    mac/DOM/DOMDocumentInternal.h
    mac/DOM/WebDOMOperations.h
    mac/DOM/DOMNodePrivate.h
    mac/DOM/DOMHTMLParagraphElement.h
    mac/DOM/DOMHTMLFormElementInternal.h
    mac/DOM/DOMProgressEvent.h
    mac/DOM/DOMDocumentFragmentInternal.h
    mac/DOM/DOMHTMLTextAreaElementPrivate.h
    mac/DOM/DOMProcessingInstructionInternal.h
    mac/DOM/DOMDocumentFragmentPrivate.h
    mac/DOM/DOMRangeInternal.h
    mac/DOM/DOMRangeException.h
    mac/DOM/DOMCSSCharsetRule.h
    mac/DOM/DOMHTMLFrameElement.h
    mac/DOM/DOMHTMLHRElement.h
    mac/DOM/DOMViews.h
    mac/DOM/DOMCSSStyleDeclarationInternal.h
    mac/DOM/DOMCSSPrimitiveValue.h
    mac/DOM/DOMCSSMediaRule.h
    mac/DOM/DOMHTMLLegendElement.h
    mac/DOM/DOMBlobInternal.h
    mac/DOM/DOMNodeFilter.h
    mac/DOM/DOMStylesheets.h
    mac/DOM/ObjCNodeFilterCondition.h
    mac/DOM/DOMHTMLCollectionInternal.h
    mac/DOM/DOMRect.h
    mac/DOM/DOMCSSRuleList.h
    mac/DOM/DOMHTMLIFrameElement.h
    mac/DOM/DOMHTMLUListElement.h
    mac/DOM/DOMFileList.h
    mac/DOM/DOMTraversal.h
    mac/DOM/DOMHTMLTableCellElement.h
    mac/DOM/DOMHTMLDirectoryElement.h
    mac/DOM/DOMNodeListInternal.h
    mac/DOM/DOMExtensions.h
    mac/DOM/DOMHTMLParamElement.h
    mac/DOM/DOMCDATASectionInternal.h
    mac/DOM/DOMHTMLOptGroupElement.h
    mac/DOM/DOMRanges.h
    mac/DOM/DOMHTMLOptionElementInternal.h
    mac/DOM/DOMXPathResultInternal.h
    mac/DOM/DOMHTMLTitleElement.h
    mac/DOM/DOMHTMLTextAreaElementInternal.h
    mac/DOM/DOMDocumentTypeInternal.h
    mac/DOM/DOMCSSPageRule.h
    mac/DOM/DOMMutationEvent.h
    mac/DOM/DOMEventException.h
    mac/DOM/DOMTimeRangesInternal.h
    mac/DOM/DOMNamedNodeMapInternal.h
    mac/DOM/DOMXPathExpression.h
    mac/DOM/DOMXPathResult.h
    mac/DOM/DOMFileInternal.h
    mac/DOM/DOMCSS.h
    mac/DOM/DOMHTMLTableSectionElement.h
    mac/DOM/DOMCSSFontFaceRule.h
    mac/DOM/DOMStyleSheet.h
    mac/DOM/DOMInternal.h
    mac/DOM/DOMNodeIterator.h
    mac/DOM/DOMCounterInternal.h
    mac/DOM/DOM.h
    mac/DOM/DOMHTMLBRElement.h
    mac/DOM/DOMTokenList.h
    mac/DOM/DOMHTMLMenuElement.h
    mac/DOM/DOMCSSStyleSheetInternal.h
    mac/DOM/DOMNodeList.h
    mac/DOM/DOMHTMLStyleElementInternal.h
    mac/DOM/DOMXPath.h
    mac/DOM/DOMWheelEventInternal.h
    mac/DOM/DOMHTMLBodyElement.h
    mac/DOM/DOMCSSValueList.h
    mac/DOM/DOMHTMLScriptElementInternal.h
    mac/DOM/DOMKeyboardEvent.h
    mac/DOM/DOMStyleSheetList.h
    mac/DOM/DOMHTMLObjectElement.h
    mac/DOM/DOMHTMLLinkElement.h
    mac/DOM/DOMHTMLLIElement.h
    mac/DOM/DOMHTMLTableRowElement.h
    mac/DOM/DOMCDATASection.h
    mac/DOM/DOMAbstractView.h
    mac/DOM/DOMHTMLSelectElement.h
    mac/DOM/DOMHTMLCanvasElement.h
    mac/DOM/WebDOMOperationsPrivate.h
    mac/DOM/DOMTreeWalkerInternal.h
    mac/DOM/DOMMediaError.h
    mac/DOM/DOMHTMLScriptElement.h
    mac/DOM/DOMHTMLAnchorElement.h
    mac/DOM/DOMHTMLInputElement.h
    mac/DOM/DOMAbstractViewInternal.h
    mac/DOM/DOMCSSImportRule.h
    mac/DOM/DOMElementInternal.h
    mac/DOM/DOMHTMLTableColElementInternal.h
    mac/DOM/DOMHTMLInputElementPrivate.h
    mac/DOM/DOMHTML.h
    mac/DOM/DOMProcessingInstruction.h
    mac/DOM/DOMNodeIteratorInternal.h
    mac/DOM/DOMHTMLMarqueeElement.h
    mac/DOM/DOMHTMLDocumentInternal.h
    mac/DOM/DOMHTMLElement.h
    mac/DOM/DOMUIKitExtensions.h
    mac/DOM/DOMHTMLElementInternal.h
    mac/DOM/DOMText.h
    mac/DOM/DOMOverflowEvent.h
    mac/DOM/DOMAbstractViewFrame.h
    mac/DOM/DOMHTMLDocument.h
    mac/DOM/DOMHTMLMediaElement.h
    mac/DOM/DOMHTMLStyleElement.h
    mac/DOM/DOMEntityReference.h
    mac/DOM/WebDOMOperationsInternal.h
    mac/DOM/DOMHTMLBaseElement.h
    mac/DOM/ObjCEventListener.h
    mac/DOM/DOMEventInternal.h
    mac/DOM/DOMCSSStyleDeclaration.h
    mac/DOM/DOMDocumentFragment.h
    mac/DOM/DOMHTMLDListElement.h
    mac/DOM/DOMCSSStyleSheet.h
    mac/DOM/DOMNamedNodeMap.h
    mac/DOM/DOMHTMLHeadElementInternal.h
    mac/DOM/ExceptionHandlers.h
    mac/DOM/DOMTextEvent.h
    mac/DOM/DOMNodeInternal.h
    mac/DOM/DOMHTMLHtmlElement.h
    mac/DOM/DOMImplementationInternal.h
    mac/DOM/DOMHTMLOptionsCollectionInternal.h
    mac/DOM/DOMCSSPrimitiveValueInternal.h
    mac/DOM/DOMHTMLTableCaptionElementInternal.h
    mac/DOM/DOMEventListener.h
    mac/DOM/DOMCounter.h
    mac/DOM/DOMDocumentPrivate.h
    mac/DOM/DOMHTMLTableCaptionElement.h
    mac/DOM/WebAutocapitalizeTypes.h
    mac/DOM/DOMHTMLAreaElementInternal.h
    mac/DOM/DOMAttr.h
    mac/DOM/DOMWheelEvent.h
    mac/DOM/DOMTreeWalker.h
    mac/DOM/DOMHTMLOptionElement.h
    mac/DOM/DOMDocumentType.h
    mac/DOM/DOMException.h
    mac/DOM/DOMHTMLFieldSetElement.h
    mac/DOM/DOMStyleSheetListInternal.h
    mac/DOM/DOMCore.h
    mac/DOM/DOMRGBColor.h
    mac/DOM/DOMHTMLOptionsCollection.h
    mac/DOM/DOMRectInternal.h
    mac/DOM/DOMEvent.h
    mac/DOM/DOMTokenListInternal.h
    mac/DOM/DOMTimeRanges.h
    mac/DOM/DOMHTMLTableCellElementInternal.h
    mac/DOM/DOMNode.h
    mac/DOM/DOMEventTarget.h
    mac/DOM/DOMAttrInternal.h
    mac/DOM/DOMHTMLTextAreaElement.h
    mac/DOM/DOMHTMLImageElement.h
    mac/DOM/DOMRGBColorInternal.h
    mac/DOM/DOMHTMLLinkElementInternal.h
    mac/DOM/DOMUIEvent.h
    mac/DOM/DOMCommentInternal.h
    mac/DOM/DOMHTMLFrameSetElement.h
    mac/DOM/DOMHTMLTableElement.h
    mac/DOM/DOMHTMLLabelElement.h
    mac/DOM/DOMHTMLOListElement.h
    mac/DOM/DOMMediaErrorInternal.h
    mac/DOM/DOMComment.h
    mac/DOM/DOMHTMLSelectElementInternal.h
    mac/DOM/DOMXPathNSResolver.h
    mac/DOM/DOMFileListInternal.h
    mac/DOM/DOMHTMLTableColElement.h
    mac/DOM/DOMTextInternal.h
    mac/DOM/DOMHTMLAreaElement.h
    mac/DOM/DOMRange.h
    mac/DOM/DOMCharacterData.h
    mac/DOM/DOMObject.h
    mac/DOM/DOMDocument.h
    mac/DOM/DOMHTMLFontElement.h
    mac/DOM/DOMHTMLAppletElement.h
    mac/DOM/DOMStyleSheetInternal.h
    mac/DOM/DOMHTMLModElement.h
    mac/DOM/DOMHTMLMetaElement.h
    mac/DOM/DOMCSSValue.h
    mac/DOM/DOMBlob.h
    mac/DOM/DOMPrivate.h
    mac/DOM/DOMXPathException.h
    mac/DOM/DOMHTMLTableSectionElementInternal.h
    mac/DOM/DOMCSSValueInternal.h
    mac/DOM/DOMFile.h
    mac/DOM/DOMCustomXPathNSResolver.h
    mac/DOM/DOMHTMLPreElement.h
    mac/DOM/DOMHTMLInputElementInternal.h
    mac/DOM/DOMHTMLQuoteElement.h
    mac/DOM/DOMHTMLEmbedElement.h
    mac/DOM/DOMHTMLVideoElement.h
    mac/DOM/DOMEntity.h
    mac/DOM/DOMHTMLElementPrivate.h
    mac/DOM/DOMHTMLButtonElement.h

    mac/History/WebHistoryItem.h
    mac/History/HistoryPropertyList.h
    mac/History/WebBackForwardList.h
    mac/History/BinaryPropertyList.h
    mac/History/WebBackForwardListInternal.h
    mac/History/BackForwardList.h
    mac/History/WebBackForwardListPrivate.h
    mac/History/WebHistory.h
    mac/History/WebURLsWithTitles.h
    mac/History/WebHistoryPrivate.h
    mac/History/WebHistoryItemPrivate.h
    mac/History/WebHistoryItemInternal.h
    mac/History/WebHistoryInternal.h

    mac/DefaultDelegates/WebDefaultPolicyDelegate.h
    mac/DefaultDelegates/WebDefaultUIDelegate.h
    mac/DefaultDelegates/WebDefaultContextMenuDelegate.h
    mac/DefaultDelegates/WebDefaultEditingDelegate.h

    mac/History/WebHistory.h
    mac/History/WebHistoryItem.h

    mac/Misc/WebKitStatisticsPrivate.h
    mac/Misc/WebCache.h
    mac/Misc/NSURLDownloadSPI.h
    mac/Misc/WebStringTruncator.h
    mac/Misc/WebNSFileManagerExtras.h
    mac/Misc/WebNSWindowExtras.h
    mac/Misc/WebDownload.h
    mac/Misc/WebNSControlExtras.h
    mac/Misc/WebNSObjectExtras.h
    mac/Misc/WebKitErrors.h
    mac/Misc/WebNSViewExtras.h
    mac/Misc/WebNSDictionaryExtras.h
    mac/Misc/WebNSURLRequestExtras.h
    mac/Misc/WebSharingServicePickerController.h
    mac/Misc/WebLocalizableStringsInternal.h
    mac/Misc/WebNSDataExtrasPrivate.h
    mac/Misc/WebNSUserDefaultsExtras.h
    mac/Misc/WebNSEventExtras.h
    mac/Misc/WebNSURLExtras.h
    mac/Misc/WebIconDatabase.h
    mac/Misc/WebKitStatistics.h
    mac/Misc/WebKitLogging.h
    mac/Misc/WebQuotaManager.h
    mac/Misc/WebKitNSStringExtras.h
    mac/Misc/WebNSPrintOperationExtras.h
    mac/Misc/WebNSImageExtras.h
    mac/Misc/WebKitErrorsPrivate.h
    mac/Misc/WebUserContentURLPattern.h
    mac/Misc/WebKitVersionChecks.h
    mac/Misc/WebLocalizableStrings.h
    mac/Misc/WebTypesInternal.h
    mac/Misc/WebCoreStatistics.h
    mac/Misc/WebNSDataExtras.h
    mac/Misc/WebElementDictionary.h
    mac/Misc/WebKit.h
    mac/Misc/WebNSPasteboardExtras.h

    mac/Panels/WebPanelAuthenticationHandler.h
    mac/Panels/WebAuthenticationPanel.h

    mac/Plugins/WebPluginViewFactoryPrivate.h
    mac/Plugins/WebBasePluginPackage.h
    mac/Plugins/WebPluginController.h
    mac/Plugins/WebPluginContainerCheck.h
    mac/Plugins/WebPluginContainer.h
    mac/Plugins/WebPluginPackagePrivate.h
    mac/Plugins/WebPluginPackage.h
    mac/Plugins/WebPluginContainerPrivate.h
    mac/Plugins/WebJavaPlugIn.h
    mac/Plugins/WebPluginViewFactory.h
    mac/Plugins/WebPluginDatabase.h
    mac/Plugins/WebPlugin.h

    mac/Storage/WebDatabaseManagerPrivate.h
    mac/Storage/WebStorageTrackerClient.h
    mac/Storage/WebDatabaseManagerInternal.h
    mac/Storage/WebStorageManagerInternal.h
    mac/Storage/WebDatabaseManagerClient.h
    mac/Storage/WebStorageManagerPrivate.h
    mac/Storage/WebDatabaseQuotaManager.h

    mac/WebInspector/WebNodeHighlighter.h
    mac/WebInspector/WebNodeHighlightView.h
    mac/WebInspector/WebNodeHighlight.h
    mac/WebInspector/WebInspector.h
    mac/WebInspector/WebInspectorPrivate.h
    mac/WebInspector/WebInspectorFrontend.h

    mac/WebCoreSupport/WebAlternativeTextClient.h
    mac/WebCoreSupport/WebSecurityOriginPrivate.h
    mac/WebCoreSupport/WebCreateFragmentInternal.h
    mac/WebCoreSupport/WebProgressTrackerClient.h
    mac/WebCoreSupport/WebDragClient.h
    mac/WebCoreSupport/WebChromeClient.h
    mac/WebCoreSupport/WebPluginInfoProvider.h
    mac/WebCoreSupport/WebEditorClient.h
    mac/WebCoreSupport/CorrectionPanel.h
    mac/WebCoreSupport/WebSwitchingGPUClient.h
    mac/WebCoreSupport/WebSecurityOriginInternal.h
    mac/WebCoreSupport/WebSelectionServiceController.h
    mac/WebCoreSupport/WebVisitedLinkStore.h
    mac/WebCoreSupport/WebApplicationCache.h
    mac/WebCoreSupport/WebInspectorClient.h
    mac/WebCoreSupport/WebApplicationCacheQuotaManager.h
    mac/WebCoreSupport/WebContextMenuClient.h
    mac/WebCoreSupport/SearchPopupMenuMac.h
    mac/WebCoreSupport/WebJavaScriptTextInputPanel.h
    mac/WebCoreSupport/WebPaymentCoordinatorClient.h
    mac/WebCoreSupport/WebApplicationCacheInternal.h
    mac/WebCoreSupport/WebPlatformStrategies.h
    mac/WebCoreSupport/WebGeolocationClient.h
    mac/WebCoreSupport/WebFrameNetworkingContext.h
    mac/WebCoreSupport/PopupMenuMac.h
    mac/WebCoreSupport/WebDeviceOrientationClient.h
    mac/WebCoreSupport/WebValidationMessageClient.h
    mac/WebCoreSupport/WebCachedFramePlatformData.h
    mac/WebCoreSupport/WebFrameLoaderClient.h
    mac/WebCoreSupport/WebNotificationClient.h
    mac/WebCoreSupport/WebKitFullScreenListener.h
    mac/WebCoreSupport/WebOpenPanelResultListener.h

    mac/WebView/WebArchive.h
    mac/WebView/WebHTMLViewPrivate.h
    mac/WebView/WebFrame.h
    mac/WebView/WebScriptWorld.h
    mac/WebView/WebFullScreenController.h
    mac/WebView/WebArchiveInternal.h
    mac/WebView/WebDocumentInternal.h
    mac/WebView/WebNavigationData.h
    mac/WebView/WebResource.h
    mac/WebView/WebClipView.h
    mac/WebView/WebNotificationInternal.h
    mac/WebView/WebVideoFullscreenController.h
    mac/WebView/WebScriptDebugDelegate.h
    mac/WebView/WebViewPrivate.h
    mac/WebView/WebArchive.h
    mac/WebView/WebDocument.h
    mac/WebView/WebFrameLoadDelegatePrivate.h
    mac/WebView/WebFormDelegate.h
    mac/WebView/WebPolicyDelegate.h
    mac/WebView/WebDeviceOrientationProvider.h
    mac/WebView/WebUIDelegate.h
    mac/WebView/WebResourceLoadDelegatePrivate.h
    mac/WebView/WebPDFDocumentExtras.h
    mac/WebView/WebResourceInternal.h
    mac/WebView/WebResourceLoadDelegate.h
    mac/WebView/WebJSPDFDoc.h
    mac/WebView/WebDeviceOrientation.h
    mac/WebView/WebUIDelegatePrivate.h
    mac/WebView/WebScriptDebugger.h
    mac/WebView/WebDeviceOrientationProviderMockInternal.h
    mac/WebView/WebDynamicScrollBarsView.h
    mac/WebView/WebPreferencesPrivate.h
    mac/WebView/WebPolicyDelegatePrivate.h
    mac/WebView/WebGeolocationPosition.h
    mac/WebView/WebEditingDelegatePrivate.h
    mac/WebView/WebScriptWorldInternal.h
    mac/WebView/WebFrameViewPrivate.h
    mac/WebView/WebViewInternal.h
    mac/WebView/WebHTMLRepresentationPrivate.h
    mac/WebView/WebMediaPlaybackTargetPicker.h
    mac/WebView/WebAllowDenyPolicyListener.h
    mac/WebView/WebPDFView.h
    mac/WebView/WebTextIterator.h
    mac/WebView/WebDataSourceInternal.h
    mac/WebView/WebDocumentLoaderMac.h
    mac/WebView/WebView.h
    mac/WebView/WebFrameView.h
    mac/WebView/WebTextCompletionController.h
    mac/WebView/WebDelegateImplementationCaching.h
    mac/WebView/WebDataSourcePrivate.h
    mac/WebView/WebFramePrivate.h
    mac/WebView/WebDeviceOrientationProviderMock.h
    mac/WebView/WebDocumentPrivate.h
    mac/WebView/WebViewData.h
    mac/WebView/WebImmediateActionController.h
    mac/WebView/WebFrameInternal.h
    mac/WebView/WebDeviceOrientationInternal.h
    mac/WebView/WebFrameLoadDelegate.h
    mac/WebView/WebPreferenceKeysPrivate.h
    mac/WebView/WebFrameViewInternal.h
    mac/WebView/WebFormDelegatePrivate.h
    mac/WebView/WebNotification.h
    mac/WebView/PDFViewSPI.h
    mac/WebView/WebResourcePrivate.h
    mac/WebView/WebPreferences.h
    mac/WebView/WebEditingDelegate.h
    mac/WebView/WebHistoryDelegate.h
    mac/WebView/WebWindowAnimation.h
    mac/WebView/WebDashboardRegion.h
    mac/WebView/WebHTMLView.h
    mac/WebView/WebIndicateLayer.h
    mac/WebView/WebHTMLRepresentation.h
    mac/WebView/WebHTMLViewInternal.h
    mac/WebView/WebDataSource.h
    mac/WebView/WebPDFRepresentation.h
    mac/WebView/WebGeolocationPositionInternal.h
    mac/WebView/WebDynamicScrollBarsViewInternal.h

    ${WEBCORE_DIR}/bridge/objc/WebScriptObject.h
    ${WEBCORE_DIR}/platform/cocoa/WebKitAvailability.h
    ${WEBCORE_DIR}/plugins/npfunctions.h
    ${WEBCORE_DIR}/plugins/npapi.h
)

add_definitions("-include WebKitPrefix.h")

set(C99_FILES
    mac/DefaultDelegates/WebDefaultEditingDelegate.m
    mac/DefaultDelegates/WebDefaultUIDelegate.m

    mac/Misc/WebKitErrors.m
    mac/Misc/WebKitStatistics.m
    mac/Misc/WebKitSystemBits.m
    mac/Misc/WebNSArrayExtras.m
    mac/Misc/WebNSControlExtras.m
    mac/Misc/WebNSEventExtras.m
    mac/Misc/WebNSPrintOperationExtras.m
    mac/Misc/WebNSURLRequestExtras.m
    mac/Misc/WebNSViewExtras.m
    mac/Misc/WebNSWindowExtras.m

    mac/Plugins/WebPluginsPrivate.m

    mac/Plugins/Hosted/WebTextInputWindowController.m

    mac/WebView/WebFormDelegate.m
)

set(CPP_FILES
    Storage/StorageThread.cpp
)

foreach (_file ${WebKitLegacy_SOURCES})
    list(FIND C99_FILES ${_file} _c99_index)
    list(FIND CPP_FILES ${_file} _cpp_index)
    if (NOT ${_c99_index} EQUAL -1)
        set_source_files_properties(${_file} PROPERTIES COMPILE_FLAGS -std=c99)
    elseif (NOT ${_cpp_index} EQUAL -1)
        set_source_files_properties(${_file} PROPERTIES COMPILE_FLAGS -std=c++2a)
    else ()
        set_source_files_properties(${_file} PROPERTIES COMPILE_FLAGS "-ObjC++ -std=c++2a")
    endif ()
endforeach ()

foreach (_file ${WebKitLegacy_LEGACY_FORWARDING_HEADERS_FILES})
    get_filename_component(_name "${_file}" NAME)
    set(_target_filename "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}/WebKitLegacy/${_name}")
    if (NOT EXISTS ${_target_filename})
        file(WRITE ${_target_filename} "#import \"${_file}\"")
    endif ()
endforeach ()

add_custom_command(
    OUTPUT ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebViewPreferencesChangedGenerated.mm ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebPreferencesInternalFeatures.mm ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebPreferencesExperimentalFeatures.mm ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebPreferencesDefinitions.h
    DEPENDS ${WebKit_WEB_PREFERENCES_TEMPLATES} ${WebKit_WEB_PREFERENCES} WTF_CopyPreferences
    COMMAND ${RUBY_EXECUTABLE} ${WTF_SCRIPTS_DIR}/GeneratePreferences.rb --frontend WebKitLegacy --base ${WTF_SCRIPTS_DIR}/Preferences/WebPreferences.yaml --debug ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesDebug.yaml --experimental ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesExperimental.yaml --internal ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesInternal.yaml --outputDir "${WebKitLegacy_DERIVED_SOURCES_DIR}" --template ${WEBKITLEGACY_DIR}/mac/Scripts/PreferencesTemplates/WebViewPreferencesChangedGenerated.mm.erb --template ${WEBKITLEGACY_DIR}/mac/Scripts/PreferencesTemplates/WebPreferencesInternalFeatures.mm.erb --template ${WEBKITLEGACY_DIR}/mac/Scripts/PreferencesTemplates/WebPreferencesExperimentalFeatures.mm.erb --template ${WEBKITLEGACY_DIR}/mac/Scripts/PreferencesTemplates/WebPreferencesDefinitions.h.erb
    VERBATIM)

list(APPEND WebKitLegacy_SOURCES
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebViewPreferencesChangedGenerated.mm
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebPreferencesInternalFeatures.mm
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebPreferencesExperimentalFeatures.mm
)

set(WebKitLegacy_OUTPUT_NAME WebKitLegacy)

set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} "-compatibility_version 1 -current_version ${WEBKIT_MAC_VERSION} -framework SecurityInterface")
