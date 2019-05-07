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
#include "WebAuthenticatorCoordinator.h"

#if ENABLE(WEB_AUTHN)

#include "WebAuthenticatorCoordinatorMessages.h"
#include "WebAuthenticatorCoordinatorProxyMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/PublicKeyCredentialCreationOptions.h>
#include <WebCore/PublicKeyCredentialRequestOptions.h>

namespace WebKit {

WebAuthenticatorCoordinator::WebAuthenticatorCoordinator(WebPage& webPage)
    : m_webPage(webPage)
{
    WebProcess::singleton().addMessageReceiver(Messages::WebAuthenticatorCoordinator::messageReceiverName(), m_webPage.pageID(), *this);
}

WebAuthenticatorCoordinator::~WebAuthenticatorCoordinator()
{
    WebProcess::singleton().removeMessageReceiver(*this);
}

void WebAuthenticatorCoordinator::makeCredential(const Vector<uint8_t>& hash, const WebCore::PublicKeyCredentialCreationOptions& options, WebCore::RequestCompletionHandler&& handler)
{
    auto messageId = setRequestCompletionHandler(WTFMove(handler));
    m_webPage.send(Messages::WebAuthenticatorCoordinatorProxy::MakeCredential(messageId, hash, options));
}

void WebAuthenticatorCoordinator::getAssertion(const Vector<uint8_t>& hash, const WebCore::PublicKeyCredentialRequestOptions& options, WebCore::RequestCompletionHandler&& handler)
{
    auto messageId = setRequestCompletionHandler(WTFMove(handler));
    m_webPage.send(Messages::WebAuthenticatorCoordinatorProxy::GetAssertion(messageId, hash, options));
}

void WebAuthenticatorCoordinator::isUserVerifyingPlatformAuthenticatorAvailable(WebCore::QueryCompletionHandler&& handler)
{
    auto messageId = addQueryCompletionHandler(WTFMove(handler));
    m_webPage.send(Messages::WebAuthenticatorCoordinatorProxy::IsUserVerifyingPlatformAuthenticatorAvailable(messageId));
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
