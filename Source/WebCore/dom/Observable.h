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

#pragma once

#include "ExceptionOr.h"
#include "ScriptWrappable.h"
#include "SubscriberCallback.h"
#include "VoidCallback.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class InternalObserver;
class ScriptExecutionContext;
class JSSubscriptionObserverCallback;
class PredicateCallback;
class MapperCallback;
struct SubscriptionObserver;
struct SubscribeOptions;

class Observable final : public ScriptWrappable, public RefCounted<Observable> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(Observable);

public:
    using ObserverUnion = std::variant<RefPtr<JSSubscriptionObserverCallback>, SubscriptionObserver>;

    static Ref<Observable> create(Ref<SubscriberCallback>);

    explicit Observable(Ref<SubscriberCallback>);

    void subscribe(ScriptExecutionContext&, std::optional<ObserverUnion>, SubscribeOptions);
    void subscribeInternal(ScriptExecutionContext&, Ref<InternalObserver>&&, const SubscribeOptions&);

    Ref<Observable> map(ScriptExecutionContext&, MapperCallback&);

    Ref<Observable> filter(ScriptExecutionContext&, PredicateCallback&);

    Ref<Observable> take(ScriptExecutionContext&, uint64_t);

    Ref<Observable> drop(ScriptExecutionContext&, uint64_t);

    // Promise-returning operators.

    void first(ScriptExecutionContext&, const SubscribeOptions&, Ref<DeferredPromise>&&);
    void last(ScriptExecutionContext&, const SubscribeOptions&, Ref<DeferredPromise>&&);

private:
    Ref<SubscriberCallback> m_subscriberCallback;
};

} // namespace WebCore
