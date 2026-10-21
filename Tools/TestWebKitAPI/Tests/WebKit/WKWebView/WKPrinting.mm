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
#import "WKPrinting.h"

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/CGImagePixelReader.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/PDFTestHelpers.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/Utilities.h"
#import <WebCore/Color.h>
#import <WebKit/WKData.h>
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKPagePrivate.h>
#import <WebKit/WKUIDelegate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferences.h>
#import <WebKit/_WKFrameHandle.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <numbers>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/darwin/DispatchExtras.h>

typedef void (^CallCompletionBlock)();

@interface PrintWithSimulatedPageComputationUIDelegate : NSObject <WKUIDelegate>

- (void)waitForPagination;

@end

@implementation PrintWithSimulatedPageComputationUIDelegate {
    bool _isDone;
}

- (void)callBlockAsync:(CallCompletionBlock)callCompletionBlock
{
    dispatch_async(mainDispatchQueueSingleton(), ^{
        callCompletionBlock();
    });
}

- (void)_webView:(WKWebView *)webView printFrame:(_WKFrameHandle *)frame pdfFirstPageSize:(CGSize)size completionHandler:(void (^)(void))completionHandler
{
    _isDone = false;
    CallCompletionBlock callCompletionBlock = ^{
        [webView _computePagesForPrinting:frame completionHandler:^{
            _isDone = true;
            completionHandler();
        }];
    };

    // Dispatch the completion handler asynchronously to ensure we don't block IPC in the web process in the unbounded sync IPC case.
    [self callBlockAsync:callCompletionBlock];
}

- (void)waitForPagination
{
    TestWebKitAPI::Util::run(&_isDone);
}

@end

@implementation DeferredPrintUIDelegate {
    RetainPtr<_WKFrameHandle> _frame;
    BlockPtr<void()> _completionHandler;
}

- (void)_webView:(WKWebView *)webView printFrame:(_WKFrameHandle *)frame pdfFirstPageSize:(CGSize)size completionHandler:(void (^)(void))completionHandler
{
    _frame = frame;
    _completionHandler = makeBlockPtr(completionHandler);
}

- (_WKFrameHandle *)waitForPrintFrame
{
    while (!_frame)
        TestWebKitAPI::Util::spinRunLoop();
    return _frame.get();
}

- (void)finishPrinting
{
    std::exchange(_completionHandler, nullptr)();
    _frame = nil;
}

@end

TEST(Printing, PrintWithDelayedCompletion)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr delegate = adoptNS([PrintWithSimulatedPageComputationUIDelegate new]);
    [webView setUIDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.print()" completionHandler:nil];
    [delegate waitForPagination];
}

TEST(Printing, DrawPagesToPDFSynchronously)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@"<body>Hello</body>"];

    RetainPtr<_WKFrameHandle> mainFrame = [webView mainFrame].info._handle;
    __block bool computedPages = false;
    [webView _computePagesForPrinting:mainFrame.get() completionHandler:^{
        computedPages = true;
    }];
    TestWebKitAPI::Util::run(&computedPages);

    RetainPtr data = [webView _drawPagesToPDFSynchronouslyForTesting:mainFrame.get() pageCount:1];
    ASSERT_NOT_NULL(data.get());
    EXPECT_TRUE([data length]);
}

TEST(Printing, DrawPagesToPDFSynchronouslyDuringDOMPrint)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    RetainPtr delegate = adoptNS([DeferredPrintUIDelegate new]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<body>Hello</body>"];

    [webView evaluateJavaScript:@"window.print()" completionHandler:nil];
    RetainPtr<_WKFrameHandle> frame = [delegate waitForPrintFrame];

    __block bool computedPages = false;
    [webView _computePagesForPrinting:frame.get() completionHandler:^{
        computedPages = true;
    }];
    TestWebKitAPI::Util::run(&computedPages);

    RetainPtr data = [webView _drawPagesToPDFSynchronouslyForTesting:frame.get() pageCount:1];
    [delegate finishPrinting];
    ASSERT_NOT_NULL(data.get());
    EXPECT_TRUE([data length]);
}

