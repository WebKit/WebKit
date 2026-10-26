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

#import "Helpers/cocoa/CGImagePixelReader.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/ImageAnalysisTestingUtilities.h"
#import "InstanceMethodSwizzler.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/PasteboardUtilities.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "TestInputDelegate.h"
#import "Helpers/ios/TestUIMenuBuilder.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/LocalizedStrings.h>
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKFeature.h>
#import <pal/spi/cocoa/VisionKitCoreSPI.h>
#import <wtf/MonotonicTime.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/MakeString.h>

#if PLATFORM(MAC)
#import "Helpers/mac/DataDetectorHighlightTestUtilities.h"
#endif

#import <pal/cocoa/VisionKitCoreSoftLink.h>

static unsigned gImageAnalysisDidProcessRequestCount = 0;

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
        return gImageAnalysisDidProcessRequestCount == numberOfRequests;
    }, 3, @"Timed out waiting for %u image analysis requests to complete.", numberOfRequests);

    [self waitForNextPresentationUpdate];
    EXPECT_EQ(gImageAnalysisDidProcessRequestCount, numberOfRequests);
}

#if PLATFORM(IOS_FAMILY)

- (unsigned)simulateImageAnalysisGesture:(CGPoint)location
{
    auto numberOfRequestsAtStart = gImageAnalysisDidProcessRequestCount;
    gSwizzledLocationInView = location;
    InstanceMethodSwizzler gestureLocationSwizzler { UILongPressGestureRecognizer.class, @selector(locationInView:), reinterpret_cast<IMP>(swizzledLocationInView) };
    [self.textInputContentView imageAnalysisGestureDidBegin:self._imageAnalysisGestureRecognizer];
    // The process of image analysis involves at most 2 round trips to the web process.
    [self waitForNextPresentationUpdate];
    [self waitForNextPresentationUpdate];
    return gImageAnalysisDidProcessRequestCount - numberOfRequestsAtStart;
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
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"TextRecognitionInVideosEnabled"] || [key isEqualToString:@"VisualTranslationEnabled"] || [key isEqualToString:@"RemoveBackgroundEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }
    [configuration _setAttachmentElementEnabled:YES];
#if ENABLE(SERVICE_CONTROLS)
    [configuration _setImageControlsEnabled:YES];
#endif
    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get()]);
}

static void processRequestWithError(id, SEL, VKImageAnalyzerRequest *request, void (^)(double progress), void (^completion)(VKImageAnalysis *analysis, NSError *error))
{
    gImageAnalysisDidProcessRequestCount++;
    processedRequests().append({ request });
    completion(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:1 userInfo:nil]);
}

static void processRequestWithResults(id, SEL, VKImageAnalyzerRequest *request, void (^)(double progress), void (^completion)(VKImageAnalysis *, NSError *))
{
    gImageAnalysisDidProcessRequestCount++;
    processedRequests().append({ request });
    completion(createImageAnalysisWithSimpleFixedResults().get(), nil);
}

static VKImageAnalyzerRequest *makeFakeRequest(id, SEL, CGImageRef image, VKImageOrientation orientation, VKAnalysisTypes requestTypes)
{
    return createRequest(image, orientation, requestTypes).leakRef();
}

template <typename FunctionType>
std::pair<std::unique_ptr<InstanceMethodSwizzler>, std::unique_ptr<InstanceMethodSwizzler>> imageAnalysisMakeRequestSwizzler(FunctionType function)
{
    return std::pair {
        makeUnique<InstanceMethodSwizzler>(PAL::getVKImageAnalyzerClassSingleton(), @selector(processRequest:progressHandler:completionHandler:), reinterpret_cast<IMP>(function)),
        makeUnique<InstanceMethodSwizzler>(PAL::getVKImageAnalyzerRequestClassSingleton(), @selector(initWithCGImage:orientation:requestType:), reinterpret_cast<IMP>(makeFakeRequest))
    };
}

#if PLATFORM(IOS_FAMILY)

TEST(ImageAnalysisTests, DoNotAnalyzeImagesInEditableContent)
{
    auto requestSwizzler = imageAnalysisMakeRequestSwizzler(processRequestWithError);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView _setEditable:YES];
    [webView synchronouslyLoadTestPageNamed:@"image"];
    EXPECT_EQ([webView simulateImageAnalysisGesture:CGPointMake(100, 100)], 0U);
}

TEST(ImageAnalysisTests, HandleImageAnalyzerErrors)
{
    auto requestSwizzler = imageAnalysisMakeRequestSwizzler(processRequestWithError);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"image"];

    EXPECT_EQ([webView simulateImageAnalysisGesture:CGPointMake(100, 100)], 2U);
}

TEST(ImageAnalysisTests, DoNotCrashWhenHitTestingOutsideOfWebView)
{
    auto requestSwizzler = imageAnalysisMakeRequestSwizzler(processRequestWithError);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"image"];

    EXPECT_EQ([webView simulateImageAnalysisGesture:CGPointMake(500, 500)], 0U);
    [webView expectElementCount:1 querySelector:@"img"];
}

TEST(ImageAnalysisTests, AvoidRedundantTextRecognitionRequests)
{
    auto requestSwizzler = imageAnalysisMakeRequestSwizzler(processRequestWithResults);

    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView synchronouslyLoadTestPageNamed:@"image"];

    EXPECT_EQ([webView simulateImageAnalysisGesture:CGPointMake(150, 100)], 1U);

    // FIXME: If we cache visual look up results as well in the future, we can bring this down to 0 (that is, no new requests).
    EXPECT_LT([webView simulateImageAnalysisGesture:CGPointMake(150, 250)], 2U);
}

