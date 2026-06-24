include(PlatformCocoa.cmake)

find_library(UIKIT_LIBRARY UIKit)

list(APPEND WebKitLegacy_PRIVATE_LIBRARIES
    ${UIKIT_LIBRARY}
)

target_compile_options(WebKitLegacy PRIVATE
    "$<$<COMPILE_LANGUAGE:OBJC>:-std=gnu99>")

set(BUNDLE_VERSION "${MACOSX_FRAMEWORK_BUNDLE_VERSION}")
set(SHORT_VERSION_STRING "${WEBKIT_MAC_VERSION}")
set(PRODUCT_NAME "WebKitLegacy")
set(PRODUCT_BUNDLE_IDENTIFIER "com.apple.WebKitLegacy")
configure_file(${WEBKITLEGACY_DIR}/mac/Info.plist ${CMAKE_CURRENT_BINARY_DIR}/WebKitLegacy-Info.plist)
execute_process(COMMAND plutil -insert MinimumOSVersion -string "${CMAKE_OSX_DEPLOYMENT_TARGET}" ${CMAKE_CURRENT_BINARY_DIR}/WebKitLegacy-Info.plist)
execute_process(COMMAND plutil -insert UIDeviceFamily -json "[1,2]" ${CMAKE_CURRENT_BINARY_DIR}/WebKitLegacy-Info.plist)

set(WebKitLegacy_POST_BUILD_COMMAND
    ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/WebKitLegacy-Info.plist
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKitLegacy.framework/Info.plist
    COMMAND codesign --force --sign - ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKitLegacy.framework
)
set_target_properties(WebKitLegacy PROPERTIES
    INSTALL_NAME_DIR "${WebKitLegacy_INSTALL_NAME_DIR}"
)
target_link_options(WebKitLegacy PRIVATE
    -compatibility_version 1.0.0
    -current_version ${WEBKIT_MAC_VERSION}
)

target_link_options(WebKitLegacy PRIVATE
    -exported_symbols_list ${WEBKITLEGACY_DIR}/WebKitLegacy-iOS.exp
)

# FIXME: Generate this list dynamically (from `tapi reexport` against the
# migrated WebCore headers + ios/WebKit.iOS.exp + mac/WebKit.exp) instead of
# relying on a static snapshot. https://bugs.webkit.org/show_bug.cgi?id=312083
# WebKitLegacy-iOS-unexported.exp is no longer applied: -exported_symbols_list
# is a whitelist, so any symbol not in WebKitLegacy-iOS.exp is implicitly
# unexported. ld errors if both lists are passed. Note: `-undefined
# dynamic_lookup` is rejected by ld for shared-cache-eligible dylibs, so
# stale entries in the snapshot must be removed by hand until the dynamic
# generation lands.

list(APPEND WebKitLegacy_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKITLEGACY_DIR}/Modules"

    "${WEBKITLEGACY_DIR}/ios"

    "${WEBKITLEGACY_DIR}/ios/DefaultDelegates"
    "${WEBKITLEGACY_DIR}/ios/Misc"
    "${WEBKITLEGACY_DIR}/ios/WebCoreSupport"
    "${WEBKITLEGACY_DIR}/ios/WebView"

    "${WEBKITLEGACY_DIR}/mac/DOM"
    "${WEBKITLEGACY_DIR}/mac/DefaultDelegates"
    "${WEBKITLEGACY_DIR}/mac/History"
    "${WEBKITLEGACY_DIR}/mac/Panels"
    "${WEBKITLEGACY_DIR}/mac/Plugins"
    "${WEBKITLEGACY_DIR}/mac/Storage"
    "${WEBKITLEGACY_DIR}/mac/WebInspector"
    "${WEBKITLEGACY_DIR}/mac/WebView"
)

list(APPEND WebKitLegacy_SOURCES
    ios/DefaultDelegates/WebDefaultFormDelegate.m
    ios/DefaultDelegates/WebDefaultFrameLoadDelegate.m
    ios/DefaultDelegates/WebDefaultResourceLoadDelegate.m
    ios/DefaultDelegates/WebDefaultUIKitDelegate.m

    ios/Misc/WebGeolocationCoreLocationProvider.mm
    ios/Misc/WebGeolocationProviderIOS.mm
    ios/Misc/WebNSStringExtrasIOS.m
    ios/Misc/WebUIKitSupport.mm

    ios/WebCoreSupport/PopupMenuIOS.mm
    ios/WebCoreSupport/SearchPopupMenuIOS.cpp
    ios/WebCoreSupport/WebChromeClientIOS.mm
    ios/WebCoreSupport/WebFixedPositionContent.mm
    ios/WebCoreSupport/WebFrameIOS.mm
    ios/WebCoreSupport/WebGeolocation.mm
    ios/WebCoreSupport/WebInspectorClientIOS.mm
    ios/WebCoreSupport/WebMIMETypeRegistry.mm
    ios/WebCoreSupport/WebSelectionRect.m
    ios/WebCoreSupport/WebVisiblePosition.mm

    ios/WebView/WebFrameViewWAKCompatibility.m
    ios/WebView/WebPDFViewIOS.mm
    ios/WebView/WebPDFViewPlaceholder.mm
    ios/WebView/WebPlainWhiteView.mm

    mac/DefaultDelegates/WebDefaultEditingDelegate.m
    mac/DefaultDelegates/WebDefaultPolicyDelegate.mm
    mac/DefaultDelegates/WebDefaultUIDelegate.mm

    mac/WebView/WebPDFDocumentExtras.mm
)

list(APPEND WebKitLegacy_PRIVATE_LIBRARIES
    "-F${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    "-framework WebCore"
)
add_dependencies(WebKitLegacy WebCore)

list(APPEND WebKitLegacy_SOURCES
    mac/History/BackForwardList.mm
    mac/History/BinaryPropertyList.cpp
    mac/History/HistoryPropertyList.mm
    mac/History/WebHistory.mm
    mac/History/WebHistoryItem.mm

    mac/Misc/WebCache.mm
    mac/Misc/WebCoreStatistics.mm
    mac/Misc/WebDownload.mm
    mac/Misc/WebElementDictionary.mm
    mac/Misc/WebKitErrors.m
    mac/Misc/WebKitLogInitialization.mm
    mac/Misc/WebKitLogging.m
    mac/Misc/WebKitNSStringExtras.mm
    mac/Misc/WebKitStatistics.m
    mac/Misc/WebKitVersionChecks.mm
    mac/Misc/WebLocalizableStrings.mm
    mac/Misc/WebLocalizableStringsInternal.mm
    mac/Misc/WebNSDataExtras.mm
    mac/Misc/WebNSDictionaryExtras.m
    mac/Misc/WebNSFileManagerExtras.mm
    mac/Misc/WebNSObjectExtras.mm
    mac/Misc/WebNSURLExtras.mm
    mac/Misc/WebNSURLRequestExtras.m
    mac/Misc/WebNSUserDefaultsExtras.mm
    mac/Misc/WebUserContentURLPattern.mm

    mac/Plugins/WebPluginPackage.mm

    mac/Storage/WebDatabaseManager.mm
    mac/Storage/WebDatabaseManagerClient.mm
    mac/Storage/WebDatabaseProvider.mm
    mac/Storage/WebDatabaseQuotaManager.mm
    mac/Storage/WebStorageManager.mm
    mac/Storage/WebStorageTrackerClient.mm

    mac/WebCoreSupport/LegacyHistoryItemClient.mm
    mac/WebCoreSupport/WebAlternativeTextClient.mm
    mac/WebCoreSupport/WebChromeClient.mm
    mac/WebCoreSupport/WebContextMenuClient.mm
    mac/WebCoreSupport/WebDragClient.mm
    mac/WebCoreSupport/WebEditorClient.mm
    mac/WebCoreSupport/WebFrameNetworkingContext.mm
    mac/WebCoreSupport/WebGeolocationClient.mm
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
    mac/WebCoreSupport/WebValidationMessageClient.mm
    mac/WebCoreSupport/WebVisitedLinkStore.mm

    mac/WebInspector/WebInspectorFrontend.mm
    mac/WebInspector/WebNodeHighlight.mm
    mac/WebInspector/WebNodeHighlightView.mm
    mac/WebInspector/WebNodeHighlighter.mm

    mac/WebView/WebArchive.mm
    mac/WebView/WebDelegateImplementationCaching.mm
    mac/WebView/WebDeviceOrientation.mm
    mac/WebView/WebDeviceOrientationProviderMock.mm
    mac/WebView/WebDocumentLoaderMac.mm
    mac/WebView/WebFeature.m
    mac/WebView/WebFormDelegate.m
    mac/WebView/WebGeolocationPosition.mm
    mac/WebView/WebHTMLRepresentation.mm
    mac/WebView/WebIndicateLayer.mm
    mac/WebView/WebJSPDFDoc.mm
    mac/WebView/WebMediaPlaybackTargetPicker.mm
    mac/WebView/WebNavigationData.mm
    mac/WebView/WebNotification.mm
    mac/WebView/WebPolicyDelegate.mm
    mac/WebView/WebPreferences.mm
    mac/WebView/WebPreferencesDefaultValues.mm
    mac/WebView/WebResource.mm
    mac/WebView/WebTextIterator.mm
    mac/WebView/WebViewData.mm
)

