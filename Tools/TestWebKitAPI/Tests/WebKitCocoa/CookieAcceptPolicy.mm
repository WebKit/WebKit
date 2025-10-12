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
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKHTTPCookieStorePrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/text/MakeString.h>

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

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"CookieMessage" withExtension:@"html"]];
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

TEST(WKHTTPCookieStore, CookiePolicy)
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
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&] (Connection connection) -> ConnectionTask { while (true) {
        auto request = co_await connection.awaitableReceiveHTTPRequest();
        requestHadCookie = contains(request.span(), "Cookie"_span);
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
    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"document.cookie='a=b';document.cookie"], "a=b; testCookie=42");
    EXPECT_FALSE(requestHadCookie);

    [webView reload];
    [webView _test_waitForDidFinishNavigation];
    EXPECT_TRUE(requestHadCookie);
}

TEST(WKHTTPCookieStore, CookiePolicyAllowIsOnlyFromMainDocumentDomain)
{
    TestWebKitAPI::HTTPServer server([connectionCount = 0] (TestWebKitAPI::Connection connection) mutable {
        ++connectionCount;
        connection.receiveHTTPRequest([connectionCount, connection] (Vector<char>&& request) {
            String reply;
            if (connectionCount == 1) {
                constexpr auto body =
                "<script>"
                    "fetch('http://www.example.com', { credentials : 'include' }).then(()=>{ alert('fetched'); }).catch((e)=>{ alert(e); })"
                "</script>"_s;
                reply = makeString(
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: "_s, body.length(), "\r\n"
                    "Set-Cookie: a=b\r\n"
                    "Connection: close\r\n"
                    "\r\n"_s, body
                );
            } else {
                EXPECT_TRUE(contains(request.span(), "GET http://www.example.com/ HTTP/1.1\r\n"_span));
                reply =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 0\r\n"
                    "Set-Cookie: c=d\r\n"
                    "Access-Control-Allow-Origin: http://www.webkit.org\r\n"
                    "Access-Control-Allow-Credentials: true\r\n"
                    "Connection: close\r\n"
                    "\r\n"_s;
            }
            connection.send(WTFMove(reply));
        });
    });

    auto dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [dataStoreConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(server.port())
    }];
    [dataStoreConfiguration setAllowsServerPreconnect:NO];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    auto cookieStore = dataStore.get().httpCookieStore;
    [cookieStore setCookiePolicy:WKCookiePolicyAllow completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore.get()];
    // Relax WebKit's third-party cookies blocking policy so the WebKit will use cookie storage's policy
    // to decide whether third-party cookeis can be stored.
    [configuration _setShouldRelaxThirdPartyCookieBlocking:true];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://www.webkit.org/"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "fetched");

    [cookieStore getAllCookies:[&](NSArray<NSHTTPCookie *> *cookies) {
        // Ensure example.com's cookie is not stored.
        EXPECT_EQ([cookies count], 1u);
        EXPECT_WK_STREQ([[cookies firstObject] domain], @"www.webkit.org");
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}
