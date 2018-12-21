/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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

#include "DOMWrapperWorld.h"
#include "EventNames.h"
#include "HTMLBodyElement.h"
#include "InspectorInstrumentation.h"
#include "JSEventListener.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "WebKitAnimationEvent.h"
#include "WebKitTransitionEvent.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace WebCore {

using namespace WTF;

bool EventTarget::isNode() const
{
    return false;
}

bool EventTarget::isPaymentRequest() const
{
    return false;
}

bool EventTarget::addEventListener(const AtomicString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    auto passive = options.passive;

    if (!passive.hasValue() && eventNames().isTouchScrollBlockingEventType(eventType)) {
        if (is<DOMWindow>(*this)) {
            auto& window = downcast<DOMWindow>(*this);
            if (auto* document = window.document())
                passive = document->settings().passiveTouchListenersAsDefaultOnDocument();
        } else if (is<Node>(*this)) {
            auto& node = downcast<Node>(*this);
            if (is<Document>(node) || node.document().documentElement() == &node || node.document().body() == &node)
                passive = node.document().settings().passiveTouchListenersAsDefaultOnDocument();
        }
    }

    bool listenerCreatedFromScript = listener->type() == EventListener::JSEventListenerType && !listener->wasCreatedFromMarkup();
    auto listenerRef = listener.copyRef();

    if (!ensureEventTargetData().eventListenerMap.add(eventType, WTFMove(listener), { options.capture, passive.valueOr(false), options.once }))
        return false;

    if (listenerCreatedFromScript)
        InspectorInstrumentation::didAddEventListener(*this, eventType, listenerRef.get(), options.capture);

    return true;
}

void EventTarget::addEventListenerForBindings(const AtomicString& eventType, RefPtr<EventListener>&& listener, AddEventListenerOptionsOrBoolean&& variant)
{
    if (!listener)
        return;

    auto visitor = WTF::makeVisitor([&](const AddEventListenerOptions& options) {
        addEventListener(eventType, listener.releaseNonNull(), options);
    }, [&](bool capture) {
        addEventListener(eventType, listener.releaseNonNull(), capture);
    });

    WTF::visit(visitor, variant);
}

void EventTarget::removeEventListenerForBindings(const AtomicString& eventType, RefPtr<EventListener>&& listener, ListenerOptionsOrBoolean&& variant)
{
    if (!listener)
        return;

    auto visitor = WTF::makeVisitor([&](const ListenerOptions& options) {
        removeEventListener(eventType, *listener, options);
    }, [&](bool capture) {
        removeEventListener(eventType, *listener, capture);
    });

    WTF::visit(visitor, variant);
}

bool EventTarget::removeEventListener(const AtomicString& eventType, EventListener& listener, const ListenerOptions& options)
{
    auto* data = eventTargetData();
    if (!data)
        return false;

    InspectorInstrumentation::willRemoveEventListener(*this, eventType, listener, options.capture);

    return data->eventListenerMap.remove(eventType, listener, options.capture);
}

bool EventTarget::setAttributeEventListener(const AtomicString& eventType, RefPtr<EventListener>&& listener, DOMWrapperWorld& isolatedWorld)
{
    auto* existingListener = attributeEventListener(eventType, isolatedWorld);
    if (!listener) {
        if (existingListener)
            removeEventListener(eventType, *existingListener, false);
        return false;
    }
    if (existingListener) {
        InspectorInstrumentation::willRemoveEventListener(*this, eventType, *existingListener, false);

        auto listenerPointer = listener.copyRef();
        eventTargetData()->eventListenerMap.replace(eventType, *existingListener, listener.releaseNonNull(), { });

        InspectorInstrumentation::didAddEventListener(*this, eventType, *listenerPointer, false);

        return true;
    }
    return addEventListener(eventType, listener.releaseNonNull());
}

EventListener* EventTarget::attributeEventListener(const AtomicString& eventType, DOMWrapperWorld& isolatedWorld)
{
    for (auto& eventListener : eventListeners(eventType)) {
        auto& listener = eventListener->callback();
        if (!listener.isAttribute())
            continue;

        auto& listenerWorld = downcast<JSEventListener>(listener).isolatedWorld();
        if (&listenerWorld == &isolatedWorld)
            return &listener;
    }

    return nullptr;
}

bool EventTarget::hasActiveEventListeners(const AtomicString& eventType) const
{
    auto* data = eventTargetData();
    return data && data->eventListenerMap.containsActive(eventType);
}

ExceptionOr<bool> EventTarget::dispatchEventForBindings(Event& event)
{
    event.setUntrusted();

    if (!event.isInitialized() || event.isBeingDispatched())
        return Exception { InvalidStateError };

    if (!scriptExecutionContext())
        return false;

    dispatchEvent(event);
    return event.legacyReturnValue();
}