TEST(ImageAnalysisTests, TextRecognitionInCrossOriginIframe)
{
    auto requestSwizzler = imageAnalysisMakeRequestSwizzler(processRequestWithResults);

    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe style='display: block; margin: 100px; width: 200px; height: 200px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<body style='margin: 0'><img style='display: block; width: 200px; height: 200px;' src='large-red-square.png'></body>"_s } },
        { "/large-red-square.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"large-red-square" withExtension:@"png"]] } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration setWebsiteDataStore:[server.httpsProxyConfiguration() websiteDataStore]];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 400, 400));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    [webView simulateImageAnalysisGesture:CGPointMake(200, 200)];

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"internals.shadowRoot(document.images[0])?.getElementById('image-overlay')?.textContent ?? ''" inFrame:childFrame.get()] isEqualToString:@"Foo bar"];
    }));
}

#endif // PLATFORM(IOS_FAMILY)

TEST(ImageAnalysisTests, StartImageAnalysisWithoutIdentifier)
{
    auto requestSwizzler = imageAnalysisMakeRequestSwizzler(processRequestWithResults);

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
    auto requestSwizzler = imageAnalysisMakeRequestSwizzler(processRequestWithResults);

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
    auto requestSwizzler = imageAnalysisMakeRequestSwizzler(processRequestWithResults);

    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView synchronouslyLoadTestPageNamed:@"image"];
    [webView _startImageAnalysis:nil target:nil];
    [webView waitForImageAnalysisRequests:1];

    [webView objectByEvaluatingJavaScript:@"document.querySelector('img').src = 'icon.png'"];
    [webView waitForImageAnalysisRequests:2];
}

TEST(ImageAnalysisTests, AnalyzeDynamicallyLoadedImages)
{
    auto requestSwizzler = imageAnalysisMakeRequestSwizzler(processRequestWithResults);

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
    EXPECT_EQ(gImageAnalysisDidProcessRequestCount, 7U);
}

TEST(ImageAnalysisTests, ResetImageAnalysisAfterNavigation)
{
    auto requestSwizzler = imageAnalysisMakeRequestSwizzler(processRequestWithResults);

    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView synchronouslyLoadTestPageNamed:@"multiple-images"];
    [webView _startImageAnalysis:nil target:nil];
    [webView waitForImageAnalysisRequests:5];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    [webView synchronouslyLoadTestPageNamed:@"multiple-images"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(gImageAnalysisDidProcessRequestCount, 5U);
}

TEST(ImageAnalysisTests, ImageAnalysisPrioritizesVisibleImages)
{
    auto requestSwizzler = imageAnalysisMakeRequestSwizzler(processRequestWithResults);
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
    auto requestSwizzler = imageAnalysisMakeRequestSwizzler(processRequestWithError);
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

static RetainPtr<CGImageRef> iconImage()
{
    auto iconPath = [NSBundle.test_resourcesBundle pathForResource:@"icon" ofType:@"png"];
#if PLATFORM(IOS_FAMILY)
    return [UIImage imageWithContentsOfFile:iconPath].CGImage;
#else
    RetainPtr image = adoptNS([[NSImage alloc] initWithContentsOfFile:iconPath]);
    return [image CGImageForProposedRect:nil context:nil hints:nil];
#endif
}

#if PLATFORM(IOS_FAMILY)

static void simulateEditMenuAppearance(TestWKWebView *webView)
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
    [webView selectTextForContextMenuWithLocationInView:location completion:^(BOOL shouldPresent) {
        result = shouldPresent;
        done = true;
    }];
    Util::run(&done);
    return result;
}

static void invokeRemoveBackgroundAction(TestWKWebView *webView)
{
    simulateEditMenuAppearance(webView);

    RetainPtr menuBuilder = adoptNS([[TestUIMenuBuilder alloc] init]);
    [webView buildMenuWithBuilder:menuBuilder.get()];
    [[menuBuilder actionWithTitle:WebCore::contextMenuItemTitleRemoveBackground().createNSString().get()] performWithSender:nil target:nil];
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

    RetainPtr menuBuilder = adoptNS([[TestUIMenuBuilder alloc] init]);
    [webView buildMenuWithBuilder:menuBuilder.get()];
    EXPECT_NOT_NULL([menuBuilder actionWithTitle:WebCore::contextMenuItemTitleRemoveBackground().createNSString().get()]);
}

