/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 James G. Speth (speth@end.com)
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

#import "config.h"

#import "DOMImplementationFront.h"
#import "DOMInternal.h"
#import "JSCSSRule.h"
#import "JSCSSRuleList.h"
#import "JSCSSStyleDeclaration.h"
#import "JSCSSValue.h"
#import "JSCounter.h"
#import "JSDOMImplementation.h"
#import "JSEvent.h"
#import "JSHTMLCollection.h"
#import "JSHTMLOptionsCollection.h"
#import "JSMediaList.h"
#import "JSNamedNodeMap.h"
#import "JSNode.h"
#import "JSNodeIterator.h"
#import "JSNodeList.h"
#import "JSRange.h"
#import "JSRect.h"
#import "JSStyleSheet.h"
#import "JSStyleSheetList.h"
#import "JSTreeWalker.h"
#import "JSXPathExpression.h"
#import "JSXPathResult.h"
#import "Node.h"
#import "WebScriptObjectPrivate.h"
#import "kjs_css.h"
#import "kjs_html.h"
#import "kjs_window.h"
#import <objc/objc-runtime.h>

// This file makes use of both the ObjC DOM API and the C++ DOM API, so we need to be careful about what
// headers are included and what namespaces we use to avoid naming conflicts.

// FIXME: This has to be in the KJS namespace to avoid an Objective-C++ ambiguity with C++ and
// Objective-C classes of the same name (even when not in the same namespace). That's also the
// reason for the use of objc_getClass in the WRAP_OLD macro below.

// Some day if the compiler is fixed, or if all the JS wrappers are named with a "JS" prefix,
// we could move the function into the WebCore namespace where it belongs.

namespace KJS {

static inline id createDOMWrapper(KJS::JSObject* object)
{
    #define WRAP(className) \
        if (object->inherits(&WebCore::JS##className::info)) \
            return [DOM##className _wrap##className:static_cast<WebCore::JS##className*>(object)->impl()];

    WRAP(CSSRule)
    WRAP(CSSRuleList)
    WRAP(CSSStyleDeclaration)
    WRAP(CSSValue)
    WRAP(Counter)
    WRAP(Event)
    WRAP(HTMLOptionsCollection)
    WRAP(MediaList)
    WRAP(NamedNodeMap)
    WRAP(Node)
    WRAP(NodeList)
    WRAP(RGBColor)
    WRAP(Range)
    WRAP(Rect)
    WRAP(StyleSheet)
    WRAP(StyleSheetList)
    WRAP(XPathExpression)
    WRAP(XPathResult)

    // This must be after the HTMLOptionsCollection check, because it's a subclass in the JavaScript
    // binding, but not a subclass in the ObjC binding.
    WRAP(HTMLCollection)

    #undef WRAP

    if (object->inherits(&Window::info))
        return [DOMAbstractView _wrapAbstractView:static_cast<Window*>(object)->impl()];
    if (object->inherits(&WebCore::JSDOMImplementation::info))
        return [DOMImplementation _wrapDOMImplementation:implementationFront(static_cast<WebCore::JSDOMImplementation*>(object))];
    if (object->inherits(&WebCore::JSNodeIterator::info))
        return [DOMNodeIterator _wrapNodeIterator:static_cast<WebCore::JSNodeIterator*>(object)->impl() filter:nil];
    if (object->inherits(&WebCore::JSTreeWalker::info))
        return [DOMTreeWalker _wrapTreeWalker:static_cast<WebCore::JSTreeWalker*>(object)->impl() filter:nil];

    return nil;
}

}

namespace WebCore {

id createDOMWrapper(KJS::JSObject* object, PassRefPtr<KJS::Bindings::RootObject> origin, PassRefPtr<KJS::Bindings::RootObject> current)
{
    id wrapper = KJS::createDOMWrapper(object);
    if (![wrapper _hasImp]) // new wrapper, not from cache
        [wrapper _setImp:object originRootObject:origin rootObject:current];
    return wrapper;
}

}
