/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <notify.h>

#if ENABLE(NOTIFY_BLOCKING)

static RetainPtr<NSNumber> getNotifyState(WKWebView *webView, const char* name)
{
    __block RetainPtr<NSNumber> result;
    __block bool done = false;
    [webView _getNotifyStateForTesting:@(name) completionHandler:^(NSNumber *state) {
        result = state;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result;
}

TEST(WKWebView, NotifyStateIsInitiallySetAndForwarded)
{
    const char* name = "org.WebKit.testNotification";
    int token = 0;
    EXPECT_EQ(notify_register_check(name, &token), 0u);
    EXPECT_EQ(notify_set_state(token, 42), 0u);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    NSURLRequest *loadRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com"]];
    [webView loadSimulatedRequest:loadRequest responseHTMLString:@"Hello world!"];
    [delegate waitForDidFinishNavigation];

    auto result = getNotifyState(webView.get(), name);
    EXPECT_TRUE(!!result);
    EXPECT_EQ([result integerValue], 42);

    EXPECT_EQ(notify_set_state(token, 100), 0u);
    EXPECT_EQ(notify_post(name), 0u);
    EXPECT_EQ(notify_cancel(token), 0u);

    auto success = TestWebKitAPI::Util::waitFor([&]() {
        return [getNotifyState(webView.get(), name) integerValue] == 100;
    });
    EXPECT_TRUE(success);
}

#endif
