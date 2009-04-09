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

VPATH = $(WEBCORE_PRIVATE_HEADERS_DIR)

INTERNAL_HEADERS_DIR = $(BUILT_PRODUCTS_DIR)/DerivedSources/WebKit
PUBLIC_HEADERS_DIR = $(TARGET_BUILD_DIR)/$(PUBLIC_HEADERS_FOLDER_PATH)
PRIVATE_HEADERS_DIR = $(TARGET_BUILD_DIR)/$(PRIVATE_HEADERS_FOLDER_PATH)

.PHONY : all
all : \
    $(PUBLIC_HEADERS_DIR)/DOM.h \
    $(PUBLIC_HEADERS_DIR)/DOMAbstractView.h \
    $(PUBLIC_HEADERS_DIR)/DOMAttr.h \
    $(PUBLIC_HEADERS_DIR)/DOMCDATASection.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSS.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSCharsetRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSFontFaceRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSImportRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSMediaRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSPageRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSPrimitiveValue.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSRuleList.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSStyleDeclaration.h \
    $(INTERNAL_HEADERS_DIR)/DOMCSSStyleDeclarationInternal.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSStyleRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSStyleSheet.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSUnknownRule.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSValue.h \
    $(PUBLIC_HEADERS_DIR)/DOMCSSValueList.h \
    $(PUBLIC_HEADERS_DIR)/DOMCharacterData.h \
    $(PUBLIC_HEADERS_DIR)/DOMComment.h \
    $(PUBLIC_HEADERS_DIR)/DOMCore.h \
    $(PUBLIC_HEADERS_DIR)/DOMCounter.h \
    $(PUBLIC_HEADERS_DIR)/DOMDocument.h \
    $(INTERNAL_HEADERS_DIR)/DOMDocumentInternal.h \
    $(PUBLIC_HEADERS_DIR)/DOMDocumentFragment.h \
    $(INTERNAL_HEADERS_DIR)/DOMDocumentFragmentInternal.h \
    $(PUBLIC_HEADERS_DIR)/DOMDocumentType.h \
    $(PUBLIC_HEADERS_DIR)/DOMElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMElementTimeControl.h \
    $(PUBLIC_HEADERS_DIR)/DOMEntity.h \
    $(PUBLIC_HEADERS_DIR)/DOMEntityReference.h \
    $(PUBLIC_HEADERS_DIR)/DOMEvent.h \
    $(PUBLIC_HEADERS_DIR)/DOMEventException.h \
    $(PUBLIC_HEADERS_DIR)/DOMEventListener.h \
    $(PUBLIC_HEADERS_DIR)/DOMEventTarget.h \
    $(PUBLIC_HEADERS_DIR)/DOMEvents.h \
    $(PUBLIC_HEADERS_DIR)/DOMException.h \
    $(PUBLIC_HEADERS_DIR)/DOMExtensions.h \
    $(PUBLIC_HEADERS_DIR)/DOMFile.h \
    $(PUBLIC_HEADERS_DIR)/DOMFileList.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTML.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLAnchorElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLAppletElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLAreaElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLBRElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLBaseElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLBaseFontElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLBodyElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLButtonElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLCollection.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLDListElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLDirectoryElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLDivElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLDocument.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMHTMLElementInternal.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLEmbedElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLEmbedElementPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLFieldSetElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLFontElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLFormElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLFrameElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLFrameSetElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLHRElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLHeadElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLHeadingElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLHtmlElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLIFrameElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLImageElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLInputElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMHTMLInputElementInternal.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLIsIndexElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLLIElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLLabelElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLLegendElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLLinkElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLMapElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLMarqueeElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLMenuElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLMetaElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLModElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLOListElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLObjectElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLObjectElementPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLOptGroupElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLOptionElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLOptionsCollection.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLParagraphElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLParamElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLPreElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLQuoteElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLScriptElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLSelectElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLStyleElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTableCaptionElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTableCellElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTableColElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTableElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTableRowElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTableSectionElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTextAreaElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMHTMLTextAreaElementInternal.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLTitleElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMHTMLUListElement.h \
    $(PUBLIC_HEADERS_DIR)/DOMImplementation.h \
    $(PUBLIC_HEADERS_DIR)/DOMKeyboardEvent.h \
    $(PUBLIC_HEADERS_DIR)/DOMMediaList.h \
    $(PUBLIC_HEADERS_DIR)/DOMMouseEvent.h \
    $(PUBLIC_HEADERS_DIR)/DOMMutationEvent.h \
    $(PUBLIC_HEADERS_DIR)/DOMNamedNodeMap.h \
    $(PUBLIC_HEADERS_DIR)/DOMNode.h \
    $(INTERNAL_HEADERS_DIR)/DOMNodeInternal.h \
    $(PUBLIC_HEADERS_DIR)/DOMNodeFilter.h \
    $(PUBLIC_HEADERS_DIR)/DOMNodeIterator.h \
    $(PUBLIC_HEADERS_DIR)/DOMNodeList.h \
    $(PUBLIC_HEADERS_DIR)/DOMNotation.h \
    $(PUBLIC_HEADERS_DIR)/DOMObject.h \
    $(PUBLIC_HEADERS_DIR)/DOMOverflowEvent.h \
    $(PRIVATE_HEADERS_DIR)/DOMPrivate.h \
    $(PUBLIC_HEADERS_DIR)/DOMProcessingInstruction.h \
    $(PUBLIC_HEADERS_DIR)/DOMProgressEvent.h \
    $(PUBLIC_HEADERS_DIR)/DOMRGBColor.h \
    $(PUBLIC_HEADERS_DIR)/DOMRange.h \
    $(INTERNAL_HEADERS_DIR)/DOMRangeInternal.h \
    $(PUBLIC_HEADERS_DIR)/DOMRangeException.h \
    $(PUBLIC_HEADERS_DIR)/DOMRanges.h \
    $(PUBLIC_HEADERS_DIR)/DOMRect.h \
    $(PUBLIC_HEADERS_DIR)/DOMStyleSheet.h \
    $(PUBLIC_HEADERS_DIR)/DOMStyleSheetList.h \
    $(PUBLIC_HEADERS_DIR)/DOMStylesheets.h \
    $(PUBLIC_HEADERS_DIR)/DOMText.h \
    $(PUBLIC_HEADERS_DIR)/DOMTraversal.h \
    $(PUBLIC_HEADERS_DIR)/DOMTreeWalker.h \
    $(PUBLIC_HEADERS_DIR)/DOMUIEvent.h \
    $(PUBLIC_HEADERS_DIR)/DOMViews.h \
    $(PUBLIC_HEADERS_DIR)/DOMWheelEvent.h \
    $(PUBLIC_HEADERS_DIR)/DOMXPath.h \
    $(PUBLIC_HEADERS_DIR)/DOMXPathException.h \
    $(PUBLIC_HEADERS_DIR)/DOMXPathExpression.h \
    $(PUBLIC_HEADERS_DIR)/DOMXPathNSResolver.h \
    $(PUBLIC_HEADERS_DIR)/DOMXPathResult.h \
    $(PRIVATE_HEADERS_DIR)/WebDashboardRegion.h \
    $(PUBLIC_HEADERS_DIR)/WebScriptObject.h \
    $(PUBLIC_HEADERS_DIR)/npapi.h \
    $(PUBLIC_HEADERS_DIR)/npfunctions.h \
    $(PUBLIC_HEADERS_DIR)/npruntime.h \
