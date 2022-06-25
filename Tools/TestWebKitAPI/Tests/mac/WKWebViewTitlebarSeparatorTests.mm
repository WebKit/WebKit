/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "AppKitSPI.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <wtf/RetainPtr.h>

#if HAVE(NSSCROLLVIEW_SEPARATOR_TRACKING_ADAPTER)

@interface TitlebarSeparatorTestWKWebView : TestWKWebView
- (id<NSScrollViewSeparatorTrackingAdapter>)separatorTrackingAdapter;
@end

@implementation TitlebarSeparatorTestWKWebView {
    RetainPtr<NSWindow> _hostWindow;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    self = [super initWithFrame:frame configuration:configuration addToWindow:NO];
    if (!self)
        return nil;

    [self synchronouslyLoadTestPageNamed:@"simple-tall"];

    _hostWindow = adoptNS([[NSWindow alloc] initWithContentRect:self.frame styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:YES]);
    [[_hostWindow contentView] addSubview:self];
    [_hostWindow makeKeyAndOrderFront:nil];

    return self;
}

- (id<NSScrollViewSeparatorTrackingAdapter>)separatorTrackingAdapter
{
    ASSERT([self conformsToProtocol:@protocol(NSScrollViewSeparatorTrackingAdapter)]);
    return (id<NSScrollViewSeparatorTrackingAdapter>)self;
}

@end

TEST(WKWebViewTitlebarSeparatorTests, ScrollWithTitlebarAdjacency)
{
    auto webView = adoptNS([[TitlebarSeparatorTestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto separatorTrackingAdapter = [webView separatorTrackingAdapter];
    EXPECT_FALSE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);

    [webView stringByEvaluatingJavaScript:@"scrollTo(0, 1000)"];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);

    [webView stringByEvaluatingJavaScript:@"scrollTo(0, 0)"];
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);
}

TEST(WKWebViewTitlebarSeparatorTests, NavigationResetsTitlebarAppearance)
{
    auto webView = adoptNS([[TitlebarSeparatorTestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto separatorTrackingAdapter = [webView separatorTrackingAdapter];
    EXPECT_FALSE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);

    [webView stringByEvaluatingJavaScript:@"scrollTo(0, 1000)"];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);

    [webView synchronouslyLoadTestPageNamed:@"simple-tall"];
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);
}

TEST(WKWebViewTitlebarSeparatorTests, ScrollWithoutTitlebarAdjacency)
{
    auto webView = adoptNS([[TitlebarSeparatorTestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setFrameSize:NSMakeSize(800, 500)];

    auto separatorTrackingAdapter = [webView separatorTrackingAdapter];
    EXPECT_FALSE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);

    [webView stringByEvaluatingJavaScript:@"scrollTo(0, 1000)"];
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);
}

TEST(WKWebViewTitlebarSeparatorTests, ChangeTitlebarAdjacency)
{
    auto webView = adoptNS([[TitlebarSeparatorTestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto separatorTrackingAdapter = [webView separatorTrackingAdapter];
    EXPECT_FALSE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);

    [webView stringByEvaluatingJavaScript:@"scrollTo(0, 1000)"];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);

    [webView setFrameSize:NSMakeSize(800, 500)];
    EXPECT_FALSE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);

    [webView setFrameSize:NSMakeSize(800, 600)];
    EXPECT_TRUE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);
}

TEST(WKWebViewTitlebarSeparatorTests, ChangeViewVisibility)
{
    auto webView = adoptNS([[TitlebarSeparatorTestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto separatorTrackingAdapter = [webView separatorTrackingAdapter];
    EXPECT_FALSE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);

    [webView stringByEvaluatingJavaScript:@"scrollTo(0, 1000)"];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);

    [webView setHidden:YES];
    EXPECT_FALSE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);

    [webView setHidden:NO];
    EXPECT_TRUE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);
}

TEST(WKWebViewTitlebarSeparatorTests, BackForwardCache)
{
    auto webView = adoptNS([[TitlebarSeparatorTestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto separatorTrackingAdapter = [webView separatorTrackingAdapter];
    EXPECT_FALSE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);

    [webView stringByEvaluatingJavaScript:@"scrollTo(0, 1000)"];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);

    [webView synchronouslyGoBack];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);

    [webView synchronouslyGoForward];
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);
}

TEST(WKWebViewTitlebarSeparatorTests, ParentWhileScrolled)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto rect = CGRectMake(0, 0, 800, 600);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:rect configuration:configuration.get() addToWindow:NO]);
    [webView synchronouslyLoadTestPageNamed:@"simple-tall"];
    [webView stringByEvaluatingJavaScript:@"scrollTo(0, 1000)"];
    [webView waitForNextPresentationUpdate];

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:rect styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];
    [window makeKeyAndOrderFront:nil];

    auto separatorTrackingAdapter = (id<NSScrollViewSeparatorTrackingAdapter>)webView.get();
    EXPECT_TRUE([separatorTrackingAdapter hasScrolledContentsUnderTitlebar]);
}

#endif // HAVE(NSSCROLLVIEW_SEPARATOR_TRACKING_ADAPTER)

#endif // PLATFORM(MAC)
