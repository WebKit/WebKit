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
#import "Test.h"
#import "TestInputDelegate.h"
#import "TestUIMenuBuilder.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/Color.h>
#import <WebCore/LocalizedStrings.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKInternalDebugFeature.h>
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
    }, 3, @"Timed out waiting for %u image analysis requests to complete.", numberOfRequests);

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

// FIXME: We can unify most of this helper class with the logic in `TestPDFPage::colorAtPoint`, and deploy this
// helper class in several other tests that read pixel data from CGImages.
class CGImagePixelReader {
    WTF_MAKE_FAST_ALLOCATED; WTF_MAKE_NONCOPYABLE(CGImagePixelReader);
public:
    CGImagePixelReader(CGImageRef image)
        : m_width(CGImageGetWidth(image))
        , m_height(CGImageGetHeight(image))
    {
        auto colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
        auto bytesPerPixel = 4;
        auto bytesPerRow = bytesPerPixel * CGImageGetWidth(image);
        auto bitsPerComponent = 8;
        auto bitmapInfo = kCGImageAlphaPremultipliedLast | kCGImageByteOrder32Big;
        m_context = adoptCF(CGBitmapContextCreateWithData(nullptr, m_width, m_height, bitsPerComponent, bytesPerRow, colorSpace.get(), bitmapInfo, nullptr, nullptr));
        CGContextDrawImage(m_context.get(), CGRectMake(0, 0, m_width, m_height), image);
    }

    bool isTransparentBlack(unsigned x, unsigned y) const
    {
        return at(x, y) == WebCore::Color::transparentBlack;
    }

    WebCore::Color at(unsigned x, unsigned y) const
    {
        auto* data = reinterpret_cast<uint8_t*>(CGBitmapContextGetData(m_context.get()));
        auto offset = 4 * (width() * y + x);
        return WebCore::makeFromComponentsClampingExceptAlpha<WebCore::SRGBA<uint8_t>>(data[offset], data[offset + 1], data[offset + 2], data[offset + 3]);
    }

    unsigned width() const { return m_width; }
    unsigned height() const { return m_height; }

private:
    unsigned m_width { 0 };
    unsigned m_height { 0 };
    RetainPtr<CGContextRef> m_context;
};

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
        if ([key isEqualToString:@"TextRecognitionInVideosEnabled"] || [key isEqualToString:@"VisualTranslationEnabled"] || [key isEqualToString:@"RemoveBackgroundEnabled"])
            [[configuration preferences] _setEnabled:YES forInternalDebugFeature:feature];
    }
    [configuration _setAttachmentElementEnabled:YES];
#if ENABLE(SERVICE_CONTROLS)
    [configuration _setImageControlsEnabled:YES];
#endif
    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get()]);
}

static void processRequestWithError(id, SEL, VKImageAnalyzerRequest *request, void (^)(double progress), void (^completion)(VKImageAnalysis *analysis, NSError *error))
{
    gDidProcessRequestCount++;
    processedRequests().append({ request });
    completion(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:1 userInfo:nil]);
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
    [webView _startImageAnalysis:nil target:nil];
    [webView waitForImageAnalysisRequests:5];

    NSArray<NSString *> *overlaysAsText = [webView objectByEvaluatingJavaScript:@"imageOverlaysAsText()"];
    EXPECT_EQ(overlaysAsText.count, 5U);
    for (NSString *overlayText in overlaysAsText)
        EXPECT_WK_STREQ(overlayText, @"Foo bar");
}

TEST(ImageAnalysisTests, AnalyzeImagesInSubframes)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithResults);

    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView synchronouslyLoadTestPageNamed:@"multiple-images"];
    __block bool doneInsertingFrame = false;
    [webView callAsyncJavaScript:@"appendAndLoadSubframe(source)" arguments:@{ @"source" : @"multiple-images.html" } inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
        doneInsertingFrame = true;
    }];
    Util::run(&doneInsertingFrame);

    [webView _startImageAnalysis:nil target:nil];
    [webView waitForImageAnalysisRequests:10];
}

TEST(ImageAnalysisTests, AnalyzeImageAfterChangingSource)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithResults);

    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView synchronouslyLoadTestPageNamed:@"image"];
    [webView _startImageAnalysis:nil target:nil];
    [webView waitForImageAnalysisRequests:1];

    [webView objectByEvaluatingJavaScript:@"document.querySelector('img').src = 'icon.png'"];
    [webView waitForImageAnalysisRequests:2];
}

TEST(ImageAnalysisTests, AnalyzeDynamicallyLoadedImages)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithResults);

    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView synchronouslyLoadTestPageNamed:@"multiple-images"];
    [webView _startImageAnalysis:nil target:nil];
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
    [webView _startImageAnalysis:nil target:nil];
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
    [webView _startImageAnalysis:nil target:nil];
    [webView waitForImageAnalysisRequests:2];

    auto firstRequestedImage = [processedRequests().first() image];
    auto lastRequestedImage = [processedRequests().last() image];
    EXPECT_EQ(200U, CGImageGetWidth(firstRequestedImage));
    EXPECT_EQ(150U, CGImageGetHeight(firstRequestedImage));
    EXPECT_EQ(600U, CGImageGetWidth(lastRequestedImage));
    EXPECT_EQ(450U, CGImageGetHeight(lastRequestedImage));
}

