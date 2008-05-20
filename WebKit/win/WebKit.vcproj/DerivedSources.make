# Copyright (C) 2007 Apple Inc. All rights reserved.
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
# 3.  Neither the name of Apple puter, Inc. ("Apple") nor the names of
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

PREFIX = IGEN_DOM

WEBKIT_IDL = $(WEBKIT)/Interfaces/WebKit.idl

HAND_WRITTEN_INTERFACES = $(filter-out $(WEBKIT_IDL), $(wildcard $(WEBKIT)/Interfaces/*.idl))

GENERATED_INTERFACES = \
    $(PREFIX)Node.idl \
    $(PREFIX)Attr.idl \
    $(PREFIX)NodeList.idl \
    $(PREFIX)Element.idl \
    $(PREFIX)Document.idl \
    $(PREFIX)CharacterData.idl \
    $(PREFIX)CDATASection.idl \
    $(PREFIX)Comment.idl \
    $(PREFIX)Text.idl \
    $(PREFIX)DocumentFragment.idl \
    $(PREFIX)DocumentType.idl \
    $(PREFIX)DOMImplementation.idl \
    $(PREFIX)Entity.idl \
    $(PREFIX)EntityReference.idl \
    $(PREFIX)NamedNodeMap.idl \
    $(PREFIX)Notation.idl \
    $(PREFIX)ProcessingInstruction.idl \
    \
    $(PREFIX)HTMLAnchorElement.idl \
    $(PREFIX)HTMLAppletElement.idl \
    $(PREFIX)HTMLAreaElement.idl \
    $(PREFIX)HTMLBRElement.idl \
    $(PREFIX)HTMLBaseElement.idl \
    $(PREFIX)HTMLBaseFontElement.idl \
    $(PREFIX)HTMLBlockquoteElement.idl \
    $(PREFIX)HTMLBodyElement.idl \
    $(PREFIX)HTMLButtonElement.idl \
    $(PREFIX)HTMLCollection.idl \
    $(PREFIX)HTMLDListElement.idl \
    $(PREFIX)HTMLDirectoryElement.idl \
    $(PREFIX)HTMLDivElement.idl \
    $(PREFIX)HTMLDocument.idl \
    $(PREFIX)HTMLElement.idl \
    $(PREFIX)HTMLEmbedElement.idl \
    $(PREFIX)HTMLFieldSetElement.idl \
    $(PREFIX)HTMLFontElement.idl \
    $(PREFIX)HTMLFormElement.idl \
    $(PREFIX)HTMLFrameElement.idl \
    $(PREFIX)HTMLFrameSetElement.idl \
    $(PREFIX)HTMLHRElement.idl \
    $(PREFIX)HTMLHeadElement.idl \
    $(PREFIX)HTMLHeadingElement.idl \
    $(PREFIX)HTMLHtmlElement.idl \
    $(PREFIX)HTMLIFrameElement.idl \
    $(PREFIX)HTMLImageElement.idl \
    $(PREFIX)HTMLInputElement.idl \
    $(PREFIX)HTMLIsIndexElement.idl \
    $(PREFIX)HTMLLIElement.idl \
    $(PREFIX)HTMLLabelElement.idl \
    $(PREFIX)HTMLLegendElement.idl \
    $(PREFIX)HTMLLinkElement.idl \
    $(PREFIX)HTMLMapElement.idl \
    $(PREFIX)HTMLMarqueeElement.idl \
    $(PREFIX)HTMLMenuElement.idl \
    $(PREFIX)HTMLMetaElement.idl \
    $(PREFIX)HTMLModElement.idl \
    $(PREFIX)HTMLOListElement.idl \
    $(PREFIX)HTMLObjectElement.idl \
    $(PREFIX)HTMLOptGroupElement.idl \
    $(PREFIX)HTMLOptionElement.idl \
    $(PREFIX)HTMLOptionsCollection.idl \
    $(PREFIX)HTMLParagraphElement.idl \
    $(PREFIX)HTMLParamElement.idl \
    $(PREFIX)HTMLPreElement.idl \
    $(PREFIX)HTMLQuoteElement.idl \
    $(PREFIX)HTMLScriptElement.idl \
    $(PREFIX)HTMLSelectElement.idl \
    $(PREFIX)HTMLStyleElement.idl \
    $(PREFIX)HTMLTableCaptionElement.idl \
    $(PREFIX)HTMLTableCellElement.idl \
    $(PREFIX)HTMLTableColElement.idl \
    $(PREFIX)HTMLTableElement.idl \
    $(PREFIX)HTMLTableRowElement.idl \
    $(PREFIX)HTMLTableSectionElement.idl \
    $(PREFIX)HTMLTextAreaElement.idl \
    $(PREFIX)HTMLTitleElement.idl \
    $(PREFIX)HTMLUListElement.idl \
    \
    $(PREFIX)CSSCharsetRule.idl \
    $(PREFIX)CSSFontFaceRule.idl \
    $(PREFIX)CSSImportRule.idl \
    $(PREFIX)CSSMediaRule.idl \
    $(PREFIX)CSSPageRule.idl \
    $(PREFIX)CSSPrimitiveValue.idl \
    $(PREFIX)CSSRule.idl \
    $(PREFIX)CSSRuleList.idl \
    $(PREFIX)CSSStyleDeclaration.idl \
    $(PREFIX)CSSStyleRule.idl \
    $(PREFIX)CSSStyleSheet.idl \
    $(PREFIX)CSSUnknownRule.idl \
    $(PREFIX)CSSValue.idl \
    $(PREFIX)CSSValueList.idl \
    $(PREFIX)Counter.idl \
    $(PREFIX)MediaList.idl \
    $(PREFIX)Rect.idl \
    $(PREFIX)StyleSheet.idl \
    $(PREFIX)StyleSheetList.idl \
    \
    $(PREFIX)Event.idl \
    $(PREFIX)EventTarget.idl \
    $(PREFIX)EventListener.idl \
#

.PHONY : all
all : \
    $(GENERATED_INTERFACES) \
    $(WEBKIT_IDL) \
#

# $(PREFIX)CanvasGradient.idl \
# $(PREFIX)CanvasPattern.idl \
# $(PREFIX)CanvasRenderingContext2D.idl \
# $(PREFIX)HTMLCanvasElement.idl \
# $(PREFIX)RGBColor.idl \

COM_BINDINGS_SCRIPTS = \
    $(WEBKIT_OUTPUT)/obj/WebKit/DOMInterfaces/CodeGenerator.pm \
    $(WEBKIT_OUTPUT)/obj/WebKit/DOMInterfaces/CodeGeneratorCOM.pm \
    $(WEBKIT_OUTPUT)/obj/WebKit/DOMInterfaces/IDLParser.pm \
    $(WEBKIT_OUTPUT)/obj/WebKit/DOMInterfaces/IDLStructure.pm \
    $(WEBKIT_OUTPUT)/obj/WebKit/DOMInterfaces/generate-bindings.pl \
#

$(PREFIX)%.idl : $(WEBKIT_OUTPUT)/obj/WebKit/DOMInterfaces/%.idl $(COM_BINDINGS_SCRIPTS)
	perl -I $(WEBKIT_OUTPUT)/obj/WebKit/DOMInterfaces/ $(WEBKIT_OUTPUT)/obj/WebKit/DOMInterfaces/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_COM" --generator COM --include $(WEBKIT_OUTPUT)/obj/WebKit/DOMInterfaces/ --outputdir . $<

$(WEBKIT_IDL) : $(HAND_WRITTEN_INTERFACES) $(GENERATED_INTERFACES)
	touch $@
