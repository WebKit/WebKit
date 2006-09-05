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
#import "kjs_dom.h"
#import "Node.h"
#import "kjs_css.h"
#import "JSCSSRuleList.h"

#import "DOMInternal.h"
#import "WebScriptObjectPrivate.h"

#import <objc/objc-runtime.h>

// This file makes use of the ObjC DOM API, and the C++ DOM API, so we need to be careful about what
// headers are included to avoid naming conflicts.

namespace KJS {

void *ScriptInterpreter::createObjcInstanceForValue(ExecState *exec, JSObject *value, const Bindings::RootObject *origin, const Bindings::RootObject *current)
{
    id newObj = nil;
    
    if (value->inherits(&DOMNode::info))
        newObj = [objc_getClass("DOMNode") _nodeWith:static_cast<DOMNode *>(value)->impl()];
    else if (value->inherits(&DOMNodeList::info))
        newObj = [objc_getClass("DOMNodeList") _nodeListWith:static_cast<DOMNodeList *>(value)->impl()];
    else if (value->inherits(&DOMNamedNodeMap::info))
        newObj = [objc_getClass("DOMNamedNodeMap") _namedNodeMapWith:static_cast<DOMNamedNodeMap *>(value)->impl()];
    else if (value->inherits(&DOMStyleSheetList::info))
        newObj = [objc_getClass("DOMStyleSheetList") _styleSheetListWith:static_cast<DOMStyleSheetList *>(value)->impl()];
    else if (value->inherits(&DOMStyleSheet::info))
        newObj = [objc_getClass("DOMStyleSheet") _styleSheetWith:static_cast<DOMStyleSheet *>(value)->impl()];
    else if (value->inherits(&DOMMediaList::info))
        newObj = [objc_getClass("DOMMediaList") _mediaListWith:static_cast<DOMMediaList *>(value)->impl()];
    else if (value->inherits(&WebCore::JSCSSRuleList::info))
        newObj = [objc_getClass("DOMCSSRuleList") _CSSRuleListWith:static_cast<WebCore::JSCSSRuleList *>(value)->impl()];
    else if (value->inherits(&DOMCSSRule::info))
        newObj = [objc_getClass("DOMCSSRule") _CSSRuleWith:static_cast<DOMCSSRule *>(value)->impl()];
    else if (value->inherits(&DOMCSSStyleDeclaration::info))
        newObj = [objc_getClass("DOMCSSStyleDeclaration") _CSSStyleDeclarationWith:static_cast<DOMCSSStyleDeclaration *>(value)->impl()];
    else if (value->inherits(&DOMCSSValue::info))
        newObj = [objc_getClass("DOMCSSValue") _CSSValueWith:static_cast<DOMCSSValue *>(value)->impl()];
    
    if (newObj)
        [newObj _initializeWithObjectImp:value originExecutionContext:origin executionContext:current];
    
    return newObj;
}

}
