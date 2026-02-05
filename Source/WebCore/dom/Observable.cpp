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
#include "Observable.h"

#include "AbortSignal.h"
#include "CallbackResult.h"
#include "Document.h"
#include "Exception.h"
#include "ExceptionCode.h"
#include "InternalObserverDrop.h"
#include "InternalObserverEvery.h"
#include "InternalObserverFilter.h"
#include "InternalObserverFind.h"
#include "InternalObserverFirst.h"
#include "InternalObserverForEach.h"
#include "InternalObserverFromScript.h"
#include "InternalObserverInspect.h"
#include "InternalObserverLast.h"
#include "InternalObserverMap.h"
#include "InternalObserverReduce.h"
#include "InternalObserverSome.h"
#include "InternalObserverTake.h"
#include "JSDOMPromiseDeferred.h"
#include "JSSubscriptionObserverCallback.h"
#include "MapperCallback.h"
#include "ObservableInspector.h"
#include "PredicateCallback.h"
#include "ReducerCallback.h"
#include "ScriptWrappableInlines.h"
#include "SubscribeOptions.h"
#include "Subscriber.h"
#include "SubscriberCallback.h"
#include "SubscriptionObserver.h"
#include "VisitorCallback.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Observable);

Ref<Observable> Observable::create(Ref<SubscriberCallback> callback)
{
    return adoptRef(*new Observable(callback));
}

void Observable::subscribe(ScriptExecutionContext& context, ObserverUnion&& observer, SubscribeOptions&& options)
{
    WTF::switchOn(WTF::move(observer),
        [&](RefPtr<JSSubscriptionObserverCallback>&& next) {
            subscribeInternal(context, InternalObserverFromScript::create(context, WTF::move(next)), WTF::move(options));
        },
        [&](SubscriptionObserver&& subscription) {
            subscribeInternal(context, InternalObserverFromScript::create(context, WTF::move(subscription)), WTF::move(options));
        }
    );
}

void Observable::subscribeInternal(ScriptExecutionContext& context, Ref<InternalObserver>&& observer, SubscribeOptions&& options)
{
    RefPtr document = dynamicDowncast<Document>(context);
    if (document && !document->isFullyActive())
        return;

    Ref subscriber = Subscriber::create(context, WTF::move(observer), WTF::move(options));

    Ref vm = context.globalObject()->vm();
    JSC::JSLockHolder lock(vm);

    // The exception is not reported, instead it is forwarded to the
    // error handler.
    JSC::Exception* previousException = nullptr;
    {
        auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
        m_subscriberCallback->invokeRethrowingException(subscriber);
        previousException = catchScope.exception();
        if (previousException) {
            catchScope.clearException();
            subscriber->error(previousException->value());
        }
    }
}

Ref<Observable> Observable::map(ScriptExecutionContext& context, MapperCallback& mapper)
{
    return create(createSubscriberCallbackMap(context, *this, mapper));
}

Ref<Observable> Observable::filter(ScriptExecutionContext& context, PredicateCallback& predicate)
{
    return create(createSubscriberCallbackFilter(context, *this, predicate));
}

Ref<Observable> Observable::take(ScriptExecutionContext& context, uint64_t amount)
{
    return create(createSubscriberCallbackTake(context, *this, amount));
}

Ref<Observable> Observable::drop(ScriptExecutionContext& context, uint64_t amount)
{
    return create(createSubscriberCallbackDrop(context, *this, amount));
}

Ref<Observable> Observable::inspect(ScriptExecutionContext& context, InspectorUnion&& inspectorUnion)
{
    return WTF::switchOn(WTF::move(inspectorUnion),
        [&](RefPtr<JSSubscriptionObserverCallback>&& next) {
            return create(createSubscriberCallbackInspect(context, *this, WTF::move(next)));
        },
        [&](ObservableInspector&& inspector) {
            return create(createSubscriberCallbackInspect(context, *this, WTF::move(inspector)));
        }
    );
}

void Observable::first(ScriptExecutionContext& context, SubscribeOptions&& options, Ref<DeferredPromise>&& promise)
{
    return createInternalObserverOperatorFirst(context, *this, WTF::move(options), WTF::move(promise));
}

void Observable::forEach(ScriptExecutionContext& context, Ref<VisitorCallback>&& callback, SubscribeOptions&& options, Ref<DeferredPromise>&& promise)
{
    return createInternalObserverOperatorForEach(context, *this, WTF::move(callback), WTF::move(options), WTF::move(promise));
}

void Observable::last(ScriptExecutionContext& context, SubscribeOptions&& options, Ref<DeferredPromise>&& promise)
{
    return createInternalObserverOperatorLast(context, *this, WTF::move(options), WTF::move(promise));
}

void Observable::find(ScriptExecutionContext& context, Ref<PredicateCallback>&& callback, SubscribeOptions&& options, Ref<DeferredPromise>&& promise)
{
    return createInternalObserverOperatorFind(context, *this, WTF::move(callback), WTF::move(options), WTF::move(promise));
}

void Observable::every(ScriptExecutionContext& context, Ref<PredicateCallback>&& callback, SubscribeOptions&& options, Ref<DeferredPromise>&& promise)
{
    return createInternalObserverOperatorEvery(context, *this, WTF::move(callback), WTF::move(options), WTF::move(promise));
}

void Observable::some(ScriptExecutionContext& context, Ref<PredicateCallback>&& callback, SubscribeOptions&& options, Ref<DeferredPromise>&& promise)
{
    return createInternalObserverOperatorSome(context, *this, WTF::move(callback), WTF::move(options), WTF::move(promise));
}

void Observable::reduce(ScriptExecutionContext& context, Ref<ReducerCallback>&& callback, JSC::JSValue initialValue, SubscribeOptions&& options, Ref<DeferredPromise>&& promise)
{
    return createInternalObserverOperatorReduce(context, *this, WTF::move(callback), initialValue, WTF::move(options), WTF::move(promise));
}

Observable::Observable(Ref<SubscriberCallback> callback)
    : m_subscriberCallback(callback)
{
}

Observable::~Observable() = default;

} // namespace WebCore