set(WebKitLegacy_LEGACY_FORWARDING_HEADERS_FILES
    ${WEBCORE_DIR}/bridge/objc/WebScriptObject.h
    ${WEBCORE_DIR}/platform/cocoa/WebKitAvailability.h
    ${WEBCORE_DIR}/platform/ios/wak/WAKAppKitStubs.h
    ${WEBCORE_DIR}/platform/ios/wak/WAKResponder.h
    ${WEBCORE_DIR}/platform/ios/wak/WAKView.h
    ${WEBCORE_DIR}/platform/ios/wak/WAKWindow.h
    ${WEBCORE_DIR}/platform/ios/wak/WKContentObservation.h
    ${WEBCORE_DIR}/platform/ios/wak/WKGraphics.h
    ${WEBCORE_DIR}/platform/ios/wak/WKTypes.h

    ${WEBCORE_DIR}/platform/ios/AbstractPasteboard.h
    ${WEBCORE_DIR}/platform/ios/KeyEventCodesIOS.h
    ${WEBCORE_DIR}/platform/ios/WebEvent.h
    ${WEBCORE_DIR}/page/ios/WebEventRegion.h
    ${WEBCORE_DIR}/platform/ios/WebItemProviderPasteboard.h
    ${WEBCORE_DIR}/platform/ios/wak/WebCoreThread.h
    ${WEBCORE_DIR}/platform/ios/wak/WebCoreThreadMessage.h
    ${WEBCORE_DIR}/platform/ios/wak/WebCoreThreadRun.h

    mac/DOM/DOM.h
    mac/DOM/DOMAbstractView.h
    mac/DOM/DOMAbstractViewFrame.h
    mac/DOM/DOMAbstractViewInternal.h
    mac/DOM/DOMAttr.h
    mac/DOM/DOMAttrInternal.h
    mac/DOM/DOMBlob.h
    mac/DOM/DOMBlobInternal.h
    mac/DOM/DOMCDATASection.h
    mac/DOM/DOMCDATASectionInternal.h
    mac/DOM/DOMCSS.h
    mac/DOM/DOMCSSCharsetRule.h
    mac/DOM/DOMCSSFontFaceRule.h
    mac/DOM/DOMCSSImportRule.h
    mac/DOM/DOMCSSMediaRule.h
    mac/DOM/DOMCSSPageRule.h
    mac/DOM/DOMCSSPrimitiveValue.h
    mac/DOM/DOMCSSPrimitiveValueInternal.h
    mac/DOM/DOMCSSRule.h
    mac/DOM/DOMCSSRuleInternal.h
    mac/DOM/DOMCSSRuleList.h
    mac/DOM/DOMCSSRuleListInternal.h
    mac/DOM/DOMCSSStyleDeclaration.h
    mac/DOM/DOMCSSStyleDeclarationInternal.h
    mac/DOM/DOMCSSStyleRule.h
    mac/DOM/DOMCSSStyleSheet.h
    mac/DOM/DOMCSSStyleSheetInternal.h
    mac/DOM/DOMCSSUnknownRule.h
    mac/DOM/DOMCSSValue.h
    mac/DOM/DOMCSSValueInternal.h
    mac/DOM/DOMCSSValueList.h
    mac/DOM/DOMCharacterData.h
    mac/DOM/DOMComment.h
    mac/DOM/DOMCommentInternal.h
    mac/DOM/DOMCore.h
    mac/DOM/DOMCounter.h
    mac/DOM/DOMCounterInternal.h
    mac/DOM/DOMCustomXPathNSResolver.h
    mac/DOM/DOMDocument.h
    mac/DOM/DOMDocumentFragment.h
    mac/DOM/DOMDocumentFragmentInternal.h
    mac/DOM/DOMDocumentFragmentPrivate.h
    mac/DOM/DOMDocumentInternal.h
    mac/DOM/DOMDocumentPrivate.h
    mac/DOM/DOMDocumentType.h
    mac/DOM/DOMDocumentTypeInternal.h
    mac/DOM/DOMElement.h
    mac/DOM/DOMElementInternal.h
    mac/DOM/DOMEntity.h
    mac/DOM/DOMEntityReference.h
    mac/DOM/DOMEvent.h
    mac/DOM/DOMEventException.h
    mac/DOM/DOMEventInternal.h
    mac/DOM/DOMEventListener.h
    mac/DOM/DOMEventTarget.h
    mac/DOM/DOMEvents.h
    mac/DOM/DOMException.h
    mac/DOM/DOMExtensions.h
    mac/DOM/DOMFile.h
    mac/DOM/DOMFileInternal.h
    mac/DOM/DOMFileList.h
    mac/DOM/DOMFileListInternal.h
    mac/DOM/DOMHTML.h
    mac/DOM/DOMHTMLAnchorElement.h
    mac/DOM/DOMHTMLAppletElement.h
    mac/DOM/DOMHTMLAreaElement.h
    mac/DOM/DOMHTMLAreaElementInternal.h
    mac/DOM/DOMHTMLBRElement.h
    mac/DOM/DOMHTMLBaseElement.h
    mac/DOM/DOMHTMLBaseFontElement.h
    mac/DOM/DOMHTMLBodyElement.h
    mac/DOM/DOMHTMLButtonElement.h
    mac/DOM/DOMHTMLCanvasElement.h
    mac/DOM/DOMHTMLCollection.h
    mac/DOM/DOMHTMLCollectionInternal.h
    mac/DOM/DOMHTMLDListElement.h
    mac/DOM/DOMHTMLDirectoryElement.h
    mac/DOM/DOMHTMLDivElement.h
    mac/DOM/DOMHTMLDocument.h
    mac/DOM/DOMHTMLDocumentInternal.h
    mac/DOM/DOMHTMLElement.h
    mac/DOM/DOMHTMLElementInternal.h
    mac/DOM/DOMHTMLElementPrivate.h
    mac/DOM/DOMHTMLEmbedElement.h
    mac/DOM/DOMHTMLFieldSetElement.h
    mac/DOM/DOMHTMLFontElement.h
    mac/DOM/DOMHTMLFormElement.h
    mac/DOM/DOMHTMLFormElementInternal.h
    mac/DOM/DOMHTMLFrameElement.h
    mac/DOM/DOMHTMLFrameSetElement.h
    mac/DOM/DOMHTMLHRElement.h
    mac/DOM/DOMHTMLHeadElement.h
    mac/DOM/DOMHTMLHeadElementInternal.h
    mac/DOM/DOMHTMLHeadingElement.h
    mac/DOM/DOMHTMLHtmlElement.h
    mac/DOM/DOMHTMLIFrameElement.h
    mac/DOM/DOMHTMLImageElement.h
    mac/DOM/DOMHTMLImageElementInternal.h
    mac/DOM/DOMHTMLInputElement.h
    mac/DOM/DOMHTMLInputElementInternal.h
    mac/DOM/DOMHTMLInputElementPrivate.h
    mac/DOM/DOMHTMLLIElement.h
    mac/DOM/DOMHTMLLabelElement.h
    mac/DOM/DOMHTMLLegendElement.h
    mac/DOM/DOMHTMLLinkElement.h
    mac/DOM/DOMHTMLLinkElementInternal.h
    mac/DOM/DOMHTMLMapElement.h
    mac/DOM/DOMHTMLMarqueeElement.h
    mac/DOM/DOMHTMLMediaElement.h
    mac/DOM/DOMHTMLMenuElement.h
    mac/DOM/DOMHTMLMetaElement.h
    mac/DOM/DOMHTMLModElement.h
    mac/DOM/DOMHTMLOListElement.h
    mac/DOM/DOMHTMLObjectElement.h
    mac/DOM/DOMHTMLOptGroupElement.h
    mac/DOM/DOMHTMLOptionElement.h
    mac/DOM/DOMHTMLOptionElementInternal.h
    mac/DOM/DOMHTMLOptionsCollection.h
    mac/DOM/DOMHTMLOptionsCollectionInternal.h
    mac/DOM/DOMHTMLParagraphElement.h
    mac/DOM/DOMHTMLParamElement.h
    mac/DOM/DOMHTMLPreElement.h
    mac/DOM/DOMHTMLQuoteElement.h
    mac/DOM/DOMHTMLScriptElement.h
    mac/DOM/DOMHTMLScriptElementInternal.h
    mac/DOM/DOMHTMLSelectElement.h
    mac/DOM/DOMHTMLSelectElementInternal.h
    mac/DOM/DOMHTMLStyleElement.h
    mac/DOM/DOMHTMLStyleElementInternal.h
    mac/DOM/DOMHTMLTableCaptionElement.h
    mac/DOM/DOMHTMLTableCaptionElementInternal.h
    mac/DOM/DOMHTMLTableCellElement.h
    mac/DOM/DOMHTMLTableCellElementInternal.h
    mac/DOM/DOMHTMLTableColElement.h
    mac/DOM/DOMHTMLTableColElementInternal.h
    mac/DOM/DOMHTMLTableElement.h
    mac/DOM/DOMHTMLTableRowElement.h
    mac/DOM/DOMHTMLTableSectionElement.h
    mac/DOM/DOMHTMLTableSectionElementInternal.h
    mac/DOM/DOMHTMLTextAreaElement.h
    mac/DOM/DOMHTMLTextAreaElementInternal.h
    mac/DOM/DOMHTMLTextAreaElementPrivate.h
    mac/DOM/DOMHTMLTitleElement.h
    mac/DOM/DOMHTMLUListElement.h
    mac/DOM/DOMHTMLVideoElement.h
    mac/DOM/DOMImplementation.h
    mac/DOM/DOMImplementationInternal.h
    mac/DOM/DOMInternal.h
    mac/DOM/DOMKeyboardEvent.h
    mac/DOM/DOMMediaError.h
    mac/DOM/DOMMediaErrorInternal.h
    mac/DOM/DOMMediaList.h
    mac/DOM/DOMMediaListInternal.h
    mac/DOM/DOMMouseEvent.h
    mac/DOM/DOMMutationEvent.h
    mac/DOM/DOMNamedNodeMap.h
    mac/DOM/DOMNamedNodeMapInternal.h
    mac/DOM/DOMNode.h
    mac/DOM/DOMNodeFilter.h
    mac/DOM/DOMNodeInternal.h
    mac/DOM/DOMNodeIterator.h
    mac/DOM/DOMNodeIteratorInternal.h
    mac/DOM/DOMNodeList.h
    mac/DOM/DOMNodeListInternal.h
    mac/DOM/DOMNodePrivate.h
    mac/DOM/DOMObject.h
    mac/DOM/DOMOverflowEvent.h
    mac/DOM/DOMPrivate.h
    mac/DOM/DOMProcessingInstruction.h
    mac/DOM/DOMProcessingInstructionInternal.h
    mac/DOM/DOMProgressEvent.h
    mac/DOM/DOMRGBColor.h
    mac/DOM/DOMRGBColorInternal.h
    mac/DOM/DOMRange.h
    mac/DOM/DOMRangeException.h
    mac/DOM/DOMRangeInternal.h
    mac/DOM/DOMRanges.h
    mac/DOM/DOMRect.h
    mac/DOM/DOMRectInternal.h
    mac/DOM/DOMStyleSheet.h
    mac/DOM/DOMStyleSheetInternal.h
    mac/DOM/DOMStyleSheetList.h
    mac/DOM/DOMStyleSheetListInternal.h
    mac/DOM/DOMStylesheets.h
    mac/DOM/DOMText.h
    mac/DOM/DOMTextEvent.h
    mac/DOM/DOMTextInternal.h
    mac/DOM/DOMTimeRanges.h
    mac/DOM/DOMTimeRangesInternal.h
    mac/DOM/DOMTokenList.h
    mac/DOM/DOMTokenListInternal.h
    mac/DOM/DOMTraversal.h
    mac/DOM/DOMTreeWalker.h
    mac/DOM/DOMTreeWalkerInternal.h
    mac/DOM/DOMUIEvent.h
    mac/DOM/DOMUIKitExtensions.h
    mac/DOM/DOMViews.h
    mac/DOM/DOMWheelEvent.h
    mac/DOM/DOMWheelEventInternal.h
    mac/DOM/DOMXPath.h
    mac/DOM/DOMXPathException.h
    mac/DOM/DOMXPathExpression.h
    mac/DOM/DOMXPathExpressionInternal.h
    mac/DOM/DOMXPathNSResolver.h
    mac/DOM/DOMXPathResult.h
    mac/DOM/DOMXPathResultInternal.h
    mac/DOM/ExceptionHandlers.h
    mac/DOM/ObjCEventListener.h
    mac/DOM/ObjCNodeFilterCondition.h
    mac/DOM/WebAutocapitalizeTypes.h
    mac/DOM/WebDOMOperations.h
    mac/DOM/WebDOMOperationsInternal.h
    mac/DOM/WebDOMOperationsPrivate.h

    mac/DefaultDelegates/WebDefaultContextMenuDelegate.h
    mac/DefaultDelegates/WebDefaultEditingDelegate.h
    mac/DefaultDelegates/WebDefaultPolicyDelegate.h
    mac/DefaultDelegates/WebDefaultUIDelegate.h

    mac/History/BackForwardList.h
    mac/History/BinaryPropertyList.h
    mac/History/HistoryPropertyList.h
    mac/History/WebBackForwardList.h
    mac/History/WebBackForwardListInternal.h
    mac/History/WebBackForwardListPrivate.h
    mac/History/WebHistory.h
    mac/History/WebHistoryInternal.h
    mac/History/WebHistoryItem.h
    mac/History/WebHistoryItemInternal.h
    mac/History/WebHistoryItemPrivate.h
    mac/History/WebHistoryPrivate.h
    mac/History/WebURLsWithTitles.h

    mac/Misc/WebCache.h
    mac/Misc/WebCoreStatistics.h
    mac/Misc/WebDownload.h
    mac/Misc/WebElementDictionary.h
    mac/Misc/WebIconDatabase.h
    mac/Misc/WebKit.h
    mac/Misc/WebKitErrors.h
    mac/Misc/WebKitErrorsPrivate.h
    mac/Misc/WebKitLogging.h
    mac/Misc/WebKitNSStringExtras.h
    mac/Misc/WebKitStatistics.h
    mac/Misc/WebKitStatisticsPrivate.h
    mac/Misc/WebKitVersionChecks.h
    mac/Misc/WebLocalizableStrings.h
    mac/Misc/WebLocalizableStringsInternal.h
    mac/Misc/WebNSControlExtras.h
    mac/Misc/WebNSDataExtras.h
    mac/Misc/WebNSDictionaryExtras.h
    mac/Misc/WebNSEventExtras.h
    mac/Misc/WebNSFileManagerExtras.h
    mac/Misc/WebNSImageExtras.h
    mac/Misc/WebNSObjectExtras.h
    mac/Misc/WebNSPasteboardExtras.h
    mac/Misc/WebNSPrintOperationExtras.h
    mac/Misc/WebNSURLExtras.h
    mac/Misc/WebNSURLRequestExtras.h
    mac/Misc/WebNSUserDefaultsExtras.h
    mac/Misc/WebNSViewExtras.h
    mac/Misc/WebNSWindowExtras.h
    mac/Misc/WebQuotaManager.h
    mac/Misc/WebSharingServicePickerController.h
    mac/Misc/WebStringTruncator.h
    mac/Misc/WebUserContentURLPattern.h

    mac/Panels/WebAuthenticationPanel.h
    mac/Panels/WebPanelAuthenticationHandler.h

    mac/Plugins/WebBasePluginPackage.h
    mac/Plugins/WebPlugin.h
    mac/Plugins/WebPluginContainer.h
    mac/Plugins/WebPluginContainerCheck.h
    mac/Plugins/WebPluginContainerPrivate.h
    mac/Plugins/WebPluginController.h
    mac/Plugins/WebPluginDatabase.h
    mac/Plugins/WebPluginPackage.h
    mac/Plugins/WebPluginPackagePrivate.h
    mac/Plugins/WebPluginViewFactory.h
    mac/Plugins/WebPluginViewFactoryPrivate.h

    mac/Storage/WebDatabaseManagerClient.h
    mac/Storage/WebDatabaseManagerInternal.h
    mac/Storage/WebDatabaseManagerPrivate.h
    mac/Storage/WebDatabaseQuotaManager.h
    mac/Storage/WebStorageManagerInternal.h
    mac/Storage/WebStorageManagerPrivate.h
    mac/Storage/WebStorageTrackerClient.h

    mac/WebCoreSupport/CorrectionPanel.h
    mac/WebCoreSupport/PopupMenuMac.h
    mac/WebCoreSupport/SearchPopupMenuMac.h
    mac/WebCoreSupport/WebAlternativeTextClient.h
    mac/WebCoreSupport/WebApplicationCache.h
    mac/WebCoreSupport/WebApplicationCacheInternal.h
    mac/WebCoreSupport/WebApplicationCacheQuotaManager.h
    mac/WebCoreSupport/WebChromeClient.h
    mac/WebCoreSupport/WebContextMenuClient.h
    mac/WebCoreSupport/WebCreateFragmentInternal.h
    mac/WebCoreSupport/WebDragClient.h
    mac/WebCoreSupport/WebEditorClient.h
    mac/WebCoreSupport/WebFrameLoaderClient.h
    mac/WebCoreSupport/WebFrameNetworkingContext.h
    mac/WebCoreSupport/WebGeolocationClient.h
    mac/WebCoreSupport/WebInspectorClient.h
    mac/WebCoreSupport/WebJavaScriptTextInputPanel.h
    mac/WebCoreSupport/WebKitFullScreenListener.h
    mac/WebCoreSupport/WebNotificationClient.h
    mac/WebCoreSupport/WebOpenPanelResultListener.h
    mac/WebCoreSupport/WebPaymentCoordinatorClient.h
    mac/WebCoreSupport/WebPlatformStrategies.h
    mac/WebCoreSupport/WebPluginInfoProvider.h
    mac/WebCoreSupport/WebProgressTrackerClient.h
    mac/WebCoreSupport/WebSecurityOriginInternal.h
    mac/WebCoreSupport/WebSecurityOriginPrivate.h
    mac/WebCoreSupport/WebSelectionServiceController.h
    mac/WebCoreSupport/WebValidationMessageClient.h
    mac/WebCoreSupport/WebVisitedLinkStore.h

    mac/WebInspector/WebInspector.h
    mac/WebInspector/WebInspectorFrontend.h
    mac/WebInspector/WebInspectorPrivate.h
    mac/WebInspector/WebNodeHighlight.h
    mac/WebInspector/WebNodeHighlightView.h
    mac/WebInspector/WebNodeHighlighter.h

    mac/WebView/WebAllowDenyPolicyListener.h
    mac/WebView/WebArchive.h
    mac/WebView/WebArchiveInternal.h
    mac/WebView/WebClipView.h
    mac/WebView/WebDataSource.h
    mac/WebView/WebDataSourceInternal.h
    mac/WebView/WebDataSourcePrivate.h
    mac/WebView/WebDelegateImplementationCaching.h
    mac/WebView/WebDeviceOrientation.h
    mac/WebView/WebDeviceOrientationInternal.h
    mac/WebView/WebDeviceOrientationProvider.h
    mac/WebView/WebDeviceOrientationProviderMock.h
    mac/WebView/WebDeviceOrientationProviderMockInternal.h
    mac/WebView/WebDocument.h
    mac/WebView/WebDocumentInternal.h
    mac/WebView/WebDocumentLoaderMac.h
    mac/WebView/WebDocumentPrivate.h
    mac/WebView/WebDynamicScrollBarsView.h
    mac/WebView/WebDynamicScrollBarsViewInternal.h
    mac/WebView/WebEditingDelegate.h
    mac/WebView/WebEditingDelegatePrivate.h
    mac/WebView/WebFeature.h
    mac/WebView/WebFormDelegate.h
    mac/WebView/WebFormDelegatePrivate.h
    mac/WebView/WebFrame.h
    mac/WebView/WebFrameInternal.h
    mac/WebView/WebFrameLoadDelegate.h
    mac/WebView/WebFrameLoadDelegatePrivate.h
    mac/WebView/WebFramePrivate.h
    mac/WebView/WebFrameView.h
    mac/WebView/WebFrameViewInternal.h
    mac/WebView/WebFrameViewPrivate.h
    mac/WebView/WebFullScreenController.h
    mac/WebView/WebGeolocationPosition.h
    mac/WebView/WebGeolocationPositionInternal.h
    mac/WebView/WebHTMLRepresentation.h
    mac/WebView/WebHTMLRepresentationPrivate.h
    mac/WebView/WebHTMLView.h
    mac/WebView/WebHTMLViewInternal.h
    mac/WebView/WebHTMLViewPrivate.h
    mac/WebView/WebHistoryDelegate.h
    mac/WebView/WebImmediateActionController.h
    mac/WebView/WebIndicateLayer.h
    mac/WebView/WebJSPDFDoc.h
    mac/WebView/WebMediaPlaybackTargetPicker.h
    mac/WebView/WebNavigationData.h
    mac/WebView/WebNotification.h
    mac/WebView/WebNotificationInternal.h
    mac/WebView/WebPDFDocumentExtras.h
    mac/WebView/WebPDFRepresentation.h
    mac/WebView/WebPDFView.h
    mac/WebView/WebPolicyDelegate.h
    mac/WebView/WebPolicyDelegatePrivate.h
    mac/WebView/WebPreferenceKeysPrivate.h
    mac/WebView/WebPreferences.h
    mac/WebView/WebPreferencesPrivate.h
    mac/WebView/WebResource.h
    mac/WebView/WebResourceInternal.h
    mac/WebView/WebResourceLoadDelegate.h
    mac/WebView/WebResourceLoadDelegatePrivate.h
    mac/WebView/WebResourcePrivate.h
    mac/WebView/WebScriptDebugDelegate.h
    mac/WebView/WebScriptDebugger.h
    mac/WebView/WebScriptWorld.h
    mac/WebView/WebScriptWorldInternal.h
    mac/WebView/WebTextCompletionController.h
    mac/WebView/WebTextIterator.h
    mac/WebView/WebUIDelegate.h
    mac/WebView/WebUIDelegatePrivate.h
    mac/WebView/WebVideoFullscreenController.h
    mac/WebView/WebView.h
    mac/WebView/WebViewData.h
    mac/WebView/WebViewInternal.h
    mac/WebView/WebViewPrivate.h
    mac/WebView/WebWindowAnimation.h
)

