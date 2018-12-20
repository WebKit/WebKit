/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "PaymentResponse.h"

#if ENABLE(PAYMENT_REQUEST)

#include "NotImplemented.h"
#include "PaymentRequest.h"
#include <wtf/RunLoop.h>

namespace WebCore {

PaymentResponse::PaymentResponse(ScriptExecutionContext* context, PaymentRequest& request)
    : ActiveDOMObject { context }
    , m_request { makeWeakPtr(request) }
{
    suspendIfNeeded();
}

void PaymentResponse::finishConstruction()
{
    ASSERT(!hasPendingActivity());
    m_pendingActivity = makePendingActivity(*this);
}

PaymentResponse::~PaymentResponse()
{
    ASSERT(!hasPendingActivity());
    ASSERT(!hasRetryPromise());
}

void PaymentResponse::setDetailsFunction(DetailsFunction&& detailsFunction)
{
    m_detailsFunction = WTFMove(detailsFunction);
    m_cachedDetails = { };
}

void PaymentResponse::complete(Optional<PaymentComplete>&& result, DOMPromiseDeferred<void>&& promise)
{
    if (m_state == State::Stopped || !m_request) {
        promise.reject(Exception { AbortError });
        return;
    }

    if (m_state == State::Completed || m_retryPromise) {
        promise.reject(Exception { InvalidStateError });
        return;
    }

    ASSERT(hasPendingActivity());
    ASSERT(m_state == State::Created);
    m_pendingActivity = nullptr;
    m_state = State::Completed;

    promise.settle(m_request->complete(WTFMove(result)));
}

void PaymentResponse::retry(PaymentValidationErrors&& errors, DOMPromiseDeferred<void>&& promise)
{
    if (m_state == State::Stopped || !m_request) {
        promise.reject(Exception { AbortError });
        return;
    }

    if (m_state == State::Completed || m_retryPromise) {
        promise.reject(Exception { InvalidStateError });
        return;
    }

    ASSERT(hasPendingActivity());
    ASSERT(m_state == State::Created);

    auto exception = m_request->retry(WTFMove(errors));
    if (exception.hasException()) {
        promise.reject(exception.releaseException());
        return;
    }

    m_retryPromise = WTFMove(promise);
}

void PaymentResponse::abortWithException(Exception&& exception)
{
    settleRetryPromise(WTFMove(exception));
    m_pendingActivity = nullptr;
    m_state = State::Completed;
}

void PaymentResponse::settleRetryPromise(ExceptionOr<void>&& result)
{
    if (!m_retryPromise)
        return;

    ASSERT(hasPendingActivity());
    ASSERT(m_state == State::Created);
    std::exchange(m_retryPromise, WTF::nullopt)->settle(WTFMove(result));
}

bool PaymentResponse::canSuspendForDocumentSuspension() const
{
    ASSERT(m_state != State::Stopped);
    return !hasPendingActivity();
}

void PaymentResponse::stop()
{
    settleRetryPromise(Exception { AbortError });
    m_pendingActivity = nullptr;
    m_state = State::Stopped;
}

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
