/*
 * Copyright (C) 2004-2006 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"

#import "DOMInternal.h"
#import "JSCSSRuleList.h"
#import "Node.h"
#import "WebScriptObjectPrivate.h"
#import "kjs_css.h"
#import "kjs_dom.h"
#import <objc/objc-runtime.h>

// This file makes use of the ObjC DOM API, and the C++ DOM API, so we need to be careful about what
// headers are included to avoid naming conflicts.

namespace KJS {

id createDOMWrapper(JSObject* object, PassRefPtr<Bindings::RootObject> origin, PassRefPtr<Bindings::RootObject> current)
{
    id newObj = nil;
    
    if (object->inherits(&DOMNode::info))
        newObj = [objc_getClass("DOMNode") _nodeWith:static_cast<DOMNode*>(object)->impl()];
    else if (object->inherits(&DOMNodeList::info))
        newObj = [objc_getClass("DOMNodeList") _nodeListWith:static_cast<DOMNodeList*>(object)->impl()];
    else if (object->inherits(&DOMNamedNodeMap::info))
        newObj = [objc_getClass("DOMNamedNodeMap") _namedNodeMapWith:static_cast<DOMNamedNodeMap*>(object)->impl()];
    else if (object->inherits(&DOMStyleSheetList::info))
        newObj = [objc_getClass("DOMStyleSheetList") _styleSheetListWith:static_cast<DOMStyleSheetList*>(object)->impl()];
    else if (object->inherits(&DOMStyleSheet::info))
        newObj = [objc_getClass("DOMStyleSheet") _styleSheetWith:static_cast<DOMStyleSheet*>(object)->impl()];
    else if (object->inherits(&DOMMediaList::info))
        newObj = [objc_getClass("DOMMediaList") _mediaListWith:static_cast<DOMMediaList*>(object)->impl()];
    else if (object->inherits(&WebCore::JSCSSRuleList::info))
        newObj = [objc_getClass("DOMCSSRuleList") _CSSRuleListWith:static_cast<WebCore::JSCSSRuleList*>(object)->impl()];
    else if (object->inherits(&DOMCSSRule::info))
        newObj = [objc_getClass("DOMCSSRule") _CSSRuleWith:static_cast<DOMCSSRule*>(object)->impl()];
    else if (object->inherits(&DOMCSSStyleDeclaration::info))
        newObj = [objc_getClass("DOMCSSStyleDeclaration") _CSSStyleDeclarationWith:static_cast<DOMCSSStyleDeclaration*>(object)->impl()];
    else if (object->inherits(&DOMCSSValue::info))
        newObj = [objc_getClass("DOMCSSValue") _CSSValueWith:static_cast<DOMCSSValue*>(object)->impl()];
    
    [newObj _initializeWithObjectImp:object originRootObject:origin rootObject:current];
    return newObj;
}

} // namespace KJS