# Add these to the project header map.
# FIXME: The "forwarding" step can probably be replaced with this headermap,
# since it makes the headers available via a framework-style import.
set(WebKitLegacy_PROJECT_HEADERS ${WebKitLegacy_LEGACY_FORWARDING_HEADERS_FILES})

list(APPEND WebKitLegacy_PUBLIC_FRAMEWORK_HEADERS
    ${WEBCORE_DIR}/bridge/objc/WebScriptObject.h
    ${WEBCORE_DIR}/platform/cocoa/WebKitAvailability.h
    ${WEBCORE_DIR}/platform/ios/AbstractPasteboard.h
    ${WEBCORE_DIR}/platform/ios/KeyEventCodesIOS.h
    ${WEBCORE_DIR}/platform/ios/WebEvent.h
    ${WEBCORE_DIR}/page/ios/WebEventRegion.h
    ${WEBCORE_DIR}/platform/ios/WebItemProviderPasteboard.h
    ${WEBCORE_DIR}/platform/ios/wak/WAKAppKitStubs.h
    ${WEBCORE_DIR}/platform/ios/wak/WAKResponder.h
    ${WEBCORE_DIR}/platform/ios/wak/WAKView.h
    ${WEBCORE_DIR}/platform/ios/wak/WAKWindow.h
    ${WEBCORE_DIR}/platform/ios/wak/WKContentObservation.h
    ${WEBCORE_DIR}/platform/ios/wak/WKGraphics.h
    ${WEBCORE_DIR}/platform/ios/wak/WKTypes.h
    ${WEBCORE_DIR}/platform/ios/wak/WebCoreThread.h
    ${WEBCORE_DIR}/platform/ios/wak/WebCoreThreadMessage.h
    ${WEBCORE_DIR}/platform/ios/wak/WebCoreThreadRun.h

    ios/Misc/WebGeolocationCoreLocationProvider.h
    ios/Misc/WebGeolocationProviderIOS.h
    ios/Misc/WebNSStringExtrasIOS.h
    ios/Misc/WebNSStringExtrasIPhone.h
    ios/Misc/WebUIKitSupport.h

    ios/WebCoreSupport/WebCaretChangeListener.h
    ios/WebCoreSupport/WebFixedPositionContent.h
    ios/WebCoreSupport/WebFrameIOS.h
    ios/WebCoreSupport/WebFrameIPhone.h
    ios/WebCoreSupport/WebGeolocationPrivate.h
    ios/WebCoreSupport/WebMIMETypeRegistry.h
    ios/WebCoreSupport/WebSelectionRect.h
    ios/WebCoreSupport/WebVisiblePosition.h

    ios/WebView/WebPDFViewIOS.h
    ios/WebView/WebPDFViewIPhone.h
    ios/WebView/WebPDFViewPlaceholder.h
    ios/WebView/WebUIKitDelegate.h

    mac/DOM/DOMInternal.h
    mac/DOM/DOMPrivate.h
    mac/DOM/ExceptionHandlers.h
    mac/DOM/ObjCEventListener.h
    mac/DOM/ObjCNodeFilterCondition.h
    mac/DOM/WebDOMOperationsInternal.h

    mac/DefaultDelegates/WebDefaultEditingDelegate.h
    mac/DefaultDelegates/WebDefaultPolicyDelegate.h
    mac/DefaultDelegates/WebDefaultUIDelegate.h

    mac/History/BackForwardList.h
    mac/History/WebBackForwardList.h
    mac/History/WebBackForwardListInternal.h
    mac/History/WebBackForwardListPrivate.h
    mac/History/WebHistory.h
    mac/History/WebHistoryInternal.h
    mac/History/WebHistoryItem.h
    mac/History/WebHistoryItemInternal.h
    mac/History/WebHistoryItemPrivate.h
    mac/History/WebHistoryPrivate.h

    mac/Misc/WebCache.h
    mac/Misc/WebCoreStatistics.h
    mac/Misc/WebDownload.h
    mac/Misc/WebElementDictionary.h
    mac/Misc/WebKit.h
    mac/Misc/WebKitErrors.h
    mac/Misc/WebKitErrorsPrivate.h
    mac/Misc/WebKitLogging.h
    mac/Misc/WebKitNSStringExtras.h
    mac/Misc/WebKitStatistics.h
    mac/Misc/WebKitStatisticsPrivate.h
    mac/Misc/WebKitVersionChecks.h
    mac/Misc/WebLocalizableStrings.h
    mac/Misc/WebLocalizableStringsInternal.h
    mac/Misc/WebNSDataExtras.h
    mac/Misc/WebNSDictionaryExtras.h
    mac/Misc/WebNSFileManagerExtras.h
    mac/Misc/WebNSObjectExtras.h
    mac/Misc/WebNSURLExtras.h
    mac/Misc/WebNSURLRequestExtras.h
    mac/Misc/WebNSUserDefaultsExtras.h
    mac/Misc/WebQuotaManager.h
    mac/Misc/WebUserContentURLPattern.h

    mac/Plugins/WebBasePluginPackage.h
    mac/Plugins/WebPlugin.h
    mac/Plugins/WebPluginContainer.h
    mac/Plugins/WebPluginContainerPrivate.h
    mac/Plugins/WebPluginController.h
    mac/Plugins/WebPluginDatabase.h
    mac/Plugins/WebPluginPackage.h
    mac/Plugins/WebPluginViewFactory.h
    mac/Plugins/WebPluginViewFactoryPrivate.h

    mac/Storage/WebDatabaseManagerClient.h
    mac/Storage/WebDatabaseManagerInternal.h
    mac/Storage/WebDatabaseManagerPrivate.h
    mac/Storage/WebDatabaseQuotaManager.h
    mac/Storage/WebStorageManagerInternal.h
    mac/Storage/WebStorageManagerPrivate.h
    mac/Storage/WebStorageTrackerClient.h

    mac/WebCoreSupport/WebApplicationCache.h
    mac/WebCoreSupport/WebApplicationCacheInternal.h
    mac/WebCoreSupport/WebApplicationCacheQuotaManager.h
    mac/WebCoreSupport/WebCreateFragmentInternal.h
    mac/WebCoreSupport/WebFrameLoaderClient.h
    mac/WebCoreSupport/WebKitFullScreenListener.h
    mac/WebCoreSupport/WebOpenPanelResultListener.h
    mac/WebCoreSupport/WebSecurityOriginInternal.h
    mac/WebCoreSupport/WebSecurityOriginPrivate.h

    mac/WebInspector/WebInspector.h
    mac/WebInspector/WebInspectorFrontend.h
    mac/WebInspector/WebInspectorPrivate.h
    mac/WebInspector/WebNodeHighlight.h
    mac/WebInspector/WebNodeHighlightView.h
    mac/WebInspector/WebNodeHighlighter.h

    mac/WebView/WebAllowDenyPolicyListener.h
    mac/WebView/WebArchive.h
    mac/WebView/WebArchiveInternal.h
    mac/WebView/WebDataSource.h
    mac/WebView/WebDataSourceInternal.h
    mac/WebView/WebDataSourcePrivate.h
    mac/WebView/WebDelegateImplementationCaching.h
    mac/WebView/WebDeviceOrientation.h
    mac/WebView/WebDeviceOrientationInternal.h
    mac/WebView/WebDeviceOrientationProvider.h
    mac/WebView/WebDeviceOrientationProviderMock.h
    mac/WebView/WebDeviceOrientationProviderMockInternal.h
    mac/WebView/WebDocument.h
    mac/WebView/WebDocumentInternal.h
    mac/WebView/WebDocumentLoaderMac.h
    mac/WebView/WebDocumentPrivate.h
    mac/WebView/WebEditingDelegate.h
    mac/WebView/WebEditingDelegatePrivate.h
    mac/WebView/WebFeature.h
    mac/WebView/WebFormDelegate.h
    mac/WebView/WebFormDelegatePrivate.h
    mac/WebView/WebFrame.h
    mac/WebView/WebFrameInternal.h
    mac/WebView/WebFrameLoadDelegate.h
    mac/WebView/WebFrameLoadDelegatePrivate.h
    mac/WebView/WebFramePrivate.h
    mac/WebView/WebFrameView.h
    mac/WebView/WebFrameViewInternal.h
    mac/WebView/WebFrameViewPrivate.h
    mac/WebView/WebGeolocationPosition.h
    mac/WebView/WebGeolocationPositionInternal.h
    mac/WebView/WebHTMLRepresentation.h
    mac/WebView/WebHTMLRepresentationPrivate.h
    mac/WebView/WebHistoryDelegate.h
    mac/WebView/WebIndicateLayer.h
    mac/WebView/WebMediaPlaybackTargetPicker.h
    mac/WebView/WebNavigationData.h
    mac/WebView/WebNotification.h
    mac/WebView/WebNotificationInternal.h
    mac/WebView/WebPolicyDelegate.h
    mac/WebView/WebPolicyDelegatePrivate.h
    mac/WebView/WebPreferenceKeysPrivate.h
    mac/WebView/WebPreferences.h
    mac/WebView/WebPreferencesPrivate.h
    mac/WebView/WebResource.h
    mac/WebView/WebResourceInternal.h
    mac/WebView/WebResourceLoadDelegate.h
    mac/WebView/WebResourceLoadDelegatePrivate.h
    mac/WebView/WebResourcePrivate.h
    mac/WebView/WebScriptDebugDelegate.h
    mac/WebView/WebScriptWorld.h
    mac/WebView/WebScriptWorldInternal.h
    mac/WebView/WebTextIterator.h
    mac/WebView/WebUIDelegate.h
    mac/WebView/WebUIDelegatePrivate.h
    mac/WebView/WebView.h
    mac/WebView/WebViewData.h
    mac/WebView/WebViewInternal.h
    mac/WebView/WebViewPrivate.h
)

