/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>

#if PLATFORM(IOS_FAMILY)
@interface RestoreScrollPositionWithLargeContentInsetWebView : TestWKWebView
@end

@implementation RestoreScrollPositionWithLargeContentInsetWebView
- (UIEdgeInsets)safeAreaInsets
{
    return UIEdgeInsetsMake(141, 0, 0, 0);
}
@end
#endif

namespace TestWebKitAPI {

#if PLATFORM(IOS_FAMILY)

TEST(RestoreScrollPositionTests, RestoreScrollPositionWithLargeContentInset)
{
    auto topInset = 1165;

    auto webView = adoptNS([[RestoreScrollPositionWithLargeContentInsetWebView alloc] initWithFrame:CGRectMake(0, 0, 375, 1024)]);
    
    [webView synchronouslyLoadTestPageNamed:@"simple-tall"];
    
    auto insets = UIEdgeInsetsMake(topInset, 0, 0, 0);
    [webView scrollView].contentInset = insets;
    [webView _setObscuredInsets:UIEdgeInsetsMake(141, 0, 0, 0)];
    [webView scrollView].contentOffset = CGPointMake(0, -topInset);
    
    [webView waitForNextPresentationUpdate];
    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(375, 727) maximumUnobscuredSizeOverride:CGSizeMake(375, 727)];

    [webView scrollView].contentInset = UIEdgeInsetsMake(1024, 0, 0, 0);
    [webView waitForNextPresentationUpdate];
    
    CGPoint contentOffsetAfterScrolling = [webView scrollView].contentOffset;
    
    EXPECT_EQ(-topInset, contentOffsetAfterScrolling.y);
}

TEST(RestoreScrollPositionTests, RestoreScrollPositionDuringResize)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().pageCacheEnabled = NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:webViewConfiguration.get()]);

    [webView synchronouslyLoadTestPageNamed:@"simple-tall"];
    [webView stringByEvaluatingJavaScript:@"scrollTo(0, 1000)"];
    [webView waitForNextPresentationUpdate];

    UIEdgeInsets contentInset = [[webView scrollView] adjustedContentInset];
    // If this fires you're probably running using the wrong type of simulated device.
    ASSERT_UNUSED(contentInset, !contentInset.top);

    CGPoint contentOffsetAfterScrolling = [webView scrollView].contentOffset;
    EXPECT_EQ(0, contentOffsetAfterScrolling.x);
    EXPECT_EQ(1000, contentOffsetAfterScrolling.y);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    CGPoint contentOffsetInNewPage = [webView scrollView].contentOffset;
    EXPECT_EQ(0, contentOffsetInNewPage.x);
    EXPECT_EQ(0, contentOffsetInNewPage.y);

    [webView _beginAnimatedResizeWithUpdates:^{
        [webView setFrame:CGRectMake(0, 0, [webView frame].size.width + 100, 500)];
    }];
    [webView synchronouslyGoBack];

    TestWebKitAPI::Util::runFor(0.5_s);
    [webView _endAnimatedResize];

    // Should restore the scroll position.
    int timeout = 0;
    do {
        if (timeout)
            TestWebKitAPI::Util::runFor(0.1_s);
    } while ([webView scrollView].contentOffset.y != 1000 && ++timeout <= 30);

    CGPoint contentOffsetAfterBack = [webView scrollView].contentOffset;
    EXPECT_EQ(0, contentOffsetAfterBack.x);
    EXPECT_EQ(1000, contentOffsetAfterBack.y);
}

TEST(RestoreScrollPositionTests, RestoreScrollPositionAfterBack)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 390, 664)]);

    [webView synchronouslyLoadTestPageNamed:@"simple-tall"];
    [webView _setViewScale:1.15]; // Simulate MobileSafari setting view scale on every didCommitNavigation.
    [webView waitForNextPresentationUpdate];

    [[webView scrollView] setContentOffset:CGPointMake(0, 1000)];
    [webView waitForNextPresentationUpdate];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView _setViewScale:1.15];
    [webView waitForNextPresentationUpdate];

    CGPoint contentOffsetInNewPage = [webView scrollView].contentOffset;
    EXPECT_EQ(0, contentOffsetInNewPage.x);
    EXPECT_EQ(0, contentOffsetInNewPage.y);

    [webView synchronouslyGoBack];
    [webView _setViewScale:1.15];
    [webView waitForNextPresentationUpdate];

    CGPoint contentOffsetAfterBack = [webView scrollView].contentOffset;
    EXPECT_EQ(0, contentOffsetAfterBack.x);
    EXPECT_TRUE(WTF::areEssentiallyEqual<float>(contentOffsetAfterBack.y, 1000.0f, 1.0f)); // It can be 999.5 but that's OK.
}
#endif

}