#if PLATFORM(MAC)
@interface WKPrintPageBordersWebView : TestWKWebView
@end

@implementation WKPrintPageBordersWebView {
    bool _didDrawPageBorder;
}

- (void)drawPageBorderWithSize:(NSSize)borderSize
{
    _didDrawPageBorder = true;
}

- (void)_waitUntilPageBorderDrawn
{
    TestWebKitAPI::Util::run(&_didDrawPageBorder);
}

@end

@interface PrintShowingPrintPanelUIDelegate : NSObject <WKUIDelegate>

@end

@implementation PrintShowingPrintPanelUIDelegate

- (void)_webView:(WKWebView *)webView printFrame:(_WKFrameHandle *)frame pdfFirstPageSize:(CGSize)size completionHandler:(void (^)(void))completionHandler
{
    RetainPtr printInfo = adoptNS([[NSPrintInfo alloc] init]);

    NSPrintOperation *operation = [webView _printOperationWithPrintInfo:printInfo.get() forFrame:frame];

    operation.showsPrintPanel = YES;
    NSPrintPanel *printPanel = operation.printPanel;
    printPanel.options = printPanel.options | NSPrintPanelShowsPaperSize | NSPrintPanelShowsOrientation | NSPrintPanelShowsScaling | NSPrintPanelShowsPreview;

    [operation runOperationModalForWindow:webView.window delegate:nil didRunSelector:nil contextInfo:nil];

    if (completionHandler)
        completionHandler();
}

@end

TEST(Printing, PrintPageBorders)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    RetainPtr webView = adoptNS([[WKPrintPageBordersWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr delegate = adoptNS([PrintShowingPrintPanelUIDelegate new]);
    [webView setUIDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.print()" completionHandler:nil];
    [webView _waitUntilPageBorderDrawn];
}

@implementation TestPDFPrintDelegate {
    NSUInteger _printFrameCallCount;
}

- (void)_webView:(WKWebView *)webView printFrame:(_WKFrameHandle *)frame pdfFirstPageSize:(CGSize)size completionHandler:(void (^)(void))completionHandler
{
    completionHandler();
    ++_printFrameCallCount;
}

- (void)waitForPrintFrameCall
{
    while (!_printFrameCallCount)
        TestWebKitAPI::Util::spinRunLoop();
}

- (NSUInteger)printFrameCallCount
{
    return _printFrameCallCount;
}

@end

using namespace TestWebKitAPI;

NSURLRequest *PrintWithJSExecutionOptionTests::namedPDFRequest(NSString *resourceName)
{
    return [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:resourceName withExtension:@"pdf"]];
}

NSURLRequest *PrintWithJSExecutionOptionTests::pdfRequest()
{
    return namedPDFRequest(@"test_print");
}

NSURLRequest *PrintWithJSExecutionOptionTests::openActionPDFRequest()
{
    return namedPDFRequest(@"test_print_openaction");
}

std::string PrintWithJSExecutionOptionTests::testNameGenerator(testing::TestParamInfo<bool> info)
{
    return std::string { "allowsContentJavascript_is_" } + (info.param ? "true" : "false");
}

void PrintWithJSExecutionOptionTests::runTest(WKWebView *webView, NSURLRequest *request)
{
    RetainPtr delegate = adoptNS([TestPDFPrintDelegate new]);
    [webView setUIDelegate:delegate];

    RetainPtr preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [preferences setAllowsContentJavaScript:allowsContentJavascript()];

    [webView synchronouslyLoadRequest:request preferences:preferences];

    [delegate waitForPrintFrameCall];
}

void PrintWithJSExecutionOptionTests::runNonPrintingOpenActionTest(WKWebView *webView)
{
    RetainPtr delegate = adoptNS([TestPDFPrintDelegate new]);
    [webView setUIDelegate:delegate];

    RetainPtr preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [preferences setAllowsContentJavaScript:allowsContentJavascript()];

    for (NSString *resourceName in @[ @"test_openaction_destination", @"test_openaction_goto", @"test_openaction_nonprint" ])
        [webView synchronouslyLoadRequest:namedPDFRequest(resourceName) preferences:preferences];

    [webView synchronouslyLoadRequest:openActionPDFRequest() preferences:preferences];
    [delegate waitForPrintFrameCall];

    EXPECT_EQ([delegate printFrameCallCount], 1u);
}

TEST_P(PrintWithJSExecutionOptionTests, PDFWithWindowPrintEmbeddedJS)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    runTest(webView, pdfRequest());
}

TEST_P(PrintWithJSExecutionOptionTests, PDFWithOpenActionPrintEmbeddedJS)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    runTest(webView, openActionPDFRequest());
}

