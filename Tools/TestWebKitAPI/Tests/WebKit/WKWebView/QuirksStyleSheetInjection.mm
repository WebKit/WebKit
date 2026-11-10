/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static constexpr auto overlayPointerEventsScript = "getComputedStyle(document.querySelector('#overlay')).pointerEvents"_s;

static RetainPtr<NSString> overlayPointerEventsInSubframe(NSString *topURL, NSString *subframeURL, bool siteIsolationEnabled)
{
    HTTPServer server({
        { "/main"_s, { makeString("<iframe src='"_s, String(subframeURL), "'></iframe>"_s) } },
        { "/subframe"_s, { "<div id='overlay'><button aria-label='Share'>Share</button><button aria-label='Report'>Report</button></div>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    [configuration preferences].siteSpecificQuirksModeEnabled = YES;
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600), siteIsolationEnabled);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:topURL]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_WK_STREQ(subframeURL, childFrame.get().request.URL.absoluteString);

    return [webView stringByEvaluatingJavaScript:@(overlayPointerEventsScript.characters()) inFrame:childFrame.get()];
}

TEST(QuirksStyleSheetInjection, AppliesToCrossOriginSubframeWithoutSiteIsolation)
{
    EXPECT_WK_STREQ("none", overlayPointerEventsInSubframe(@"https://www.yelp.com/main", @"https://webkit.org/subframe", false).get());
}

TEST(QuirksStyleSheetInjection, AppliesToCrossOriginSubframeWithSiteIsolation)
{
    EXPECT_WK_STREQ("none", overlayPointerEventsInSubframe(@"https://www.yelp.com/main", @"https://webkit.org/subframe", true).get());
}

TEST(QuirksStyleSheetInjection, AppliesToSameOriginSubframeWithoutSiteIsolation)
{
    EXPECT_WK_STREQ("none", overlayPointerEventsInSubframe(@"https://www.yelp.com/main", @"https://www.yelp.com/subframe", false).get());
}

TEST(QuirksStyleSheetInjection, AppliesToSameOriginSubframeWithSiteIsolation)
{
    EXPECT_WK_STREQ("none", overlayPointerEventsInSubframe(@"https://www.yelp.com/main", @"https://www.yelp.com/subframe", true).get());
}

TEST(QuirksStyleSheetInjection, DoesNotApplyToCrossOriginSubframeOfNonMatchingTopDocumentWithoutSiteIsolation)
{
    EXPECT_WK_STREQ("auto", overlayPointerEventsInSubframe(@"https://example.com/main", @"https://webkit.org/subframe", false).get());
}

TEST(QuirksStyleSheetInjection, DoesNotApplyToCrossOriginSubframeOfNonMatchingTopDocumentWithSiteIsolation)
{
    EXPECT_WK_STREQ("auto", overlayPointerEventsInSubframe(@"https://example.com/main", @"https://webkit.org/subframe", true).get());
}

} // namespace TestWebKitAPI
