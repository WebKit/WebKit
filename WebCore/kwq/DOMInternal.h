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

#import "DOM.h"

namespace DOM {
    class CSSStyleDeclarationImpl;
    class CSSStyleSheetImpl;
    class DocumentImpl;
    class ElementImpl;
    class NodeImpl;
    class NodeListImpl;
    class RangeImpl;
    class StyleSheetListImpl;
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
@end

@interface DOMRange (WebCoreInternal)
+ (DOMRange *)_rangeWithImpl:(DOM::RangeImpl *)impl;
- (DOM::RangeImpl *)_rangeImpl;
@end

@interface DOMObject (WebCoreInternal)
- (id)_init;
@end

@interface DOMCSSStyleDeclaration (WebCoreInternal)
+ (DOMCSSStyleDeclaration *)_styleDeclarationWithImpl:(DOM::CSSStyleDeclarationImpl *)impl;
@end

@interface DOMStyleSheetList (WebCoreInternal)
+ (DOMStyleSheetList *)_styleSheetListWithImpl:(DOM::StyleSheetListImpl *)impl;
@end

@interface DOMCSSStyleSheet (WebCoreInternal)
+ (DOMCSSStyleSheet *)_CSSStyleSheetWithImpl:(DOM::CSSStyleSheetImpl *)impl;
@end

// Helper functions for DOM wrappers and gluing to Objective-C

id getDOMWrapperForImpl(const void *impl);
void setDOMWrapperForImpl(id wrapper, const void *impl);
void removeDOMWrapperForImpl(const void *impl);
void raiseDOMException(int code);

inline void raiseOnDOMError(int code) 
{
    if (code) 
        raiseDOMException(code);
}
