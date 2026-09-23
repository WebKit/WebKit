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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/Utilities.h"
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

// These values must match WebKit::NetworkActivityTracker::CompletionCode.
static constexpr uint8_t completionCodeSuccess = 2;
static constexpr uint8_t completionCodeCancel = 4;

TEST(NetworkActivityTracker, PageLoadCompletedReportsSuccess)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    HTTPServer server({ { "/"_s, { "<html><body>Hello</body></html>"_s } } });
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    [delegate waitForDidFinishNavigation];

    __block bool done = false;
    __block RetainPtr<NSNumber> resultCode;
    [webView _lastPageLoadNetworkActivityCompletionCodeForTesting:^(NSNumber *code) {
        resultCode = code;
        done = true;
    }];
    Util::run(&done);

    EXPECT_NOT_NULL(resultCode);
    EXPECT_EQ([resultCode unsignedCharValue], completionCodeSuccess);
}

TEST(NetworkActivityTracker, NavigationCancelReportsCancel)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    // Load a first page successfully to establish root activity tracking for this page.
    HTTPServer server({ { "/"_s, { "<html><body>Page 1</body></html>"_s } } });
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    [delegate waitForDidFinishNavigation];

    // Cancel the next navigation to simulate the user stopping the load (or a server rejecting).
    delegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *, void (^handler)(WKNavigationResponsePolicy)) {
        handler(WKNavigationResponsePolicyCancel);
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    [delegate waitForDidFailProvisionalNavigation];

    __block bool done = false;
    __block RetainPtr<NSNumber> resultCode;
    [webView _lastPageLoadNetworkActivityCompletionCodeForTesting:^(NSNumber *code) {
        resultCode = code;
        done = true;
    }];
    Util::run(&done);

    EXPECT_NOT_NULL(resultCode);
    EXPECT_EQ([resultCode unsignedCharValue], completionCodeCancel);
}

} // namespace TestWebKitAPI
