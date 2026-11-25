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

#import "config.h"
#import "SOAuthorizationSwiftSupport.h"

#if HAVE(APP_SSO)

#import "APIUIClient.h"
#import "UIKitUtilities.h"
#import "WKWebViewInternal.h"
#import <WebCore/ContentSecurityPolicy.h>
#import <WebCore/ContentSecurityPolicyResponseHeaders.h>

namespace WebKit {

void SOAuthorizationSupport::decidePolicy(WebPageProxy& page, const String& extensionName, SOAuthorizationPolicyReply reply)
{
    page.decidePolicyForSOAuthorizationLoad(extensionName, [reply = makeBlockPtr(reply)](SOAuthorizationLoadPolicy policy) {
        reply(policy);
    });
}

void SOAuthorizationSupport::setCookies(WebPageProxy& page, NSArray<NSHTTPCookie *> *cookies, SOAuthorizationReply reply)
{
    Vector<WebCore::Cookie> cookieVector(cookies.count, [cookies](size_t i) {
        return WebCore::Cookie { cookies[i] };
    });
    protect(protect(page.websiteDataStore())->cookieStore())->setCookies(WTF::move(cookieVector), [reply = makeBlockPtr(reply)] {
        reply();
    });
}

SOAuthorizationSupport::FrameAncestorsDecision SOAuthorizationSupport::frameAncestorsDecision(const WebCore::ResourceResponse& response, const String& referrer, const SOAuthorizationOriginVector& frameAncestorOrigins)
{
    auto url = response.url();
    auto contentSecurityPolicy = makeUnique<WebCore::ContentSecurityPolicy>(URL { url }, nullptr, nullptr);
    contentSecurityPolicy->didReceiveHeaders(WebCore::ContentSecurityPolicyResponseHeaders { response }, String { referrer });
    if (!contentSecurityPolicy->allowFrameAncestors(frameAncestorOrigins, url))
        return FrameAncestorsDecision::Interrupt;

    if (contentSecurityPolicy->overridesXFrameOptions())
        return FrameAncestorsDecision::Allow;
    return FrameAncestorsDecision::CheckXFrameOptions;
}

void SOAuthorizationSupport::callUIClientCallback(API::NavigationAction& navigationAction, SOAuthorizationUIClientCallback& uiClientCallback, SOAuthorizationNewPageCallback& newPageCallback)
{
    auto callback = WTF::move(*uiClientCallback);
    callback(Ref { navigationAction }, WTF::move(*newPageCallback));
}

#if PLATFORM(IOS_FAMILY)
UIViewController *SOAuthorizationSupport::presentingViewController(WebPageProxy& page)
{
    // FIXME: When in element fullscreen, UIClient::presentingViewController() may not return the
    // WKFullScreenViewController even though that is the presenting view controller of the WKWebView.
    // We should call PageClientImpl::presentingViewController() instead.
    RetainPtr presentingViewController = page.uiClient().presentingViewController();
#if PLATFORM(VISION)
    page.dispatchWillPresentModalUI();
#else
    if (!presentingViewController)
        presentingViewController = [page.cocoaView() _wk_viewControllerForFullScreenPresentation];
#endif
    return presentingViewController.autorelease();
}
#endif

} // namespace WebKit

#endif // HAVE(APP_SSO)
