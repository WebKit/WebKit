/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2021 Apple Inc. All rights reserved.
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

#include "AddEventListenerOptions.h"
#include "DOMWrapperWorld.h"
#include "EventNames.h"
#include "EventPath.h"
#include "EventTargetConcrete.h"
#include "HTMLBodyElement.h"
#include "HTMLHtmlElement.h"
#include "InspectorInstrumentation.h"
#include "JSErrorHandler.h"
#include "JSEventListener.h"
#include "Logging.h"
#include "Quirks.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(EventTarget);

struct SameSizeAsEventTarget : ScriptWrappable, CanMakeWeakPtrWithBitField<EventTarget, WeakPtrFactoryInitialization::Lazy, WeakPtrImplWithEventTargetData> {
    virtual ~SameSizeAsEventTarget() = default; // Allocate vtable pointer.
};

static_assert(sizeof(EventTarget) == sizeof(SameSizeAsEventTarget), "EventTarget should stay small");

Ref<EventTarget> EventTarget::create(ScriptExecutionContext& context)
{
    return EventTargetConcrete::create(context);
}

EventTarget::~EventTarget()
{
    // Explicitly tearing down since WeakPtrImpl can be alive longer than EventTarget.
    if (auto* eventTargetData = this->eventTargetData())
        eventTargetData->clear();
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

    if (options.signal && options.signal->aborted())
        return false;

    auto passive = options.passive;

    auto typeInfo = eventNames().typeInfoForEvent(eventType);
    if (!passive.has_value() && Quirks::shouldMakeEventListenerPassive(*this, typeInfo))
        passive = true;

    bool listenerCreatedFromScript = [&] {
        auto* jsEventListener = dynamicDowncast<JSEventListener>(listener.get());
        return jsEventListener && !jsEventListener->wasCreatedFromMarkup();
    }();

    if (!ensureEventTargetData().eventListenerMap.add(eventType, listener.copyRef(), { options.capture, passive.value_or(false), options.once }))
        return false;

    if (options.signal) {
        options.signal->addAlgorithm([weakThis = WeakPtr { *this }, eventType, listener = WeakPtr { listener }, capture = options.capture](JSC::JSValue) {
            if (weakThis && listener)
                Ref { *weakThis }->removeEventListener(eventType, *listener, capture);
        });
    }

    if (listenerCreatedFromScript)
        InspectorInstrumentation::didAddEventListener(*this, eventType, listener.get(), options.capture);

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

    std::visit(visitor, variant);
}

void EventTarget::removeEventListenerForBindings(const AtomString& eventType, RefPtr<EventListener>&& listener, EventListenerOptionsOrBoolean&& variant)
{
    if (!listener)
        return;

    auto visitor = WTF::makeVisitor([&](const EventListenerOptions& options) {
        removeEventListener(eventType, *listener, options);
    }, [&](bool capture) {
        removeEventListener(eventType, *listener, capture);
    });

    std::visit(visitor, variant);
}

bool EventTarget::removeEventListener(const AtomString& eventType, EventListener& listener, const EventListenerOptions& options)
{
    auto* data = eventTargetData();
    if (!data)
        return false;

    InspectorInstrumentation::willRemoveEventListener(*this, eventType, listener, options.capture);

    if (data->eventListenerMap.remove(eventType, listener, options.capture)) {
        eventListenersDidChange();
        return true;
    }
    return false;
}

template<typename JSMaybeErrorEventListener>
void EventTarget::setAttributeEventListener(const AtomString& eventType, JSC::JSValue listener, JSC::JSObject& jsEventTarget)
{
    Ref isolatedWorld = worldForDOMObject(jsEventTarget);
    RefPtr existingListener = attributeEventListener(eventType, isolatedWorld);
    if (!listener.isObject()) {
        if (existingListener)
            removeEventListener(eventType, *existingListener, false);
    } else if (existingListener) {
        bool capture = false;

        InspectorInstrumentation::willRemoveEventListener(*this, eventType, *existingListener, capture);
        existingListener->replaceJSFunctionForAttributeListener(asObject(listener), &jsEventTarget);
        InspectorInstrumentation::didAddEventListener(*this, eventType, *existingListener, capture);
    } else
        addEventListener(eventType, JSMaybeErrorEventListener::create(*asObject(listener), jsEventTarget, true, isolatedWorld), { });
}

template void EventTarget::setAttributeEventListener<JSErrorHandler>(const AtomString& eventType, JSC::JSValue listener, JSC::JSObject& jsEventTarget);
template void EventTarget::setAttributeEventListener<JSEventListener>(const AtomString& eventType, JSC::JSValue listener, JSC::JSObject& jsEventTarget);

bool EventTarget::setAttributeEventListener(const AtomString& eventType, RefPtr<EventListener>&& listener, DOMWrapperWorld& isolatedWorld)
{
    RefPtr existingListener = attributeEventListener(eventType, isolatedWorld);
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

        eventTargetData()->eventListenerMap.replace(eventType, *existingListener, *listener, { });

        InspectorInstrumentation::didAddEventListener(*this, eventType, *listener, false);

        return true;
    }
    return addEventListener(eventType, listener.releaseNonNull(), { });
}