TEST(ImageAnalysisTests, MenuControllerItems)
{
    RemoveBackgroundSwizzler swizzler { iconImage().autorelease(), CGRectMake(10, 10, 215, 174) };

    auto webView = createWebViewWithTextRecognitionEnhancements();
    RetainPtr inputDelegate = adoptNS([TestInputDelegate new]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[](WKWebView *, id <_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView _setEditable:YES];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"multiple-images"];
    [webView objectByEvaluatingJavaScript:@"let image = document.images[0]; nodeIndex = [...image.parentNode.childNodes].indexOf(image);"
        "getSelection().setBaseAndExtent(image.parentNode, nodeIndex, image.parentNode, nodeIndex + 1);"];
    [webView waitForNextPresentationUpdate];
    simulateEditMenuAppearance(webView.get());

    RetainPtr menuBuilder = adoptNS([[TestUIMenuBuilder alloc] init]);
    [webView buildMenuWithBuilder:menuBuilder.get()];
    EXPECT_NOT_NULL([menuBuilder actionWithTitle:WebCore::contextMenuItemTitleRemoveBackground().createNSString().get()]);

    [webView selectAll:nil];
    [webView waitForNextPresentationUpdate];
    simulateEditMenuAppearance(webView.get());

    [menuBuilder reset];
    [webView buildMenuWithBuilder:menuBuilder.get()];
    EXPECT_NULL([menuBuilder actionWithTitle:WebCore::contextMenuItemTitleRemoveBackground().createNSString().get()]);

    [webView objectByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(document.body, 0, image.parentNode, nodeIndex + 1);"];
    [webView waitForNextPresentationUpdate];
    simulateEditMenuAppearance(webView.get());

    [menuBuilder reset];
    [webView buildMenuWithBuilder:menuBuilder.get()];
    EXPECT_NOT_NULL([menuBuilder actionWithTitle:WebCore::contextMenuItemTitleRemoveBackground().createNSString().get()]);
}

static RetainPtr<TestWKWebView> runMarkupTest(NSString *testPage, NSString *scriptToSelectText, Function<void(TestWKWebView *, NSString *)>&& checkWebView = { })
{
    RemoveBackgroundSwizzler swizzler { iconImage().autorelease(), CGRectMake(10, 10, 215, 174) };

    auto webView = createWebViewWithTextRecognitionEnhancements();
    RetainPtr inputDelegate = adoptNS([TestInputDelegate new]);
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
    simulateEditMenuAppearance(webView.get());

    RetainPtr menuBuilder = adoptNS([TestUIMenuBuilder new]);
    [webView buildMenuWithBuilder:menuBuilder.get()];

    EXPECT_NULL([menuBuilder actionWithTitle:WebCore::contextMenuItemTitleRemoveBackground().createNSString().get()]);
}

TEST(ImageAnalysisTests, PerformRemoveBackgroundInCrossOriginIframe)
{
    RemoveBackgroundSwizzler swizzler { iconImage().autorelease(), CGRectMake(10, 10, 215, 174) };

    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable><img src='large-red-square.png'></body>"_s } },
        { "/large-red-square.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"large-red-square" withExtension:@"png"]] } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration.get(), @"RemoveBackgroundEnabled", true);
    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server, configuration.get());
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);
    [webView waitForNextPresentationUpdate];

    invokeRemoveBackgroundAction(webView.get());
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"document.images[0].getBoundingClientRect().width" inFrame:childFrame.get()] intValue] == 215;
    }));
}

#endif // PLATFORM(IOS_FAMILY)

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

static bool generalPasteboardContainsRichTextData()
{
#if PLATFORM(MAC)
    RetainPtr pasteboard = [NSPasteboard generalPasteboard];
    return !![pasteboard dataForType:NSPasteboardTypeRTF] || !![pasteboard dataForType:NSPasteboardTypeRTFD];
#else
    for (NSItemProvider *itemProvider in UIPasteboard.generalPasteboard.itemProviders) {
        for (NSString *identifier in itemProvider.registeredTypeIdentifiers) {
            RetainPtr type = [UTType typeWithIdentifier:identifier];
            if ([type conformsToType:UTTypeRTF] || [type conformsToType:UTTypeFlatRTFD])
                return true;
        }
    }
    return false;
#endif
}

static NSString *stringFromGeneralPasteboard()
{
#if PLATFORM(MAC)
    return [NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString];
#else
    return UIPasteboard.generalPasteboard.string;
#endif
}

