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
#import "DOMAttr.h"
#import "DOMCDATASection.h"
#import "DOMCSSPrimitiveValue.h"
#import "DOMCSSRule.h"
#import "DOMCSSRuleList.h"
#import "DOMCSSStyleDeclaration.h"
#import "DOMCSSStyleSheet.h"
#import "DOMCSSValue.h"
#import "DOMComment.h"
#import "DOMCounter.h"
#import "DOMDOMImplementation.h"
#import "DOMDocument.h"
#import "DOMDocumentFragment.h"
#import "DOMDocumentType.h"
#import "DOMElement.h"
#import "DOMEntityReference.h"
#import "DOMEvents.h"
#import "DOMHTMLCollection.h"
#import "DOMHTMLDocument.h"
#import "DOMHTMLElement.h"
#import "DOMHTMLFormElement.h"
#import "DOMHTMLImageElement.h"
#import "DOMHTMLInputElement.h"
#import "DOMHTMLObjectElement.h"
#import "DOMHTMLOptionElement.h"
#import "DOMHTMLOptionsCollection.h"
#import "DOMHTMLTableCaptionElement.h"
#import "DOMHTMLTableCellElement.h"
#import "DOMHTMLTableElement.h"
#import "DOMHTMLTableSectionElement.h"
#import "DOMMediaList.h"
#import "DOMNamedNodeMap.h"
#import "DOMNode.h"
#import "DOMNodeList.h"
#import "DOMObject.h"
#import "DOMProcessingInstruction.h"
#import "DOMRGBColor.h"
#import "DOMRect.h"
#import "DOMStyleSheet.h"
#import "DOMStyleSheetList.h"
#import "DOMText.h"
#import "DOMViews.h"

#ifdef XPATH_SUPPORT
#import "DOMXPath.h"
#endif // XPATH_SUPPORT

namespace WebCore {
    class Attr;
    class CDATASection;
    class CSSRule;
    class CSSRuleList;
    class CSSStyleDeclaration;
    class CSSStyleSheet;
    class CSSValue;
    class Comment;
    class Counter;
    class DOMImplementationFront;
    class DOMWindow;
    class Document;
    class DocumentFragment;
    class DocumentType;
    class Element;
    class EntityReference;
    class Event;
    class HTMLCollection;
    class HTMLDocument;
    class HTMLElement;
    class HTMLFormElement;
    class HTMLImageElement;
    class HTMLInputElement;
    class HTMLObjectElement;
    class HTMLOptionElement;
    class HTMLOptionsCollection;
    class HTMLTableCaptionElement;
    class HTMLTableCellElement;
    class HTMLTableElement;
    class HTMLTableSectionElement;
    class MediaList;
    class NamedNodeMap;
    class Node;
    class NodeFilter;
    class NodeIterator;
    class NodeList;
    class ProcessingInstruction;
    class Range;
    class RectImpl;
    class StyleSheet;
    class StyleSheetList;
    class Text;
    class TreeWalker;

#ifdef XPATH_SUPPORT
    class XPathExpression;
    class XPathNSResolver;
    class XPathResult;
#endif // XPATH_SUPPORT

    typedef int ExceptionCode;
    typedef DOMWindow AbstractView;
}

// Core Internal Interfaces

@interface DOMObject (WebCoreInternal)
- (id)_init;
@end

@interface DOMNode (WebCoreInternal)
+ (DOMNode *)_nodeWith:(WebCore::Node *)impl;
- (WebCore::Node *)_node;
@end

@interface DOMNamedNodeMap (WebCoreInternal)
+ (DOMNamedNodeMap *)_namedNodeMapWith:(WebCore::NamedNodeMap *)impl;
@end

@interface DOMNodeList (WebCoreInternal)
+ (DOMNodeList *)_nodeListWith:(WebCore::NodeList *)impl;
@end

@interface DOMText (WebCoreInternal)
+ (DOMText *)_textWith:(WebCore::Text *)impl;
@end

@interface DOMComment (WebCoreInternal)
+ (DOMComment *)_commentWith:(WebCore::Comment *)impl;
@end

@interface DOMCDATASection (WebCoreInternal)
+ (DOMCDATASection *)_CDATASectionWith:(WebCore::CDATASection *)impl;
@end

@interface DOMProcessingInstruction (WebCoreInternal)
+ (DOMProcessingInstruction *)_processingInstructionWith:(WebCore::ProcessingInstruction *)impl;
@end

@interface DOMEntityReference (WebCoreInternal)
+ (DOMEntityReference *)_entityReferenceWith:(WebCore::EntityReference *)impl;
@end

@interface DOMElement (WebCoreInternal)
+ (DOMElement *)_elementWith:(WebCore::Element *)impl;
- (WebCore::Element *)_element;
@end

@interface DOMAttr (WebCoreInternal)
+ (DOMAttr *)_attrWith:(WebCore::Attr *)impl;
- (WebCore::Attr *)_attr;
@end

@interface DOMDocumentType (WebCoreInternal)
+ (DOMDocumentType *)_documentTypeWith:(WebCore::DocumentType *)impl;
- (WebCore::DocumentType *)_documentType;
@end

