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

#pragma once

#if ENABLE(WEB_AUTHN)

#include "ActiveDOMObject.h"
#include "JSDOMPromiseDeferredForward.h"
#include <optional>
#include <wtf/Function.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AbortSignal;
class BasicCredential;
class CredentialRequestCoordinator;
class ScriptExecutionContext;

using CredentialPromise = DOMPromiseDeferred<IDLNullable<IDLInterface<BasicCredential>>>;

class DigitalCredentialsSession final : public RefCounted<DigitalCredentialsSession>, public ActiveDOMObject {
    WTF_MAKE_TZONE_ALLOCATED(DigitalCredentialsSession);
    WTF_MAKE_NONCOPYABLE(DigitalCredentialsSession);

public:
    static Ref<DigitalCredentialsSession> create(ScriptExecutionContext&, CredentialRequestCoordinator&, CredentialPromise&&);
    ~DigitalCredentialsSession();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    bool hasPromise() const { return !!m_promise; }

    void setAbortAlgorithm(RefPtr<AbortSignal>&&, uint32_t identifier);
    void clearAbortAlgorithm();

    // https://w3c-fedid.github.io/digital-credentials/#dfn-reject-the-credential-request-with
    void queueSettlement(Function<void(CredentialPromise&)>&&);

private:
    DigitalCredentialsSession(ScriptExecutionContext&, CredentialRequestCoordinator&, CredentialPromise&&);

    void settle(Function<void(CredentialPromise&)>&&);
    void abandon();

    // ActiveDOMObject
    void stop() final;
    void suspend(ReasonForSuspension) final;

    WeakPtr<CredentialRequestCoordinator> m_coordinator;
    std::unique_ptr<CredentialPromise> m_promise;
    RefPtr<AbortSignal> m_abortSignal;
    std::optional<uint32_t> m_abortAlgorithmIdentifier;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
