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

#pragma once

#if ENABLE(WEB_AUTHN)

#import "WKFoundation.h"

#import "APIWebAuthenticationPanelClient.h"
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WeakPtr.h>

@class _WKWebAuthenticationPanel;
@protocol _WKWebAuthenticationPanelDelegate;

namespace WebKit {

class WebAuthenticationPanelClient final : public API::WebAuthenticationPanelClient, public CanMakeWeakPtr<WebAuthenticationPanelClient> {
public:
    WebAuthenticationPanelClient(_WKWebAuthenticationPanel *, id <_WKWebAuthenticationPanelDelegate>);

    RetainPtr<id <_WKWebAuthenticationPanelDelegate> > delegate();

private:
    // API::WebAuthenticationPanelClient
    void updatePanel(WebAuthenticationStatus) const final;
    void dismissPanel(WebAuthenticationResult) const final;
    void requestPin(uint64_t, CompletionHandler<void(const WTF::String&)>&&) const final;
    void selectAssertionResponse(Vector<Ref<WebCore::AuthenticatorAssertionResponse>>&&, WebAuthenticationSource, CompletionHandler<void(WebCore::AuthenticatorAssertionResponse*)>&&) const final;
    void decidePolicyForLocalAuthenticator(CompletionHandler<void(LocalAuthenticatorPolicy)>&&) const final;

    _WKWebAuthenticationPanel *m_panel;
    WeakObjCPtr<id <_WKWebAuthenticationPanelDelegate> > m_delegate;

    struct {
        bool panelUpdateWebAuthenticationPanel : 1;
        bool panelDismissWebAuthenticationPanelWithResult : 1;
        bool panelRequestPinWithRemainingRetriesCompletionHandler : 1;
        bool panelSelectAssertionResponseSourceCompletionHandler : 1;
        bool panelDecidePolicyForLocalAuthenticatorCompletionHandler : 1;
    } m_delegateMethods;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
