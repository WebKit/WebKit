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
#include "WebAuthenticatorCoordinatorProxy.h"

#if ENABLE(WEB_AUTHN)

#include "AuthenticatorManager.h"
#include "LocalService.h"
#include "WebAuthenticatorCoordinatorMessages.h"
#include "WebAuthenticatorCoordinatorProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/PublicKeyCredentialData.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

namespace WebKit {

WebAuthenticatorCoordinatorProxy::WebAuthenticatorCoordinatorProxy(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
{
    m_webPageProxy.process().addMessageReceiver(Messages::WebAuthenticatorCoordinatorProxy::messageReceiverName(), m_webPageProxy.pageID(), *this);
}

WebAuthenticatorCoordinatorProxy::~WebAuthenticatorCoordinatorProxy()
{
    m_webPageProxy.process().removeMessageReceiver(Messages::WebAuthenticatorCoordinatorProxy::messageReceiverName(), m_webPageProxy.pageID());
}

void WebAuthenticatorCoordinatorProxy::makeCredential(const Vector<uint8_t>& hash, const WebCore::PublicKeyCredentialCreationOptions& options)
{
    auto callback = [weakThis = makeWeakPtr(*this)] (Variant<WebCore::PublicKeyCredentialData, WebCore::ExceptionData>&& result) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;

        WTF::switchOn(result, [&](const WebCore::PublicKeyCredentialData& data) {
            weakThis->requestReply(data, { });
        }, [&](const  WebCore::ExceptionData& exception) {
            weakThis->requestReply({ }, exception);
        });
    };
    m_webPageProxy.websiteDataStore().authenticatorManager().makeCredential(hash, options, WTFMove(callback));
}

void WebAuthenticatorCoordinatorProxy::getAssertion(const Vector<uint8_t>& hash, const WebCore::PublicKeyCredentialRequestOptions& options)
{
    auto callback = [weakThis = makeWeakPtr(*this)] (Variant<WebCore::PublicKeyCredentialData, WebCore::ExceptionData>&& result) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;

        WTF::switchOn(result, [&](const WebCore::PublicKeyCredentialData& data) {
            weakThis->requestReply(data, { });
        }, [&](const  WebCore::ExceptionData& exception) {
            weakThis->requestReply({ }, exception);
        });
    };
    m_webPageProxy.websiteDataStore().authenticatorManager().getAssertion(hash, options, WTFMove(callback));
}

void WebAuthenticatorCoordinatorProxy::isUserVerifyingPlatformAuthenticatorAvailable(uint64_t messageId)
{
    m_webPageProxy.send(Messages::WebAuthenticatorCoordinator::IsUserVerifyingPlatformAuthenticatorAvailableReply(messageId, LocalService::isAvailable()));
}

void WebAuthenticatorCoordinatorProxy::requestReply(const WebCore::PublicKeyCredentialData& data, const WebCore::ExceptionData& exception)
{
    m_webPageProxy.send(Messages::WebAuthenticatorCoordinator::RequestReply(data, exception));
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