TEST(ImageAnalysisTests, ImageAnalysisWithTransparentImages)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithError);
    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView synchronouslyLoadTestPageNamed:@"fade-in-image"];
    [webView _startImageAnalysis:@"en_US" target:@"zh_TW"];
    [webView waitForImageAnalysisRequests:1];

    CGImagePixelReader reader { [processedRequests().first() image] };
    EXPECT_TRUE(reader.isTransparentBlack(1, 1));
    EXPECT_TRUE(reader.isTransparentBlack(reader.width() - 1, 1));
    EXPECT_TRUE(reader.isTransparentBlack(reader.width() - 1, reader.height() - 1));
    EXPECT_TRUE(reader.isTransparentBlack(1, reader.height() - 1));
    EXPECT_FALSE(reader.isTransparentBlack(reader.width() / 2, reader.height() / 2));
}

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

static RetainPtr<CGImageRef> iconImage()
{
    auto iconPath = [NSBundle.mainBundle pathForResource:@"icon" ofType:@"png" inDirectory:@"TestWebKitAPI.resources"];
#if PLATFORM(IOS_FAMILY)
    return [UIImage imageWithContentsOfFile:iconPath].CGImage;
#else
    auto image = adoptNS([[NSImage alloc] initWithContentsOfFile:iconPath]);
    return [image CGImageForProposedRect:nil context:nil hints:nil];
#endif
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS) && PLATFORM(IOS_FAMILY)

static void simulateCalloutBarAppearance(TestWKWebView *webView)
{
    __block bool done = false;
    [webView.textInputContentView requestPreferredArrowDirectionForEditMenuWithCompletionHandler:^(UIEditMenuArrowDirection) {
        done = true;
    }];
    Util::run(&done);
}

static BOOL simulateEditContextMenuAppearance(TestWKWebView *webView, CGPoint location)
{
    __block BOOL result;
    __block bool done = false;
    [webView.textInputContentView prepareSelectionForContextMenuWithLocationInView:location completionHandler:^(BOOL shouldPresent, RVItem *) {
        result = shouldPresent;
        done = true;
    }];
    Util::run(&done);
    return result;
}

static void invokeRemoveBackgroundAction(TestWKWebView *webView)
{
    simulateCalloutBarAppearance(webView);

    auto menuBuilder = adoptNS([[TestUIMenuBuilder alloc] init]);
    [webView buildMenuWithBuilder:menuBuilder.get()];
    [[menuBuilder actionWithTitle:WebCore::contextMenuItemTitleRemoveBackground()] performWithSender:nil target:nil];
    [webView waitForNextPresentationUpdate];
}

TEST(ImageAnalysisTests, RemoveBackgroundUsingContextMenu)
{
    RemoveBackgroundSwizzler swizzler { iconImage().autorelease(), CGRectMake(10, 10, 215, 174) };

    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView _setEditable:YES];
    [webView synchronouslyLoadTestPageNamed:@"image"];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE(simulateEditContextMenuAppearance(webView.get(), CGPointMake(100, 100)));

    auto menuBuilder = adoptNS([[TestUIMenuBuilder alloc] init]);
    [webView buildMenuWithBuilder:menuBuilder.get()];
    EXPECT_NOT_NULL([menuBuilder actionWithTitle:WebCore::contextMenuItemTitleRemoveBackground()]);
}

