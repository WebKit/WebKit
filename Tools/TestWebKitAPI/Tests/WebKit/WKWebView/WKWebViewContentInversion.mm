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

#if PLATFORM(IOS_FAMILY)

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestCocoa.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

@interface WKWebViewEffectivePageAppearanceObserver : NSObject
@property (nonatomic) NSUInteger changeCount;
@end

@implementation WKWebViewEffectivePageAppearanceObserver

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if ([keyPath isEqualToString:@"_effectivePageAppearance"])
        ++_changeCount;
}

@end

namespace TestWebKitAPI {

static constexpr auto imagePageMarkup = "<body><img src='data:image/gif;base64,R0lGODlhAQABAIAAAP///wAAACH5BAEAAAAALAAAAAABAAEAAAICRAEAOw=='></body>"_s;

static bool invertedColorsMatches(TestWKWebView *webView, WKFrameInfo *frame = nil)
{
    return [[webView objectByEvaluatingJavaScript:@"matchMedia('(inverted-colors: inverted)').matches" inFrame:frame] boolValue];
}

static NSString *imageFilter(TestWKWebView *webView, WKFrameInfo *frame = nil)
{
    return [webView objectByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('img')).filter" inFrame:frame];
}

TEST(WKWebViewContentInversion, ControlsInvertedColorsMediaQuery)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)]);
    [webView synchronouslyLoadHTMLString:imagePageMarkup.createNSString().get()];
    EXPECT_EQ([webView _contentInversionForAccessibility], _WKContentInversionSystem);

    [webView _setContentInversionForAccessibility:_WKContentInversionInverted];
    EXPECT_TRUE(invertedColorsMatches(webView.get()));
    EXPECT_WK_STREQ("invert(1)", imageFilter(webView.get()));

    [webView _setContentInversionForAccessibility:_WKContentInversionNotInverted];
    EXPECT_FALSE(invertedColorsMatches(webView.get()));
    EXPECT_WK_STREQ("none", imageFilter(webView.get()));
}

TEST(WKWebViewContentInversion, PersistsAcrossProcessSwap)
{
    HTTPServer server({
        { "/page"_s, { imagePageMarkup } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 480) configuration:server.httpsProxyConfiguration()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView _setContentInversionForAccessibility:_WKContentInversionInverted];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/page"]]];
    [navigationDelegate waitForDidFinishNavigation];
    auto firstProcessIdentifier = [webView _webProcessIdentifier];
    EXPECT_TRUE(invertedColorsMatches(webView.get()));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/page"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_NE(firstProcessIdentifier, [webView _webProcessIdentifier]);
    EXPECT_TRUE(invertedColorsMatches(webView.get()));
    EXPECT_WK_STREQ("invert(1)", imageFilter(webView.get()));
}

TEST(WKWebViewContentInversion, AppliesToCrossSiteIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { imagePageMarkup } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView _setContentInversionForAccessibility:_WKContentInversionInverted];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr iframe = [webView firstChildFrame];
    EXPECT_TRUE(invertedColorsMatches(webView.get(), iframe.get()));
    EXPECT_WK_STREQ("invert(1)", imageFilter(webView.get(), iframe.get()));

    [webView _setContentInversionForAccessibility:_WKContentInversionNotInverted];
    EXPECT_FALSE(invertedColorsMatches(webView.get(), iframe.get()));
    EXPECT_WK_STREQ("none", imageFilter(webView.get(), iframe.get()));
}

TEST(WKWebViewContentInversion, EffectivePageAppearance)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)]);
    RetainPtr observer = adoptNS([[WKWebViewEffectivePageAppearanceObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:@"_effectivePageAppearance" options:0 context:nil];
    EXPECT_EQ([webView _effectivePageAppearance], _WKEffectivePageAppearanceUnknown);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body { background-color: white; } body.dark { background-color: #111; } </style>"];
    EXPECT_EQ([webView _effectivePageAppearance], _WKEffectivePageAppearanceLight);

    auto changeCountBeforeDark = [observer changeCount];
    [webView objectByEvaluatingJavaScript:@"document.body.className = 'dark'"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ([webView _effectivePageAppearance], _WKEffectivePageAppearanceDark);
    EXPECT_GT([observer changeCount], changeCountBeforeDark);

    auto changeCountBeforeSimilarColor = [observer changeCount];
    [webView objectByEvaluatingJavaScript:@"document.body.style.backgroundColor = '#222'"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ([webView _effectivePageAppearance], _WKEffectivePageAppearanceDark);
    EXPECT_EQ([observer changeCount], changeCountBeforeSimilarColor);

    [webView removeObserver:observer.get() forKeyPath:@"_effectivePageAppearance"];
}

#if ENABLE(AX_CUSTOM_COLOR_MODE)

TEST(WKWebViewContentInversion, EffectivePageAppearanceWithAXCustomColorMode)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)]);
    RetainPtr observer = adoptNS([[WKWebViewEffectivePageAppearanceObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:@"_effectivePageAppearance" options:0 context:nil];

    // The page paints no background color of its own, so AX custom color mode is the only thing that makes it dark.
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<body>Hello</body>"];
    EXPECT_EQ([webView _effectivePageAppearance], _WKEffectivePageAppearanceLight);

    auto changeCountBeforeEnabling = [observer changeCount];
    [webView configuration].preferences._axCustomColorModeEnabled = YES;
    EXPECT_EQ([webView _effectivePageAppearance], _WKEffectivePageAppearanceDark);
    EXPECT_GT([observer changeCount], changeCountBeforeEnabling);

    auto changeCountBeforeDisabling = [observer changeCount];
    [webView configuration].preferences._axCustomColorModeEnabled = NO;
    EXPECT_EQ([webView _effectivePageAppearance], _WKEffectivePageAppearanceLight);
    EXPECT_GT([observer changeCount], changeCountBeforeDisabling);

    [webView removeObserver:observer.get() forKeyPath:@"_effectivePageAppearance"];
}

#endif // ENABLE(AX_CUSTOM_COLOR_MODE)

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
