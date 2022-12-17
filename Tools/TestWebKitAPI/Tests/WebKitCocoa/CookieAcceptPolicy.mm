/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import <WebKit/WKHTTPCookieStorePrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/RetainPtr.h>

@interface CookieAcceptPolicyMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation CookieAcceptPolicyMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    lastScriptMessage = message;
    receivedScriptMessage = true;
}

@end

TEST(WebKit, CookieAcceptPolicy)
{
    auto originalCookieAcceptPolicy = [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto handler = adoptNS([[CookieAcceptPolicyMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"CookieMessage" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    __block bool setPolicy = false;
    [configuration.get().websiteDataStore.httpCookieStore _setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyNever completionHandler:^{
        setPolicy = true;
    }];
    TestWebKitAPI::Util::run(&setPolicy);
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_STREQ([(NSString *)[lastScriptMessage body] UTF8String], "COOKIE:");

    setPolicy = false;
    [configuration.get().websiteDataStore.httpCookieStore _setCookieAcceptPolicy:originalCookieAcceptPolicy completionHandler:^{
        setPolicy = true;
    }];
    TestWebKitAPI::Util::run(&setPolicy);
}

TEST(WebKit, WKCookiePolicy)
{
    using namespace TestWebKitAPI;

    __block bool done { false };
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto dataStore = [WKWebsiteDataStore nonPersistentDataStore];
    auto cookieStore = dataStore.httpCookieStore;
    configuration.get().websiteDataStore = dataStore;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    [cookieStore getCookiePolicy:^(WKCookiePolicy policy) {
        EXPECT_EQ(policy, WKCookiePolicyAllow);
        done = true;
    }];
    Util::run(&done);
    done = false;

    [cookieStore setCookiePolicy:WKCookiePolicyDisallow completionHandler:^{
        done = true;
    }];
    Util::run(&done);
    done = false;

    [cookieStore getCookiePolicy:^(WKCookiePolicy policy) {
        EXPECT_EQ(policy, WKCookiePolicyDisallow);
        done = true;
    }];
    Util::run(&done);
    done = false;

    bool requestHadCookie { false };
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&] (Connection connection) -> Task { while (true) {
        auto request = co_await connection.awaitableReceiveHTTPRequest();
        requestHadCookie = strnstr(request.data(), "Cookie", request.size());
        co_await connection.awaitableSend(HTTPResponse({ { "Set-Cookie"_s, "testCookie=42"_s } }, "hi"_s).serialize());
    } });

    [webView loadRequest:server.request()];
    [webView _test_waitForDidFinishNavigation];
    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"document.cookie"], "");
    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"document.cookie='a=b';document.cookie"], "");
    EXPECT_FALSE(requestHadCookie);

    [cookieStore setCookiePolicy:WKCookiePolicyAllow completionHandler:^{
        done = true;
    }];
    Util::run(&done);
    done = false;

    [webView reload];
    [webView _test_waitForDidFinishNavigation];
    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"document.cookie"], "testCookie=42");
    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"document.cookie='a=b';document.cookie"], "testCookie=42");
    EXPECT_FALSE(requestHadCookie);

    [webView reload];
    [webView _test_waitForDidFinishNavigation];
    EXPECT_TRUE(requestHadCookie);
}
