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

#import "DOMTreeWalkerInternal.h"

#import "DOMInternal.h"
#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/JSExecState.h>
#import <WebCore/NativeNodeFilter.h>
#import <WebCore/Node.h>
#import "ObjCNodeFilterCondition.h"
#import <WebCore/ThreadCheck.h>
#import <WebCore/TreeWalker.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>

#define IMPL reinterpret_cast<WebCore::TreeWalker*>(_internal)

@implementation DOMTreeWalker

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([DOMTreeWalker class], self))
        return;

    if (_internal)
        IMPL->deref();
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

- (DOMNode *)currentNode
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->currentNode()));
}

- (void)setCurrentNode:(DOMNode *)newCurrentNode
{
    WebCore::JSMainThreadNullState state;
    ASSERT(newCurrentNode);

    if (!core(newCurrentNode))
        raiseTypeErrorException();
    IMPL->setCurrentNode(*core(newCurrentNode));
}

- (DOMNode *)parentNode
{
    WebCore::JSMainThreadNullState state;

    auto result = IMPL->parentNode();
    if (result.hasException())
        return nil;
    
    return kit(WTF::getPtr(result.releaseReturnValue()));
}

- (DOMNode *)firstChild
{
    WebCore::JSMainThreadNullState state;

    auto result = IMPL->firstChild();
    if (result.hasException())
        return nil;
    
    return kit(WTF::getPtr(result.releaseReturnValue()));
}

- (DOMNode *)lastChild
{
    WebCore::JSMainThreadNullState state;

    auto result = IMPL->lastChild();
    if (result.hasException())
        return nil;
    
    return kit(WTF::getPtr(result.releaseReturnValue()));
}

- (DOMNode *)previousSibling
{
    WebCore::JSMainThreadNullState state;

    auto result = IMPL->previousSibling();
    if (result.hasException())
        return nil;
    
    return kit(WTF::getPtr(result.releaseReturnValue()));
}

- (DOMNode *)nextSibling
{
    WebCore::JSMainThreadNullState state;

    auto result = IMPL->nextSibling();
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

- (DOMNode *)nextNode
{
    WebCore::JSMainThreadNullState state;

    auto result = IMPL->nextNode();
    if (result.hasException())
        return nil;
    
    return kit(WTF::getPtr(result.releaseReturnValue()));
}

@end

DOMTreeWalker *kit(WebCore::TreeWalker* value)
{
    WebCoreThreadViolationCheckRoundOne();
    if (!value)
        return nil;
    if (DOMTreeWalker *wrapper = getDOMWrapper(value))
        return [[wrapper retain] autorelease];
    DOMTreeWalker *wrapper = [[DOMTreeWalker alloc] _init];
    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(value);
    value->ref();
    addDOMWrapper(wrapper, value);
    return [wrapper autorelease];
}

#undef IMPL
