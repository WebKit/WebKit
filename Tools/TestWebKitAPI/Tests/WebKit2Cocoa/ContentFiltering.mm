/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if WK_API_ENABLED && PLATFORM(MAC)

#import "MockContentFilterEnabler.h"
#import "PlatformUtilities.h"
#import "TestProtocol.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKBrowsingContextController.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/_WKDownloadDelegate.h>
#import <wtf/RetainPtr.h>

static bool isDone;

static RetainPtr<WKWebViewConfiguration> configurationWithContentFilterSettings(WebMockContentFilterDecision decision, WebMockContentFilterDecisionPoint decisionPoint)
{
    auto configuration = retainPtr([WKWebViewConfiguration testwebkitapi_configurationWithTestPlugInClassName:@"ContentFilteringPlugIn"]);
    auto contentFilterEnabler = adoptNS([[WebMockContentFilterEnabler alloc] initWithDecision:decision decisionPoint:decisionPoint blockedString:nil]);
    [[configuration processPool] _setObject:contentFilterEnabler.get() forBundleParameter:NSStringFromClass([WebMockContentFilterEnabler class])];
    return configuration;
}

@interface ServerRedirectNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation ServerRedirectNavigationDelegate

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    EXPECT_WK_STREQ(webView.URL.absoluteString, @"http://redirect/?pass");
}

- (void)webView:(WKWebView *)webView didReceiveServerRedirectForProvisionalNavigation:(WKNavigation *)navigation
{
    EXPECT_WK_STREQ(webView.URL.absoluteString, @"http://pass/");
}

- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

@end

TEST(ContentFiltering, URLAfterServerRedirect)
{
    @autoreleasepool {
        [NSURLProtocol registerClass:[TestProtocol class]];
        [WKBrowsingContextController registerSchemeForCustomProtocol:[TestProtocol scheme]];

        auto configuration = configurationWithContentFilterSettings(WebMockContentFilterDecisionAllow, WebMockContentFilterDecisionPointAfterAddData);
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        auto navigationDelegate = adoptNS([[ServerRedirectNavigationDelegate alloc] init]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://redirect?pass"]]];
        TestWebKitAPI::Util::run(&isDone);

        [WKBrowsingContextController unregisterSchemeForCustomProtocol:[TestProtocol scheme]];
        [NSURLProtocol unregisterClass:[TestProtocol class]];
    }
}

@interface BecomeDownloadDelegate : NSObject <WKNavigationDelegate>
@end

@implementation BecomeDownloadDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    decisionHandler(_WKNavigationResponsePolicyBecomeDownload);
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    isDone = true;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

@end

static bool downloadDidStart;

@interface ContentFilteringDownloadDelegate : NSObject <_WKDownloadDelegate>
@end

@implementation ContentFilteringDownloadDelegate

- (void)_downloadDidStart:(_WKDownload *)download
{
    downloadDidStart = true;
}

@end

static void downloadTest(WebMockContentFilterDecision decision, WebMockContentFilterDecisionPoint decisionPoint)
{
    @autoreleasepool {
        [NSURLProtocol registerClass:[TestProtocol class]];
        [WKBrowsingContextController registerSchemeForCustomProtocol:[TestProtocol scheme]];

        auto configuration = configurationWithContentFilterSettings(decision, decisionPoint);
        auto downloadDelegate = adoptNS([[ContentFilteringDownloadDelegate alloc] init]);
        [[configuration processPool] _setDownloadDelegate:downloadDelegate.get()];
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        auto navigationDelegate = adoptNS([[BecomeDownloadDelegate alloc] init]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://redirect/?download"]]];

        isDone = false;
        downloadDidStart = false;
        const bool downloadShouldStart = decision == WebMockContentFilterDecisionAllow || decisionPoint > WebMockContentFilterDecisionPointAfterResponse;
        if (downloadShouldStart)
            TestWebKitAPI::Util::run(&downloadDidStart);
        else
            TestWebKitAPI::Util::run(&isDone);

        EXPECT_EQ(downloadShouldStart, downloadDidStart);

        [WKBrowsingContextController unregisterSchemeForCustomProtocol:[TestProtocol scheme]];
        [NSURLProtocol unregisterClass:[TestProtocol class]];
    }
}

TEST(ContentFiltering, AllowDownloadAfterWillSendRequest)
{
    downloadTest(WebMockContentFilterDecisionAllow, WebMockContentFilterDecisionPointAfterWillSendRequest);
}

TEST(ContentFiltering, BlockDownloadAfterWillSendRequest)
{
    downloadTest(WebMockContentFilterDecisionBlock, WebMockContentFilterDecisionPointAfterWillSendRequest);
}

TEST(ContentFiltering, AllowDownloadAfterRedirect)
{
    downloadTest(WebMockContentFilterDecisionAllow, WebMockContentFilterDecisionPointAfterRedirect);
}

TEST(ContentFiltering, BlockDownloadAfterRedirect)
{
    downloadTest(WebMockContentFilterDecisionBlock, WebMockContentFilterDecisionPointAfterRedirect);
}

TEST(ContentFiltering, AllowDownloadAfterResponse)
{
    downloadTest(WebMockContentFilterDecisionAllow, WebMockContentFilterDecisionPointAfterResponse);
}

TEST(ContentFiltering, BlockDownloadAfterResponse)
{
    downloadTest(WebMockContentFilterDecisionBlock, WebMockContentFilterDecisionPointAfterResponse);
}

TEST(ContentFiltering, AllowDownloadAfterAddData)
{
    downloadTest(WebMockContentFilterDecisionAllow, WebMockContentFilterDecisionPointAfterAddData);
}

TEST(ContentFiltering, BlockDownloadAfterAddData)
{
    downloadTest(WebMockContentFilterDecisionBlock, WebMockContentFilterDecisionPointAfterAddData);
}

TEST(ContentFiltering, AllowDownloadAfterFinishedAddingData)
{
    downloadTest(WebMockContentFilterDecisionAllow, WebMockContentFilterDecisionPointAfterFinishedAddingData);
}

TEST(ContentFiltering, BlockDownloadAfterFinishedAddingData)
{
    downloadTest(WebMockContentFilterDecisionBlock, WebMockContentFilterDecisionPointAfterFinishedAddingData);
}

TEST(ContentFiltering, AllowDownloadNever)
{
    downloadTest(WebMockContentFilterDecisionAllow, WebMockContentFilterDecisionPointNever);
}

TEST(ContentFiltering, BlockDownloadNever)
{
    downloadTest(WebMockContentFilterDecisionBlock, WebMockContentFilterDecisionPointNever);
}

#endif // WK_API_ENABLED
