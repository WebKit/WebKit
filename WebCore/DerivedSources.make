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
    $(WebCore)/storage \
    $(WebCore)/xml \
    $(WebCore)/svg
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
    DOMMessageEvent.h \
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
    DOMSVGDefinitionSrcElement.h \
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
    DOMSVGFontElement.h \
    DOMSVGFontFaceElement.h \
    DOMSVGFontFaceFormatElement.h \
    DOMSVGFontFaceNameElement.h \
    DOMSVGFontFaceSrcElement.h \
    DOMSVGFontFaceUriElement.h \
    DOMSVGFilterElement.h \
    DOMSVGFilterPrimitiveStandardAttributes.h \
    DOMSVGFitToViewBox.h \
    DOMSVGForeignObjectElement.h \
    DOMSVGGElement.h \
    DOMSVGGlyphElement.h \
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
    DOMSVGMissingGlyphElement.h \
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
    DOMSVGTextPathElement.h \
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
    JSConsole.h \
    JSCounter.h \
    JSCSSStyleDeclaration.h \
    JSDOMCoreException.h \
    JSDOMImplementation.h \
    JSDOMParser.h \
    JSDOMSelection.h \
    JSDOMWindow.h \
    JSDatabase.h \
    JSDocument.h \
    JSDocumentFragment.h \
    JSDocumentType.h \
    JSElement.h \
    JSEntity.h \
    JSEntityReference.h \
    JSEvent.h \
    JSEventException.h \
    JSEventTargetBase.lut.h \
    JSHTMLAnchorElement.h \
    JSHTMLAppletElement.h \
    JSHTMLAreaElement.h \
    JSHTMLAudioElement.h \
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
    JSHTMLMediaElement.h \
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
    JSHTMLSourceElement.h \
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
    JSHTMLVideoElement.h \
    JSHistory.h \
    JSKeyboardEvent.h \
    JSLocation.lut.h \
    JSMediaError.h \
    JSMediaList.h \
    JSMessageEvent.h \
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
    JSProgressEvent.h \
    JSRange.h \
    JSRangeException.h \
    JSRect.h \
    JSSQLError.h \
    JSSQLResultSet.h \
    JSSQLResultSetRowList.h \
    JSSQLTransaction.h \
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
    JSSVGDefinitionSrcElement.h \
    JSSVGDescElement.h \
    JSSVGDocument.h \
    JSSVGException.h \
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
    JSSVGTextPathElement.h \
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
    JSSVGFontElement.h \
    JSSVGFontFaceElement.h \
    JSSVGFontFaceFormatElement.h \
    JSSVGFontFaceNameElement.h \
    JSSVGFontFaceSrcElement.h \
    JSSVGFontFaceUriElement.h \
    JSSVGForeignObjectElement.h \
    JSSVGGElement.h \
    JSSVGGlyphElement.h \
    JSSVGGradientElement.h \
    JSSVGImageElement.h \
    JSSVGLength.h \
    JSSVGLengthList.h \
    JSSVGLineElement.h \
    JSSVGLinearGradientElement.h \
    JSSVGMaskElement.h \
    JSSVGMarkerElement.h \
    JSSVGMissingGlyphElement.h \
    JSSVGTransform.h \
    JSSVGZoomEvent.h \
    JSScreen.h \
    JSStyleSheet.h \
    JSStyleSheetList.h \
    JSText.h \
    JSTextEvent.h \
    JSTimeRanges.h \
    JSTreeWalker.h \
    JSUIEvent.h \
    JSVoidCallback.h \
    JSWheelEvent.h \
    JSXMLHttpRequest.lut.h \
    JSXMLHttpRequestException.h \
    JSXMLSerializer.h \
    JSXPathEvaluator.h \
    JSXPathException.h \
    JSXPathExpression.h \
    JSXPathNSResolver.h \
    JSXPathResult.h \
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
    tokenizer.cpp \
    WebCore.exp \
#

# CSS property names and value keywords

ifeq ($(findstring ENABLE_SVG,$(FEATURE_DEFINES)), ENABLE_SVG)

CSSPropertyNames.h : css/CSSPropertyNames.in css/SVGCSSPropertyNames.in
	if sort $< $(WebCore)/css/SVGCSSPropertyNames.in | uniq -d | grep -E '^[^#]'; then echo 'Duplicate value!'; exit 1; fi
	cat $< $(WebCore)/css/SVGCSSPropertyNames.in > CSSPropertyNames.in
	perl "$(WebCore)/css/makeprop.pl"

CSSValueKeywords.h : css/CSSValueKeywords.in css/SVGCSSValueKeywords.in
	# Lower case all the values, as CSS values are case-insensitive
	perl -ne 'print lc' $(WebCore)/css/SVGCSSValueKeywords.in > SVGCSSValueKeywords.in
	if sort $< SVGCSSValueKeywords.in | uniq -d | grep -E '^[^#]'; then echo 'Duplicate value!'; exit 1; fi
	cat $< SVGCSSValueKeywords.in > CSSValueKeywords.in
	perl "$(WebCore)/css/makevalues.pl"

else

CSSPropertyNames.h : css/CSSPropertyNames.in css/makeprop.pl
	cp $< CSSPropertyNames.in
	perl "$(WebCore)/css/makeprop.pl"

