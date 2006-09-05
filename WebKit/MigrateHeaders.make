# Copyright (C) 2006 Apple Computer, Inc. All rights reserved.
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
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

VPATH = $(TARGET_BUILD_DIR)/$(PUBLIC_HEADERS_FOLDER_PATH)

.PHONY : all
all : \
    DOM.h \
    DOMAttr.h \
    DOMCDATASection.h \
    DOMCSS.h \
    DOMCSSCharsetRule.h \
    DOMCSSFontFaceRule.h \
    DOMCSSImportRule.h \
    DOMCSSMediaRule.h \
    DOMCSSMediaRulePrivate.h \
    DOMCSSPageRule.h \
    DOMCSSPrimitiveValue.h \
    DOMCSSPrimitiveValuePrivate.h \
    DOMCSSRule.h \
    DOMCSSRuleList.h \
    DOMCSSStyleDeclaration.h \
    DOMCSSStyleDeclarationPrivate.h \
    DOMCSSStyleRule.h \
    DOMCSSStyleSheet.h \
    DOMCSSStyleSheetPrivate.h \
    DOMCSSUnknownRule.h \
    DOMCSSValue.h \
    DOMCSSValueList.h \
    DOMCharacterData.h \
    DOMCharacterDataPrivate.h \
    DOMComment.h \
    DOMCore.h \
    DOMCounter.h \
    DOMDOMImplementation.h \
    DOMDOMImplementationPrivate.h \
    DOMDocument.h \
    DOMDocumentFragment.h \
    DOMDocumentPrivate.h \
    DOMDocumentType.h \
    DOMElement.h \
    DOMElementPrivate.h \
    DOMEntity.h \
    DOMEntityReference.h \
    DOMEvents.h \
    DOMExtensions.h \
    DOMHTML.h \
    DOMHTMLAnchorElement.h \
    DOMHTMLAnchorElementPrivate.h \
    DOMHTMLAppletElement.h \
    DOMHTMLAreaElement.h \
    DOMHTMLAreaElementPrivate.h \
    DOMHTMLBRElement.h \
    DOMHTMLBaseElement.h \
    DOMHTMLBaseFontElement.h \
    DOMHTMLBodyElement.h \
    DOMHTMLBodyElementPrivate.h \
    DOMHTMLButtonElement.h \
    DOMHTMLButtonElementPrivate.h \
    DOMHTMLCollection.h \
    DOMHTMLDListElement.h \
    DOMHTMLDirectoryElement.h \
    DOMHTMLDivElement.h \
    DOMHTMLDocument.h \
    DOMHTMLElement.h \
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
    DOMHTMLImageElementPrivate.h \
    DOMHTMLInputElement.h \
    DOMHTMLInputElementPrivate.h \
    DOMHTMLIsIndexElement.h \
    DOMHTMLLIElement.h \
    DOMHTMLLabelElement.h \
    DOMHTMLLabelElementPrivate.h \
    DOMHTMLLegendElement.h \
    DOMHTMLLegendElementPrivate.h \
    DOMHTMLLinkElement.h \
    DOMHTMLLinkElementPrivate.h \
    DOMHTMLMapElement.h \
    DOMHTMLMenuElement.h \
    DOMHTMLMetaElement.h \
    DOMHTMLModElement.h \
    DOMHTMLOListElement.h \
    DOMHTMLObjectElement.h \
    DOMHTMLOptGroupElement.h \
    DOMHTMLOptionElement.h \
    DOMHTMLOptionsCollection.h \
    DOMHTMLOptionsCollectionPrivate.h \
    DOMHTMLParagraphElement.h \
    DOMHTMLParamElement.h \
    DOMHTMLPreElement.h \
    DOMHTMLPreElementPrivate.h \
    DOMHTMLQuoteElement.h \
    DOMHTMLScriptElement.h \
    DOMHTMLSelectElement.h \
    DOMHTMLSelectElementPrivate.h \
    DOMHTMLStyleElement.h \
    DOMHTMLStyleElementPrivate.h \
    DOMHTMLTableCaptionElement.h \
    DOMHTMLTableCellElement.h \
    DOMHTMLTableColElement.h \
    DOMHTMLTableElement.h \
    DOMHTMLTableRowElement.h \
    DOMHTMLTableSectionElement.h \
    DOMHTMLTextAreaElement.h \
    DOMHTMLTextAreaElementPrivate.h \
    DOMHTMLTitleElement.h \
    DOMHTMLUListElement.h \
    DOMList.h \
    DOMMediaList.h \
    DOMNamedNodeMap.h \
    DOMNamedNodeMapPrivate.h \
    DOMNode.h \
    DOMNodeList.h \
    DOMNotation.h \
    DOMObject.h \
    DOMPrivate.h \
    DOMProcessingInstruction.h \
    DOMProcessingInstructionPrivate.h \
    DOMRGBColor.h \
    DOMRange.h \
    DOMRect.h \
    DOMStyleSheet.h \
    DOMStyleSheetList.h \
    DOMStylesheets.h \
    DOMText.h \
    DOMTraversal.h \
    DOMViews.h \
    DOMXPath.h \
    WebDashboardRegion.h \
    WebScriptObject.h \
    npapi.h \
    npruntime.h \
