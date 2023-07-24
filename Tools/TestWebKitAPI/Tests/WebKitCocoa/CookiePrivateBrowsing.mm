/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKHTTPCookieStorePrivate.h>
#import <WebKit/WKProcessPool.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/StringConcatenateNumbers.h>
#import <wtf/text/WTFString.h>

static bool didRunJavaScriptAlertForCookiePrivateBrowsing;

@interface CookiePrivateBrowsingDelegate : NSObject <WKUIDelegate>
@end

@implementation CookiePrivateBrowsingDelegate

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    EXPECT_STREQ(message.UTF8String, "old cookie: <>");
    didRunJavaScriptAlertForCookiePrivateBrowsing = true;
    completionHandler();
}

@end

TEST(WebKit, CookiePrivateBrowsing)
{
    auto delegate = adoptNS([[CookiePrivateBrowsingDelegate alloc] init]);

    auto configuration1 = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto configuration2 = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration2 setProcessPool:[configuration1 processPool]];
    [configuration1 setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    [configuration2 setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    auto view1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration1.get()]);
    auto view2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration2.get()]);
    [view1 setUIDelegate:delegate.get()];
    [view2 setUIDelegate:delegate.get()];
    NSString *alertOldCookie = @"<script>var oldCookie = document.cookie; document.cookie = 'key=value'; alert('old cookie: <' + oldCookie + '>');</script>";
    [view1 loadHTMLString:alertOldCookie baseURL:[NSURL URLWithString:@"http://example.com/"]];
    TestWebKitAPI::Util::run(&didRunJavaScriptAlertForCookiePrivateBrowsing);
    didRunJavaScriptAlertForCookiePrivateBrowsing = false;
    [view2 loadHTMLString:alertOldCookie baseURL:[NSURL URLWithString:@"http://example.com/"]];
    TestWebKitAPI::Util::run(&didRunJavaScriptAlertForCookiePrivateBrowsing);
}

TEST(WebKit, CookieCacheSyncAcrossProcess)
{
    __block bool setDefaultCookieAcceptPolicy = false;
    [[WKWebsiteDataStore defaultDataStore].httpCookieStore _setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain completionHandler:^{
        setDefaultCookieAcceptPolicy = true;
    }];
    TestWebKitAPI::Util::run(&setDefaultCookieAcceptPolicy);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    auto view1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto view2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [view1 synchronouslyLoadHTMLString:@"foo" baseURL:[NSURL URLWithString:@"http://example.com/"]];
    [view2 synchronouslyLoadHTMLString:@"bar" baseURL:[NSURL URLWithString:@"http://example.com/"]];

    // Cache DOM cookies in first WebView.
    __block bool doneEvaluatingJavaScript = false;
    [view1 evaluateJavaScript:@"document.cookie;" completionHandler:^(id _Nullable cookie, NSError * _Nullable error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([cookie isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ("", (NSString *)cookie);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);

    // Cache DOM cookies in second WebView.
    doneEvaluatingJavaScript = false;
    [view2 evaluateJavaScript:@"document.cookie;" completionHandler:^(id _Nullable cookie, NSError * _Nullable error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([cookie isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ("", (NSString *)cookie);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);

    // Setting cookie in first Webview / process.
    doneEvaluatingJavaScript = false;
    [view1 evaluateJavaScript:@"document.cookie = 'foo=bar'; document.cookie;" completionHandler:^(id _Nullable cookie, NSError * _Nullable error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([cookie isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ("foo=bar", (NSString *)cookie);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);

    // Making sure new cookie gets sync'd to second WebView process.
    int timeout = 0;
    __block String cookieString;
    do {
        TestWebKitAPI::Util::runFor(0.1_s);
        doneEvaluatingJavaScript = false;
        [view2 evaluateJavaScript:@"document.cookie;" completionHandler:^(id _Nullable cookie, NSError * _Nullable error) {
            EXPECT_NULL(error);
            EXPECT_TRUE([cookie isKindOfClass:[NSString class]]);
            cookieString = (NSString *)cookie;
            doneEvaluatingJavaScript = true;
        }];
        TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
        ++timeout;
    } while (!cookieString.isEmpty() && timeout < 50);
    EXPECT_WK_STREQ("foo=bar", cookieString);
}

TEST(WebKit, CookieCachePruning)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto view = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    for (unsigned i = 0; i < 100; ++i) {
        [view synchronouslyLoadHTMLString:@"foo" baseURL:[NSURL URLWithString:makeString("http://foo", i, ".example.com/")]];

        __block bool doneEvaluatingJavaScript = false;
        [view evaluateJavaScript:@"document.cookie;" completionHandler:^(id _Nullable cookie, NSError * _Nullable error) {
            EXPECT_NULL(error);
            EXPECT_TRUE([cookie isKindOfClass:[NSString class]]);
            EXPECT_WK_STREQ("", (NSString *)cookie);
            doneEvaluatingJavaScript = true;
        }];
        TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    }
}

// FIXME: Re-enable this test for iOS once webkit.org/b/253387 is resolved
#if PLATFORM(IOS) || PLATFORM(VISION)
TEST(WebKit, DISABLED_CookieAccessFromPDFInAboutBlank)
#else
TEST(WebKit, CookieAccessFromPDFInAboutBlank)
#endif
{
    auto delegate = adoptNS([TestUIDelegate new]);
    __block RetainPtr<WKWebView> openedWebView;
    delegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *, WKWindowFeatures *) {
        openedWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        return openedWebView.get();
    };

    auto webProcessStarter = adoptNS([TestWKWebView new]);
    [webProcessStarter synchronouslyLoadHTMLString:@"start network process so the creation of the second web view doesn't send NetworkProcessCreationParameters" baseURL:nil];

    TestWebKitAPI::HTTPServer server({ { "/"_s, { "hi"_s } } });
    auto webView = adoptNS([TestWKWebView new]);
    webView.get().UIDelegate = delegate.get();
    NSString *html = [NSString stringWithFormat:@"<script>"
        "var w = window.open();"
        "w.document.write('<embed type=\"application/pdf\" src=\"%@\"></embed>');"
        "</script>", server.request().URL];
    [webView loadHTMLString:html baseURL:server.request().URL];

    while (!server.totalRequests())
        TestWebKitAPI::Util::spinRunLoop();
}
