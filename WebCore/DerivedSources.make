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

VPATH = \
    $(WebCore) \
    $(WebCore)/bindings/js \
    $(WebCore)/css \
    $(WebCore)/dom \
    $(WebCore)/html \
    $(WebCore)/page \
    $(WebCore)/xpath \
#

.PHONY : all
all : \
    CSSGrammar.cpp \
    CSSPropertyNames.h \
    CSSValueKeywords.h \
    CharsetData.cpp \
    ColorData.c \
    DocTypeStrings.cpp \
    HTMLEntityNames.c \
    JSAttr.h \
    JSCSSPrimitiveValue.h \
    JSCSSRule.h \
    JSCSSValue.h \
    JSCanvasGradient.h \
    JSCanvasPattern.h \
    JSCanvasRenderingContext2D.h \
    JSCanvasRenderingContext2DBaseTable.cpp \
    JSCharacterData.h \
    JSCounter.h \
    JSCSSStyleDeclaration.h \
    JSDOMImplementation.h \
    JSDOMParser.lut.h \
    JSDOMWindow.h \
    JSDocument.h \
    JSDocumentFragment.h \
    JSDocumentType.h \
    JSElement.h \
    JSEvent.h \
    JSEntity.h \
    JSHTMLBaseElement.h \
    JSHTMLCanvasElement.h \
    JSHTMLDocument.h \
    JSHTMLElement.h \
    JSHTMLHeadElement.h \
    JSHTMLLinkElement.h \
    JSHTMLMetaElement.h \
    JSHTMLStyleElement.h \
    JSHTMLTitleElement.h \
    JSKeyboardEvent.h \
    JSMouseEvent.h \
    JSMutationEvent.h \
    JSNode.h \
    JSNodeFilter.h \
    JSNotation.h \
    JSProcessingInstruction.h \
    JSRange.h \
    JSText.h \
    JSUIEvent.h \
    JSXPathEvaluator.h \
    JSXPathExpression.h \
    JSXPathNSResolver.h \
    JSXPathResult.h \
    JSWheelEvent.h \
    JSXMLHttpRequest.lut.h \
    JSXMLSerializer.lut.h \
    JSXSLTProcessor.lut.h \
    SVGNames.cpp \
    UserAgentStyleSheets.h \
    XLinkNames.cpp \
    XPathGrammar.cpp \
    kjs_css.lut.h \
    kjs_dom.lut.h \
    kjs_events.lut.h \
    kjs_html.lut.h \
    kjs_navigator.lut.h \
    kjs_traversal.lut.h \
    kjs_window.lut.h \
    ksvgcssproperties.h \
    ksvgcssvalues.h \
    tokenizer.cpp \
#

# CSS property names and value keywords

CSSPropertyNames.h : css/CSSPropertyNames.in css/makeprop
	cat $< > CSSPropertyNames.in
	sh "$(WebCore)/css/makeprop"

CSSValueKeywords.h : css/CSSValueKeywords.in css/makevalues
	cat $< > CSSValueKeywords.in
	sh "$(WebCore)/css/makevalues"

# DOCTYPE strings

DocTypeStrings.cpp : html/DocTypeStrings.gperf
	gperf -CEot -L ANSI-C -k "*" -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards $< > $@

# HTML entity names

HTMLEntityNames.c : html/HTMLEntityNames.gperf
	gperf -a -L ANSI-C -C -G -c -o -t -k '*' -N findEntity -D -s 2 $< > $@

# color names

ColorData.c : platform/ColorData.gperf
	gperf -CDEot -L ANSI-C -k '*' -N findColor -D -s 2 $< > $@

# CSS tokenizer

tokenizer.cpp : css/tokenizer.flex css/maketokenizer
	flex -t $< | perl $(WebCore)/css/maketokenizer > $@

# CSS grammar

CSSGrammar.cpp : css/CSSGrammar.y
	bison -d -p cssyy $< -o $@
	touch CSSGrammar.cpp.h
	touch CSSGrammar.hpp
	cat CSSGrammar.cpp.h CSSGrammar.hpp > CSSGrammar.h
	rm -f CSSGrammar.cpp.h CSSGrammar.hpp

