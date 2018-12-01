/*
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "DOMDocumentFragmentInternal.h"

#import "DOMElementInternal.h"
#import "DOMHTMLCollectionInternal.h"
#import "DOMNodeInternal.h"
#import "DOMNodeListInternal.h"
#import <WebCore/DocumentFragment.h>
#import <WebCore/Element.h>
#import "ExceptionHandlers.h"
#import <WebCore/HTMLCollection.h>
#import <WebCore/JSExecState.h>
#import <WebCore/NameNodeList.h>
#import <WebCore/NodeList.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::DocumentFragment*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMDocumentFragment

- (DOMHTMLCollection *)children
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->children()));
}

- (DOMElement *)firstElementChild
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->firstElementChild()));
}

- (DOMElement *)lastElementChild
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->lastElementChild()));
}

- (unsigned)childElementCount
{
    WebCore::JSMainThreadNullState state;
    return IMPL->childElementCount();
}

- (DOMElement *)getElementById:(NSString *)elementId
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->getElementById(AtomicString(elementId))));
}

- (DOMElement *)querySelector:(NSString *)selectors
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->querySelector(selectors)));
}

- (DOMNodeList *)querySelectorAll:(NSString *)selectors
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->querySelectorAll(selectors)).ptr());
}

@end

WebCore::DocumentFragment* core(DOMDocumentFragment *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::DocumentFragment*>(wrapper->_internal) : 0;
}

DOMDocumentFragment *kit(WebCore::DocumentFragment* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMDocumentFragment*>(kit(static_cast<WebCore::Node*>(value)));
}