void EventTarget::dispatchEvent(Event& event)
{
    // FIXME: We should always use EventDispatcher.
    ASSERT(event.isInitialized());
    ASSERT(!event.isBeingDispatched());

    event.setTarget(this);
    event.setCurrentTarget(this);
    event.setEventPhase(Event::AT_TARGET);
    event.resetBeforeDispatch();
    fireEventListeners(event, EventInvokePhase::Capturing);
    fireEventListeners(event, EventInvokePhase::Bubbling);
    event.resetAfterDispatch();
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

    // FIXME: This legacy name is not part of the specification (https://dom.spec.whatwg.org/#dispatching-events).
    if (event.type() == eventNames().wheelEvent)
        return eventNames().mousewheelEvent;

    return nullAtom();
}

// https://dom.spec.whatwg.org/#concept-event-listener-invoke
void EventTarget::fireEventListeners(Event& event, EventInvokePhase phase)
{
    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::isEventAllowedInMainThread());
    ASSERT(event.isInitialized());

    auto* data = eventTargetData();
    if (!data)
        return;

    SetForScope<bool> firingEventListenersScope(data->isFiringEventListeners, true);

    if (auto* listenersVector = data->eventListenerMap.find(event.type())) {
        innerInvokeEventListeners(event, *listenersVector, phase);
        return;
    }

    // Only fall back to legacy types for trusted events.
    if (!event.isTrusted())
        return;

    const AtomicString& legacyTypeName = legacyType(event);
    if (!legacyTypeName.isNull()) {
        if (auto* legacyListenersVector = data->eventListenerMap.find(legacyTypeName)) {
            AtomicString typeName = event.type();
            event.setType(legacyTypeName);
            innerInvokeEventListeners(event, *legacyListenersVector, phase);
            event.setType(typeName);
        }
    }
}

// Intentionally creates a copy of the listeners vector to avoid event listeners added after this point from being run.
// Note that removal still has an effect due to the removed field in RegisteredEventListener.
// https://dom.spec.whatwg.org/#concept-event-listener-inner-invoke
void EventTarget::innerInvokeEventListeners(Event& event, EventListenerVector listeners, EventInvokePhase phase)
{
    Ref<EventTarget> protectedThis(*this);
    ASSERT(!listeners.isEmpty());
    ASSERT(scriptExecutionContext());

    auto& context = *scriptExecutionContext();
    bool contextIsDocument = is<Document>(context);
    InspectorInstrumentationCookie willDispatchEventCookie;
    if (contextIsDocument)
        willDispatchEventCookie = InspectorInstrumentation::willDispatchEvent(downcast<Document>(context), event, true);

    for (auto& registeredListener : listeners) {
        if (UNLIKELY(registeredListener->wasRemoved()))
            continue;

        if (phase == EventInvokePhase::Capturing && !registeredListener->useCapture())
            continue;
        if (phase == EventInvokePhase::Bubbling && registeredListener->useCapture())
            continue;

        if (InspectorInstrumentation::isEventListenerDisabled(*this, event.type(), registeredListener->callback(), registeredListener->useCapture()))
            continue;

        // If stopImmediatePropagation has been called, we just break out immediately, without
        // handling any more events on this target.
        if (event.immediatePropagationStopped())
            break;

        // Do this before invocation to avoid reentrancy issues.
        if (registeredListener->isOnce())
            removeEventListener(event.type(), registeredListener->callback(), ListenerOptions(registeredListener->useCapture()));

        if (registeredListener->isPassive())
            event.setInPassiveListener(true);

        InspectorInstrumentation::willHandleEvent(context, event, *registeredListener);
        registeredListener->callback().handleEvent(context, event);
        InspectorInstrumentation::didHandleEvent(context);

        if (registeredListener->isPassive())
            event.setInPassiveListener(false);
    }

    if (contextIsDocument)
        InspectorInstrumentation::didDispatchEvent(willDispatchEventCookie);
}

const EventListenerVector& EventTarget::eventListeners(const AtomicString& eventType)
{
    auto* data = eventTargetData();
    auto* listenerVector = data ? data->eventListenerMap.find(eventType) : nullptr;
    static NeverDestroyed<EventListenerVector> emptyVector;
    return listenerVector ? *listenerVector : emptyVector.get();
}

void EventTarget::removeAllEventListeners()
{
    auto& threadData = threadGlobalData();
    RELEASE_ASSERT(!threadData.isInRemoveAllEventListeners());

    threadData.setIsInRemoveAllEventListeners(true);

    auto* data = eventTargetData();
    if (data)
        data->eventListenerMap.clear();

    threadData.setIsInRemoveAllEventListeners(false);
}

void EventTarget::visitJSEventListeners(JSC::SlotVisitor& visitor)
{
    EventTargetData* data = eventTargetDataConcurrently();
    if (!data)
        return;
    
    auto locker = holdLock(data->eventListenerMap.lock());
    EventListenerIterator iterator(&data->eventListenerMap);
    while (auto* listener = iterator.nextListener())
        listener->visitJSFunction(visitor);
}

} // namespace WebCore
