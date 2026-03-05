/*
 * Copyright (C) 2024 Marais Rossouw <me@marais.co>. All rights reserved.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "InternalObserverFind.h"

#include "AbortSignal.h"
#include "CallbackResult.h"
#include "ContextDestructionObserverInlines.h"
#include "InternalObserver.h"
#include "JSDOMPromiseDeferred.h"
#include "Observable.h"
#include "PredicateCallback.h"
#include "ScriptExecutionContext.h"
#include "SubscribeOptions.h"
#include "Subscriber.h"
#include "SubscriberCallback.h"
#include <JavaScriptCore/JSCJSValueInlines.h>

namespace WebCore {

class InternalObserverFind final : public InternalObserver {
public:
    static Ref<InternalObserverFind> create(ScriptExecutionContext& context, Ref<PredicateCallback>&& callback, Ref<AbortSignal>&& signal, Ref<DeferredPromise>&& promise)
    {
        Ref internalObserver = adoptRef(*new InternalObserverFind(context, WTF::move(callback), WTF::move(signal), WTF::move(promise)));
        internalObserver->suspendIfNeeded();
        return internalObserver;
    }

private:
    void next(JSC::JSValue value) final
    {
        auto* globalObject = protect(scriptExecutionContext())->globalObject();
        ASSERT(globalObject);

        Ref vm = globalObject->vm();

        bool hasPassed = false;

        {
            JSC::JSLockHolder lock(vm);

            // The exception is not reported, instead it is forwarded to the
            // abort signal and promise rejection.
            auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

            auto result = m_callback->invokeRethrowingException(value, m_idx++);

            JSC::Exception* exception = scope.exception();
            if (exception) [[unlikely]] {
                scope.clearException();
                auto value = exception->value();
                m_promise->reject<IDLAny>(value);
                m_signal->signalAbort(value);
                return;
            }

            if (result.type() == CallbackResultType::Success)
                hasPassed = result.releaseReturnValue();
        }

        if (hasPassed) {
            m_promise->resolve<IDLAny>(value);
            m_signal->signalAbort(JSC::jsUndefined());
        }
    }

    void error(JSC::JSValue value) final
    {
        m_promise->reject<IDLAny>(value);
    }

    void complete() final
    {
        InternalObserver::complete();
        m_promise->resolve();
    }

    void visitAdditionalChildren(JSC::AbstractSlotVisitor& visitor) const final
    {
        m_callback->visitJSFunction(visitor);
    }

    InternalObserverFind(ScriptExecutionContext& context, Ref<PredicateCallback>&& callback, Ref<AbortSignal>&& signal, Ref<DeferredPromise>&& promise)
        : InternalObserver(context)
        , m_callback(WTF::move(callback))
        , m_signal(WTF::move(signal))
        , m_promise(WTF::move(promise))
    {
    }

    uint64_t m_idx { 0 };
    const Ref<PredicateCallback> m_callback;
    const Ref<AbortSignal> m_signal;
    const Ref<DeferredPromise> m_promise;
};

void createInternalObserverOperatorFind(ScriptExecutionContext& context, Observable& observable, Ref<PredicateCallback>&& callback, SubscribeOptions&& options, Ref<DeferredPromise>&& promise)
{
    Ref signal = AbortSignal::create(&context);

    Vector<Ref<AbortSignal>> dependentSignals = { signal };
    if (options.signal)
        dependentSignals.append(options.signal.releaseNonNull());
    Ref dependentSignal = AbortSignal::any(context, dependentSignals);

    if (dependentSignal->aborted())
        return promise->reject<IDLAny>(dependentSignal->reason().getValue());

    dependentSignal->addAlgorithm([promise](JSC::JSValue reason) {
        promise->reject<IDLAny>(reason);
    });

    Ref observer = InternalObserverFind::create(context, WTF::move(callback), WTF::move(signal), WTF::move(promise));

    observable.subscribeInternal(context, WTF::move(observer), SubscribeOptions { .signal = WTF::move(dependentSignal) });
}

} // namespace WebCore