#

ifeq ($(findstring ENABLE_SVG_DOM_OBJC_BINDINGS,$(FEATURE_DEFINES)), ENABLE_SVG_DOM_OBJC_BINDINGS)

all : \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLFrameElementPrivate.h \
    $(PRIVATE_HEADERS_DIR)/DOMHTMLIFrameElementPrivate.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVG.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAltGlyphElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAltGlyphElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAngle.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAngleInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimateColorElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimateColorElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimateElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimateElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimateTransformElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimateTransformElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimatedAngle.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimatedAngleInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimatedBoolean.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimatedBooleanInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimatedEnumeration.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimatedEnumerationInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimatedInteger.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimatedIntegerInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimatedLength.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimatedLengthInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimatedLengthList.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimatedLengthListInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimatedNumber.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimatedNumberInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimatedNumberList.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimatedNumberListInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimatedPathData.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimatedPoints.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimatedPreserveAspectRatio.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimatedPreserveAspectRatioInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimatedRect.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimatedRectInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimatedString.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimatedStringInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimatedTransformList.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimatedTransformListInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGAnimationElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGAnimationElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGCircleElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGCircleElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGClipPathElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGClipPathElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGColor.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGColorInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGComponentTransferFunctionElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGComponentTransferFunctionElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGCursorElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGCursorElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGDefsElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGDefsElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGDefinitionSrcElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGDescElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGDescElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGDocument.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGDocumentInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGElementInstance.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGElementInstanceInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGElementInstanceList.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGElementInstanceListInternal.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGEllipseElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGEllipseElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGException.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGExternalResourcesRequired.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEBlendElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEBlendElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEColorMatrixElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEColorMatrixElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEComponentTransferElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEComponentTransferElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFECompositeElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFECompositeElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEDiffuseLightingElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEDiffuseLightingElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEDisplacementMapElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEDisplacementMapElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEDistantLightElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEDistantLightElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEFloodElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEFloodElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEFuncAElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEFuncAElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEFuncBElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEFuncBElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEFuncGElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEFuncGElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEFuncRElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEFuncRElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEGaussianBlurElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEGaussianBlurElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEImageElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEImageElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEMergeElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEMergeElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEMergeNodeElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEMergeNodeElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEOffsetElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEOffsetElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFEPointLightElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFEPointLightElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFESpecularLightingElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFESpecularLightingElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFESpotLightElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFESpotLightElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFETileElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFETileElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFETurbulenceElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFETurbulenceElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFilterElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGFilterElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFilterPrimitiveStandardAttributes.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFitToViewBox.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFontElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFontFaceElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFontFaceFormatElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFontFaceNameElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFontFaceSrcElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGFontFaceUriElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGForeignObjectElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGForeignObjectElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGGElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGGElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGGlyphElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGGradientElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGGradientElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGImageElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGImageElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGLangSpace.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGLength.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGLengthInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGLengthList.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGLengthListInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGLineElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGLineElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGLinearGradientElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGLinearGradientElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGLocatable.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGMarkerElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGMarkerElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGMaskElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGMaskElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGMatrix.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGMatrixInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGMetadataElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGMetadataElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGMissingGlyphElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGNumber.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGNumberList.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGNumberListInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPaint.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPaintInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSeg.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegArcAbs.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegArcAbsInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegArcRel.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegArcRelInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegClosePath.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegClosePathInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegCurvetoCubicAbs.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegCurvetoCubicAbsInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegCurvetoCubicRel.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegCurvetoCubicRelInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegCurvetoCubicSmoothAbs.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegCurvetoCubicSmoothAbsInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegCurvetoCubicSmoothRel.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegCurvetoCubicSmoothRelInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegCurvetoQuadraticAbs.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegCurvetoQuadraticAbsInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegCurvetoQuadraticRel.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegCurvetoQuadraticRelInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegCurvetoQuadraticSmoothAbs.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegCurvetoQuadraticSmoothAbsInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegCurvetoQuadraticSmoothRel.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegCurvetoQuadraticSmoothRelInternal.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegLinetoAbs.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegLinetoAbsInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegLinetoHorizontalAbs.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegLinetoHorizontalAbsInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegLinetoHorizontalRel.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegLinetoHorizontalRelInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegLinetoRel.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegLinetoRelInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegLinetoVerticalAbs.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegLinetoVerticalAbsInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegLinetoVerticalRel.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegLinetoVerticalRelInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegList.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegListInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegMovetoAbs.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegMovetoAbsInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPathSegMovetoRel.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPathSegMovetoRelInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPatternElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPatternElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPoint.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPointList.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPointListInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPolygonElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPolygonElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPolylineElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPolylineElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGPreserveAspectRatio.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGPreserveAspectRatioInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGRadialGradientElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGRadialGradientElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGRect.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGRectElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGRectElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGRenderingIntent.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGRenderingIntentInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGSVGElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGSVGElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGScriptElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGScriptElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGSetElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGSetElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGStopElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGStopElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGStringList.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGStringListInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGStylable.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGStyleElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGStyleElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGSwitchElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGSwitchElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGSymbolElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGSymbolElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGTRefElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGTRefElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGTSpanElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGTSpanElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGTests.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGTextContentElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGTextContentElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGTextElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGTextElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGTextPathElement.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGTextPositioningElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGTextPositioningElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGTitleElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGTitleElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGTransform.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGTransformInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGTransformList.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGTransformListInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGTransformable.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGURIReference.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGUnitTypes.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGUnitTypesInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGUseElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGUseElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGViewElement.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGViewElementInternal.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGZoomAndPan.h \
    $(PRIVATE_HEADERS_DIR)/DOMSVGZoomEvent.h \
    $(INTERNAL_HEADERS_DIR)/DOMSVGZoomEventInternal.h \

