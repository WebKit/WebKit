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

#import "DOMEventInternal.h"

#import "DOMKeyboardEvent.h"
#import "DOMMouseEvent.h"
#import "DOMMutationEvent.h"
#import "DOMOverflowEvent.h"
#import "DOMProgressEvent.h"
#import "DOMTextEvent.h"
#import "DOMWheelEvent.h"
#import <WebCore/Event.h>
#import <WebCore/EventNames.h>

using WebCore::eventNames;

Class kitClass(WebCore::Event* impl)
{
    switch (impl->eventInterface()) {
    case WebCore::KeyboardEventInterfaceType:
        return [DOMKeyboardEvent class];
    case WebCore::MouseEventInterfaceType:
        return [DOMMouseEvent class];
    case WebCore::MutationEventInterfaceType:
        return [DOMMutationEvent class];
    case WebCore::OverflowEventInterfaceType:
        return [DOMOverflowEvent class];
    case WebCore::ProgressEventInterfaceType:
    case WebCore::XMLHttpRequestProgressEventInterfaceType:
        return [DOMProgressEvent class];
    case WebCore::TextEventInterfaceType:
        return [DOMTextEvent class];
    case WebCore::WheelEventInterfaceType:
        return [DOMWheelEvent class];
    default:
        if (impl->isUIEvent())
            return [DOMUIEvent class];

        return [DOMEvent class];
    }
}
