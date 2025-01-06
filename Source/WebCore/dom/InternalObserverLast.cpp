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
#include "InternalObserverLast.h"

#include "AbortController.h"
#include "AbortSignal.h"
#include "Exception.h"
#include "ExceptionCode.h"
#include "InternalObserver.h"
#include "JSDOMPromiseDeferred.h"
#include "JSValueInWrappedObject.h"
#include "Observable.h"
#include "ScriptExecutionContext.h"
#include "SubscribeOptions.h"
#include "Subscriber.h"
#include "SubscriberCallback.h"
#include <JavaScriptCore/JSCJSValueInlines.h>

namespace WebCore {

class InternalObserverLast final : public InternalObserver {
public:
    static Ref<InternalObserverLast> create(ScriptExecutionContext& context, Ref<DeferredPromise>&& promise)
    {
        Ref internalObserver = adoptRef(*new InternalObserverLast(context, WTFMove(promise)));
        internalObserver->suspendIfNeeded();
        return internalObserver;
    }

private:
    void next(JSC::JSValue value) final
    {
        m_lastValue.setWeakly(value);
    }

    void error(JSC::JSValue value) final
    {
        protectedPromise()->reject<IDLAny>(value);
    }

    void complete() final
    {
        InternalObserver::complete();

        if (UNLIKELY(!m_lastValue))
            return protectedPromise()->reject(Exception { ExceptionCode::RangeError, "No values in Observable"_s });

        protectedPromise()->resolve<IDLAny>(m_lastValue.getValue());
    }

    void visitAdditionalChildren(JSC::AbstractSlotVisitor& visitor) const final
    {
        m_lastValue.visit(visitor);
    }

    Ref<DeferredPromise> protectedPromise() const { return m_promise; }

    InternalObserverLast(ScriptExecutionContext& context, Ref<DeferredPromise>&& promise)
        : InternalObserver(context)
        , m_promise(WTFMove(promise))
    {
    }

    JSValueInWrappedObject m_lastValue;
    const Ref<DeferredPromise> m_promise;
};

void createInternalObserverOperatorLast(ScriptExecutionContext& context, Observable& observable, const SubscribeOptions& options, Ref<DeferredPromise>&& promise)
{
    if (RefPtr signal = options.signal) {
        if (signal->aborted())
            return promise->reject<IDLAny>(signal->reason().getValue());

        signal->addAlgorithm([promise](JSC::JSValue reason) {
            promise->reject<IDLAny>(reason);
        });
    }

    Ref observer = InternalObserverLast::create(context, WTFMove(promise));

    observable.subscribeInternal(context, WTFMove(observer), options);
}

} // namespace WebCore
