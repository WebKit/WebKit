/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <AVKit/AVKit.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WebKit.h>

#if HAVE(AVKIT)
#import <WebCore/WebAVPlayerLayer.h>
#endif

@interface VideoPresentationModeUIDelegate : NSObject <WKUIDelegate>
- (void)waitForDidEnterFullscreen;
- (void)waitForDidExitFullscreen;
- (void)waitForDidEnterStandby;
- (void)waitForDidExitStandby;
@end

@implementation VideoPresentationModeUIDelegate {
    bool _willEnterFullscreen;
    bool _didEnterFullscreen;
    bool _didExitFullscreen;
    bool _didEnterStandby;
    bool _didExitStandby;
}

- (void)waitForDidEnterFullscreen
{
    _willEnterFullscreen = false;
    _didEnterFullscreen = false;
    TestWebKitAPI::Util::run(&_willEnterFullscreen);
    TestWebKitAPI::Util::run(&_didEnterFullscreen);
}

- (void)waitForDidExitFullscreen
{
    _didExitFullscreen = false;
    TestWebKitAPI::Util::run(&_didExitFullscreen);
}

- (void)waitForDidEnterStandby
{
    _didEnterStandby = false;
    TestWebKitAPI::Util::run(&_didEnterStandby);
}

- (void)waitForDidExitStandby
{
    _didExitStandby = false;
    TestWebKitAPI::Util::run(&_didExitStandby);
}

#pragma mark WKUIDelegate

- (void)_webViewWillEnterFullscreen:(WKWebView *)webView
{
    _willEnterFullscreen = true;
}

- (void)_webViewDidEnterFullscreen:(WKWebView *)webView
{
    _didEnterFullscreen = true;
}

- (void)_webViewDidExitFullscreen:(WKWebView *)webView
{
    _didExitFullscreen = true;
}

- (void)_webViewDidEnterStandbyForTesting:(WKWebView *)webView
{
    _didEnterStandby = true;
}

- (void)_webViewDidExitStandbyForTesting:(WKWebView *)webView
{
    _didExitStandby = true;
}

@end

#pragma mark -

