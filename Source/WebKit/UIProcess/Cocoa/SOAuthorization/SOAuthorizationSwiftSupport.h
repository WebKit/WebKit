/*
 * Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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

#include "APIFrameHandle.h"
#include "APIHTTPCookieStore.h"
#include "APINavigationAction.h"
#include "APIPageConfiguration.h"
#include "FrameLoadState.h"
#include "PageLoadState.h"
#include "SOAuthorizationCoordinator.h"
#include "SOAuthorizationLoadPolicy.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebViewDidMoveToWindowObserver.h"
#include "WebsiteDataStore.h"
#include <WebCore/Cookie.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SecurityOrigin.h>
#include <pal/spi/cocoa/AuthKitSPI.h>
#include <wtf/BlockPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/URL.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

namespace WebKit {

// FIXME: (rdar://170854276) Swift cannot spell a C++ class template instantiation, so these aliases name them.
// The session gives setCookies its cookies in this vector.
using SOAuthorizationCookieVector = Vector<WebCore::Cookie>;
// The subframe check gives the policy its frame ancestors' origins in this.
using SOAuthorizationOriginVector = Vector<Ref<WebCore::SecurityOrigin>>;
// The new page callback takes this when the pop-up does not open.
using RefPtrWebPageProxy = RefPtr<WebPageProxy>;

// A main-actor block: every caller calls it on the main thread.
typedef void (NS_SWIFT_UI_ACTOR ^SOAuthorizationReply)(void);

namespace SOAuthorizationSupport {

// The session's two calls for Swift 6.3, which does not add WTF's CompletionHandler conformance (327763@main).
// FIXME: Remove these and the Swift's compiler(<6.4) paths when WebKit no longer builds its Swift with Swift 6.3.
typedef void (NS_SWIFT_UI_ACTOR ^SOAuthorizationPolicyReply)(SOAuthorizationLoadPolicy);
void decidePolicy(WebPageProxy&, const String& extensionName, SOAuthorizationPolicyReply);
void setCookies(WebPageProxy&, NSArray<NSHTTPCookie *> *, SOAuthorizationReply);

// FIXME: (rdar://130765784) Swift cannot compare foreign references for identity, so the comparison stays in C++.
inline bool isFrameInPage(const WebFrameProxy& frame, const WebPageProxy& page) { return frame.page() == &page; }

// FIXME: (rdar://192678485) Swift cannot own a uniquely owned C++ object, so C++ makes, uses and frees the policy.
enum class FrameAncestorsDecision : uint8_t { Interrupt, Allow, CheckXFrameOptions };
FrameAncestorsDecision frameAncestorsDecision(const WebCore::ResourceResponse&, const String& referrer, const SOAuthorizationOriginVector&);

// Moves both callbacks out of their boxes and calls the UI client callback.
// The session reads the emptied UI client callback box as the navigation action given away.
// FIXME: (rdar://192180990) Swift passes a box's member inout to a C++ call that can reach the box, so C++ moves it.
void callUIClientCallback(API::NavigationAction&, SOAuthorizationUIClientCallback&, SOAuthorizationNewPageCallback&);
#if PLATFORM(IOS_FAMILY)
// FIXME: (rdar://192678485) Swift cannot borrow API::UIClient, which the page owns through a std::unique_ptr.
UIViewController * _Nullable presentingViewController(WebPageProxy&);
#endif

} // namespace SOAuthorizationSupport

// FIXME: (rdar://105042781) Swift cannot inherit from a C++ class, so these observers call the session through a block.
// Swift holds each one as a shared reference; its initializer returns it at RefCounted's count of one, retained.
// Only Swift makes them; the constructors are public for its import. C++ would have to adoptRef a new one, never put one on the stack.
class SOAuthorizationDidMoveToWindowObserver final : public WebViewDidMoveToWindowObserver, public RefCounted<SOAuthorizationDidMoveToWindowObserver> {
public:
    explicit SOAuthorizationDidMoveToWindowObserver(SOAuthorizationReply callback) SWIFT_RETURNS_RETAINED
        : m_callback(makeBlockPtr(callback))
    {
    }

    NS_SWIFT_UNAVAILABLE("Swift counts the reference itself") void ref() const final { RefCounted::ref(); }
    NS_SWIFT_UNAVAILABLE("Swift counts the reference itself") void deref() const final { RefCounted::deref(); }
    void addTo(WebPageProxy& page) SWIFT_NAME(add(to:)) { page.addDidMoveToWindowObserver(*this); }
    void removeFrom(WebPageProxy& page) SWIFT_NAME(remove(from:)) { page.removeDidMoveToWindowObserver(*this); }
private:
    void webViewDidMoveToWindow() final { m_callback(); }
    const BlockPtr<void()> m_callback;
} SWIFT_SHARED_REFERENCE(.ref, .deref);

class SOAuthorizationFrameLoadObserver final : public FrameLoadStateObserver, public RefCounted<SOAuthorizationFrameLoadObserver> {
public:
    explicit SOAuthorizationFrameLoadObserver(SOAuthorizationReply callback) SWIFT_RETURNS_RETAINED
        : m_callback(makeBlockPtr(callback))
    {
    }

    NS_SWIFT_UNAVAILABLE("Swift counts the reference itself") void ref() const final { RefCounted::ref(); }
    NS_SWIFT_UNAVAILABLE("Swift counts the reference itself") void deref() const final { RefCounted::deref(); }
    void addTo(WebFrameProxy& frame) SWIFT_NAME(add(to:)) { frame.frameLoadState().addObserver(*this); }
    void removeFrom(WebFrameProxy& frame) SWIFT_NAME(remove(from:)) { frame.frameLoadState().removeObserver(*this); }
private:
    void didFinishLoad(IsMainFrame, const URL&) final { m_callback(); }
    const BlockPtr<void()> m_callback;
} SWIFT_SHARED_REFERENCE(.ref, .deref);

} // namespace WebKit

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // HAVE(APP_SSO)
