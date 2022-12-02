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
#include "WebViewDidMoveToWindowObserver.h"
#include <wtf/CompletionHandler.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

// When the WebView, owner of the page, is not in the window, the session will then pause
// and later resume after the WebView being moved into the window.
// The reason to apply the above rule to the whole session instead of UI session only is UI
// can be shown out of process, in which case WebKit will not even get notified.
// FSM: Idle => isInWindow => Active => Completed
//      Idle => !isInWindow => Waiting => become isInWindow => Active => Completed
class NavigationSOAuthorizationSession : public SOAuthorizationSession, private WebViewDidMoveToWindowObserver {
public:
    ~NavigationSOAuthorizationSession();

protected:
    using Callback = CompletionHandler<void(bool)>;

    NavigationSOAuthorizationSession(RetainPtr<WKSOAuthorizationDelegate>, Ref<API::NavigationAction>&&, WebPageProxy&, InitiatingAction, Callback&&);

    void invokeCallback(bool intercepted) { m_callback(intercepted); }

private:
    // SOAuthorizationSession
    void shouldStartInternal() final;

    // WebViewDidMoveToWindowObserver
    void webViewDidMoveToWindow() final;

    virtual void beforeStart() = 0;

    bool pageActiveURLDidChangeDuringWaiting() const;

    Callback m_callback;
    String m_waitingPageActiveURL;
};

} // namespace WebKit

#endif