# Headers in WebKitLegacy_LEGACY_FORWARDING_HEADERS_FILES that are not part of
# the iOS WebKitLegacy.framework Copy Headers phase in pbxproj. Mac-only files
# (WebClipView, WebDynamicScrollBarsView, NSWindow extras, plugin packages,
# AppKit panels, ...) and project-internal types (DOM*Internal.h, Web*Client.h,
# Web*Internal.h pairs) come along for the ride in the cmake list because the
# upstream forwarding-header recipe is shared with Mac. Restoring upstream
# WebKitLegacy.private.modulemap (umbrella "PrivateHeaders" + module *) requires
# this set to match Xcode's iOS WebKitLegacy.framework/PrivateHeaders/ exactly --
# otherwise the umbrella's auto-discovered submodules try to compile Mac-only
# headers and cycle through WebKit. https://bugs.webkit.org/show_bug.cgi?id=312083
set(_wkl_excluded_for_ios
    BackForwardList.h
    BinaryPropertyList.h
    CorrectionPanel.h
    DOMAbstractViewFrame.h
    DOMAbstractViewInternal.h
    DOMAttrInternal.h
    DOMBlobInternal.h
    DOMCDATASectionInternal.h
    DOMCSSPrimitiveValueInternal.h
    DOMCSSRuleInternal.h
    DOMCSSRuleListInternal.h
    DOMCSSStyleDeclarationInternal.h
    DOMCSSStyleSheetInternal.h
    DOMCSSValueInternal.h
    DOMCommentInternal.h
    DOMCounterInternal.h
    DOMCustomXPathNSResolver.h
    DOMDocumentFragmentInternal.h
    DOMDocumentInternal.h
    DOMDocumentTypeInternal.h
    DOMElementInternal.h
    DOMEventInternal.h
    DOMFileInternal.h
    DOMFileListInternal.h
    DOMHTMLAreaElementInternal.h
    DOMHTMLCollectionInternal.h
    DOMHTMLDocumentInternal.h
    DOMHTMLElementInternal.h
    DOMHTMLFormElementInternal.h
    DOMHTMLHeadElementInternal.h
    DOMHTMLImageElementInternal.h
    DOMHTMLInputElementInternal.h
    DOMHTMLLinkElementInternal.h
    DOMHTMLOptionElementInternal.h
    DOMHTMLOptionsCollectionInternal.h
    DOMHTMLScriptElementInternal.h
    DOMHTMLSelectElementInternal.h
    DOMHTMLStyleElementInternal.h
    DOMHTMLTableCaptionElementInternal.h
    DOMHTMLTableCellElementInternal.h
    DOMHTMLTableColElementInternal.h
    DOMHTMLTableSectionElementInternal.h
    DOMHTMLTextAreaElementInternal.h
    DOMImplementationInternal.h
    DOMInternal.h
    DOMMediaErrorInternal.h
    DOMMediaListInternal.h
    DOMNamedNodeMapInternal.h
    DOMNodeInternal.h
    DOMNodeIteratorInternal.h
    DOMNodeListInternal.h
    DOMProcessingInstructionInternal.h
    DOMRGBColorInternal.h
    DOMRangeInternal.h
    DOMRectInternal.h
    DOMStyleSheetInternal.h
    DOMStyleSheetListInternal.h
    DOMTextInternal.h
    DOMTimeRangesInternal.h
    DOMTokenList.h
    DOMTokenListInternal.h
    DOMTreeWalkerInternal.h
    DOMWheelEventInternal.h
    DOMXPathExpressionInternal.h
    DOMXPathResultInternal.h
    ExceptionHandlers.h
    HistoryPropertyList.h
    ObjCEventListener.h
    ObjCNodeFilterCondition.h
    PopupMenuMac.h
    SearchPopupMenuMac.h
    WebAlternativeTextClient.h
    WebApplicationCache.h
    WebApplicationCacheInternal.h
    WebApplicationCacheQuotaManager.h
    WebArchiveInternal.h
    WebAuthenticationPanel.h
    WebBackForwardListInternal.h
    WebBasePluginPackage.h
    WebChromeClient.h
    WebClipView.h
    WebContextMenuClient.h
    WebDOMOperationsInternal.h
    WebDataSourceInternal.h
    WebDatabaseManagerClient.h
    WebDatabaseManagerInternal.h
    WebDefaultContextMenuDelegate.h
    WebDefaultEditingDelegate.h
    WebDefaultUIDelegate.h
    WebDelegateImplementationCaching.h
    WebDeviceOrientationInternal.h
    WebDeviceOrientationProviderMockInternal.h
    WebDocumentInternal.h
    WebDocumentLoaderMac.h
    WebDragClient.h
    WebDynamicScrollBarsView.h
    WebDynamicScrollBarsViewInternal.h
    WebEditorClient.h
    WebElementDictionary.h
    WebFrameInternal.h
    WebFrameLoaderClient.h
    WebFrameNetworkingContext.h
    WebFrameViewInternal.h
    WebFullScreenController.h
    WebGeolocationClient.h
    WebGeolocationPositionInternal.h
    WebHTMLViewInternal.h
    WebHistoryDelegate.h
    WebHistoryInternal.h
    WebHistoryItemInternal.h
    WebIconDatabase.h
    WebImmediateActionController.h
    WebIndicateLayer.h
    WebInspectorClient.h
    WebInspectorFrontend.h
    WebJSPDFDoc.h
    WebJavaScriptTextInputPanel.h
    WebKitFullScreenListener.h
    WebKitLogging.h
    WebKitStatisticsPrivate.h
    WebKitVersionChecks.h
    WebLocalizableStringsInternal.h
    WebMediaPlaybackTargetPicker.h
    WebNSControlExtras.h
    WebNSDataExtras.h
    WebNSDictionaryExtras.h
    WebNSEventExtras.h
    WebNSImageExtras.h
    WebNSObjectExtras.h
    WebNSPasteboardExtras.h
    WebNSPrintOperationExtras.h
    WebNSURLRequestExtras.h
    WebNSWindowExtras.h
    WebNodeHighlight.h
    WebNodeHighlightView.h
    WebNodeHighlighter.h
    WebNotificationClient.h
    WebNotificationInternal.h
    WebOpenPanelResultListener.h
    WebPDFDocumentExtras.h
    WebPDFRepresentation.h
    WebPDFView.h
    WebPanelAuthenticationHandler.h
    WebPaymentCoordinatorClient.h
    WebPlatformStrategies.h
    WebPluginContainerCheck.h
    WebPluginController.h
    WebPluginInfoProvider.h
    WebPluginPackage.h
    WebProgressTrackerClient.h
    WebResourceInternal.h
    WebScriptDebugger.h
    WebScriptWorldInternal.h
    WebSecurityOriginInternal.h
    WebSelectionServiceController.h
    WebSharingServicePickerController.h
    WebStorageManagerInternal.h
    WebStorageTrackerClient.h
    WebStringTruncator.h
    WebTextCompletionController.h
    WebValidationMessageClient.h
    WebVideoFullscreenController.h
    WebViewData.h
    WebViewInternal.h
    WebVisitedLinkStore.h
    WebWindowAnimation.h
)

