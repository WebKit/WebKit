# Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
    $(WebCore)/bindings/objc \
    $(WebCore)/css \
    $(WebCore)/dom \
    $(WebCore)/html \
    $(WebCore)/page \
    $(WebCore)/xml \
    $(WebCore)/ksvg2/svg \
    $(WebCore)/ksvg2/events \
#

.PHONY : all

ifeq ($(OS),MACOS)
all : \
    CharsetData.cpp \
    DOMAbstractView.h \
    DOMAttr.h \
    DOMCDATASection.h \
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
    DOMCounter.h \
    DOMDOMImplementation.h \
    DOMDocument.h \
    DOMDocumentFragment.h \
    DOMDocumentType.h \
    DOMElement.h \
    DOMEntity.h \
    DOMEntityReference.h \
    DOMEvent.h \
    DOMEventListener.h \
    DOMEventTarget.h \
    DOMHTMLAnchorElement.h \
    DOMHTMLAppletElement.h \
    DOMHTMLAreaElement.h \
    DOMHTMLBRElement.h \
    DOMHTMLBaseElement.h \
    DOMHTMLBaseFontElement.h \
    DOMHTMLBodyElement.h \
    DOMHTMLButtonElement.h \
    DOMHTMLCanvasElement.h \
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
    DOMHTMLIsIndexElement.h \
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
    DOMOverflowEvent.h \
    DOMProcessingInstruction.h \
    DOMRGBColor.h \
    DOMRange.h \
    DOMRect.h \
    DOMSVGAElement.h \
    DOMSVGAngle.h \
    DOMSVGAnimateColorElement.h \
    DOMSVGAnimateElement.h \
    DOMSVGAnimateTransformElement.h \
    DOMSVGAnimatedAngle.h \
    DOMSVGAnimatedBoolean.h \
    DOMSVGAnimatedEnumeration.h \
    DOMSVGAnimatedInteger.h \
    DOMSVGAnimatedLength.h \
    DOMSVGAnimatedLengthList.h \
    DOMSVGAnimatedNumber.h \
    DOMSVGAnimatedNumberList.h \
    DOMSVGAnimatedPathData.h \
    DOMSVGAnimatedPoints.h \
    DOMSVGAnimatedPreserveAspectRatio.h \
    DOMSVGAnimatedRect.h \
    DOMSVGAnimatedString.h \
    DOMSVGAnimatedTransformList.h \
    DOMSVGAnimationElement.h \
    DOMSVGCircleElement.h \
    DOMSVGClipPathElement.h \
    DOMSVGColor.h \
    DOMSVGComponentTransferFunctionElement.h \
    DOMSVGCursorElement.h \
    DOMSVGDefsElement.h \
    DOMSVGDescElement.h \
    DOMSVGDocument.h \
    DOMSVGElement.h \
    DOMSVGElementInstance.h \
    DOMSVGElementInstanceList.h \
    DOMSVGEllipseElement.h \
    DOMSVGExternalResourcesRequired.h \
    DOMSVGFEBlendElement.h \
    DOMSVGFEColorMatrixElement.h \
    DOMSVGFEComponentTransferElement.h \
    DOMSVGFECompositeElement.h \
    DOMSVGFEDiffuseLightingElement.h \
    DOMSVGFEDisplacementMapElement.h \
    DOMSVGFEDistantLightElement.h \
    DOMSVGFEFloodElement.h \
    DOMSVGFEFuncAElement.h \
    DOMSVGFEFuncBElement.h \
    DOMSVGFEFuncGElement.h \
    DOMSVGFEFuncRElement.h \
    DOMSVGFEGaussianBlurElement.h \
    DOMSVGFEImageElement.h \
    DOMSVGFEMergeElement.h \
    DOMSVGFEMergeNodeElement.h \
    DOMSVGFEOffsetElement.h \
    DOMSVGFEPointLightElement.h \
    DOMSVGFESpecularLightingElement.h \
    DOMSVGFESpotLightElement.h \
    DOMSVGFETileElement.h \
    DOMSVGFETurbulenceElement.h \
    DOMSVGFilterElement.h \
    DOMSVGFilterPrimitiveStandardAttributes.h \
    DOMSVGFitToViewBox.h \
    DOMSVGForeignObjectElement.h \
    DOMSVGGElement.h \
    DOMSVGGradientElement.h \
    DOMSVGImageElement.h \
    DOMSVGLangSpace.h \
    DOMSVGLength.h \
    DOMSVGLengthList.h \
    DOMSVGLineElement.h \
    DOMSVGLinearGradientElement.h \
    DOMSVGLocatable.h \
    DOMSVGMarkerElement.h \
    DOMSVGMaskElement.h \
    DOMSVGMatrix.h \
    DOMSVGMetadataElement.h \
    DOMSVGNumber.h \
    DOMSVGNumberList.h \
    DOMSVGPaint.h \
    DOMSVGPathElement.h \
    DOMSVGPathSeg.h \
    DOMSVGPathSegArcAbs.h \
    DOMSVGPathSegArcRel.h \
    DOMSVGPathSegClosePath.h \
    DOMSVGPathSegCurvetoCubicAbs.h \
    DOMSVGPathSegCurvetoCubicRel.h \
    DOMSVGPathSegCurvetoCubicSmoothAbs.h \
    DOMSVGPathSegCurvetoCubicSmoothRel.h \
    DOMSVGPathSegCurvetoQuadraticAbs.h \
    DOMSVGPathSegCurvetoQuadraticRel.h \
    DOMSVGPathSegCurvetoQuadraticSmoothAbs.h \
    DOMSVGPathSegCurvetoQuadraticSmoothRel.h \
    DOMSVGPathSegLinetoAbs.h \
    DOMSVGPathSegLinetoHorizontalAbs.h \
    DOMSVGPathSegLinetoHorizontalRel.h \
    DOMSVGPathSegLinetoRel.h \
    DOMSVGPathSegLinetoVerticalAbs.h \
    DOMSVGPathSegLinetoVerticalRel.h \
    DOMSVGPathSegList.h \
    DOMSVGPathSegMovetoAbs.h \
    DOMSVGPathSegMovetoRel.h \
    DOMSVGPatternElement.h \
    DOMSVGPoint.h \
    DOMSVGPointList.h \
    DOMSVGPolygonElement.h \
    DOMSVGPolylineElement.h \
    DOMSVGPreserveAspectRatio.h \
    DOMSVGRadialGradientElement.h \
    DOMSVGRect.h \
    DOMSVGRectElement.h \
    DOMSVGRenderingIntent.h \
    DOMSVGSVGElement.h \
    DOMSVGScriptElement.h \
    DOMSVGSetElement.h \
    DOMSVGStopElement.h \
    DOMSVGStringList.h \
    DOMSVGStylable.h \
    DOMSVGStyleElement.h \
    DOMSVGSwitchElement.h \
    DOMSVGSymbolElement.h \
    DOMSVGTRefElement.h \
    DOMSVGTSpanElement.h \
    DOMSVGTests.h \
    DOMSVGTextContentElement.h \
    DOMSVGTextElement.h \
    DOMSVGTextPositioningElement.h \
    DOMSVGTitleElement.h \
    DOMSVGTransform.h \
    DOMSVGTransformList.h \
    DOMSVGTransformable.h \
    DOMSVGURIReference.h \
    DOMSVGUnitTypes.h \
    DOMSVGUseElement.h \
    DOMSVGViewElement.h \
    DOMSVGZoomAndPan.h \
    DOMSVGZoomEvent.h \
    DOMStyleSheet.h \
    DOMStyleSheetList.h \
    DOMText.h \
    DOMTextEvent.h \
    DOMTreeWalker.h \
    DOMUIEvent.h \
    DOMWheelEvent.h \
    DOMXPathExpression.h \
    DOMXPathNSResolver.h \
    DOMXPathResult.h