TEST_P(PrintWithJSExecutionOptionTests, PDFWithNonPrintingOpenActionDoesNotPrint)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    runNonPrintingOpenActionTest(webView);
}

INSTANTIATE_TEST_SUITE_P(Printing, PrintWithJSExecutionOptionTests, testing::Bool(), &TestWebKitAPI::PrintWithJSExecutionOptionTests::testNameGenerator);

#if ENABLE(UNIFIED_PDF) && WK_HAVE_C_SPI

namespace TestWebKitAPI {

static constexpr WKPrintInfo letterPrintInfo { 1, 612, 792 };

static RetainPtr<WKWebViewConfiguration> configurationForPrintingUnifiedPDF(bool remoteSnapshottingEnabled)
{
    RetainPtr configuration = configurationForWebViewTestingUnifiedPDF();
    setFeatureEnabled(configuration.get(), @"RemoteSnapshottingEnabled", remoteSnapshottingEnabled);
    setFeatureEnabled(configuration.get(), @"RemoteSnapshottingUnifiedPDFVectorDrawingEnabled", true);
    return configuration;
}

// Prints the main frame like WKPrintingView does: compute the page rects, then ask for the pages as PDF data.
static RetainPtr<NSData> printMainFrameToPDF(WKWebView *webView)
{
    WKPageRef page = [webView _pageRefForTransitionToWKWebView];
    WKFrameRef mainFrame = WKPageGetMainFrame(page);

    struct PrintState {
        bool computedPages { false };
        uint32_t pageCount { 0 };
        bool done { false };
        RetainPtr<NSData> data;
    } state;

    WKPageComputePagesForPrinting(page, mainFrame, letterPrintInfo, [](WKRect*, uint32_t pageCount, double, WKErrorRef, void* context) {
        auto& state = *static_cast<PrintState*>(context);
        state.pageCount = pageCount;
        state.computedPages = true;
    }, &state);
    Util::run(&state.computedPages);
    EXPECT_GT(state.pageCount, 0u);

    WKPageDrawPagesToPDF(page, mainFrame, letterPrintInfo, 0, state.pageCount, [](WKDataRef data, WKErrorRef, void* context) {
        auto& state = *static_cast<PrintState*>(context);
        if (data)
            state.data = adoptNS([[NSData alloc] initWithBytes:WKDataGetBytes(data) length:WKDataGetSize(data)]);
        state.done = true;
    }, &state);
    Util::run(&state.done);

    WKPageEndPrinting(page);
    return state.data;
}

static RetainPtr<CGPDFDocumentRef> createPDFDocument(NSData *pdfData)
{
    RetainPtr provider = adoptCF(CGDataProviderCreateWithCFData(bridge_cast(pdfData)));
    return adoptCF(CGPDFDocumentCreateWithProvider(provider.get()));
}

static RetainPtr<CGImageRef> renderPrintedPage(NSData *pdfData, size_t pageNumber, CGFloat scale = 1)
{
    RetainPtr document = createPDFDocument(pdfData);
    RetainPtr page = CGPDFDocumentGetPage(document.get(), pageNumber);
    if (!page)
        return nullptr;

    auto mediaBox = CGPDFPageGetBoxRect(page.get(), kCGPDFMediaBox);
    RetainPtr colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    RetainPtr context = adoptCF(CGBitmapContextCreate(nullptr, mediaBox.size.width * scale, mediaBox.size.height * scale, 8, 0, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
    CGContextSetRGBFillColor(context.get(), 1, 1, 1, 1);
    CGContextFillRect(context.get(), CGRectMake(0, 0, mediaBox.size.width * scale, mediaBox.size.height * scale));
    CGContextScaleCTM(context.get(), scale, scale);
    CGContextTranslateCTM(context.get(), -mediaBox.origin.x, -mediaBox.origin.y);
    CGContextDrawPDFPage(context.get(), page.get());
    return adoptCF(CGBitmapContextCreateImage(context.get()));
}

static bool colorsAreClose(const WebCore::Color& a, const WebCore::Color& b, int tolerance)
{
    auto [r1, g1, b1, a1] = a.toColorTypeLossy<WebCore::SRGBA<uint8_t>>().resolved();
    auto [r2, g2, b2, a2] = b.toColorTypeLossy<WebCore::SRGBA<uint8_t>>().resolved();
    return std::abs(r1 - r2) <= tolerance && std::abs(g1 - g2) <= tolerance && std::abs(b1 - b2) <= tolerance && std::abs(a1 - a2) <= tolerance;
}

static size_t countPixelsCloseTo(CGImageRef image, const WebCore::Color& color)
{
    CGImagePixelReader reader { image };
    size_t count = 0;
    for (unsigned y = 0; y < reader.height(); ++y) {
        for (unsigned x = 0; x < reader.width(); ++x) {
            if (colorsAreClose(reader.at(x, y), color, 24))
                ++count;
        }
    }
    return count;
}

static size_t countDifferentPixels(CGImageRef image, CGImageRef expectedImage)
{
    CGImagePixelReader reader { image };
    CGImagePixelReader expectedReader { expectedImage };
    EXPECT_EQ(reader.width(), expectedReader.width());
    EXPECT_EQ(reader.height(), expectedReader.height());
    if (reader.width() != expectedReader.width() || reader.height() != expectedReader.height())
        return std::numeric_limits<size_t>::max();

    size_t count = 0;
    for (unsigned y = 0; y < reader.height(); ++y) {
        for (unsigned x = 0; x < reader.width(); ++x) {
            if (!colorsAreClose(reader.at(x, y), expectedReader.at(x, y), 32))
                ++count;
        }
    }
    return count;
}

struct PDFResourceCounts {
    size_t images { 0 };
    size_t imageMasks { 0 };
    CGPDFInteger maximumImageWidth { 0 };
    CGPDFInteger maximumImageBitsPerComponent { 0 };
    size_t tilingPatterns { 0 };
};

// Includes the resources of nested form XObjects and tiling patterns.
static void countPDFResources(CGPDFDictionaryRef resources, PDFResourceCounts& counts, unsigned depth = 0)
{
    if (!resources || depth > 16)
        return;

    auto countStreamResources = [&](CGPDFStreamRef stream) {
        CGPDFDictionaryRef streamResources = nullptr;
        if (CGPDFDictionaryGetDictionary(CGPDFStreamGetDictionary(stream), "Resources", &streamResources))
            countPDFResources(streamResources, counts, depth + 1);
    };

    CGPDFDictionaryRef xObjects = nullptr;
    if (CGPDFDictionaryGetDictionary(resources, "XObject", &xObjects)) {
        CGPDFDictionaryApplyBlock(xObjects, ^bool(const char*, CGPDFObjectRef object, void*) {
            CGPDFStreamRef stream = nullptr;
            const char* subtype = nullptr;
            if (!CGPDFObjectGetValue(object, kCGPDFObjectTypeStream, &stream) || !CGPDFDictionaryGetName(CGPDFStreamGetDictionary(stream), "Subtype", &subtype))
                return true;
            if (String::fromLatin1(subtype) == "Image"_s) {
                ++counts.images;
                CGPDFInteger width = 0;
                if (CGPDFDictionaryGetInteger(CGPDFStreamGetDictionary(stream), "Width", &width))
                    counts.maximumImageWidth = std::max(counts.maximumImageWidth, width);
                CGPDFInteger bitsPerComponent = 0;
                if (CGPDFDictionaryGetInteger(CGPDFStreamGetDictionary(stream), "BitsPerComponent", &bitsPerComponent))
                    counts.maximumImageBitsPerComponent = std::max(counts.maximumImageBitsPerComponent, bitsPerComponent);
                CGPDFBoolean isImageMask = false;
                if (CGPDFDictionaryGetBoolean(CGPDFStreamGetDictionary(stream), "ImageMask", &isImageMask) && isImageMask)
                    ++counts.imageMasks;
            } else if (String::fromLatin1(subtype) == "Form"_s)
                countStreamResources(stream);
            return true;
        }, nullptr);
    }

    CGPDFDictionaryRef patterns = nullptr;
    if (CGPDFDictionaryGetDictionary(resources, "Pattern", &patterns)) {
        CGPDFDictionaryApplyBlock(patterns, ^bool(const char*, CGPDFObjectRef object, void*) {
            // Tiling patterns are streams, shading patterns are dictionaries.
            CGPDFStreamRef stream = nullptr;
            if (!CGPDFObjectGetValue(object, kCGPDFObjectTypeStream, &stream))
                return true;
            ++counts.tilingPatterns;
            countStreamResources(stream);
            return true;
        }, nullptr);
    }
}

static PDFResourceCounts countPDFResourcesOfPage(NSData *pdfData, size_t pageNumber)
{
    PDFResourceCounts counts;
    RetainPtr document = createPDFDocument(pdfData);
    CGPDFDictionaryRef resources = nullptr;
    if (CGPDFDictionaryGetDictionary(CGPDFPageGetDictionary(CGPDFDocumentGetPage(document.get(), pageNumber)), "Resources", &resources))
        countPDFResources(resources, counts);
    return counts;
}

static PDFResourceCounts countPDFResourcesOfFirstPage(NSData *pdfData)
{
    return countPDFResourcesOfPage(pdfData, 1);
}

static void waitForRequests(const HTTPServer& server, size_t count)
{
    Util::waitFor([&] {
        return server.totalRequests() >= count;
    });
}

// Loads `mainFrameHTML` from https://a.com in a site isolated web view, and waits until the server
// has served `requestCount` requests and the result has painted.
static RetainPtr<TestWKWebView> loadPageForPrinting(NSString *mainFrameHTML, HTTPServer& server, size_t requestCount, bool remoteSnapshottingEnabled)
{
    RetainPtr configuration = configurationForPrintingUnifiedPDF(remoteSnapshottingEnabled);
    [configuration setWebsiteDataStore:[server.httpsProxyConfiguration() websiteDataStore]];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));
    server.addResponse("/mainframe"_s, { String(mainFrameHTML) });
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    waitForRequests(server, requestCount);
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];
    return webView;
}

