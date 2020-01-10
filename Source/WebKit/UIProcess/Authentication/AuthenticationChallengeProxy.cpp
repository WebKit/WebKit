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

#include "config.h"
#include "AuthenticationChallengeProxy.h"

#include "AuthenticationChallengeDisposition.h"
#include "AuthenticationDecisionListener.h"
#include "AuthenticationManager.h"
#include "AuthenticationManagerMessages.h"
#include "AuxiliaryProcessProxy.h"
#include "WebCertificateInfo.h"
#include "WebCoreArgumentCoders.h"
#include "WebCredential.h"
#include "WebProcessProxy.h"
#include "WebProtectionSpace.h"

#if HAVE(SEC_KEY_PROXY)
#include "SecKeyProxyStore.h"
#endif

namespace WebKit {

AuthenticationChallengeProxy::AuthenticationChallengeProxy(WebCore::AuthenticationChallenge&& authenticationChallenge, uint64_t challengeID, Ref<IPC::Connection>&& connection, WeakPtr<SecKeyProxyStore>&& secKeyProxyStore)
    : m_coreAuthenticationChallenge(WTFMove(authenticationChallenge))
    , m_listener(AuthenticationDecisionListener::create(challengeID ? CompletionHandler<void(AuthenticationChallengeDisposition, const WebCore::Credential&)>([challengeID, connection = WTFMove(connection), secKeyProxyStore = WTFMove(secKeyProxyStore)](AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential) {
#if HAVE(SEC_KEY_PROXY)
        if (secKeyProxyStore && secKeyProxyStore->initialize(credential)) {
            sendClientCertificateCredentialOverXpc(connection, *secKeyProxyStore, challengeID, credential);
            return;
        }
#endif
        connection->send(Messages::AuthenticationManager::CompleteAuthenticationChallenge(challengeID, disposition, credential), 0);
    }) : nullptr))
{
}

WebCredential* AuthenticationChallengeProxy::proposedCredential() const
{
    if (!m_webCredential)
        m_webCredential = WebCredential::create(m_coreAuthenticationChallenge.proposedCredential());

    return m_webCredential.get();
}

WebProtectionSpace* AuthenticationChallengeProxy::protectionSpace() const
{
    if (!m_webProtectionSpace)
        m_webProtectionSpace = WebProtectionSpace::create(m_coreAuthenticationChallenge.protectionSpace());

    return m_webProtectionSpace.get();
}

} // namespace WebKit
