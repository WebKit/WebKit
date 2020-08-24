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
#include "EventTargetConcrete.h"
#include "HTMLBodyElement.h"
#include "HTMLHtmlElement.h"
#include "InspectorInstrumentation.h"
#include "JSEventListener.h"
#include "JSLazyEventListener.h"
#include "Logging.h"
#include "Quirks.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "WebKitAnimationEvent.h"
#include "WebKitTransitionEvent.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(EventTarget);
WTF_MAKE_ISO_ALLOCATED_IMPL(EventTargetWithInlineData);

Ref<EventTarget> EventTarget::create(ScriptExecutionContext& context)
{
    return EventTargetConcrete::create(context);
}

bool EventTarget::isNode() const
{
    return false;
}

bool EventTarget::isPaymentRequest() const
{
    return false;
}

bool EventTarget::addEventListener(const AtomString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
#if ASSERT_ENABLED
    listener->checkValidityForEventTarget(*this);
#endif

    auto passive = options.passive;

    if (!passive.hasValue() && Quirks::shouldMakeEventListenerPassive(*this, eventType, listener.get()))
        passive = true;

    bool listenerCreatedFromScript = listener->type() == EventListener::JSEventListenerType && !listener->wasCreatedFromMarkup();
    auto listenerRef = listener.copyRef();

    if (!ensureEventTargetData().eventListenerMap.add(eventType, WTFMove(listener), { options.capture, passive.valueOr(false), options.once }))
        return false;

    if (listenerCreatedFromScript)
        InspectorInstrumentation::didAddEventListener(*this, eventType, listenerRef.get(), options.capture);

    if (eventNames().isWheelEventType(eventType))
        invalidateEventListenerRegions();

    eventListenersDidChange();
    return true;
}

void EventTarget::addEventListenerForBindings(const AtomString& eventType, RefPtr<EventListener>&& listener, AddEventListenerOptionsOrBoolean&& variant)
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

void EventTarget::removeEventListenerForBindings(const AtomString& eventType, RefPtr<EventListener>&& listener, ListenerOptionsOrBoolean&& variant)
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

bool EventTarget::removeEventListener(const AtomString& eventType, EventListener& listener, const ListenerOptions& options)
{
    auto* data = eventTargetData();
    if (!data)
        return false;

    InspectorInstrumentation::willRemoveEventListener(*this, eventType, listener, options.capture);

    if (data->eventListenerMap.remove(eventType, listener, options.capture)) {
        if (eventNames().isWheelEventType(eventType))
            invalidateEventListenerRegions();

        eventListenersDidChange();
        return true;
    }
    return false;
}

bool EventTarget::setAttributeEventListener(const AtomString& eventType, RefPtr<EventListener>&& listener, DOMWrapperWorld& isolatedWorld)
{
    auto* existingListener = attributeEventListener(eventType, isolatedWorld);
    if (!listener) {
        if (existingListener)
            removeEventListener(eventType, *existingListener, false);
        return false;
    }
    if (existingListener) {
        InspectorInstrumentation::willRemoveEventListener(*this, eventType, *existingListener, false);

#if ASSERT_ENABLED
        listener->checkValidityForEventTarget(*this);
#endif

        auto listenerPointer = listener.copyRef();
        eventTargetData()->eventListenerMap.replace(eventType, *existingListener, listener.releaseNonNull(), { });

        InspectorInstrumentation::didAddEventListener(*this, eventType, *listenerPointer, false);

        return true;
    }
    return addEventListener(eventType, listener.releaseNonNull());
}

EventListener* EventTarget::attributeEventListener(const AtomString& eventType, DOMWrapperWorld& isolatedWorld)
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

bool EventTarget::hasActiveEventListeners(const AtomString& eventType) const
{
    auto* data = eventTargetData();
    return data && data->eventListenerMap.containsActive(eventType);
}

ExceptionOr<bool> EventTarget::dispatchEventForBindings(Event& event)
{
    if (!event.isInitialized() || event.isBeingDispatched())
        return Exception { InvalidStateError };

    if (!scriptExecutionContext())
        return false;

    event.setUntrusted();

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

static const AtomString& legacyType(const Event& event)
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

    const AtomString& legacyTypeName = legacyType(event);
    if (!legacyTypeName.isNull()) {
        if (auto* legacyListenersVector = data->eventListenerMap.find(legacyTypeName)) {
            AtomString typeName = event.type();
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
    if (contextIsDocument)
        InspectorInstrumentation::willDispatchEvent(downcast<Document>(context), event);

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

        // Make sure the JS wrapper and function stay alive until the end of this scope. Otherwise,
        // event listeners with 'once' flag may get collected as soon as they get unregistered below,
        // before we call the js function.
        JSC::EnsureStillAliveScope wrapperProtector(registeredListener->callback().wrapper());
        JSC::EnsureStillAliveScope jsFunctionProtector(registeredListener->callback().jsFunction());

        // Do this before invocation to avoid reentrancy issues.
        if (registeredListener->isOnce())
            removeEventListener(event.type(), registeredListener->callback(), ListenerOptions(registeredListener->useCapture()));

        if (registeredListener->isPassive())
            event.setInPassiveListener(true);

#if ASSERT_ENABLED
        registeredListener->callback().checkValidityForEventTarget(*this);
#endif

        InspectorInstrumentation::willHandleEvent(context, event, *registeredListener);
        registeredListener->callback().handleEvent(context, event);
        InspectorInstrumentation::didHandleEvent(context, event, *registeredListener);

        if (registeredListener->isPassive())
            event.setInPassiveListener(false);
    }

    if (contextIsDocument)
        InspectorInstrumentation::didDispatchEvent(downcast<Document>(context), event);
}

Vector<AtomString> EventTarget::eventTypes()
{
    if (auto* data = eventTargetData())
        return data->eventListenerMap.eventTypes();
    return { };
}

const EventListenerVector& EventTarget::eventListeners(const AtomString& eventType)
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
    if (data && !data->eventListenerMap.isEmpty()) {
        if (data->eventListenerMap.contains(eventNames().wheelEvent) || data->eventListenerMap.contains(eventNames().mousewheelEvent))
            invalidateEventListenerRegions();

        data->eventListenerMap.clear();
        eventListenersDidChange();
    }

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

void EventTarget::invalidateEventListenerRegions()
{
    if (is<Element>(*this)) {
        downcast<Element>(*this).invalidateEventListenerRegions();
        return;
    }

    auto* document = [&]() -> Document* {
        if (is<Document>(*this))
            return &downcast<Document>(*this);
        if (is<DOMWindow>(*this))
            return downcast<DOMWindow>(*this).document();
        return nullptr;
    }();

    if (document)
        document->invalidateEventListenerRegions();
}

} // namespace WebCore
