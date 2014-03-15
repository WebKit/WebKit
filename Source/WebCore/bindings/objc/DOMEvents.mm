/*
 * Copyright (C) 2004, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Jonas Witt <jonas.witt@gmail.com>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source ec must retain the above copyright
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
#import "DOMEventInternal.h"

#import "DOMBeforeLoadEvent.h"
#import "DOMKeyboardEvent.h"
#import "DOMMessageEvent.h"
#import "DOMMouseEvent.h"
#import "DOMMutationEvent.h"
#import "DOMOverflowEvent.h"
#import "DOMProgressEvent.h"
#import "DOMTextEvent.h"
#import "DOMWheelEvent.h"
#import "Event.h"
#import "EventNames.h"

#if ENABLE(TOUCH_EVENTS)
#import "DOMTouchEvent.h"
#endif

#if ENABLE(IOS_GESTURE_EVENTS)
#import "DOMGestureEvent.h"
#endif

using WebCore::eventNames;

Class kitClass(WebCore::Event* impl)
{
    if (impl->isUIEvent()) {
        if (impl->isKeyboardEvent())
            return [DOMKeyboardEvent class];
        if (impl->isMouseEvent())
            return [DOMMouseEvent class];

        WebCore::EventInterface desiredInterface = impl->eventInterface();
        if (desiredInterface == WebCore::TextEventInterfaceType)
            return [DOMTextEvent class];
        if (desiredInterface == WebCore::WheelEventInterfaceType)
            return [DOMWheelEvent class];        
#if PLATFORM(IOS) && ENABLE(TOUCH_EVENTS)
        if (desiredInterface == WebCore::TouchEventInterfaceType) 
            return [DOMTouchEvent class];
#endif
#if ENABLE(IOS_GESTURE_EVENTS)
        if (desiredInterface == WebCore::GestureEventInterfaceType)
            return [DOMGestureEvent class];
#endif
        return [DOMUIEvent class];
    }

    WebCore::EventInterface desiredInterface = impl->eventInterface();
    if (desiredInterface == WebCore::MutationEventInterfaceType)
        return [DOMMutationEvent class];
    if (desiredInterface == WebCore::OverflowEventInterfaceType)
        return [DOMOverflowEvent class];
    if (desiredInterface == WebCore::MessageEventInterfaceType)
        return [DOMMessageEvent class];
    if (desiredInterface == WebCore::ProgressEventInterfaceType || desiredInterface == WebCore::XMLHttpRequestProgressEventInterfaceType)
        return [DOMProgressEvent class];
    if (desiredInterface == WebCore::BeforeLoadEventInterfaceType)
        return [DOMBeforeLoadEvent class];
    return [DOMEvent class];
}
