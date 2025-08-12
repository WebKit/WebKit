/*
 * Copyright (C) 2025 Marais Rossouw <me@marais.co>. All rights reserved.
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
#include "InternalObserverToArray.h"

#include "InternalObserver.h"
#include "JSDOMPromiseDeferred.h"
#include "JSValueInWrappedObject.h"
#include "Observable.h"
#include "ScriptExecutionContext.h"
#include "SubscribeOptions.h"
#include <wtf/Vector.h>

namespace WebCore {

class InternalObserverToArray final : public InternalObserver {
public:
    static Ref<InternalObserverToArray> create(ScriptExecutionContext& context, Ref<DeferredPromise>&& promise)
    {
        Ref internalObserver = adoptRef(*new InternalObserverToArray(context, WTFMove(promise)));
        internalObserver->suspendIfNeeded();
        return internalObserver;
    }

private:
    void next(JSC::JSValue value) final
    {
        m_list.append({ globalVM(), value });
    }

    void error(JSC::JSValue value) final
    {
        m_promise->reject<IDLAny>(value);
    }

    void complete() final
    {
        InternalObserver::complete();

        m_promise->resolve<IDLSequence<IDLAny>>(m_list);
    }

    void visitAdditionalChildren(JSC::AbstractSlotVisitor&) const final
    {
    }

    InternalObserverToArray(ScriptExecutionContext& context, Ref<DeferredPromise>&& promise)
        : InternalObserver(context)
        , m_promise(WTFMove(promise))
    {
    }

    Vector<JSC::Strong<JSC::Unknown>> m_list;
    const Ref<DeferredPromise> m_promise;
};

void createInternalObserverOperatorToArray(ScriptExecutionContext& context, Observable& observable, const SubscribeOptions& options, Ref<DeferredPromise>&& promise)
{
    if (RefPtr signal = options.signal) {
        if (signal->aborted())
            return promise->reject<IDLAny>(signal->reason().getValue());

        signal->addAlgorithm([promise](JSC::JSValue reason) {
            promise->reject<IDLAny>(reason);
        });
    }

    Ref observer = InternalObserverToArray::create(context, WTFMove(promise));
    observable.subscribeInternal(context, WTFMove(observer), options);
}

} // namespace WebCore