TEST(ImageAnalysisTests, CopyImageOverlayTextWithoutAttributedString)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    for (_WKFeature *feature in WKPreferences._features) {
        if ([feature.key isEqualToString:@"WriteRichTextDataWhenCopyingOrDragging"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 300) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"simple-image-overlay"];
    [webView stringByEvaluatingJavaScript:@"selectImageOverlay()"];
    [webView waitForNextPresentationUpdate];

    clearPasteboard();
    [webView copy:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE(generalPasteboardContainsRichTextData());
    EXPECT_TRUE([stringFromGeneralPasteboard() containsString:@"foobar"]);
    EXPECT_WK_STREQ("IMG", [webView stringByEvaluatingJavaScript:@"document.querySelector('img').tagName"]);
}

#endif // !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

#if PLATFORM(MAC) && ENABLE(DATA_DETECTION) && ENABLE(REVEAL)

TEST(ImageAnalysisTests, DataDetectorMenuLocationInSubframe)
{
    __block bool didShowMenu = false;
    __block CGRect elementBounds = CGRectNull;
    __block CGPoint menuLocation = CGPointZero;
    InstanceMethodSwizzler showContextMenuSwizzler {
        NSClassFromString(@"WKRevealItemPresenter"),
        NSSelectorFromString(@"showContextMenu"),
        imp_implementationWithBlock(^(NSObject *presenter) {
            elementBounds = [(NSValue *)[presenter valueForKey:@"frameInView"] rectValue];
            menuLocation = [(NSValue *)[presenter valueForKey:@"menuLocationInView"] pointValue];
            didShowMenu = true;
        })
    };

    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<body style='margin: 0'><iframe style='position: absolute; left: 100px; top: 150px; width: 500px; height: 300px; border: none;' srcdoc=\"<body style='margin: 0'><img style='display: block; width: 400px; height: 100px;' src='400x400-green.png'></body>\"></iframe></body>"];
    [webView objectByEvaluatingJavaScript:@"(() => {"
        @"    const quad = { topLeft: new DOMPointReadOnly(0, 0.5), topRight: new DOMPointReadOnly(0.5, 0.5), bottomRight: new DOMPointReadOnly(0.5, 1), bottomLeft: new DOMPointReadOnly(0, 1) };"
        @"    const image = document.querySelector('iframe').contentDocument.querySelector('img');"
        @"    internals.installImageOverlay(image, [{ ...quad, children: [{ text: 'webkit.org', ...quad }] }], [], [quad]);"
        @"    return true;"
        @"})()"];
    [webView waitForNextPresentationUpdate];

    auto moveMouse = [&](CGPoint pointInView) {
        [webView mouseMoveToPoint:[webView convertPoint:pointInView toView:nil] withFlags:0];
        [webView waitForPendingMouseEvents];
    };

    // The data detector is at (0, 50, 200, 50) in the iframe, whose content box is at (100, 150).
    auto expectedElementBounds = CGRectMake(100, 200, 200, 50);
    moveMouse(CGPointMake(199, 224));
    moveMouse(CGPointMake(200, 225));
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"internals.hasActiveDataDetectorHighlight"] boolValue];
    }));

    auto buttonLocation = dataDetectorHighlightGeometry(expectedElementBounds, [webView bounds]).buttonCenter;
    moveMouse(buttonLocation);
    [webView sendClickAtPoint:[webView convertPoint:buttonLocation toView:nil]];
    EXPECT_TRUE(Util::runFor(&didShowMenu, 5_s));

    EXPECT_NEAR(elementBounds.origin.x, expectedElementBounds.origin.x, 1);
    EXPECT_NEAR(elementBounds.origin.y, expectedElementBounds.origin.y, 1);
    EXPECT_NEAR(menuLocation.x, buttonLocation.x, 1);
    EXPECT_NEAR(menuLocation.y, buttonLocation.y, 1);
}

static constexpr CGFloat liveTextDataDetectorRectTolerance = 2;

class LiveTextDataDetectorInSubframe : public DataDetectorInSubframeTest {
public:
    static constexpr int nestedFrameWidth = 340;
    static constexpr int nestedFrameHeight = 200;
    static constexpr NSRect nestedFrameContentBox { nestedFrameContentOrigin, { nestedFrameWidth, nestedFrameHeight } };
    static constexpr size_t fiveSecondsOfTries { 50 };

