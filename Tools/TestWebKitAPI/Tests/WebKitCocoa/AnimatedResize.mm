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

#import "config.h"

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferences.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)

#if HAVE(UI_WINDOW_SCENE_LIVE_RESIZE)
@interface WKWebView ()
- (void)_beginLiveResize;
- (void)_endLiveResize;
@end
#endif

static bool didLayout;
static bool didEndAnimatedResize;
static bool didChangeSafeAreaShouldAffectObscuredInsets;

@interface AnimatedResizeWebView : WKWebView <WKUIDelegate>

@end

@implementation AnimatedResizeWebView

- (void)_endAnimatedResize
{
    didEndAnimatedResize = true;

    [super _endAnimatedResize];
}

- (void)_webView:(WKWebView *)webView didChangeSafeAreaShouldAffectObscuredInsets:(BOOL)safeAreaShouldAffectObscuredInsets
{
    didChangeSafeAreaShouldAffectObscuredInsets = true;
}

@end

static RetainPtr<AnimatedResizeWebView> createAnimatedResizeWebView()
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [processPoolConfiguration setIgnoreSynchronousMessagingTimeoutsForTesting:YES];
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];

    auto webView = adoptNS([[AnimatedResizeWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

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

TEST(AnimatedResize, DISABLED_ResizeWithHiddenContentDoesNotHang)
{
    auto webView = createAnimatedResizeWebView();
    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"blinking-div" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    auto navigationDelegate = createFirstVisuallyNonEmptyWatchingNavigationDelegate();
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto window = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
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

TEST(AnimatedResize, AnimatedResizeDoesNotHang)
{
    auto webView = createAnimatedResizeWebView();
    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"blinking-div" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    auto navigationDelegate = createFirstVisuallyNonEmptyWatchingNavigationDelegate();
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto window = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
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

TEST(AnimatedResize, AnimatedResizeBlocksViewportFitChanges)
{
    auto webView = createAnimatedResizeWebView();
    [webView setUIDelegate:webView.get()];

    // We need to have something loaded before beginning the animated
    // resize, or it will bail.
    [webView loadHTMLString:@"<head></head>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    auto window = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [window addSubview:webView.get()];
    [window setHidden:NO];

    [webView _beginAnimatedResizeWithUpdates:^{
        [webView setFrame:CGRectMake(0, 0, [webView frame].size.width + 100, 400)];
    }];

    // Load a page that will change the state of viewport-fit,
    // in the middle of the resize.
    [webView loadHTMLString:@"<head><meta name='viewport' content='viewport-fit=cover'></head>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    didChangeSafeAreaShouldAffectObscuredInsets = false;

    // Wait for a commit to come in /after/ loading the viewport-fit=cover
    // page, and ensure that we didn't call the UIDelegate callback,
    // because we're still in the resize. Then, end the resize.
    [webView _doAfterNextPresentationUpdateWithoutWaitingForAnimatedResizeForTesting:^{
        EXPECT_FALSE(didChangeSafeAreaShouldAffectObscuredInsets);
        [webView _endAnimatedResize];
    }];

    TestWebKitAPI::Util::run(&didEndAnimatedResize);
    didEndAnimatedResize = false;

    // Wait for one more commit so that we see the viewport-fit state
    // change actually take place (post-resize), and ensure that it does.
    __block bool didGetCommitAfterEndAnimatedResize = false;
    [webView _doAfterNextPresentationUpdate:^{
        didGetCommitAfterEndAnimatedResize = true;
    }];
    TestWebKitAPI::Util::run(&didGetCommitAfterEndAnimatedResize);

    EXPECT_TRUE(didChangeSafeAreaShouldAffectObscuredInsets);
}

TEST(AnimatedResize, OverrideLayoutSizeChangesDuringAnimatedResizeSucceed)
{
    auto webView = createAnimatedResizeWebView();
    [webView setUIDelegate:webView.get()];

    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(200, 50) maximumUnobscuredSizeOverride:CGSizeMake(200, 50)];

    [webView loadHTMLString:@"<head><meta name='viewport' content='initial-scale=1'></head>" baseURL:nil];
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForDidFinishNavigation];

    auto window = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [window addSubview:webView.get()];
    [window setHidden:NO];

    [webView _beginAnimatedResizeWithUpdates:^{
        [webView setFrame:CGRectMake(0, 0, [webView frame].size.width + 100, 400)];
    }];

    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(100, 200) maximumUnobscuredSizeOverride:CGSizeMake(100, 200)];
    [webView _endAnimatedResize];

    __block bool didReadLayoutSize = false;

    [webView _doAfterNextPresentationUpdate:^{
        [webView evaluateJavaScript:@"[window.innerWidth, window.innerHeight]" completionHandler:^(id value, NSError *error) {
            CGFloat innerWidth = [[value objectAtIndex:0] floatValue];
            CGFloat innerHeight = [[value objectAtIndex:1] floatValue];

            EXPECT_EQ(innerWidth, 100);
            EXPECT_EQ(innerHeight, 200);

            didReadLayoutSize = true;
        }];
    }];
    
    TestWebKitAPI::Util::run(&didReadLayoutSize);
}

TEST(AnimatedResize, OverrideLayoutSizeIsRestoredAfterProcessRelaunch)
{
    auto webView = createAnimatedResizeWebView();
    [webView setUIDelegate:webView.get()];

    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(200, 50) maximumUnobscuredSizeOverride:CGSizeMake(200, 50)];

    [webView loadHTMLString:@"<head><meta name='viewport' content='initial-scale=1'></head>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    auto window = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [window addSubview:webView.get()];
    [window setHidden:NO];

    [webView _killWebContentProcessAndResetState];
    [webView loadHTMLString:@"<head><meta name='viewport' content='initial-scale=1'></head>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    __block bool didReadLayoutSize = false;
    [webView evaluateJavaScript:@"[window.innerWidth, window.innerHeight]" completionHandler:^(id value, NSError *error) {
        CGFloat innerWidth = [[value objectAtIndex:0] floatValue];
        CGFloat innerHeight = [[value objectAtIndex:1] floatValue];

        EXPECT_EQ(innerWidth, 200);
        EXPECT_EQ(innerHeight, 50);

        didReadLayoutSize = true;
    }];
    TestWebKitAPI::Util::run(&didReadLayoutSize);
}

TEST(AnimatedResize, OverrideLayoutSizeIsRestoredAfterChangingDuringProcessRelaunch)
{
    auto webView = createAnimatedResizeWebView();
    [webView setUIDelegate:webView.get()];

    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(100, 100) maximumUnobscuredSizeOverride:CGSizeMake(100, 100)];

    [webView loadHTMLString:@"<head><meta name='viewport' content='initial-scale=1'></head>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    auto window = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [window addSubview:webView.get()];
    [window setHidden:NO];

    [webView _killWebContentProcessAndResetState];
    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(200, 50) maximumUnobscuredSizeOverride:CGSizeMake(200, 50)];

    [webView loadHTMLString:@"<head><meta name='viewport' content='initial-scale=1'></head>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    __block bool didReadLayoutSize = false;
    [webView evaluateJavaScript:@"[window.innerWidth, window.innerHeight]" completionHandler:^(id value, NSError *error) {
        CGFloat innerWidth = [[value objectAtIndex:0] floatValue];
        CGFloat innerHeight = [[value objectAtIndex:1] floatValue];

        EXPECT_EQ(innerWidth, 200);
        EXPECT_EQ(innerHeight, 50);

        didReadLayoutSize = true;
    }];
    TestWebKitAPI::Util::run(&didReadLayoutSize);
}

TEST(AnimatedResize, ChangeFrameAndMinimumEffectiveDeviceWidthDuringAnimatedResize)
{
    auto webView = createAnimatedResizeWebView();
    [[webView configuration] preferences]._shouldIgnoreMetaViewport = YES;
    [webView setUIDelegate:webView.get()];
    [webView loadHTMLString:@"<body>Hello world</body>" baseURL:nil];

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [navigationDelegate waitForDidFinishNavigation];

    auto window = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [window addSubview:webView.get()];
    [window setHidden:NO];

    [webView _beginAnimatedResizeWithUpdates:^{
        [webView _setMinimumEffectiveDeviceWidth:1000];
        [webView setFrame:CGRectMake(0, 0, 500, 375)];
    }];

    [webView _endAnimatedResize];

    __block bool didReadLayoutSize = false;
    [webView _doAfterNextPresentationUpdate:^{
        [webView evaluateJavaScript:@"[window.innerWidth, window.innerHeight, visualViewport.scale]" completionHandler:^(NSArray<NSNumber *> *value, NSError *error) {
            CGFloat innerWidth = value[0].floatValue;
            CGFloat innerHeight = value[1].floatValue;
            CGFloat scale = value[2].floatValue;
            EXPECT_EQ(innerWidth, 1000);
            EXPECT_EQ(innerHeight, 750);
            EXPECT_EQ(scale, 0.5);
            didReadLayoutSize = true;
        }];
    }];

    TestWebKitAPI::Util::run(&didReadLayoutSize);
}

static UIView *immediateSubviewOfClass(UIView *view, Class cls)
{
    UIView *foundSubview = nil;
    
    for (UIView *subview in view.subviews) {
        if ([subview isKindOfClass:cls]) {
            // Make it harder to write a bad test; if there's more than one subview
            // of the given class, fail the test!
            ASSERT(!foundSubview);

            foundSubview = subview;
        }
    }

    return foundSubview;
}

TEST(AnimatedResize, ResizeWithContentHiddenCompletes)
{
    auto webView = createAnimatedResizeWebView();
    [webView setUIDelegate:webView.get()];
    
    [webView loadHTMLString:@"<head><meta name='viewport' content='initial-scale=1'></head>" baseURL:nil];
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForDidFinishNavigation];
    
    auto window = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [window addSubview:webView.get()];
    [window setHidden:NO];
    
    [webView _resizeWhileHidingContentWithUpdates:^{
        [webView setFrame:CGRectMake(0, 0, 100, 200)];
    }];
    
    __block bool didReadLayoutSize = false;
    [webView _doAfterNextPresentationUpdate:^{
        [webView evaluateJavaScript:@"[window.innerWidth, window.innerHeight]" completionHandler:^(id value, NSError *error) {
            CGFloat innerWidth = [[value objectAtIndex:0] floatValue];
            CGFloat innerHeight = [[value objectAtIndex:1] floatValue];
            
            EXPECT_EQ(innerWidth, 100);
            EXPECT_EQ(innerHeight, 200);
            
            didReadLayoutSize = true;
        }];
    }];
    TestWebKitAPI::Util::run(&didReadLayoutSize);
    
    UIView *scrollView = immediateSubviewOfClass(webView.get(), NSClassFromString(@"WKScrollView"));
    UIView *contentView = immediateSubviewOfClass(scrollView, NSClassFromString(@"WKContentView"));
    
    // Make sure that we've put the view hierarchy back together after the resize completed.
    EXPECT_NOT_NULL(scrollView);
    EXPECT_NOT_NULL(contentView);
    EXPECT_FALSE(contentView.hidden);
}

TEST(AnimatedResize, ResizeWithContentHiddenWithSubsequentNoOpResizeCompletes)
{
    auto webView = createAnimatedResizeWebView();
    [webView setUIDelegate:webView.get()];

    [webView loadHTMLString:@"<head><meta name='viewport' content='initial-scale=1'></head>" baseURL:nil];
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [navigationDelegate waitForDidFinishNavigation];

    auto window = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [window addSubview:webView.get()];
    [window setHidden:NO];

    [webView _resizeWhileHidingContentWithUpdates:^{
        [webView setFrame:CGRectMake(0, 0, 100, 200)];
    }];

    [webView _beginAnimatedResizeWithUpdates:^{
        [webView setFrame:CGRectMake(0, 0, 100, 200)];
    }];

    [webView _endAnimatedResize];

    __block bool didReadLayoutSize = false;
    [webView _doAfterNextPresentationUpdate:^{
        [webView evaluateJavaScript:@"[window.innerWidth, window.innerHeight]" completionHandler:^(id value, NSError *error) {
            CGFloat innerWidth = [[value objectAtIndex:0] floatValue];
            CGFloat innerHeight = [[value objectAtIndex:1] floatValue];

            EXPECT_EQ(innerWidth, 100);
            EXPECT_EQ(innerHeight, 200);

            didReadLayoutSize = true;
        }];
    }];
    TestWebKitAPI::Util::run(&didReadLayoutSize);

    UIView *scrollView = immediateSubviewOfClass(webView.get(), NSClassFromString(@"WKScrollView"));
    UIView *contentView = immediateSubviewOfClass(scrollView, NSClassFromString(@"WKContentView"));

    // Make sure that we've put the view hierarchy back together after the resize completed.
    EXPECT_NOT_NULL(scrollView);
    EXPECT_NOT_NULL(contentView);
    EXPECT_FALSE(contentView.hidden);
}

TEST(AnimatedResize, AnimatedResizeBlocksDoAfterNextPresentationUpdate)
{
    auto webView = createAnimatedResizeWebView();
    [webView setUIDelegate:webView.get()];

    // We need to have something loaded before beginning the animated
    // resize, or it will bail.
    [webView loadHTMLString:@"<head></head>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    auto window = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [window addSubview:webView.get()];
    [window setHidden:NO];

    [webView _beginAnimatedResizeWithUpdates:^{
        [webView setFrame:CGRectMake(0, 0, [webView frame].size.width + 100, 400)];
    }];

    __block bool didGetCommitAfterEndAnimatedResize = false;

    // Despite being invoked first, this doAfterNextPresentationUpdate
    // should be deferred until after we call endAnimatedResize, inside
    // the below _doAfterNextPresentationUpdateWithoutWaitingForAnimatedResize.
    [webView _doAfterNextPresentationUpdate:^{
        EXPECT_TRUE(didEndAnimatedResize);
        didGetCommitAfterEndAnimatedResize = true;
    }];

    [webView _doAfterNextPresentationUpdateWithoutWaitingForAnimatedResizeForTesting:^{
        [webView _endAnimatedResize];
    }];

    TestWebKitAPI::Util::run(&didEndAnimatedResize);
    TestWebKitAPI::Util::run(&didGetCommitAfterEndAnimatedResize);
}

TEST(AnimatedResize, CreateWebPageAfterAnimatedResize)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 1024, 768)]);
    [webView synchronouslyLoadTestPageNamed:@"large-red-square-image"];

    [webView _beginAnimatedResizeWithUpdates:^{
        [webView setFrame:CGRectMake(0, 0, 768, 1024)];
    }];

    __block bool doneWaitingForPresentationUpdate = false;
    [webView _doAfterNextPresentationUpdate:^{
        doneWaitingForPresentationUpdate = true;
    }];

    [webView _doAfterNextPresentationUpdateWithoutWaitingForAnimatedResizeForTesting:^{
        [webView _endAnimatedResize];
    }];

    TestWebKitAPI::Util::run(&doneWaitingForPresentationUpdate);

    [webView _killWebContentProcessAndResetState];
    [webView synchronouslyLoadTestPageNamed:@"large-red-square-image"];

    NSArray<NSNumber *> *dimensions = [webView objectByEvaluatingJavaScript:@"[innerWidth, innerHeight]"];
    EXPECT_EQ(768, dimensions.firstObject.intValue);
    EXPECT_EQ(1024, dimensions.lastObject.intValue);
}

#if HAVE(UI_WINDOW_SCENE_LIVE_RESIZE)
TEST(AnimatedResize, MinimumEffectiveDeviceWidthChangeIsDeferredDuringLiveResize)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='initial-scale=1' />"];

    EXPECT_EQ([webView scrollView].zoomScale, 1);

    [webView _beginLiveResize];

    [webView _setMinimumEffectiveDeviceWidth:400];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ([webView scrollView].zoomScale, 1);

    [webView _endLiveResize];

    [webView waitForNextPresentationUpdate];
    EXPECT_EQ([webView scrollView].zoomScale, 0.5);
}
#endif