static RetainPtr<NSData> createPDFData(void (^draw)(CGContextRef))
{
    RetainPtr data = adoptNS([[NSMutableData alloc] init]);
    RetainPtr consumer = adoptCF(CGDataConsumerCreateWithCFData(bridge_cast(data.get())));
    CGRect mediaBox = CGRectMake(0, 0, 612, 792);
    RetainPtr context = adoptCF(CGPDFContextCreate(consumer.get(), &mediaBox, nullptr));
    CGPDFContextBeginPage(context.get(), nullptr);
    draw(context.get());
    CGPDFContextEndPage(context.get());
    CGPDFContextClose(context.get());
    return data;
}

static RetainPtr<NSData> createPDFDataWithObjects(NSArray<NSString *> *objects)
{
    RetainPtr pdf = [NSMutableString stringWithString:@"%PDF-1.4\n"];
    RetainPtr offsets = [NSMutableArray array];
    for (NSUInteger i = 0; i < objects.count; ++i) {
        [offsets addObject:@([pdf length])];
        [pdf appendFormat:@"%lu 0 obj\n%@\nendobj\n", static_cast<unsigned long>(i + 1), objects[i]];
    }
    NSUInteger xrefOffset = [pdf length];
    [pdf appendFormat:@"xref\n0 %lu\n0000000000 65535 f \n", static_cast<unsigned long>(objects.count + 1)];
    for (NSNumber *offset in offsets.get())
        [pdf appendFormat:@"%010lu 00000 n \n", offset.unsignedLongValue];
    [pdf appendFormat:@"trailer\n<< /Size %lu /Root 1 0 R >>\nstartxref\n%lu\n%%%%EOF\n", static_cast<unsigned long>(objects.count + 1), static_cast<unsigned long>(xrefOffset)];
    return [pdf dataUsingEncoding:NSASCIIStringEncoding];
}

