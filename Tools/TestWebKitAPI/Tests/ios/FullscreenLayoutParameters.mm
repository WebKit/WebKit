/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>

static void swizzledPresentViewController(UIViewController *, SEL, UIViewController *, BOOL, dispatch_block_t completion)
{
    RunLoop::mainSingleton().dispatch([completion = makeBlockPtr(completion)] {
        if (completion)
            completion();
    });
}

@interface FullscreenLayoutParametersWebView : TestWKWebView
@end

@implementation FullscreenLayoutParametersWebView {
    std::unique_ptr<InstanceMethodSwizzler> _viewControllerPresentationSwizzler;
    bool _doneEnteringFullscreen;
    bool _doneExitingFullscreen;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if (!(self = [super initWithFrame:frame configuration:configuration]))
        return nil;

    // Since TestWebKitAPIApp is not a real UIApplication, -presentViewController:animated:completion:
    // never calls the completion handler and we never transition into WKFullscreenStateInFullscreen.
    // Work around that fact by swizzling the method.
    _viewControllerPresentationSwizzler = WTF::makeUnique<InstanceMethodSwizzler>(
        UIViewController.class,
        @selector(presentViewController:animated:completion:),
        reinterpret_cast<IMP>(swizzledPresentViewController)
    );

    return self;
}

- (void)enterFullscreen
{
    _doneEnteringFullscreen = false;
    [self evaluateJavaScript:@"enterFullscreen()" completionHandler:nil];
    TestWebKitAPI::Util::run(&_doneEnteringFullscreen);
    [self waitForNextPresentationUpdate];
}

- (void)exitFullscreen
{
    _doneExitingFullscreen = false;
    [self evaluateJavaScript:@"exitFullscreen()" completionHandler:nil];
    TestWebKitAPI::Util::run(&_doneExitingFullscreen);
    [self waitForNextPresentationUpdate];
}

- (void)didChangeValueForKey:(NSString *)key
{
    [super didChangeValueForKey:key];

    if (![key isEqualToString:@"fullscreenState"])
        return;

    auto state = self.fullscreenState;
    switch (state) {
    case WKFullscreenStateNotInFullscreen:
        _doneExitingFullscreen = true;
        break;
    case WKFullscreenStateInFullscreen:
        _doneEnteringFullscreen = true;
        break;
    default:
        break;
    }
}

@end

namespace TestWebKitAPI {

TEST(Fullscreen, OverriddenLayoutParameters)
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    configuration.preferences.elementFullscreenEnabled = YES;
    auto webView = adoptNS([[FullscreenLayoutParametersWebView alloc] initWithFrame:CGRectMake(0, 0, 390, 800) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"element-fullscreen"];

    auto layoutSize = CGSizeMake(390, 745);
    [webView _overrideLayoutParametersWithMinimumLayoutSize:layoutSize minimumUnobscuredSizeOverride:layoutSize maximumUnobscuredSizeOverride:layoutSize];

    [webView enterFullscreen];
    [webView exitFullscreen];

    EXPECT_TRUE(CGSizeEqualToSize([webView _minimumUnobscuredSizeOverride], layoutSize));
    EXPECT_TRUE(CGSizeEqualToSize([webView _maximumUnobscuredSizeOverride], layoutSize));
}

TEST(Fullscreen, ResizeEventOrder)
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    configuration.preferences.elementFullscreenEnabled = YES;
    auto webView = adoptNS([[FullscreenLayoutParametersWebView alloc] initWithFrame:CGRectMake(0, 0, 390, 800) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"element-fullscreen"];

    [webView waitForNextPresentationUpdate];

    bool hasFullscreenChanged = false;
    bool hasResized = false;

    [webView objectByEvaluatingJavaScript:@(R"script(
        hasFullscreenChanged = false;

        document.addEventListener('fullscreenchange', () => {
            hasFullscreenChanged = true;
            webkit.messageHandlers.testHandler.postMessage('fullscreen-changed');
        });

        window.addEventListener('resize', () => {
            if (!hasFullscreenChanged)
                webkit.messageHandlers.testHandler.postMessage('fail');
            webkit.messageHandlers.testHandler.postMessage('resize');
        });)script")];

    [webView performAfterReceivingMessage:@"fullscreen-changed" action:[&] { hasFullscreenChanged = true; }];
    [webView performAfterReceivingMessage:@"resize" action:[&] { hasResized = true; }];
    [webView performAfterReceivingMessage:@"fail" action:[] { FAIL(); }];

    [webView enterFullscreen];

    Util::run(&hasFullscreenChanged);
    Util::run(&hasResized);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
