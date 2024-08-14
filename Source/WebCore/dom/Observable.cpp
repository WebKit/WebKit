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
#include "InternalObserverFilter.h"
#include "InternalObserverFromScript.h"
#include "InternalObserverMap.h"
#include "InternalObserverTake.h"
#include "JSSubscriptionObserverCallback.h"
#include "MapperCallback.h"
#include "PredicateCallback.h"
#include "SubscribeOptions.h"
#include "Subscriber.h"
#include "SubscriberCallback.h"
#include "SubscriptionObserver.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(Observable);

Ref<Observable> Observable::create(Ref<SubscriberCallback> callback)
{
    return adoptRef(*new Observable(callback));
}

void Observable::subscribe(ScriptExecutionContext& context, std::optional<ObserverUnion> observer, SubscribeOptions options)
{
    if (observer) {
        WTF::switchOn(
            observer.value(),
            [&](RefPtr<JSSubscriptionObserverCallback>& next) {
                subscribeInternal(context, InternalObserverFromScript::create(context, next), options);
            },
            [&](SubscriptionObserver& subscription) {
                subscribeInternal(context, InternalObserverFromScript::create(context, subscription), options);
            }
        );
    } else
        subscribeInternal(context, InternalObserverFromScript::create(context, nullptr), options);
}

void Observable::subscribeInternal(ScriptExecutionContext& context, Ref<InternalObserver> observer, SubscribeOptions options)
{
    RefPtr document = dynamicDowncast<Document>(context);
    if (document && !document->isFullyActive())
        return;

    auto subscriber = Subscriber::create(context, observer);

    if (options.signal)
        subscriber->followSignal(*options.signal.get());

    Ref vm = context.globalObject()->vm();
    JSC::JSLockHolder lock(vm);

    // The exception is not reported, instead it is forwarded to the
    // error handler. As such, SusbcribeCallback `[RethrowsException]` and
    // here a catch scope is declared so the error can be passed to the
    // subscription error handler.
    JSC::Exception* previousException = nullptr;
    {
        auto catchScope = DECLARE_CATCH_SCOPE(vm);
        m_subscriberCallback->handleEvent(subscriber);
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

Observable::Observable(Ref<SubscriberCallback> callback)
    : m_subscriberCallback(callback)
{
}

} // namespace WebCore
