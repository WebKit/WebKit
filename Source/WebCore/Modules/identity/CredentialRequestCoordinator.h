/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "DigitalCredentialsProtocols.h"
#include "ExceptionOr.h"
#include "JSDOMPromiseDeferredForward.h"
#include "UnvalidatedDigitalCredentialRequest.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <optional>
#include <wtf/CanMakeWeakPtr.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {
class AbortSignal;
class BasicCredential;
class CredentialRequestCoordinatorClient;
class Document;
class LocalFrame;
class Page;
struct DigitalCredentialsResponseData;
struct ExceptionData;

using CredentialPromise = DOMPromiseDeferred<IDLNullable<IDLInterface<BasicCredential>>>;

class CredentialRequestCoordinator final : public RefCounted<CredentialRequestCoordinator>, public ActiveDOMObject {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(CredentialRequestCoordinator, WEBCORE_EXPORT);
    WTF_MAKE_NONCOPYABLE(CredentialRequestCoordinator);

public:
    static Ref<CredentialRequestCoordinator> create(Ref<CredentialRequestCoordinatorClient>&&, Page&);
    WEBCORE_EXPORT void prepareCredentialRequests(const Document&, CredentialPromise&&, Vector<UnvalidatedDigitalCredentialRequest>&&, RefPtr<AbortSignal>);
    WEBCORE_EXPORT void abortTheCredentialRequest(ExceptionOr<JSC::JSValue>&&);
    ~CredentialRequestCoordinator();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void contextDestroyed() final;

private:
    void settleTheCredentialRequest(ExceptionOr<RefPtr<BasicCredential>>&&);
    void rejectTheCredentialRequestWith(Exception&&);
    void clearAbortAlgorithm();

    class InteractionStateGuard final {
    public:
        explicit InteractionStateGuard(CredentialRequestCoordinator&);
        InteractionStateGuard(const InteractionStateGuard&) = delete;
        InteractionStateGuard& operator=(const InteractionStateGuard&) = delete;
        void deactivate() { m_active = false; }

        InteractionStateGuard(InteractionStateGuard&&) noexcept = delete;
        InteractionStateGuard& operator=(InteractionStateGuard&&) noexcept = delete;

        ~InteractionStateGuard();

    private:
        WeakRef<CredentialRequestCoordinator> m_coordinator;
        bool m_active { true };
    }; // class InteractionStateGuard

    enum class InteractionState : uint8_t {
        Idle,
        Requesting,
        Aborting
    }; // enum class InteractionState

    bool NODELETE canTransitionTo(InteractionState) const;
    InteractionState NODELETE interactionState() const;
    void NODELETE setInteractionState(InteractionState);
    bool hasCurrentPromise() const { return !!m_currentPromise; }
    void setCurrentPromise(CredentialPromise&&);
    CredentialPromise* NODELETE currentPromise();

    ExceptionOr<JSC::JSObject*> parseDigitalCredentialsResponseData(const String&) const;
    void initiateTheCredentialRequest(const Document&, Vector<ValidatedDigitalCredentialRequest>&&, Vector<UnvalidatedDigitalCredentialRequest>&&, RefPtr<AbortSignal>);
    void processCredentialChooserResponse(Expected<DigitalCredentialsResponseData, ExceptionData>&& responseOrException, RefPtr<AbortSignal>);

    explicit CredentialRequestCoordinator(Ref<CredentialRequestCoordinatorClient>&&, Page&);
    const Ref<CredentialRequestCoordinatorClient> m_client;
    InteractionState m_interactionState { InteractionState::Idle };
    RefPtr<AbortSignal> m_abortSignal;
    std::unique_ptr<CredentialPromise> m_currentPromise;
    std::optional<uint32_t> m_abortAlgorithmIdentifier;
    WeakPtr<Page> m_page;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
