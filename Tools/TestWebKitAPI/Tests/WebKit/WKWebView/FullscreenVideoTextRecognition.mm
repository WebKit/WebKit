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

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

#import "Helpers/cocoa/ImageAnalysisTestingUtilities.h"
#import "InstanceMethodSwizzler.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <pal/spi/cocoa/VisionKitCoreSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/darwin/DispatchExtras.h>

#import <pal/cocoa/VisionKitCoreSoftLink.h>

#if PLATFORM(IOS_FAMILY)

static void swizzledPresentViewController(UIViewController *, SEL, UIViewController *, BOOL, dispatch_block_t completion)
{
    RunLoop::mainSingleton().dispatch([completion = makeBlockPtr(completion)] {
        if (completion)
            completion();
    });
}

#endif // PLATFORM(IOS_FAMILY)

static int32_t swizzledProcessRequest(VKCImageAnalyzer *, SEL, id request, void (^)(double progress), void (^completion)(VKImageAnalysis *, NSError *))
{
    dispatch_async(mainDispatchQueueSingleton(), [completion = makeBlockPtr(completion)] {
        completion(TestWebKitAPI::createImageAnalysisWithSimpleFixedResults().get(), nil);
    });
    return 100;
}

static void swizzledSetAnalysis(VKCImageAnalysisInteraction *, SEL, VKCImageAnalysis *)
{
}

@interface FullscreenVideoTextRecognitionWebView : TestWKWebView
@end

@implementation FullscreenVideoTextRecognitionWebView {
    std::unique_ptr<InstanceMethodSwizzler> _imageAnalysisRequestSwizzler;
#if PLATFORM(IOS_FAMILY)
    std::unique_ptr<InstanceMethodSwizzler> _viewControllerPresentationSwizzler;
    std::unique_ptr<InstanceMethodSwizzler> _imageAnalysisInteractionSwizzler;
#else
    std::unique_ptr<InstanceMethodSwizzler> _imageAnalysisOverlaySwizzler;
#endif
    bool _doneEnteringFullscreen;
    bool _doneExitingFullscreen;
}

+ (RetainPtr<FullscreenVideoTextRecognitionWebView>)create
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    configuration.preferences.elementFullscreenEnabled = YES;
#if PLATFORM(IOS_FAMILY)
    configuration.allowsInlineMediaPlayback = YES;