endif

all : \
    CSSGrammar.cpp \
    CSSPropertyNames.h \
    CSSValueKeywords.h \
    ColorData.c \
    DocTypeStrings.cpp \
    HTMLEntityNames.c \
    JSAttr.h \
    JSBarInfo.h \
    JSCDATASection.h \
    JSCSSCharsetRule.h \
    JSCSSFontFaceRule.h \
    JSCSSImportRule.h \
    JSCSSMediaRule.h \
    JSCSSPageRule.h \
    JSCSSPrimitiveValue.h \
    JSCSSRule.h \
    JSCSSRuleList.h \
    JSCSSStyleRule.h \
    JSCSSStyleSheet.h \
    JSCSSValue.h \
    JSCSSValueList.h \
    JSCanvasGradient.h \
    JSCanvasPattern.h \
    JSCanvasRenderingContext2D.h \
    JSCharacterData.h \
    JSComment.h \
    JSCounter.h \
    JSCSSStyleDeclaration.h \
    JSDOMExceptionConstructor.lut.h \
    JSDOMImplementation.h \
    JSDOMParser.h \
    JSDOMSelection.h \
    JSDOMWindow.h \
    JSDocument.h \
    JSDocumentFragment.h \
    JSDocumentType.h \
    JSElement.h \
    JSEntity.h \
    JSEntityReference.h \
    JSEvent.h \
    JSEventTargetNode.lut.h \
    JSHTMLAppletElement.h \
    JSHTMLAnchorElement.h \
    JSHTMLAreaElement.h \
    JSHTMLBaseElement.h \
    JSHTMLBaseFontElement.h \
    JSHTMLBlockquoteElement.h \
    JSHTMLBodyElement.h \
    JSHTMLBRElement.h \
    JSHTMLButtonElement.h \
    JSHTMLCanvasElement.h \
    JSHTMLCollection.h \
    JSHTMLDListElement.h \
    JSHTMLDirectoryElement.h \
    JSHTMLDivElement.h \
    JSHTMLDocument.h \
    JSHTMLElement.h \
    JSHTMLEmbedElement.h \
    JSHTMLFieldSetElement.h \
    JSHTMLFontElement.h \
    JSHTMLFormElement.h \
    JSHTMLFrameElement.h \
    JSHTMLFrameSetElement.h \
    JSHTMLHRElement.h \
    JSHTMLHeadElement.h \
    JSHTMLHeadingElement.h \
    JSHTMLHtmlElement.h \
    JSHTMLIFrameElement.h \
    JSHTMLImageElement.h \
    JSHTMLInputElement.h \
    JSHTMLInputElementBaseTable.cpp \
    JSHTMLIsIndexElement.h \
    JSHTMLLIElement.h \
    JSHTMLLabelElement.h \
    JSHTMLLegendElement.h \
    JSHTMLLinkElement.h \
    JSHTMLMapElement.h \
    JSHTMLMarqueeElement.h \
    JSHTMLMenuElement.h \
    JSHTMLMetaElement.h \
    JSHTMLModElement.h \
    JSHTMLOListElement.h \
    JSHTMLOptGroupElement.h \
    JSHTMLObjectElement.h \
    JSHTMLOptionElement.h \
    JSHTMLOptionsCollection.h \
    JSHTMLParagraphElement.h \
    JSHTMLParamElement.h \
    JSHTMLPreElement.h \
    JSHTMLQuoteElement.h \
    JSHTMLScriptElement.h \
    JSHTMLSelectElement.h \
    JSHTMLStyleElement.h \
    JSHTMLTableCaptionElement.h \
    JSHTMLTableCellElement.h \
    JSHTMLTableColElement.h \
    JSHTMLTableElement.h \
    JSHTMLTableRowElement.h \
    JSHTMLTableSectionElement.h \
    JSHTMLTextAreaElement.h \
    JSHTMLTitleElement.h \
    JSHTMLUListElement.h \
    JSHistory.h \
    JSKeyboardEvent.h \
    JSMediaList.h \
    JSMouseEvent.h \
    JSMutationEvent.h \
    JSNamedNodeMap.h \
    JSNode.h \
    JSNodeFilter.h \
    JSNodeIterator.h \
    JSNodeList.h \
    JSNotation.h \
    JSOverflowEvent.h \
    JSProcessingInstruction.h \
    JSRange.h \
    JSRangeException.h \
    JSRect.h \
    JSSVGAElement.h \
    JSSVGAngle.h \
    JSSVGAnimatedAngle.h \
    JSSVGAnimateColorElement.h \
    JSSVGAnimateElement.h \
    JSSVGAnimateTransformElement.h \
    JSSVGAnimatedBoolean.h \
    JSSVGAnimatedEnumeration.h \
    JSSVGAnimatedInteger.h \
    JSSVGAnimatedLength.h \
    JSSVGAnimatedLengthList.h \
    JSSVGAnimatedNumber.h \
    JSSVGAnimatedNumberList.h \
    JSSVGAnimatedPoints.h \
    JSSVGAnimatedPreserveAspectRatio.h \
    JSSVGAnimatedRect.h \
    JSSVGAnimatedString.h \
    JSSVGAnimatedTransformList.h \
    JSSVGAnimationElement.h \
    JSSVGColor.h \
    JSSVGCircleElement.h \
    JSSVGClipPathElement.h \
    JSSVGComponentTransferFunctionElement.h \
    JSSVGCursorElement.h \
    JSSVGDefsElement.h \
    JSSVGDescElement.h \
    JSSVGDocument.h \
    JSSVGLength.h \
    JSSVGMatrix.h \
    JSSVGMetadataElement.h \
    JSSVGPathElement.h \
    JSSVGPathSeg.h \
    JSSVGPathSegArcAbs.h \
    JSSVGPathSegArcRel.h \
    JSSVGPathSegClosePath.h \
    JSSVGPathSegCurvetoCubicAbs.h \
    JSSVGPathSegCurvetoCubicRel.h \
    JSSVGPathSegCurvetoCubicSmoothAbs.h \
    JSSVGPathSegCurvetoCubicSmoothRel.h \
    JSSVGPathSegCurvetoQuadraticAbs.h \
    JSSVGPathSegCurvetoQuadraticRel.h \
    JSSVGPathSegCurvetoQuadraticSmoothAbs.h \
    JSSVGPathSegCurvetoQuadraticSmoothRel.h \
    JSSVGPathSegLinetoAbs.h \
    JSSVGPathSegLinetoHorizontalAbs.h \
    JSSVGPathSegLinetoHorizontalRel.h \
    JSSVGPathSegLinetoRel.h \
    JSSVGPathSegLinetoVerticalAbs.h \
    JSSVGPathSegLinetoVerticalRel.h \
    JSSVGPathSegMovetoAbs.h \
    JSSVGPathSegMovetoRel.h \
    JSSVGNumber.h \
    JSSVGNumberList.h \
    JSSVGPaint.h \
    JSSVGPathSegList.h \
    JSSVGPatternElement.h \
    JSSVGPoint.h \
    JSSVGPointList.h \
    JSSVGPolygonElement.h \
    JSSVGPolylineElement.h \
    JSSVGRadialGradientElement.h \
    JSSVGRect.h \
    JSSVGRectElement.h \
    JSSVGRenderingIntent.h \
    JSSVGSetElement.h \
    JSSVGScriptElement.h \
    JSSVGStyleElement.h \
    JSSVGSwitchElement.h \
    JSSVGStopElement.h \
    JSSVGStringList.h \
    JSSVGSymbolElement.h \
    JSSVGTRefElement.h \
    JSSVGTSpanElement.h \
    JSSVGTextElement.h \
    JSSVGTextContentElement.h \
    JSSVGTextPositioningElement.h \
    JSSVGTitleElement.h \
    JSSVGTransform.h \
    JSSVGTransformList.h \
    JSSVGUnitTypes.h \
    JSSVGUseElement.h \
    JSSVGViewElement.h \
    JSSVGPreserveAspectRatio.h \
    JSSVGElement.h \
    JSSVGElementInstance.h \
    JSSVGElementInstanceList.h \
    JSSVGSVGElement.h \
    JSSVGEllipseElement.h \
    JSSVGFEBlendElement.h \
    JSSVGFEColorMatrixElement.h \
    JSSVGFEComponentTransferElement.h \
    JSSVGFECompositeElement.h \
    JSSVGFEDiffuseLightingElement.h \
    JSSVGFEDisplacementMapElement.h \
    JSSVGFEDistantLightElement.h \
    JSSVGFEFloodElement.h \
    JSSVGFEFuncAElement.h \
    JSSVGFEFuncBElement.h \
    JSSVGFEFuncGElement.h \
    JSSVGFEFuncRElement.h \
    JSSVGFEGaussianBlurElement.h \
    JSSVGFEImageElement.h \
    JSSVGFEMergeElement.h \
    JSSVGFEMergeNodeElement.h \
    JSSVGFEOffsetElement.h \
    JSSVGFEPointLightElement.h \
    JSSVGFESpecularLightingElement.h \
    JSSVGFESpotLightElement.h \
    JSSVGFETileElement.h \
    JSSVGFETurbulenceElement.h \
    JSSVGFilterElement.h \
    JSSVGForeignObjectElement.h \
    JSSVGGElement.h \
    JSSVGGradientElement.h \
    JSSVGImageElement.h \
    JSSVGLength.h \
    JSSVGLengthList.h \
    JSSVGLineElement.h \
    JSSVGLinearGradientElement.h \
    JSSVGMaskElement.h \
    JSSVGMarkerElement.h \
    JSSVGTransform.h \
    JSSVGZoomEvent.h \
    JSScreen.h \
    JSStyleSheet.h \
    JSText.h \
    JSTextEvent.h \
    JSTreeWalker.h \
    JSUIEvent.h \
    JSXPathEvaluator.h \
    JSXPathExpression.h \
    JSXPathNSResolver.h \
    JSXPathResult.h \
    JSWheelEvent.h \
    JSXMLHttpRequest.lut.h \
    JSXMLSerializer.h \
    JSXSLTProcessor.lut.h \
    SVGElementFactory.cpp \
    SVGNames.cpp \
    HTMLNames.cpp \
    UserAgentStyleSheets.h \
    XLinkNames.cpp \
    XMLNames.cpp \
    XPathGrammar.cpp \
    kjs_css.lut.h \
    kjs_events.lut.h \
    kjs_navigator.lut.h \
    kjs_window.lut.h \
    ksvgcssproperties.h \
    ksvgcssvalues.h \
    tokenizer.cpp \
