# Copyright (C) 2006, 2007, 2008, 2014 Apple Inc. All rights reserved.
# Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Migration of WebKit Legacy headers to WebKit.

ifeq ($(PLATFORM_NAME), macosx)

PUBLIC_HEADERS_DIR = $(TARGET_BUILD_DIR)/$(PUBLIC_HEADERS_FOLDER_PATH)
PRIVATE_HEADERS_DIR = $(TARGET_BUILD_DIR)/$(PRIVATE_HEADERS_FOLDER_PATH)

WEBKIT_PUBLIC_HEADERS = \
    CarbonUtils.h \
    DOM.h \
    DOMAbstractView.h \
    DOMAttr.h \
    DOMBlob.h \
    DOMCDATASection.h \
    DOMCSS.h \
    DOMCSSCharsetRule.h \
    DOMCSSFontFaceRule.h \
    DOMCSSImportRule.h \
    DOMCSSMediaRule.h \
    DOMCSSPageRule.h \
    DOMCSSPrimitiveValue.h \
    DOMCSSRule.h \
    DOMCSSRuleList.h \
    DOMCSSStyleDeclaration.h \
    DOMCSSStyleRule.h \
    DOMCSSStyleSheet.h \
    DOMCSSUnknownRule.h \
    DOMCSSValue.h \
    DOMCSSValueList.h \
    DOMCharacterData.h \
    DOMComment.h \
    DOMCore.h \
    DOMCounter.h \
    DOMDocument.h \
    DOMDocumentFragment.h \
    DOMDocumentType.h \
    DOMElement.h \
    DOMEntity.h \
    DOMEntityReference.h \
    DOMEvent.h \
    DOMEventException.h \
    DOMEventListener.h \
    DOMEventTarget.h \
    DOMEvents.h \
    DOMException.h \
    DOMExtensions.h \
    DOMFile.h \
    DOMFileList.h \
    DOMHTML.h \
    DOMHTMLAnchorElement.h \
    DOMHTMLAppletElement.h \
    DOMHTMLAreaElement.h \
    DOMHTMLBRElement.h \
    DOMHTMLBaseElement.h \
    DOMHTMLBaseFontElement.h \
    DOMHTMLBodyElement.h \
    DOMHTMLButtonElement.h \
    DOMHTMLCollection.h \
    DOMHTMLDListElement.h \
    DOMHTMLDirectoryElement.h \
    DOMHTMLDivElement.h \
    DOMHTMLDocument.h \
    DOMHTMLElement.h \
    DOMHTMLEmbedElement.h \
    DOMHTMLFieldSetElement.h \
    DOMHTMLFontElement.h \
    DOMHTMLFormElement.h \
    DOMHTMLFrameElement.h \
    DOMHTMLFrameSetElement.h \
    DOMHTMLHRElement.h \
    DOMHTMLHeadElement.h \
    DOMHTMLHeadingElement.h \
    DOMHTMLHtmlElement.h \
    DOMHTMLIFrameElement.h \
    DOMHTMLImageElement.h \
    DOMHTMLInputElement.h \
    DOMHTMLLIElement.h \
    DOMHTMLLabelElement.h \
    DOMHTMLLegendElement.h \
    DOMHTMLLinkElement.h \
    DOMHTMLMapElement.h \
    DOMHTMLMarqueeElement.h \
    DOMHTMLMenuElement.h \
    DOMHTMLMetaElement.h \
    DOMHTMLModElement.h \
    DOMHTMLOListElement.h \
    DOMHTMLObjectElement.h \
    DOMHTMLOptGroupElement.h \
    DOMHTMLOptionElement.h \
    DOMHTMLOptionsCollection.h \
    DOMHTMLParagraphElement.h \
    DOMHTMLParamElement.h \
    DOMHTMLPreElement.h \
    DOMHTMLQuoteElement.h \
    DOMHTMLScriptElement.h \
    DOMHTMLSelectElement.h \
    DOMHTMLStyleElement.h \
    DOMHTMLTableCaptionElement.h \
    DOMHTMLTableCellElement.h \
    DOMHTMLTableColElement.h \
    DOMHTMLTableElement.h \
    DOMHTMLTableRowElement.h \
    DOMHTMLTableSectionElement.h \
    DOMHTMLTextAreaElement.h \
    DOMHTMLTitleElement.h \
    DOMHTMLUListElement.h \
    DOMImplementation.h \
    DOMKeyboardEvent.h \
    DOMMediaList.h \
    DOMMouseEvent.h \
    DOMMutationEvent.h \
    DOMNamedNodeMap.h \
    DOMNode.h \
    DOMNodeFilter.h \
    DOMNodeIterator.h \
    DOMNodeList.h \
    DOMNotation.h \
    DOMObject.h \
    DOMOverflowEvent.h \
    DOMProcessingInstruction.h \
    DOMProgressEvent.h \
    DOMRGBColor.h \
    DOMRange.h \
    DOMRangeException.h \
    DOMRanges.h \
    DOMRect.h \
    DOMStyleSheet.h \
    DOMStyleSheetList.h \
    DOMStylesheets.h \
    DOMText.h \
    DOMTraversal.h \
    DOMTreeWalker.h \
    DOMUIEvent.h \
    DOMViews.h \
    DOMWheelEvent.h \
    DOMXPath.h \
    DOMXPathException.h \
    DOMXPathExpression.h \
    DOMXPathNSResolver.h \
    DOMXPathResult.h \
    HIWebView.h \
    WebArchive.h \
    WebBackForwardList.h \
    WebDOMOperations.h \
    WebDataSource.h \
    WebDocument.h \
    WebDownload.h \
    WebEditingDelegate.h \
    WebFrame.h \
    WebFrameLoadDelegate.h \
    WebFrameView.h \
    WebHistory.h \
    WebHistoryItem.h \
    WebKitAvailability.h \
    WebKitErrors.h \
    WebPlugin.h \
    WebPluginContainer.h \
    WebPluginViewFactory.h \
    WebPolicyDelegate.h \
    WebPreferences.h \
    WebResource.h \
    WebResourceLoadDelegate.h \
    WebScriptObject.h \
    WebUIDelegate.h \
    WebView.h \
    npapi.h \
    npfunctions.h \
    npruntime.h \
    nptypes.h \
