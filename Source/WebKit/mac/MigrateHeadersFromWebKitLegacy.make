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

PRIVATE_HEADERS_DIR = $(BUILT_PRODUCTS_DIR)/$(PRIVATE_HEADERS_FOLDER_PATH)
PUBLIC_HEADERS_DIR = $(BUILT_PRODUCTS_DIR)/$(PUBLIC_HEADERS_FOLDER_PATH)

ifeq ($(WK_PLATFORM_NAME), macosx)

WEBKIT_PUBLIC_HEADERS = \
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

WEBKIT_LEGACY_PUBLIC_HEADERS = $(addprefix $(PUBLIC_HEADERS_DIR)/, $(filter $(WEBKIT_PUBLIC_HEADERS), $(notdir $(wildcard $(WEBKIT_LEGACY_PRIVATE_HEADERS_DIR)/*.h))) WebKitLegacy.h)

WEBKIT_LEGACY_PRIVATE_HEADERS = $(addprefix $(PRIVATE_HEADERS_DIR)/, $(filter-out $(WEBKIT_PUBLIC_HEADERS) WebKit.h, $(notdir $(wildcard $(WEBKIT_LEGACY_PRIVATE_HEADERS_DIR)/*.h))))

WEBKIT_LEGACY_HEADER_REPLACE_RULES = -e s/\<WebKitLegacy/\<WebKit/
WEBKIT_LEGACY_HEADER_MIGRATE_CMD = sed $(WEBKIT_LEGACY_HEADER_REPLACE_RULES) $< > $@

PUBLIC_HEADER_CHECK_CMD = @if grep -q "AVAILABLE.*9876_5" "$<"; then line=$$(awk "/AVAILABLE.*9876_5/ { print FNR; exit }" "$<" ); echo "$<:$$line: error: A class within a public header has unspecified availability."; false; fi

$(PUBLIC_HEADERS_DIR)/% : $(WEBKIT_LEGACY_PRIVATE_HEADERS_DIR)/% MigrateHeadersFromWebKitLegacy.make
	$(PUBLIC_HEADER_CHECK_CMD)
	$(WEBKIT_LEGACY_HEADER_MIGRATE_CMD)

$(PRIVATE_HEADERS_DIR)/% : $(WEBKIT_LEGACY_PRIVATE_HEADERS_DIR)/% MigrateHeadersFromWebKitLegacy.make
	$(WEBKIT_LEGACY_HEADER_MIGRATE_CMD)

all : $(WEBKIT_LEGACY_PUBLIC_HEADERS) $(WEBKIT_LEGACY_PRIVATE_HEADERS)

$(PUBLIC_HEADERS_DIR)/WebKitLegacy.h : $(WEBKIT_LEGACY_PRIVATE_HEADERS_DIR)/WebKit.h MigrateHeadersFromWebKitLegacy.make
	$(PUBLIC_HEADER_CHECK_CMD)
	$(WEBKIT_LEGACY_HEADER_MIGRATE_CMD)

else

WEBKIT_LEGACY_PRIVATE_HEADERS = $(addprefix $(PRIVATE_HEADERS_DIR)/, $(filter-out WebKit.h, $(notdir $(wildcard $(WEBKIT_LEGACY_PRIVATE_HEADERS_DIR)/*.h))))

all : $(WEBKIT_LEGACY_PRIVATE_HEADERS) $(PUBLIC_HEADERS_DIR)/WebKitLegacy.h

WEBKIT_HEADER_MIGRATE_CMD = echo "\#import <WebKitLegacy/"`basename $<`">" > $@

$(PRIVATE_HEADERS_DIR)/% : $(WEBKIT_LEGACY_PRIVATE_HEADERS_DIR)/% MigrateHeadersFromWebKitLegacy.make
	$(WEBKIT_HEADER_MIGRATE_CMD)

$(PUBLIC_HEADERS_DIR)/WebKitLegacy.h : $(WEBKIT_LEGACY_PRIVATE_HEADERS_DIR)/WebKit.h MigrateHeadersFromWebKitLegacy.make
	echo "#if defined(__has_include) && __has_include(<WebKitLegacy/WebKit.h>)" > $@
	echo "#import <WebKitLegacy/"`basename $<`">" >> $@
	echo "#endif" >> $@

endif
