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
#include "DigitalCredentialsSession.h"

#if ENABLE(WEB_AUTHN)

#include "AbortSignal.h"
#include "CredentialRequestCoordinator.h"
#include "Document.h"
#include "JSDOMPromiseDeferred.h"
#include "TaskSource.h"
#include <Logging.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DigitalCredentialsSession);

Ref<DigitalCredentialsSession> DigitalCredentialsSession::create(ScriptExecutionContext& context, CredentialRequestCoordinator& coordinator, CredentialPromise&& promise)
{
    Ref session = adoptRef(*new DigitalCredentialsSession(context, coordinator, WTF::move(promise)));
    session->suspendIfNeeded();
    return session;
}

DigitalCredentialsSession::DigitalCredentialsSession(ScriptExecutionContext& context, CredentialRequestCoordinator& coordinator, CredentialPromise&& promise)
    : ActiveDOMObject(&context)
    , m_coordinator(coordinator)
    , m_promise(makeUnique<CredentialPromise>(WTF::move(promise)))
{
}

DigitalCredentialsSession::~DigitalCredentialsSession()
{
    clearAbortAlgorithm();

    if (m_promise)
        m_promise->reject(Exception { ExceptionCode::AbortError, "The credential request was abandoned."_s });
}

void DigitalCredentialsSession::setAbortAlgorithm(RefPtr<AbortSignal>&& signal, uint32_t identifier)
{
    m_abortSignal = WTF::move(signal);
    m_abortAlgorithmIdentifier = identifier;
}

void DigitalCredentialsSession::clearAbortAlgorithm()
{
    if (!m_abortAlgorithmIdentifier)
        return;

    if (RefPtr signal = m_abortSignal)
        signal->removeAlgorithm(*m_abortAlgorithmIdentifier);

    m_abortAlgorithmIdentifier.reset();
    m_abortSignal = nullptr;
}

void DigitalCredentialsSession::queueSettlement(Function<void(CredentialPromise&)>&& settleFunction)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [settleFunction = WTF::move(settleFunction)](auto& session) mutable {
        session.settle(WTF::move(settleFunction));
    });
}

void DigitalCredentialsSession::settle(Function<void(CredentialPromise&)>&& settleFunction)
{
    if (!m_promise) {
        abandon();
        return;
    }

    clearAbortAlgorithm();
    auto promise = WTF::move(m_promise);
    settleFunction(*promise);
    abandon();
}

void DigitalCredentialsSession::abandon()
{
    if (RefPtr coordinator = m_coordinator.get())
        coordinator->sessionDidFinish(*this);
}

void DigitalCredentialsSession::stop()
{
    if (RefPtr coordinator = m_coordinator.get())
        coordinator->dismissChooser();

    queueSettlement([](auto& promise) {
        promise.reject(Exception { ExceptionCode::AbortError, "The document was stopped."_s });
    });

    clearAbortAlgorithm();
    abandon();
}

void DigitalCredentialsSession::suspend(ReasonForSuspension reason)
{
    if (reason != ReasonForSuspension::BackForwardCache)
        return;

    LOG(DigitalCredentials, "Credential request abandoned because its page entered the back/forward cache");
    stop();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
