/*
* Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(IMAGE_ANALYSIS)

#import "ImageAnalysisTestingUtilities.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <pal/cocoa/VisionKitCoreSoftLink.h>
#import <pal/spi/cocoa/VisionKitCoreSPI.h>

static unsigned gDidProcessRequestCount = 0;

@interface TestWKWebView (ImageAnalysisTests)
- (void)waitForImageAnalysisRequests:(unsigned)numberOfRequests;
@end

@implementation TestWKWebView (ImageAnalysisTests)

- (void)waitForImageAnalysisRequests:(unsigned)numberOfRequests
{
    TestWebKitAPI::Util::waitForConditionWithLogging([&] {
        return gDidProcessRequestCount == numberOfRequests;
    }, 3, @"Timed out waiting for %u image analysis to complete.", numberOfRequests);

    [self waitForNextPresentationUpdate];
    EXPECT_EQ(gDidProcessRequestCount, numberOfRequests);
}

@end

namespace TestWebKitAPI {

static RetainPtr<TestWKWebView> createWebViewWithTextRecognitionEnhancements()
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    for (_WKInternalDebugFeature *feature in WKPreferences._internalDebugFeatures) {
        if ([feature.key isEqualToString:@"TextRecognitionEnhancementsEnabled"]) {
            [[configuration preferences] _setEnabled:YES forInternalDebugFeature:feature];
            break;
        }
    }
    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get()]);
}

static void swizzledProcessRequestWithResults(id, SEL, VKImageAnalyzerRequest *, void (^)(double progress), void (^completion)(VKImageAnalysis *, NSError *))
{
    gDidProcessRequestCount++;
    auto analysis = createImageAnalysisWithSimpleFixedResults();
    completion(analysis.get(), nil);
}

#if PLATFORM(IOS_FAMILY)

static CGPoint gSwizzledLocationInView = CGPoint { 100, 100 };
static CGPoint swizzledLocationInView(id, SEL, UIView *)
{
    return gSwizzledLocationInView;
}

static void swizzledProcessRequestWithError(id, SEL, VKImageAnalyzerRequest *, void (^)(double progress), void (^completion)(VKImageAnalysis *analysis, NSError *error))
{
    gDidProcessRequestCount++;
    completion(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:1 userInfo:nil]);
}

TEST(ImageAnalysisTests, DoNotAnalyzeImagesInEditableContent)
{
    InstanceMethodSwizzler gestureLocationSwizzler { UILongPressGestureRecognizer.class, @selector(locationInView:), reinterpret_cast<IMP>(swizzledLocationInView) };
    InstanceMethodSwizzler imageAnalysisRequestSwizzler { PAL::getVKImageAnalyzerClass(), @selector(processRequest:progressHandler:completionHandler:), reinterpret_cast<IMP>(swizzledProcessRequestWithError) };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView _setEditable:YES];
    [webView synchronouslyLoadTestPageNamed:@"image"];

    [webView _imageAnalysisGestureRecognizer].state = UIGestureRecognizerStateBegan;
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(gDidProcessRequestCount, 0U);
}

TEST(ImageAnalysisTests, HandleImageAnalyzerError)
{
    InstanceMethodSwizzler gestureLocationSwizzler { UILongPressGestureRecognizer.class, @selector(locationInView:), reinterpret_cast<IMP>(swizzledLocationInView) };
    InstanceMethodSwizzler imageAnalysisRequestSwizzler { PAL::getVKImageAnalyzerClass(), @selector(processRequest:progressHandler:completionHandler:), reinterpret_cast<IMP>(swizzledProcessRequestWithError) };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"image"];

    [webView _imageAnalysisGestureRecognizer].state = UIGestureRecognizerStateBegan;
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(gDidProcessRequestCount, 1U);
}

TEST(ImageAnalysisTests, DoNotCrashWhenHitTestingOutsideOfWebView)
{
    InstanceMethodSwizzler gestureLocationSwizzler { UILongPressGestureRecognizer.class, @selector(locationInView:), reinterpret_cast<IMP>(swizzledLocationInView) };
    InstanceMethodSwizzler imageAnalysisRequestSwizzler { PAL::getVKImageAnalyzerClass(), @selector(processRequest:progressHandler:completionHandler:), reinterpret_cast<IMP>(swizzledProcessRequestWithError) };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"image"];

    gSwizzledLocationInView = CGPointMake(500, 500);
    [webView _imageAnalysisGestureRecognizer].state = UIGestureRecognizerStateBegan;
    [webView waitForNextPresentationUpdate];
    [webView expectElementCount:1 querySelector:@"img"];
    EXPECT_EQ(gDidProcessRequestCount, 0U);
}

#endif // PLATFORM(IOS_FAMILY)

TEST(ImageAnalysisTests, StartImageAnalysisWithoutIdentifier)
{
    InstanceMethodSwizzler imageAnalysisRequestSwizzler { PAL::getVKImageAnalyzerClass(), @selector(processRequest:progressHandler:completionHandler:), reinterpret_cast<IMP>(swizzledProcessRequestWithResults) };

    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView synchronouslyLoadTestPageNamed:@"multiple-images"];
    [webView _startImageAnalysis:nil];
    [webView waitForImageAnalysisRequests:5];

    NSArray<NSString *> *overlaysAsText = [webView objectByEvaluatingJavaScript:@"imageOverlaysAsText()"];
    EXPECT_EQ(overlaysAsText.count, 5U);
    for (NSString *overlayText in overlaysAsText)
        EXPECT_WK_STREQ(overlayText, @"Foo bar");
}

} // namespace TestWebKitAPI

#endif // ENABLE(IMAGE_ANALYSIS)