#

# CSS property names and value keywords

CSSPropertyNames.h : css/CSSPropertyNames.in css/makeprop.pl
	cat $< > CSSPropertyNames.in
	perl "$(WebCore)/css/makeprop.pl"

CSSValueKeywords.h : css/CSSValueKeywords.in css/makevalues.pl
	cat $< > CSSValueKeywords.in
	perl "$(WebCore)/css/makevalues.pl"

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
# NOTE: older versions of bison do not inject an inclusion guard, so we do it

CSSGrammar.cpp : css/CSSGrammar.y
	bison -d -p cssyy $< -o $@
	touch CSSGrammar.cpp.h
	touch CSSGrammar.hpp
	echo '#ifndef CSSGrammar_h' > CSSGrammar.h
	echo '#define CSSGrammar_h' >> CSSGrammar.h
	cat CSSGrammar.cpp.h CSSGrammar.hpp >> CSSGrammar.h
	echo '#endif' >> CSSGrammar.h
	rm -f CSSGrammar.cpp.h CSSGrammar.hpp

# XPath grammar
# NOTE: older versions of bison do not inject an inclusion guard, so we do it

XPathGrammar.cpp : xml/XPathGrammar.y $(PROJECT_FILE)
	bison -d -p xpathyy $< -o $@
	touch XPathGrammar.cpp.h
	touch XPathGrammar.hpp
	echo '#ifndef XPathGrammar_h' > XPathGrammar.h
	echo '#define XPathGrammar_h' >> XPathGrammar.h
	cat XPathGrammar.cpp.h XPathGrammar.hpp >> XPathGrammar.h
	echo '#endif' >> XPathGrammar.h
	rm -f XPathGrammar.cpp.h XPathGrammar.hpp

