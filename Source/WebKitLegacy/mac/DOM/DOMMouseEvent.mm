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

#import "DOMMouseEvent.h"

#import "DOMAbstractViewInternal.h"
#import "DOMEventInternal.h"
#import "DOMEventTarget.h"
#import "DOMNode.h"
#import "DOMNodeInternal.h"
#import <WebCore/DOMWindow.h>
#import "ExceptionHandlers.h"
#import <WebCore/JSExecState.h>
#import <WebCore/MouseEvent.h>
#import <WebCore/Node.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::MouseEvent*>(reinterpret_cast<WebCore::Event*>(_internal))

@implementation DOMMouseEvent

- (int)screenX
{
    WebCore::JSMainThreadNullState state;
    return IMPL->screenX();
}

- (int)screenY
{
    WebCore::JSMainThreadNullState state;
    return IMPL->screenY();
}

- (int)clientX
{
    WebCore::JSMainThreadNullState state;
    return IMPL->clientX();
}

- (int)clientY
{
    WebCore::JSMainThreadNullState state;
    return IMPL->clientY();
}

- (BOOL)ctrlKey
{
    WebCore::JSMainThreadNullState state;
    return IMPL->ctrlKey();
}

- (BOOL)shiftKey
{
    WebCore::JSMainThreadNullState state;
    return IMPL->shiftKey();
}

- (BOOL)altKey
{
    WebCore::JSMainThreadNullState state;
    return IMPL->altKey();
}

- (BOOL)metaKey
{
    WebCore::JSMainThreadNullState state;
    return IMPL->metaKey();
}

- (unsigned short)button
{
    WebCore::JSMainThreadNullState state;
    return IMPL->button();
}

- (id <DOMEventTarget>)relatedTarget
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->relatedTarget()));
}

- (int)offsetX
{
    WebCore::JSMainThreadNullState state;
    return IMPL->offsetX();
}

- (int)offsetY
{
    WebCore::JSMainThreadNullState state;
    return IMPL->offsetY();
}

- (int)x
{
    WebCore::JSMainThreadNullState state;
    return IMPL->x();
}

- (int)y
{
    WebCore::JSMainThreadNullState state;
    return IMPL->y();
}

- (DOMNode *)fromElement
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->fromElement()));
}

- (DOMNode *)toElement
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->toElement()));
}

- (void)initMouseEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view detail:(int)detail screenX:(int)inScreenX screenY:(int)inScreenY clientX:(int)inClientX clientY:(int)inClientY ctrlKey:(BOOL)inCtrlKey altKey:(BOOL)inAltKey shiftKey:(BOOL)inShiftKey metaKey:(BOOL)inMetaKey button:(unsigned short)inButton relatedTarget:(id <DOMEventTarget>)inRelatedTarget
{
    WebCore::JSMainThreadNullState state;
    DOMNode* inRelatedTargetObjC = inRelatedTarget;
    WebCore::Node* inRelatedTargetNode = core(inRelatedTargetObjC);
    IMPL->initMouseEvent(type, canBubble, cancelable, toWindowProxy(view), detail, inScreenX, inScreenY, inClientX, inClientY, inCtrlKey, inAltKey, inShiftKey, inMetaKey, inButton, inRelatedTargetNode);
}

@end

@implementation DOMMouseEvent (DOMMouseEventDeprecated)

- (void)initMouseEvent:(NSString *)type :(BOOL)canBubble :(BOOL)cancelable :(DOMAbstractView *)view :(int)detail :(int)inScreenX :(int)inScreenY :(int)inClientX :(int)inClientY :(BOOL)inCtrlKey :(BOOL)inAltKey :(BOOL)inShiftKey :(BOOL)inMetaKey :(unsigned short)inButton :(id <DOMEventTarget>)inRelatedTarget
{
    [self initMouseEvent:type canBubble:canBubble cancelable:cancelable view:view detail:detail screenX:inScreenX screenY:inScreenY clientX:inClientX clientY:inClientY ctrlKey:inCtrlKey altKey:inAltKey shiftKey:inShiftKey metaKey:inMetaKey button:inButton relatedTarget:inRelatedTarget];
}

@end
