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

#include <WebCore/ActiveDOMObject.h>
#include <WebCore/DigitalCredentialsProtocols.h>
#include <WebCore/JSDOMPromiseDeferred.h>
#include <WebCore/UnvalidatedDigitalCredentialRequest.h>
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
struct DigitalCredentialsRequestData;
struct DigitalCredentialsResponseData;
struct ExceptionData;

using CredentialPromise = DOMPromiseDeferred<IDLNullable<IDLInterface<BasicCredential>>>;

class CredentialRequestCoordinator final : public RefCounted<CredentialRequestCoordinator>, public ActiveDOMObject {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(CredentialRequestCoordinator, WEBCORE_EXPORT);
    WTF_MAKE_NONCOPYABLE(CredentialRequestCoordinator);

public:
    static Ref<CredentialRequestCoordinator> create(Ref<CredentialRequestCoordinatorClient>&&, Page&);
    WEBCORE_EXPORT void presentPicker(const Document&, CredentialPromise&&, Vector<UnvalidatedDigitalCredentialRequest>&&, RefPtr<AbortSignal>);
    WEBCORE_EXPORT void abortPicker(ExceptionOr<JSC::JSValue>&&);
    ~CredentialRequestCoordinator();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void contextDestroyed() final;

private:
    static constexpr bool canPresentDigitalCredentialsUI()
    {
#if HAVE(DIGITAL_CREDENTIALS_UI)
        return true;
#else
        return false;
#endif
    }
    class PickerStateGuard final {
    public:
        explicit PickerStateGuard(CredentialRequestCoordinator&);
        PickerStateGuard(const PickerStateGuard&) = delete;
        PickerStateGuard& operator=(const PickerStateGuard&) = delete;

        PickerStateGuard(PickerStateGuard&&) noexcept;
        PickerStateGuard& operator=(PickerStateGuard&&) noexcept;

        ~PickerStateGuard();

    private:
        WeakRef<CredentialRequestCoordinator> m_coordinator;
        bool m_active { true };
    }; // class PickerStateGuard

    enum class PickerState : uint8_t {
        Idle,
        Presenting,
        Aborting
    }; // enum class PickerState

    bool canTransitionTo(PickerState) const;
    PickerState currentState() const;
    void setState(PickerState);
    bool hasCurrentPromise() const { return m_currentPromise.has_value(); }
    void setCurrentPromise(CredentialPromise&&);
    CredentialPromise* currentPromise();

    ExceptionOr<JSC::JSObject*> parseDigitalCredentialsResponseData(Document&, const String&) const;
    void handleDigitalCredentialsPickerResult(Expected<DigitalCredentialsResponseData, ExceptionData>&& responseOrException, RefPtr<AbortSignal>);
    void finalizeDigitalCredential(const DigitalCredentialsResponseData&);

    explicit CredentialRequestCoordinator(Ref<CredentialRequestCoordinatorClient>&&, Page&);
    const Ref<CredentialRequestCoordinatorClient> m_client;
    PickerState m_state { PickerState::Idle };
    std::optional<CredentialPromise> m_currentPromise;
    WeakPtr<Page> m_page;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
