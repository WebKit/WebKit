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
#include "FetchEvent.h"

#include "JSFetchResponse.h"

#if ENABLE(SERVICE_WORKER)

namespace WebCore {

FetchEvent::FetchEvent(const AtomicString& type, Init&& initializer, IsTrusted isTrusted)
    : ExtendableEvent(type, initializer, isTrusted)
    , m_request(initializer.request.releaseNonNull())
    , m_clientId(WTFMove(initializer.clientId))
    , m_reservedClientId(WTFMove(initializer.reservedClientId))
    , m_targetClientId(WTFMove(initializer.targetClientId))
{
}

ExceptionOr<void> FetchEvent::respondWith(JSC::ExecState& state, JSC::JSValue promise)
{
    if (isBeingDispatched())
        return Exception { InvalidStateError, ASCIILiteral("Event is being dispatched") };

    if (m_respondWithEntered)
        return Exception { InvalidStateError, ASCIILiteral("Event respondWith flag is set") };

    m_respondPromise = DOMPromise::create(state, promise);
    addPendingPromise(*m_respondPromise);

    m_respondPromise->whenSettled([this, weakThis = createWeakPtr()] () {
        if (!weakThis)
            return;
        promiseIsSettled();
    });

    stopPropagation();
    stopImmediatePropagation();

    m_respondWithEntered = true;
    m_waitToRespond = true;

    return { };
}

void FetchEvent::onResponse(WTF::Function<void()>&& callback)
{
    ASSERT(!m_onResponse);
    m_onResponse = WTFMove(callback);
}

void FetchEvent::respondWithError()
{
    m_respondWithError = true;
    processResponse();
}

void FetchEvent::processResponse()
{
    m_respondPromise = nullptr;
    m_waitToRespond = false;
    if (auto callback = WTFMove(m_onResponse))
        callback();
}

void FetchEvent::promiseIsSettled()
{
    if (m_respondPromise->status() == DOMPromise::Status::Rejected) {
        respondWithError();
        return;
    }

    ASSERT(m_respondPromise->status() == DOMPromise::Status::Fulfilled);
    auto response = JSFetchResponse::toWrapped(m_respondPromise->globalObject()->globalExec()->vm(), m_respondPromise->result());
    if (!response) {
        respondWithError();
        return;
    }

    if (response->isDisturbedOrLocked()) {
        respondWithError();
        return;
    }

    m_response = WTFMove(response);

    // FIXME: We should process the response and send the body in streaming.
    if (m_response->hasReadableStreamBody()) {
        m_response->consumeBodyFromReadableStream([this, protectedThis = makeRef(*this)] (ExceptionOr<RefPtr<SharedBuffer>>&& result) mutable {
            if (result.hasException()) {
                respondWithError();
                return;
            }
            m_responseBody = result.releaseReturnValue();
            processResponse();
        });
        return;
    }
    if (m_response->isLoading()) {
        m_response->consumeBodyWhenLoaded([this, protectedThis = makeRef(*this)] (ExceptionOr<RefPtr<SharedBuffer>>&& result) mutable {
            if (result.hasException()) {
                respondWithError();
                return;
            }
            m_responseBody = result.releaseReturnValue();
            processResponse();
        });
        return;
    }

    auto body = m_response->consumeBody();
    WTF::switchOn(body, [] (Ref<FormData>&) {
        // FIXME: Support FormData response bodies.
    }, [this] (Ref<SharedBuffer>& buffer) {
        m_responseBody = WTFMove(buffer);
    }, [] (std::nullptr_t&) {
    });

    processResponse();
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
