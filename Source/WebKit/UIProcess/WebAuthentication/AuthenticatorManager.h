/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "Authenticator.h"
#include "AuthenticatorTransportService.h"
#include "WebAuthenticationRequestData.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/PublicKeyCredentialData.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>

namespace WebKit {

class AuthenticatorManager : public AuthenticatorTransportService::Observer, public Authenticator::Observer {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(AuthenticatorManager);
public:
    using Respond = Variant<WebCore::PublicKeyCredentialData, WebCore::ExceptionData>;
    using Callback = CompletionHandler<void(Respond&&)>;
    using TransportSet = HashSet<WebCore::AuthenticatorTransport, WTF::IntHash<WebCore::AuthenticatorTransport>, WTF::StrongEnumHashTraits<WebCore::AuthenticatorTransport>>;

    using AuthenticatorTransportService::Observer::weakPtrFactory;

    AuthenticatorManager();
    virtual ~AuthenticatorManager() = default;

    void makeCredential(const Vector<uint8_t>& hash, const WebCore::PublicKeyCredentialCreationOptions&, Callback&&);
    void getAssertion(const Vector<uint8_t>& hash, const WebCore::PublicKeyCredentialRequestOptions&, Callback&&);

    virtual bool isMock() const { return false; }

protected:
    Callback& pendingCompletionHandler() { return m_pendingCompletionHandler; }
    RunLoop::Timer<AuthenticatorManager>& requestTimeOutTimer() { return m_requestTimeOutTimer; }
    void clearStateAsync(); // To void cyclic dependence.

private:
    // AuthenticatorTransportService::Observer
    void authenticatorAdded(Ref<Authenticator>&&) final;

    // Authenticator::Observer
    void respondReceived(Respond&&) final;

    // Overriden by MockAuthenticatorManager.
    virtual UniqueRef<AuthenticatorTransportService> createService(WebCore::AuthenticatorTransport, AuthenticatorTransportService::Observer&) const;
    // Overriden to return every exception for tests to confirm.
    virtual void respondReceivedInternal(Respond&&);

    void startDiscovery(const TransportSet&);
    void initTimeOutTimer(const Optional<unsigned>& timeOutInMs);
    void timeOutTimerFired();

    // Request: We only allow one request per time.
    WebAuthenticationRequestData m_pendingRequestData;
    Callback m_pendingCompletionHandler;
    RunLoop::Timer<AuthenticatorManager> m_requestTimeOutTimer;

    Vector<UniqueRef<AuthenticatorTransportService>> m_services;
    HashSet<Ref<Authenticator>> m_authenticators;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