# Apply the same exclusion to WebKitLegacy_PUBLIC_FRAMEWORK_HEADERS so
# WEBKIT_COPY_FILES doesn't stage them. Also drop ${WEBCORE_DIR} entries: the
# foreach below stages forwarding stubs instead.
set(_wkl_filtered "")
foreach (_path IN LISTS WebKitLegacy_PUBLIC_FRAMEWORK_HEADERS)
    get_filename_component(_pathname "${_path}" NAME)
    cmake_path(IS_PREFIX WEBCORE_DIR "${_path}" NORMALIZE _is_from_webcore)
    if (NOT _pathname IN_LIST _wkl_excluded_for_ios AND NOT _is_from_webcore)
        list(APPEND _wkl_filtered "${_path}")
    endif ()
endforeach ()
set(WebKitLegacy_PUBLIC_FRAMEWORK_HEADERS ${_wkl_filtered})
unset(_wkl_filtered)
unset(_pathname)

# Source/ThirdParty/unifdef/CMakeLists.txt seeds UNIFDEF_EXECUTABLE to a
# build-tree path that doesn't exist yet on first configure; find_program
# short-circuits on a set cache var, so re-find from /usr/bin if stale.
if (UNIFDEF_EXECUTABLE AND NOT EXISTS "${UNIFDEF_EXECUTABLE}")
    unset(UNIFDEF_EXECUTABLE CACHE)