#endif
    RetainPtr webView = adoptNS([[FullscreenVideoTextRecognitionWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"element-fullscreen"];
    return webView;
}

+ (RetainPtr<FullscreenVideoTextRecognitionWebView>)createForVideoFullscreen
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    configuration.preferences.elementFullscreenEnabled = YES;
    [configuration.preferences _setVideoFullscreenRequiresElementFullscreen:YES];
    RetainPtr webView = adoptNS([[FullscreenVideoTextRecognitionWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"element-fullscreen"];
    return webView;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if (!(self = [super initWithFrame:frame configuration:configuration]))
        return nil;

    _imageAnalysisRequestSwizzler = WTF::makeUnique<InstanceMethodSwizzler>(
        PAL::getVKImageAnalyzerClassSingleton(),
        @selector(processRequest:progressHandler:completionHandler:),
        reinterpret_cast<IMP>(swizzledProcessRequest)
    );

#if PLATFORM(IOS_FAMILY)
    _imageAnalysisInteractionSwizzler = WTF::makeUnique<InstanceMethodSwizzler>(
        PAL::getVKCImageAnalysisInteractionClassSingleton(),
        @selector(setAnalysis:),
        reinterpret_cast<IMP>(swizzledSetAnalysis)
    );
    // Work around lack of a real UIApplication in TestWebKitAPIApp on iOS. Without this,
    // -presentViewController:animated:completion: never calls the completion handler,
    // which means we never transition into WKFullscreenStateInFullscreen.
    _viewControllerPresentationSwizzler = WTF::makeUnique<InstanceMethodSwizzler>(
        UIViewController.class,
        @selector(presentViewController:animated:completion:),
        reinterpret_cast<IMP>(swizzledPresentViewController)
    );
#else
    _imageAnalysisOverlaySwizzler = WTF::makeUnique<InstanceMethodSwizzler>(
        PAL::getVKCImageAnalysisOverlayViewClassSingleton(),
        @selector(setAnalysis:),
        reinterpret_cast<IMP>(swizzledSetAnalysis)
    );
#endif

    return self;
}

- (void)loadVideoSource:(NSString *)source
{
    __block bool done = false;
    [self callAsyncJavaScript:@"return loadSource(source)" arguments:@{ @"source" : source } inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    [self waitForNextPresentationUpdate];
}

- (void)enterFullscreen
{
    _doneEnteringFullscreen = false;
    [self evaluateJavaScript:@"enterFullscreen()" completionHandler:nil];
    TestWebKitAPI::Util::run(&_doneEnteringFullscreen);
    [self waitForNextPresentationUpdate];
}

- (void)enterVideoFullscreen
{
    _doneEnteringFullscreen = false;
    [self evaluateJavaScript:@"enterVideoFullscreen()" completionHandler:nil];
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

- (void)dismissFullscreen
{
#if PLATFORM(IOS_FAMILY)
    [self exitFullscreen];
#else
    _doneExitingFullscreen = false;
    [self sendKey:@"\x1B" code:0x35 isDown:YES modifiers:0];
    [self sendKey:@"\x1B" code:0x35 isDown:NO modifiers:0];
    TestWebKitAPI::Util::run(&_doneExitingFullscreen);
    [self waitForNextPresentationUpdate];
#endif
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

- (void)pause
{
    [self objectByEvaluatingJavaScript:@"video.pause()"];
    [self waitForNextPresentationUpdate];
}

- (void)play
{
    __block bool done = false;
    [self callAsyncJavaScript:@"video.play()" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    [self waitForNextPresentationUpdate];
}

- (double)waitForVideoFrame
{
    __block double result = 0;
    __block bool done = false;
    [self callAsyncJavaScript:@"waitForVideoFrame()" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(NSNumber *timestamp, NSError *error) {
        EXPECT_NULL(error);
        result = timestamp.doubleValue;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result;
}

- (BOOL)hasActiveImageAnalysis
{
#if PLATFORM(IOS_FAMILY)
    for (id<UIInteraction> interaction in self.textInputContentView.interactions) {
        if ([interaction isKindOfClass:PAL::getVKCImageAnalysisInteractionClassSingleton()])
            return YES;
    }
#else
    for (NSView *subview in self.subviews) {
        if ([subview isKindOfClass:PAL::getVKCImageAnalysisOverlayViewClassSingleton()])
            return YES;
    }
#endif
    return NO;
}

- (void)beginSeek:(double)time
{
    [self objectByEvaluatingJavaScript:[NSString stringWithFormat:@"video.currentTime = %f", time]];
}

- (void)waitForImageAnalysisToBegin
{
    TestWebKitAPI::Util::waitForConditionWithLogging([&] {
        return self.hasActiveImageAnalysis;
    }, 3, @"Expected image analysis to begin.");
}

- (void)waitForImageAnalysisToEnd
{
    TestWebKitAPI::Util::waitForConditionWithLogging([&] {
        return !self.hasActiveImageAnalysis;
    }, 3, @"Expected image analysis to end.");
}

@end

namespace TestWebKitAPI {

TEST(FullscreenVideoTextRecognition, TogglePlaybackInElementFullscreen)
{
    auto webView = [FullscreenVideoTextRecognitionWebView create];
    [webView loadVideoSource:@"test.mp4"];

    [webView enterFullscreen];
    [webView pause];
    [webView waitForImageAnalysisToBegin];

    [webView play];
    [webView waitForImageAnalysisToEnd];
}

TEST(FullscreenVideoTextRecognition, AddVideoAfterEnteringFullscreen)
{
    auto webView = [FullscreenVideoTextRecognitionWebView create];
    [webView loadVideoSource:@"test.mp4"];
    [webView objectByEvaluatingJavaScript:@"video.remove()"];

    [webView enterFullscreen];
    [webView objectByEvaluatingJavaScript:@"container.appendChild(video); 0;"];
    [webView play];
    [webView waitForVideoFrame];
    [webView pause];
    [webView waitForImageAnalysisToBegin];
}

TEST(FullscreenVideoTextRecognition, DoNotAnalyzeVideoAfterExitingFullscreen)
{
    auto webView = [FullscreenVideoTextRecognitionWebView create];
    [webView loadVideoSource:@"test.mp4"];

    [webView enterFullscreen];
    [webView pause];
    [webView waitForImageAnalysisToBegin];

    [webView exitFullscreen];
    [webView waitForImageAnalysisToEnd];

    [webView play];
    [webView pause];

    bool doneWaiting = false;
    RunLoop::mainSingleton().dispatchAfter(300_ms, [&] {
        EXPECT_FALSE([webView hasActiveImageAnalysis]);
        doneWaiting = true;
    });
    Util::run(&doneWaiting);
}

TEST(FullscreenVideoTextRecognition, NoOverlayInstalledAfterExitingFullscreenViaEscape)
{
    auto webView = [FullscreenVideoTextRecognitionWebView create];
    [webView loadVideoSource:@"test.mp4"];

    [webView enterFullscreen];
    [webView waitForVideoFrame];

    // Exit fullscreen. Verify that no image analysis overlay is
    // installed after fullscreen exit, even if playback state changes during
    // the exit trigger TextRecognitionRequest::requestTextRecognitionFor.
    [webView dismissFullscreen];

    // Wait longer than the 250ms text recognition timer to ensure no overlay
    // is installed after fullscreen exit.
    bool doneWaiting = false;
    RunLoop::mainSingleton().dispatchAfter(400_ms, [&] {
        EXPECT_FALSE([webView hasActiveImageAnalysis]);
        doneWaiting = true;
    });
    Util::run(&doneWaiting);
}

TEST(FullscreenVideoTextRecognition, NoOverlayInstalledAfterSeekAndExitingFullscreenViaEscape)
{
    auto webView = [FullscreenVideoTextRecognitionWebView create];
    [webView loadVideoSource:@"test.mp4"];

    [webView enterFullscreen];
    [webView waitForVideoFrame];
    [webView pause];
    [webView waitForImageAnalysisToBegin];
    [webView beginSeek:0.5];

    // Allow the seek to start before exiting fullscreen.
    bool seekStarted = false;
    RunLoop::mainSingleton().dispatchAfter(100_ms, [&] {
        seekStarted = true;
    });
    Util::run(&seekStarted);

    // Exit fullscreen while a seek is in progress. Verify that no
    // image analysis overlay is installed after fullscreen exit.
    [webView dismissFullscreen];

    // Wait longer than the 250ms text recognition timer to ensure no overlay
    // is installed after fullscreen exit.
    bool doneWaiting = false;
    RunLoop::mainSingleton().dispatchAfter(300_ms, [&] {
        EXPECT_FALSE([webView hasActiveImageAnalysis]);
        doneWaiting = true;
    });
    Util::run(&doneWaiting);
}

#if PLATFORM(MAC)
TEST(FullscreenVideoTextRecognition, NoOverlayInstalledAfterExitingVideoFullscreenViaEscape)
{
    auto webView = [FullscreenVideoTextRecognitionWebView createForVideoFullscreen];
    [webView loadVideoSource:@"test.mp4"];

    [webView enterVideoFullscreen];
    [webView waitForVideoFrame];

    // Exit fullscreen. Verify that no image analysis overlay is
    // installed after fullscreen exit, even if playback state changes during
    // the exit trigger TextRecognitionRequest::requestTextRecognitionFor.
    [webView dismissFullscreen];

    // Wait longer than the 250ms text recognition timer to ensure no overlay
    // is installed after fullscreen exit.
    bool doneWaiting = false;
    RunLoop::mainSingleton().dispatchAfter(400_ms, [&] {
        EXPECT_FALSE([webView hasActiveImageAnalysis]);
        doneWaiting = true;
    });
    Util::run(&doneWaiting);
}

TEST(FullscreenVideoTextRecognition, NoOverlayInstalledAfterSeekAndExitingVideoFullscreenViaEscape)
{
    auto webView = [FullscreenVideoTextRecognitionWebView createForVideoFullscreen];
    [webView loadVideoSource:@"test.mp4"];

    [webView enterVideoFullscreen];
    [webView waitForVideoFrame];
    [webView pause];
    [webView waitForImageAnalysisToBegin];
    [webView beginSeek:0.5];

    // Allow the seek to start before exiting fullscreen.
    bool seekStarted = false;
    RunLoop::mainSingleton().dispatchAfter(100_ms, [&] {
        seekStarted = true;
    });
    Util::run(&seekStarted);

    // Exit fullscreen while a seek is in progress. Verify that no
    // image analysis overlay is installed after fullscreen exit.
    [webView dismissFullscreen];

    // Wait longer than the 250ms text recognition timer to ensure no overlay
    // is installed after fullscreen exit.
    bool doneWaiting = false;
    RunLoop::mainSingleton().dispatchAfter(300_ms, [&] {
        EXPECT_FALSE([webView hasActiveImageAnalysis]);
        doneWaiting = true;
    });
    Util::run(&doneWaiting);
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)
TEST(FullscreenVideoTextRecognition, NoOverlayInstalledAfterExitingNativeVideoFullscreen)
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[FullscreenVideoTextRecognitionWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"element-fullscreen"];
    [webView objectByEvaluatingJavaScript:@"window.internals.setMockVideoPresentationModeEnabled(true)"];
    [webView loadVideoSource:@"test.mp4"];

    // Enter native video fullscreen via mock presentation mode.
    __block bool enteredFullscreen = false;
    [webView performAfterReceivingMessage:@"enteredFullscreen" action:^{ enteredFullscreen = true; }];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"enterNativeVideoFullscreen()"];
    Util::run(&enteredFullscreen);

    // Wait for the fullscreen mode change to complete (DidEnterFullscreen IPC round-trip).
    do {
        if (![webView stringByEvaluatingJavaScript:@"window.internals.isChangingPresentationMode(document.querySelector('video'))"].boolValue)
            break;
        Util::runFor(100_ms);
    } while (true);

    [webView waitForVideoFrame];

    // Exit native video fullscreen.
    __block bool exitedFullscreen = false;
    [webView performAfterReceivingMessage:@"exitedFullscreen" action:^{ exitedFullscreen = true; }];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"exitNativeVideoFullscreen()"];
    Util::run(&exitedFullscreen);
    [webView waitForNextPresentationUpdate];

    // Wait longer than the 250ms text recognition timer to ensure no overlay
    // is installed after fullscreen exit.
    bool doneWaiting = false;
    RunLoop::mainSingleton().dispatchAfter(400_ms, [&] {
        EXPECT_FALSE([webView hasActiveImageAnalysis]);
        doneWaiting = true;
    });
    Util::run(&doneWaiting);
}

TEST(FullscreenVideoTextRecognition, NoOverlayInstalledAfterSeekAndExitingNativeVideoFullscreen)
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[FullscreenVideoTextRecognitionWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"element-fullscreen"];
    [webView objectByEvaluatingJavaScript:@"window.internals.setMockVideoPresentationModeEnabled(true)"];
    [webView loadVideoSource:@"test.mp4"];

    // Enter native video fullscreen via mock presentation mode.
    __block bool enteredFullscreen = false;
    [webView performAfterReceivingMessage:@"enteredFullscreen" action:^{ enteredFullscreen = true; }];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"enterNativeVideoFullscreen()"];
    Util::run(&enteredFullscreen);

    // Wait for the fullscreen mode change to complete (DidEnterFullscreen IPC round-trip).
    do {
        if (![webView stringByEvaluatingJavaScript:@"window.internals.isChangingPresentationMode(document.querySelector('video'))"].boolValue)
            break;
        Util::runFor(100_ms);
    } while (true);

    [webView waitForVideoFrame];
    [webView pause];
    [webView waitForImageAnalysisToBegin];
    [webView beginSeek:0.5];

    // Allow the seek to start before exiting fullscreen.
    bool seekStarted = false;
    RunLoop::mainSingleton().dispatchAfter(100_ms, [&] {
        seekStarted = true;
    });
    Util::run(&seekStarted);

    // Exit native video fullscreen while a seek is in progress.
    __block bool exitedFullscreen = false;
    [webView performAfterReceivingMessage:@"exitedFullscreen" action:^{ exitedFullscreen = true; }];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"exitNativeVideoFullscreen()"];
    Util::run(&exitedFullscreen);
    [webView waitForNextPresentationUpdate];

    // Wait longer than the 250ms text recognition timer to ensure no overlay
    // is installed after fullscreen exit.
    bool doneWaiting = false;
    RunLoop::mainSingleton().dispatchAfter(300_ms, [&] {
        EXPECT_FALSE([webView hasActiveImageAnalysis]);
        doneWaiting = true;
    });
    Util::run(&doneWaiting);
}

#endif // PLATFORM(IOS_FAMILY)

} // namespace TestWebKitAPI

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
