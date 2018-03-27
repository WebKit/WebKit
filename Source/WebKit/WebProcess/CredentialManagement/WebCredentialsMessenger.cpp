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
#include "WebCredentialsMessenger.h"

#if ENABLE(WEB_AUTHN)

#include "WebCredentialsMessengerMessages.h"
#include "WebCredentialsMessengerProxyMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/PublicKeyCredentialCreationOptions.h>
#include <WebCore/PublicKeyCredentialRequestOptions.h>

namespace WebKit {

WebCredentialsMessenger::WebCredentialsMessenger(WebPage& webPage)
    : m_webPage(webPage)
{
    WebProcess::singleton().addMessageReceiver(Messages::WebCredentialsMessenger::messageReceiverName(), m_webPage.pageID(), *this);
}

WebCredentialsMessenger::~WebCredentialsMessenger()
{
    WebProcess::singleton().removeMessageReceiver(*this);
}

void WebCredentialsMessenger::makeCredential(const Vector<uint8_t>& hash, const WebCore::PublicKeyCredentialCreationOptions& options, WebCore::CreationCompletionHandler&& handler)
{
    auto messageId = addCreationCompletionHandler(WTFMove(handler));
    m_webPage.send(Messages::WebCredentialsMessengerProxy::MakeCredential(messageId, hash, options));
}

void WebCredentialsMessenger::getAssertion(const Vector<uint8_t>& hash, const WebCore::PublicKeyCredentialRequestOptions& options, WebCore::RequestCompletionHandler&& handler)
{
    auto messageId = addRequestCompletionHandler(WTFMove(handler));
    m_webPage.send(Messages::WebCredentialsMessengerProxy::GetAssertion(messageId, hash, options));
}

void WebCredentialsMessenger::isUserVerifyingPlatformAuthenticatorAvailable(WebCore::QueryCompletionHandler&& handler)
{
    auto messageId = addQueryCompletionHandler(WTFMove(handler));
    m_webPage.send(Messages::WebCredentialsMessengerProxy::IsUserVerifyingPlatformAuthenticatorAvailable(messageId));
}

void WebCredentialsMessenger::makeCredentialReply(uint64_t messageId, const Vector<uint8_t>& credentialId, const Vector<uint8_t>& attestationObject)
{
    auto handler = takeCreationCompletionHandler(messageId);
    handler(WebCore::CreationReturnBundle(ArrayBuffer::create(credentialId.data(), credentialId.size()), ArrayBuffer::create(attestationObject.data(), attestationObject.size())));
}

void WebCredentialsMessenger::getAssertionReply(uint64_t messageId, const Vector<uint8_t>& credentialId, const Vector<uint8_t>& authenticatorData, const Vector<uint8_t>& signature, const Vector<uint8_t>& userHandle)
{
    auto handler = takeRequestCompletionHandler(messageId);
    handler(WebCore::AssertionReturnBundle(ArrayBuffer::create(credentialId.data(), credentialId.size()), ArrayBuffer::create(authenticatorData.data(), authenticatorData.size()), ArrayBuffer::create(signature.data(), signature.size()), ArrayBuffer::create(userHandle.data(), userHandle.size())));
}

void WebCredentialsMessenger::isUserVerifyingPlatformAuthenticatorAvailableReply(uint64_t messageId, bool result)
{
    auto handler = takeQueryCompletionHandler(messageId);
    handler(result);
}


} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
