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

#import "config.h"
#import "DOMXPathResultInternal.h"

#import "DOMInternal.h"
#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import "JSMainThreadExecState.h"
#import "Node.h"
#import "ThreadCheck.h"
#import "URL.h"
#import "WebCoreObjCExtras.h"
#import "WebScriptObjectPrivate.h"
#import "XPathResult.h"
#import <wtf/GetPtr.h>

#define IMPL reinterpret_cast<WebCore::XPathResult*>(_internal)

@implementation DOMXPathResult

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([DOMXPathResult class], self))
        return;

    if (_internal)
        IMPL->deref();
    [super dealloc];
}

- (unsigned short)resultType
{
    WebCore::JSMainThreadNullState state;
    return IMPL->resultType();
}

- (double)numberValue
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    double result = IMPL->numberValue(ec);
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (NSString *)stringValue
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    NSString *result = IMPL->stringValue(ec);
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (BOOL)booleanValue
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    BOOL result = IMPL->booleanValue(ec);
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMNode *)singleNodeValue
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMNode *result = kit(WTF::getPtr(IMPL->singleNodeValue(ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (BOOL)invalidIteratorState
{
    WebCore::JSMainThreadNullState state;
    return IMPL->invalidIteratorState();
}

- (unsigned)snapshotLength
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    unsigned result = IMPL->snapshotLength(ec);
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMNode *)iterateNext
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMNode *result = kit(WTF::getPtr(IMPL->iterateNext(ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMNode *)snapshotItem:(unsigned)index
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMNode *result = kit(WTF::getPtr(IMPL->snapshotItem(index, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

@end

WebCore::XPathResult* core(DOMXPathResult *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::XPathResult*>(wrapper->_internal) : 0;
}

DOMXPathResult *kit(WebCore::XPathResult* value)
{
    WebCoreThreadViolationCheckRoundOne();
    if (!value)
        return nil;
    if (DOMXPathResult *wrapper = getDOMWrapper(value))
        return [[wrapper retain] autorelease];
    DOMXPathResult *wrapper = [[DOMXPathResult alloc] _init];
    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(value);
    value->ref();
    addDOMWrapper(wrapper, value);
    return [wrapper autorelease];
}
