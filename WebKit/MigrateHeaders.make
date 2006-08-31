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
    DOMCharacterData.h \
    DOMComment.h \
    DOMCore.h \
    DOMDOMImplementation.h \
    DOMDocument.h \
    DOMDocumentFragment.h \
    DOMDocumentType.h \
    DOMElement.h \
    DOMEntity.h \
    DOMEntityReference.h \
    DOMEvents.h \
    DOMExtensions.h \
    DOMHTML.h \
    DOMHTMLBRElement.h \
    DOMHTMLBaseElement.h \
    DOMHTMLBodyElement.h \
    DOMHTMLButtonElement.h \
    DOMHTMLCollection.h \
    DOMHTMLDListElement.h \
    DOMHTMLDirectoryElement.h \
    DOMHTMLDivElement.h \
    DOMHTMLDocument.h \
    DOMHTMLElement.h \
    DOMHTMLFieldSetElement.h \
    DOMHTMLFormElement.h \
    DOMHTMLHeadElement.h \
    DOMHTMLHeadingElement.h \
    DOMHTMLHtmlElement.h \
    DOMHTMLInputElement.h \
    DOMHTMLIsIndexElement.h \
    DOMHTMLLIElement.h \
    DOMHTMLLabelElement.h \
    DOMHTMLLegendElement.h \
    DOMHTMLLinkElement.h \
    DOMHTMLMenuElement.h \
    DOMHTMLMetaElement.h \
    DOMHTMLOListElement.h \
    DOMHTMLOptGroupElement.h \
    DOMHTMLOptionsCollection.h \
    DOMHTMLParagraphElement.h \
    DOMHTMLPreElement.h \
    DOMHTMLQuoteElement.h \
    DOMHTMLSelectElement.h \
    DOMHTMLStyleElement.h \
    DOMHTMLTextAreaElement.h \
    DOMHTMLTitleElement.h \
    DOMHTMLUListElement.h \
    DOMList.h \
    DOMNamedNodeMap.h \
    DOMNode.h \
    DOMNodeList.h \
    DOMNotation.h \
    DOMObject.h \
    DOMPrivate.h \
    DOMProcessingInstruction.h \
    DOMRange.h \
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

DOM.h : $(WEBCORE_PRIVATE_HEADERS_DIR)/DOM.h
	sed -e s/WebCore/WebKit/ "$<" > "$(TARGET_BUILD_DIR)/$(PUBLIC_HEADERS_FOLDER_PATH)/$@"

DOMPrivate.h : $(WEBCORE_PRIVATE_HEADERS_DIR)/DOMPrivate.h
	sed -e s/WebCore/WebKit/ "$<" > "$(TARGET_BUILD_DIR)/$(PRIVATE_HEADERS_FOLDER_PATH)/$@"

DOM%.h : $(WEBCORE_PRIVATE_HEADERS_DIR)/DOM%.h
	sed -e s/WebCore/WebKit/ "$<" > "$(TARGET_BUILD_DIR)/$(PUBLIC_HEADERS_FOLDER_PATH)/$@"

Web%.h : $(WEBCORE_PRIVATE_HEADERS_DIR)/Web%.h
	sed -e s/WebCore/WebKit/ "$<" > "$(TARGET_BUILD_DIR)/$(PUBLIC_HEADERS_FOLDER_PATH)/$@"

np%.h : $(JAVASCRIPTCORE_PRIVATE_HEADERS_DIR)/np%.h
	sed -e s/JavaScriptCore/WebKit/ "$<" > "$(TARGET_BUILD_DIR)/$(PUBLIC_HEADERS_FOLDER_PATH)/$@"