# user agent style sheets

USER_AGENT_STYLE_SHEETS = $(WebCore)/css/html4.css $(WebCore)/css/quirks.css $(WebCore)/css/view-source.css $(WebCore)/css/svg.css 
UserAgentStyleSheets.h : css/make-css-file-arrays.pl $(USER_AGENT_STYLE_SHEETS)
	perl $< $@ UserAgentStyleSheetsData.cpp $(USER_AGENT_STYLE_SHEETS)

# character set name table

CharsetData.cpp : platform/mac/make-charset-table.pl platform/mac/character-sets.txt platform/mac/mac-encodings.txt
	perl $^ kTextEncoding > $@

# lookup tables for old-style JavaScript bindings

%.lut.h: %.cpp $(CREATE_HASH_TABLE)
	$(CREATE_HASH_TABLE) $< > $@
%Table.cpp: %.cpp $(CREATE_HASH_TABLE)
	$(CREATE_HASH_TABLE) $< > $@

# HTML tag and attribute names

HTMLNames.cpp : ksvg2/scripts/make_names.pl html/HTMLTagNames.in html/HTMLAttributeNames.in
	perl $< --tags $(WebCore)/html/HTMLTagNames.in --attrs $(WebCore)/html/HTMLAttributeNames.in \
            --namespace HTML --namespacePrefix xhtml --cppNamespace WebCore --namespaceURI "http://www.w3.org/1999/xhtml" --attrsNullNamespace --output .

