/*
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Include in here only header files that are common to all platforms, and that
// can be included from C and C++ source files. Files that should only be
// included from C++ source files should be enumerated in the special "cpp"
// section in WebKitLegacy_macOS.private.modulemap and/or
// WebKitLegacy_iOS.private.modulemap.

#import <WebKitLegacy/DOM.h>
#import <WebKitLegacy/DOMAbstractView.h>
#import <WebKitLegacy/DOMAttr.h>
#import <WebKitLegacy/DOMBlob.h>
#import <WebKitLegacy/DOMCDATASection.h>
#import <WebKitLegacy/DOMCSS.h>
#import <WebKitLegacy/DOMCSSCharsetRule.h>
#import <WebKitLegacy/DOMCSSFontFaceRule.h>
#import <WebKitLegacy/DOMCSSImportRule.h>
#import <WebKitLegacy/DOMCSSMediaRule.h>
#import <WebKitLegacy/DOMCSSPageRule.h>
#import <WebKitLegacy/DOMCSSPrimitiveValue.h>
#import <WebKitLegacy/DOMCSSRule.h>
#import <WebKitLegacy/DOMCSSRuleList.h>
#import <WebKitLegacy/DOMCSSStyleDeclaration.h>
#import <WebKitLegacy/DOMCSSStyleRule.h>
#import <WebKitLegacy/DOMCSSStyleSheet.h>
#import <WebKitLegacy/DOMCSSUnknownRule.h>
#import <WebKitLegacy/DOMCSSValue.h>
#import <WebKitLegacy/DOMCSSValueList.h>
#import <WebKitLegacy/DOMCharacterData.h>
#import <WebKitLegacy/DOMComment.h>
#import <WebKitLegacy/DOMCore.h>
#import <WebKitLegacy/DOMCounter.h>
#import <WebKitLegacy/DOMDocument.h>
#import <WebKitLegacy/DOMDocumentFragment.h>
#import <WebKitLegacy/DOMDocumentFragmentPrivate.h>
#import <WebKitLegacy/DOMDocumentPrivate.h>
#import <WebKitLegacy/DOMDocumentType.h>
#import <WebKitLegacy/DOMElement.h>
#import <WebKitLegacy/DOMEntity.h>
#import <WebKitLegacy/DOMEntityReference.h>
#import <WebKitLegacy/DOMEvent.h>
#import <WebKitLegacy/DOMEventException.h>
#import <WebKitLegacy/DOMEventListener.h>
#import <WebKitLegacy/DOMEventTarget.h>
#import <WebKitLegacy/DOMEvents.h>
#import <WebKitLegacy/DOMException.h>
#import <WebKitLegacy/DOMExtensions.h>
#import <WebKitLegacy/DOMFile.h>
#import <WebKitLegacy/DOMFileList.h>
#import <WebKitLegacy/DOMHTML.h>
#import <WebKitLegacy/DOMHTMLAnchorElement.h>
#import <WebKitLegacy/DOMHTMLAppletElement.h>
#import <WebKitLegacy/DOMHTMLAreaElement.h>
#import <WebKitLegacy/DOMHTMLBRElement.h>
#import <WebKitLegacy/DOMHTMLBaseElement.h>
#import <WebKitLegacy/DOMHTMLBaseFontElement.h>
#import <WebKitLegacy/DOMHTMLBodyElement.h>
#import <WebKitLegacy/DOMHTMLButtonElement.h>
#import <WebKitLegacy/DOMHTMLCanvasElement.h>
#import <WebKitLegacy/DOMHTMLCollection.h>
#import <WebKitLegacy/DOMHTMLDListElement.h>
#import <WebKitLegacy/DOMHTMLDirectoryElement.h>
#import <WebKitLegacy/DOMHTMLDivElement.h>
#import <WebKitLegacy/DOMHTMLDocument.h>
#import <WebKitLegacy/DOMHTMLElement.h>
#import <WebKitLegacy/DOMHTMLElementPrivate.h>
#import <WebKitLegacy/DOMHTMLEmbedElement.h>
#import <WebKitLegacy/DOMHTMLFieldSetElement.h>
#import <WebKitLegacy/DOMHTMLFontElement.h>
#import <WebKitLegacy/DOMHTMLFormElement.h>
#import <WebKitLegacy/DOMHTMLFrameElement.h>
#import <WebKitLegacy/DOMHTMLFrameSetElement.h>
#import <WebKitLegacy/DOMHTMLHRElement.h>
#import <WebKitLegacy/DOMHTMLHeadElement.h>
#import <WebKitLegacy/DOMHTMLHeadingElement.h>
#import <WebKitLegacy/DOMHTMLHtmlElement.h>
#import <WebKitLegacy/DOMHTMLIFrameElement.h>
#import <WebKitLegacy/DOMHTMLImageElement.h>
#import <WebKitLegacy/DOMHTMLInputElement.h>
#import <WebKitLegacy/DOMHTMLInputElementPrivate.h>
#import <WebKitLegacy/DOMHTMLLIElement.h>
#import <WebKitLegacy/DOMHTMLLabelElement.h>
#import <WebKitLegacy/DOMHTMLLegendElement.h>
#import <WebKitLegacy/DOMHTMLLinkElement.h>
#import <WebKitLegacy/DOMHTMLMapElement.h>
#import <WebKitLegacy/DOMHTMLMarqueeElement.h>
#import <WebKitLegacy/DOMHTMLMediaElement.h>
#import <WebKitLegacy/DOMHTMLMenuElement.h>
#import <WebKitLegacy/DOMHTMLMetaElement.h>
#import <WebKitLegacy/DOMHTMLModElement.h>
#import <WebKitLegacy/DOMHTMLOListElement.h>
#import <WebKitLegacy/DOMHTMLObjectElement.h>
#import <WebKitLegacy/DOMHTMLOptGroupElement.h>
#import <WebKitLegacy/DOMHTMLOptionElement.h>
#import <WebKitLegacy/DOMHTMLOptionsCollection.h>
#import <WebKitLegacy/DOMHTMLParagraphElement.h>
#import <WebKitLegacy/DOMHTMLParamElement.h>
#import <WebKitLegacy/DOMHTMLPreElement.h>
#import <WebKitLegacy/DOMHTMLQuoteElement.h>
#import <WebKitLegacy/DOMHTMLScriptElement.h>
#import <WebKitLegacy/DOMHTMLSelectElement.h>
#import <WebKitLegacy/DOMHTMLStyleElement.h>
#import <WebKitLegacy/DOMHTMLTableCaptionElement.h>
#import <WebKitLegacy/DOMHTMLTableCellElement.h>
#import <WebKitLegacy/DOMHTMLTableColElement.h>
#import <WebKitLegacy/DOMHTMLTableElement.h>
#import <WebKitLegacy/DOMHTMLTableRowElement.h>
#import <WebKitLegacy/DOMHTMLTableSectionElement.h>
#import <WebKitLegacy/DOMHTMLTextAreaElement.h>
#import <WebKitLegacy/DOMHTMLTitleElement.h>
#import <WebKitLegacy/DOMHTMLUListElement.h>
#import <WebKitLegacy/DOMHTMLVideoElement.h>
#import <WebKitLegacy/DOMImplementation.h>
#import <WebKitLegacy/DOMKeyboardEvent.h>
#import <WebKitLegacy/DOMMediaError.h>
#import <WebKitLegacy/DOMMediaList.h>
#import <WebKitLegacy/DOMMouseEvent.h>
#import <WebKitLegacy/DOMMutationEvent.h>
#import <WebKitLegacy/DOMNamedNodeMap.h>
#import <WebKitLegacy/DOMNode.h>
#import <WebKitLegacy/DOMNodeFilter.h>
#import <WebKitLegacy/DOMNodeIterator.h>
#import <WebKitLegacy/DOMNodeList.h>
#import <WebKitLegacy/DOMNodePrivate.h>
#import <WebKitLegacy/DOMObject.h>
#import <WebKitLegacy/DOMOverflowEvent.h>
#import <WebKitLegacy/DOMPrivate.h>
#import <WebKitLegacy/DOMProcessingInstruction.h>
#import <WebKitLegacy/DOMProgressEvent.h>
#import <WebKitLegacy/DOMRGBColor.h>
#import <WebKitLegacy/DOMRange.h>
#import <WebKitLegacy/DOMRangeException.h>
#import <WebKitLegacy/DOMRanges.h>
#import <WebKitLegacy/DOMRect.h>
#import <WebKitLegacy/DOMStyleSheet.h>
#import <WebKitLegacy/DOMStyleSheetList.h>
#import <WebKitLegacy/DOMStylesheets.h>
#import <WebKitLegacy/DOMText.h>
#import <WebKitLegacy/DOMTextEvent.h>
#import <WebKitLegacy/DOMTimeRanges.h>
#import <WebKitLegacy/DOMTraversal.h>
#import <WebKitLegacy/DOMTreeWalker.h>
#import <WebKitLegacy/DOMUIEvent.h>
#import <WebKitLegacy/DOMViews.h>
#import <WebKitLegacy/DOMWheelEvent.h>
#import <WebKitLegacy/DOMXPath.h>
#import <WebKitLegacy/DOMXPathException.h>
#import <WebKitLegacy/DOMXPathExpression.h>
#import <WebKitLegacy/DOMXPathNSResolver.h>
#import <WebKitLegacy/DOMXPathResult.h>
// #import <WebKitLegacy/NSURLDownloadSPI.h> // Skipped in favor of NSURLDownload in Apple Internal SDK headers.
#import <WebKitLegacy/WebAllowDenyPolicyListener.h>
#import <WebKitLegacy/WebApplicationCache.h>
#import <WebKitLegacy/WebArchive.h>
#import <WebKitLegacy/WebAutocapitalizeTypes.h>
#import <WebKitLegacy/WebBackForwardList.h>
#import <WebKitLegacy/WebBackForwardListPrivate.h>
#import <WebKitLegacy/WebCache.h>
#import <WebKitLegacy/WebCoreStatistics.h>
// #import <WebKitLegacy/WebCreateFragmentInternal.h> // Handled in modulemap file due to requirement that it only be included from C++ source files.
#import <WebKitLegacy/WebDOMOperations.h>
#import <WebKitLegacy/WebDOMOperationsPrivate.h>
#import <WebKitLegacy/WebDataSource.h>
#import <WebKitLegacy/WebDataSourcePrivate.h>
#import <WebKitLegacy/WebDatabaseManagerPrivate.h>
#import <WebKitLegacy/WebDatabaseQuotaManager.h>
#import <WebKitLegacy/WebDefaultPolicyDelegate.h>
#import <WebKitLegacy/WebDeviceOrientation.h>
#import <WebKitLegacy/WebDeviceOrientationProvider.h>
#import <WebKitLegacy/WebDeviceOrientationProviderMock.h>
#import <WebKitLegacy/WebDocument.h>
#import <WebKitLegacy/WebDocumentPrivate.h>
#import <WebKitLegacy/WebDownload.h>
#import <WebKitLegacy/WebEditingDelegate.h>
#import <WebKitLegacy/WebEditingDelegatePrivate.h>
#import <WebKitLegacy/WebFeature.h>
#import <WebKitLegacy/WebFormDelegate.h>
#import <WebKitLegacy/WebFormDelegatePrivate.h>
#import <WebKitLegacy/WebFrame.h>
#import <WebKitLegacy/WebFrameLoadDelegate.h>
#import <WebKitLegacy/WebFrameLoadDelegatePrivate.h>
#import <WebKitLegacy/WebFramePrivate.h>
#import <WebKitLegacy/WebFrameView.h>
#import <WebKitLegacy/WebFrameViewPrivate.h>
#import <WebKitLegacy/WebGeolocationPosition.h>
#import <WebKitLegacy/WebHTMLRepresentation.h>
#import <WebKitLegacy/WebHTMLRepresentationPrivate.h>
#import <WebKitLegacy/WebHTMLView.h>
#import <WebKitLegacy/WebHTMLViewPrivate.h>
#import <WebKitLegacy/WebHistory.h>
#import <WebKitLegacy/WebHistoryItem.h>
#import <WebKitLegacy/WebHistoryItemPrivate.h>
#import <WebKitLegacy/WebHistoryPrivate.h>
#import <WebKitLegacy/WebInspector.h>
#import <WebKitLegacy/WebInspectorPrivate.h>
#import <WebKitLegacy/WebKit.h>
#import <WebKitLegacy/WebKitAvailability.h>
#import <WebKitLegacy/WebKitErrors.h>
#import <WebKitLegacy/WebKitErrorsPrivate.h>
#import <WebKitLegacy/WebKitNSStringExtras.h>
#import <WebKitLegacy/WebKitPluginHostTypes.h>
#import <WebKitLegacy/WebKitStatistics.h>
#import <WebKitLegacy/WebLocalizableStrings.h>
#import <WebKitLegacy/WebNSDataExtrasPrivate.h>
#import <WebKitLegacy/WebNSFileManagerExtras.h>
#import <WebKitLegacy/WebNSURLExtras.h>
#import <WebKitLegacy/WebNSUserDefaultsExtras.h>
#import <WebKitLegacy/WebNSViewExtras.h>
#import <WebKitLegacy/WebNavigationData.h>
#import <WebKitLegacy/WebNotification.h>
#import <WebKitLegacy/WebPlugin.h>
#import <WebKitLegacy/WebPluginContainer.h>
#import <WebKitLegacy/WebPluginContainerPrivate.h>
#import <WebKitLegacy/WebPluginDatabase.h>
#import <WebKitLegacy/WebPluginPackagePrivate.h>
#import <WebKitLegacy/WebPluginViewFactory.h>
#import <WebKitLegacy/WebPluginViewFactoryPrivate.h>
#import <WebKitLegacy/WebPolicyDelegate.h>
#import <WebKitLegacy/WebPolicyDelegatePrivate.h>
#import <WebKitLegacy/WebPreferenceKeysPrivate.h>
#import <WebKitLegacy/WebPreferences.h>
#import <WebKitLegacy/WebPreferencesPrivate.h>
#import <WebKitLegacy/WebQuotaManager.h>
#import <WebKitLegacy/WebResource.h>
#import <WebKitLegacy/WebResourceLoadDelegate.h>
#import <WebKitLegacy/WebResourceLoadDelegatePrivate.h>
#import <WebKitLegacy/WebResourcePrivate.h>
#import <WebKitLegacy/WebScriptDebugDelegate.h>
#import <WebKitLegacy/WebScriptObject.h>
#import <WebKitLegacy/WebScriptWorld.h>
#import <WebKitLegacy/WebSecurityOriginPrivate.h>
#import <WebKitLegacy/WebStorageManagerPrivate.h>
#import <WebKitLegacy/WebTextIterator.h>
#import <WebKitLegacy/WebUIDelegate.h>
#import <WebKitLegacy/WebUIDelegatePrivate.h>
#import <WebKitLegacy/WebURLsWithTitles.h>
#import <WebKitLegacy/WebUserContentURLPattern.h>
#import <WebKitLegacy/WebView.h>
#import <WebKitLegacy/WebViewPrivate.h>
