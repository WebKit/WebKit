/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
 *
 */

#include "config.h"
#include "EventTarget.h"

#include "ExceptionCode.h"
#include "InspectorInstrumentation.h"
#include "NoEventDispatchAssertion.h"
#include "ScriptController.h"
#include "WebKitAnimationEvent.h"
#include "WebKitTransitionEvent.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

using namespace WTF;

namespace WebCore {

EventTargetData::EventTargetData()
{
}

EventTargetData::~EventTargetData()
{
}

EventTarget::~EventTarget()
{
}

Node* EventTarget::toNode()
{
    return 0;
}

DOMWindow* EventTarget::toDOMWindow()
{
    return 0;
}

bool EventTarget::isMessagePort() const
{
    return false;
}

bool EventTarget::addEventListener(const AtomicString& eventType, RefPtr<EventListener>&& listener, bool useCapture)
{
    return ensureEventTargetData().eventListenerMap.add(eventType, WTFMove(listener), useCapture);
}

bool EventTarget::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    EventTargetData* d = eventTargetData();
    if (!d)
        return false;

    size_t indexOfRemovedListener;

    if (!d->eventListenerMap.remove(eventType, listener, useCapture, indexOfRemovedListener))
        return false;

    // Notify firing events planning to invoke the listener at 'index' that
    // they have one less listener to invoke.
    if (!d->firingEventIterators)
        return true;
    for (auto& firingIterator : *d->firingEventIterators) {
        if (eventType != firingIterator.eventType)
            continue;

        if (indexOfRemovedListener >= firingIterator.size)
            continue;

        --firingIterator.size;
        if (indexOfRemovedListener <= firingIterator.iterator)
            --firingIterator.iterator;
    }

    return true;
}

bool EventTarget::setAttributeEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener)
{
    clearAttributeEventListener(eventType);
    if (!listener)
        return false;
    return addEventListener(eventType, listener, false);
}

EventListener* EventTarget::getAttributeEventListener(const AtomicString& eventType)
{
    for (auto& eventListener : getEventListeners(eventType)) {
        if (eventListener.listener->isAttribute())
            return eventListener.listener.get();
    }
    return 0;
}

bool EventTarget::clearAttributeEventListener(const AtomicString& eventType)
{
    EventListener* listener = getAttributeEventListener(eventType);
    if (!listener)
        return false;
    return removeEventListener(eventType, listener, false);
}

bool EventTarget::dispatchEventForBindings(Event* event, ExceptionCode& ec)
{
    if (!event) {
        ec = TypeError;
        return false;
    }

    event->setUntrusted();

    if (!event->isInitialized() || event->isBeingDispatched()) {
        ec = INVALID_STATE_ERR;
        return false;
    }

    if (!scriptExecutionContext())
        return false;

    return dispatchEvent(*event);
}

bool EventTarget::dispatchEvent(Event& event)
{
    ASSERT(event.isInitialized());
    ASSERT(!event.isBeingDispatched());

    event.setTarget(this);
    event.setCurrentTarget(this);
    event.setEventPhase(Event::AT_TARGET);
    bool defaultPrevented = fireEventListeners(event);
    event.setEventPhase(0);
    return defaultPrevented;
}

void EventTarget::uncaughtExceptionInEventHandler()
{
}

static const AtomicString& legacyType(const Event& event)
{
    if (event.type() == eventNames().animationendEvent)
        return eventNames().webkitAnimationEndEvent;

    if (event.type() == eventNames().animationstartEvent)
        return eventNames().webkitAnimationStartEvent;

    if (event.type() == eventNames().animationiterationEvent)
        return eventNames().webkitAnimationIterationEvent;

    if (event.type() == eventNames().transitionendEvent)
        return eventNames().webkitTransitionEndEvent;

    if (event.type() == eventNames().wheelEvent)
        return eventNames().mousewheelEvent;

    return emptyAtom;
}

bool EventTarget::fireEventListeners(Event& event)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!NoEventDispatchAssertion::isEventDispatchForbidden());
    ASSERT(event.isInitialized());

    EventTargetData* d = eventTargetData();
    if (!d)
        return true;

    EventListenerVector* legacyListenersVector = nullptr;
    const AtomicString& legacyTypeName = legacyType(event);
    if (!legacyTypeName.isEmpty())
        legacyListenersVector = d->eventListenerMap.find(legacyTypeName);

    EventListenerVector* listenersVector = d->eventListenerMap.find(event.type());

    if (listenersVector)
        fireEventListeners(event, d, *listenersVector);
    else if (legacyListenersVector) {
        AtomicString typeName = event.type();
        event.setType(legacyTypeName);
        fireEventListeners(event, d, *legacyListenersVector);
        event.setType(typeName);
    }

    return !event.defaultPrevented();
}
        
void EventTarget::fireEventListeners(Event& event, EventTargetData* d, EventListenerVector& entry)
{
    Ref<EventTarget> protect(*this);

    // Fire all listeners registered for this event. Don't fire listeners removed during event dispatch.
    // Also, don't fire event listeners added during event dispatch. Conveniently, all new event listeners will be added
    // after or at index |size|, so iterating up to (but not including) |size| naturally excludes new event listeners.

    size_t i = 0;
    size_t size = entry.size();
    if (!d->firingEventIterators)
        d->firingEventIterators = std::make_unique<FiringEventIteratorVector>();
    d->firingEventIterators->append(FiringEventIterator(event.type(), i, size));

    ScriptExecutionContext* context = scriptExecutionContext();
    Document* document = nullptr;
    InspectorInstrumentationCookie willDispatchEventCookie;
    if (is<Document>(context)) {
        document = downcast<Document>(context);
        willDispatchEventCookie = InspectorInstrumentation::willDispatchEvent(*document, event, size > 0);
    }

    for (; i < size; ++i) {
        RegisteredEventListener& registeredListener = entry[i];
        if (event.eventPhase() == Event::CAPTURING_PHASE && !registeredListener.useCapture)
            continue;
        if (event.eventPhase() == Event::BUBBLING_PHASE && registeredListener.useCapture)
            continue;

        // If stopImmediatePropagation has been called, we just break out immediately, without
        // handling any more events on this target.
        if (event.immediatePropagationStopped())
            break;

        InspectorInstrumentationCookie cookie = InspectorInstrumentation::willHandleEvent(context, event);
        // To match Mozilla, the AT_TARGET phase fires both capturing and bubbling
        // event listeners, even though that violates some versions of the DOM spec.
        registeredListener.listener->handleEvent(context, &event);
        InspectorInstrumentation::didHandleEvent(cookie);
    }
    d->firingEventIterators->removeLast();

    if (document)
        InspectorInstrumentation::didDispatchEvent(willDispatchEventCookie);
}

const EventListenerVector& EventTarget::getEventListeners(const AtomicString& eventType)
{
    auto* data = eventTargetData();
    auto* listenerVector = data ? data->eventListenerMap.find(eventType) : nullptr;
    static NeverDestroyed<EventListenerVector> emptyVector;
    return listenerVector ? *listenerVector : emptyVector.get();
}

void EventTarget::removeAllEventListeners()
{
    EventTargetData* d = eventTargetData();
    if (!d)
        return;
    d->eventListenerMap.clear();

    // Notify firing events planning to invoke the listener at 'index' that
    // they have one less listener to invoke.
    if (d->firingEventIterators) {
        for (auto& firingEventIterator : *d->firingEventIterators) {
            firingEventIterator.iterator = 0;
            firingEventIterator.size = 0;
        }
    }
}

} // namespace WebCore
