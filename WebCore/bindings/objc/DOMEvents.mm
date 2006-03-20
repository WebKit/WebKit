/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#import "DOMEventsInternal.h"
#import "DOMInternal.h"
#import "DOMViewsInternal.h"
#import "Document.h"
#import "dom2_eventsimpl.h"
#import "AbstractView.h"
#import <kxmlcore/Assertions.h>

using WebCore::Event;
using WebCore::ExceptionCode;
using WebCore::MouseEvent;
using WebCore::MutationEvent;
using WebCore::UIEvent;

ALLOW_DOM_CAST(Event)

@implementation DOMEvent

- (NSString *)type
{
    return [self _event]->type();
}

- (id <DOMEventTarget>)target
{
    return [DOMNode _nodeWith:[self _event]->target()];
}

- (id <DOMEventTarget>)currentTarget
{
    return [DOMNode _nodeWith:[self _event]->currentTarget()];
}

- (unsigned short)eventPhase
{
    return [self _event]->eventPhase();
}

- (BOOL)bubbles
{
    return [self _event]->bubbles();
}

- (BOOL)cancelable
{
    return [self _event]->cancelable();
}

- (DOMTimeStamp)timeStamp
{
    return [self _event]->timeStamp();
}

- (void)stopPropagation
{
    [self _event]->stopPropagation();
}

- (void)preventDefault
{
    [self _event]->preventDefault();
}

- (void)initEvent:(NSString *)eventTypeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg
{
    [self _event]->initEvent(eventTypeArg, canBubbleArg, cancelableArg);
}

@end

@implementation DOMEvent (WebCoreInternal)

- (Event *)_event
{
    return DOM_cast<Event *>(_internal);
}

- (id)_initWithEvent:(Event *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMEvent *)_eventWith:(Event *)impl
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
    return [[[wrapperClass alloc] _initWithEvent:impl] autorelease];
}

@end

@implementation DOMMouseEvent

- (MouseEvent *)_mouseEvent
{
    return static_cast<MouseEvent *>(DOM_cast<Event *>(_internal));
}

- (int)screenX
{
    return [self _mouseEvent]->screenX();
}

- (int)screenY
{
    return [self _mouseEvent]->screenY();
}

- (int)clientX
{
    return [self _mouseEvent]->clientX();
}

- (int)clientY
{
    return [self _mouseEvent]->clientY();
}

- (BOOL)ctrlKey
{
    return [self _mouseEvent]->ctrlKey();
}

- (BOOL)shiftKey
{
    return [self _mouseEvent]->shiftKey();
}

- (BOOL)altKey
{
    return [self _mouseEvent]->altKey();
}

- (BOOL)metaKey
{
    return [self _mouseEvent]->metaKey();
}

- (unsigned short)button
{
    return [self _mouseEvent]->button();
}

- (id <DOMEventTarget>)relatedTarget
{
    return [DOMNode _nodeWith:[self _mouseEvent]->relatedTarget()];
}

- (void)initMouseEvent:(NSString *)typeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg :(DOMAbstractView *)viewArg :(int)detailArg :(int)screenXArg :(int)screenYArg :(int)clientX :(int)clientY :(BOOL)ctrlKeyArg :(BOOL)altKeyArg :(BOOL)shiftKeyArg :(BOOL)metaKeyArg :(unsigned short)buttonArg :(id <DOMEventTarget>)relatedTargetArg
{
    DOMNode *relatedTarget = relatedTargetArg;
    [self _mouseEvent]->initMouseEvent(typeArg, canBubbleArg, cancelableArg,
        [viewArg _abstractView], detailArg, screenXArg, screenYArg, clientX, clientY,
        shiftKeyArg, ctrlKeyArg, altKeyArg, metaKeyArg, buttonArg,
        [relatedTarget _node]);
}

@end

@implementation DOMMutationEvent

- (MutationEvent *)_mutationEvent
{
    return static_cast<MutationEvent *>(DOM_cast<Event *>(_internal));
}

- (DOMNode *)relatedNode
{
    return [DOMNode _nodeWith:[self _mutationEvent]->relatedNode()];
}

- (NSString *)prevValue
{
    return [self _mutationEvent]->prevValue();
}

- (NSString *)newValue
{
    return [self _mutationEvent]->newValue();
}

- (NSString *)attrName
{
    return [self _mutationEvent]->attrName();
}

- (unsigned short)attrChange
{
    return [self _mutationEvent]->attrChange();
}

- (void)initMutationEvent:(NSString *)typeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg :(DOMNode *)relatedNodeArg :(NSString *)prevValueArg :(NSString *)newValueArg :(NSString *)attrNameArg :(unsigned short)attrChangeArg
{
    [self _mutationEvent]->initMutationEvent(typeArg, canBubbleArg, cancelableArg,
        [relatedNodeArg _node], prevValueArg, newValueArg, attrNameArg, attrChangeArg);
}

@end

@implementation DOMUIEvent

- (UIEvent *)_UIEvent
{
    return static_cast<UIEvent *>(DOM_cast<Event *>(_internal));
}

- (DOMAbstractView *)view
{
    return [DOMAbstractView _abstractViewWith:[self _UIEvent]->view()];
}

- (int)detail
{
    return [self _UIEvent]->detail();
}

- (void)initUIEvent:(NSString *)typeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg :(DOMAbstractView *)viewArg :(int)detailArg
{
    [self _UIEvent]->initUIEvent(typeArg, canBubbleArg, cancelableArg, [viewArg _abstractView], detailArg);
}

@end

@implementation DOMDocument (DOMDocumentEvent)

- (DOMEvent *)createEvent:(NSString *)eventType
{
    ExceptionCode ec = 0;
    RefPtr<Event> event = [self _document]->createEvent(eventType, ec);
    raiseOnDOMError(ec);
    return [DOMEvent _eventWith:event.get()];
}

@end
