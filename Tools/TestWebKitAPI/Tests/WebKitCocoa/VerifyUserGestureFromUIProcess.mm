/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/_WKUserInitiatedAction.h>

namespace TestWebKitAPI {

#if PLATFORM(MAC)
TEST(VerifyUserGesture, WindowOpenMouseEvent)
{
    auto openerHTML = "<script>"
    "addEventListener('mouseup', () => {"
    "    window.open('https://domain2.com/opened');"
    "})"
    "</script>"_s;
    HTTPServer server({
        { "/opener"_s, { openerHTML } },
        { "/opened"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto configuration = server.httpsProxyConfiguration();
    [[configuration preferences] _setVerifyWindowOpenUserGestureFromUIProcess:YES];
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    auto openerWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [openerWebView setNavigationDelegate:navigationDelegate.get()];
    [openerWebView setUIDelegate:uiDelegate.get()];

    __block BOOL consumed = NO;
    __block RetainPtr<TestWKWebView> openedWebView;
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *navigationAction, WKWindowFeatures *) {
        consumed = navigationAction._userInitiatedAction.consumed;
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
        return openedWebView.get();
    };

    [openerWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/opener"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [openerWebView evaluateJavaScript:@"window.open('https://domain2.com/opened');" completionHandler:nil];
    while (!openedWebView)
        Util::spinRunLoop();
    EXPECT_TRUE(consumed);

    openedWebView = nullptr;
    [openerWebView mouseDownAtPoint:CGPointMake(50, 50) simulatePressure:NO];
    [openerWebView mouseUpAtPoint:CGPointMake(50, 50)];
    while (!openedWebView)
        Util::spinRunLoop();
    EXPECT_FALSE(consumed);
}

TEST(VerifyUserGesture, WindowOpenKeyEvent)
{
    auto openerHTML = "<script>"
    "addEventListener('keyup', () => {"
    "    window.open('https://domain2.com/opened');"
    "})"
    "</script>"_s;
    HTTPServer server({
        { "/opener"_s, { openerHTML } },
        { "/opened"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto configuration = server.httpsProxyConfiguration();
    [[configuration preferences] _setVerifyWindowOpenUserGestureFromUIProcess:YES];
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    auto openerWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [openerWebView setNavigationDelegate:navigationDelegate.get()];
    [openerWebView setUIDelegate:uiDelegate.get()];

    __block BOOL consumed = NO;
    __block RetainPtr<TestWKWebView> openedWebView;
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *navigationAction, WKWindowFeatures *) {
        consumed = navigationAction._userInitiatedAction.consumed;
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
        return openedWebView.get();
    };

    [openerWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/opener"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [openerWebView evaluateJavaScript:@"window.open('https://domain2.com/opened');" completionHandler:nil];
    while (!openedWebView)
        Util::spinRunLoop();
    EXPECT_TRUE(consumed);

    openedWebView = nullptr;
    [openerWebView typeCharacter:'a'];
    while (!openedWebView)
        Util::spinRunLoop();
    EXPECT_FALSE(consumed);
}
#endif

}