endif ()
find_program(UNIFDEF_EXECUTABLE unifdef
    HINTS /usr/bin
    DOC "unifdef tool used by postprocess-header-rule"
    REQUIRED)
# Configure-time scratch dir for the migrate+postprocess pipeline. Must
# exist before the foreach below writes its temporary input/output files.
set(_wkl_migrate_tmp_dir "${CMAKE_BINARY_DIR}/WebKitLegacy/_migrate_tmp")
file(MAKE_DIRECTORY "${_wkl_migrate_tmp_dir}")

# unifdef flags mirror Source/WebKitLegacy/scripts/postprocess-header-rule.
# WK_PLATFORM_NAME = iphonesimulator | iphoneos. CMAKE_OSX_SYSROOT distinguishes.
if (CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
    set(_wkl_unifdef_args -B -DTARGET_OS_IPHONE=1 -DTARGET_OS_SIMULATOR=1 -DUSE_APPLE_INTERNAL_SDK=1)
else ()
    set(_wkl_unifdef_args -B -DTARGET_OS_IPHONE=1 -DTARGET_OS_SIMULATOR=0 -DUSE_APPLE_INTERNAL_SDK=1)
endif ()
# ENABLE_TOUCH_EVENTS / ENABLE_IOS_GESTURE_EVENTS: postprocess uses 0 / 1
# depending on the env value being non-empty. Mirror via cmake variable check.
foreach (_feat ENABLE_TOUCH_EVENTS ENABLE_IOS_GESTURE_EVENTS)
    if (${_feat})
        list(APPEND _wkl_unifdef_args "-D${_feat}=1")
    else ()
        list(APPEND _wkl_unifdef_args "-D${_feat}=0")
    endif ()
endforeach ()

foreach (_file ${WebKitLegacy_LEGACY_FORWARDING_HEADERS_FILES})
    get_filename_component(_name "${_file}" NAME)
    if (_name IN_LIST _wkl_excluded_for_ios)
        continue ()
    endif ()
    set(_target_filename "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}/WebKitLegacy/${_name}")
    if (IS_ABSOLUTE "${_file}")
        set(_src_path "${_file}")
    else ()
        set(_src_path "${CMAKE_CURRENT_SOURCE_DIR}/${_file}")
    endif ()
    # Stage 1 -- migrate-header-rule:
    #   sed -E -e 's/<WebCore\//<WebKitLegacy\//' -e 's/(^ *)WEBCORE_EXPORT /\1/'
    # Mac-only `<WebCore/...>` imports become `<WebKitLegacy/...>` because Xcode
    # also re-stages those WebCore headers under the WebKitLegacy framework.
    # Stripping WEBCORE_EXPORT avoids the macro being undefined in the consumer's
    # parse context. The prior `#import "/abs/path/source.h"` stub form pulled
    # the original Mac source straight in and exposed Mac-only macros to iOS.
    file(READ "${_src_path}" _migrated)
    string(REPLACE "<WebCore/" "<WebKitLegacy/" _migrated "${_migrated}")
    # `(^ *)WEBCORE_EXPORT ` from the sed rule -- multi-line, line-anchored.
    # CMake's REGEX REPLACE treats the input as a single string with `^` matching
    # only the very start, so use explicit `\n` plus a separate pass for the first line.
    string(REGEX REPLACE "(\n[ \t]*)WEBCORE_EXPORT " "\\1" _migrated "${_migrated}")
    string(REGEX REPLACE "^([ \t]*)WEBCORE_EXPORT " "\\1" _migrated "${_migrated}")
    # Stage 2 -- postprocess-header-rule:
    # 2a. Strip `WEBKIT_*_MAC(...)` annotations (non-mac, non-WebKitAvailability.h).
    #     The macros aren't defined on iOS, so leaving them in source-style headers
    #     produces ObjC `@property has a previous declaration` errors when the
    #     migrated copy and the original are both reachable in a unified build TU
    #     (mac/DOM/X.mm `#import "X.h"` -> source; chain via DOMPrivate.h ->
    #     `<WebKitLegacy/X.h>` -> migrated; both parse).
    if (NOT _name STREQUAL "WebKitAvailability.h")
        string(REGEX REPLACE " *WEBKIT_(CLASS_|ENUM_)?(AVAILABLE|DEPRECATED)_MAC\\([^)]+\\)" "" _migrated "${_migrated}")
    endif ()
    # 2b. unifdef: strip `#if TARGET_OS_IPHONE` / etc branches. Run via
    # stdin/stdout to avoid an intermediate file. Exit codes 0 and 1 are
    # success (1 = file was modified); 2+ are errors.
    set(_unifdef_input "${_wkl_migrate_tmp_dir}/in_${_name}")
    set(_unifdef_output "${_wkl_migrate_tmp_dir}/out_${_name}")
    file(WRITE "${_unifdef_input}" "${_migrated}")
    execute_process(
        COMMAND "${UNIFDEF_EXECUTABLE}" ${_wkl_unifdef_args} -o "${_unifdef_output}" "${_unifdef_input}"
        RESULT_VARIABLE _unifdef_rc
        OUTPUT_VARIABLE _unifdef_stdout
        ERROR_VARIABLE _unifdef_stderr
    )
    if (_unifdef_rc GREATER 1)
        message(FATAL_ERROR "unifdef rc=${_unifdef_rc} for ${_src_path}\nstdout: ${_unifdef_stdout}\nstderr: ${_unifdef_stderr}")
    endif ()
    file(READ "${_unifdef_output}" _migrated)
    file(REMOVE "${_unifdef_input}" "${_unifdef_output}")

    set(_existing "")
    if (EXISTS "${_target_filename}")
        file(READ "${_target_filename}" _existing)
    endif ()
    if (NOT _existing STREQUAL _migrated)
        file(REMOVE "${_target_filename}")
        file(WRITE "${_target_filename}" "${_migrated}")
    endif ()
endforeach ()

# Symlink WebKit/ -> WebKitLegacy/ for <WebKit/...> imports.
if (NOT EXISTS "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}/WebKit")
    file(CREATE_LINK "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}/WebKitLegacy"
                     "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}/WebKit" SYMBOLIC)
endif ()

set(_wkl_fw "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKitLegacy.framework")
make_directory("${_wkl_fw}")
make_directory("${_wkl_fw}/Modules")

if (NOT EXISTS "${_wkl_fw}/PrivateHeaders")
    file(CREATE_LINK "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}/WebKitLegacy"
                     "${_wkl_fw}/PrivateHeaders" SYMBOLIC)
endif ()

configure_file(${WEBKITLEGACY_DIR}/Modules/WebKitLegacy.modulemap
               ${_wkl_fw}/Modules/module.modulemap COPYONLY)
set(_wkl_modulemap_body "")
foreach (_file ${WebKitLegacy_LEGACY_FORWARDING_HEADERS_FILES})
    get_filename_component(_name "${_file}" NAME)
    if (_name IN_LIST _wkl_excluded_for_ios)
        continue ()
    endif ()
    string(APPEND _wkl_modulemap_body "    header \"${_name}\"\n")
endforeach ()
string(APPEND _wkl_modulemap_body "    header \"WorkAround173516139.h\"\n")
file(WRITE ${_wkl_fw}/Modules/module.private.modulemap
"framework module WebKitLegacy [system] {
${_wkl_modulemap_body}    export *
}
")
unset(_wkl_modulemap_body)
configure_file(${WEBKITLEGACY_DIR}/Modules/WorkAround173516139.h
               ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}/WebKitLegacy/WorkAround173516139.h
               COPYONLY)