XMLNames.cpp : ksvg2/scripts/make_names.pl xml/xmlattrs.in
	perl $< --attrs $(WebCore)/xml/xmlattrs.in \
            --namespace XML --cppNamespace WebCore --namespaceURI "http://www.w3.org/XML/1998/namespace" --output .

ifeq ($(findstring ENABLE_SVG,$(FEATURE_DEFINES)), ENABLE_SVG)

ifeq ($(findstring ENABLE_SVG_EXPERIMENTAL_FEATURES,$(FEATURE_DEFINES)), ENABLE_SVG_EXPERIMENTAL_FEATURES)
# SVG tag and attribute names (need to pass an extra flag if svg experimental features are enabled)
SVGElementFactory.cpp SVGNames.cpp : ksvg2/scripts/make_names.pl ksvg2/svg/svgtags.in ksvg2/svg/svgattrs.in
	perl $< --tags $(WebCore)/ksvg2/svg/svgtags.in --attrs $(WebCore)/ksvg2/svg/svgattrs.in --extraDefines "ENABLE_SVG_EXPERIMENTAL_FEATURES=1" \
            --namespace SVG --cppNamespace WebCore --namespaceURI "http://www.w3.org/2000/svg" --factory --attrsNullNamespace --output .
else
# SVG tag and attribute names
SVGElementFactory.cpp SVGNames.cpp : ksvg2/scripts/make_names.pl ksvg2/svg/svgtags.in ksvg2/svg/svgattrs.in
	perl $< --tags $(WebCore)/ksvg2/svg/svgtags.in --attrs $(WebCore)/ksvg2/svg/svgattrs.in \
            --namespace SVG --cppNamespace WebCore --namespaceURI "http://www.w3.org/2000/svg" --factory --attrsNullNamespace --output .
