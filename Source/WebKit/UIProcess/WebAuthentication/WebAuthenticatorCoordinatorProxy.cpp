/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#include "APIUIClient.h"
#include "AuthenticatorManager.h"
#include "LocalService.h"
#include "Logging.h"
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
#if HAVE(UNIFIED_ASC_AUTH_UI)
    cancel();
#endif // HAVE(UNIFIED_ASC_AUTH_UI)
    m_webPageProxy.process().removeMessageReceiver(Messages::WebAuthenticatorCoordinatorProxy::messageReceiverName(), m_webPageProxy.webPageID());
}

void WebAuthenticatorCoordinatorProxy::makeCredential(FrameIdentifier frameId, FrameInfoData&& frameInfo, Vector<uint8_t>&& hash, PublicKeyCredentialCreationOptions&& options, bool processingUserGesture, RequestCompletionHandler&& handler)
{
    handleRequest({ WTFMove(hash), WTFMove(options), m_webPageProxy, WebAuthenticationPanelResult::Unavailable, nullptr, GlobalFrameIdentifier { m_webPageProxy.webPageID(), frameId }, WTFMove(frameInfo), processingUserGesture, String(), nullptr, std::nullopt, std::nullopt }, WTFMove(handler));
}

void WebAuthenticatorCoordinatorProxy::getAssertion(FrameIdentifier frameId, FrameInfoData&& frameInfo, Vector<uint8_t>&& hash, PublicKeyCredentialRequestOptions&& options, MediationRequirement mediation, std::optional<WebCore::SecurityOriginData> parentOrigin, bool processingUserGesture, RequestCompletionHandler&& handler)
{
    handleRequest({ WTFMove(hash), WTFMove(options), m_webPageProxy, WebAuthenticationPanelResult::Unavailable, nullptr, GlobalFrameIdentifier { m_webPageProxy.webPageID(), frameId }, WTFMove(frameInfo), processingUserGesture, String(), nullptr, mediation, parentOrigin }, WTFMove(handler));
}

void WebAuthenticatorCoordinatorProxy::handleRequest(WebAuthenticationRequestData&& data, RequestCompletionHandler&& handler)
{
    auto origin = API::SecurityOrigin::create(data.frameInfo.securityOrigin.protocol, data.frameInfo.securityOrigin.host, data.frameInfo.securityOrigin.port);

    CompletionHandler<void(bool)> afterConsent = [this, data = WTFMove(data), handler = WTFMove(handler)] (bool result) mutable {
        auto& authenticatorManager = m_webPageProxy.websiteDataStore().authenticatorManager();
        if (result) {
#if HAVE(UNIFIED_ASC_AUTH_UI)
            if (!authenticatorManager.isMock() && !authenticatorManager.isVirtual()) {
                auto context = contextForRequest(WTFMove(data));
                if (context.get() == nullptr) {
                    handler({ }, (AuthenticatorAttachment)0, ExceptionData { NotAllowedError, "The origin of the document is not the same as its ancestors."_s });
                    RELEASE_LOG_ERROR(WebAuthn, "The origin of the document is not the same as its ancestors.");
                    return;
                }
                // performRequest calls out to ASCAgent which will then call [_WKWebAuthenticationPanel makeCredential/getAssertionWithChallenge]
                // which calls authenticatorManager.handleRequest(..)
                performRequest(context, WTFMove(handler));
                return;
            }
#else
            if (data.parentOrigin && !authenticatorManager.isMock() && !authenticatorManager.isVirtual()) {
                handler({ }, (AuthenticatorAttachment)0, ExceptionData { NotAllowedError, "The origin of the document is not the same as its ancestors."_s });
                RELEASE_LOG_ERROR(WebAuthn, "The origin of the document is not the same as its ancestors.");
                return;
            }
#endif // HAVE(UNIFIED_ASC_AUTH_UI)

            authenticatorManager.handleRequest(WTFMove(data), [handler = WTFMove(handler)] (std::variant<Ref<AuthenticatorResponse>, ExceptionData>&& result) mutable {
                ASSERT(RunLoop::isMain());
                WTF::switchOn(result, [&](const Ref<AuthenticatorResponse>& response) {
                    handler(response->data(), response->attachment(), { });
                }, [&](const ExceptionData& exception) {
                    handler({ }, (AuthenticatorAttachment)0, exception);
                });
            });
        } else {
            handler({ }, (AuthenticatorAttachment)0, ExceptionData { NotAllowedError, "This request has been cancelled by the user."_s });
            RELEASE_LOG_ERROR(WebAuthn, "Request cancelled due to rejected prompt after lack of user gesture.");
        }
    };
    
    if (!data.processingUserGesture && data.mediation != MediationRequirement::Conditional && !m_webPageProxy.websiteDataStore().authenticatorManager().isVirtual())
        m_webPageProxy.uiClient().requestWebAuthenticationNoGesture(origin, WTFMove(afterConsent));
    else
        afterConsent(true);
}

#if !HAVE(UNIFIED_ASC_AUTH_UI)
void WebAuthenticatorCoordinatorProxy::cancel()
{
}

void WebAuthenticatorCoordinatorProxy::isUserVerifyingPlatformAuthenticatorAvailable(const SecurityOriginData&, QueryCompletionHandler&& handler)
{
    handler(LocalService::isAvailable());
}

void WebAuthenticatorCoordinatorProxy::isConditionalMediationAvailable(const SecurityOriginData&, QueryCompletionHandler&& handler)
{
    handler(false);
}
#endif // !HAVE(UNIFIED_ASC_AUTH_UI)

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