RefPtr<JSEventListener> EventTarget::attributeEventListener(const AtomString& eventType, DOMWrapperWorld& isolatedWorld)
{
    for (RefPtr eventListener : eventListeners(eventType)) {
        RefPtr jsListener = dynamicDowncast<JSEventListener>(eventListener->callback());
        if (jsListener && jsListener->isAttribute() && jsListener->isolatedWorld() == &isolatedWorld)
            return jsListener;
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
        return Exception { ExceptionCode::InvalidStateError };

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

    EventPath eventPath { *this };
    event.setTarget(this);
    event.setCurrentTarget(this);
    event.setEventPhase(Event::AT_TARGET);
    event.resetBeforeDispatch();
    event.setEventPath(eventPath);
    fireEventListeners(event, EventInvokePhase::Capturing);
    fireEventListeners(event, EventInvokePhase::Bubbling);
    event.resetAfterDispatch();
}

void EventTarget::uncaughtExceptionInEventHandler()
{
}

const AtomString& EventTarget::legacyTypeForEvent(const Event& event)
{
    auto& eventNames = WebCore::eventNames();
    if (event.type() == eventNames.animationendEvent)
        return eventNames.webkitAnimationEndEvent;

    if (event.type() == eventNames.animationstartEvent)
        return eventNames.webkitAnimationStartEvent;

    if (event.type() == eventNames.animationiterationEvent)
        return eventNames.webkitAnimationIterationEvent;

    if (event.type() == eventNames.transitionendEvent)
        return eventNames.webkitTransitionEndEvent;

    // FIXME: This legacy name is not part of the specification (https://dom.spec.whatwg.org/#dispatching-events).
    if (event.type() == eventNames.wheelEvent)
        return eventNames.mousewheelEvent;

    return nullAtom();
}

// https://dom.spec.whatwg.org/#concept-event-listener-invoke
void EventTarget::fireEventListeners(Event& event, EventInvokePhase phase)
{
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    if (RefPtr node = dynamicDowncast<Node>(*this))
        ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isEventDispatchAllowedInSubtree(*node));
    else
        ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::isScriptAllowedInMainThread());
#endif
    ASSERT(event.isInitialized());

    auto* data = eventTargetData();
    if (!data)
        return;

    if (auto* listenersVector = data->eventListenerMap.find(event.type())) {
        innerInvokeEventListeners(event, *listenersVector, phase);
        return;
    }

    // Only fall back to legacy types for trusted events.
    if (!event.isTrusted())
        return;

    const AtomString& legacyTypeName = legacyTypeForEvent(event);
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
    Ref protectedThis { *this };
    ASSERT(!listeners.isEmpty());
    ASSERT(scriptExecutionContext());

    Ref context = *scriptExecutionContext();
    auto* document = dynamicDowncast<Document>(context.get());
    if (document)
        InspectorInstrumentation::willDispatchEvent(*document, event);

    for (auto& registeredListener : listeners) {
        if (UNLIKELY(registeredListener->wasRemoved()))
            continue;

        if (phase == EventInvokePhase::Capturing && !registeredListener->useCapture())
            continue;
        if (phase == EventInvokePhase::Bubbling && registeredListener->useCapture())
            continue;

        Ref callback = registeredListener->callback();
        if (InspectorInstrumentation::isEventListenerDisabled(*this, event.type(), callback, registeredListener->useCapture()))
            continue;

        // If stopImmediatePropagation has been called, we just break out immediately, without
        // handling any more events on this target.
        if (event.immediatePropagationStopped())
            break;

        // Make sure the JS wrapper and function stay alive until the end of this scope. Otherwise,
        // event listeners with 'once' flag may get collected as soon as they get unregistered below,
        // before we call the js function.
        JSC::EnsureStillAliveScope wrapperProtector(callback->wrapper());
        JSC::EnsureStillAliveScope jsFunctionProtector(callback->jsFunction());

        // Do this before invocation to avoid reentrancy issues.
        if (registeredListener->isOnce())
            removeEventListener(event.type(), callback, registeredListener->useCapture());

        if (registeredListener->isPassive())
            event.setInPassiveListener(true);

#if ASSERT_ENABLED
        callback->checkValidityForEventTarget(*this);
#endif

        InspectorInstrumentation::willHandleEvent(context, event, *registeredListener);
        callback->handleEvent(context, event);
        InspectorInstrumentation::didHandleEvent(context, event, *registeredListener);

        if (registeredListener->isPassive())
            event.setInPassiveListener(false);
    }

    if (document)
        InspectorInstrumentation::didDispatchEvent(*document, event);
}

Vector<AtomString> EventTarget::eventTypes() const
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
        data->eventListenerMap.clear();
        eventListenersDidChange();
    }

    threadData.setIsInRemoveAllEventListeners(false);
}

} // namespace WebCore
