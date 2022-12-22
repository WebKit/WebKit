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

#import "ImageAnalysisTestingUtilities.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <pal/spi/cocoa/VisionKitCoreSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>

#import <pal/cocoa/VisionKitCoreSoftLink.h>

#if PLATFORM(IOS_FAMILY)

static void swizzledPresentViewController(UIViewController *, SEL, UIViewController *, BOOL, dispatch_block_t completion)
{
    RunLoop::main().dispatch([completion = makeBlockPtr(completion)] {
        if (completion)
            completion();
    });
}

#endif // PLATFORM(IOS_FAMILY)

static int32_t swizzledProcessRequest(VKCImageAnalyzer *, SEL, id request, void (^)(double progress), void (^completion)(VKImageAnalysis *, NSError *))
{
    dispatch_async(dispatch_get_main_queue(), [completion = makeBlockPtr(completion)] {
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
    configuration.preferences._fullScreenEnabled = YES;
    auto webView = adoptNS([[FullscreenVideoTextRecognitionWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"element-fullscreen"];
    return webView;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if (!(self = [super initWithFrame:frame configuration:configuration]))
        return nil;

    _imageAnalysisRequestSwizzler = WTF::makeUnique<InstanceMethodSwizzler>(
        PAL::getVKImageAnalyzerClass(),
        @selector(processRequest:progressHandler:completionHandler:),
        reinterpret_cast<IMP>(swizzledProcessRequest)
    );

#if PLATFORM(IOS_FAMILY)
    _imageAnalysisInteractionSwizzler = WTF::makeUnique<InstanceMethodSwizzler>(
        PAL::getVKCImageAnalysisInteractionClass(),
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
        PAL::getVKCImageAnalysisOverlayViewClass(),
        @selector(setAnalysis:),
        reinterpret_cast<IMP>(swizzledSetAnalysis)
    );
#endif

    return self;
}

- (void)loadVideoSource:(NSString *)source
{
    __block bool done = false;
    [self callAsyncJavaScript:@"loadSource(source)" arguments:@{ @"source" : source } inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id, NSError *error) {
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
        if ([interaction isKindOfClass:PAL::getVKCImageAnalysisInteractionClass()])
            return YES;
    }
#else
    for (NSView *subview in self.subviews) {
        if ([subview isKindOfClass:PAL::getVKCImageAnalysisOverlayViewClass()])
            return YES;
    }
#endif
    return NO;
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

// FIXME: Re-enable this test for iOS once webkit.org/b/248094 is resolved
#if PLATFORM(IOS)
TEST(FullscreenVideoTextRecognition, DISABLED_TogglePlaybackInElementFullscreen)
#else
TEST(FullscreenVideoTextRecognition, TogglePlaybackInElementFullscreen)
#endif
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

// FIXME: Re-enable this test for iOS once webkit.org/b/248094 is resolved
// FIXME: Re-enable this test once webkit.org/b/248093 is resolved.
#if PLATFORM(IOS) || !defined(NDEBUG)
TEST(FullscreenVideoTextRecognition, DISABLED_DoNotAnalyzeVideoAfterExitingFullscreen)
#else
TEST(FullscreenVideoTextRecognition, DoNotAnalyzeVideoAfterExitingFullscreen)
#endif
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
    RunLoop::main().dispatchAfter(300_ms, [&] {
        EXPECT_FALSE([webView hasActiveImageAnalysis]);
        doneWaiting = true;
    });
    Util::run(&doneWaiting);
}

} // namespace TestWebKitAPI

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
