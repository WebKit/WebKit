/*
 * Copyright (C) 2024 Keith Cirkel <webkit@keithcirkel.co.uk>. All rights reserved.
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
#include "InternalObserverFilter.h"

#include "InternalObserver.h"
#include "Observable.h"
#include "PredicateCallback.h"
#include "ScriptExecutionContext.h"
#include "SubscribeOptions.h"
#include "Subscriber.h"
#include "SubscriberCallback.h"
#include <JavaScriptCore/JSCJSValueInlines.h>

namespace WebCore {

class InternalObserverFilter final : public InternalObserver {
public:
    static Ref<InternalObserverFilter> create(ScriptExecutionContext& context, Ref<Subscriber> subscriber, Ref<PredicateCallback> predicate)
    {
        Ref internalObserver = adoptRef(*new InternalObserverFilter(context, subscriber, predicate));
        internalObserver->suspendIfNeeded();
        return internalObserver;
    }

    class SubscriberCallbackFilter final : public SubscriberCallback {
    public:
        static Ref<SubscriberCallbackFilter> create(ScriptExecutionContext& context, Ref<Observable> source, Ref<PredicateCallback> predicate)
        {
            return adoptRef(*new InternalObserverFilter::SubscriberCallbackFilter(context, source, predicate));
        }

        CallbackResult<void> handleEvent(Subscriber& subscriber) final
        {
            RefPtr context = scriptExecutionContext();

            if (!context) {
                subscriber.complete();
                return { };
            }

            SubscribeOptions options;
            options.signal = &subscriber.signal();
            m_sourceObservable->subscribeInternal(*context, InternalObserverFilter::create(*context, subscriber, m_predicate), options);

            return { };
        }

        CallbackResult<void> handleEventRethrowingException(Subscriber& subscriber) final
        {
            return handleEvent(subscriber);
        }

    private:
        SubscriberCallbackFilter(ScriptExecutionContext& context, Ref<Observable> source, Ref<PredicateCallback> predicate)
            : SubscriberCallback(&context)
            , m_sourceObservable(source)
            , m_predicate(predicate)
        { }

        bool hasCallback() const final { return true; }

        Ref<Observable> m_sourceObservable;
        Ref<PredicateCallback> m_predicate;
    };

private:
    void next(JSC::JSValue value) final
    {
        RefPtr context = scriptExecutionContext();
        if (!context)
            return;

        Ref vm = context->globalObject()->vm();
        JSC::JSLockHolder lock(vm);

        auto matches = false;

        // The exception is not reported, instead it is forwarded to the
        // error handler.
        JSC::Exception* previousException = nullptr;
        {
            auto catchScope = DECLARE_CATCH_SCOPE(vm);
            auto result = m_predicate->handleEventRethrowingException(value, m_idx);
            previousException = catchScope.exception();
            if (previousException) {
                catchScope.clearException();
                m_subscriber->error(previousException->value());
                return;
            }

            if (result.type() == CallbackResultType::Success)
                matches = result.releaseReturnValue();
        }

        m_idx += 1;

        if (matches)
            m_subscriber->next(value);
    }

    void error(JSC::JSValue value) final
    {
        m_subscriber->error(value);
    }

    void complete() final
    {
        InternalObserver::complete();
        m_subscriber->complete();
    }

    void visitAdditionalChildren(JSC::AbstractSlotVisitor& visitor) const final
    {
        m_subscriber->visitAdditionalChildren(visitor);
        m_predicate->visitJSFunction(visitor);
    }

    void visitAdditionalChildren(JSC::SlotVisitor& visitor) const final
    {
        m_subscriber->visitAdditionalChildren(visitor);
        m_predicate->visitJSFunction(visitor);
    }

    InternalObserverFilter(ScriptExecutionContext& context, Ref<Subscriber> subscriber, Ref<PredicateCallback> predicate)
        : InternalObserver(context)
        , m_subscriber(subscriber)
        , m_predicate(predicate)
    { }

    Ref<Subscriber> m_subscriber;
    Ref<PredicateCallback> m_predicate;
    uint64_t m_idx { 0 };
};

Ref<SubscriberCallback> createSubscriberCallbackFilter(ScriptExecutionContext& context, Ref<Observable> observable, Ref<PredicateCallback> predicate)
{
    return InternalObserverFilter::SubscriberCallbackFilter::create(context, observable, predicate);
}

} // namespace WebCore