@interface DOMImplementation (WebCoreInternal)
+ (DOMImplementation *)_DOMImplementationWith:(WebCore::DOMImplementationFront *)impl;
- (WebCore::DOMImplementationFront *)_DOMImplementation;
@end

@interface DOMDocument (WebCoreInternal)
+ (DOMDocument *)_documentWith:(WebCore::Document *)impl;
- (WebCore::Document *)_document;
- (DOMElement *)_ownerElement;
@end

@interface DOMDocumentFragment (WebCoreInternal)
+ (DOMDocumentFragment *)_documentFragmentWith:(WebCore::DocumentFragment *)impl;
- (WebCore::DocumentFragment *)_fragment;
@end

// HTML Internal Interfaces

@interface DOMHTMLOptionsCollection (WebCoreInternal)
+ (DOMHTMLOptionsCollection *)_HTMLOptionsCollectionWith:(WebCore::HTMLOptionsCollection *)impl;
@end

@interface DOMHTMLDocument (WebCoreInternal)
+ (DOMHTMLDocument *)_HTMLDocumentWith:(WebCore::HTMLDocument *)impl;
@end

@interface DOMHTMLCollection (WebCoreInternal)
+ (DOMHTMLCollection *)_HTMLCollectionWith:(WebCore::HTMLCollection *)impl;
@end

@interface DOMHTMLElement (WebCoreInternal)
+ (DOMHTMLElement *)_HTMLElementWith:(WebCore::HTMLElement *)impl;
- (WebCore::HTMLElement *)_HTMLElement;
@end

@interface DOMHTMLFormElement (WebCoreInternal)
+ (DOMHTMLFormElement *)_HTMLFormElementWith:(WebCore::HTMLFormElement *)impl;
@end

@interface DOMHTMLTableCaptionElement (WebCoreInternal)
+ (DOMHTMLTableCaptionElement *)_HTMLTableCaptionElementWith:(WebCore::HTMLTableCaptionElement *)impl;
- (WebCore::HTMLTableCaptionElement *)_HTMLTableCaptionElement;
@end

@interface DOMHTMLTableSectionElement (WebCoreInternal)
+ (DOMHTMLTableSectionElement *)_HTMLTableSectionElementWith:(WebCore::HTMLTableSectionElement *)impl;
- (WebCore::HTMLTableSectionElement *)_HTMLTableSectionElement;
@end

@interface DOMHTMLTableElement (WebCoreInternal)
+ (DOMHTMLTableElement *)_HTMLTableElementWith:(WebCore::HTMLTableElement *)impl;
- (WebCore::HTMLTableElement *)_HTMLTableElement;
@end

@interface DOMHTMLTableCellElement (WebCoreInternal)
+ (DOMHTMLTableCellElement *)_HTMLTableCellElementWith:(WebCore::HTMLTableCellElement *)impl;
- (WebCore::HTMLTableCellElement *)_HTMLTableCellElement;
@end

@interface DOMHTMLImageElement (WebCoreInternal)
- (WebCore::HTMLImageElement *)_HTMLImageElement;
@end

@interface DOMHTMLObjectElement (WebCoreInternal)
- (WebCore::HTMLObjectElement *)_HTMLObjectElement;
@end

@interface DOMHTMLOptionElement (WebCoreInternal)
- (WebCore::HTMLOptionElement *)_HTMLOptionElement;
@end

@interface DOMHTMLInputElement (WebCoreInternal)
- (WebCore::HTMLInputElement *)_HTMLInputElement;
@end

// CSS Internal Interfaces

@interface DOMCSSRuleList (WebCoreInternal)
+ (DOMCSSRuleList *)_CSSRuleListWith:(WebCore::CSSRuleList *)impl;
@end

@interface DOMCSSRule (WebCoreInternal)
+ (DOMCSSRule *)_CSSRuleWith:(WebCore::CSSRule *)impl;
@end

@interface DOMCSSValue (WebCoreInternal)
+ (DOMCSSValue *)_CSSValueWith:(WebCore::CSSValue *)impl;
@end

@interface DOMCSSPrimitiveValue (WebCoreInternal)
+ (DOMCSSPrimitiveValue *)_CSSPrimitiveValueWith:(WebCore::CSSValue *)impl;
@end

@interface DOMRGBColor (WebCoreInternal)
+ (DOMRGBColor *)_RGBColorWithRGB:(WebCore::RGBA32)value;
@end

@interface DOMRect (WebCoreInternal)
+ (DOMRect *)_rectWith:(WebCore::RectImpl *)impl;
@end

@interface DOMCounter (WebCoreInternal)
+ (DOMCounter *)_counterWith:(WebCore::Counter *)impl;
@end

@interface DOMCSSStyleDeclaration (WebCoreInternal)
+ (DOMCSSStyleDeclaration *)_CSSStyleDeclarationWith:(WebCore::CSSStyleDeclaration *)impl;
- (WebCore::CSSStyleDeclaration *)_CSSStyleDeclaration;
@end

