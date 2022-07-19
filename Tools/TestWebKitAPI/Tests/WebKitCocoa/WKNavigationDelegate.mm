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

#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

TEST(WKNavigationDelegate, NoCrashIfExceptionInDecidePolicyForNavigationAction)
{
    __block bool didCallDelegate = false;

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDecidePolicyForNavigationAction:^(WKNavigationAction *, void (^decisionHandler)(WKNavigationActionPolicy)) {
        didCallDelegate = true;

        dispatch_async(dispatch_get_main_queue(), ^{
            decisionHandler(WKNavigationActionPolicyAllow);
        });

        @throw [NSException exceptionWithName:@"test" reason:@"should not crash" userInfo:nil];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadTestPageNamed:@"simple"];
    TestWebKitAPI::Util::run(&didCallDelegate);
}

TEST(WKNavigationDelegate, NoCrashIfExceptionInDecidePolicyForNavigationActionWithPreferences)
{
    __block bool didCallDelegate = false;

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDecidePolicyForNavigationActionWithPreferences:^(WKNavigationAction *, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        didCallDelegate = true;

        dispatch_async(dispatch_get_main_queue(), ^{
            decisionHandler(WKNavigationActionPolicyAllow, preferences);
        });

        @throw [NSException exceptionWithName:@"test" reason:@"should not crash" userInfo:nil];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadTestPageNamed:@"simple"];
    TestWebKitAPI::Util::run(&didCallDelegate);
}

TEST(WKNavigationDelegate, NoCrashIfExceptionInDecidePolicyForNavigationResponse)
{
    __block bool didCallDelegate = false;

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDecidePolicyForNavigationResponse:^(WKNavigationResponse *, void (^decisionHandler)(WKNavigationResponsePolicy)) {
        didCallDelegate = true;

        dispatch_async(dispatch_get_main_queue(), ^{
            decisionHandler(WKNavigationResponsePolicyAllow);
        });

        @throw [NSException exceptionWithName:@"test" reason:@"should not crash" userInfo:nil];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadTestPageNamed:@"simple"];
    TestWebKitAPI::Util::run(&didCallDelegate);
}

TEST(WKNavigationDelegate, NoCrashIfExceptionInDidCommitNavigation)
{
    __block bool didCallDelegate = false;

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDidCommitNavigation:^(WKWebView *, WKNavigation *) {
        didCallDelegate = true;

        @throw [NSException exceptionWithName:@"test" reason:@"should not crash" userInfo:nil];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadTestPageNamed:@"simple"];
    TestWebKitAPI::Util::run(&didCallDelegate);
}

TEST(WKNavigationDelegate, NoCrashIfExceptionInDidFinishNavigation)
{
    __block bool didCallDelegate = false;

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didCallDelegate = true;

        @throw [NSException exceptionWithName:@"test" reason:@"should not crash" userInfo:nil];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadTestPageNamed:@"simple"];
    TestWebKitAPI::Util::run(&didCallDelegate);
}
