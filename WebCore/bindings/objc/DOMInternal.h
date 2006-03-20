/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

namespace WebCore {
    class CSSStyleDeclaration;
    class CSSStyleSheet;
    class DocumentFragment;
    class Document;
    class DocumentType;
    class Element;
    class NodeFilter;
    class Node;
    class NodeIterator;
    class NodeList;
    class Range;
    class StyleSheetList;
    class TreeWalker;

    typedef int ExceptionCode;
}

@interface DOMNode (WebCoreInternal)
+ (DOMNode *)_nodeWith:(WebCore::Node *)impl;
- (WebCore::Node *)_node;
@end

@interface DOMNodeList (WebCoreInternal)
+ (DOMNodeList *)_nodeListWith:(WebCore::NodeList *)impl;
@end

@interface DOMElement (WebCoreInternal)
+ (DOMElement *)_elementWith:(WebCore::Element *)impl;
- (WebCore::Element *)_element;
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

@interface DOMRange (WebCoreInternal)
+ (DOMRange *)_rangeWith:(WebCore::Range *)impl;
- (WebCore::Range *)_range;
@end

@interface DOMNodeIterator (WebCoreInternal)
+ (DOMNodeIterator *)_nodeIteratorWith:(WebCore::NodeIterator *)impl filter:(id <DOMNodeFilter>)filter;
@end

@interface DOMTreeWalker (WebCoreInternal)
+ (DOMTreeWalker *)_treeWalkerWith:(WebCore::TreeWalker *)impl filter:(id <DOMNodeFilter>)filter;
@end

@interface DOMObject (WebCoreInternal)
- (id)_init;
@end

@interface DOMCSSStyleDeclaration (WebCoreInternal)
+ (DOMCSSStyleDeclaration *)_styleDeclarationWith:(WebCore::CSSStyleDeclaration *)impl;
- (WebCore::CSSStyleDeclaration *)_styleDeclaration;
@end

@interface DOMStyleSheetList (WebCoreInternal)
+ (DOMStyleSheetList *)_styleSheetListWith:(WebCore::StyleSheetList *)impl;
@end

@interface DOMCSSStyleSheet (WebCoreInternal)
+ (DOMCSSStyleSheet *)_CSSStyleSheetWith:(WebCore::CSSStyleSheet *)impl;
@end

@interface DOMNodeFilter : DOMObject <DOMNodeFilter>
+ (DOMNodeFilter *)_nodeFilterWith:(WebCore::NodeFilter *)impl;
@end

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