# XPath grammar

XPathGrammar.cpp : xpath/impl/XPathGrammar.y
	bison -d -p xpathyy $< -o $@
	touch XPathGrammar.cpp.h
	touch XPathGrammar.hpp
	cat XPathGrammar.cpp.h XPathGrammar.hpp > XPathGrammar.h
	rm -f XPathGrammar.cpp.h XPathGrammar.hpp

# user agent style sheets

USER_AGENT_STYLE_SHEETS = $(WebCore)/css/html4.css $(WebCore)/css/quirks.css $(WebCore)/css/svg.css 
UserAgentStyleSheets.h : css/make-css-file-arrays.pl $(USER_AGENT_STYLE_SHEETS)
	$< $@ UserAgentStyleSheetsData.cpp $(USER_AGENT_STYLE_SHEETS)

# character set name table

CharsetData.cpp : platform/make-charset-table.pl platform/character-sets.txt $(ENCODINGS_FILE)
	$^ $(ENCODINGS_PREFIX) > $@

# lookup tables for old-style JavaScript bindings

%.lut.h: %.cpp $(CREATE_HASH_TABLE)
	$(CREATE_HASH_TABLE) $< > $@
%Table.cpp: %.cpp $(CREATE_HASH_TABLE)
	$(CREATE_HASH_TABLE) $< > $@

# SVG tag and attribute names

SVGNames.cpp : ksvg2/scripts/make_names.pl ksvg2/svg/svgtags.in ksvg2/svg/svgattrs.in
	$< --tags $(WebCore)/ksvg2/svg/svgtags.in --attrs $(WebCore)/ksvg2/svg/svgattrs.in \
            --namespace SVG --cppNamespace WebCore --namespaceURI "http://www.w3.org/2000/svg" --factory --attrsNullNamespace --output .

XLinkNames.cpp : ksvg2/scripts/make_names.pl ksvg2/misc/xlinkattrs.in
	$< --attrs $(WebCore)/ksvg2/misc/xlinkattrs.in \
            --namespace XLink --cppNamespace WebCore --namespaceURI "http://www.w3.org/1999/xlink" --output .
	touch $(WebCore)/WebCore+SVG/XLinkNamesWrapper.cpp

# SVG CSS property names and value keywords

ksvgcssproperties.h : ksvg2/scripts/cssmakeprops css/CSSPropertyNames.in ksvg2/css/CSSPropertyNames.in
	if sort $(WebCore)/css/CSSPropertyNames.in $(WebCore)/ksvg2/css/CSSPropertyNames.in | uniq -d | grep -E '^[^#]'; then echo 'Duplicate value!'; exit 1; fi
	cat $(WebCore)/ksvg2/css/CSSPropertyNames.in > ksvgcssproperties.in
	$(WebCore)/ksvg2/scripts/cssmakeprops -n SVG -f ksvgcssproperties.in

ksvgcssvalues.h : ksvg2/scripts/cssmakevalues css/CSSValueKeywords.in ksvg2/css/CSSValueKeywords.in
	if sort $(WebCore)/css/CSSValueKeywords.in $(WebCore)/ksvg2/css/CSSValueKeywords.in | uniq -d | grep -E '^[^#]'; then echo 'Duplicate value!'; exit 1; fi
	# Lower case all the values, as CSS values are case-insensitive
	perl -ne 'print lc' $(WebCore)/ksvg2/css/CSSValueKeywords.in > ksvgcssvalues.in
	$(WebCore)/ksvg2/scripts/cssmakevalues -n SVG -f ksvgcssvalues.in

# new-style JavaScript bindings

JS_BINDINGS_SCRIPTS = \
    bindings/scripts/CodeGenerator.pm \
    bindings/scripts/CodeGeneratorJS.pm \
    bindings/scripts/IDLParser.pm \
    bindings/scripts/IDLStructure.pm \
    bindings/scripts/generate-bindings.pl \
#

JS%.h : %.idl $(JS_BINDINGS_SCRIPTS)
	perl -I$(WebCore)/bindings/scripts $(WebCore)/bindings/scripts/generate-bindings.pl --generator JS --include dom --include html --include xpath --outputdir  . $<
