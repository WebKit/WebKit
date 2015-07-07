# Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

VPATH = $(WEBCORE_PRIVATE_HEADERS_DIR)

INTERNAL_HEADERS_DIR = $(BUILT_PRODUCTS_DIR)/DerivedSources/WebKitLegacy
PUBLIC_HEADERS_DIR = $(TARGET_BUILD_DIR)/$(PUBLIC_HEADERS_FOLDER_PATH)
PRIVATE_HEADERS_DIR = $(TARGET_BUILD_DIR)/$(PRIVATE_HEADERS_FOLDER_PATH)

.PHONY : all
all : \
    $(PRIVATE_HEADERS_DIR)/DOM.h \
    $(PRIVATE_HEADERS_DIR)/DOMAbstractView.h \
    $(PRIVATE_HEADERS_DIR)/DOMAttr.h \
    $(PRIVATE_HEADERS_DIR)/DOMBlob.h \
    $(INTERNAL_HEADERS_DIR)/DOMBlobInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMCDATASection.h \
    $(PRIVATE_HEADERS_DIR)/DOMCSS.h \
    $(PRIVATE_HEADERS_DIR)/DOMCSSCharsetRule.h \
    $(PRIVATE_HEADERS_DIR)/DOMCSSFontFaceRule.h \
    $(PRIVATE_HEADERS_DIR)/DOMCSSImportRule.h \
    $(PRIVATE_HEADERS_DIR)/DOMCSSMediaRule.h \
    $(PRIVATE_HEADERS_DIR)/DOMCSSPageRule.h \
    $(PRIVATE_HEADERS_DIR)/DOMCSSPrimitiveValue.h \
    $(PRIVATE_HEADERS_DIR)/DOMCSSRule.h \
    $(PRIVATE_HEADERS_DIR)/DOMCSSRuleList.h \
    $(PRIVATE_HEADERS_DIR)/DOMCSSStyleDeclaration.h \
    $(INTERNAL_HEADERS_DIR)/DOMCSSStyleDeclarationInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMCSSStyleRule.h \
    $(PRIVATE_HEADERS_DIR)/DOMCSSStyleSheet.h \
    $(PRIVATE_HEADERS_DIR)/DOMCSSUnknownRule.h \
    $(PRIVATE_HEADERS_DIR)/DOMCSSValue.h \
    $(PRIVATE_HEADERS_DIR)/DOMCSSValueList.h \
    $(PRIVATE_HEADERS_DIR)/DOMCharacterData.h \
    $(PRIVATE_HEADERS_DIR)/DOMComment.h \
    $(PRIVATE_HEADERS_DIR)/DOMCore.h \
    $(PRIVATE_HEADERS_DIR)/DOMCounter.h \
    $(PRIVATE_HEADERS_DIR)/DOMDocument.h \
    $(PRIVATE_HEADERS_DIR)/DOMDocumentFragment.h \
    $(INTERNAL_HEADERS_DIR)/DOMDocumentFragmentInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMDocumentFragmentPrivate.h \
    $(INTERNAL_HEADERS_DIR)/DOMDocumentInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMDocumentPrivate.h \
    $(PRIVATE_HEADERS_DIR)/DOMDocumentType.h \
    $(PRIVATE_HEADERS_DIR)/DOMElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMEntity.h \
    $(PRIVATE_HEADERS_DIR)/DOMEntityReference.h \
    $(PRIVATE_HEADERS_DIR)/DOMEvent.h \
    $(PRIVATE_HEADERS_DIR)/DOMEventException.h \
    $(PRIVATE_HEADERS_DIR)/DOMEventListener.h \
    $(PRIVATE_HEADERS_DIR)/DOMEventTarget.h \
    $(PRIVATE_HEADERS_DIR)/DOMEvents.h \
    $(PRIVATE_HEADERS_DIR)/DOMException.h \
    $(PRIVATE_HEADERS_DIR)/DOMExtensions.h \
    $(PRIVATE_HEADERS_DIR)/DOMFile.h \
    $(PRIVATE_HEADERS_DIR)/DOMFileList.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTML.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLAnchorElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLAppletElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLAreaElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLBRElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLBaseElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLBaseFontElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLBodyElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLButtonElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLCollection.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLDListElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLDirectoryElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLDivElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLDocument.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMHTMLElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLEmbedElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLEmbedElementPrivate.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLFieldSetElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLFontElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLFormElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMHTMLFormElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLFrameElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLFrameSetElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLHRElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLHeadElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLHeadingElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLHtmlElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLIFrameElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLImageElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLInputElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLInputElementPrivate.h \
    $(INTERNAL_HEADERS_DIR)/DOMHTMLInputElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLLIElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLLabelElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLLegendElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLLinkElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLMapElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLMarqueeElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLMenuElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLMetaElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLModElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLOListElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLObjectElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLObjectElementPrivate.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLOptGroupElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLOptionElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLOptionsCollection.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLParagraphElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLParamElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLPreElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLQuoteElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLScriptElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLSelectElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLStyleElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLTableCaptionElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLTableCellElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLTableColElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLTableElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLTableRowElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLTableSectionElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLTextAreaElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMHTMLTextAreaElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLTitleElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLUListElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMImplementation.h \
    $(PRIVATE_HEADERS_DIR)/DOMKeyboardEvent.h \
    $(PRIVATE_HEADERS_DIR)/DOMMediaList.h \
    $(PRIVATE_HEADERS_DIR)/DOMMouseEvent.h \
    $(PRIVATE_HEADERS_DIR)/DOMMutationEvent.h \
    $(PRIVATE_HEADERS_DIR)/DOMNamedNodeMap.h \
    $(PRIVATE_HEADERS_DIR)/DOMNode.h \
    $(INTERNAL_HEADERS_DIR)/DOMNodeInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMNodePrivate.h \
    $(PRIVATE_HEADERS_DIR)/DOMNodeFilter.h \
    $(PRIVATE_HEADERS_DIR)/DOMNodeIterator.h \
    $(PRIVATE_HEADERS_DIR)/DOMNodeList.h \
    $(PRIVATE_HEADERS_DIR)/DOMNotation.h \
    $(PRIVATE_HEADERS_DIR)/DOMObject.h \
    $(PRIVATE_HEADERS_DIR)/DOMOverflowEvent.h \
    $(PRIVATE_HEADERS_DIR)/DOMPrivate.h \
    $(PRIVATE_HEADERS_DIR)/DOMProcessingInstruction.h \
    $(PRIVATE_HEADERS_DIR)/DOMProgressEvent.h \
    $(PRIVATE_HEADERS_DIR)/DOMRGBColor.h \
    $(PRIVATE_HEADERS_DIR)/DOMRange.h \
    $(INTERNAL_HEADERS_DIR)/DOMRangeInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMRangeException.h \
    $(PRIVATE_HEADERS_DIR)/DOMRanges.h \
    $(PRIVATE_HEADERS_DIR)/DOMRect.h \
    $(PRIVATE_HEADERS_DIR)/DOMStyleSheet.h \
    $(PRIVATE_HEADERS_DIR)/DOMStyleSheetList.h \
    $(PRIVATE_HEADERS_DIR)/DOMStylesheets.h \
    $(PRIVATE_HEADERS_DIR)/DOMText.h \
    $(PRIVATE_HEADERS_DIR)/DOMTraversal.h \
    $(PRIVATE_HEADERS_DIR)/DOMTreeWalker.h \
    $(PRIVATE_HEADERS_DIR)/DOMUIEvent.h \
    $(PRIVATE_HEADERS_DIR)/DOMViews.h \
    $(PRIVATE_HEADERS_DIR)/DOMWheelEvent.h \
    $(INTERNAL_HEADERS_DIR)/DOMWheelEventInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMXPath.h \
    $(PRIVATE_HEADERS_DIR)/DOMXPathException.h \
    $(PRIVATE_HEADERS_DIR)/DOMXPathExpression.h \
    $(PRIVATE_HEADERS_DIR)/DOMXPathNSResolver.h \
    $(PRIVATE_HEADERS_DIR)/DOMXPathResult.h \
    $(PRIVATE_HEADERS_DIR)/WebKitAvailability.h \
    $(PRIVATE_HEADERS_DIR)/WebScriptObject.h \
    $(PRIVATE_HEADERS_DIR)/npapi.h \
    $(PRIVATE_HEADERS_DIR)/npfunctions.h \
    $(PRIVATE_HEADERS_DIR)/npruntime.h \
    $(PRIVATE_HEADERS_DIR)/nptypes.h \
