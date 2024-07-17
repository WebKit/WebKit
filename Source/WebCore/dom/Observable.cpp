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

#include "CallbackResult.h"
#include "Exception.h"
#include "ExceptionCode.h"
#include "SubscribeOptions.h"
#include "Subscriber.h"
#include "SubscriberCallback.h"
#include "SubscriptionObserver.h"
#include "SubscriptionObserverCallback.h"

namespace WebCore {

ExceptionOr<Ref<Observable>> Observable::create(Ref<SubscriberCallback> callback)
{
    return adoptRef(*new Observable(callback));
}

void Observable::subscribe(ScriptExecutionContext& context, std::optional<ObserverUnion> observer, SubscribeOptions options)
{
    RefPtr document = dynamicDowncast<Document>(context);
    if (document && !document->isFullyActive())
        return;

    Ref<Subscriber> subscriber = makeSubscriber(context, observer);

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
        m_subscriber->handleEvent(subscriber);
        previousException = catchScope.exception();
        if (previousException) {
            catchScope.clearException();
            subscriber->error(previousException->value());
        }
    }
}

Ref<Subscriber> Observable::makeSubscriber(ScriptExecutionContext& context, std::optional<ObserverUnion> observer)
{
    if (observer.has_value()) {
        return WTF::switchOn(
            observer.value(),
            [&](RefPtr<JSSubscriptionObserverCallback>& next) {
                return Subscriber::create(context, next);
            },
            [&](SubscriptionObserver& subscription) {
                return Subscriber::create(context, subscription.next, subscription.error, subscription.complete);
            }
        );
    }

    return Subscriber::create(context);
}

WTF_MAKE_ISO_ALLOCATED_IMPL(Observable);

Observable::Observable(Ref<SubscriberCallback> callback)
    : m_subscriber(callback)
{
}

} // namespace WebCore
