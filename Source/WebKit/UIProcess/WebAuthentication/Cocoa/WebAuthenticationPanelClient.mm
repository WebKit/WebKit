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
#import "WebAuthenticationFlags.h"
#import "_WKWebAuthenticationAssertionResponseInternal.h"
#import "_WKWebAuthenticationPanel.h"
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
    m_delegateMethods.panelSelectAssertionResponseCompletionHandler = [delegate respondsToSelector:@selector(panel:selectAssertionResponse:completionHandler:)];
    m_delegateMethods.panelVerifyUserWithAccessControlCompletionHandler = [delegate respondsToSelector:@selector(panel:verifyUserWithAccessControl:completionHandler:)];
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
        completionHandler(emptyString());
        return;
    }

    auto delegate = m_delegate.get();
    if (!delegate) {
        completionHandler(emptyString());
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(panel:requestPINWithRemainingRetries:completionHandler:));
    [delegate panel:m_panel requestPINWithRemainingRetries:retries completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](NSString *pin) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(pin);
    }).get()];
}

void WebAuthenticationPanelClient::selectAssertionResponse(Vector<Ref<WebCore::AuthenticatorAssertionResponse>>&& responses, CompletionHandler<void(const WebCore::AuthenticatorAssertionResponse&)>&& completionHandler) const
{
    ASSERT(!responses.isEmpty());

    if (!m_delegateMethods.panelSelectAssertionResponseCompletionHandler) {
        completionHandler(responses[0]);
        return;
    }

    auto delegate = m_delegate.get();
    if (!delegate) {
        completionHandler(responses[0]);
        return;
    }

    Vector<RefPtr<API::Object>> apiResponses;
    apiResponses.reserveInitialCapacity(responses.size());
    for (auto& response : responses)
        apiResponses.uncheckedAppend(API::WebAuthenticationAssertionResponse::create(response.copyRef()));

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(panel:selectAssertionResponse:completionHandler:));
    [delegate panel:m_panel selectAssertionResponse:wrapper(API::Array::create(WTFMove(apiResponses))) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](_WKWebAuthenticationAssertionResponse *response) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(static_cast<API::WebAuthenticationAssertionResponse&>([response _apiObject]).response());
    }).get()];
}

void WebAuthenticationPanelClient::verifyUser(SecAccessControlRef accessControl, CompletionHandler<void(LAContext *)>&& completionHandler) const
{
    if (!m_delegateMethods.panelVerifyUserWithAccessControlCompletionHandler) {
        completionHandler(adoptNS([allocLAContextInstance() init]).get());
        return;
    }

    auto delegate = m_delegate.get();
    if (!delegate) {
        completionHandler(adoptNS([allocLAContextInstance() init]).get());
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(panel:verifyUserWithAccessControl:completionHandler:));
    [delegate panel:m_panel verifyUserWithAccessControl:accessControl completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](LAContext *context) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(context);
    }).get()];
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
