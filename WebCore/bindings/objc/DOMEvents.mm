/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source exceptionCode must retain the above copyright
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

#include "config.h"
#import "DOMEvents.h"

#import "DOMEventsInternal.h"
#import "DOMViewsInternal.h"
#import "DOMInternal.h"
#import <kxmlcore/Assertions.h>

#import "DocumentImpl.h"
#import "dom2_eventsimpl.h"
#import "dom2_viewsimpl.h"

using DOM::EventImpl;
using DOM::MouseEventImpl;
using DOM::MutationEventImpl;
using DOM::UIEventImpl;

ALLOW_DOM_CAST(EventImpl)

@implementation DOMEvent

- (NSString *)type
{
    return [self _eventImpl]->type();
}

- (id <DOMEventTarget>)target
{
    return [DOMNode _nodeWithImpl:[self _eventImpl]->target()];
}

- (id <DOMEventTarget>)currentTarget
{
    return [DOMNode _nodeWithImpl:[self _eventImpl]->currentTarget()];
}

- (unsigned short)eventPhase
{
    return [self _eventImpl]->eventPhase();
}

- (BOOL)bubbles
{
    return [self _eventImpl]->bubbles();
}

- (BOOL)cancelable
{
    return [self _eventImpl]->cancelable();
}

- (DOMTimeStamp)timeStamp
{
    return [self _eventImpl]->timeStamp();
}

- (void)stopPropagation
{
    [self _eventImpl]->stopPropagation();
}

- (void)preventDefault
{
    [self _eventImpl]->preventDefault();
}

- (void)initEvent:(NSString *)eventTypeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg
{
    [self _eventImpl]->initEvent(eventTypeArg, canBubbleArg, cancelableArg);
}

@end

@implementation DOMEvent (WebCoreInternal)

- (EventImpl *)_eventImpl
{
    return DOM_cast<EventImpl *>(_internal);
}

- (id)_initWithEventImpl:(EventImpl *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMEvent *)_eventWithImpl:(EventImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    Class wrapperClass = nil;
    if (impl->isMouseEvent()) {
        wrapperClass = [DOMMouseEvent class];
    } else if (impl->isMutationEvent()) {
        wrapperClass = [DOMMutationEvent class];
    } else if (impl->isUIEvent()) {
        wrapperClass = [DOMUIEvent class];
    } else {
        wrapperClass = [DOMEvent class];
    }
    return [[[wrapperClass alloc] _initWithEventImpl:impl] autorelease];
}

@end

@implementation DOMMouseEvent

- (MouseEventImpl *)_mouseEventImpl
{
    return static_cast<MouseEventImpl *>(DOM_cast<EventImpl *>(_internal));
}

- (int)screenX
{
    return [self _mouseEventImpl]->screenX();
}

- (int)screenY
{
    return [self _mouseEventImpl]->screenY();
}

- (int)clientX
{
    return [self _mouseEventImpl]->clientX();
}

- (int)clientY
{
    return [self _mouseEventImpl]->clientY();
}

- (BOOL)ctrlKey
{
    return [self _mouseEventImpl]->ctrlKey();
}

- (BOOL)shiftKey
{
    return [self _mouseEventImpl]->shiftKey();
}

- (BOOL)altKey
{
    return [self _mouseEventImpl]->altKey();
}

- (BOOL)metaKey
{
    return [self _mouseEventImpl]->metaKey();
}

- (unsigned short)button
{
    return [self _mouseEventImpl]->button();
}

- (id <DOMEventTarget>)relatedTarget
{
    return [DOMNode _nodeWithImpl:[self _mouseEventImpl]->relatedTarget()];
}

- (void)initMouseEvent:(NSString *)typeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg :(DOMAbstractView *)viewArg :(int)detailArg :(int)screenXArg :(int)screenYArg :(int)clientX :(int)clientY :(BOOL)ctrlKeyArg :(BOOL)altKeyArg :(BOOL)shiftKeyArg :(BOOL)metaKeyArg :(unsigned short)buttonArg :(id <DOMEventTarget>)relatedTargetArg
{
    DOMNode *relatedTarget = relatedTargetArg;
    [self _mouseEventImpl]->initMouseEvent(typeArg, canBubbleArg, cancelableArg,
        [viewArg _abstractViewImpl], detailArg, screenXArg, screenYArg, clientX, clientY,
        shiftKeyArg, ctrlKeyArg, altKeyArg, metaKeyArg, buttonArg,
        [relatedTarget _nodeImpl]);
}

@end

@implementation DOMMutationEvent

- (MutationEventImpl *)_mutationEventImpl
{
    return static_cast<MutationEventImpl *>(DOM_cast<EventImpl *>(_internal));
}

- (DOMNode *)relatedNode
{
    return [DOMNode _nodeWithImpl:[self _mutationEventImpl]->relatedNode()];
}

- (NSString *)prevValue
{
    return [self _mutationEventImpl]->prevValue();
}

- (NSString *)newValue
{
    return [self _mutationEventImpl]->newValue();
}

- (NSString *)attrName
{
    return [self _mutationEventImpl]->attrName();
}

- (unsigned short)attrChange
{
    return [self _mutationEventImpl]->attrChange();
}

- (void)initMutationEvent:(NSString *)typeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg :(DOMNode *)relatedNodeArg :(NSString *)prevValueArg :(NSString *)newValueArg :(NSString *)attrNameArg :(unsigned short)attrChangeArg
{
    [self _mutationEventImpl]->initMutationEvent(typeArg, canBubbleArg, cancelableArg,
        [relatedNodeArg _nodeImpl], prevValueArg, newValueArg, attrNameArg, attrChangeArg);
}

@end

@implementation DOMUIEvent

- (UIEventImpl *)_UIEventImpl
{
    return static_cast<UIEventImpl *>(DOM_cast<EventImpl *>(_internal));
}

- (DOMAbstractView *)view
{
    return [DOMAbstractView _abstractViewWithImpl:[self _UIEventImpl]->view()];
}

- (int)detail
{
    return [self _UIEventImpl]->detail();
}

- (void)initUIEvent:(NSString *)typeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg :(DOMAbstractView *)viewArg :(int)detailArg
{
    [self _UIEventImpl]->initUIEvent(typeArg, canBubbleArg, cancelableArg, [viewArg _abstractViewImpl], detailArg);
}

@end

@implementation DOMDocument (DOMDocumentEvent)

- (DOMEvent *)createEvent:(NSString *)eventType
{
    int exceptionCode = 0;
    RefPtr<EventImpl> event = [self _documentImpl]->createEvent(eventType, exceptionCode);
    raiseOnDOMError(exceptionCode);
    return [DOMEvent _eventWithImpl:event.get()];
}

@end