    void SetUp() override
    {
        static constexpr auto imageHTML { "<body style='margin:0'><img style='display:block; margin:40px 0 0 30px; width:300px; height:100px' src='large-red-square.png'>"_s };
        static constexpr auto mainFrameImageHTML { "<img style='position:absolute; left:560px; top:120px; width:300px; height:100px' src='large-red-square.png'>"_s };
        String secondIframeOrigin { GetParam().crossOrigin ? "https://apple.com"_s : "https://example.com"_s };
        setUpWebView({
            { "/main"_s, html(mainHTML(iframe(iframeLeft, GetParam().subframeURL("/image.html"_s)))) },
            { "/main-with-scrollable-iframe"_s, html(mainHTML(iframe(iframeLeft, GetParam().subframeURL("/tall-image.html"_s)))) },
            { "/main-with-two-iframes"_s, html(mainHTML(makeString(iframe(iframeLeft, GetParam().subframeURL("/image.html"_s)), iframe(secondIframeLeft, makeString(secondIframeOrigin, "/text.html"_s))))) },
            { "/main-with-two-image-iframes"_s, html(mainHTML(makeString(iframe(iframeLeft, GetParam().subframeURL("/image.html"_s)), iframe(secondIframeLeft, makeString(secondIframeOrigin, "/image.html"_s))))) },
            { "/main-with-nested-frame"_s, html(mainHTML(iframe(iframeLeft, GetParam().subframeURL("/nested.html"_s)))) },
            { "/main-with-nested-main-site-frame"_s, html(mainHTML(iframe(iframeLeft, GetParam().subframeURL("/nested-main-site.html"_s)))) },
            { "/main-with-nested-main-site-frame-and-image"_s, html(mainHTML(makeString(iframe(iframeLeft, GetParam().subframeURL("/nested-main-site.html"_s)), mainFrameImageHTML))) },
            { "/nested.html"_s, html(nestedFrameHTML("image.html"_s, nestedFrameWidth, nestedFrameHeight)) },
            { "/nested-main-site.html"_s, html(nestedFrameHTML("https://example.com/image.html"_s, nestedFrameWidth, nestedFrameHeight)) },
            { "/image.html"_s, html(makeString(imageHTML, "</body>"_s)) },
            { "/tall-image.html"_s, html(makeString(imageHTML, "<div style='height:1000px'></div></body>"_s)) },
            { "/text.html"_s, html("<body style='margin:0; font:20px Helvetica'><p style='margin:40px'>No image here</p></body>"_s) },
            { "/large-red-square.png"_s, HTTPResponse { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"large-red-square" withExtension:@"png"]] } },
        }, [](WKWebViewConfiguration *) { });
    }

    CGRect imageOverlayElementRect(WKFrameInfo *frame, CGPoint frameContentOrigin, NSString *elementScript)
    {
        RetainPtr rect { dynamic_objc_cast<NSArray>([webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"(() => {"
            "  const overlay = internals.shadowRoot(document.images[0]);"
            "  const rect = %@.getBoundingClientRect();"
            "  return [rect.x, rect.y, rect.width, rect.height];"
            "})()", elementScript] inFrame:frame]) };
        return rectInWebView(rect.get(), frameContentOrigin);
    }

    CGRect installImageOverlayWithDataDetector(WKFrameInfo *frame, CGPoint frameContentOrigin)
    {
        EXPECT_TRUE(Util::waitFor([&] {
            return [[webView objectByEvaluatingJavaScript:@"document.images[0].complete" inFrame:frame] boolValue];
        }));
        [webView objectByEvaluatingJavaScript:@"(() => {"
            "  const quad = (x1, y1, x2, y2) => ({ topLeft: new DOMPointReadOnly(x1, y1), topRight: new DOMPointReadOnly(x2, y1), bottomRight: new DOMPointReadOnly(x2, y2), bottomLeft: new DOMPointReadOnly(x1, y2) });"
            "  const dataDetector = quad(0, 0.5, 0.5, 1);"
            "  const otherLine = quad(0.6, 0, 1, 0.4);"
            "  internals.installImageOverlay(document.images[0], ["
            "    { ...dataDetector, children: [{ text: 'webkit.org', ...dataDetector }] },"
            "    { ...otherLine, children: [{ text: 'other', ...otherLine }] },"
            "  ], [], [dataDetector]);"
            "  return true;"
            "})()" inFrame:frame];
        [webView waitForNextPresentationUpdate];
        return imageOverlayElementRect(frame, frameContentOrigin, @"overlay.querySelector('.image-overlay-data-detector-result')");
    }

    CGRect otherLineRect(WKFrameInfo *frame, CGPoint frameContentOrigin)
    {
        return imageOverlayElementRect(frame, frameContentOrigin, @"overlay.querySelectorAll('.image-overlay-line')[1]");
    }

    bool hasActiveHighlight(WKFrameInfo *frame)
    {
        return [[webView objectByEvaluatingJavaScript:@"internals.hasActiveDataDetectorHighlight" inFrame:frame] boolValue];
    }

    bool hasVisibleHighlightLayer(CGRect highlightBoundingRect)
    {
        return static_cast<bool>(hasVisibleLayerNear([webView layer], highlightBoundingRect, liveTextDataDetectorRectTolerance));
    }

    testing::AssertionResult hoverUntilHighlightIsActive(WKFrameInfo *frame, CGRect dataDetectorRect, CGRect highlightBoundingRect)
    {
        auto isShown { hoverUntilHighlightIsShown(webView.get(), dataDetectorRect, highlightBoundingRect, liveTextDataDetectorRectTolerance) };
        if (!isShown)
            return isShown;
        if (!hasActiveHighlight(frame))
            return testing::AssertionFailure() << "The highlight's layer is visible, but the image's process has no active highlight.";
        return testing::AssertionSuccess();
    }

    bool moveAwayUntilHighlightIsHidden(WKFrameInfo *frame, CGPoint point, CGRect highlightBoundingRect)
    {
        moveMouse(point);
        return Util::waitFor([&] {
            [webView waitForNextPresentationUpdate];
            return !hasVisibleHighlightLayer(highlightBoundingRect) && !hasActiveHighlight(frame);
        });
    }

    bool hoveringShowsHighlight(WKFrameInfo *frame, CGPoint point, CGRect highlightBoundingRect)
    {
        auto isShown = [&] {
            [webView waitForNextPresentationUpdate];
            return hasVisibleHighlightLayer(highlightBoundingRect) || hasActiveHighlight(frame);
        };
        for (unsigned i = 0; i < 5; ++i) {
            moveMouse(CGPointMake(point.x + (i % 2), point.y));
            if (isShown())
                return true;
        }
        Util::runFor(500_ms);
        return isShown();
    }

    bool twoMovesShowHighlight(WKFrameInfo *frame, CGRect dataDetectorRect, CGRect highlightBoundingRect)
    {
        moveMouse(center(dataDetectorRect));
        moveMouse(CGPointMake(CGRectGetMidX(dataDetectorRect) + 1, CGRectGetMidY(dataDetectorRect)));
        return Util::waitFor([&] {
            [webView waitForNextPresentationUpdate];
            return hasVisibleHighlightLayer(highlightBoundingRect) && hasActiveHighlight(frame);
        }, fiveSecondsOfTries);
    }

    struct Menu {
        CGRect elementBounds { CGRectNull };
        CGPoint location { CGPointZero };
    };

    std::optional<Menu> presentedMenu(NOESCAPE const Function<void()>& trigger, Seconds timeout)
    {
        __block bool didShowMenu { false };
        __block Menu menu;
        InstanceMethodSwizzler showContextMenuSwizzler {
            NSClassFromString(@"WKRevealItemPresenter"),
            NSSelectorFromString(@"showContextMenu"),
            imp_implementationWithBlock(^(NSObject *presenter) {
                menu.elementBounds = [checked_objc_cast<NSValue>([presenter valueForKey:@"frameInView"]) rectValue];
                menu.location = [checked_objc_cast<NSValue>([presenter valueForKey:@"menuLocationInView"]) pointValue];
                didShowMenu = true;
            })
        };

        trigger();
        if (!didShowMenu && timeout)
            Util::runFor(&didShowMenu, timeout);
        if (!didShowMenu)
            return std::nullopt;
        return menu;
    }

    std::optional<Menu> click(CGPoint pointInWebView, Seconds timeout = 5_s)
    {
        return presentedMenu([&] {
            [webView sendClickAtPoint:[webView convertPoint:pointInWebView toView:nil]];
        }, timeout);
    }

    void showHighlightAndMenu(WKFrameInfo *frame, CGRect dataDetectorRect, CGRect visibleRect)
    {
        auto highlight { dataDetectorHighlightGeometry(dataDetectorRect, visibleRect) };
        ASSERT_TRUE(hoverUntilHighlightIsActive(frame, dataDetectorRect, highlight.boundingRect));

        moveMouse(highlight.buttonCenter);
        auto menu { click(highlight.buttonCenter) };
        ASSERT_TRUE(menu.has_value());

        EXPECT_TRUE(rectsAreNear(menu->elementBounds, dataDetectorRect, liveTextDataDetectorRectTolerance)) << "Expected " << NSStringFromRect(dataDetectorRect).UTF8String << ", got " << NSStringFromRect(menu->elementBounds).UTF8String;
        EXPECT_NEAR(menu->location.x, highlight.buttonCenter.x, 1);
        EXPECT_NEAR(menu->location.y, highlight.buttonCenter.y, 1);
    }
};