#

ifneq ($(filter iphoneos iphonesimulator, $(PLATFORM_NAME)), )
all : \
    $(PRIVATE_HEADERS_DIR)/DOMGestureEvent.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLTextAreaElementPrivate.h \
    $(PRIVATE_HEADERS_DIR)/DOMTouch.h \
    $(PRIVATE_HEADERS_DIR)/DOMTouchEvent.h \
    $(PRIVATE_HEADERS_DIR)/DOMTouchList.h \
    $(PRIVATE_HEADERS_DIR)/DOMUIKitExtensions.h \
    $(PRIVATE_HEADERS_DIR)/KeyEventCodesIOS.h \
    $(PRIVATE_HEADERS_DIR)/MediaPlayerProxy.h \
    $(PRIVATE_HEADERS_DIR)/PluginData.h \
    $(PRIVATE_HEADERS_DIR)/ScrollTypes.h \
    $(PRIVATE_HEADERS_DIR)/SystemMemory.h \
    $(PRIVATE_HEADERS_DIR)/WAKAppKitStubs.h \
    $(PRIVATE_HEADERS_DIR)/WAKResponder.h \
    $(PRIVATE_HEADERS_DIR)/WAKScrollView.h \
    $(PRIVATE_HEADERS_DIR)/WAKView.h \
    $(PRIVATE_HEADERS_DIR)/WAKViewPrivate.h \
    $(PRIVATE_HEADERS_DIR)/WAKWindow.h \
    $(PRIVATE_HEADERS_DIR)/WKContentObservation.h \
    $(PRIVATE_HEADERS_DIR)/WKGraphics.h \
    $(PRIVATE_HEADERS_DIR)/WKTypes.h \
    $(PRIVATE_HEADERS_DIR)/WKUtilities.h \
    $(PRIVATE_HEADERS_DIR)/WebAutocapitalize.h \
    $(PRIVATE_HEADERS_DIR)/WebCoreFrameView.h \
    $(PRIVATE_HEADERS_DIR)/WebCoreThread.h \
    $(PRIVATE_HEADERS_DIR)/WebCoreThreadMessage.h \
    $(PRIVATE_HEADERS_DIR)/WebCoreThreadRun.h \
    $(PRIVATE_HEADERS_DIR)/WebEvent.h \
    $(PRIVATE_HEADERS_DIR)/WebEventRegion.h


# Special case WAKScrollView.h, which contains the protocol named
# <WebCoreFrameScrollView> and shouldn't be changed by the default rule.
$(PRIVATE_HEADERS_DIR)/WAKScrollView.h : WAKScrollView.h MigrateHeaders.make
	cat $< > $@

endif

WEBCORE_HEADER_REPLACE_RULES = -e s/\<WebCore/\<WebKitLegacy/ -e s/DOMDOMImplementation/DOMImplementation/
WEBCORE_HEADER_MIGRATE_CMD = sed $(WEBCORE_HEADER_REPLACE_RULES) $< > $@

$(PRIVATE_HEADERS_DIR)/DOM% : DOMDOM% MigrateHeaders.make
	$(WEBCORE_HEADER_MIGRATE_CMD)

$(PRIVATE_HEADERS_DIR)/% : % MigrateHeaders.make
	$(WEBCORE_HEADER_MIGRATE_CMD)

$(INTERNAL_HEADERS_DIR)/% : % MigrateHeaders.make
	$(WEBCORE_HEADER_MIGRATE_CMD)
