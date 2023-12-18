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

#include "Document.h"
#include "JSDOMPromiseDeferred.h"
#include "NotImplemented.h"
#include "PaymentComplete.h"
#include "PaymentCompleteDetails.h"
#include "PaymentRequest.h"
#include <JavaScriptCore/JSONObject.h>
#include <JavaScriptCore/ThrowScope.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/RunLoop.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(PaymentResponse);

PaymentResponse::PaymentResponse(ScriptExecutionContext* context, PaymentRequest& request)
    : ActiveDOMObject { context }
    , m_request { request }
{
}

void PaymentResponse::finishConstruction()
{
    ASSERT(!hasPendingActivity());
    m_pendingActivity = makePendingActivity(*this);
    suspendIfNeeded();
}

PaymentResponse::~PaymentResponse()
{
    ASSERT(!hasPendingActivity() || isContextStopped());
    ASSERT(!hasRetryPromise() || isContextStopped());
}

void PaymentResponse::setDetailsFunction(DetailsFunction&& detailsFunction)
{
    m_detailsFunction = WTFMove(detailsFunction);
    m_cachedDetails.clear();
}

void PaymentResponse::complete(Document& document, std::optional<PaymentComplete>&& result, std::optional<PaymentCompleteDetails>&& details, DOMPromiseDeferred<void>&& promise)
{
    if (m_state == State::Stopped || !m_request) {
        promise.reject(Exception { ExceptionCode::AbortError });
        return;
    }

    if (m_state == State::Completed || m_retryPromise) {
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    String serializedData;
    if (details) {
        if (auto data = details->data) {
            auto throwScope = DECLARE_THROW_SCOPE(document.globalObject()->vm());

            serializedData = JSONStringify(document.globalObject(), data.get(), 0);
            if (throwScope.exception()) {
                promise.reject(Exception { ExceptionCode::ExistingExceptionError });
                return;
            }
        }
    }

    auto exception = m_request->complete(document, WTFMove(result), WTFMove(serializedData));
    if (!exception.hasException()) {
        ASSERT(hasPendingActivity());
        ASSERT(m_state == State::Created);
        m_pendingActivity = nullptr;
        m_state = State::Completed;
    }
    promise.settle(WTFMove(exception));
}

void PaymentResponse::retry(PaymentValidationErrors&& errors, DOMPromiseDeferred<void>&& promise)
{
    if (m_state == State::Stopped || !m_request) {
        promise.reject(Exception { ExceptionCode::AbortError });
        return;
    }

    if (m_state == State::Completed || m_retryPromise) {
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    ASSERT(hasPendingActivity());
    ASSERT(m_state == State::Created);

    auto exception = m_request->retry(WTFMove(errors));
    if (exception.hasException()) {
        promise.reject(exception.releaseException());
        return;
    }

    m_retryPromise = makeUnique<DOMPromiseDeferred<void>>(WTFMove(promise));
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
    ASSERT(m_state == State::Created || m_state == State::Stopped);
    m_retryPromise->settle(WTFMove(result));
    m_retryPromise = nullptr;
}

void PaymentResponse::stop()
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Payment, [this, pendingActivity = std::exchange(m_pendingActivity, nullptr)] {
        settleRetryPromise(Exception { ExceptionCode::AbortError });
    });
    m_state = State::Stopped;
}

void PaymentResponse::suspend(ReasonForSuspension reason)
{
    if (reason != ReasonForSuspension::BackForwardCache)
        return;

    if (m_state != State::Created) {
        ASSERT(!hasPendingActivity());
        ASSERT(!m_retryPromise);
        return;
    }

    stop();
}

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