#

REPLACE_COMMANDS = -e s/WebCore/WebKit/ -e s/JavaScriptCore/WebKit/ -e s/DOMDOMImplementation/DOMImplementation/
PRIVATE_HEADER_MIGRATE = sed $(REPLACE_COMMANDS) "$<" > "$(TARGET_BUILD_DIR)/$(PRIVATE_HEADERS_FOLDER_PATH)/$@"
PUBLIC_HEADER_MIGRATE = sed $(REPLACE_COMMANDS) "$<" > "$(TARGET_BUILD_DIR)/$(PUBLIC_HEADERS_FOLDER_PATH)/$@"

WebDashboardRegion.h : $(WEBCORE_PRIVATE_HEADERS_DIR)/WebDashboardRegion.h
	$(PRIVATE_HEADER_MIGRATE)

DOMPrivate.h : $(WEBCORE_PRIVATE_HEADERS_DIR)/DOMPrivate.h
	$(PRIVATE_HEADER_MIGRATE)

DOMDOMImplementationPrivate.h : $(WEBCORE_PRIVATE_HEADERS_DIR)/DOMDOMImplementationPrivate.h
	sed $(REPLACE_COMMANDS) "$<" > "$(TARGET_BUILD_DIR)/$(PRIVATE_HEADERS_FOLDER_PATH)/DOMImplementationPrivate.h"

DOM%Private.h : $(WEBCORE_PRIVATE_HEADERS_DIR)/DOM%Private.h
	$(PRIVATE_HEADER_MIGRATE)

DOM.h : $(WEBCORE_PRIVATE_HEADERS_DIR)/DOM.h
	$(PUBLIC_HEADER_MIGRATE)

DOMDOMImplementation.h : $(WEBCORE_PRIVATE_HEADERS_DIR)/DOMDOMImplementation.h
	sed $(REPLACE_COMMANDS) "$<" > "$(TARGET_BUILD_DIR)/$(PRIVATE_HEADERS_FOLDER_PATH)/DOMImplementation.h"

DOM%.h : $(WEBCORE_PRIVATE_HEADERS_DIR)/DOM%.h
	$(PUBLIC_HEADER_MIGRATE)

Web%.h : $(WEBCORE_PRIVATE_HEADERS_DIR)/Web%.h
	$(PUBLIC_HEADER_MIGRATE)

np%.h : $(JAVASCRIPTCORE_PRIVATE_HEADERS_DIR)/np%.h
	$(PUBLIC_HEADER_MIGRATE)
