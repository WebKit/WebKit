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
#include "InternalObserverReduce.h"

#include "AbortSignal.h"
#include "Exception.h"
#include "ExceptionCode.h"
#include "InternalObserver.h"
#include "JSDOMPromiseDeferred.h"
#include "JSValueInWrappedObject.h"
#include "Observable.h"
#include "ReducerCallback.h"
#include "ScriptExecutionContext.h"
#include "SubscribeOptions.h"
#include "Subscriber.h"
#include "SubscriberCallback.h"
#include <JavaScriptCore/JSCJSValueInlines.h>

namespace WebCore {

class InternalObserverReduce final : public InternalObserver {
public:
    static Ref<InternalObserverReduce> create(ScriptExecutionContext& context, Ref<AbortSignal>&& signal, Ref<ReducerCallback>&& callback, JSC::JSValue initialValue, Ref<DeferredPromise>&& promise)
    {
        Ref internalObserver = adoptRef(*new InternalObserverReduce(context, WTFMove(signal), WTFMove(callback), initialValue, WTFMove(promise)));
        internalObserver->suspendIfNeeded();
        return internalObserver;
    }

private:
    void next(JSC::JSValue value) final
    {
        if (!m_accumulator) {
            m_index++;
            m_accumulator.setWeakly(value);
            return;
        }

        auto* globalObject = protectedScriptExecutionContext()->globalObject();
        ASSERT(globalObject);

        Ref vm = globalObject->vm();

        JSC::JSLockHolder lock(vm);
        auto scope = DECLARE_CATCH_SCOPE(vm);

        auto result = protectedCallback()->handleEventRethrowingException(m_accumulator.getValue(), value, m_index++);

        JSC::Exception* exception = scope.exception();
        if (UNLIKELY(exception)) {
            scope.clearException();
            auto value = exception->value();
            protectedPromise()->reject<IDLAny>(value);
            Ref { m_signal }->signalAbort(value);
        }

        if (result.type() == CallbackResultType::Success)
            m_accumulator.setWeakly(result.releaseReturnValue());
    }

    void error(JSC::JSValue value) final
    {
        protectedPromise()->reject<IDLAny>(value);
    }

    void complete() final
    {
        InternalObserver::complete();

        if (UNLIKELY(!m_accumulator)) {
            protectedPromise()->reject(Exception { ExceptionCode::TypeError, "No inital value for Observable with no values"_s });
            return;
        }

        protectedPromise()->resolve<IDLAny>(m_accumulator.getValue());
    }

    void visitAdditionalChildren(JSC::AbstractSlotVisitor& visitor) const final
    {
        m_callback->visitJSFunction(visitor);
        m_accumulator.visit(visitor);
    }

    Ref<DeferredPromise> protectedPromise() const { return m_promise; }
    Ref<ReducerCallback> protectedCallback() const { return m_callback; }

    InternalObserverReduce(ScriptExecutionContext& context, Ref<AbortSignal>&& signal, Ref<ReducerCallback>&& callback, JSC::JSValue initialValue, Ref<DeferredPromise>&& promise)
        : InternalObserver(context)
        , m_signal(WTFMove(signal))
        , m_callback(WTFMove(callback))
        , m_promise(WTFMove(promise))
    {
        if (UNLIKELY(!initialValue.isUndefined()))
            m_accumulator.setWeakly(initialValue);
    }

    uint64_t m_index { 0 };
    const Ref<AbortSignal> m_signal;
    const Ref<ReducerCallback> m_callback;
    JSValueInWrappedObject m_accumulator;
    const Ref<DeferredPromise> m_promise;
};

void createInternalObserverOperatorReduce(ScriptExecutionContext& context, Observable& observable, Ref<ReducerCallback>&& callback, JSC::JSValue initialValue, const SubscribeOptions& options, Ref<DeferredPromise>&& promise)
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

    Ref observer = InternalObserverReduce::create(context, WTFMove(signal), WTFMove(callback), initialValue, WTFMove(promise));
    observable.subscribeInternal(context, WTFMove(observer), SubscribeOptions { .signal = WTFMove(dependentSignal) });
}

} // namespace WebCore