endif

REPLACE_RULES = -e s/\<WebCore/\<WebKit/ -e s/DOMDOMImplementation/DOMImplementation/
HEADER_MIGRATE_CMD = sed $(REPLACE_RULES) $< $(PROCESS_HEADER_FOR_MACOSX_TARGET_CMD) > $@

ifeq ($(MACOSX_DEPLOYMENT_TARGET),10.4)
PROCESS_HEADER_FOR_MACOSX_TARGET_CMD = | ( unifdef -DBUILDING_ON_TIGER || exit 0 )
else
PROCESS_HEADER_FOR_MACOSX_TARGET_CMD = | ( unifdef -UBUILDING_ON_TIGER || exit 0 )
endif

$(PUBLIC_HEADERS_DIR)/DOM% : DOMDOM% MigrateHeaders.make
	$(HEADER_MIGRATE_CMD)

$(PRIVATE_HEADERS_DIR)/DOM% : DOMDOM% MigrateHeaders.make
	$(HEADER_MIGRATE_CMD)

$(PUBLIC_HEADERS_DIR)/% : % MigrateHeaders.make
	$(HEADER_MIGRATE_CMD)

$(PRIVATE_HEADERS_DIR)/% : % MigrateHeaders.make
	$(HEADER_MIGRATE_CMD)

$(INTERNAL_HEADERS_DIR)/% : % MigrateHeaders.make
	$(HEADER_MIGRATE_CMD)
