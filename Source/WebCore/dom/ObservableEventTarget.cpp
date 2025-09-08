/*
 * Copyright (C) 2025 Marais Rossouw <me@marais.co>. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
// https://wicg.github.io/observable/#event-target-integration

#include "config.h"
#include "ObservableEventTarget.h"

#include "AddEventListenerOptions.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "JSEvent.h"
#include "ObservableEventListenerOptions.h"
#include "ScriptExecutionContext.h"
#include "SubscribeOptions.h"
#include "Subscriber.h"
#include "SubscriberCallback.h"
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class ObservableEventListener final : public EventListener {
public:
    static Ref<ObservableEventListener> create(Ref<Subscriber>&& subscriber)
    {
        return adoptRef(*new ObservableEventListener(WTFMove(subscriber)));
    }

    void handleEvent(ScriptExecutionContext& context, Event& event) override
    {
        auto* globalObject = context.globalObject();
        if (!globalObject)
            return;

        m_subscriber->next(toJS(globalObject, JSC::jsCast<JSDOMGlobalObject*>(globalObject), event));
    }

private:
    ObservableEventListener(Ref<Subscriber>&& subscriber)
        : EventListener(EventListener::CPPEventListenerType)
        , m_subscriber(WTFMove(subscriber))
    {
    }

    const Ref<Subscriber> m_subscriber;
};

class SubscriberCallbackEventTarget final : public SubscriberCallback {
public:
    static Ref<SubscriberCallbackEventTarget> create(ScriptExecutionContext& context, EventTarget& eventTarget, const AtomString& eventType, const ObservableEventListenerOptions& options)
    {
        return adoptRef(*new SubscriberCallbackEventTarget(context, eventTarget, eventType, options));
    }

    CallbackResult<void> invoke(Subscriber& subscriber) final
    {
        if (subscriber.signal().aborted())
            return { };

        if (RefPtr eventTarget = m_eventTarget.get()) {
            AddEventListenerOptions addEventListenerOptions = { m_options.capture, m_options.passive, /* once */ false, subscriber.signal() };
            eventTarget->addEventListener(m_eventType, ObservableEventListener::create(subscriber), WTFMove(addEventListenerOptions));
        }

        return { };
    }

    CallbackResult<void> invokeRethrowingException(Subscriber& subscriber) final { return invoke(subscriber); }

private:
    bool hasCallback() const final { return true; }

    SubscriberCallbackEventTarget(ScriptExecutionContext& context, EventTarget& eventTarget, const AtomString& eventType, const ObservableEventListenerOptions& options)
        : SubscriberCallback(&context)
        , m_eventTarget(eventTarget)
        , m_eventType(eventType)
        , m_options(options)
    {
    }

    WeakPtr<EventTarget, WeakPtrImplWithEventTargetData> m_eventTarget;
    AtomString m_eventType;
    ObservableEventListenerOptions m_options;
};

Ref<SubscriberCallback> createSubscriberCallbackEventTarget(ScriptExecutionContext& context, EventTarget& eventTarget, const AtomString& eventType, const ObservableEventListenerOptions& options)
{
    return SubscriberCallbackEventTarget::create(context, eventTarget, eventType, options);
}

} // namespace WebCore