# Generate a clang VFS overlay for WebKitLegacy's own ObjC++ build, mirroring
# what Xcode emits at WebKitLegacy.build/.../*-VFS-iphonesimulator/all-product-headers.yaml.
# When WebKitLegacy.mm files compile, both the local `#import "X.h"` (resolved
# to Source/WebKitLegacy/mac/.../X.h) and `#import <WebKitLegacy/X.h>` (resolved
# via -F to WebKitLegacy.framework/PrivateHeaders/X.h, the migrated copy) end up
# parsing two paths whose @property declarations clang treats as a redeclaration
# (`property has previous declaration`) -- even when content is byte-identical
# after preprocessing. The overlay redirects PrivateHeaders/X.h *at compile
# time* to the source path, so both routes point to the same inode and clang's
# `#import` dedupes. The on-disk migrated copy is kept for installhdrs / clients
# that import the SDK directly.
set(_wkl_vfs "${CMAKE_BINARY_DIR}/WebKitLegacy-vfs-overlay.yaml")
set(_wkl_vfs_modules_entries "")
list(APPEND _wkl_vfs_modules_entries
    "{\"type\":\"file\",\"name\":\"module.modulemap\",\"external-contents\":\"${_wkl_fw}/Modules/module.modulemap\"}"
    "{\"type\":\"file\",\"name\":\"module.private.modulemap\",\"external-contents\":\"${_wkl_fw}/Modules/module.private.modulemap\"}"
)
set(_wkl_vfs_headers_entries "")
foreach (_file ${WebKitLegacy_LEGACY_FORWARDING_HEADERS_FILES})
    get_filename_component(_name "${_file}" NAME)
    if (_name IN_LIST _wkl_excluded_for_ios)
        continue ()
    endif ()
    if (IS_ABSOLUTE "${_file}")
        set(_src "${_file}")
    else ()
        set(_src "${CMAKE_CURRENT_SOURCE_DIR}/${_file}")
    endif ()
    list(APPEND _wkl_vfs_headers_entries
        "{\"type\":\"file\",\"name\":\"${_name}\",\"external-contents\":\"${_src}\"}"
    )
endforeach ()
list(JOIN _wkl_vfs_modules_entries "," _wkl_vfs_modules_str)
list(JOIN _wkl_vfs_headers_entries "," _wkl_vfs_headers_str)
file(WRITE "${_wkl_vfs}"
"{\"case-sensitive\":\"false\",\"version\":0,\"roots\":[\
{\"type\":\"directory\",\"name\":\"${_wkl_fw}/Modules\",\"contents\":[${_wkl_vfs_modules_str}]},\
{\"type\":\"directory\",\"name\":\"${_wkl_fw}/PrivateHeaders\",\"contents\":[${_wkl_vfs_headers_str}]},\
{\"type\":\"directory\",\"name\":\"${WebKitLegacy_FRAMEWORK_HEADERS_DIR}/WebKitLegacy\",\"contents\":[${_wkl_vfs_headers_str}]}\
]}\n")
target_compile_options(WebKitLegacy PRIVATE
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:SHELL:-ivfsoverlay ${_wkl_vfs}>")
unset(_wkl_vfs)
unset(_wkl_vfs_modules_entries)
unset(_wkl_vfs_modules_str)
unset(_wkl_vfs_headers_entries)
unset(_wkl_vfs_headers_str)
