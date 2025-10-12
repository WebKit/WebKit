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

#import "PoseAsClass.h"
#import "TestCocoa.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPIForTesting.h"

@interface TestNavigationInteractiveTransition : UIPercentDrivenInteractiveTransition
@end

@implementation TestNavigationInteractiveTransition

- (void)startInteractiveTransition:(id<UIViewControllerContextTransitioning>)transitionContext
{
    [super startInteractiveTransition:transitionContext];
    EXPECT_TRUE([transitionContext.containerView.window.firstResponder resignFirstResponder]);
}

@end
#endif // PLATFORM(IOS_FAMILY)

namespace TestWebKitAPI {

#if PLATFORM(IOS_FAMILY)

TEST(NavigationSwipeTests, RestoreFirstResponderAfterNavigationSwipe)
{
    poseAsClass("TestNavigationInteractiveTransition", "_UINavigationInteractiveTransitionBase");

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setAllowsBackForwardNavigationGestures:YES];
    [webView becomeFirstResponder];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView synchronouslyLoadTestPageNamed:@"simple2"];

    [webView _beginBackSwipeForTesting];
    [webView _completeBackSwipeForTesting];
    EXPECT_TRUE([webView _contentViewIsFirstResponder]);
}

TEST(NavigationSwipeTests, DoNotBecomeFirstResponderAfterNavigationSwipeIfWebViewIsUnparented)
{
    poseAsClass("TestNavigationInteractiveTransition", "_UINavigationInteractiveTransitionBase");

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setAllowsBackForwardNavigationGestures:YES];
    [webView becomeFirstResponder];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView synchronouslyLoadTestPageNamed:@"simple2"];

    [webView _beginBackSwipeForTesting];
    [webView removeFromSuperview];
    [webView _completeBackSwipeForTesting];
    EXPECT_FALSE([webView _contentViewIsFirstResponder]);
}

TEST(NavigationSwipeTests, DoNotAssertWhenSnapshottingZeroSizeView)
{
    poseAsClass("TestNavigationInteractiveTransition", "_UINavigationInteractiveTransitionBase");

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero]);
    [webView setAllowsBackForwardNavigationGestures:YES];
    [webView becomeFirstResponder];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView synchronouslyLoadTestPageNamed:@"simple2"];

    [webView _beginBackSwipeForTesting];
    [webView _completeBackSwipeForTesting];
}

#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)

TEST(NavigationSwipeTests, WindowRelativeBoundsForCustomSwipeViews)
{
    RetainPtr customSwipeView = adoptNS([[NSView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setAllowsBackForwardNavigationGestures:YES];
    [webView addSubview:customSwipeView.get()];
    [webView _setCustomSwipeViews:@[ customSwipeView.get() ]];
    [webView _setCustomSwipeViewsObscuredContentInsets:NSEdgeInsetsMake(100, 200, 50, 50)];

    auto boundsForCustomSwipeView = [webView _windowRelativeBoundsForCustomSwipeViewsForTesting];
    EXPECT_EQ(boundsForCustomSwipeView.origin.x, 200.0);
    EXPECT_EQ(boundsForCustomSwipeView.origin.y, 50.0);
    EXPECT_EQ(boundsForCustomSwipeView.size.width, 550.0);
    EXPECT_EQ(boundsForCustomSwipeView.size.height, 450.0);
}

TEST(NavigationSwipeTests, SwipeSnapshotLayerBounds)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setAllowsBackForwardNavigationGestures:YES];
    [webView _setAutomaticallyAdjustsContentInsets:NO];
    [webView _setObscuredContentInsets:NSEdgeInsetsMake(100, 200, 0, 0) immediate:NO];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    [webView synchronouslyLoadTestPageNamed:@"simple2"];
    [webView waitForNextPresentationUpdate];

    [webView _beginBackSwipeForTesting];

    RetainPtr swipeSnapshotLayer = [webView firstLayerWithName:@"Gesture Swipe Snapshot Layer"];
    EXPECT_EQ([swipeSnapshotLayer frame], NSMakeRect(200, 0, 600, 500));
}

#endif // PLATFORM(MAC)

} // namespace TestWebKitAPI
