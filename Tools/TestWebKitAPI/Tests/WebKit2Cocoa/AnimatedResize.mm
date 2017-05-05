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

#include "config.h"

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import <WebKit/WKPreferences.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED && PLATFORM(IOS)

static bool didLayout;
static bool didEndAnimatedResize;

@interface AnimatedResizeWebView : WKWebView

@end

@implementation AnimatedResizeWebView

- (void)_endAnimatedResize
{
    [super _endAnimatedResize];

    didEndAnimatedResize = true;
}

@end

static RetainPtr<WKWebView> createAnimatedResizeWebView()
{
    RetainPtr<_WKProcessPoolConfiguration> processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [processPoolConfiguration setIgnoreSynchronousMessagingTimeoutsForTesting:YES];
    RetainPtr<WKProcessPool> processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    RetainPtr<WKWebViewConfiguration> webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];

    RetainPtr<WKWebView> webView = adoptNS([[AnimatedResizeWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"blinking-div" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    return webView;
}

static RetainPtr<TestNavigationDelegate> createFirstVisuallyNonEmptyWatchingNavigationDelegate()
{
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setRenderingProgressDidChange:^(WKWebView *, _WKRenderingProgressEvents progressEvents) {
        if (progressEvents & _WKRenderingProgressEventFirstVisuallyNonEmptyLayout)
            didLayout = true;
    }];
    return navigationDelegate;
}

TEST(WebKit2, DISABLED_ResizeWithHiddenContentDoesNotHang)
{
    auto webView = createAnimatedResizeWebView();
    auto navigationDelegate = createFirstVisuallyNonEmptyWatchingNavigationDelegate();
    [webView setNavigationDelegate:navigationDelegate.get()];
    RetainPtr<UIWindow> window = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [window addSubview:webView.get()];
    [window setHidden:NO];

    TestWebKitAPI::Util::run(&didLayout);
    didLayout = false;

    for (unsigned i = 0; i < 50; i++) {
        [webView _resizeWhileHidingContentWithUpdates:^{
            [webView setFrame:CGRectMake(0, 0, [webView frame].size.width + 100, 400)];
        }];

        TestWebKitAPI::Util::run(&didEndAnimatedResize);
        didEndAnimatedResize = false;
    }
}

TEST(WebKit2, AnimatedResizeDoesNotHang)
{
    auto webView = createAnimatedResizeWebView();
    auto navigationDelegate = createFirstVisuallyNonEmptyWatchingNavigationDelegate();
    [webView setNavigationDelegate:navigationDelegate.get()];
    RetainPtr<UIWindow> window = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [window addSubview:webView.get()];
    [window setHidden:NO];

    TestWebKitAPI::Util::run(&didLayout);
    didLayout = false;

    for (unsigned i = 0; i < 50; i++) {
        [webView _beginAnimatedResizeWithUpdates:^{
            [webView setFrame:CGRectMake(0, 0, [webView frame].size.width + 100, 400)];
        }];

        dispatch_async(dispatch_get_main_queue(), ^{
            [webView _endAnimatedResize];
        });

        TestWebKitAPI::Util::run(&didEndAnimatedResize);
        didEndAnimatedResize = false;
    }
}

#endif
