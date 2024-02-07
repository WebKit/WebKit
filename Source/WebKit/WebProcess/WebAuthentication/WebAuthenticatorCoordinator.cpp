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

#include "DefaultWebBrowserChecks.h"
#include "FrameInfoData.h"
#include "WebAuthenticatorCoordinatorProxyMessages.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <WebCore/AuthenticatorAttachment.h>
#include <WebCore/AuthenticatorResponseData.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/MediationRequirement.h>
#include <WebCore/PublicKeyCredentialCreationOptions.h>
#include <WebCore/PublicKeyCredentialRequestOptions.h>
#include <WebCore/Quirks.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/UserGestureIndicator.h>
#include <WebCore/WebAuthenticationConstants.h>

#undef WEBAUTHN_RELEASE_LOG
#define PAGE_ID (m_webPage.identifier().toUInt64())
#define FRAME_ID (webFrame->frameID().object().toUInt64())
#define WEBAUTHN_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(WebAuthn, "%p - [webPageID=%" PRIu64 ", webFrameID=%" PRIu64 "] WebAuthenticatorCoordinator::" fmt, this, PAGE_ID, FRAME_ID, ##__VA_ARGS__)
#define WEBAUTHN_RELEASE_LOG_ERROR_NO_FRAME(fmt, ...) RELEASE_LOG_ERROR(WebAuthn, "%p - [webPageID=%" PRIu64 "] WebAuthenticatorCoordinator::" fmt, this, PAGE_ID, ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

namespace {
inline bool isWebBrowser()
{
    return isParentProcessAFullWebBrowser(WebProcess::singleton());
}
}

WebAuthenticatorCoordinator::WebAuthenticatorCoordinator(WebPage& webPage)
    : m_webPage(webPage)
{
}

void WebAuthenticatorCoordinator::makeCredential(const LocalFrame& frame, const PublicKeyCredentialCreationOptions& options, MediationRequirement mediation, RequestCompletionHandler&& handler)
{
    auto webFrame = WebFrame::fromCoreFrame(frame);
    if (!webFrame)
        return;

    m_webPage.sendWithAsyncReply(Messages::WebAuthenticatorCoordinatorProxy::MakeCredential(webFrame->frameID(), webFrame->info(), options), WTFMove(handler));
}

void WebAuthenticatorCoordinator::getAssertion(const LocalFrame& frame, const PublicKeyCredentialRequestOptions& options, MediationRequirement mediation, const ScopeAndCrossOriginParent& scopeAndCrossOriginParent, RequestCompletionHandler&& handler)
{
    auto webFrame = WebFrame::fromCoreFrame(frame);
    if (!webFrame)
        return;

    m_webPage.sendWithAsyncReply(Messages::WebAuthenticatorCoordinatorProxy::GetAssertion(webFrame->frameID(), webFrame->info(), options, mediation, scopeAndCrossOriginParent.second), WTFMove(handler));
}

void WebAuthenticatorCoordinator::isConditionalMediationAvailable(const SecurityOrigin& origin, QueryCompletionHandler&& handler)
{
    m_webPage.sendWithAsyncReply(Messages::WebAuthenticatorCoordinatorProxy::isConditionalMediationAvailable(origin.data()), WTFMove(handler));
};

void WebAuthenticatorCoordinator::isUserVerifyingPlatformAuthenticatorAvailable(const SecurityOrigin& origin, QueryCompletionHandler&& handler)
{
    m_webPage.sendWithAsyncReply(Messages::WebAuthenticatorCoordinatorProxy::IsUserVerifyingPlatformAuthenticatorAvailable(origin.data()), WTFMove(handler));
}

void WebAuthenticatorCoordinator::cancel(CompletionHandler<void()>&& handler)
{
    m_webPage.sendWithAsyncReply(Messages::WebAuthenticatorCoordinatorProxy::Cancel(), WTFMove(handler));
}

void WebAuthenticatorCoordinator::getClientCapabilities(const SecurityOrigin& origin, CapabilitiesCompletionHandler&& handler)
{
    m_webPage.sendWithAsyncReply(Messages::WebAuthenticatorCoordinatorProxy::GetClientCapabilities(origin.data()), WTFMove(handler));
}

} // namespace WebKit

#undef WEBAUTHN_RELEASE_LOG_ERROR_NO_FRAME
#undef WEBAUTHN_RELEASE_LOG_ERROR
#undef FRAME_ID
#undef PAGE_ID

#endif // ENABLE(WEB_AUTHN)
