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
#import "DOMTextEventInternal.h"

#import "DOMAbstractViewInternal.h"
#import "DOMEventInternal.h"
#import "DOMNodeInternal.h"
#import "DOMWindow.h"
#import "ExceptionHandlers.h"
#import "JSMainThreadExecState.h"
#import "TextEvent.h"
#import "ThreadCheck.h"
#import "URL.h"
#import "WebScriptObjectPrivate.h"
#import <wtf/GetPtr.h>

#define IMPL static_cast<WebCore::TextEvent*>(reinterpret_cast<WebCore::Event*>(_internal))

@implementation DOMTextEvent

- (NSString *)data
{
    WebCore::JSMainThreadNullState state;
    return IMPL->data();
}

- (void)initTextEvent:(NSString *)typeArg canBubbleArg:(BOOL)canBubbleArg cancelableArg:(BOOL)cancelableArg viewArg:(DOMAbstractView *)viewArg dataArg:(NSString *)dataArg
{
    WebCore::JSMainThreadNullState state;
    IMPL->initTextEvent(typeArg, canBubbleArg, cancelableArg, core(viewArg), dataArg);
}

@end

WebCore::TextEvent* core(DOMTextEvent *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::TextEvent*>(wrapper->_internal) : 0;
}

DOMTextEvent *kit(WebCore::TextEvent* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMTextEvent*>(kit(static_cast<WebCore::Event*>(value)));
}
