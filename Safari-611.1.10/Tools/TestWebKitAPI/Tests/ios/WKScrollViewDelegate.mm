/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebView.h>

// WKScrollViewDelegateCrash

static bool delegateIsDeallocated;

@interface TestDelegateForScrollView : NSObject <UIScrollViewDelegate>

@end

@implementation TestDelegateForScrollView

- (void)dealloc
{
    delegateIsDeallocated = true;
    [super dealloc];
}

@end

namespace TestWebKitAPI {

TEST(WKWebView, WKScrollViewDelegateCrash)
{
    WKWebView *webView = [[WKWebView alloc] init];
    TestDelegateForScrollView *delegateForScrollView = [[TestDelegateForScrollView alloc] init];
    @autoreleasepool {
        webView.scrollView.delegate = delegateForScrollView;
    }
    delegateIsDeallocated = false;
    [delegateForScrollView release];
    TestWebKitAPI::Util::run(&delegateIsDeallocated);

    EXPECT_NULL(webView.scrollView.delegate);
}

}

// WKScrollViewDelegateCannotOverrideViewForZooming

static BOOL didCallViewForZoomingInScrollView;

@interface WKScrollViewDelegateWithViewForZoomingOverridden : NSObject <UIScrollViewDelegate>
@property (nonatomic, assign) CGFloat overrideScale;
@end

@implementation WKScrollViewDelegateWithViewForZoomingOverridden

- (UIView *)viewForZoomingInScrollView:(UIScrollView *)scrollView
{
    didCallViewForZoomingInScrollView = true;

    UIView *fakeZoomingView = [[UIView alloc] init];
    fakeZoomingView.layer.affineTransform = CGAffineTransformMakeScale(_overrideScale, _overrideScale);
    return fakeZoomingView;
}

@end

namespace TestWebKitAPI {

TEST(WKWebView, WKScrollViewDelegateCannotOverrideViewForZooming)
{
    TestWKWebView *webView = [[TestWKWebView alloc] init];
    WKScrollViewDelegateWithViewForZoomingOverridden *delegateForScrollView = [[WKScrollViewDelegateWithViewForZoomingOverridden alloc] init];
    @autoreleasepool {
        webView.scrollView.delegate = delegateForScrollView;
    }

    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='initial-scale=1'>"];
    [webView waitForNextPresentationUpdate];

    // Have WKScrollView's external delegate return a view with scale=2 from viewForZoomingInScrollView.
    delegateForScrollView.overrideScale = 2;

    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='initial-scale=2'>"];
    [webView waitForNextPresentationUpdate];

    @autoreleasepool {
        webView.scrollView.delegate = nil;
    }

    EXPECT_FALSE(didCallViewForZoomingInScrollView);
    EXPECT_EQ(2, [[webView scrollView] zoomScale]);
}

}

#endif
