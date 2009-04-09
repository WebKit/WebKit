/*
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "DOMEvents.h"

#import "DOMInternal.h"
#import "DOMMessageEvent.h"
#import "DOMPrivate.h"
#import "DOMProgressEvent.h"
#import "Event.h"
#import "KeyboardEvent.h"
#import "MessageEvent.h"
#import "MouseEvent.h"
#import "MutationEvent.h"
#import "OverflowEvent.h"
#import "ProgressEvent.h"
#import "UIEvent.h"

#if ENABLE(SVG_DOM_OBJC_BINDINGS)
#import "DOMSVGZoomEvent.h"
#import "SVGZoomEvent.h"
#endif

//------------------------------------------------------------------------------------------
// DOMEvent

@implementation DOMEvent (WebCoreInternal)

- (WebCore::Event *)_event
{
    return reinterpret_cast<WebCore::Event*>(_internal);
}

- (id)_initWithEvent:(WebCore::Event *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    WebCore::addDOMWrapper(self, impl);
    return self;
}

+ (DOMEvent *)_wrapEvent:(WebCore::Event *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = WebCore::getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];

    Class wrapperClass = nil;
    if (impl->isUIEvent()) {
        if (impl->isKeyboardEvent())
            wrapperClass = [DOMKeyboardEvent class];
        else if (impl->isTextEvent())
            wrapperClass = [DOMTextEvent class];
        else if (impl->isMouseEvent())
            wrapperClass = [DOMMouseEvent class];
        else if (impl->isWheelEvent())
            wrapperClass = [DOMWheelEvent class];        
#if ENABLE(SVG_DOM_OBJC_BINDINGS)
        else if (impl->isSVGZoomEvent())
            wrapperClass = [DOMSVGZoomEvent class];
#endif
        else
            wrapperClass = [DOMUIEvent class];
    } else if (impl->isMutationEvent())
        wrapperClass = [DOMMutationEvent class];
    else if (impl->isOverflowEvent())
        wrapperClass = [DOMOverflowEvent class];
    else if (impl->isMessageEvent())
        wrapperClass = [DOMMessageEvent class];
    else if (impl->isProgressEvent())
        wrapperClass = [DOMProgressEvent class];
    else
        wrapperClass = [DOMEvent class];

    return [[[wrapperClass alloc] _initWithEvent:impl] autorelease];
}

@end
