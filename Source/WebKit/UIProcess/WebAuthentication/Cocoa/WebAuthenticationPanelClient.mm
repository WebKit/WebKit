/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebAuthenticationPanelClient.h"

#if ENABLE(WEB_AUTHN)

#import "APIArray.h"
#import "APIWebAuthenticationAssertionResponse.h"
#import "CompletionHandlerCallChecker.h"
#import "WKNSArray.h"
#import "_WKWebAuthenticationAssertionResponseInternal.h"
#import <WebKit/_WKWebAuthenticationPanel.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>

#import "LocalAuthenticationSoftLink.h"

namespace WebKit {

WebAuthenticationPanelClient::WebAuthenticationPanelClient(_WKWebAuthenticationPanel *panel, id <_WKWebAuthenticationPanelDelegate> delegate)
    : m_panel(panel)
    , m_delegate(delegate)
{
    m_delegateMethods.panelUpdateWebAuthenticationPanel = [delegate respondsToSelector:@selector(panel:updateWebAuthenticationPanel:)];
    m_delegateMethods.panelDismissWebAuthenticationPanelWithResult = [delegate respondsToSelector:@selector(panel:dismissWebAuthenticationPanelWithResult:)];
    m_delegateMethods.panelRequestPinWithRemainingRetriesCompletionHandler = [delegate respondsToSelector:@selector(panel:requestPINWithRemainingRetries:completionHandler:)];
    m_delegateMethods.panelSelectAssertionResponseSourceCompletionHandler = [delegate respondsToSelector:@selector(panel:selectAssertionResponse:source:completionHandler:)];
    m_delegateMethods.panelDecidePolicyForLocalAuthenticatorCompletionHandler = [delegate respondsToSelector:@selector(panel:decidePolicyForLocalAuthenticatorWithCompletionHandler:)];
    m_delegateMethods.panelRequestLAContextForUserVerificationCompletionHandler = [delegate respondsToSelector:@selector(panel:requestLAContextForUserVerificationWithCompletionHandler:)];
}

RetainPtr<id <_WKWebAuthenticationPanelDelegate> > WebAuthenticationPanelClient::delegate()
{
    return m_delegate.get();
}

static _WKWebAuthenticationPanelUpdate wkWebAuthenticationPanelUpdate(WebAuthenticationStatus status)
{
    if (status == WebAuthenticationStatus::MultipleNFCTagsPresent)
        return _WKWebAuthenticationPanelUpdateMultipleNFCTagsPresent;
    if (status == WebAuthenticationStatus::NoCredentialsFound)
        return _WKWebAuthenticationPanelUpdateNoCredentialsFound;
    if (status == WebAuthenticationStatus::PinBlocked)
        return _WKWebAuthenticationPanelUpdatePINBlocked;
    if (status == WebAuthenticationStatus::PinAuthBlocked)
        return _WKWebAuthenticationPanelUpdatePINAuthBlocked;
    if (status == WebAuthenticationStatus::PinInvalid)
        return _WKWebAuthenticationPanelUpdatePINInvalid;
    if (status == WebAuthenticationStatus::LAError)
        return _WKWebAuthenticationPanelUpdateLAError;
    if (status == WebAuthenticationStatus::LAExcludeCredentialsMatched)
        return _WKWebAuthenticationPanelUpdateLAExcludeCredentialsMatched;
    if (status == WebAuthenticationStatus::LANoCredential)
        return _WKWebAuthenticationPanelUpdateLANoCredential;
    ASSERT_NOT_REACHED();
    return _WKWebAuthenticationPanelUpdateMultipleNFCTagsPresent;
}

void WebAuthenticationPanelClient::updatePanel(WebAuthenticationStatus status) const
{
    if (!m_delegateMethods.panelUpdateWebAuthenticationPanel)
        return;

    auto delegate = m_delegate.get();
    if (!delegate)
        return;

    [delegate panel:m_panel updateWebAuthenticationPanel:wkWebAuthenticationPanelUpdate(status)];
}

static _WKWebAuthenticationResult wkWebAuthenticationResult(WebAuthenticationResult result)
{
    switch (result) {
    case WebAuthenticationResult::Succeeded:
        return _WKWebAuthenticationResultSucceeded;
    case WebAuthenticationResult::Failed:
        return _WKWebAuthenticationResultFailed;
    }
    ASSERT_NOT_REACHED();
    return _WKWebAuthenticationResultFailed;
}

void WebAuthenticationPanelClient::dismissPanel(WebAuthenticationResult result) const
{
    if (!m_delegateMethods.panelDismissWebAuthenticationPanelWithResult)
        return;

    auto delegate = m_delegate.get();
    if (!delegate)
        return;

    [delegate panel:m_panel dismissWebAuthenticationPanelWithResult:wkWebAuthenticationResult(result)];
}

void WebAuthenticationPanelClient::requestPin(uint64_t retries, CompletionHandler<void(const WTF::String&)>&& completionHandler) const
{
    if (!m_delegateMethods.panelRequestPinWithRemainingRetriesCompletionHandler) {
        completionHandler(String());
        return;
    }

    auto delegate = m_delegate.get();
    if (!delegate) {
        completionHandler(String());
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(panel:requestPINWithRemainingRetries:completionHandler:));
    [delegate panel:m_panel requestPINWithRemainingRetries:retries completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](NSString *pin) mutable {
        ensureOnMainThread([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker), pin = retainPtr(pin)] () mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(pin.get());
        });
    }).get()];
}