static WebCore::Color solidPDFColor()
{
    return WebCore::SRGBA<uint8_t> { 1, 113, 0 };
}

static RetainPtr<NSData> createPDFDataWithSolidFill()
{
    return createPDFData(^(CGContextRef context) {
        auto [red, green, blue, alpha] = solidPDFColor().toColorTypeLossy<WebCore::SRGBA<float>>().resolved();
        CGContextSetRGBFillColor(context, red, green, blue, alpha);
        CGContextFillRect(context, CGRectMake(0, 0, 612, 792));
    });
}

static RetainPtr<NSData> printDocumentWithEmbeddedPDFs(bool remoteSnapshottingEnabled)
{
    RetainPtr pdfData = createPDFDataWithSolidFill();
    HTTPServer server({
        { "/document.pdf"_s, { { { "Content-Type"_s, "application/pdf"_s } }, pdfData.get() } },
    }, HTTPServer::Protocol::HttpsProxy);
    RetainPtr webView = loadPageForPrinting(@"<body style='margin: 0'>"
        "<iframe src='https://b.com/document.pdf' style='display: block; width: 300px; height: 300px; border: none'></iframe>"
        "<embed src='/document.pdf' type='application/pdf' style='display: block; width: 300px; height: 300px; break-before: page'>"
        "</body>", server, 3, remoteSnapshottingEnabled);
    return printMainFrameToPDF(webView.get());
}

