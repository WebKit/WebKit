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
#import "TestInputDelegate.h"
#import "TestUIMenuBuilder.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/LocalizedStrings.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <pal/cocoa/VisionKitCoreSoftLink.h>
#import <pal/spi/cocoa/VisionKitCoreSPI.h>

static unsigned gDidProcessRequestCount = 0;

#if PLATFORM(IOS_FAMILY)

static CGPoint gSwizzledLocationInView = CGPointZero;
static CGPoint swizzledLocationInView(id, SEL, UIView *)
{
    return gSwizzledLocationInView;
}

@interface UIView (ImageAnalysisTesting)
- (void)imageAnalysisGestureDidBegin:(UIGestureRecognizer *)gestureRecognizer;
@end

#endif // PLATFORM(IOS_FAMILY)

@interface VKImageAnalyzerRequest (TestSupport)
@property (nonatomic, readonly) CGImageRef image;
@end

@interface TestWKWebView (ImageAnalysisTests)
- (void)waitForImageAnalysisRequests:(unsigned)numberOfRequests;
#if PLATFORM(IOS_FAMILY)
- (unsigned)simulateImageAnalysisGesture:(CGPoint)location;
#endif
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

#if PLATFORM(IOS_FAMILY)

- (unsigned)simulateImageAnalysisGesture:(CGPoint)location
{
    auto numberOfRequestsAtStart = gDidProcessRequestCount;
    gSwizzledLocationInView = location;
    InstanceMethodSwizzler gestureLocationSwizzler { UILongPressGestureRecognizer.class, @selector(locationInView:), reinterpret_cast<IMP>(swizzledLocationInView) };
    [self.textInputContentView imageAnalysisGestureDidBegin:self._imageAnalysisGestureRecognizer];
    // The process of image analysis involves at most 2 round trips to the web process.
    [self waitForNextPresentationUpdate];
    [self waitForNextPresentationUpdate];
    return gDidProcessRequestCount - numberOfRequestsAtStart;
}

#endif // PLATFORM(IOS_FAMILY)

@end

namespace TestWebKitAPI {

static Vector<RetainPtr<VKImageAnalyzerRequest>>& processedRequests()
{
    static NeverDestroyed requests = Vector<RetainPtr<VKImageAnalyzerRequest>> { };
    return requests.get();
}

static RetainPtr<TestWKWebView> createWebViewWithTextRecognitionEnhancements()
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    for (_WKInternalDebugFeature *feature in WKPreferences._internalDebugFeatures) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"TextRecognitionEnhancementsEnabled"] || [key isEqualToString:@"ImageAnalysisQueueEnabled"] || [key isEqualToString:@"ImageAnalysisMarkupEnabled"])
            [[configuration preferences] _setEnabled:YES forInternalDebugFeature:feature];
    }
    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get()]);
}

static void processRequestWithResults(id, SEL, VKImageAnalyzerRequest *request, void (^)(double progress), void (^completion)(VKImageAnalysis *, NSError *))
{
    gDidProcessRequestCount++;
    processedRequests().append({ request });
    completion(createImageAnalysisWithSimpleFixedResults().get(), nil);
}

static VKImageAnalyzerRequest *makeFakeRequest(id, SEL, CGImageRef image, VKImageOrientation orientation, VKAnalysisTypes requestTypes)
{
    return createRequest(image, orientation, requestTypes).leakRef();
}

template <typename FunctionType>
std::pair<std::unique_ptr<InstanceMethodSwizzler>, std::unique_ptr<InstanceMethodSwizzler>> makeImageAnalysisRequestSwizzler(FunctionType function)
{
    return std::pair {
        makeUnique<InstanceMethodSwizzler>(PAL::getVKImageAnalyzerClass(), @selector(processRequest:progressHandler:completionHandler:), reinterpret_cast<IMP>(function)),
        makeUnique<InstanceMethodSwizzler>(PAL::getVKImageAnalyzerRequestClass(), @selector(initWithCGImage:orientation:requestType:), reinterpret_cast<IMP>(makeFakeRequest))
    };
}

#if PLATFORM(IOS_FAMILY)

static void processRequestWithError(id, SEL, VKImageAnalyzerRequest *request, void (^)(double progress), void (^completion)(VKImageAnalysis *analysis, NSError *error))
{
    gDidProcessRequestCount++;
    processedRequests().append({ request });
    completion(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:1 userInfo:nil]);
}

TEST(ImageAnalysisTests, DoNotAnalyzeImagesInEditableContent)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithError);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView _setEditable:YES];
    [webView synchronouslyLoadTestPageNamed:@"image"];
    EXPECT_EQ([webView simulateImageAnalysisGesture:CGPointMake(100, 100)], 0U);
}

TEST(ImageAnalysisTests, HandleImageAnalyzerErrors)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithError);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"image"];

    EXPECT_EQ([webView simulateImageAnalysisGesture:CGPointMake(100, 100)], 2U);
}

