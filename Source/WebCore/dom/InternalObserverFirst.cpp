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
#include "InternalObserverFirst.h"

#include "AbortController.h"
#include "AbortSignal.h"
#include "Exception.h"
#include "ExceptionCode.h"
#include "InternalObserver.h"
#include "JSDOMPromiseDeferred.h"
#include "Observable.h"
#include "ScriptExecutionContext.h"
#include "SubscribeOptions.h"
#include "Subscriber.h"
#include "SubscriberCallback.h"
#include <JavaScriptCore/JSCJSValueInlines.h>

namespace WebCore {

class InternalObserverFirst final : public InternalObserver {
public:
    static Ref<InternalObserverFirst> create(ScriptExecutionContext& context, Ref<AbortController>&& controller, Ref<DeferredPromise>&& promise)
    {
        Ref internalObserver = adoptRef(*new InternalObserverFirst(context, WTFMove(controller), WTFMove(promise)));
        internalObserver->suspendIfNeeded();
        return internalObserver;
    }

private:
    void next(JSC::JSValue value) final
    {
        Ref { m_promise }->resolve<IDLAny>(value);
        Ref { m_controller }->abort(JSC::jsUndefined());
    }

    void error(JSC::JSValue value) final
    {
        Ref { m_promise }->reject<IDLAny>(value);
    }

    void complete() final
    {
        InternalObserver::complete();
        Ref { m_promise }->reject(Exception { ExceptionCode::RangeError, "No values in Observable"_s });
    }

    void visitAdditionalChildren(JSC::AbstractSlotVisitor&) const final
    {
    }

    void visitAdditionalChildren(JSC::SlotVisitor&) const final
    {
    }

    InternalObserverFirst(ScriptExecutionContext& context, Ref<AbortController>&& controller, Ref<DeferredPromise>&& promise)
        : InternalObserver(context)
        , m_controller(WTFMove(controller))
        , m_promise(WTFMove(promise))
    {
    }

    Ref<AbortController> m_controller;
    Ref<DeferredPromise> m_promise;
};

void createInternalObserverOperatorFirst(ScriptExecutionContext& context, Observable& observable, const SubscribeOptions& options, Ref<DeferredPromise>&& promise)
{
    Ref controller = AbortController::create(context);

    Vector<Ref<AbortSignal>> dependentSignals = { controller->protectedSignal() };
    if (options.signal)
        dependentSignals.append(Ref { *options.signal });
    Ref dependentSignal = AbortSignal::any(context, dependentSignals);

    if (dependentSignal->aborted())
        return promise->reject<IDLAny>(dependentSignal->reason().getValue());

    dependentSignal->addAlgorithm([promise](JSC::JSValue reason) {
        promise->reject<IDLAny>(reason);
    });

    Ref observer = InternalObserverFirst::create(context, WTFMove(controller), WTFMove(promise));

    observable.subscribeInternal(context, WTFMove(observer), SubscribeOptions { .signal = WTFMove(dependentSignal) });
}

} // namespace WebCore
