/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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

#if HAVE(APP_SSO)

#include "SOAuthorizationSession.h"
#include <wtf/CompletionHandler.h>

OBJC_CLASS WKSOSecretDelegate;
OBJC_CLASS WKWebView;

namespace API {
class NavigationAction;
}

namespace WebKit {

// FSM: Idle => Active => Completed
class PopUpSOAuthorizationSession final : public SOAuthorizationSession {
public:
    using NewPageCallback = CompletionHandler<void(RefPtr<WebPageProxy>&&)>;
    using UIClientCallback = Function<void(Ref<API::NavigationAction>&&, NewPageCallback&&)>;

    static Ref<SOAuthorizationSession> create(RetainPtr<WKSOAuthorizationDelegate>, WebPageProxy&, Ref<API::NavigationAction>&&, NewPageCallback&&, UIClientCallback&&);
    ~PopUpSOAuthorizationSession();

    void close(WKWebView *);

private:
    PopUpSOAuthorizationSession(RetainPtr<WKSOAuthorizationDelegate>, WebPageProxy&, Ref<API::NavigationAction>&&, NewPageCallback&&, UIClientCallback&&);

    void shouldStartInternal() final;
    void fallBackToWebPathInternal() final;
    void abortInternal() final;
    void completeInternal(const WebCore::ResourceResponse&, NSData *) final;

    void initSecretWebView();

    NewPageCallback m_newPageCallback;
    UIClientCallback m_uiClientCallback;

    RetainPtr<WKSOSecretDelegate> m_secretDelegate;
    RetainPtr<WKWebView> m_secretWebView;
};

} // namespace WebKit

#endif
