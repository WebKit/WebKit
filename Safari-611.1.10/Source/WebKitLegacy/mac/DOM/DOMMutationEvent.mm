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

#import "DOMMutationEvent.h"

#import "DOMEventInternal.h"
#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/JSExecState.h>
#import <WebCore/MutationEvent.h>
#import <WebCore/Node.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::MutationEvent*>(reinterpret_cast<WebCore::Event*>(_internal))

@implementation DOMMutationEvent

- (DOMNode *)relatedNode
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->relatedNode()));
}

- (NSString *)prevValue
{
    WebCore::JSMainThreadNullState state;
    return IMPL->prevValue();
}

- (NSString *)newValue
{
    WebCore::JSMainThreadNullState state;
    return IMPL->newValue();
}

- (NSString *)attrName
{
    WebCore::JSMainThreadNullState state;
    return IMPL->attrName();
}

- (unsigned short)attrChange
{
    WebCore::JSMainThreadNullState state;
    return IMPL->attrChange();
}

- (void)initMutationEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable relatedNode:(DOMNode *)inRelatedNode prevValue:(NSString *)inPrevValue newValue:(NSString *)inNewValue attrName:(NSString *)inAttrName attrChange:(unsigned short)inAttrChange
{
    WebCore::JSMainThreadNullState state;
    IMPL->initMutationEvent(type, canBubble, cancelable, core(inRelatedNode), inPrevValue, inNewValue, inAttrName, inAttrChange);
}

@end

@implementation DOMMutationEvent (DOMMutationEventDeprecated)

- (void)initMutationEvent:(NSString *)type :(BOOL)canBubble :(BOOL)cancelable :(DOMNode *)inRelatedNode :(NSString *)inPrevValue :(NSString *)inNewValue :(NSString *)inAttrName :(unsigned short)inAttrChange
{
    [self initMutationEvent:type canBubble:canBubble cancelable:cancelable relatedNode:inRelatedNode prevValue:inPrevValue newValue:inNewValue attrName:inAttrName attrChange:inAttrChange];
}

@end

#undef IMPL