TEST(AnimatedResize, ResizeWithWithSubsequentNoOpResizeIsNotCancelled)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 1024, 768)]);
    [webView synchronouslyLoadHTMLString:@"<head><meta name='viewport' content='initial-scale=1'></head>"];

    UIView *scrollView = immediateSubviewOfClass(webView.get(), NSClassFromString(@"WKScrollView"));
    UIView *contentView = immediateSubviewOfClass(scrollView, NSClassFromString(@"WKContentView"));

    [webView _resizeWhileHidingContentWithUpdates:^{
        [webView setFrame:CGRectMake(0, 0, 100, 200)];
    }];

    EXPECT_TRUE(contentView.hidden);

    [webView _resizeWhileHidingContentWithUpdates:^{
        [webView setFrame:CGRectMake(0, 0, 100, 200)];
    }];

    // The animated resize shouldn't be cancelled just because we did a no-op resize;
    // the first one is still in progress at this point.
    EXPECT_TRUE(contentView.hidden);

    // Eventually we should get a commit that un-hides the content view.
    while (1) {
        [webView waitForNextPresentationUpdate];
        if (!contentView.hidden)
            break;
    }
}

TEST(AnimatedResize, ScaleDuringAnimatedResizeDoesNotMoveContentViewFrameOrigin)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 1024, 768)]);
    [webView synchronouslyLoadTestPageNamed:@"large-red-square-image"];

    [webView _beginAnimatedResizeWithUpdates:^{
        [webView setFrame:CGRectMake(0, 0, [webView frame].size.width + 100, 400)];
    }];

    [webView _endAnimatedResize];

    [[webView scrollView] setZoomScale:2];
    [webView waitForNextPresentationUpdate];

    auto frameOrigin = [webView wkContentView].frame.origin;
    EXPECT_EQ(frameOrigin.x, 0);
    EXPECT_EQ(frameOrigin.y, 0);
}

#endif
