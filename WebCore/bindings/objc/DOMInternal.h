/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#import <WebCore/DOM.h>

namespace DOM {
    class CSSStyleDeclarationImpl;
    class CSSStyleSheetImpl;
    class DocumentFragmentImpl;
    class DocumentImpl;
    class DocumentTypeImpl;
    class ElementImpl;
    class NodeFilterImpl;
    class NodeImpl;
    class NodeIteratorImpl;
    class NodeListImpl;
    class RangeImpl;
    class StyleSheetListImpl;
    class TreeWalkerImpl;
}

@interface DOMNode (WebCoreInternal)
+ (DOMNode *)_nodeWithImpl:(DOM::NodeImpl *)impl;
- (DOM::NodeImpl *)_nodeImpl;
@end

@interface DOMNodeList (WebCoreInternal)
+ (DOMNodeList *)_nodeListWithImpl:(DOM::NodeListImpl *)impl;
@end

@interface DOMElement (WebCoreInternal)
+ (DOMElement *)_elementWithImpl:(DOM::ElementImpl *)impl;
- (DOM::ElementImpl *)_elementImpl;
@end

@interface DOMDocument (WebCoreInternal)
+ (DOMDocument *)_documentWithImpl:(DOM::DocumentImpl *)impl;
- (DOM::DocumentImpl *)_documentImpl;
- (DOMElement *)_ownerElement;
@end

@interface DOMDocumentFragment (WebCoreInternal)
+ (DOMDocumentFragment *)_documentFragmentWithImpl:(DOM::DocumentFragmentImpl *)impl;
- (DOM::DocumentFragmentImpl *)_fragmentImpl;
@end

@interface DOMRange (WebCoreInternal)
+ (DOMRange *)_rangeWithImpl:(DOM::RangeImpl *)impl;
- (DOM::RangeImpl *)_rangeImpl;
@end

@interface DOMNodeIterator (WebCoreInternal)
+ (DOMNodeIterator *)_nodeIteratorWithImpl:(DOM::NodeIteratorImpl *)impl filter:(id <DOMNodeFilter>)filter;
@end

@interface DOMTreeWalker (WebCoreInternal)
+ (DOMTreeWalker *)_treeWalkerWithImpl:(DOM::TreeWalkerImpl *)impl filter:(id <DOMNodeFilter>)filter;
@end

@interface DOMObject (WebCoreInternal)
- (id)_init;
@end

@interface DOMCSSStyleDeclaration (WebCoreInternal)
+ (DOMCSSStyleDeclaration *)_styleDeclarationWithImpl:(DOM::CSSStyleDeclarationImpl *)impl;
- (DOM::CSSStyleDeclarationImpl *)_styleDeclarationImpl;
@end

@interface DOMStyleSheetList (WebCoreInternal)
+ (DOMStyleSheetList *)_styleSheetListWithImpl:(DOM::StyleSheetListImpl *)impl;
@end

@interface DOMCSSStyleSheet (WebCoreInternal)
+ (DOMCSSStyleSheet *)_CSSStyleSheetWithImpl:(DOM::CSSStyleSheetImpl *)impl;
@end

@interface DOMNodeFilter : DOMObject <DOMNodeFilter>
+ (DOMNodeFilter *)_nodeFilterWithImpl:(DOM::NodeFilterImpl *)impl;
@end

// Helper functions for DOM wrappers and gluing to Objective-C

// Like reinterpret_cast, but a compiler error if you use it on the wrong type.
template <class Target, class Source> Target DOM_cast(Source) { Source::failToCompile(); }

// Type safe DOM wrapper access.

id getDOMWrapperImpl(DOMObjectInternal *impl);
void addDOMWrapperImpl(id wrapper, DOMObjectInternal *impl);

template <class Source> inline id getDOMWrapper(Source impl) { return getDOMWrapperImpl(DOM_cast<DOMObjectInternal *>(impl)); }
template <class Source> inline void addDOMWrapper(NSObject * wrapper, Source impl) { addDOMWrapperImpl(wrapper, DOM_cast<DOMObjectInternal *>(impl)); }
void removeDOMWrapper(DOMObjectInternal *impl);

void raiseDOMException(int code);

inline void raiseOnDOMError(int code) 
{
    if (code) 
        raiseDOMException(code);
}

// Implementation details for the above.

#define ALLOW_DOM_CAST(type) \
    namespace DOM { class type; } \
    template <> inline DOMObjectInternal *DOM_cast<DOMObjectInternal *, DOM::type *>(DOM::type *p) \
        { return reinterpret_cast<DOMObjectInternal *>(p); } \
    template <> inline DOM::type *DOM_cast<DOM::type *, DOMObjectInternal *>(DOMObjectInternal *p) \
        { return reinterpret_cast<DOM::type *>(p); }

// No class should appear in this list if it's base class is already here.
ALLOW_DOM_CAST(CounterImpl)
ALLOW_DOM_CAST(CSSRuleImpl)
ALLOW_DOM_CAST(CSSRuleListImpl)
ALLOW_DOM_CAST(CSSStyleDeclarationImpl)
ALLOW_DOM_CAST(CSSStyleSheetImpl)
ALLOW_DOM_CAST(CSSValueImpl)
ALLOW_DOM_CAST(DOMImplementationImpl)
ALLOW_DOM_CAST(HTMLCollectionImpl)
ALLOW_DOM_CAST(HTMLOptionsCollectionImpl)
ALLOW_DOM_CAST(MediaListImpl)
ALLOW_DOM_CAST(NamedNodeMapImpl)
ALLOW_DOM_CAST(NodeFilterImpl)
ALLOW_DOM_CAST(NodeImpl)
ALLOW_DOM_CAST(NodeIteratorImpl)
ALLOW_DOM_CAST(NodeListImpl)
ALLOW_DOM_CAST(RangeImpl)
ALLOW_DOM_CAST(RectImpl)
ALLOW_DOM_CAST(StyleSheetImpl)
ALLOW_DOM_CAST(StyleSheetListImpl)
ALLOW_DOM_CAST(TreeWalkerImpl)