endif

XLinkNames.cpp : ksvg2/scripts/make_names.pl ksvg2/misc/xlinkattrs.in
	perl $< --attrs $(WebCore)/ksvg2/misc/xlinkattrs.in \
            --namespace XLink --cppNamespace WebCore --namespaceURI "http://www.w3.org/1999/xlink" --output .

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

else

SVGElementFactory.cpp :
	echo > SVGElementFactory.cpp

SVGNames.cpp :
	echo > SVGNames.cpp

XLinkNames.cpp :
	echo > XLinkNames.cpp

ksvgcssproperties.h :
	echo > ksvgcssproperties.h

ksvgcssvalues.h :
	echo > ksvgcssvalues.h

endif

# new-style Objective-C bindings

OBJC_BINDINGS_SCRIPTS = \
    bindings/scripts/CodeGenerator.pm \
    bindings/scripts/CodeGeneratorObjC.pm \
    bindings/scripts/IDLParser.pm \
    bindings/scripts/IDLStructure.pm \
    bindings/scripts/generate-bindings.pl \
#

DOM%.h : %.idl $(OBJC_BINDINGS_SCRIPTS) bindings/objc/PublicDOMInterfaces.h
	perl -I $(WebCore)/bindings/scripts $(WebCore)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_OBJECTIVE_C" --generator ObjC --include dom --include html --include css --include page --include xml --include ksvg2/svg --include ksvg2/events --outputdir . $<

# new-style JavaScript bindings

JS_BINDINGS_SCRIPTS = \
    bindings/scripts/CodeGenerator.pm \
    bindings/scripts/CodeGeneratorJS.pm \
    bindings/scripts/IDLParser.pm \
    bindings/scripts/IDLStructure.pm \
    bindings/scripts/generate-bindings.pl \
#

JS%.h : %.idl $(JS_BINDINGS_SCRIPTS)
	perl -I $(WebCore)/bindings/scripts $(WebCore)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --include css --include page --include xml --include ksvg2/svg --include ksvg2/events --outputdir . $<
