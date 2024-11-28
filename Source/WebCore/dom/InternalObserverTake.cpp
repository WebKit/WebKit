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
#include "InternalObserverTake.h"

#include "InternalObserver.h"
#include "Observable.h"
#include "ScriptExecutionContext.h"
#include "SubscribeOptions.h"
#include "Subscriber.h"
#include "SubscriberCallback.h"
#include <JavaScriptCore/JSCJSValueInlines.h>

namespace WebCore {

class InternalObserverTake final : public InternalObserver {
public:
    static Ref<InternalObserverTake> create(ScriptExecutionContext& context, Ref<Subscriber> subscriber, uint64_t amount)
    {
        Ref internalObserver = adoptRef(*new InternalObserverTake(context, subscriber, amount));
        internalObserver->suspendIfNeeded();
        return internalObserver;
    }

    class SubscriberCallbackTake final : public SubscriberCallback {
    public:
        static Ref<SubscriberCallbackTake> create(ScriptExecutionContext& context, Ref<Observable> source, uint64_t amount)
        {
            return adoptRef(*new InternalObserverTake::SubscriberCallbackTake(context, source, amount));
        }

        CallbackResult<void> handleEventRethrowingException(Subscriber& subscriber) final
        {
            RefPtr context = scriptExecutionContext();

            if (!context || !m_amount) {
                subscriber.complete();
                return { };
            }

            SubscribeOptions options;
            options.signal = &subscriber.signal();
            m_sourceObservable->subscribeInternal(*context, InternalObserverTake::create(*context, subscriber, m_amount), options);

            return { };
        }

    private:
        SubscriberCallbackTake(ScriptExecutionContext& context, Ref<Observable> source, uint64_t amount)
            : SubscriberCallback(&context)
            , m_sourceObservable(source)
            , m_amount(amount)
        { }

        bool hasCallback() const final { return true; }

        Ref<Observable> m_sourceObservable;
        uint64_t m_amount;
    };

private:
    void next(JSC::JSValue value) final
    {
        if (!m_amount)
            return;

        m_subscriber->next(value);
        m_amount -= 1;
        if (!m_amount)
            m_subscriber->complete();
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
    }

    void visitAdditionalChildren(JSC::SlotVisitor& visitor) const final
    {
        m_subscriber->visitAdditionalChildren(visitor);
    }

    InternalObserverTake(ScriptExecutionContext& context, Ref<Subscriber> subscriber, uint64_t amount)
        : InternalObserver(context)
        , m_subscriber(subscriber)
        , m_amount(amount)
    { }

    Ref<Subscriber> m_subscriber;
    uint64_t m_amount;
};

Ref<SubscriberCallback> createSubscriberCallbackTake(ScriptExecutionContext& context, Ref<Observable> observable, uint64_t amount)
{
    return InternalObserverTake::SubscriberCallbackTake::create(context, observable, amount);
}

} // namespace WebCore
