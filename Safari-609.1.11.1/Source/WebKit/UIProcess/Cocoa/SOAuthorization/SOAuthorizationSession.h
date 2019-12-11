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

#if HAVE(APP_SSO)

#include <pal/spi/cocoa/AppSSOSPI.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakObjCPtr.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS SOAuthorization;

namespace API {
class NavigationAction;
}

namespace WebCore {
class ResourceResponse;
}

namespace WebKit {

class WebPageProxy;

// A session will only be executed once.
class SOAuthorizationSession : public RefCounted<SOAuthorizationSession>, public CanMakeWeakPtr<SOAuthorizationSession> {
public:
    enum class InitiatingAction : uint8_t {
        Redirect,
        PopUp,
        SubFrame
    };

    using UICallback = void (^)(BOOL, NSError *);

    virtual ~SOAuthorizationSession();

    // Probably not start immediately.
    void shouldStart();

    // The following should only be called by SOAuthorizationDelegate methods.
    void fallBackToWebPath();
    void abort();
    // Only responses that meet all of the following requirements will be processed:
    // 1) it has the same origin as the request;
    // 2) it has a status code of 302 or 200.
    // Otherwise, it falls back to the web path.
    // Only the following HTTP headers will be processed:
    // { Set-Cookie, Location }.
    void complete(NSHTTPURLResponse *, NSData *);
    void presentViewController(SOAuthorizationViewController, UICallback);

protected:
    // FSM depends on derived classes.
    enum class State : uint8_t {
        Idle,
        Active,
        Waiting,
        Completed
    };

    SOAuthorizationSession(SOAuthorization *, Ref<API::NavigationAction>&&, WebPageProxy&, InitiatingAction);

    void start();
    WebPageProxy* page() const { return m_page.get(); }
    State state() const { return m_state; }
    void setState(State state) { m_state = state; }
    const API::NavigationAction* navigationAction() { return m_navigationAction.get(); }
    Ref<API::NavigationAction> releaseNavigationAction();

private:
    virtual void shouldStartInternal() = 0;
    virtual void fallBackToWebPathInternal() = 0;
    virtual void abortInternal() = 0;
    virtual void completeInternal(const WebCore::ResourceResponse&, NSData *) = 0;

    void becomeCompleted();
    void dismissViewController();

    State m_state  { State::Idle };
    WeakObjCPtr<SOAuthorization *> m_soAuthorization;
    RefPtr<API::NavigationAction> m_navigationAction;
    WeakPtr<WebPageProxy> m_page;
    InitiatingAction m_action;

    RetainPtr<SOAuthorizationViewController> m_viewController;
#if PLATFORM(MAC)
    RetainPtr<NSWindow> m_sheetWindow;
    RetainPtr<NSObject> m_sheetWindowWillCloseObserver;
#endif
};

} // namespace WebKit

#endif
