/*
 * Copyright (C) 2004-2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 James G. Speth (speth@end.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "DOM.h"

#import "Color.h"
#import "DOMObject.h"
#import "DOMRGBColor.h"
#import "HitTestResult.h"

#if ENABLE(XPATH)
#import "DOMXPathExpressionInternal.h"
#import "DOMXPathNSResolver.h"
#import "DOMXPathResultInternal.h"
#endif // ENABLE(XPATH)

// Auto-generated internal interfaces
#import "DOMAbstractViewInternal.h"
#import "DOMAttrInternal.h"
#import "DOMCDATASectionInternal.h"
#import "DOMCSSCharsetRuleInternal.h"
#import "DOMCSSFontFaceRuleInternal.h"
#import "DOMCSSImportRuleInternal.h"
#import "DOMCSSMediaRuleInternal.h"
#import "DOMCSSPageRuleInternal.h"
#import "DOMCSSPrimitiveValueInternal.h"
#import "DOMCSSRuleInternal.h"
#import "DOMCSSRuleListInternal.h"
#import "DOMCSSStyleDeclarationInternal.h"
#import "DOMCSSStyleRuleInternal.h"
#import "DOMCSSStyleSheetInternal.h"
#import "DOMCSSUnknownRuleInternal.h"
#import "DOMCSSValueInternal.h"
#import "DOMCSSValueListInternal.h"
#import "DOMCharacterDataInternal.h"
#import "DOMCommentInternal.h"
#import "DOMCounterInternal.h"
#import "DOMDOMImplementationInternal.h"
#import "DOMDocumentFragmentInternal.h"
#import "DOMDocumentInternal.h"
#import "DOMDocumentTypeInternal.h"
#import "DOMElementInternal.h"
#import "DOMEntityInternal.h"
#import "DOMEntityReferenceInternal.h"
#import "DOMEventInternal.h"
#import "DOMHTMLAnchorElementInternal.h"
#import "DOMHTMLAppletElementInternal.h"
#import "DOMHTMLAreaElementInternal.h"
#import "DOMHTMLBRElementInternal.h"
#import "DOMHTMLBaseElementInternal.h"
#import "DOMHTMLBaseFontElementInternal.h"
#import "DOMHTMLBodyElementInternal.h"
#import "DOMHTMLButtonElementInternal.h"
#import "DOMHTMLCollectionInternal.h"
#import "DOMHTMLDListElementInternal.h"
#import "DOMHTMLDirectoryElementInternal.h"
#import "DOMHTMLDivElementInternal.h"
#import "DOMHTMLDocumentInternal.h"
#import "DOMHTMLElementInternal.h"
#import "DOMHTMLEmbedElementInternal.h"
#import "DOMHTMLFieldSetElementInternal.h"
#import "DOMHTMLFontElementInternal.h"
#import "DOMHTMLFormElementInternal.h"
#import "DOMHTMLFrameElementInternal.h"
#import "DOMHTMLFrameSetElementInternal.h"
#import "DOMHTMLHRElementInternal.h"
#import "DOMHTMLHeadElementInternal.h"
#import "DOMHTMLHeadingElementInternal.h"
#import "DOMHTMLHtmlElementInternal.h"
#import "DOMHTMLIFrameElementInternal.h"
#import "DOMHTMLImageElementInternal.h"
#import "DOMHTMLInputElementInternal.h"
#import "DOMHTMLIsIndexElementInternal.h"
#import "DOMHTMLLIElementInternal.h"
#import "DOMHTMLLabelElementInternal.h"
#import "DOMHTMLLegendElementInternal.h"
#import "DOMHTMLLinkElementInternal.h"
#import "DOMHTMLMapElementInternal.h"
#import "DOMHTMLMarqueeElementInternal.h"
#import "DOMHTMLMenuElementInternal.h"
#import "DOMHTMLMetaElementInternal.h"
#import "DOMHTMLModElementInternal.h"
#import "DOMHTMLOListElementInternal.h"
#import "DOMHTMLObjectElementInternal.h"
#import "DOMHTMLOptGroupElementInternal.h"
#import "DOMHTMLOptionElementInternal.h"
#import "DOMHTMLOptionsCollectionInternal.h"
#import "DOMHTMLParagraphElementInternal.h"
#import "DOMHTMLParamElementInternal.h"
#import "DOMHTMLPreElementInternal.h"
#import "DOMHTMLQuoteElementInternal.h"
#import "DOMHTMLScriptElementInternal.h"
#import "DOMHTMLSelectElementInternal.h"
#import "DOMHTMLStyleElementInternal.h"
#import "DOMHTMLTableCaptionElementInternal.h"
#import "DOMHTMLTableCellElementInternal.h"
#import "DOMHTMLTableColElementInternal.h"
#import "DOMHTMLTableElementInternal.h"
#import "DOMHTMLTableRowElementInternal.h"
#import "DOMHTMLTableSectionElementInternal.h"
#import "DOMHTMLTextAreaElementInternal.h"
#import "DOMHTMLTitleElementInternal.h"
#import "DOMHTMLUListElementInternal.h"
#import "DOMKeyboardEventInternal.h"
#import "DOMMediaListInternal.h"
#import "DOMMouseEventInternal.h"
#import "DOMMutationEventInternal.h"
#import "DOMNamedNodeMapInternal.h"
#import "DOMNodeInternal.h"
#import "DOMNodeIteratorInternal.h"
#import "DOMNodeListInternal.h"
#import "DOMNotationInternal.h"
#import "DOMOverflowEventInternal.h"
#import "DOMProcessingInstructionInternal.h"
#import "DOMRangeInternal.h"
#import "DOMRectInternal.h"
#import "DOMStyleSheetInternal.h"
#import "DOMStyleSheetListInternal.h"
#import "DOMTextInternal.h"
#import "DOMTextEventInternal.h"
#import "DOMTreeWalkerInternal.h"
#import "DOMUIEventInternal.h"
#import "DOMWheelEventInternal.h"

#if ENABLE(SVG)
#import "DOMSVGAElementInternal.h"
#import "DOMSVGAngleInternal.h"
#import "DOMSVGAnimateColorElementInternal.h"
#import "DOMSVGAnimateElementInternal.h"
#import "DOMSVGAnimateTransformElementInternal.h"
#import "DOMSVGAnimatedAngleInternal.h"
#import "DOMSVGAnimatedBooleanInternal.h"
#import "DOMSVGAnimatedEnumerationInternal.h"
#import "DOMSVGAnimatedIntegerInternal.h"
#import "DOMSVGAnimatedLengthInternal.h"
#import "DOMSVGAnimatedLengthListInternal.h"
#import "DOMSVGAnimatedNumberInternal.h"
#import "DOMSVGAnimatedNumberListInternal.h"
#import "DOMSVGAnimatedPreserveAspectRatioInternal.h"
#import "DOMSVGAnimatedRectInternal.h"
#import "DOMSVGAnimatedStringInternal.h"
#import "DOMSVGAnimatedTransformListInternal.h"
#import "DOMSVGAnimationElementInternal.h"
#import "DOMSVGCircleElementInternal.h"
#import "DOMSVGClipPathElementInternal.h"
#import "DOMSVGColorInternal.h"
#import "DOMSVGComponentTransferFunctionElementInternal.h"
#import "DOMSVGCursorElementInternal.h"
#import "DOMSVGDefsElementInternal.h"
#import "DOMSVGDefinitionSrcElementInternal.h"
#import "DOMSVGDescElementInternal.h"
#import "DOMSVGDocumentInternal.h"
#import "DOMSVGElementInternal.h"
#import "DOMSVGElementInstanceInternal.h"
#import "DOMSVGElementInstanceListInternal.h"
#import "DOMSVGEllipseElementInternal.h"
#import "DOMSVGFEBlendElementInternal.h"
#import "DOMSVGFEColorMatrixElementInternal.h"
#import "DOMSVGFEComponentTransferElementInternal.h"
#import "DOMSVGFECompositeElementInternal.h"
#import "DOMSVGFEDiffuseLightingElementInternal.h"
#import "DOMSVGFEDisplacementMapElementInternal.h"
#import "DOMSVGFEDistantLightElementInternal.h"
#import "DOMSVGFEFloodElementInternal.h"
#import "DOMSVGFEFuncAElementInternal.h"
#import "DOMSVGFEFuncBElementInternal.h"
#import "DOMSVGFEFuncGElementInternal.h"
#import "DOMSVGFEFuncRElementInternal.h"
#import "DOMSVGFEGaussianBlurElementInternal.h"
#import "DOMSVGFEImageElementInternal.h"
#import "DOMSVGFEMergeElementInternal.h"
#import "DOMSVGFEMergeNodeElementInternal.h"
#import "DOMSVGFEOffsetElementInternal.h"
#import "DOMSVGFEPointLightElementInternal.h"
#import "DOMSVGFESpecularLightingElementInternal.h"
#import "DOMSVGFESpotLightElementInternal.h"
#import "DOMSVGFETileElementInternal.h"
#import "DOMSVGFETurbulenceElementInternal.h"
#import "DOMSVGFilterElementInternal.h"
#import "DOMSVGFontElementInternal.h"
#import "DOMSVGFontFaceElementInternal.h"
#import "DOMSVGFontFaceFormatElementInternal.h"
#import "DOMSVGFontFaceNameElementInternal.h"
#import "DOMSVGFontFaceSrcElementInternal.h"
#import "DOMSVGFontFaceUriElementInternal.h"
#import "DOMSVGForeignObjectElementInternal.h"
#import "DOMSVGGElementInternal.h"
#import "DOMSVGGlyphElementInternal.h"
#import "DOMSVGGradientElementInternal.h"
#import "DOMSVGImageElementInternal.h"
#import "DOMSVGLengthInternal.h"
#import "DOMSVGLengthListInternal.h"
#import "DOMSVGLineElementInternal.h"
#import "DOMSVGLinearGradientElementInternal.h"
#import "DOMSVGMarkerElementInternal.h"
#import "DOMSVGMaskElementInternal.h"
#import "DOMSVGMatrixInternal.h"
#import "DOMSVGMetadataElementInternal.h"
#import "DOMSVGMissingGlyphElementInternal.h"
#import "DOMSVGNumberInternal.h"
#import "DOMSVGNumberListInternal.h"
#import "DOMSVGPaintInternal.h"
#import "DOMSVGPathElementInternal.h"
#import "DOMSVGPathSegArcAbsInternal.h"
#import "DOMSVGPathSegArcRelInternal.h"
#import "DOMSVGPathSegClosePathInternal.h"
#import "DOMSVGPathSegCurvetoCubicAbsInternal.h"
#import "DOMSVGPathSegCurvetoCubicRelInternal.h"
#import "DOMSVGPathSegCurvetoCubicSmoothAbsInternal.h"
#import "DOMSVGPathSegCurvetoCubicSmoothRelInternal.h"
#import "DOMSVGPathSegCurvetoQuadraticAbsInternal.h"
#import "DOMSVGPathSegCurvetoQuadraticRelInternal.h"
#import "DOMSVGPathSegCurvetoQuadraticSmoothAbsInternal.h"
#import "DOMSVGPathSegCurvetoQuadraticSmoothRelInternal.h"
#import "DOMSVGPathSegInternal.h"
#import "DOMSVGPathSegLinetoAbsInternal.h"
#import "DOMSVGPathSegLinetoHorizontalAbsInternal.h"
#import "DOMSVGPathSegLinetoHorizontalRelInternal.h"
#import "DOMSVGPathSegLinetoRelInternal.h"
#import "DOMSVGPathSegLinetoVerticalAbsInternal.h"
#import "DOMSVGPathSegLinetoVerticalRelInternal.h"
#import "DOMSVGPathSegListInternal.h"
#import "DOMSVGPathSegMovetoAbsInternal.h"
#import "DOMSVGPathSegMovetoRelInternal.h"
#import "DOMSVGPatternElementInternal.h"
#import "DOMSVGPointInternal.h"
#import "DOMSVGPointListInternal.h"
#import "DOMSVGPolygonElementInternal.h"
#import "DOMSVGPolylineElementInternal.h"
#import "DOMSVGPreserveAspectRatioInternal.h"
#import "DOMSVGRadialGradientElementInternal.h"
#import "DOMSVGRectElementInternal.h"
#import "DOMSVGRectInternal.h"
#import "DOMSVGRenderingIntentInternal.h"
#import "DOMSVGSVGElementInternal.h"
#import "DOMSVGScriptElementInternal.h"
#import "DOMSVGSetElementInternal.h"
#import "DOMSVGStopElementInternal.h"
#import "DOMSVGStringListInternal.h"
#import "DOMSVGStyleElementInternal.h"
#import "DOMSVGSwitchElementInternal.h"
#import "DOMSVGSymbolElementInternal.h"
#import "DOMSVGTRefElementInternal.h"
#import "DOMSVGTSpanElementInternal.h"
#import "DOMSVGTextContentElementInternal.h"
#import "DOMSVGTextElementInternal.h"
#import "DOMSVGTextPathElementInternal.h"
#import "DOMSVGTextPositioningElementInternal.h"
#import "DOMSVGTitleElementInternal.h"
#import "DOMSVGTransformInternal.h"
#import "DOMSVGTransformListInternal.h"
#import "DOMSVGUnitTypesInternal.h"
#import "DOMSVGUseElementInternal.h"
#import "DOMSVGViewElementInternal.h"
#import "DOMSVGZoomEventInternal.h"
#endif // ENABLE(SVG)

namespace KJS {
    class JSObject;
    
    namespace Bindings {
        class RootObject;
    }
}

namespace WebCore {
    class NodeFilter;

#if ENABLE(SVG)
    class AffineTransform;
    class FloatPoint;
    class FloatRect;
#endif // ENABLE(SVG)

#if ENABLE(XPATH)
    class XPathNSResolver;
#endif // ENABLE(XPATH)
}

// Core Internal Interfaces

@interface DOMObject (WebCoreInternal)
- (id)_init;
@end

// CSS Internal Interfaces

@interface DOMRGBColor (WebCoreInternal)
+ (DOMRGBColor *)_wrapRGBColor:(WebCore::RGBA32)value;
- (WebCore::RGBA32)_RGBColor;
@end

// Traversal Internal Interfaces

@interface DOMNodeFilter : DOMObject <DOMNodeFilter>
+ (DOMNodeFilter *)_wrapNodeFilter:(WebCore::NodeFilter *)impl;
@end

#if ENABLE(XPATH)

// XPath Internal Interfaces

@interface DOMNativeXPathNSResolver : DOMObject <DOMXPathNSResolver>
+ (DOMNativeXPathNSResolver *)_wrapXPathNSResolver:(WebCore::XPathNSResolver *)impl;
- (WebCore::XPathNSResolver *)_xpathNSResolver;
@end

#endif // ENABLE(XPATH)

// Helper functions for DOM wrappers and gluing to Objective-C

namespace WebCore {

    id createDOMWrapper(KJS::JSObject*, PassRefPtr<KJS::Bindings::RootObject> origin, PassRefPtr<KJS::Bindings::RootObject> current);

    NSObject* getDOMWrapper(DOMObjectInternal*);
    void addDOMWrapper(NSObject* wrapper, DOMObjectInternal*);
    void removeDOMWrapper(DOMObjectInternal*);

    template <class Source>
    inline id getDOMWrapper(Source impl)
    {
        return getDOMWrapper(reinterpret_cast<DOMObjectInternal*>(impl));
    }

    template <class Source>
    inline void addDOMWrapper(NSObject* wrapper, Source impl)
    {
        addDOMWrapper(wrapper, reinterpret_cast<DOMObjectInternal*>(impl));
    }

} // namespace WebCore