TEST(ImageAnalysisTests, DoNotCrashWhenHitTestingOutsideOfWebView)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithError);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"image"];

    EXPECT_EQ([webView simulateImageAnalysisGesture:CGPointMake(500, 500)], 0U);
    [webView expectElementCount:1 querySelector:@"img"];
}

TEST(ImageAnalysisTests, AvoidRedundantTextRecognitionRequests)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithResults);

    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView synchronouslyLoadTestPageNamed:@"image"];

    EXPECT_EQ([webView simulateImageAnalysisGesture:CGPointMake(150, 100)], 1U);

    // FIXME: If we cache visual look up results as well in the future, we can bring this down to 0 (that is, no new requests).
    EXPECT_LT([webView simulateImageAnalysisGesture:CGPointMake(150, 250)], 2U);
}

#endif // PLATFORM(IOS_FAMILY)

TEST(ImageAnalysisTests, StartImageAnalysisWithoutIdentifier)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithResults);

    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView synchronouslyLoadTestPageNamed:@"multiple-images"];
    [webView _startImageAnalysis:nil];
    [webView waitForImageAnalysisRequests:5];

    NSArray<NSString *> *overlaysAsText = [webView objectByEvaluatingJavaScript:@"imageOverlaysAsText()"];
    EXPECT_EQ(overlaysAsText.count, 5U);
    for (NSString *overlayText in overlaysAsText)
        EXPECT_WK_STREQ(overlayText, @"Foo bar");
}

TEST(ImageAnalysisTests, AnalyzeDynamicallyLoadedImages)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithResults);

    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView synchronouslyLoadTestPageNamed:@"multiple-images"];
    [webView _startImageAnalysis:nil];
    [webView waitForImageAnalysisRequests:5];

    [webView objectByEvaluatingJavaScript:@"appendImage('apple.gif')"];
    [webView waitForImageAnalysisRequests:6];

    [webView objectByEvaluatingJavaScript:@"appendImage('icon.png')"];
    [webView waitForImageAnalysisRequests:7];

    [webView objectByEvaluatingJavaScript:@"hideAllImages()"];
    [webView waitForNextPresentationUpdate];
    [webView objectByEvaluatingJavaScript:@"showAllImages()"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(gDidProcessRequestCount, 7U);
}

TEST(ImageAnalysisTests, ResetImageAnalysisAfterNavigation)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithResults);

    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView synchronouslyLoadTestPageNamed:@"multiple-images"];
    [webView _startImageAnalysis:nil];
    [webView waitForImageAnalysisRequests:5];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    [webView synchronouslyLoadTestPageNamed:@"multiple-images"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(gDidProcessRequestCount, 5U);
}

TEST(ImageAnalysisTests, ImageAnalysisPrioritizesVisibleImages)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithResults);
    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView synchronouslyLoadTestPageNamed:@"offscreen-image"];
    [webView _startImageAnalysis:nil];
    [webView waitForImageAnalysisRequests:2];

    auto firstRequestedImage = [processedRequests().first() image];
    auto lastRequestedImage = [processedRequests().last() image];
    EXPECT_EQ(200U, CGImageGetWidth(firstRequestedImage));
    EXPECT_EQ(150U, CGImageGetHeight(firstRequestedImage));
    EXPECT_EQ(600U, CGImageGetWidth(lastRequestedImage));
    EXPECT_EQ(450U, CGImageGetHeight(lastRequestedImage));
}

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS) && PLATFORM(IOS_FAMILY)

TEST(ImageAnalysisTests, MenuControllerItems)
{
    auto webView = createWebViewWithTextRecognitionEnhancements();
    auto inputDelegate = adoptNS([TestInputDelegate new]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[](WKWebView *, id <_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView _setEditable:YES];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"multiple-images"];
    [webView objectByEvaluatingJavaScript:@"let image = document.images[0]; getSelection().setBaseAndExtent(image, 0, image, 1);"];
    [webView waitForNextPresentationUpdate];

    auto menuBuilder = adoptNS([[TestUIMenuBuilder alloc] init]);
    [webView buildMenuWithBuilder:menuBuilder.get()];
    EXPECT_TRUE([menuBuilder containsActionWithTitle:WebCore::contextMenuItemTitleMarkupImage()]);

    [webView selectAll:nil];
    [webView waitForNextPresentationUpdate];

    [menuBuilder reset];
    [webView buildMenuWithBuilder:menuBuilder.get()];
    EXPECT_FALSE([menuBuilder containsActionWithTitle:WebCore::contextMenuItemTitleMarkupImage()]);

    [webView objectByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(document.body, 0, document.images[0], 1);"];
    [webView waitForNextPresentationUpdate];

    [menuBuilder reset];
    [webView buildMenuWithBuilder:menuBuilder.get()];
    EXPECT_TRUE([menuBuilder containsActionWithTitle:WebCore::contextMenuItemTitleMarkupImage()]);
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS) && PLATFORM(IOS_FAMILY)

} // namespace TestWebKitAPI

#endif // ENABLE(IMAGE_ANALYSIS)
