/*
 * Copyright (C) 2024 Marais Rossouw <me@marais.co>. All rights reserved.
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

#include "config.h"
#include "InternalObserverInspect.h"

#include "InternalObserver.h"
#include "JSSubscriptionObserverCallback.h"
#include "Observable.h"
#include "ObservableInspector.h"
#include "ScriptExecutionContext.h"
#include "SubscribeOptions.h"
#include "Subscriber.h"
#include "SubscriberCallback.h"
#include <JavaScriptCore/JSCJSValueInlines.h>

namespace WebCore {

class InternalObserverInspect final : public InternalObserver {
public:
    static Ref<InternalObserverInspect> create(ScriptExecutionContext& context, Ref<Subscriber>&& subscriber, ObservableInspector&& inspector)
    {
        Ref internalObserver = adoptRef(*new InternalObserverInspect(context, WTFMove(subscriber), WTFMove(inspector)));
        internalObserver->suspendIfNeeded();
        return internalObserver;
    }

    class SubscriberCallbackInspect final : public SubscriberCallback {
    public:
        static Ref<SubscriberCallbackInspect> create(ScriptExecutionContext& context, Ref<Observable>&& source, ObservableInspector&& inspector)
        {
            return adoptRef(*new SubscriberCallbackInspect(context, WTFMove(source), WTFMove(inspector)));
        }

        CallbackResult<void> handleEvent(Subscriber& subscriber) final
        {
            RefPtr context = scriptExecutionContext();

            if (!context) {
                subscriber.complete();
                return { };
            }

            if (RefPtr subscribe = m_inspector.subscribe) {
                auto* globalObject = protectedScriptExecutionContext()->globalObject();
                ASSERT(globalObject);

                Ref vm = globalObject->vm();

                JSC::JSLockHolder lock(vm);
                auto scope = DECLARE_CATCH_SCOPE(vm);

                subscribe->handleEventRethrowingException();

                JSC::Exception* exception = scope.exception();
                if (UNLIKELY(exception)) {
                    scope.clearException();
                    subscriber.error(exception->value());
                    return { };
                }
            }

            Ref inspect = InternalObserverInspect::create(*context, subscriber, ObservableInspector { m_inspector });
            Ref { m_sourceObservable }->subscribeInternal(*context, WTFMove(inspect), SubscribeOptions { &subscriber.signal() });

            return { };
        }

        CallbackResult<void> handleEventRethrowingException(Subscriber& subscriber) final
        {
            return handleEvent(subscriber);
        }

    private:
        bool hasCallback() const final { return true; }

        SubscriberCallbackInspect(ScriptExecutionContext& context, Ref<Observable>&& source, ObservableInspector&& inspector)
            : SubscriberCallback(&context)
            , m_sourceObservable(WTFMove(source))
            , m_inspector(WTFMove(inspector))
        { }

        const Ref<Observable> m_sourceObservable;
        const ObservableInspector m_inspector;
    };

private:
    void next(JSC::JSValue value) final
    {
        if (RefPtr next = m_inspector.next) {
            Ref vm = this->vm();
            JSC::JSLockHolder lock(vm);
            auto scope = DECLARE_CATCH_SCOPE(vm);

            next->handleEventRethrowingException(value);

            JSC::Exception* exception = scope.exception();
            if (UNLIKELY(exception)) {
                scope.clearException();
                protectedSubscriber()->error(exception->value());
                return;
            }
        }

        protectedSubscriber()->next(value);
    }

    void error(JSC::JSValue value) final
    {
        removeAbortHandler();

        if (RefPtr error = m_inspector.error) {
            Ref vm = this->vm();
            JSC::JSLockHolder lock(vm);
            auto scope = DECLARE_CATCH_SCOPE(vm);

            error->handleEventRethrowingException(value);

            JSC::Exception* exception = scope.exception();
            if (UNLIKELY(exception)) {
                scope.clearException();
                protectedSubscriber()->error(exception->value());
                return;
            }
        }

        protectedSubscriber()->error(value);
    }

    void complete() final
    {
        InternalObserver::complete();

        removeAbortHandler();

        if (RefPtr complete = m_inspector.complete) {
            Ref vm = this->vm();
            JSC::JSLockHolder lock(vm);
            auto scope = DECLARE_CATCH_SCOPE(vm);

            complete->handleEventRethrowingException();

            JSC::Exception* exception = scope.exception();
            if (UNLIKELY(exception)) {
                scope.clearException();
                protectedSubscriber()->error(exception->value());
                return;
            }
        }

        protectedSubscriber()->complete();
    }

    void visitAdditionalChildren(JSC::AbstractSlotVisitor& visitor) const final
    {
        m_subscriber->visitAdditionalChildren(visitor);
        if (m_inspector.next)
            SUPPRESS_UNCOUNTED_ARG m_inspector.next->visitJSFunction(visitor);
        if (m_inspector.error)
            SUPPRESS_UNCOUNTED_ARG m_inspector.error->visitJSFunction(visitor);
        if (m_inspector.complete)
            SUPPRESS_UNCOUNTED_ARG m_inspector.complete->visitJSFunction(visitor);
        if (m_inspector.subscribe)
            SUPPRESS_UNCOUNTED_ARG m_inspector.subscribe->visitJSFunction(visitor);
        if (m_inspector.abort)
            SUPPRESS_UNCOUNTED_ARG m_inspector.abort->visitJSFunction(visitor);
    }

    void removeAbortHandler()
    {
        if (!m_abortAlgorithmHandler)
            return;

        auto handle = std::exchange(m_abortAlgorithmHandler, std::nullopt);
        protectedSubscriber()->protectedSignal()->removeAlgorithm(*handle);
    }

    JSC::VM& vm() const
    {
        auto* globalObject = protectedScriptExecutionContext()->globalObject();
        ASSERT(globalObject);
        return globalObject->vm();
    }

    Ref<Subscriber> protectedSubscriber() const
    {
        return m_subscriber;
    }

    InternalObserverInspect(ScriptExecutionContext& context, Ref<Subscriber>&& subscriber, ObservableInspector&& inspector)
        : InternalObserver(context)
        , m_subscriber(WTFMove(subscriber))
        , m_inspector(WTFMove(inspector))
    {
        if (RefPtr abort = m_inspector.abort) {
            Ref signal = protectedSubscriber()->signal();
            m_abortAlgorithmHandler = signal->addAlgorithm([abort = WTFMove(abort)](JSC::JSValue reason) {
                abort->handleEvent(reason);
            });
        }
    }

    const Ref<Subscriber> m_subscriber;
    const ObservableInspector m_inspector;
    std::optional<uint32_t> m_abortAlgorithmHandler;
};

Ref<SubscriberCallback> createSubscriberCallbackInspect(ScriptExecutionContext& context, Ref<Observable>&& observable, RefPtr<JSSubscriptionObserverCallback>&& next)
{
    return InternalObserverInspect::SubscriberCallbackInspect::create(context, WTFMove(observable), ObservableInspector { .next = WTFMove(next) });
}

Ref<SubscriberCallback> createSubscriberCallbackInspect(ScriptExecutionContext& context, Ref<Observable>&& observable, ObservableInspector&& inspector)
{
    return InternalObserverInspect::SubscriberCallbackInspect::create(context, WTFMove(observable), WTFMove(inspector));
}

} // namespace WebCore
