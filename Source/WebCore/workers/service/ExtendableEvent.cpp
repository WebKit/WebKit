/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ExtendableEvent.h"

#if ENABLE(SERVICE_WORKER)

#include "JSDOMPromise.h"

namespace WebCore {

ExtendableEvent::ExtendableEvent(const AtomicString& type, const ExtendableEventInit& initializer, IsTrusted isTrusted)
    : Event(type, initializer, isTrusted)
{
}

ExtendableEvent::ExtendableEvent(const AtomicString& type, bool bubbles, bool cancelable)
    : Event(type, bubbles, cancelable)
{
}

ExtendableEvent::~ExtendableEvent()
{
}

ExceptionOr<void> ExtendableEvent::waitUntil(Ref<DOMPromise>&& promise)
{
    if (!isTrusted())
        return Exception { InvalidStateError, ASCIILiteral("Event is not trusted") };

    if (m_pendingPromises.isEmpty() && isBeingDispatched())
        return Exception { InvalidStateError, ASCIILiteral("Event is being dispatched") };

    addPendingPromise(WTFMove(promise));
    return { };
}

void ExtendableEvent::onFinishedWaitingForTesting(WTF::Function<void()>&& callback)
{
    ASSERT(!m_onFinishedWaitingForTesting);
    m_onFinishedWaitingForTesting = WTFMove(callback);
}

void ExtendableEvent::addPendingPromise(Ref<DOMPromise>&& promise)
{
    promise->whenSettled([this, weakThis = createWeakPtr(), settledPromise = promise.ptr()] () {
        if (!weakThis)
            return;

        auto promise = m_pendingPromises.take(*settledPromise);

        // FIXME: Implement registration handling as per https://w3c.github.io/ServiceWorker/v1/#dom-extendableevent-waituntil.

        if (!m_pendingPromises.isEmpty())
            return;

        if (auto callback = WTFMove(m_onFinishedWaitingForTesting))
            callback();
    });
    m_pendingPromises.add(WTFMove(promise));
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
