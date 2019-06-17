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

#include "JSDOMPromise.h"
#include "JSFetchResponse.h"

#if ENABLE(SERVICE_WORKER)

namespace WebCore {

Ref<FetchEvent> FetchEvent::createForTesting(ScriptExecutionContext& context)
{
    FetchEvent::Init init;
    init.request = FetchRequest::create(context, { }, FetchHeaders::create(FetchHeaders::Guard::Immutable, { }), { }, { }, { });
    return FetchEvent::create("fetch", WTFMove(init), Event::IsTrusted::Yes);
}

FetchEvent::FetchEvent(const AtomString& type, Init&& initializer, IsTrusted isTrusted)
    : ExtendableEvent(type, initializer, isTrusted)
    , m_request(initializer.request.releaseNonNull())
    , m_clientId(WTFMove(initializer.clientId))
    , m_reservedClientId(WTFMove(initializer.reservedClientId))
    , m_targetClientId(WTFMove(initializer.targetClientId))
{
}

FetchEvent::~FetchEvent()
{
    if (auto callback = WTFMove(m_onResponse))
        callback(makeUnexpected(ResourceError { errorDomainWebKitServiceWorker, 0, m_request->url(), "Fetch event is destroyed."_s, ResourceError::Type::Cancellation }));
}

ResourceError FetchEvent::createResponseError(const URL& url, const String& errorMessage)
{
    return ResourceError { errorDomainWebKitServiceWorker, 0, url, makeString("FetchEvent.respondWith received an error: ", errorMessage), ResourceError::Type::General };

}

ExceptionOr<void> FetchEvent::respondWith(Ref<DOMPromise>&& promise)
{
    if (!isBeingDispatched())
        return Exception { InvalidStateError, "Event is not being dispatched"_s };

    if (m_respondWithEntered)
        return Exception { InvalidStateError, "Event respondWith flag is set"_s };

    m_respondPromise = WTFMove(promise);
    addExtendLifetimePromise(*m_respondPromise);

    m_respondPromise->whenSettled([this, weakThis = makeWeakPtr(*this)] () {
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

void FetchEvent::onResponse(ResponseCallback&& callback)
{
    ASSERT(!m_onResponse);
    m_onResponse = WTFMove(callback);
}

void FetchEvent::respondWithError(ResourceError&& error)
{
    m_respondWithError = true;
    processResponse(makeUnexpected(WTFMove(error)));
}

void FetchEvent::processResponse(Expected<Ref<FetchResponse>, ResourceError>&& result)
{
    m_respondPromise = nullptr;
    m_waitToRespond = false;
    if (auto callback = WTFMove(m_onResponse))
        callback(WTFMove(result));
}

void FetchEvent::promiseIsSettled()
{
    if (m_respondPromise->status() == DOMPromise::Status::Rejected) {
        auto reason = m_respondPromise->result().toWTFString(m_respondPromise->globalObject()->globalExec());
        respondWithError(createResponseError(m_request->url(), reason));
        return;
    }

    ASSERT(m_respondPromise->status() == DOMPromise::Status::Fulfilled);
    auto response = JSFetchResponse::toWrapped(m_respondPromise->globalObject()->globalExec()->vm(), m_respondPromise->result());
    if (!response) {
        respondWithError(createResponseError(m_request->url(), "Returned response is null."_s));
        return;
    }

    if (response->isDisturbedOrLocked()) {
        respondWithError(createResponseError(m_request->url(), "Response is disturbed or locked."_s));
        return;
    }

    processResponse(makeRef(*response));
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
