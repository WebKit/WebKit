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

#import "DOMInternal.h"

#import "DOMNodeIterator.h"

#import "DOMNodeInternal.h"
#import "DOMNodeIteratorInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/JSExecState.h>
#import <WebCore/NativeNodeFilter.h>
#import <WebCore/Node.h>
#import <WebCore/NodeIterator.h>
#import "ObjCNodeFilterCondition.h"
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>

#define IMPL reinterpret_cast<WebCore::NodeIterator*>(_internal)

@implementation DOMNodeIterator

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([DOMNodeIterator class], self))
        return;

    if (_internal) {
        [self detach];
        IMPL->deref();
    };
    [super dealloc];
}

- (DOMNode *)root
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->root()));
}

- (unsigned)whatToShow
{
    WebCore::JSMainThreadNullState state;
    return IMPL->whatToShow();
}

- (id <DOMNodeFilter>)filter
{
    WebCore::JSMainThreadNullState state;
    return kit(IMPL->filter());
}

- (BOOL)expandEntityReferences
{
    return NO;
}

- (DOMNode *)referenceNode
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->referenceNode()));
}

- (BOOL)pointerBeforeReferenceNode
{
    WebCore::JSMainThreadNullState state;
    return IMPL->pointerBeforeReferenceNode();
}

- (DOMNode *)nextNode
{
    WebCore::JSMainThreadNullState state;
    
    auto result = IMPL->nextNode();
    if (result.hasException())
        return nil;
    
    return kit(WTF::getPtr(result.releaseReturnValue()));
}

- (DOMNode *)previousNode
{
    WebCore::JSMainThreadNullState state;

    auto result = IMPL->previousNode();
    if (result.hasException())
        return nil;
    
    return kit(WTF::getPtr(result.releaseReturnValue()));
}

- (void)detach
{
    WebCore::JSMainThreadNullState state;
    IMPL->detach();
}

@end

DOMNodeIterator *kit(WebCore::NodeIterator* value)
{
    WebCoreThreadViolationCheckRoundOne();
    if (!value)
        return nil;
    if (DOMNodeIterator *wrapper = getDOMWrapper(value))
        return [[wrapper retain] autorelease];
    DOMNodeIterator *wrapper = [[DOMNodeIterator alloc] _init];
    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(value);
    value->ref();
    addDOMWrapper(wrapper, value);
    return [wrapper autorelease];
}

#undef IMPL
