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
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/PictureInPictureSupport.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

// We can enable the test for old iOS versions after <rdar://problem/63572534> is fixed.
#if ENABLE(VIDEO_PRESENTATION_MODE) && (PLATFORM(MAC) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 140000))

static void loadPictureInPicture(RetainPtr<TestWKWebView> webView)
{
    [webView synchronouslyLoadHTMLString:@"<video src=video-with-audio.mp4 webkit-playsinline playsinline loop></video>"];

    [[NSUserDefaults standardUserDefaults] registerDefaults:@{
        @"WebCoreLogging": @"",
        @"WebKit2Logging": @"",
    }];

    [webView objectByEvaluatingJavaScript:@"document.querySelector('video').addEventListener('webkitpresentationmodechanged', event => { window.webkit.messageHandlers.testHandler.postMessage('presentationmodechanged'); });"];

    __block bool presentationModeChanged = false;
    [webView performAfterReceivingMessage:@"presentationmodechanged" action:^{ presentationModeChanged = true; }];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.querySelector('video').webkitSetPresentationMode('picture-in-picture')"];
    ASSERT_UNUSED(presentationModeChanged, TestWebKitAPI::Util::runFor(&presentationModeChanged, 10_s));
    do {
        if (![webView stringByEvaluatingJavaScript:@"window.internals.isChangingPresentationMode(document.querySelector('video'))"].boolValue)
            break;

        TestWebKitAPI::Util::runFor(0.5_s);
    } while (true);
}

// FIXME: Re-enable this test for Big Sur once webkit.org/b/245241 is resolved
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 131000)
TEST(WKWebViewCloseAllMediaPresentations, DISABLED_PictureInPicture)
#else
TEST(WKWebViewCloseAllMediaPresentations, PictureInPicture)
#endif
{
    if (!WebCore::supportsPictureInPicture())
        return;

    [[NSUserDefaults standardUserDefaults] registerDefaults:@{
        @"WebCoreLogging": @"Fullscreen=debug",
        @"WebKit2Logging": @"Fullscreen=debug",
    }];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setAllowsPictureInPictureMediaPlayback:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration addToWindow:YES]);

    loadPictureInPicture(webView);

    static bool isDone = false;
    [webView closeAllMediaPresentationsWithCompletionHandler:^{
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_TRUE([webView _allMediaPresentationsClosed]);
}

// FIXME: Re-enable this test for Big Sur once webkit.org/b/245241 is resolved
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 131000)
TEST(WKWebViewCloseAllMediaPresentationsInternal, DISABLED_PictureInPicture)
#else
TEST(WKWebViewCloseAllMediaPresentationsInternal, PictureInPicture)
#endif
{
    if (!WebCore::supportsPictureInPicture())
        return;

    [[NSUserDefaults standardUserDefaults] registerDefaults:@{
        @"WebCoreLogging": @"Fullscreen=debug",
        @"WebKit2Logging": @"Fullscreen=debug",
    }];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setAllowsPictureInPictureMediaPlayback:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration addToWindow:YES]);

    loadPictureInPicture(webView);

    [webView _closeAllMediaPresentations];

    do {
        if (![webView stringByEvaluatingJavaScript:@"window.internals.isChangingPresentationMode(document.querySelector('video'))"].boolValue)
            break;

        TestWebKitAPI::Util::runFor(0.5_s);
    } while (true);

    EXPECT_TRUE([webView _allMediaPresentationsClosed]);
}

#endif

#if ENABLE(FULLSCREEN_API)

