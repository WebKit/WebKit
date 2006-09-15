/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#import "DOMPrivate.h"
#import "Document.h"
#import "Event.h"
#import "MouseEvent.h"
#import "KeyboardEvent.h"
#import "MutationEvent.h"
#import "OverflowEvent.h"
#import "UIEvent.h"

ALLOW_DOM_CAST(Event)

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
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMEvent *)_eventWith:(WebCore::Event *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    Class wrapperClass = nil;
    if (impl->isWheelEvent())
        wrapperClass = [DOMWheelEvent class];        
    else if (impl->isMouseEvent())
        wrapperClass = [DOMMouseEvent class];
    else if (impl->isMutationEvent())
        wrapperClass = [DOMMutationEvent class];
    else if (impl->isKeyboardEvent())
        wrapperClass = [DOMKeyboardEvent class];
    else if (impl->isUIEvent())
        wrapperClass = [DOMUIEvent class];
    else if (impl->isOverflowEvent())
        wrapperClass = [DOMOverflowEvent class];
    else
        wrapperClass = [DOMEvent class];

    return [[[wrapperClass alloc] _initWithEvent:impl] autorelease];
}

@end


//------------------------------------------------------------------------------------------
// DOMKeyboardEvent

@implementation DOMKeyboardEvent (NonStandardAdditions)

- (BOOL)getModifierState:(NSString *)keyIdentifierArg
{
    if ([keyIdentifierArg isEqualToString:@"Control"] && [self ctrlKey])
        return YES;
    if ([keyIdentifierArg isEqualToString:@"Shift"] && [self shiftKey])
        return YES;
    if ([keyIdentifierArg isEqualToString:@"Alt"] && [self altKey])
        return YES;
    if ([keyIdentifierArg isEqualToString:@"Meta"] && [self metaKey])
        return YES;
    return NO;
}

@end
