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

#if HAVE(NSREFRESHCONTROLLER)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/mac/AppKitSPI.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

@interface TestRefreshControllerTarget : NSObject
@property (nonatomic, readonly) NSUInteger activationCount;
@property (nonatomic, weak) WKWebView *webView;
- (void)refreshControllerActivated:(NSRefreshController *)sender;
@end

@implementation TestRefreshControllerTarget

- (void)refreshControllerActivated:(NSRefreshController *)sender
{
    ++_activationCount;
    [_webView reload];
}

@end

namespace TestWebKitAPI {

static void pullDownAndWaitForRefresh(TestWKWebView *webView, NSRefreshController *refreshController)
{
    auto eventLocation = NSMakePoint(NSMidX([webView bounds]), NSMidY([webView bounds]));
    [webView wheelEventAtPoint:eventLocation wheelDelta:CGSizeMake(0, 1) phase:kCGScrollPhaseBegan momentumPhase:kCGMomentumScrollPhaseNone];

    for (int i = 0; i < 200 && ![refreshController isRefreshing]; ++i) {
        [webView wheelEventAtPoint:eventLocation wheelDelta:CGSizeMake(0, 100) phase:kCGScrollPhaseChanged momentumPhase:kCGMomentumScrollPhaseNone];
        [webView waitForNextPresentationUpdate];
    }

    [webView wheelEventAtPoint:eventLocation wheelDelta:CGSizeMake(0, 0) phase:kCGScrollPhaseEnded momentumPhase:kCGMomentumScrollPhaseNone];
    [webView waitForNextPresentationUpdate];
}

class NSRefreshControllerTests : public testing::Test {
public:
    RetainPtr<TestWKWebView> webView;
    RetainPtr<NSRefreshController> refreshController;
    RetainPtr<TestRefreshControllerTarget> target;

    void setUpWithWindowHeight(CGFloat windowHeight)
    {
        RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, windowHeight) configuration:configuration.get() addToWindow:YES]);

        refreshController = adoptNS([[NSRefreshController alloc] init]);
        target = adoptNS([[TestRefreshControllerTarget alloc] init]);
        [target setWebView:webView.get()];
        [refreshController setTarget:target.get()];
        [refreshController setAction:@selector(refreshControllerActivated:)];
        [webView setRefreshController:refreshController.get()];
    }
};

TEST_F(NSRefreshControllerTests, Basic)
{
    setUpWithWindowHeight(600);

    [webView synchronouslyLoadHTMLString:@"<body style='height: 2000px;'></body>"];
    [webView waitForNextPresentationUpdate];

    pullDownAndWaitForRefresh(webView.get(), refreshController.get());

    EXPECT_EQ([target activationCount], 1U);
    EXPECT_TRUE([refreshController isRefreshing]);
}

TEST_F(NSRefreshControllerTests, DoesNotRefireDuringSettleAtSmallWindowHeight)
{
    setUpWithWindowHeight(200);

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadHTMLString:@"<body style='height: 2000px;'></body>" baseURL:nil];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    __block bool reloadFinished = false;
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        reloadFinished = true;
    }];

    pullDownAndWaitForRefresh(webView.get(), refreshController.get());
    EXPECT_EQ([target activationCount], 1U);
    ASSERT_TRUE([refreshController isRefreshing]);


    Util::run(&reloadFinished);
    [refreshController endRefreshing];

    Util::waitForConditionWithLogging([&] {
        return ![refreshController isRefreshing];
    }, 5, @"Timed out waiting for refresh controller to leave the refreshing state.");

    Util::runFor(1_s);
    EXPECT_EQ([target activationCount], 1U);
    EXPECT_FALSE([refreshController isRefreshing]);
}

} // namespace TestWebKitAPI

#endif // HAVE(NSREFRESHCONTROLLER)
