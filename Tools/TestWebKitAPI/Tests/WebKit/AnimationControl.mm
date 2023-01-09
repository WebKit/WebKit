/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKProcessPoolPrivate.h>

@interface WKWebView (AnimationTesting)
- (void)_pauseAllAnimationsWithCompletionHandler:(void(^_Nullable)(void))completionHandler;
- (void)_playAllAnimationsWithCompletionHandler:(void(^_Nullable)(void))completionHandler;
@property (nonatomic, readonly) BOOL _allowsAnyAnimationToPlay;
@end

static bool isAnimating(NSString *domID, RetainPtr<TestWKWebView> webView)
{
    NSString *js = [NSString stringWithFormat:@"window.internals.isImageAnimating(document.getElementById('%@'))", domID];
    return [webView stringByEvaluatingJavaScript:js].boolValue;
}

TEST(WebKit, PlayAllPauseAllAnimationSupport)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:@"<img id='imgOne' src='test-mse.mp4'><img id='imgTwo' src='test-without-audio-track.mp4'>"];
    [webView stringByEvaluatingJavaScript:@"window.internals.settings.setImageAnimationControlEnabled(true)"];

    // Wait for the animations to become paused (explicitly exercising the completionHandler).
    static bool isDone = false;
    [webView _pauseAllAnimationsWithCompletionHandler:^{
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    ASSERT_FALSE([webView _allowsAnyAnimationToPlay]);

    // Wait for the animations to start playing (explicitly exercising the completionHandler).
    isDone = false;
    [webView _playAllAnimationsWithCompletionHandler:^{
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
    ASSERT_TRUE([webView _allowsAnyAnimationToPlay]);
}

TEST(WebKit, IsAnyAnimationAllowedToPlayBehaviorWithIndividualAnimationControl)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:@"<img id='imgOne' src='test-mse.mp4'><img id='imgTwo' src='test-without-audio-track.mp4'>"];
    [webView stringByEvaluatingJavaScript:@"window.internals.settings.setImageAnimationControlEnabled(true)"];

    [webView _pauseAllAnimationsWithCompletionHandler:nil];
    while (isAnimating(@"imgOne", webView) || isAnimating(@"imgTwo", webView))
        TestWebKitAPI::Util::runFor(0.05_s);
    ASSERT_FALSE([webView _allowsAnyAnimationToPlay]);

    [webView stringByEvaluatingJavaScript:@"internals.resumeImageAnimation(document.getElementById('imgOne'))"];
    while (!isAnimating(@"imgOne", webView))
        TestWebKitAPI::Util::runFor(0.05_s);
    // imgOne is now playing despite the rest of the animations on the page being paused, so _allowsAnyAnimationToPlay should be true.
    ASSERT_TRUE([webView _allowsAnyAnimationToPlay]);

    // Deleting the individually animating image should result in _allowsAnyAnimationToPlay being false.
    [webView stringByEvaluatingJavaScript:@"document.getElementById('imgOne').remove()"];
    [configuration.processPool _garbageCollectJavaScriptObjectsForTesting];
    while ([webView _allowsAnyAnimationToPlay])
        TestWebKitAPI::Util::runFor(0.05_s);
    ASSERT_FALSE([webView _allowsAnyAnimationToPlay]);

    [webView _playAllAnimationsWithCompletionHandler:nil];
    while (![webView _allowsAnyAnimationToPlay])
        TestWebKitAPI::Util::runFor(0.05_s);
    ASSERT_TRUE([webView _allowsAnyAnimationToPlay]);
}
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
