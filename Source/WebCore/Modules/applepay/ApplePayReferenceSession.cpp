/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "ApplePayReferenceSession.h"

#if ENABLE(APPLE_PAY_REFERENCE_SESSION)

#include "ContextDestructionObserverInlines.h"
#include "Document.h"
#include "DocumentPage.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMPromiseDeferred.h"
#include "Page.h"
#include "PaymentCoordinator.h"
#include "PaymentSession.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ApplePayReferenceSession);

ExceptionOr<Ref<ApplePayReferenceSession>> ApplePayReferenceSession::create(Document& document, ApplePayReferenceRequest&& request)
{
    if (request.clientIdentifier.isEmpty())
        return Exception { ExceptionCode::TypeError, "clientIdentifier must not be empty."_s };

    auto canCall = PaymentSession::canCreateSession(document);
    if (canCall.hasException())
        return canCall.releaseException();

    if (!document.page())
        return Exception { ExceptionCode::InvalidAccessError, "Frame is detached"_s };

    Ref session = adoptRef(*new ApplePayReferenceSession(document, WTF::move(request)));
    session->suspendIfNeeded();
    return session;
}

ApplePayReferenceSession::ApplePayReferenceSession(Document& document, ApplePayReferenceRequest&& request)
    : ActiveDOMObject { document }
    , m_request { WTF::move(request) }
{
}

ApplePayReferenceSession::~ApplePayReferenceSession() = default;

void ApplePayReferenceSession::prepare(Document& document, PreparePromise&& promise)
{
    if (m_didCallPrepare) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "prepare() has already been called on this session."_s });
        return;
    }

    RefPtr page = document.page();
    if (!page || isContextStopped()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Frame is detached"_s });
        return;
    }

    m_didCallPrepare = true;
    m_preparePromise = makeUnique<PreparePromise>(WTF::move(promise));

    protect(page->paymentCoordinator())->prepareReferenceSession(m_request, PaymentSession::shouldDiscloseApplePayCapability(document), [pendingActivity = makePendingActivity(*this)](std::optional<String>&& token) {
        auto promise = std::exchange(pendingActivity->object().m_preparePromise, nullptr);
        if (!promise)
            return;

        if (!token)
            promise->reject(Exception { ExceptionCode::NotSupportedError, "Reference sessions are not supported."_s });
        else
            promise->resolve(WTF::move(*token));
    });
}

void ApplePayReferenceSession::stop()
{
    if (auto promise = std::exchange(m_preparePromise, nullptr))
        promise->reject(Exception { ExceptionCode::AbortError });
}

void ApplePayReferenceSession::suspend(ReasonForSuspension reason)
{
    if (reason == ReasonForSuspension::BackForwardCache)
        stop();
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY_REFERENCE_SESSION)
