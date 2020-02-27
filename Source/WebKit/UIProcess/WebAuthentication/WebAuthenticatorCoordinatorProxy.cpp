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
#include "WebAuthenticationFlags.h"
#include "WebAuthenticatorCoordinatorProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/AuthenticatorResponseData.h>
#include <WebCore/ExceptionData.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

namespace WebKit {
using namespace WebCore;

WebAuthenticatorCoordinatorProxy::WebAuthenticatorCoordinatorProxy(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
{
    m_webPageProxy.process().addMessageReceiver(Messages::WebAuthenticatorCoordinatorProxy::messageReceiverName(), m_webPageProxy.webPageID(), *this);
}

WebAuthenticatorCoordinatorProxy::~WebAuthenticatorCoordinatorProxy()
{
    m_webPageProxy.process().removeMessageReceiver(Messages::WebAuthenticatorCoordinatorProxy::messageReceiverName(), m_webPageProxy.webPageID());
}

void WebAuthenticatorCoordinatorProxy::makeCredential(FrameIdentifier frameId, SecurityOriginData&& origin, Vector<uint8_t>&& hash, PublicKeyCredentialCreationOptions&& options, RequestCompletionHandler&& handler)
{
    handleRequest({ WTFMove(hash), WTFMove(options), makeWeakPtr(m_webPageProxy), WebAuthenticationPanelResult::Unavailable, nullptr, GlobalFrameIdentifier { m_webPageProxy.webPageID(), frameId }, WTFMove(origin) }, WTFMove(handler));
}

void WebAuthenticatorCoordinatorProxy::getAssertion(FrameIdentifier frameId, SecurityOriginData&& origin, Vector<uint8_t>&& hash, PublicKeyCredentialRequestOptions&& options, RequestCompletionHandler&& handler)
{
    handleRequest({ WTFMove(hash), WTFMove(options), makeWeakPtr(m_webPageProxy), WebAuthenticationPanelResult::Unavailable, nullptr, GlobalFrameIdentifier { m_webPageProxy.webPageID(), frameId }, WTFMove(origin) }, WTFMove(handler));
}

void WebAuthenticatorCoordinatorProxy::handleRequest(WebAuthenticationRequestData&& data, RequestCompletionHandler&& handler)
{
    auto callback = [handler = WTFMove(handler)] (Variant<Ref<AuthenticatorResponse>, ExceptionData>&& result) mutable {
        ASSERT(RunLoop::isMain());
        WTF::switchOn(result, [&](const Ref<AuthenticatorResponse>& response) {
            handler(response->data(), { });
        }, [&](const ExceptionData& exception) {
            handler({ }, exception);
        });
    };
    m_webPageProxy.websiteDataStore().authenticatorManager().handleRequest(WTFMove(data), WTFMove(callback));
}

void WebAuthenticatorCoordinatorProxy::isUserVerifyingPlatformAuthenticatorAvailable(QueryCompletionHandler&& handler)
{
    handler(LocalService::isAvailable());
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