@interface DOMCSSStyleSheet (WebCoreInternal)
+ (DOMCSSStyleSheet *)_CSSStyleSheetWith:(WebCore::CSSStyleSheet *)impl;
@end

// StyleSheets Internal Interfaces

@interface DOMStyleSheet (WebCoreInternal)
+ (DOMStyleSheet *)_styleSheetWith:(WebCore::StyleSheet *)impl;
@end

@interface DOMStyleSheetList (WebCoreInternal)
+ (DOMStyleSheetList *)_styleSheetListWith:(WebCore::StyleSheetList *)impl;
@end

@interface DOMMediaList (WebCoreInternal)
+ (DOMMediaList *)_mediaListWith:(WebCore::MediaList *)impl;
@end

// Events Internal Interfaces

@interface DOMEvent (WebCoreInternal)
+ (DOMEvent *)_eventWith:(WebCore::Event *)impl;
- (WebCore::Event *)_event;
@end

// Range Internal Interfaces

@interface DOMRange (WebCoreInternal)
+ (DOMRange *)_rangeWith:(WebCore::Range *)impl;
- (WebCore::Range *)_range;
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

// Views Internal Interfaces

@interface DOMAbstractView (WebCoreInternal)
+ (DOMAbstractView *)_abstractViewWith:(WebCore::AbstractView *)impl;
- (WebCore::AbstractView *)_abstractView;
@end

#ifdef XPATH_SUPPORT
// XPath Internal Interfaces

@interface DOMXPathResult (WebCoreInternal)
+ (DOMXPathResult *)_xpathResultWith:(WebCore::XPathResult *)impl;
- (WebCore::XPathResult *)_xpathResult;
@end

@interface DOMXPathExpression (WebCoreInternal)
+ (DOMXPathExpression *)_xpathExpressionWith:(WebCore::XPathExpression *)impl;
- (WebCore::XPathExpression *)_xpathExpression;
@end

@interface DOMNativeXPathNSResolver : DOMObject <DOMXPathNSResolver>
+ (DOMNativeXPathNSResolver *)_xpathNSResolverWith:(WebCore::XPathNSResolver *)impl;
- (WebCore::XPathNSResolver *)_xpathNSResolver;
@end

#endif // XPATH_SUPPORT


// Helper functions for DOM wrappers and gluing to Objective-C

// Like reinterpret_cast, but a compiler error if you use it on the wrong type.
template <class Target, class Source> Target DOM_cast(Source) { Source::failToCompile(); }

// Type safe DOM wrapper access.

NSObject* getDOMWrapper(DOMObjectInternal*);
void addDOMWrapper(NSObject* wrapper, DOMObjectInternal*);

template <class Source> inline id getDOMWrapper(Source impl) { return getDOMWrapper(DOM_cast<DOMObjectInternal*>(impl)); }
template <class Source> inline void addDOMWrapper(NSObject* wrapper, Source impl) { addDOMWrapper(wrapper, DOM_cast<DOMObjectInternal*>(impl)); }
void removeDOMWrapper(DOMObjectInternal*);

void raiseDOMException(WebCore::ExceptionCode);

inline void raiseOnDOMError(WebCore::ExceptionCode ec) 
{
    if (ec) 
        raiseDOMException(ec);
}

// Implementation details for the above.

#define ALLOW_DOM_CAST(type) \
    namespace WebCore { class type; } \
    template <> inline DOMObjectInternal* DOM_cast<DOMObjectInternal*, class WebCore::type*>(class WebCore::type* p) \
        { return reinterpret_cast<DOMObjectInternal *>(p); } \
    template <> inline class WebCore::type* DOM_cast<class WebCore::type*, DOMObjectInternal*>(DOMObjectInternal* p) \
        { return reinterpret_cast<class WebCore::type*>(p); }

// No class should appear in this list if its base class is already here.
ALLOW_DOM_CAST(Counter)
ALLOW_DOM_CAST(CSSRule)
ALLOW_DOM_CAST(CSSRuleList)
ALLOW_DOM_CAST(CSSStyleDeclaration)
ALLOW_DOM_CAST(CSSStyleSheet)
ALLOW_DOM_CAST(CSSValue)
ALLOW_DOM_CAST(DOMImplementationFront)
ALLOW_DOM_CAST(HTMLCollection)
ALLOW_DOM_CAST(HTMLOptionsCollection)
ALLOW_DOM_CAST(MediaList)
ALLOW_DOM_CAST(NamedNodeMap)
ALLOW_DOM_CAST(NodeFilter)
ALLOW_DOM_CAST(Node)
ALLOW_DOM_CAST(NodeIterator)
ALLOW_DOM_CAST(NodeList)
ALLOW_DOM_CAST(Range)
ALLOW_DOM_CAST(RectImpl)
ALLOW_DOM_CAST(StyleSheet)
ALLOW_DOM_CAST(StyleSheetList)
ALLOW_DOM_CAST(TreeWalker)
#ifdef XPATH_SUPPORT
ALLOW_DOM_CAST(XPathExpression)
ALLOW_DOM_CAST(XPathNSResolver)
ALLOW_DOM_CAST(XPathResult)
#endif // XPATH_SUPPORT
