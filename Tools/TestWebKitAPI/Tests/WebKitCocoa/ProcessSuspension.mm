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
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/Function.h>

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
    TestWebKitAPI::Util::runFor(0.1_s);
    [webView2 _close];

    EXPECT_EQ(pid1, [webView3 _webProcessIdentifier]);
    TestWebKitAPI::Util::runFor(1_s);
    EXPECT_EQ(pid1, [webView3 _webProcessIdentifier]);
}

TEST(ProcessSuspension, WKWebViewSuspendPage)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto testMessageHandler = adoptNS([[TestMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:testMessageHandler.get() name:@"testHandler"];
    __block unsigned timerFiredCount = 0;
    [testMessageHandler addMessage:@"timer fired" withHandler:^{
        ++timerFiredCount;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadTestPageNamed:@"postMessage-regularly"];

    auto pid = [webView _webProcessIdentifier];
    EXPECT_NE(pid, 0);

    // The timer should be firing.
    auto lastTimerFireCount = timerFiredCount;
    while (lastTimerFireCount == timerFiredCount)
        TestWebKitAPI::Util::spinRunLoop(10);
    EXPECT_TRUE(timerFiredCount > lastTimerFireCount);

    auto testRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/"]];
    id testInteractionState = [webView interactionState];
    auto originalURL = [webView URL];

    // Suspend the page.
    [webView removeFromSuperview];
    __block bool done = false;
    [webView _suspendPage:^(BOOL success) {
        EXPECT_TRUE(success);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // The timer should no longer be firing.
    lastTimerFireCount = timerFiredCount;
    TestWebKitAPI::Util::spinRunLoop(100);
    EXPECT_EQ(lastTimerFireCount, timerFiredCount);

    auto checkThrows = [](Function<void()>&& f) {
        @try {
            f();
        } @catch (NSException *e) {
            return true;
        }
        return false;
    };

    // Most operations on WKWebView should throw an exception when suspended.
    EXPECT_TRUE(checkThrows([&] { [webView evaluateJavaScript:@"window.name" completionHandler:nil]; }));
    EXPECT_TRUE(checkThrows([&] { [webView synchronouslyLoadTestPageNamed:@"simple2"]; }));
    EXPECT_TRUE(checkThrows([&] { [webView loadHTMLString:@"foo" baseURL:nil]; }));
    EXPECT_TRUE(checkThrows([&] { [webView goToBackForwardListItem:webView.get().backForwardList.currentItem]; }));
    EXPECT_TRUE(checkThrows([&] { [webView reload]; }));
    EXPECT_TRUE(checkThrows([&] { [webView reloadFromOrigin]; }));
    EXPECT_TRUE(checkThrows([&] { [webView goBack]; }));
    EXPECT_TRUE(checkThrows([&] { [webView goForward]; }));
    EXPECT_TRUE(checkThrows([&] { [webView stopLoading]; }));
    EXPECT_TRUE(checkThrows([&] { [webView closeAllMediaPresentationsWithCompletionHandler:nil]; }));
    EXPECT_TRUE(checkThrows([&] { [webView pauseAllMediaPlaybackWithCompletionHandler:nil]; }));
    EXPECT_TRUE(checkThrows([&] { [webView setAllMediaPlaybackSuspended:YES completionHandler:nil]; }));
    EXPECT_TRUE(checkThrows([&] { [webView requestMediaPlaybackStateWithCompletionHandler:^(WKMediaPlaybackState) { }]; }));
    EXPECT_TRUE(checkThrows([&] { [webView setCameraCaptureState:WKMediaCaptureStateActive completionHandler:nil]; }));
    EXPECT_TRUE(checkThrows([&] { [webView setMicrophoneCaptureState:WKMediaCaptureStateActive completionHandler:nil]; }));
#if TARGET_OS_IPHONE
    EXPECT_TRUE(checkThrows([&] { [webView takeSnapshotWithConfiguration:nil completionHandler:^(UIImage *snapshot, NSError *error) { }]; }));
#else
    EXPECT_TRUE(checkThrows([&] { [webView takeSnapshotWithConfiguration:nil completionHandler:^(NSImage *snapshot, NSError *error) { }]; }));
#endif
    EXPECT_TRUE(checkThrows([&] { [webView createPDFWithConfiguration:nil completionHandler:^(NSData *data, NSError *error) { }]; }));
    EXPECT_TRUE(checkThrows([&] { [webView createWebArchiveDataWithCompletionHandler:^(NSData *data, NSError *error) { }]; }));
    EXPECT_TRUE(checkThrows([&] { webView.get().allowsBackForwardNavigationGestures = YES; }));
    EXPECT_TRUE(checkThrows([&] { webView.get().customUserAgent = @"foo"; }));
    EXPECT_TRUE(checkThrows([&] { webView.get().allowsLinkPreview = YES; }));
#if !TARGET_OS_IPHONE
    EXPECT_TRUE(checkThrows([&] { webView.get().allowsMagnification = YES; }));
    EXPECT_TRUE(checkThrows([&] { webView.get().magnification = 1.1; }));
    EXPECT_TRUE(checkThrows([&] { [webView setMagnification:1.1 centeredAtPoint:CGPointMake(0, 0)]; }));
#endif
    EXPECT_TRUE(checkThrows([&] { webView.get().pageZoom = 1.1; }));
    EXPECT_TRUE(checkThrows([&] { [webView findString:@"foo" withConfiguration:nil completionHandler:^(WKFindResult *result) { }]; }));
    EXPECT_TRUE(checkThrows([&] { [webView startDownloadUsingRequest:testRequest completionHandler:^(WKDownload *download) { }]; }));
    EXPECT_TRUE(checkThrows([&] { webView.get().interactionState = testInteractionState; }));
    EXPECT_TRUE(checkThrows([&] { webView.get().mediaType = @"text/html"; }));

    // Some of the basic API still works.
    EXPECT_WK_STREQ([webView URL].absoluteString, originalURL.absoluteString);
    EXPECT_EQ([webView backForwardList].backItem, nil);
    EXPECT_WK_STREQ([webView title], @"");
    EXPECT_FALSE([webView isLoading]);
    EXPECT_EQ([webView estimatedProgress], 1.0);
    EXPECT_FALSE([webView canGoBack]);
    EXPECT_FALSE([webView canGoForward]);
    EXPECT_FALSE([webView hasOnlySecureContent]);
    EXPECT_EQ([webView serverTrust], nil);
    EXPECT_EQ([webView cameraCaptureState], WKMediaCaptureStateNone);
    EXPECT_EQ([webView microphoneCaptureState], WKMediaCaptureStateNone);
    EXPECT_WK_STREQ([webView customUserAgent], @"");
    EXPECT_TRUE([webView allowsLinkPreview]);
#if !TARGET_OS_IPHONE
    EXPECT_FALSE([webView allowsMagnification]);
    EXPECT_EQ([webView magnification], 1.0);
#endif
    EXPECT_EQ([webView pageZoom], 1.0);
    EXPECT_WK_STREQ([webView mediaType], @"");
    EXPECT_FALSE(checkThrows([&] { [webView interactionState]; }));
    EXPECT_FALSE(checkThrows([&] { [webView themeColor]; }));

    // Resume the page.
    done = false;
    [webView _resumePage:^(BOOL success) {
        EXPECT_TRUE(success);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    [webView addToTestWindow];

    // The timer should start firing again.
    lastTimerFireCount = timerFiredCount;
    while (lastTimerFireCount == timerFiredCount)
        TestWebKitAPI::Util::spinRunLoop(10);
    EXPECT_TRUE(timerFiredCount > lastTimerFireCount);

    // Make sure we did not crash.
    EXPECT_EQ(pid, [webView _webProcessIdentifier]);
}

TEST(ProcessSuspension, DeallocateSuspendedView)
{
    // Deallocating a suspended WebView should not throw or crash.
    @autoreleasepool {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
        [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
        [webView _test_waitForDidFinishNavigation];

        __block bool done = false;
        [webView _suspendPage:^(BOOL success) {
            EXPECT_TRUE(success);
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);

        [webView _close];
    }
}
