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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestCocoa.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "PoseAsClass.h"
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

void simulateBackSwipeAndWaitForGestureToEnd(WKWebView *webView, Function<void()>&& runBeforeCompletion = { })
{
    [webView _beginBackSwipeForTesting];
    if (runBeforeCompletion)
        runBeforeCompletion();

    [webView _completeBackSwipeForTesting];

    Util::waitForConditionWithLogging([webView] -> bool {
        return [webView _didCallEndSwipeGestureForTesting];
    }, 3, @"Timed out waiting for endSwipeGesture to be called");
}

TEST(NavigationSwipeTests, RestoreFirstResponderAfterNavigationSwipe)
{
    poseAsClass("TestNavigationInteractiveTransition", "_UINavigationInteractiveTransitionBase");

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setAllowsBackForwardNavigationGestures:YES];
    [webView becomeFirstResponder];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView synchronouslyLoadTestPageNamed:@"simple2"];

    simulateBackSwipeAndWaitForGestureToEnd(webView.get());
    EXPECT_TRUE([webView _contentViewIsFirstResponder]);
}

TEST(NavigationSwipeTests, DoNotBecomeFirstResponderAfterNavigationSwipeIfWebViewIsUnparented)
{
    poseAsClass("TestNavigationInteractiveTransition", "_UINavigationInteractiveTransitionBase");

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setAllowsBackForwardNavigationGestures:YES];
    [webView becomeFirstResponder];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView synchronouslyLoadTestPageNamed:@"simple2"];

    simulateBackSwipeAndWaitForGestureToEnd(webView.get(), [webView] {
        [webView removeFromSuperview];
    });

    EXPECT_FALSE([webView _contentViewIsFirstResponder]);
}

TEST(NavigationSwipeTests, DoNotAssertWhenSnapshottingZeroSizeView)
{
    poseAsClass("TestNavigationInteractiveTransition", "_UINavigationInteractiveTransitionBase");

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero]);
    [webView setAllowsBackForwardNavigationGestures:YES];
    [webView becomeFirstResponder];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView synchronouslyLoadTestPageNamed:@"simple2"];

    simulateBackSwipeAndWaitForGestureToEnd(webView.get());
}

static constexpr CGFloat initialObscuredInsetTop = 100;
static constexpr CGFloat changedObscuredInsetTop = 50;
static constexpr CGFloat contentOffsetWhenLeavingPage = 800;

static RetainPtr<TestWKWebView> createWebViewAndChangeObscuredInsetsAfterLeavingScrolledPage(const HTTPServer& server)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setAllowsBackForwardNavigationGestures:YES];
    [webView _setObscuredInsets:UIEdgeInsetsMake(initialObscuredInsetTop, 0, 0, 0)];

    [webView synchronouslyLoadRequest:server.request("/first"_s)];
    [webView waitForNextPresentationUpdate];

    [[webView scrollView] setContentOffset:CGPointMake(0, contentOffsetWhenLeavingPage)];
    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];

    [webView synchronouslyLoadRequest:server.request("/second"_s)];
    [webView waitForNextPresentationUpdate];

    // Simulate the client collapsing its bars after leaving the first page.
    [webView _setObscuredInsets:UIEdgeInsetsMake(changedObscuredInsetTop, 0, 0, 0)];
    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];

    return webView;
}

static void waitForScrollPositionRestoration(TestWKWebView *webView)
{
    Util::waitForConditionWithLogging([webView] -> bool {
        return [webView scrollView].contentOffset.y > 0;
    }, 3, @"Timed out waiting for the scroll position to be restored");
    [webView waitForNextPresentationUpdate];
}

static HTTPServer createServerWithTallPages()
{
    static constexpr auto tallPage = "<meta name='viewport' content='width=device-width, initial-scale=1'><body style='margin: 0; height: 5000px'></body>"_s;
    return HTTPServer({
        { "/first"_s, { tallPage } },
        { "/second"_s, { tallPage } },
    });
}

TEST(NavigationSwipeTests, RestoredScrollPositionMatchesSwipeSnapshotAfterObscuredInsetsChange)
{
    poseAsClass("TestNavigationInteractiveTransition", "_UINavigationInteractiveTransitionBase");

    auto server = createServerWithTallPages();
    RetainPtr webView = createWebViewAndChangeObscuredInsetsAfterLeavingScrolledPage(server);
    [webView becomeFirstResponder];

    simulateBackSwipeAndWaitForGestureToEnd(webView);
    waitForScrollPositionRestoration(webView);

    // The swipe snapshot was taken at the content offset at which the page was left, so the page
    // should be restored to that content offset to avoid shifting when the snapshot is removed.
    EXPECT_EQ([webView scrollView].contentOffset.y, contentOffsetWhenLeavingPage);
}

TEST(NavigationSwipeTests, RestoredScrollPositionWithoutSwipeAccountsForObscuredInsetsChange)
{
    auto server = createServerWithTallPages();
    RetainPtr webView = createWebViewAndChangeObscuredInsetsAfterLeavingScrolledPage(server);

    [webView synchronouslyGoBack];
    waitForScrollPositionRestoration(webView);

    // Without a swipe snapshot, the content that was at the top of the unobscured area should remain there.
    EXPECT_EQ([webView scrollView].contentOffset.y, contentOffsetWhenLeavingPage + initialObscuredInsetTop - changedObscuredInsetTop);
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
