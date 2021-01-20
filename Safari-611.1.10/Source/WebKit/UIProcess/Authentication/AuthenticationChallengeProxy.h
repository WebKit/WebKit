/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include <WebCore/AuthenticationChallenge.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class Connection;
}

namespace WebKit {

class AuthenticationDecisionListener;
class SecKeyProxyStore;
class WebCredential;
class WebProtectionSpace;

class AuthenticationChallengeProxy : public API::ObjectImpl<API::Object::Type::AuthenticationChallenge> {
public:
    static Ref<AuthenticationChallengeProxy> create(WebCore::AuthenticationChallenge&& authenticationChallenge, uint64_t challengeID, Ref<IPC::Connection>&& connection, WeakPtr<SecKeyProxyStore>&& secKeyProxyStore)
    {
        return adoptRef(*new AuthenticationChallengeProxy(WTFMove(authenticationChallenge), challengeID, WTFMove(connection), WTFMove(secKeyProxyStore)));
    }

    WebCredential* proposedCredential() const;
    WebProtectionSpace* protectionSpace() const;

    AuthenticationDecisionListener& listener() const { return m_listener.get(); }
    const WebCore::AuthenticationChallenge& core() { return m_coreAuthenticationChallenge; }

private:
    AuthenticationChallengeProxy(WebCore::AuthenticationChallenge&&, uint64_t challengeID, Ref<IPC::Connection>&&, WeakPtr<SecKeyProxyStore>&&);

#if HAVE(SEC_KEY_PROXY)
    static void sendClientCertificateCredentialOverXpc(IPC::Connection&, SecKeyProxyStore&, uint64_t challengeID, const WebCore::Credential&);
#endif

    WebCore::AuthenticationChallenge m_coreAuthenticationChallenge;
    mutable RefPtr<WebCredential> m_webCredential;
    mutable RefPtr<WebProtectionSpace> m_webProtectionSpace;
    Ref<AuthenticationDecisionListener> m_listener;
};

} // namespace WebKit
