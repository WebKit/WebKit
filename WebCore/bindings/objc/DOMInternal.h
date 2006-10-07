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
#import "DOMCSSRule.h"
#import "DOMCSSValue.h"
#import "DOMEvents.h"
#import "DOMNode.h"
#import "DOMObject.h"
#import "DOMRGBColor.h"
#import "DOMStyleSheet.h"

#ifdef SVG_SUPPORT
#import "DOMSVGNumber.h"
#import "DOMSVGPoint.h"
#import "DOMSVGRect.h"
#endif // SVG_SUPPORT

#ifdef XPATH_SUPPORT
#import "DOMXPathNSResolver.h"
#endif // XPATH_SUPPORT

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
#import "DOMCSSRuleListInternal.h"
#import "DOMCSSStyleDeclarationInternal.h"
#import "DOMCSSStyleRuleInternal.h"
#import "DOMCSSStyleSheetInternal.h"
#import "DOMCSSUnknownRuleInternal.h"
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
#import "DOMHTMLAnchorElementInternal.h"
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
#import "DOMNodeListInternal.h"
#import "DOMNotationInternal.h"
#import "DOMOverflowEventInternal.h"
#import "DOMProcessingInstructionInternal.h"
#import "DOMRangeInternal.h"
#import "DOMRectInternal.h"
#import "DOMStyleSheetListInternal.h"
#import "DOMTextInternal.h"
#import "DOMUIEventInternal.h"
#import "DOMWheelEventInternal.h"

#ifdef SVG_SUPPORT
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
#import "DOMSVGDescElementInternal.h"
#import "DOMSVGDocumentInternal.h"
#import "DOMSVGElementInternal.h"
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
#import "DOMSVGFilterElement.h"
#import "DOMSVGForeignObjectElement.h"
#import "DOMSVGGElement.h"
#import "DOMSVGGradientElement.h"
#import "DOMSVGImageElement.h"
#import "DOMSVGLengthInternal.h"
#import "DOMSVGLengthListInternal.h"
#import "DOMSVGLineElement.h"
#import "DOMSVGLinearGradientElement.h"
#import "DOMSVGMarkerElement.h"
#import "DOMSVGMaskElement.h"
#import "DOMSVGMatrixInternal.h"
#import "DOMSVGMetadataElementInternal.h"
#import "DOMSVGNumberListInternal.h"
#import "DOMSVGPaint.h"
#import "DOMSVGPathSegInternal.h"
#import "DOMSVGPathSegListInternal.h"
#import "DOMSVGPreserveAspectRatioInternal.h"
#import "DOMSVGRectElementInternal.h"
#import "DOMSVGStringListInternal.h"
#import "DOMSVGStyleElementInternal.h"
#import "DOMSVGTransformInternal.h"
#import "DOMSVGTransformListInternal.h"
#endif // SVG_SUPPORT

#ifdef XPATH_SUPPORT
#import "DOMXPathExpressionInternal.h"
#import "DOMXPathResultInternal.h"
#endif // XPATH_SUPPORT

namespace WebCore {
    class CSSRule;
    class CSSValue;
    class Event;
    class Node;
    class NodeFilter;
    class NodeIterator;
    class StyleSheet;
    class TreeWalker;

#ifdef SVG_SUPPORT
    class FloatPoint;
    class FloatRect;
#endif // SVG_SUPPORT

#ifdef XPATH_SUPPORT
    class XPathNSResolver;
#endif // XPATH_SUPPORT

    typedef int ExceptionCode;
}

// Core Internal Interfaces

@interface DOMObject (WebCoreInternal)
- (id)_init;
@end

@interface DOMNode (WebCoreInternal)
+ (DOMNode *)_nodeWith:(WebCore::Node *)impl;
- (WebCore::Node *)_node;
@end

// CSS Internal Interfaces

@interface DOMCSSRule (WebCoreInternal)
+ (DOMCSSRule *)_CSSRuleWith:(WebCore::CSSRule *)impl;
- (WebCore::CSSRule *)_CSSRule;
@end

@interface DOMCSSValue (WebCoreInternal)
+ (DOMCSSValue *)_CSSValueWith:(WebCore::CSSValue *)impl;
- (WebCore::CSSValue *)_CSSValue;
@end

@interface DOMRGBColor (WebCoreInternal)
+ (DOMRGBColor *)_RGBColorWithRGB:(WebCore::RGBA32)value;
@end

// StyleSheets Internal Interfaces

@interface DOMStyleSheet (WebCoreInternal)
+ (DOMStyleSheet *)_styleSheetWith:(WebCore::StyleSheet *)impl;
- (WebCore::StyleSheet *)_styleSheet;
@end

// Events Internal Interfaces

@interface DOMEvent (WebCoreInternal)
+ (DOMEvent *)_eventWith:(WebCore::Event *)impl;
- (WebCore::Event *)_event;
@end

// Traversal Internal Interfaces

@interface DOMNodeIterator (WebCoreInternal)
+ (DOMNodeIterator *)_nodeIteratorWith:(WebCore::NodeIterator *)impl filter:(id <DOMNodeFilter>)filter;
@end

@interface DOMTreeWalker (WebCoreInternal)
+ (DOMTreeWalker *)_treeWalkerWith:(WebCore::TreeWalker *)impl filter:(id <DOMNodeFilter>)filter;
@end

@interface DOMNodeFilter : DOMObject <DOMNodeFilter>
+ (DOMNodeFilter *)_nodeFilterWith:(WebCore::NodeFilter *)impl;
@end


#ifdef SVG_SUPPORT
// SVG Internal Interfaces

@interface DOMSVGNumber (WebCoreInternal)
+ (DOMSVGNumber *)_SVGNumberWith:(float)value;
- (float)_SVGNumber;
@end

@interface DOMSVGPoint (WebCoreInternal)
+ (DOMSVGPoint *)_SVGPointWith:(WebCore::FloatPoint)impl;
- (WebCore::FloatPoint)_SVGPoint;
@end

@interface DOMSVGRect (WebCoreInternal)
+ (DOMSVGRect *)_SVGRectWith:(WebCore::FloatRect)impl;
- (WebCore::FloatRect)_SVGRect;
@end

#endif // SVG_SUPPORT


#ifdef XPATH_SUPPORT
// XPath Internal Interfaces

@interface DOMNativeXPathNSResolver : DOMObject <DOMXPathNSResolver>
+ (DOMNativeXPathNSResolver *)_xpathNSResolverWith:(WebCore::XPathNSResolver *)impl;
- (WebCore::XPathNSResolver *)_xpathNSResolver;
@end

#endif // XPATH_SUPPORT


// Helper functions for DOM wrappers and gluing to Objective-C

NSObject* getDOMWrapper(DOMObjectInternal*);
void addDOMWrapper(NSObject* wrapper, DOMObjectInternal*);

template <class Source> inline id getDOMWrapper(Source impl) { return getDOMWrapper(reinterpret_cast<DOMObjectInternal*>(impl)); }
template <class Source> inline void addDOMWrapper(NSObject* wrapper, Source impl) { addDOMWrapper(wrapper, reinterpret_cast<DOMObjectInternal*>(impl)); }
void removeDOMWrapper(DOMObjectInternal*);

void raiseDOMException(WebCore::ExceptionCode);

inline void raiseOnDOMError(WebCore::ExceptionCode ec) 
{
    if (ec) 
        raiseDOMException(ec);
}