TEST(ImageAnalysisTests, MenuControllerItems)
{
    RemoveBackgroundSwizzler swizzler { iconImage().autorelease(), CGRectMake(10, 10, 215, 174) };

    auto webView = createWebViewWithTextRecognitionEnhancements();
    auto inputDelegate = adoptNS([TestInputDelegate new]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[](WKWebView *, id <_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView _setEditable:YES];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"multiple-images"];
    [webView objectByEvaluatingJavaScript:@"let image = document.images[0]; nodeIndex = [...image.parentNode.childNodes].indexOf(image);"
        "getSelection().setBaseAndExtent(image.parentNode, nodeIndex, image.parentNode, nodeIndex + 1);"];
    [webView waitForNextPresentationUpdate];
    simulateCalloutBarAppearance(webView.get());

    auto menuBuilder = adoptNS([[TestUIMenuBuilder alloc] init]);
    [webView buildMenuWithBuilder:menuBuilder.get()];
    EXPECT_NOT_NULL([menuBuilder actionWithTitle:WebCore::contextMenuItemTitleRemoveBackground()]);

    [webView selectAll:nil];
    [webView waitForNextPresentationUpdate];
    simulateCalloutBarAppearance(webView.get());

    [menuBuilder reset];
    [webView buildMenuWithBuilder:menuBuilder.get()];
    EXPECT_NULL([menuBuilder actionWithTitle:WebCore::contextMenuItemTitleRemoveBackground()]);

    [webView objectByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(document.body, 0, image.parentNode, nodeIndex + 1);"];
    [webView waitForNextPresentationUpdate];
    simulateCalloutBarAppearance(webView.get());

    [menuBuilder reset];
    [webView buildMenuWithBuilder:menuBuilder.get()];
    EXPECT_NOT_NULL([menuBuilder actionWithTitle:WebCore::contextMenuItemTitleRemoveBackground()]);
}

static RetainPtr<TestWKWebView> runMarkupTest(NSString *testPage, NSString *scriptToSelectText, Function<void(TestWKWebView *, NSString *)>&& checkWebView = { })
{
    RemoveBackgroundSwizzler swizzler { iconImage().autorelease(), CGRectMake(10, 10, 215, 174) };

    auto webView = createWebViewWithTextRecognitionEnhancements();
    auto inputDelegate = adoptNS([TestInputDelegate new]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[](WKWebView *, id <_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView _setEditable:YES];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadTestPageNamed:testPage];
    [webView objectByEvaluatingJavaScript:scriptToSelectText];
    [webView waitForNextPresentationUpdate];

    NSString *previousSelectedText = [webView selectedText];
    invokeRemoveBackgroundAction(webView.get());

    if (checkWebView)
        checkWebView(webView.get(), previousSelectedText);

    return webView;
}

TEST(ImageAnalysisTests, PerformRemoveBackground)
{
    runMarkupTest(@"multiple-images", @"image = document.images[0]; getSelection().setBaseAndExtent(document.body, 0, image.parentNode, [...image.parentNode.childNodes].indexOf(image) + 1)", [](TestWKWebView *webView, NSString *previousSelectedText) {
        EXPECT_WK_STREQ(previousSelectedText, webView.selectedText);
        Util::waitForConditionWithLogging([&] {
            return [[webView objectByEvaluatingJavaScript:@"document.images[0].getBoundingClientRect().width"] intValue] == 215;
        }, 3, @"Expected bounding client rect to become 215.");

        NSString *undoTitle = webView.undoManager.undoMenuItemTitle;
        EXPECT_GT(undoTitle.length, 0U);
        EXPECT_FALSE([undoTitle containsString:@"Paste"]);
    });
}

TEST(ImageAnalysisTests, PerformRemoveBackgroundWithWebPImages)
{
    runMarkupTest(@"webp-image", @"getSelection().selectAllChildren(document.body)", [](TestWKWebView *webView, NSString *) {
        Util::waitForConditionWithLogging([&] {
            return [[webView objectByEvaluatingJavaScript:@"document.images[0].getBoundingClientRect().width"] intValue] == 215;
        }, 3, @"Expected bounding client rect to become 215.");
    });
}

TEST(ImageAnalysisTests, AllowRemoveBackgroundOnce)
{
    auto webView = runMarkupTest(@"image", @"getSelection().selectAllChildren(document.body)");

    RemoveBackgroundSwizzler swizzler { iconImage().autorelease(), CGRectMake(10, 10, 215, 174) };

    [webView objectByEvaluatingJavaScript:@"let image = document.images[0]; nodeIndex = [...image.parentNode.childNodes].indexOf(image);"
        "getSelection().setBaseAndExtent(image.parentNode, nodeIndex, image.parentNode, nodeIndex + 1)"];
    [webView waitForNextPresentationUpdate];
    simulateCalloutBarAppearance(webView.get());

    auto menuBuilder = adoptNS([TestUIMenuBuilder new]);
    [webView buildMenuWithBuilder:menuBuilder.get()];

    EXPECT_NULL([menuBuilder actionWithTitle:WebCore::contextMenuItemTitleRemoveBackground()]);
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS) && PLATFORM(IOS_FAMILY)

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS) && ENABLE(SERVICE_CONTROLS)

TEST(ImageAnalysisTests, RemoveBackgroundItemInServicesMenu)
{
    RemoveBackgroundSwizzler swizzler { iconImage().autorelease(), CGRectMake(10, 10, 215, 174) };

    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView _setEditable:YES];
    [webView synchronouslyLoadTestPageNamed:@"image-controls"];
    [[webView window] orderFrontRegardless];
    [webView callAsyncJavaScript:@"waitForAndClickImageControls()" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];

    __block bool foundRemoveBackgroundItem = false;
    RetainPtr timer = [NSTimer timerWithTimeInterval:0.1 repeats:YES block:^(NSTimer *) {
        NSMenu *menu = [webView _activeMenu];
        for (NSMenuItem *item in menu.itemArray) {
            if ([item.title isEqualToString:WebCore::contextMenuItemTitleRemoveBackground()]) {
                foundRemoveBackgroundItem = true;
                [menu cancelTracking];
                break;
            }
        }
    }];

    [NSRunLoop.mainRunLoop addTimer:timer.get() forMode:NSEventTrackingRunLoopMode];
    Util::run(&foundRemoveBackgroundItem);
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS) && ENABLE(SERVICE_CONTROLS)

} // namespace TestWebKitAPI

#endif // ENABLE(IMAGE_ANALYSIS)
