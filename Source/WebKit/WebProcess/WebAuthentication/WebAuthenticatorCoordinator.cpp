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
#include "WebAuthnConnectionToWebProcess.h"
#include "WebAuthnProcessConnection.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <WebCore/AuthenticatorAttachment.h>
#include <WebCore/AuthenticatorResponseData.h>
#include <WebCore/Frame.h>
#include <WebCore/PublicKeyCredentialCreationOptions.h>
#include <WebCore/PublicKeyCredentialRequestOptions.h>
#include <WebCore/Quirks.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/UserGestureIndicator.h>

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

void WebAuthenticatorCoordinator::makeCredential(const Frame& frame, const SecurityOrigin&, const Vector<uint8_t>& hash, const PublicKeyCredentialCreationOptions& options, RequestCompletionHandler&& handler)
{
    auto* webFrame = WebFrame::fromCoreFrame(frame);
    if (!webFrame)
        return;

    auto isProcessingUserGesture = processingUserGesture(frame, webFrame->frameID());
#if HAVE(UNIFIED_ASC_AUTH_UI)
    bool useWebAuthnProcess = false;
#else
    bool useWebAuthnProcess = RuntimeEnabledFeatures::sharedFeatures().webAuthenticationModernEnabled();
#endif
    if (!useWebAuthnProcess) {
        m_webPage.sendWithAsyncReply(Messages::WebAuthenticatorCoordinatorProxy::MakeCredential(webFrame->frameID(), webFrame->info(), hash, options, isProcessingUserGesture), WTFMove(handler));
        return;
    }

    if (!isWebBrowser())
        return;
    WebProcess::singleton().ensureWebAuthnProcessConnection().connection().sendWithAsyncReply(Messages::WebAuthnConnectionToWebProcess::MakeCredential(hash, options, isProcessingUserGesture), WTFMove(handler));
}

void WebAuthenticatorCoordinator::getAssertion(const Frame& frame, const SecurityOrigin&, const Vector<uint8_t>& hash, const PublicKeyCredentialRequestOptions& options, MediationRequirement mediation, RequestCompletionHandler&& handler)
{
    auto* webFrame = WebFrame::fromCoreFrame(frame);
    if (!webFrame)
        return;

    auto isProcessingUserGesture = processingUserGesture(frame, webFrame->frameID());
#if HAVE(UNIFIED_ASC_AUTH_UI)
    bool useWebAuthnProcess = false;
#else
    bool useWebAuthnProcess = RuntimeEnabledFeatures::sharedFeatures().webAuthenticationModernEnabled();
#endif
    if (!useWebAuthnProcess) {
        m_webPage.sendWithAsyncReply(Messages::WebAuthenticatorCoordinatorProxy::GetAssertion(webFrame->frameID(), webFrame->info(), hash, options, mediation, isProcessingUserGesture), WTFMove(handler));
        return;
    }

    if (!isWebBrowser())
        return;
    WebProcess::singleton().ensureWebAuthnProcessConnection().connection().sendWithAsyncReply(Messages::WebAuthnConnectionToWebProcess::GetAssertion(hash, options, isProcessingUserGesture), WTFMove(handler));
}

void WebAuthenticatorCoordinator::isConditionalMediationAvailable(QueryCompletionHandler&& handler)
{
    m_webPage.sendWithAsyncReply(Messages::WebAuthenticatorCoordinatorProxy::isConditionalMediationAvailable(), WTFMove(handler));
};

void WebAuthenticatorCoordinator::isUserVerifyingPlatformAuthenticatorAvailable(QueryCompletionHandler&& handler)
{
#if HAVE(UNIFIED_ASC_AUTH_UI)
    bool useWebAuthnProcess = false;
#else
    bool useWebAuthnProcess = RuntimeEnabledFeatures::sharedFeatures().webAuthenticationModernEnabled();
#endif
    if (!useWebAuthnProcess) {
        m_webPage.sendWithAsyncReply(Messages::WebAuthenticatorCoordinatorProxy::IsUserVerifyingPlatformAuthenticatorAvailable(), WTFMove(handler));
        return;
    }

    if (!isWebBrowser())
        return;
    WebProcess::singleton().ensureWebAuthnProcessConnection().connection().sendWithAsyncReply(Messages::WebAuthnConnectionToWebProcess::IsUserVerifyingPlatformAuthenticatorAvailable(), WTFMove(handler));
}

bool WebAuthenticatorCoordinator::processingUserGesture(const Frame& frame, const FrameIdentifier& frameID)
{
    auto processingUserGesture = UserGestureIndicator::processingUserGestureForMedia();
    bool processingUserGestureOrFreebie = processingUserGesture || !m_requireUserGesture;
    if (!processingUserGestureOrFreebie)
        m_webPage.addConsoleMessage(frameID, MessageSource::Other, MessageLevel::Warning, "User gesture is not detected. To use the WebAuthn API, call 'navigator.credentials.create' or 'navigator.credentials.get' within user activated events."_s);

    if (processingUserGesture && m_requireUserGesture)
        m_requireUserGesture = false;
    else if (!processingUserGesture)
        m_requireUserGesture = true;
    return processingUserGestureOrFreebie;
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