static _WKWebAuthenticationSource wkWebAuthenticationSource(WebAuthenticationSource result)
{
    switch (result) {
    case WebAuthenticationSource::Local:
        return _WKWebAuthenticationSourceLocal;
    case WebAuthenticationSource::External:
        return _WKWebAuthenticationSourceExternal;
    }
    ASSERT_NOT_REACHED();
    return _WKWebAuthenticationSourceLocal;
}

void WebAuthenticationPanelClient::selectAssertionResponse(Vector<Ref<WebCore::AuthenticatorAssertionResponse>>&& responses, WebAuthenticationSource source, CompletionHandler<void(WebCore::AuthenticatorAssertionResponse*)>&& completionHandler) const
{
    ASSERT(!responses.isEmpty());

    if (!m_delegateMethods.panelSelectAssertionResponseSourceCompletionHandler) {
        completionHandler(nullptr);
        return;
    }

    auto delegate = m_delegate.get();
    if (!delegate) {
        completionHandler(nullptr);
        return;
    }

    auto apiResponses = responses.map([](auto& response) -> RefPtr<API::Object> {
        return API::WebAuthenticationAssertionResponse::create(response.copyRef());
    });
    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(panel:selectAssertionResponse:source:completionHandler:));
    [delegate panel:m_panel selectAssertionResponse:wrapper(API::Array::create(WTFMove(apiResponses))) source:wkWebAuthenticationSource(source) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](_WKWebAuthenticationAssertionResponse *response) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();

        if (!response) {
            completionHandler(nullptr);
            return;
        }
        completionHandler(static_cast<API::WebAuthenticationAssertionResponse&>([response _apiObject]).response());
    }).get()];
}

static LocalAuthenticatorPolicy localAuthenticatorPolicy(_WKLocalAuthenticatorPolicy result)
{
    switch (result) {
    case _WKLocalAuthenticatorPolicyAllow:
        return LocalAuthenticatorPolicy::Allow;
    case _WKLocalAuthenticatorPolicyDisallow:
        return LocalAuthenticatorPolicy::Disallow;
    }
    ASSERT_NOT_REACHED();
    return LocalAuthenticatorPolicy::Allow;
}

void WebAuthenticationPanelClient::decidePolicyForLocalAuthenticator(CompletionHandler<void(LocalAuthenticatorPolicy)>&& completionHandler) const
{
    if (!m_delegateMethods.panelDecidePolicyForLocalAuthenticatorCompletionHandler) {
        completionHandler(LocalAuthenticatorPolicy::Disallow);
        return;
    }

    auto delegate = m_delegate.get();
    if (!delegate) {
        completionHandler(LocalAuthenticatorPolicy::Disallow);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(panel:decidePolicyForLocalAuthenticatorWithCompletionHandler:));
    [delegate panel:m_panel decidePolicyForLocalAuthenticatorWithCompletionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](_WKLocalAuthenticatorPolicy policy) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(localAuthenticatorPolicy(policy));
    }).get()];
}

void WebAuthenticationPanelClient::requestLAContextForUserVerification(CompletionHandler<void(LAContext *)>&& completionHandler) const
{
    if (!m_delegateMethods.panelRequestLAContextForUserVerificationCompletionHandler) {
        completionHandler(nullptr);
        return;
    }

    auto delegate = m_delegate.get();
    if (!delegate) {
        completionHandler(nullptr);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(panel:requestLAContextForUserVerificationWithCompletionHandler:));
    [delegate panel:m_panel requestLAContextForUserVerificationWithCompletionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](LAContext *context) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(context);
    }).get()];
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
