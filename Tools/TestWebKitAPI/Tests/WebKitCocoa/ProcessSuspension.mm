/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>

TEST(ProcessSuspension, CancelWebProcessSuspension)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadTestPageNamed:@"large-red-square-image"];

    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_NE(pid1, 0);

    [webView _processWillSuspendImminentlyForTesting];
    [webView _processDidResumeForTesting];

    bool done = false;
    [webView evaluateJavaScript:@"window.location" completionHandler: [&] (id result, NSError *error) {
        auto pid2 = [webView _webProcessIdentifier];
        EXPECT_EQ(pid1, pid2);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(ProcessSuspension, DestroyWebPageDuringWebProcessSuspension)
{
    auto configuration1 = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration1.get() addToWindow:YES]);
    [webView1 synchronouslyLoadTestPageNamed:@"large-red-square-image"];

    auto pid1 = [webView1 _webProcessIdentifier];
    EXPECT_NE(pid1, 0);

    auto configuration2 = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration2.get().processPool = configuration1.get().processPool;
    configuration2.get()._relatedWebView = webView1.get();
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(100, 0, 100, 100) configuration:configuration2.get() addToWindow:YES]);
    [webView2 synchronouslyLoadTestPageNamed:@"large-red-square-image"];

    auto pid2 = [webView2 _webProcessIdentifier];
    EXPECT_EQ(pid1, pid2);

    auto webView3 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(200, 0, 100, 100) configuration:configuration2.get() addToWindow:YES]);
    [webView3 synchronouslyLoadTestPageNamed:@"large-red-square-image"];

    [webView3 _processWillSuspendForTesting:^{ }];
    [webView1 _close];
    TestWebKitAPI::Util::sleep(0.1);
    [webView2 _close];

    EXPECT_EQ(pid1, [webView3 _webProcessIdentifier]);
    TestWebKitAPI::Util::sleep(1);
    EXPECT_EQ(pid1, [webView3 _webProcessIdentifier]);
}