TEST(VideoPresentationMode, Fullscreen)
{
    // This test must run in TestWebKitAPI.app
    if (![NSBundle.mainBundle.bundleIdentifier isEqualToString:@"org.webkit.TestWebKitAPI"])
        return;

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setAllowsInlineMediaPlayback:YES];
    RetainPtr uiDelegate = adoptNS([[VideoPresentationModeUIDelegate alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<video src=video-with-audio.mp4 playsinline loop controls></video>"];
    [webView evaluateJavaScript:@"document.querySelector('video').play()" completionHandler:nil];
    [webView evaluateJavaScript:@"document.querySelector('video').webkitSetPresentationMode('fullscreen')" completionHandler:nil];

    [uiDelegate waitForDidEnterFullscreen];

    UIApplication *application = UIApplication.sharedApplication;
    EXPECT_EQ(application.connectedScenes.count, 1U);

    UIScene *scene = application.connectedScenes.anyObject;
    RELEASE_ASSERT([scene isKindOfClass:UIWindowScene.class]);

    UIViewController *fullScreenViewController = nil;
    for (UIWindow *window in [(UIWindowScene *)scene windows]) {
        UIViewController *presentedViewController = window.rootViewController.presentedViewController;
        if ([presentedViewController isKindOfClass:NSClassFromString(@"AVFullScreenViewController")]) {
            fullScreenViewController = presentedViewController;
            break;
        }
    }
    EXPECT_TRUE(!!fullScreenViewController);

    UIView *fullScreenView = fullScreenViewController.viewIfLoaded;
    EXPECT_TRUE(!!fullScreenView);

    RetainPtr<UIView> playerLayerView;
    RetainPtr viewStack = adoptNS([[NSMutableArray alloc] initWithObjects:fullScreenView, nil]);
    while ([viewStack count]) {
        RetainPtr view = (UIView *)[viewStack lastObject];
        [viewStack removeLastObject];

        if ([view isKindOfClass:NSClassFromString(@"WebAVPlayerLayerView")]) {
            playerLayerView = WTFMove(view);
            break;
        }

        [viewStack addObjectsFromArray:[view subviews]];
    }
    EXPECT_TRUE(!!playerLayerView);

    [webView evaluateJavaScript:@"document.querySelector('video').webkitSetPresentationMode('inline')" completionHandler:nil];

    [uiDelegate waitForDidExitFullscreen];
}

TEST(VideoPresentationMode, Inline)
{
    // This test must run in TestWebKitAPI.app
    if (![NSBundle.mainBundle.bundleIdentifier isEqualToString:@"org.webkit.TestWebKitAPI"])
        return;

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setAllowsInlineMediaPlayback:YES];
    RetainPtr uiDelegate = adoptNS([[VideoPresentationModeUIDelegate alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<video src=video-with-audio.mp4 playsinline loop controls></video>"];
    [webView evaluateJavaScript:@"document.querySelector('video').play()" completionHandler:nil];
    [webView evaluateJavaScript:@"document.querySelector('video').webkitSetPresentationMode('fullscreen')" completionHandler:nil];

    [uiDelegate waitForDidEnterFullscreen];

    [webView evaluateJavaScript:@"document.querySelector('video').webkitSetPresentationMode('inline')" completionHandler:nil];

    [uiDelegate waitForDidExitFullscreen];

    UIApplication *application = UIApplication.sharedApplication;
    EXPECT_EQ(application.connectedScenes.count, 1U);

    UIScene *scene = application.connectedScenes.anyObject;
    RELEASE_ASSERT([scene isKindOfClass:UIWindowScene.class]);

    UIViewController *fullScreenViewController = nil;
    UIWindow *hostWindow = nil;
    for (UIWindow *window in [(UIWindowScene *)scene windows]) {
        UIViewController *presentedViewController = window.rootViewController.presentedViewController;
        if ([presentedViewController isKindOfClass:NSClassFromString(@"AVFullScreenViewController")])
            fullScreenViewController = presentedViewController;
        if ([window isKindOfClass:NSClassFromString(@"TestWKWebViewHostWindow")])
            hostWindow = window;
    }
    EXPECT_TRUE(!fullScreenViewController);
    EXPECT_TRUE(!!hostWindow);

    RetainPtr<UIView> playerLayerView;
    RetainPtr viewStack = adoptNS([[NSMutableArray alloc] initWithObjects:hostWindow, nil]);
    while ([viewStack count]) {
        RetainPtr view = (UIView *)[viewStack lastObject];
        [viewStack removeLastObject];

        if ([view isKindOfClass:NSClassFromString(@"WebAVPlayerLayerView")]) {
            playerLayerView = WTFMove(view);
            break;
        }

        [viewStack addObjectsFromArray:[view subviews]];
    }
    EXPECT_TRUE(!!playerLayerView);
}

TEST(VideoPresentationMode, Standby)
{
    // This test must run in TestWebKitAPI.app
    if (![NSBundle.mainBundle.bundleIdentifier isEqualToString:@"org.webkit.TestWebKitAPI"])
        return;

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setAllowsInlineMediaPlayback:YES];
    [configuration preferences].elementFullscreenEnabled = YES;
    RetainPtr uiDelegate = adoptNS([[VideoPresentationModeUIDelegate alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<video src=video-with-audio.mp4 playsinline loop controls></video>"];
    [webView evaluateJavaScript:@"document.querySelector('video').play()" completionHandler:nil];
    [webView evaluateJavaScript:@"document.querySelector('body').requestFullscreen()" completionHandler:nil];

    [uiDelegate waitForDidEnterStandby];

    UIApplication *application = UIApplication.sharedApplication;
    EXPECT_EQ(application.connectedScenes.count, 1U);

    UIScene *scene = application.connectedScenes.anyObject;
    RELEASE_ASSERT([scene isKindOfClass:UIWindowScene.class]);

    RetainPtr<UIViewController> playerViewController;
    for (UIWindow *window in [(UIWindowScene *)scene windows]) {
        UIViewController *rootViewController = window.rootViewController;
        RetainPtr viewControllerStack = adoptNS([[NSMutableArray alloc] initWithObjects:rootViewController, nil]);
        while ([viewControllerStack count]) {
            RetainPtr viewController = (UIViewController *)[viewControllerStack lastObject];
            [viewControllerStack removeLastObject];

            if ([viewController isKindOfClass:AVPlayerViewController.class]) {
                playerViewController = WTFMove(viewController);
                EXPECT_TRUE(window.isHidden);
                break;
            }

            [viewControllerStack addObjectsFromArray:[viewController childViewControllers]];
        }

        if (!!playerViewController)
            break;
    }
    EXPECT_TRUE(!!playerViewController);

    UIView *playerView = [playerViewController viewIfLoaded];
    EXPECT_TRUE(playerView.isHidden);

    [webView evaluateJavaScript:@"document.exitFullscreen()" completionHandler:nil];

    [uiDelegate waitForDidExitStandby];
}

#if !PLATFORM(IOS_SIMULATOR)

TEST(VideoPresentationMode, PictureInPicture)
{
    // This test must run in TestWebKitAPI.app
    if (![NSBundle.mainBundle.bundleIdentifier isEqualToString:@"org.webkit.TestWebKitAPI"])
        return;

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setAllowsInlineMediaPlayback:YES];
    RetainPtr uiDelegate = adoptNS([[VideoPresentationModeUIDelegate alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<video src=video-with-audio.mp4 playsinline loop controls></video>"];
    [webView evaluateJavaScript:@"document.querySelector('video').play()" completionHandler:nil];
    [webView evaluateJavaScript:@"document.querySelector('video').webkitSetPresentationMode('picture-in-picture')" completionHandler:nil];

    [uiDelegate waitForDidEnterFullscreen];

    UIApplication *application = UIApplication.sharedApplication;
    EXPECT_EQ(application.connectedScenes.count, 1U);

    UIScene *scene = application.connectedScenes.anyObject;
    RELEASE_ASSERT([scene isKindOfClass:UIWindowScene.class]);

    RetainPtr<UIViewController> playerViewController;
    for (UIWindow *window in [(UIWindowScene *)scene windows]) {
        UIViewController *rootViewController = window.rootViewController;
        RetainPtr viewControllerStack = adoptNS([[NSMutableArray alloc] initWithObjects:rootViewController, nil]);
        while ([viewControllerStack count]) {
            RetainPtr viewController = (UIViewController *)[viewControllerStack lastObject];
            [viewControllerStack removeLastObject];

            if ([viewController isKindOfClass:AVPlayerViewController.class]) {
                playerViewController = WTFMove(viewController);
                break;
            }

            [viewControllerStack addObjectsFromArray:[viewController childViewControllers]];
        }

        if (!!playerViewController)
            break;
    }
    EXPECT_TRUE(!!playerViewController);

    UIView *playerView = [playerViewController viewIfLoaded];
    EXPECT_TRUE(!!playerView);

    RetainPtr<UIView> playerLayerView;
    RetainPtr viewStack = adoptNS([[NSMutableArray alloc] initWithObjects:playerView, nil]);
    while ([viewStack count]) {
        RetainPtr view = (UIView *)[viewStack lastObject];
        [viewStack removeLastObject];

        if ([view isKindOfClass:NSClassFromString(@"WebAVPlayerLayerView")]) {
            playerLayerView = WTFMove(view);
            break;
        }

        [viewStack addObjectsFromArray:[view subviews]];
    }
    EXPECT_TRUE(!!playerLayerView);

    [webView evaluateJavaScript:@"document.querySelector('video').webkitSetPresentationMode('inline')" completionHandler:nil];

    [uiDelegate waitForDidExitFullscreen];
}

#if HAVE(AVKIT)
TEST(VideoPresentationMode, CaptionPreview)
{
    if (![NSBundle.mainBundle.bundleIdentifier isEqualToString:@"org.webkit.TestWebKitAPI"])
        return;

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setAllowsInlineMediaPlayback:YES];
    RetainPtr uiDelegate = adoptNS([[VideoPresentationModeUIDelegate alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<video src=video-with-audio.mp4 playsinline loop controls></video>"];
    [webView evaluateJavaScript:@"document.querySelector('video').play()" completionHandler:nil];
    [webView evaluateJavaScript:@"document.querySelector('video').webkitSetPresentationMode('fullscreen')" completionHandler:nil];

    [uiDelegate waitForDidEnterFullscreen];

    RetainPtr<UIView> playerLayerView;
    UIApplication *application = UIApplication.sharedApplication;
    UIScene *scene = application.connectedScenes.anyObject;
    RELEASE_ASSERT([scene isKindOfClass:UIWindowScene.class]);

    for (UIWindow *window in [(UIWindowScene *)scene windows]) {
        RetainPtr viewStack = adoptNS([[NSMutableArray alloc] initWithObjects:window, nil]);
        while ([viewStack count]) {
            RetainPtr view = (UIView *)[viewStack lastObject];
            [viewStack removeLastObject];
            if ([view isKindOfClass:NSClassFromString(@"WebAVPlayerLayerView")]) {
                playerLayerView = WTFMove(view);
                break;
            }
            [viewStack addObjectsFromArray:[view subviews]];
        }
        if (playerLayerView)
            break;
    }

    EXPECT_TRUE(!!playerLayerView);

    WebAVPlayerLayer *webAVPlayerLayer = (WebAVPlayerLayer *)[playerLayerView layer];
    EXPECT_TRUE([webAVPlayerLayer isKindOfClass:[WebAVPlayerLayer class]]);

    EXPECT_FALSE([webAVPlayerLayer showingCaptionPreview]);

    [webAVPlayerLayer setCaptionPreviewProfileID:@"test-profile" position:CGPointMake(10, 20) text:@"Test caption text"];
    EXPECT_TRUE([webAVPlayerLayer showingCaptionPreview]);

    [webAVPlayerLayer stopShowingCaptionPreview];
    EXPECT_FALSE([webAVPlayerLayer showingCaptionPreview]);

    [webAVPlayerLayer setCaptionPreviewProfileID:@"another-profile" position:CGPointMake(50, 100) text:@"Another test"];
    EXPECT_TRUE([webAVPlayerLayer showingCaptionPreview]);

    [webAVPlayerLayer setCaptionPreviewProfileID:@"third-profile" position:CGPointMake(0, 0) text:nil];
    EXPECT_TRUE([webAVPlayerLayer showingCaptionPreview]);

    [webAVPlayerLayer stopShowingCaptionPreview];
    EXPECT_FALSE([webAVPlayerLayer showingCaptionPreview]);

    [webAVPlayerLayer stopShowingCaptionPreview];
    EXPECT_FALSE([webAVPlayerLayer showingCaptionPreview]);

    [webView evaluateJavaScript:@"document.querySelector('video').webkitSetPresentationMode('inline')" completionHandler:nil];
    [uiDelegate waitForDidExitFullscreen];
}
#endif // HAVE(AVKIT)

#endif // !PLATFORM(IOS_SIMULATOR)

#endif // PLATFORM(IOS)