CSSValueKeywords.h : css/CSSValueKeywords.in css/makevalues.pl
	cp $< CSSValueKeywords.in
	perl "$(WebCore)/css/makevalues.pl"

endif 


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

CharsetData.cpp : platform/text/mac/make-charset-table.pl platform/text/mac/character-sets.txt platform/text/mac/mac-encodings.txt
	perl $^ kTextEncoding > $@

# lookup tables for old-style JavaScript bindings

%.lut.h: %.cpp $(CREATE_HASH_TABLE)
	$(CREATE_HASH_TABLE) $< > $@
%Table.cpp: %.cpp $(CREATE_HASH_TABLE)
	$(CREATE_HASH_TABLE) $< > $@

# HTML tag and attribute names

HTMLNames.cpp : dom/make_names.pl html/HTMLTagNames.in html/HTMLAttributeNames.in
	perl $< --tags $(WebCore)/html/HTMLTagNames.in --attrs $(WebCore)/html/HTMLAttributeNames.in \
            --namespace HTML --namespacePrefix xhtml --cppNamespace WebCore --namespaceURI "http://www.w3.org/1999/xhtml" --attrsNullNamespace --output .

XMLNames.cpp : dom/make_names.pl xml/xmlattrs.in
	perl $< --attrs $(WebCore)/xml/xmlattrs.in \
            --namespace XML --cppNamespace WebCore --namespaceURI "http://www.w3.org/XML/1998/namespace" --output .

ifeq ($(findstring ENABLE_SVG,$(FEATURE_DEFINES)), ENABLE_SVG)

WEBCORE_EXPORT_DEPENDENCIES := WebCore.SVG.exp

ifeq ($(findstring ENABLE_SVG_USE,$(FEATURE_DEFINES)), ENABLE_SVG_USE)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_USE=1
endif

ifeq ($(findstring ENABLE_SVG_FONTS,$(FEATURE_DEFINES)), ENABLE_SVG_FONTS)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_FONTS=1
endif

ifeq ($(findstring ENABLE_SVG_FILTERS,$(FEATURE_DEFINES)), ENABLE_SVG_FILTERS)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_FILTERS=1
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.SVG.Filters.exp
endif

ifeq ($(findstring ENABLE_SVG_AS_IMAGE,$(FEATURE_DEFINES)), ENABLE_SVG_AS_IMAGE)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_AS_IMAGE=1
endif

ifeq ($(findstring ENABLE_SVG_ANIMATION,$(FEATURE_DEFINES)), ENABLE_SVG_ANIMATION)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_ANIMATION=1
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.SVG.Animation.exp
endif

ifeq ($(findstring ENABLE_SVG_FOREIGN_OBJECT,$(FEATURE_DEFINES)), ENABLE_SVG_FOREIGN_OBJECT)
	SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_FOREIGN_OBJECT=1
	WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.SVG.ForeignObject.exp
endif

# SVG tag and attribute names (need to pass an extra flag if svg experimental features are enabled)
ifdef SVG_FLAGS
SVGElementFactory.cpp SVGNames.cpp : dom/make_names.pl svg/svgtags.in svg/svgattrs.in
	perl $< --tags $(WebCore)/svg/svgtags.in --attrs $(WebCore)/svg/svgattrs.in --extraDefines "$(SVG_FLAGS)" \
            --namespace SVG --cppNamespace WebCore --namespaceURI "http://www.w3.org/2000/svg" --factory --attrsNullNamespace --output .
else
SVGElementFactory.cpp SVGNames.cpp : dom/make_names.pl svg/svgtags.in svg/svgattrs.in
	perl $< --tags $(WebCore)/svg/svgtags.in --attrs $(WebCore)/svg/svgattrs.in \
            --namespace SVG --cppNamespace WebCore --namespaceURI "http://www.w3.org/2000/svg" --factory --attrsNullNamespace --output .

endif

XLinkNames.cpp : dom/make_names.pl svg/xlinkattrs.in
	perl $< --attrs $(WebCore)/svg/xlinkattrs.in \
            --namespace XLink --cppNamespace WebCore --namespaceURI "http://www.w3.org/1999/xlink" --output .

# Add SVG Symbols to the WebCore exported symbols file
WebCore.exp : WebCore.base.exp $(WEBCORE_EXPORT_DEPENDENCIES)
	cat $^ > $@

else

SVGElementFactory.cpp :
	echo > $@

SVGNames.cpp :
	echo > $@

XLinkNames.cpp :
	echo > $@

WebCore.exp : WebCore.base.exp
	cat $^ > $@

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
	perl -I $(WebCore)/bindings/scripts $(WebCore)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_OBJECTIVE_C" --generator ObjC --include dom --include html --include css --include page --include xml --include svg --outputdir . $<

# new-style JavaScript bindings

JS_BINDINGS_SCRIPTS = \
    bindings/scripts/CodeGenerator.pm \
    bindings/scripts/CodeGeneratorJS.pm \
    bindings/scripts/IDLParser.pm \
    bindings/scripts/IDLStructure.pm \
    bindings/scripts/generate-bindings.pl \
#

JS%.h : %.idl $(JS_BINDINGS_SCRIPTS)
	perl -I $(WebCore)/bindings/scripts $(WebCore)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --include css --include page --include xml --include svg --outputdir . $<
