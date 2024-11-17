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
#include "InternalObserverFind.h"

#include "AbortSignal.h"
#include "CallbackResult.h"
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
        Ref internalObserver = adoptRef(*new InternalObserverFind(context, WTFMove(callback), WTFMove(signal), WTFMove(promise)));
        internalObserver->suspendIfNeeded();
        return internalObserver;
    }

private:
    void next(JSC::JSValue value) final
    {
        auto* globalObject = protectedScriptExecutionContext()->globalObject();
        ASSERT(globalObject);

        Ref vm = globalObject->vm();

        bool hasPassed = false;

        {
            JSC::JSLockHolder lock(vm);

            // The exception is not reported, instead it is forwarded to the
            // abort signal and promise rejection.
            auto scope = DECLARE_CATCH_SCOPE(vm);

            auto result = protectedCallback()->handleEventRethrowingException(value, m_idx++);

            JSC::Exception* exception = scope.exception();
            if (UNLIKELY(exception)) {
                scope.clearException();
                auto value = exception->value();
                protectedPromise()->reject<IDLAny>(value);
                protectedSignal()->signalAbort(value);
                return;
            }

            if (result.type() == CallbackResultType::Success)
                hasPassed = result.releaseReturnValue();
        }

        if (hasPassed) {
            protectedPromise()->resolve<IDLAny>(value);
            protectedSignal()->signalAbort(JSC::jsUndefined());
        }
    }

    void error(JSC::JSValue value) final
    {
        protectedPromise()->reject<IDLAny>(value);
    }

    void complete() final
    {
        InternalObserver::complete();
        protectedPromise()->resolve();
    }

    void visitAdditionalChildren(JSC::AbstractSlotVisitor& visitor) const final
    {
        protectedCallback()->visitJSFunction(visitor);
    }

    void visitAdditionalChildren(JSC::SlotVisitor& visitor) const final
    {
        protectedCallback()->visitJSFunction(visitor);
    }

    Ref<DeferredPromise> protectedPromise() const { return m_promise; }
    Ref<PredicateCallback> protectedCallback() const { return m_callback; }
    Ref<AbortSignal> protectedSignal() const { return m_signal; }

    InternalObserverFind(ScriptExecutionContext& context, Ref<PredicateCallback>&& callback, Ref<AbortSignal>&& signal, Ref<DeferredPromise>&& promise)
        : InternalObserver(context)
        , m_callback(WTFMove(callback))
        , m_signal(WTFMove(signal))
        , m_promise(WTFMove(promise))
    {
    }

    uint64_t m_idx { 0 };
    Ref<PredicateCallback> m_callback;
    Ref<AbortSignal> m_signal;
    Ref<DeferredPromise> m_promise;
};

void createInternalObserverOperatorFind(ScriptExecutionContext& context, Observable& observable, Ref<PredicateCallback>&& callback, const SubscribeOptions& options, Ref<DeferredPromise>&& promise)
{
    Ref signal = AbortSignal::create(&context);

    Vector<Ref<AbortSignal>> dependentSignals = { signal };
    if (options.signal)
        dependentSignals.append(Ref { *options.signal });
    Ref dependentSignal = AbortSignal::any(context, dependentSignals);

    if (dependentSignal->aborted())
        return promise->reject<IDLAny>(dependentSignal->reason().getValue());

    dependentSignal->addAlgorithm([promise](JSC::JSValue reason) {
        promise->reject<IDLAny>(reason);
    });

    Ref observer = InternalObserverFind::create(context, WTFMove(callback), WTFMove(signal), WTFMove(promise));

    observable.subscribeInternal(context, WTFMove(observer), SubscribeOptions { .signal = WTFMove(dependentSignal) });
}

} // namespace WebCore