TEST_P(LiveTextDataDetectorInSubframe, HighlightAndMenuAreAtTheDataDetector)
{
    load(@"/main");
    RetainPtr frame { [webView firstChildFrame] };
    CGRect rect { installImageOverlayWithDataDetector(frame.get(), iframeContentBox.origin) };
    showHighlightAndMenu(frame.get(), rect, rootVisibleRect(iframeContentBox));
}

TEST_P(LiveTextDataDetectorInSubframe, HighlightAndMenuAccountForMainFrameScroll)
{
    load(@"/main");

    static constexpr int scrollOffset = 40;
    EXPECT_TRUE(scrollTo(nil, scrollOffset));

    RetainPtr frame { [webView firstChildFrame] };
    CGRect contentBox { NSOffsetRect(iframeContentBox, 0, -scrollOffset) };
    CGRect rect { installImageOverlayWithDataDetector(frame.get(), contentBox.origin) };
    showHighlightAndMenu(frame.get(), rect, rootVisibleRect(contentBox));
}

TEST_P(LiveTextDataDetectorInSubframe, HighlightAndMenuAccountForSubframeScroll)
{
    load(@"/main-with-scrollable-iframe");
    RetainPtr frame { [webView firstChildFrame] };

    static constexpr int scrollOffset = 30;
    EXPECT_TRUE(scrollTo(frame.get(), scrollOffset));

    CGRect rect { installImageOverlayWithDataDetector(frame.get(), iframeContentBox.origin) };
    showHighlightAndMenu(frame.get(), rect, rootVisibleRect(iframeContentBox));
}

TEST_P(LiveTextDataDetectorInSubframe, HighlightAndMenuAreAtTheDataDetectorInNestedFrame)
{
    load(@"/main-with-nested-frame");
    RetainPtr frame { nestedFrame() };
    CGRect rect { installImageOverlayWithDataDetector(frame.get(), nestedFrameContentBox.origin) };
    showHighlightAndMenu(frame.get(), rect, rootVisibleRect(iframeContentBox));
}

TEST_P(LiveTextDataDetectorInSubframe, HighlightAndMenuInFrameNestedUnderCrossSiteFrame)
{
    load(@"/main-with-nested-main-site-frame");
    RetainPtr frame { nestedFrame() };
    CGRect rect { installImageOverlayWithDataDetector(frame.get(), nestedFrameContentBox.origin) };

    showHighlightAndMenu(frame.get(), rect, rootVisibleRect(nestedFrameContentBox));
    if (HasFatalFailure())
        return;

    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(nestedFrameContentBox)) };
    EXPECT_TRUE(moveAwayUntilHighlightIsHidden(frame.get(), center(otherLineRect(frame.get(), nestedFrameContentBox.origin)), highlight.boundingRect));

    CGPoint nestedFrameOrigin { nestedFrameContentBox.origin };
    CGRect highlightRelativeToMainFrame { CGRectOffset(highlight.boundingRect, -nestedFrameOrigin.x, -nestedFrameOrigin.y) };
    EXPECT_FALSE(hoveringShowsHighlight(frame.get(), CGPointMake(CGRectGetMidX(rect) - nestedFrameOrigin.x, CGRectGetMidY(rect) - nestedFrameOrigin.y), highlight.boundingRect));
    EXPECT_FALSE(hasVisibleHighlightLayer(highlightRelativeToMainFrame));

    ASSERT_TRUE(hoverUntilHighlightIsActive(frame.get(), rect, highlight.boundingRect));
    [webView objectByEvaluatingJavaScript:@"document.querySelector('iframe').remove()"];
    ASSERT_EQ([[webView objectByEvaluatingJavaScript:@"document.querySelectorAll('iframe').length"] intValue], 0);
    moveMouse(CGPointMake(50, 50));
    EXPECT_TRUE(Util::waitFor([&] {
        return !hasActiveHighlight(nil);
    }, fiveSecondsOfTries));
    bool wasShown { false };
    auto removalTime { MonotonicTime::now() };
    while (MonotonicTime::now() - removalTime < 500_ms) {
        [webView waitForNextPresentationUpdate];
        wasShown |= hasVisibleHighlightLayer(highlight.boundingRect) || hasVisibleHighlightLayer(highlightRelativeToMainFrame);
    }
    EXPECT_FALSE(wasShown);
}