UNIFIED_PDF_TEST(PrintEmbeddedPDFWithRemoteSnapshotting)
{
    RetainPtr localPDFData = printDocumentWithEmbeddedPDFs(false);
    RetainPtr remotePDFData = printDocumentWithEmbeddedPDFs(true);
    ASSERT_GT([localPDFData length], 0u);
    ASSERT_GT([remotePDFData length], 0u);

    RetainPtr localDocument = createPDFDocument(localPDFData.get());
    RetainPtr remoteDocument = createPDFDocument(remotePDFData.get());
    EXPECT_EQ(CGPDFDocumentGetNumberOfPages(localDocument.get()), 2u);
    EXPECT_EQ(CGPDFDocumentGetNumberOfPages(remoteDocument.get()), CGPDFDocumentGetNumberOfPages(localDocument.get()));

    // Printing without remote snapshotting can't draw the cross-origin iframe on the first page.
    RetainPtr localFirstPage = renderPrintedPage(localPDFData.get(), 1);
    RetainPtr remoteFirstPage = renderPrintedPage(remotePDFData.get(), 1);
    ASSERT_TRUE(localFirstPage && remoteFirstPage);
    EXPECT_EQ(countPixelsCloseTo(localFirstPage.get(), solidPDFColor()), 0u);
    EXPECT_GT(countPixelsCloseTo(remoteFirstPage.get(), solidPDFColor()), 10000u);

    RetainPtr localSecondPage = renderPrintedPage(localPDFData.get(), 2);
    RetainPtr remoteSecondPage = renderPrintedPage(remotePDFData.get(), 2);
    ASSERT_TRUE(localSecondPage && remoteSecondPage);
    EXPECT_GT(countPixelsCloseTo(localSecondPage.get(), solidPDFColor()), 10000u);
    EXPECT_GT(countPixelsCloseTo(remoteSecondPage.get(), solidPDFColor()), 10000u);
    size_t pageArea = CGImageGetWidth(localSecondPage.get()) * CGImageGetHeight(localSecondPage.get());
    EXPECT_LT(countDifferentPixels(remoteSecondPage.get(), localSecondPage.get()), pageArea / 100);

    // The PDF content is drawn as vectors, not as an image of the page.
    EXPECT_EQ(countPDFResourcesOfPage(remotePDFData.get(), 1).images, 0u);
    EXPECT_EQ(countPDFResourcesOfPage(remotePDFData.get(), 2).images, 0u);
}

