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

#if HAVE(WEBCONTENTRESTRICTIONS)

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKErrorRef.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>

using namespace TestWebKitAPI;

TEST(ParentalControlsContentFilteringTests, BlockedURLAfterRedirect)
{
    HTTPServer server({
        { "/start"_s, { 302, {{ "Location"_s, "/final"_s }}, "redirecting..."_s } },
        { "/final"_s, { "Done."_s } }
    });

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto blockedURL = [NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/start", server.port()]];
    __block bool mockInstalled = false;
    [[webView configuration].websiteDataStore _installMockParentalControlsURLFilterForTestingWithBlockedURLs:@[blockedURL] completionHandler:^{
        mockInstalled = true;
    }];
    Util::run(&mockInstalled);

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    __block bool didFail = false;
    [navigationDelegate setDidFailProvisionalNavigation:^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_WK_STREQ(WebKitErrorDomain, error.domain);
        EXPECT_EQ(kWKErrorCodeFrameLoadBlockedByContentFilter, error.code);
        didFail = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:server.request("/start"_s)];

    Util::run(&didFail);
}

TEST(ParentalControlsContentFilteringTests, BlockedURLAfterMultipleRedirections_BlockStart)
{
    HTTPServer server({
        { "/start"_s, { 302, {{ "Location"_s, "/middle"_s }}, "redirecting..."_s } },
        { "/middle"_s, { 302, {{ "Location"_s, "/final"_s }}, "redirecting..."_s } },
        { "/final"_s, { "Done."_s } }
    });

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto blockedURL = [NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/start", server.port()]];
    __block bool mockInstalled = false;
    [[webView configuration].websiteDataStore _installMockParentalControlsURLFilterForTestingWithBlockedURLs:@[blockedURL] completionHandler:^{
        mockInstalled = true;
    }];
    Util::run(&mockInstalled);

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    __block bool didFail = false;
    [navigationDelegate setDidFailProvisionalNavigation:^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_WK_STREQ(WebKitErrorDomain, error.domain);
        EXPECT_EQ(kWKErrorCodeFrameLoadBlockedByContentFilter, error.code);
        didFail = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:server.request("/start"_s)];

    Util::run(&didFail);
}

TEST(ParentalControlsContentFilteringTests, BlockedURLAfterMultipleRedirections_BlockMiddle)
{
    HTTPServer server({
        { "/start"_s, { 302, {{ "Location"_s, "/middle"_s }}, "redirecting..."_s } },
        { "/middle"_s, { 302, {{ "Location"_s, "/final"_s }}, "redirecting..."_s } },
        { "/final"_s, { "Done."_s } }
    });

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto blockedURL = [NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/middle", server.port()]];
    __block bool mockInstalled = false;
    [[webView configuration].websiteDataStore _installMockParentalControlsURLFilterForTestingWithBlockedURLs:@[blockedURL] completionHandler:^{
        mockInstalled = true;
    }];
    Util::run(&mockInstalled);

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    __block bool didFail = false;
    [navigationDelegate setDidFailProvisionalNavigation:^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_WK_STREQ(WebKitErrorDomain, error.domain);
        EXPECT_EQ(kWKErrorCodeFrameLoadBlockedByContentFilter, error.code);
        didFail = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:server.request("/start"_s)];

    Util::run(&didFail);
}

TEST(ParentalControlsContentFilteringTests, BlockedURLAfterMultipleRedirections_BlockFinal)
{
    HTTPServer server({
        { "/start"_s, { 302, {{ "Location"_s, "/middle"_s }}, "redirecting..."_s } },
        { "/middle"_s, { 302, {{ "Location"_s, "/final"_s }}, "redirecting..."_s } },
        { "/final"_s, { "Done."_s } }
    });

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto blockedURL = [NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/final", server.port()]];
    __block bool mockInstalled = false;
    [[webView configuration].websiteDataStore _installMockParentalControlsURLFilterForTestingWithBlockedURLs:@[blockedURL] completionHandler:^{
        mockInstalled = true;
    }];
    Util::run(&mockInstalled);

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    __block bool didFail = false;
    [navigationDelegate setDidFailProvisionalNavigation:^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_WK_STREQ(WebKitErrorDomain, error.domain);
        EXPECT_EQ(kWKErrorCodeFrameLoadBlockedByContentFilter, error.code);
        didFail = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:server.request("/start"_s)];

    Util::run(&didFail);
}

#endif // HAVE(WEBCONTENTRESTRICTIONS)
