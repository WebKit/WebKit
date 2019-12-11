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

#import "DOMKeyboardEvent.h"

#import "DOMAbstractViewInternal.h"
#import "DOMEventInternal.h"
#import "DOMNodeInternal.h"
#import <WebCore/DOMWindow.h>
#import "ExceptionHandlers.h"
#import <WebCore/JSExecState.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::KeyboardEvent*>(reinterpret_cast<WebCore::Event*>(_internal))

@implementation DOMKeyboardEvent

- (NSString *)keyIdentifier
{
    WebCore::JSMainThreadNullState state;
    return IMPL->keyIdentifier();
}

- (unsigned)location
{
    WebCore::JSMainThreadNullState state;
    return IMPL->location();
}

- (unsigned)keyLocation
{
    WebCore::JSMainThreadNullState state;
    return IMPL->location();
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

- (BOOL)altGraphKey
{
    WebCore::JSMainThreadNullState state;
    return IMPL->altGraphKey();
}

- (int)keyCode
{
    WebCore::JSMainThreadNullState state;
    return IMPL->keyCode();
}

- (int)charCode
{
    WebCore::JSMainThreadNullState state;
    return IMPL->charCode();
}

- (BOOL)getModifierState:(NSString *)keyIdentifierArg
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getModifierState(keyIdentifierArg);
}

- (void)initKeyboardEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view keyIdentifier:(NSString *)inKeyIdentifier location:(unsigned)inLocation ctrlKey:(BOOL)inCtrlKey altKey:(BOOL)inAltKey shiftKey:(BOOL)inShiftKey metaKey:(BOOL)inMetaKey altGraphKey:(BOOL)inAltGraphKey
{
    WebCore::JSMainThreadNullState state;
    IMPL->initKeyboardEvent(type, canBubble, cancelable, toWindowProxy(view), inKeyIdentifier, inLocation, inCtrlKey, inAltKey, inShiftKey, inMetaKey, inAltGraphKey);
}

- (void)initKeyboardEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view keyIdentifier:(NSString *)inKeyIdentifier location:(unsigned)inLocation ctrlKey:(BOOL)inCtrlKey altKey:(BOOL)inAltKey shiftKey:(BOOL)inShiftKey metaKey:(BOOL)inMetaKey
{
    WebCore::JSMainThreadNullState state;
    IMPL->initKeyboardEvent(type, canBubble, cancelable, toWindowProxy(view), inKeyIdentifier, inLocation, inCtrlKey, inAltKey, inShiftKey, inMetaKey);
}

- (void)initKeyboardEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view keyIdentifier:(NSString *)inKeyIdentifier keyLocation:(unsigned)inKeyLocation ctrlKey:(BOOL)inCtrlKey altKey:(BOOL)inAltKey shiftKey:(BOOL)inShiftKey metaKey:(BOOL)inMetaKey altGraphKey:(BOOL)inAltGraphKey
{
    WebCore::JSMainThreadNullState state;
    IMPL->initKeyboardEvent(type, canBubble, cancelable, toWindowProxy(view), inKeyIdentifier, inKeyLocation, inCtrlKey, inAltKey, inShiftKey, inMetaKey, inAltGraphKey);
}

- (void)initKeyboardEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view keyIdentifier:(NSString *)inKeyIdentifier keyLocation:(unsigned)inKeyLocation ctrlKey:(BOOL)inCtrlKey altKey:(BOOL)inAltKey shiftKey:(BOOL)inShiftKey metaKey:(BOOL)inMetaKey
{
    WebCore::JSMainThreadNullState state;
    IMPL->initKeyboardEvent(type, canBubble, cancelable, toWindowProxy(view), inKeyIdentifier, inKeyLocation, inCtrlKey, inAltKey, inShiftKey, inMetaKey);
}

@end

#undef IMPL