// Remote snapshotting is only used when the page has a cross-origin iframe.
static RetainPtr<NSData> printEmbeddedPDFData(NSData *pdfData, bool remoteSnapshottingEnabled, bool withRemoteFrame = true)
{
    HTTPServer server({
        { "/document.pdf"_s, { { { "Content-Type"_s, "application/pdf"_s } }, pdfData } },
        { "/subframe"_s, { "<body></body>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    RetainPtr iframe = withRemoteFrame ? @"<iframe src='https://b.com/subframe' style='display: block; width: 10px; height: 10px; border: none'></iframe>" : @"";
    RetainPtr webView = loadPageForPrinting([NSString stringWithFormat:@"<body style='margin: 0'><embed src='/document.pdf' type='application/pdf' style='display: block; width: 400px; height: 518px'>%@</body>", iframe.get()], server, withRemoteFrame ? 3 : 2, remoteSnapshottingEnabled);
    return printMainFrameToPDF(webView.get());
}

static RetainPtr<NSData> createPDFDataWithPathsAndStrokes()
{
    return createPDFData(^(CGContextRef context) {
        CGContextSetRGBFillColor(context, 0, 0.5, 1, 1);
        CGContextFillRect(context, CGRectMake(72, 600, 200, 120));

        CGContextSetRGBStrokeColor(context, 0.8, 0.1, 0.1, 1);
        CGContextSetLineWidth(context, 8);
        CGContextSetLineJoin(context, kCGLineJoinRound);
        CGContextStrokeRect(context, CGRectMake(340, 600, 200, 120));

        CGContextSetLineWidth(context, 6);
        CGContextSetLineCap(context, kCGLineCapRound);
        std::array<CGFloat, 2> dashes { 20, 12 };
        CGContextSetLineDash(context, 4, dashes.data(), dashes.size());
        std::array<CGPoint, 4> segments { CGPointMake(72, 560), CGPointMake(540, 560), CGPointMake(72, 530), CGPointMake(540, 530) };
        CGContextStrokeLineSegments(context, segments.data(), segments.size());
        CGContextSetLineDash(context, 0, nullptr, 0);
        CGContextSetLineCap(context, kCGLineCapButt);

        CGContextSaveGState(context);
        CGContextTranslateCTM(context, 200, 380);
        CGContextRotateCTM(context, std::numbers::pi / 6);
        CGContextScaleCTM(context, 1, 0.5);
        CGContextSetLineJoin(context, kCGLineJoinMiter);
        CGContextSetMiterLimit(context, 10);
        CGContextSetRGBStrokeColor(context, 0, 0, 0, 1);
        CGContextSetRGBFillColor(context, 1, 0.8, 0, 1);
        CGContextMoveToPoint(context, 0, 100);
        for (unsigned i = 1; i < 5; ++i) {
            double angle = std::numbers::pi / 2 + i * 4 * std::numbers::pi / 5;
            CGContextAddLineToPoint(context, 100 * std::cos(angle), 100 * std::sin(angle));
        }
        CGContextClosePath(context);
        CGContextDrawPath(context, kCGPathEOFillStroke);
        CGContextRestoreGState(context);

        CGContextSetRGBStrokeColor(context, 0, 0.6, 0.2, 1);
        CGContextSetLineWidth(context, 10);
        CGContextAddArc(context, 440, 380, 70, 0, 3 * std::numbers::pi / 2, false);
        CGContextStrokePath(context);

        CGContextSaveGState(context);
        CGContextAddEllipseInRect(context, CGRectMake(72, 72, 240, 160));
        CGContextClip(context);
        CGContextSetRGBFillColor(context, 0.5, 0, 0.5, 1);
        CGContextFillRect(context, CGRectMake(72, 72, 240, 160));
        CGContextRestoreGState(context);
    });
}

UNIFIED_PDF_TEST(PrintEmbeddedPDFPathsAndStrokesWithRemoteSnapshotting)
{
    RetainPtr sourcePDFData = createPDFDataWithPathsAndStrokes();
    RetainPtr localPDFData = printEmbeddedPDFData(sourcePDFData.get(), false);
    RetainPtr remotePDFData = printEmbeddedPDFData(sourcePDFData.get(), true);
    ASSERT_GT([localPDFData length], 0u);
    ASSERT_GT([remotePDFData length], 0u);

    EXPECT_EQ(countPDFResourcesOfFirstPage(remotePDFData.get()).images, 0u);

    static constexpr CGFloat scale = 2;
    RetainPtr localPage = renderPrintedPage(localPDFData.get(), 1, scale);
    RetainPtr remotePage = renderPrintedPage(remotePDFData.get(), 1, scale);
    ASSERT_TRUE(localPage && remotePage);
    EXPECT_GT(countPixelsCloseTo(localPage.get(), WebCore::Color::black), 100u);
    size_t pageArea = CGImageGetWidth(localPage.get()) * CGImageGetHeight(localPage.get());
    EXPECT_LT(countDifferentPixels(remotePage.get(), localPage.get()), pageArea / 500);
}

// A page with a Text annotation and its open Popup annotation, which PDFKit draws on screen but not when printing.
static RetainPtr<NSData> createPDFDataWithOpenPopupAnnotation()
{
    return createPDFDataWithObjects(@[
        @"<< /Type /Catalog /Pages 2 0 R >>",
        @"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        @"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R] >>",
        @"<< /Type /Annot /Subtype /Text /Rect [100 650 124 674] /Contents (Note) /Name /Comment /C [1 1 0] /Popup 5 0 R /F 28 >>",
        @"<< /Type /Annot /Subtype /Popup /Parent 4 0 R /Rect [200 500 450 650] /Open true /F 28 >>",
    ]);
}

static size_t countYellowPixelsInPopup(CGImageRef image)
{
    CGImagePixelReader reader { image };
    size_t count = 0;
    for (unsigned y = 792 - 650; y < 792 - 500 && y < reader.height(); ++y) {
        for (unsigned x = 200; x < 450 && x < reader.width(); ++x) {
            auto [red, green, blue, alpha] = reader.at(x, y).toColorTypeLossy<WebCore::SRGBA<uint8_t>>().resolved();
            if (red > 200 && green > 200 && blue < 150)
                ++count;
        }
    }
    return count;
}

UNIFIED_PDF_TEST(PrintEmbeddedPDFOpenPopupAnnotationWithRemoteSnapshotting)
{
    RetainPtr sourcePDFData = createPDFDataWithOpenPopupAnnotation();
    RetainPtr localPDFData = printEmbeddedPDFData(sourcePDFData.get(), false);
    RetainPtr remotePDFData = printEmbeddedPDFData(sourcePDFData.get(), true);
    ASSERT_GT([localPDFData length], 0u);
    ASSERT_GT([remotePDFData length], 0u);

    RetainPtr localPage = renderPrintedPage(localPDFData.get(), 1);
    RetainPtr remotePage = renderPrintedPage(remotePDFData.get(), 1);
    ASSERT_TRUE(localPage && remotePage);
    EXPECT_EQ(countYellowPixelsInPopup(localPage.get()), 0u);
    EXPECT_EQ(countYellowPixelsInPopup(remotePage.get()), 0u);
}

} // namespace TestWebKitAPI

#endif // ENABLE(UNIFIED_PDF) && WK_HAVE_C_SPI

#endif // PLATFORM(MAC)