#

WEBKIT_LEGACY_PUBLIC_HEADERS = $(addprefix $(PUBLIC_HEADERS_DIR)/, $(filter $(WEBKIT_PUBLIC_HEADERS), $(notdir $(wildcard $(BUILT_PRODUCTS_DIR)/WebKitLegacy.framework/PrivateHeaders/*))) WebKitLegacy.h)

WEBKIT_LEGACY_PRIVATE_HEADERS = $(addprefix $(PRIVATE_HEADERS_DIR)/, $(filter-out $(WEBKIT_PUBLIC_HEADERS) WebKit.h, $(notdir $(wildcard $(BUILT_PRODUCTS_DIR)/WebKitLegacy.framework/PrivateHeaders/*))))

WEBKIT_LEGACY_HEADER_REPLACE_RULES = -e s/\<WebKitLegacy/\<WebKit/
WEBKIT_LEGACY_HEADER_MIGRATE_CMD = sed $(WEBKIT_LEGACY_HEADER_REPLACE_RULES) $< > $@

PUBLIC_HEADER_CHECK_CMD = @if grep -q "AVAILABLE.*TBD" "$<"; then line=$$(awk "/AVAILABLE.*TBD/ { print FNR; exit }" "$<" ); echo "$<:$$line: error: A class within a public header has unspecified availability."; false; fi

$(PUBLIC_HEADERS_DIR)/% : $(BUILT_PRODUCTS_DIR)/WebKitLegacy.framework/PrivateHeaders/% MigrateHeadersFromWebKitLegacy.make
	$(PUBLIC_HEADER_CHECK_CMD)
	$(WEBKIT_LEGACY_HEADER_MIGRATE_CMD)

$(PRIVATE_HEADERS_DIR)/% : $(BUILT_PRODUCTS_DIR)/WebKitLegacy.framework/PrivateHeaders/% MigrateHeadersFromWebKitLegacy.make
	$(WEBKIT_LEGACY_HEADER_MIGRATE_CMD)

# Migration of WebKit2 headers to WebKit

WEBKIT2_HEADERS = \
    WKBackForwardList.h \
    WKBackForwardListItem.h \
    WKBackForwardListPrivate.h \
    WKFoundation.h \
    WKFrameInfo.h \
    WKHistoryDelegatePrivate.h \
    WKNavigation.h \
    WKNavigationAction.h \
    WKNavigationDelegate.h \
    WKNavigationDelegatePrivate.h \
    WKNavigationResponse.h \
    WKPreferences.h \
    WKProcessPool.h \
    WKProcessPoolPrivate.h \
    WKScriptMessage.h \
    WKScriptMessageHandler.h \
    WKScriptMessagePrivate.h \
    WKUIDelegate.h \
    WKUIDelegatePrivate.h \
    WKUserContentController.h \
    WKUserContentControllerPrivate.h \
    WKWebView.h \
    WKWebViewConfiguration.h \
    WKWebViewConfigurationPrivate.h \
    WKWebViewPrivate.h \
    _WKActivatedElementInfo.h \
    _WKDownload.h \
    _WKDownloadDelegate.h \
    _WKElementAction.h \
    _WKFormDelegate.h \
    _WKFormInputSession.h \
    _WKProcessPoolConfiguration.h \
    _WKScriptWorld.h \
    _WKThumbnailView.h \
    _WKVisitedLinkProvider.h \
#

WEBKIT2_PUBLIC_HEADERS = $(addprefix $(PUBLIC_HEADERS_DIR)/, $(filter $(WEBKIT2_HEADERS),$(notdir $(wildcard $(WEBKIT2_FRAMEWORKS_DIR)/WebKit2.framework/Headers/*))))
WEBKIT2_PRIVATE_HEADERS = $(addprefix $(PRIVATE_HEADERS_DIR)/, $(filter $(WEBKIT2_HEADERS),$(notdir $(wildcard $(WEBKIT2_FRAMEWORKS_DIR)/WebKit2.framework/PrivateHeaders/*))))

WEBKIT2_HEADER_REPLACE_RULES = -e s/\<WebKit2/\<WebKit/ -e /$\#.*\<WebKit\\/WK.*Ref\\.h\>/d
WEBKIT2_HEADER_MIGRATE_CMD = sed $(WEBKIT2_HEADER_REPLACE_RULES) $< > $@

$(PUBLIC_HEADERS_DIR)/% : $(WEBKIT2_FRAMEWORKS_DIR)/WebKit2.framework/Headers/% MigrateHeadersFromWebKitLegacy.make
	$(WEBKIT2_HEADER_MIGRATE_CMD)

$(PRIVATE_HEADERS_DIR)/% : $(WEBKIT2_FRAMEWORKS_DIR)/WebKit2.framework/PrivateHeaders/% MigrateHeadersFromWebKitLegacy.make
	$(WEBKIT2_HEADER_MIGRATE_CMD)

all : $(WEBKIT_LEGACY_PUBLIC_HEADERS) $(WEBKIT_LEGACY_PRIVATE_HEADERS) $(WEBKIT2_PUBLIC_HEADERS) $(WEBKIT2_PRIVATE_HEADERS)

$(PUBLIC_HEADERS_DIR)/WebKitLegacy.h : $(BUILT_PRODUCTS_DIR)/WebKitLegacy.framework/PrivateHeaders/WebKit.h MigrateHeadersFromWebKitLegacy.make
	$(PUBLIC_HEADER_CHECK_CMD)
	$(WEBKIT_LEGACY_HEADER_MIGRATE_CMD)

else

PRIVATE_HEADERS_DIR = $(TARGET_BUILD_DIR)/$(PRIVATE_HEADERS_FOLDER_PATH)

WEBKIT_LEGACY_PRIVATE_HEADERS = $(addprefix $(PRIVATE_HEADERS_DIR)/, $(filter-out WebKit.h, $(notdir $(wildcard $(BUILT_PRODUCTS_DIR)/WebKitLegacy.framework/PrivateHeaders/*))))

all : $(WEBKIT_LEGACY_PRIVATE_HEADERS) $(PRIVATE_HEADERS_DIR)/WebKitLegacy.h

WEBKIT_HEADER_MIGRATE_CMD = echo "\#import <WebKitLegacy/"`basename $<`">" > $@

$(PRIVATE_HEADERS_DIR)/% : $(BUILT_PRODUCTS_DIR)/WebKitLegacy.framework/PrivateHeaders/% MigrateHeadersFromWebKitLegacy.make
	$(WEBKIT_HEADER_MIGRATE_CMD)

$(PRIVATE_HEADERS_DIR)/WebKitLegacy.h : $(BUILT_PRODUCTS_DIR)/WebKitLegacy.framework/PrivateHeaders/WebKit.h MigrateHeadersFromWebKitLegacy.make
	$(WEBKIT_HEADER_MIGRATE_CMD)

endif