TEST_P(LiveTextDataDetectorInSubframe, MouseOverAnotherFrameDoesNotShowHighlight)
{
    load(@"/main-with-two-iframes");
    RetainPtr frame { [webView firstChildFrame] };
    CGRect rect { installImageOverlayWithDataDetector(frame.get(), iframeContentBox.origin) };
    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(iframeContentBox)) };
    ASSERT_TRUE(hoverUntilHighlightIsActive(frame.get(), rect, highlight.boundingRect));
    EXPECT_TRUE(moveAwayUntilHighlightIsHidden(frame.get(), center(otherLineRect(frame.get(), iframeContentBox.origin)), highlight.boundingRect));

    EXPECT_FALSE(click(pointInSecondFrame(highlight.buttonCenter), 1_s).has_value());
    EXPECT_FALSE(hoveringShowsHighlight(frame.get(), pointInSecondFrame(center(rect)), highlight.boundingRect));
}

TEST_P(LiveTextDataDetectorInSubframe, HighlightGoesAwayWhenMouseMovesIntoMainFrame)
{
    load(@"/main");
    RetainPtr frame { [webView firstChildFrame] };
    CGRect rect { installImageOverlayWithDataDetector(frame.get(), iframeContentBox.origin) };
    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(iframeContentBox)) };
    ASSERT_TRUE(hoverUntilHighlightIsActive(frame.get(), rect, highlight.boundingRect));

    EXPECT_TRUE(moveAwayUntilHighlightIsHidden(frame.get(), CGPointMake(50, CGRectGetMidY(rect)), highlight.boundingRect));

    showHighlightAndMenu(frame.get(), rect, rootVisibleRect(iframeContentBox));
}

TEST_P(LiveTextDataDetectorInSubframe, HighlightShowsWhenMouseMovesFromHighlightIntoAnotherFrame)
{
    load(@"/main-with-two-image-iframes");
    RetainPtr frame { [webView firstChildFrame] };
    RetainPtr secondFrame { [webView secondChildFrame] };
    CGRect rect { installImageOverlayWithDataDetector(frame.get(), iframeContentBox.origin) };
    CGRect secondRect { installImageOverlayWithDataDetector(secondFrame.get(), secondIframeContentBox.origin) };
    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(iframeContentBox)) };
    ASSERT_TRUE(hoverUntilHighlightIsActive(frame.get(), rect, highlight.boundingRect));

    auto secondHighlight { dataDetectorHighlightGeometry(secondRect, rootVisibleRect(secondIframeContentBox)) };
    EXPECT_TRUE(twoMovesShowHighlight(secondFrame.get(), secondRect, secondHighlight.boundingRect));
}

TEST_P(LiveTextDataDetectorInSubframe, HighlightShowsWhenMouseMovesFromHighlightIntoMainFrame)
{
    load(@"/main-with-nested-main-site-frame-and-image");
    RetainPtr frame { nestedFrame() };
    CGRect rect { installImageOverlayWithDataDetector(frame.get(), nestedFrameContentBox.origin) };
    CGRect mainFrameRect { installImageOverlayWithDataDetector(nil, CGPointZero) };
    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(nestedFrameContentBox)) };
    ASSERT_TRUE(hoverUntilHighlightIsActive(frame.get(), rect, highlight.boundingRect));

    auto mainFrameHighlight { dataDetectorHighlightGeometry(mainFrameRect, [webView bounds]) };
    EXPECT_TRUE(twoMovesShowHighlight(nil, mainFrameRect, mainFrameHighlight.boundingRect));
}

INSTANTIATE_TEST_SUITE_P(ImageAnalysisTests, LiveTextDataDetectorInSubframe, testing::ValuesIn(siteIsolationParams), &LiveTextDataDetectorInSubframe::testNameGenerator);

#if ENABLE(IPC_TESTING_API)

class LiveTextDataDetectorMenuRequestFromSubframe : public LiveTextDataDetectorInSubframe {
public:
    void adjustConfiguration(WKWebViewConfiguration *configuration) override
    {
        setFeatureEnabled(configuration, @"IPCTestingAPIEnabled", true);
        setFeatureEnabled(configuration, @"IgnoreInvalidMessageWhenIPCTestingAPIEnabled", true);
    }
};