TEST(WKWebViewCloseAllMediaPresentations, VideoFullscreen)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setFullScreenEnabled:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:@"<video src=video-with-audio.mp4 webkit-playsinline playsinline loop></video>"];
    [webView objectByEvaluatingJavaScript:@"document.querySelector('video').addEventListener('webkitpresentationmodechanged', event => { window.webkit.messageHandlers.testHandler.postMessage('presentationmodechanged'); });"];

    __block bool presentationModeChanged = false;
    [webView performAfterReceivingMessage:@"presentationmodechanged" action:^{ presentationModeChanged = true; }];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.querySelector('video').webkitEnterFullscreen()"];
    [webView objectByEvaluatingJavaScript:@"window.internals.setMockVideoPresentationModeEnabled(true)"];

    TestWebKitAPI::Util::run(&presentationModeChanged);
    do {
        if (![webView stringByEvaluatingJavaScript:@"window.internals.isChangingPresentationMode(document.querySelector('video'))"].boolValue)
            break;

        TestWebKitAPI::Util::runFor(0.5_s);
    } while (true);

    static bool isDone = false;
    [webView closeAllMediaPresentationsWithCompletionHandler:^{
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
    
    EXPECT_TRUE([webView _allMediaPresentationsClosed]);
}

TEST(WKWebViewCloseAllMediaPresentations, ElementFullscreen)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setFullScreenEnabled:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:@"<div id=\"target\" style=\"width:100px;height:100px;background-color:red\"></div>"];
    [webView objectByEvaluatingJavaScript:@"document.querySelector('#target').addEventListener('webkitfullscreenchange', event => { window.webkit.messageHandlers.testHandler.postMessage('fullscreenchange'); });"];

    __block bool fullscreenModeChanged = false;
    [webView performAfterReceivingMessage:@"fullscreenchange" action:^{ fullscreenModeChanged = true; }];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.querySelector('#target').webkitRequestFullscreen()"];

    TestWebKitAPI::Util::run(&fullscreenModeChanged);

    fullscreenModeChanged = false;
    [webView performAfterReceivingMessage:@"fullscreenchange" action:^{ fullscreenModeChanged = true; }];

    static bool isDone = false;
    [webView closeAllMediaPresentationsWithCompletionHandler:^{
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
    
    EXPECT_TRUE([webView _allMediaPresentationsClosed]);
}

// FIXME: Re-enable this test for Big Sur once webkit.org/b/245241 is resolved
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 131000)
TEST(WKWebViewCloseAllMediaPresentations, DISABLED_MultipleSequentialCloseAllMediaPresentations)
#else
TEST(WKWebViewCloseAllMediaPresentations, MultipleSequentialCloseAllMediaPresentations)
#endif
{
    if (!WebCore::supportsPictureInPicture())
        return;

    [[NSUserDefaults standardUserDefaults] registerDefaults:@{
        @"WebCoreLogging": @"Fullscreen=debug",
        @"WebKit2Logging": @"Fullscreen=debug",
    }];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setAllowsPictureInPictureMediaPlayback:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration addToWindow:YES]);

    loadPictureInPicture(webView);

    static bool firstIsDone = false;
    [webView closeAllMediaPresentationsWithCompletionHandler:^{
        firstIsDone = true;
    }];

    static bool secondIsDone = false;
    [webView closeAllMediaPresentationsWithCompletionHandler:^{
        secondIsDone = true;
    }];

    TestWebKitAPI::Util::run(&firstIsDone);
    TestWebKitAPI::Util::run(&secondIsDone);

    EXPECT_TRUE([webView _allMediaPresentationsClosed]);
}

TEST(WKWebViewCloseAllMediaPresentations, RemovedCloseAllMediaPresentationAPIs)
{
    // In r271970, we renamed -closeAllMediaPresentations to -closeAllMediaPresentations:completionHandler, which broke
    // binary compatability of apps linked against older SDKs. Ensure calling the removed API does not crash.

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration addToWindow:YES]);

    EXPECT_TRUE([webView respondsToSelector:@selector(closeAllMediaPresentations)]);

    RetainPtr<NSException> exception;
    @try {
        [webView closeAllMediaPresentations];
    } @catch(NSException *caught) {
        exception = caught;
    }
    EXPECT_FALSE(exception);
}

#endif
