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

#include "config.h"
#include "WebCredentialsMessengerProxy.h"

#if ENABLE(WEB_AUTHN)

#include "WebCredentialsMessengerMessages.h"
#include "WebCredentialsMessengerProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/LocalAuthenticator.h>

namespace WebKit {

WebCredentialsMessengerProxy::WebCredentialsMessengerProxy(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
{
    m_webPageProxy.process().addMessageReceiver(Messages::WebCredentialsMessengerProxy::messageReceiverName(), m_webPageProxy.pageID(), *this);
    m_authenticator = std::make_unique<WebCore::LocalAuthenticator>();
}

WebCredentialsMessengerProxy::~WebCredentialsMessengerProxy()
{
    m_webPageProxy.process().removeMessageReceiver(Messages::WebCredentialsMessengerProxy::messageReceiverName(), m_webPageProxy.pageID());
}

void WebCredentialsMessengerProxy::makeCredential(uint64_t messageId, const Vector<uint8_t>& hash, const WebCore::PublicKeyCredentialCreationOptions& options)
{
    // FIXME(182767)
    if (!m_authenticator) {
        exceptionReply(messageId, { WebCore::NotAllowedError, ASCIILiteral("No avaliable authenticators.") });
        return;
    }
    // FIXME(183534): Weak pointers doesn't work in another thread because of race condition.
    // FIXME(183534): Unify callbacks.
    auto weakThis = m_weakFactory.createWeakPtr(*this);
    auto callback = [weakThis, messageId] (const Vector<uint8_t>& credentialId, const Vector<uint8_t>& attestationObject) {
        if (!weakThis)
            return;
        weakThis->makeCredentialReply(messageId, credentialId, attestationObject);
    };
    auto exceptionCallback = [weakThis, messageId] (const WebCore::ExceptionData& exception) {
        if (!weakThis)
            return;
        weakThis->exceptionReply(messageId, exception);
    };
    m_authenticator->makeCredential(hash, options, WTFMove(callback), WTFMove(exceptionCallback));
}

void WebCredentialsMessengerProxy::getAssertion(uint64_t messageId, const Vector<uint8_t>& hash, const WebCore::PublicKeyCredentialRequestOptions& options)
{
    // FIXME(182767)
    if (!m_authenticator)
        exceptionReply(messageId, { WebCore::NotAllowedError, ASCIILiteral("No avaliable authenticators.") });
    // FIXME(183534): Weak pointers doesn't work in another thread because of race condition.
    // FIXME(183534): Unify callbacks.
    auto weakThis = m_weakFactory.createWeakPtr(*this);
    auto callback = [weakThis, messageId] (const Vector<uint8_t>& credentialId, const Vector<uint8_t>& authenticatorData, const Vector<uint8_t>& signature, const Vector<uint8_t>& userHandle) {
        if (weakThis)
            weakThis->getAssertionReply(messageId, credentialId, authenticatorData, signature, userHandle);
    };
    auto exceptionCallback = [weakThis, messageId] (const WebCore::ExceptionData& exception) {
        if (weakThis)
            weakThis->exceptionReply(messageId, exception);
    };
    m_authenticator->getAssertion(hash, options, WTFMove(callback), WTFMove(exceptionCallback));
}

void WebCredentialsMessengerProxy::isUserVerifyingPlatformAuthenticatorAvailable(uint64_t messageId)
{
    if (!m_authenticator) {
        isUserVerifyingPlatformAuthenticatorAvailableReply(messageId, false);
        return;
    }
    isUserVerifyingPlatformAuthenticatorAvailableReply(messageId, m_authenticator->isAvailable());
}

void WebCredentialsMessengerProxy::exceptionReply(uint64_t messageId, const WebCore::ExceptionData& exception)
{
    m_webPageProxy.send(Messages::WebCredentialsMessenger::ExceptionReply(messageId, exception));
}

void WebCredentialsMessengerProxy::makeCredentialReply(uint64_t messageId, const Vector<uint8_t>& credentialId, const Vector<uint8_t>& attestationObject)
{
    m_webPageProxy.send(Messages::WebCredentialsMessenger::MakeCredentialReply(messageId, credentialId, attestationObject));
}

void WebCredentialsMessengerProxy::getAssertionReply(uint64_t messageId, const Vector<uint8_t>& credentialId, const Vector<uint8_t>& authenticatorData, const Vector<uint8_t>& signature, const Vector<uint8_t>& userHandle)
{
    m_webPageProxy.send(Messages::WebCredentialsMessenger::GetAssertionReply(messageId, credentialId, authenticatorData, signature, userHandle));
}

void WebCredentialsMessengerProxy::isUserVerifyingPlatformAuthenticatorAvailableReply(uint64_t messageId, bool result)
{
    m_webPageProxy.send(Messages::WebCredentialsMessenger::IsUserVerifyingPlatformAuthenticatorAvailableReply(messageId, result));
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