TEST_P(LiveTextDataDetectorMenuRequestFromSubframe, ReplayShowsMenuOnlyFromTheImageFrameProcess)
{
    load(@"/main");
    RetainPtr frame { [webView firstChildFrame] };
    ASSERT_NE(frame.get()._processIdentifier, [webView _webProcessIdentifier]);
    CGRect rect { installImageOverlayWithDataDetector(frame.get(), iframeContentBox.origin) };

    [webView objectByEvaluatingJavaScript:@"window.capturedRequest = null;"
        "IPC.addOutgoingMessageListener('UI', message => {"
        "  if (!window.capturedRequest && message.name === IPC.messages.WebPageProxy_HandleClickForDataDetectionResult.name)"
        "    window.capturedRequest = new Uint8Array(message.buffer).slice(16);"
        "}); true" inFrame:frame.get()];
    showHighlightAndMenu(frame.get(), rect, rootVisibleRect(iframeContentBox));
    if (HasFatalFailure())
        return;
    ASSERT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"!!window.capturedRequest" inFrame:frame.get()] boolValue];
    }));

    RetainPtr<NSArray> bytes { [webView objectByEvaluatingJavaScript:@"Array.from(window.capturedRequest)" inFrame:frame.get()] };
    RetainPtr bytesJSON { adoptNS([[NSString alloc] initWithData:[NSJSONSerialization dataWithJSONObject:bytes.get() options:0 error:nil] encoding:NSUTF8StringEncoding]) };
    auto replay = [&](WKFrameInfo *sender) -> RetainPtr<NSString> {
        return [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"(() => {"
            "  IPC.sendMessage('UI', IPC.pageID, IPC.messages.WebPageProxy_HandleClickForDataDetectionResult.name, new Uint8Array(%@));"
            "  const reply = IPC.sendSyncMessage('UI', 0, IPC.messages.WebProcessProxy_TakeInvalidMessageStringForTesting.name, 1000, []);"
            "  return reply ? (reply.arguments && reply.arguments[0] ? reply.arguments[0].value : '') : '<no reply>';"
            "})()", bytesJSON.get()] inFrame:sender];
    };

    RetainPtr<NSString> invalidMessage;
    EXPECT_FALSE(presentedMenu([&] {
        invalidMessage = replay(nil);
    }, 0_s).has_value());
    EXPECT_WK_STREQ(@"", invalidMessage.get());

    auto menu { presentedMenu([&] {
        invalidMessage = replay(frame.get());
    }, 0_s) };
    ASSERT_TRUE(menu.has_value());
    EXPECT_WK_STREQ(@"", invalidMessage.get());
    EXPECT_TRUE(rectsAreNear(menu->elementBounds, rect, liveTextDataDetectorRectTolerance)) << NSStringFromRect(menu->elementBounds).UTF8String;
}

INSTANTIATE_TEST_SUITE_P(ImageAnalysisTests, LiveTextDataDetectorMenuRequestFromSubframe, testing::ValuesIn(crossOriginWithSiteIsolationParams), &LiveTextDataDetectorMenuRequestFromSubframe::testNameGenerator);

#endif // ENABLE(IPC_TESTING_API)

#endif // PLATFORM(MAC) && ENABLE(DATA_DETECTION) && ENABLE(REVEAL)

#if ENABLE(SERVICE_CONTROLS)

static void clickImageControlsAndWaitForRemoveBackgroundItem(TestWKWebView *webView)
{
    [[webView window] orderFrontRegardless];
    [webView callAsyncJavaScript:@"waitForAndClickImageControls()" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];

    __block bool foundRemoveBackgroundItem = false;
    RetainPtr timer = [NSTimer timerWithTimeInterval:0.1 repeats:YES block:^(NSTimer *) {
        NSMenu *menu = [webView _activeMenu];
        for (NSMenuItem *item in menu.itemArray) {
            if ([item.title isEqualToString:WebCore::contextMenuItemTitleRemoveBackground().createNSString().get()]) {
                foundRemoveBackgroundItem = true;
                [menu cancelTracking];
                break;
            }
        }
    }];

    [NSRunLoop.mainRunLoop addTimer:timer.get() forMode:NSEventTrackingRunLoopMode];
    Util::run(&foundRemoveBackgroundItem);
    [timer invalidate];
}

TEST(ImageAnalysisTests, RemoveBackgroundItemInServicesMenu)
{
    RemoveBackgroundSwizzler swizzler { iconImage().autorelease(), CGRectMake(10, 10, 215, 174) };

    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView _setEditable:YES];
    [webView synchronouslyLoadTestPageNamed:@"image-controls"];
    clickImageControlsAndWaitForRemoveBackgroundItem(webView.get());
}

TEST(ImageAnalysisTests, ServicesMenuImageForSVGWithoutNaturalSize)
{
    RemoveBackgroundSwizzler swizzler { iconImage().autorelease(), CGRectMake(10, 10, 215, 174) };

    auto webView = createWebViewWithTextRecognitionEnhancements();
    [webView _setEditable:YES];
    [webView synchronouslyLoadHTMLString:@"<script>"
        "addEventListener('load', () => HTMLAttachmentElement.getAttachmentIdentifier(document.querySelector('img')));"
        "async function waitForAndClickImageControls() {"
        "    let button = null;"
        "    do {"
        "        await new Promise(requestAnimationFrame);"
        "        button = internals.shadowRoot(document.querySelector('img')).getElementById('image-controls-button');"
        "    } while (!button);"
        "    button.click();"
        "}"
        "</script>"
        "<img style='width: 200px; height: 100px' src=\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 2 1'><rect width='2' height='1' fill='green'/></svg>\">"];
    clickImageControlsAndWaitForRemoveBackgroundItem(webView.get());

    auto size = swizzler.lastRequestImageSize();
    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(size->width, 200);
    EXPECT_EQ(size->height, 100);
}

#endif // ENABLE(SERVICE_CONTROLS)

} // namespace TestWebKitAPI

#endif // ENABLE(IMAGE_ANALYSIS)
