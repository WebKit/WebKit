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

#if ENABLE(UNIFIED_PDF)

#import "AppKitSPI.h"
#import "CGImagePixelReader.h"
#import "HTTPServer.h"
#import "IOSMouseEventTestHarness.h"
#import "InstanceMethodSwizzler.h"
#import "MouseSupportUIDelegate.h"
#import "PDFTestHelpers.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestCocoa.h"
#import "TestNavigationDelegate.h"
#import "TestPDFDocument.h"
#import "TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "UISideCompositingScope.h"
#import "WKPrinting.h"
#import "WKWebViewConfigurationExtras.h"
#import "WKWebViewForTestingImmediateActions.h"
#import <WebCore/Color.h>
#import <WebCore/ColorSerialization.h>
#import <WebCore/WebEvent.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/_WKFeature.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/MakeString.h>

@interface WKWebView ()
- (void)copy:(id)sender;
@end

#if PLATFORM(IOS_FAMILY)
@interface UIPrintInteractionController ()
- (BOOL)_setupPrintPanel:(void (^)(UIPrintInteractionController *printInteractionController, BOOL completed, NSError *error))completion;
- (void)_generatePrintPreview:(void (^)(NSURL *previewPDF, BOOL shouldRenderOnChosenPaper))completionHandler;
- (void)_cleanPrintState;
@end

@interface WKApplicationStateTrackingView : UIView
- (BOOL)isBackground;
@end
#endif

@interface ObserveWebContentCrashNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation ObserveWebContentCrashNavigationDelegate {
    bool _webProcessCrashed;
    bool _navigationFinished;
}

- (void)_webView:(WKWebView *)webView webContentProcessDidTerminateWithReason:(_WKProcessTerminationReason)reason
{
    _webProcessCrashed = true;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    _navigationFinished = true;
}

- (bool)webProcessCrashed
{
    return _webProcessCrashed;
}

- (bool)navigationFinished
{
    return _navigationFinished;
}

@end

namespace TestWebKitAPI {

#if PLATFORM(MAC)

class UnifiedPDFWithKeyboardScrolling : public testing::Test {
public:
    RetainPtr<TestWKWebView> webView;

    void SetUp() final
    {
        webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configurationForWebViewTestingUnifiedPDF().get() addToWindow:YES]);
        [webView setForceWindowToBecomeKey:YES];
    }

    void synchronouslyLoadPDFDocument(String documentName)
    {
        RetainPtr request = adoptNS([[NSURLRequest alloc] initWithURL:[NSBundle.test_resourcesBundle URLForResource:documentName.createNSString().get() withExtension:@"pdf"]]);
        [webView synchronouslyLoadRequest:request.get()];
        [[webView window] makeFirstResponder:webView.get()];
        [[webView window] makeKeyAndOrderFront:nil];
        [[webView window] orderFrontRegardless];
        [webView waitForNextPresentationUpdate];
    }

    void scrollDown(Seconds duration = 200_ms)
    {
        pressKey(NSDownArrowFunctionKey, DownArrowKeyCode, duration);
    }

    void scrollRight(Seconds duration = 200_ms)
    {
        pressKey(NSRightArrowFunctionKey, RightArrowKeyCode, duration);
    }

private:
    static constexpr unsigned short DownArrowKeyCode { 0x7D };
    static constexpr unsigned short RightArrowKeyCode { 0x7C };

    void pressKey(auto key, unsigned short code, Seconds duration = 200_ms)
    {
        NSString *keyString = [NSString stringWithFormat:@"%C", static_cast<unichar>(key)];
        [webView sendKey:keyString code:code isDown:YES modifiers:0];
        Util::runFor(duration);
        [webView sendKey:keyString code:code isDown:NO modifiers:0];
        Util::runFor(50_ms);
    }
};

TEST_F(UnifiedPDFWithKeyboardScrolling, KeyboardScrollingInSinglePageMode)
{
    if constexpr (!unifiedPDFForTestingEnabled)
        return;

    synchronouslyLoadPDFDocument("multiple-pages"_s);

    [webView objectByEvaluatingJavaScript:@"internals.setPDFDisplayModeForTesting(document.querySelector('embed'), 'SinglePageDiscrete')"];
    [webView waitForNextPresentationUpdate];
    [webView setMagnification:2];

    auto colorsBeforeScrolling = [webView sampleColors];
    Vector<WebCore::Color> colorsAfterScrollingDown;
    while (true) {
        scrollDown();
        colorsAfterScrollingDown = [webView sampleColors];
        if (colorsBeforeScrolling != colorsAfterScrollingDown)
            break;
    }

    Vector<WebCore::Color> colorsAfterScrollingRight;
    while (true) {
        scrollRight();
        colorsAfterScrollingRight = [webView sampleColors];
        if (colorsAfterScrollingDown != colorsAfterScrollingRight)
            break;
    }
}

TEST_F(UnifiedPDFWithKeyboardScrolling, DisplayModeTransitionLandingPage)
{
    if constexpr (!unifiedPDFForTestingEnabled)
        return;

    synchronouslyLoadPDFDocument("multiple-pages-colored"_s);

    auto colorsBefore = [webView sampleColors];

    scrollDown(800_ms);
    [webView objectByEvaluatingJavaScript:@"internals.setPDFDisplayModeForTesting(document.querySelector('embed'), 'SinglePageDiscrete')"];
    [webView waitForNextPresentationUpdate];

    auto colorsAfter = [webView sampleColors];

    EXPECT_NE(colorsBefore, colorsAfter);
}

UNIFIED_PDF_TEST(CopyEditingCommandOnEmptySelectionShouldNotCrash)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configurationForWebViewTestingUnifiedPDF().get() addToWindow:YES]);
    [webView setForceWindowToBecomeKey:YES];

    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-pages" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];
    [[webView window] makeFirstResponder:webView.get()];
    [[webView window] makeKeyAndOrderFront:nil];
    [[webView window] orderFrontRegardless];

    [webView sendClickAtPoint:NSMakePoint(200, 200)];
    [webView objectByEvaluatingJavaScript:@"internals.sendEditingCommandToPDFForTesting(document.querySelector('embed'), 'copy')"];
}

TEST_P(PrintWithJSExecutionOptionTests, PDFWithWindowPrintEmbeddedJS)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configurationForWebViewTestingUnifiedPDF().get() addToWindow:YES]);
    runTest(webView.get());
}

INSTANTIATE_TEST_SUITE_P(UnifiedPDF, PrintWithJSExecutionOptionTests, testing::Bool(), &PrintWithJSExecutionOptionTests::testNameGenerator);

UNIFIED_PDF_TEST(DictionaryLookupDoesNotAssertOnEmptyRange)
{
    RetainPtr webView = adoptNS([[WKWebViewForTestingImmediateActions alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
    [webView synchronouslyLoadHTMLString:@"<embed src='metalSpecTOC.pdf' width='600' height='600'>"];
    [webView waitForNextPresentationUpdate];
    [webView simulateImmediateAction:NSMakePoint(200, 200)];
}

UNIFIED_PDF_TEST(TabKeyOnPDFTextFieldShouldNotCrash)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configurationForWebViewTestingUnifiedPDF().get() addToWindow:YES]);
    RetainPtr delegate = adoptNS([[ObserveWebContentCrashNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test_form" withExtension:@"pdf"]];
    [webView loadRequest:request.get()];

    Util::waitFor([delegate] {
        return [delegate webProcessCrashed] || [delegate navigationFinished];
    });
    EXPECT_FALSE([delegate webProcessCrashed]);

    auto pressTabKey = [&](auto completionHandler) {
        [webView sendKey:@"\t" code:0x30 isDown:YES modifiers:0];
        [webView sendKey:@"\t" code:0x30 isDown:NO modifiers:0];
        completionHandler();
    };

    auto testTabKeysAtPoint = [&](NSPoint point) {
        [webView sendClickAtPoint:point];
        [webView waitForNextPresentationUpdate];

        bool doneWaiting = false;
        pressTabKey([&doneWaiting] {
            doneWaiting = true;
        });
        Util::run(&doneWaiting);
        EXPECT_FALSE([delegate webProcessCrashed]);
    };

    testTabKeysAtPoint(NSMakePoint(220, 300));
}

// FIXME when rdar://168769459 is resolved.
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 260000
UNIFIED_PDF_TEST(DISABLED_PrintSize)
#else
UNIFIED_PDF_TEST(PrintSize)
#endif
{
    RetainPtr configuration = configurationForWebViewTestingUnifiedPDF();
    RetainPtr schemeHandler = adoptNS([TestURLSchemeHandler new]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"test"];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr delegate = adoptNS([PDFPrintUIDelegate new]);
    [webView setUIDelegate:delegate.get()];

    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto url = task.request.URL;
        NSData *data;
        NSString *mimeType;
        if ([url.path isEqualToString:@"/main.html"]) {
            mimeType = @"text/html";
            const char* html = "<br/><iframe src='test.pdf' id='pdfframe'></iframe>";
            data = [NSData dataWithBytes:html length:strlen(html)];
        } else if ([url.path isEqualToString:@"/test.pdf"]) {
            mimeType = @"application/pdf";
            data = testPDFData().get();
        } else {
            EXPECT_WK_STREQ(url.path, "/test_print.pdf");
            mimeType = @"application/pdf";
            data = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"test_print" withExtension:@"pdf"]];
        }
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:url MIMEType:mimeType expectedContentLength:data.length textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:data];
        [task didFinish];
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///test_print.pdf"]]];
    auto size = [delegate waitForPageSize];
    EXPECT_EQ(size.height, 792.0);
    EXPECT_EQ(size.width, 612.0);

    __block bool receivedSize = false;
    [webView _getPDFFirstPageSizeInFrame:[webView _mainFrame] completionHandler:^(CGSize requestedSize) {
        EXPECT_EQ(requestedSize.height, 792.0);
        EXPECT_EQ(requestedSize.width, 612.0);
        receivedSize = true;
    }];
    TestWebKitAPI::Util::run(&receivedSize);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    [webView _test_waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"window.print()" completionHandler:nil];
    auto mainFrameSize = [delegate waitForPageSize];
    EXPECT_EQ(mainFrameSize.height, 0.0);
    EXPECT_EQ(mainFrameSize.width, 0.0);

    receivedSize = false;
    [webView _getPDFFirstPageSizeInFrame:[webView _mainFrame] completionHandler:^(CGSize requestedSize) {
        EXPECT_EQ(requestedSize.height, 0.0);
        EXPECT_EQ(requestedSize.width, 0.0);
        receivedSize = true;
    }];
    TestWebKitAPI::Util::run(&receivedSize);

    [webView evaluateJavaScript:@"pdfframe.contentWindow.print()" completionHandler:nil];
    auto pdfFrameSize = [delegate waitForPageSize];
    EXPECT_NEAR(pdfFrameSize.height, 28.799999, .00001);
    EXPECT_NEAR(pdfFrameSize.width, 129.600006, .00001);

    receivedSize = false;
    [webView _getPDFFirstPageSizeInFrame:[delegate lastPrintedFrame] completionHandler:^(CGSize requestedSize) {
        EXPECT_NEAR(requestedSize.height, 28.799999, .00001);
        EXPECT_NEAR(requestedSize.width, 129.600006, .00001);
        receivedSize = true;
    }];
    TestWebKitAPI::Util::run(&receivedSize);
}

UNIFIED_PDF_TEST(TextAnnotationHoverEffect)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configurationForWebViewTestingUnifiedPDF().get() addToWindow:YES]);
    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"textInput" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];
    [[webView window] makeFirstResponder:webView.get()];
    [[webView window] makeKeyAndOrderFront:nil];
    [[webView window] orderFrontRegardless];

    auto colorsBeforeHover = [webView sampleColors];

    [webView mouseMoveToPoint:NSMakePoint(200, 200) withFlags:0];
    [webView waitForPendingMouseEvents];
    [webView waitForNextPresentationUpdate];
    auto colorsDuringHover = [webView sampleColors];
    EXPECT_NE(colorsBeforeHover, colorsDuringHover);

    [webView mouseMoveToPoint:NSMakePoint(50, 50) withFlags:0];
    [webView waitForPendingMouseEvents];
    [webView waitForNextPresentationUpdate];
    auto colorsAfterHover = [webView sampleColors];
    EXPECT_EQ(colorsBeforeHover, colorsAfterHover);
}

#endif // PLATFORM(MAC)

#if ENABLE(PDF_HUD)

UNIFIED_PDF_TEST(SetPageZoomFactorDoesNotBailIncorrectly)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForWebViewTestingUnifiedPDF(true).get()]);
    [webView _setWindowOcclusionDetectionEnabled:NO];
    [webView loadData:testPDFData().get() MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:[NSURL URLWithString:@"https://www.apple.com/testPath"]];
    [webView _test_waitForDidFinishNavigation];

    double scaleBeforeZooming = [webView _pageZoomFactor];

    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    [[webView _pdfHUDs].anyObject performSelector:NSSelectorFromString(@"_performActionForControl:") withObject:@"plus.magnifyingglass"];
    [webView waitForNextPresentationUpdate];

    double scaleAfterZooming = [webView _pageZoomFactor];
    EXPECT_GT(scaleAfterZooming, scaleBeforeZooming);

    [webView _setPageZoomFactor:1];
    [webView waitForNextPresentationUpdate];

    double scaleAfterResetting = [webView _pageZoomFactor];
    EXPECT_LT(scaleAfterResetting, scaleAfterZooming);
    EXPECT_EQ(scaleAfterResetting, 1.0);
}

static void checkFrame(NSRect frame, CGFloat x, CGFloat y, CGFloat width, CGFloat height, std::optional<CGFloat> frameOriginTolerance = { })
{
    if (frameOriginTolerance) {
        auto tolerance = *frameOriginTolerance;
        EXPECT_TRUE(std::abs(frame.origin.x - x) <= tolerance) << "Expected frameOrigin.x to be around " << x << ", got " << frame.origin.x;
        EXPECT_TRUE(std::abs(frame.origin.y - y) <= tolerance) << "Expected frameOrigin.y to be around " << y << ", got " << frame.origin.y;
    } else {
        EXPECT_EQ(frame.origin.x, x);
        EXPECT_EQ(frame.origin.y, y);
    }
    EXPECT_EQ(frame.size.width, width);
    EXPECT_EQ(frame.size.height, height);
}

UNIFIED_PDF_TEST(PDFHUDMainResourcePDF)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForWebViewTestingUnifiedPDF(true).get()]);
    [webView _setWindowOcclusionDetectionEnabled:NO];
    [webView loadData:testPDFData().get() MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:[NSURL URLWithString:@"https://www.apple.com/testPath"]];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 0, 0, 800, 600);

    RetainPtr delegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:delegate.get()];
    __block bool saveRequestReceived = false;
    [delegate setSaveDataToFile:^(WKWebView *webViewFromDelegate, NSData *data, NSString *suggestedFilename, NSString *mimeType, NSURL *originatingURL) {
        EXPECT_EQ(webView.get(), webViewFromDelegate);
        EXPECT_TRUE([data isEqualToData:testPDFData().get()]);
        EXPECT_WK_STREQ(suggestedFilename, "testPath.pdf");
        EXPECT_WK_STREQ(mimeType, "application/pdf");
        saveRequestReceived = true;
    }];
    [[webView _pdfHUDs].anyObject performSelector:NSSelectorFromString(@"_performActionForControl:") withObject:@"arrow.down.circle"];
    TestWebKitAPI::Util::run(&saveRequestReceived);

    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    [webView _killWebContentProcess];
    while ([webView _pdfHUDs].count)
        TestWebKitAPI::Util::spinRunLoop();
}

UNIFIED_PDF_TEST(PDFHUDMoveIFrame)
{
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
            const char* html = "<br/><iframe src='test.pdf' id='pdfframe'></iframe>";
            [task didReceiveResponse:response.get()];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else {
            EXPECT_WK_STREQ(task.request.URL.path, "/test.pdf");
            RetainPtr data = testPDFData();
            RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"application/pdf" expectedContentLength:[data length] textEncodingName:nil]);
            [task didReceiveResponse:response.get()];
            [task didReceiveData:data.get()];
            [task didFinish];
        }
    }];

    RetainPtr configuration = configurationForWebViewTestingUnifiedPDF(true);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView _setWindowOcclusionDetectionEnabled:NO];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];

    // If the TestWKWebView is not visible, visibilityDidChange will be called with false, and there will be no HUD.
    if (![webView _pdfHUDs].count)
        return;

    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 10, 28, 300, 150);

    [webView evaluateJavaScript:@"pdfframe.width=400" completionHandler:nil];
    while ([webView _pdfHUDs].anyObject.frame.size.width != 400)
        TestWebKitAPI::Util::spinRunLoop();
    checkFrame([webView _pdfHUDs].anyObject.frame, 10, 28, 400, 150);

    [webView evaluateJavaScript:@"var frameReference = pdfframe; document.body.removeChild(pdfframe)" completionHandler:nil];
    while ([webView _pdfHUDs].count)
        TestWebKitAPI::Util::spinRunLoop();
    [webView evaluateJavaScript:@"document.body.appendChild(frameReference)" completionHandler:nil];
    while (![webView _pdfHUDs].count)
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 0, 0, 0, 0);
    while ([webView _pdfHUDs].anyObject.frame.size.width != 400)
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 10, 28, 400, 150);

    [webView setPageZoom:1.4];
    while ([webView _pdfHUDs].anyObject.frame.size.width != 560)
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 13, 40, 560, 210, 1);
}

UNIFIED_PDF_TEST(PDFHUDNestedIFrames)
{
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        RetainPtr htmlResponse = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            const char* html = "<iframe src='frame.html' id='parentframe'></iframe>";
            [task didReceiveResponse:htmlResponse.get()];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else if ([task.request.URL.path isEqualToString:@"/frame.html"]) {
            const char* html = "<iframe src='test.pdf'></iframe>";
            [task didReceiveResponse:htmlResponse.get()];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else {
            EXPECT_WK_STREQ(task.request.URL.path, "/test.pdf");
            RetainPtr data = testPDFData();
            RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"application/pdf" expectedContentLength:[data length] textEncodingName:nil]);
            [task didReceiveResponse:response.get()];
            [task didReceiveData:data.get()];
            [task didFinish];
        }
    }];

    RetainPtr configuration = configurationForWebViewTestingUnifiedPDF(true);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView _setWindowOcclusionDetectionEnabled:NO];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 20, 20, 300, 150);

    [webView evaluateJavaScript:@"document.body.removeChild(parentframe)" completionHandler:nil];
    while ([webView _pdfHUDs].count)
        TestWebKitAPI::Util::spinRunLoop();
}

UNIFIED_PDF_TEST(PDFHUDIFrame3DTransform)
{
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        RetainPtr htmlResponse = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            const char* html = "<iframe src='test.pdf' height=500 width=500 style='transform:rotateY(235deg);'></iframe>";
            [task didReceiveResponse:htmlResponse.get()];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else {
            EXPECT_WK_STREQ(task.request.URL.path, "/test.pdf");
            RetainPtr data = testPDFData();
            RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"application/pdf" expectedContentLength:[data length] textEncodingName:nil]);
            [task didReceiveResponse:response.get()];
            [task didReceiveData:data.get()];
            [task didFinish];
        }
    }];

    RetainPtr configuration = configurationForWebViewTestingUnifiedPDF(true);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView _setWindowOcclusionDetectionEnabled:NO];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 403, 10, 500, 500);
}

UNIFIED_PDF_TEST(PDFHUDMultipleIFrames)
{
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        RetainPtr htmlResponse = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            const char* html = "<iframe src='test.pdf' height=100 width=150></iframe><iframe src='test.pdf' height=123 width=134></iframe>";
            [task didReceiveResponse:htmlResponse.get()];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else {
            EXPECT_WK_STREQ(task.request.URL.path, "/test.pdf");
            RetainPtr data = testPDFData();
            RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"application/pdf" expectedContentLength:[data length] textEncodingName:nil]);
            [task didReceiveResponse:response.get()];
            [task didReceiveData:data.get()];
            [task didFinish];
        }
    }];

    RetainPtr configuration = configurationForWebViewTestingUnifiedPDF(true);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    [webView _setWindowOcclusionDetectionEnabled:NO];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ([webView _pdfHUDs].count, 2u);
    bool hadLeftFrame = false;
    bool hadRightFrame = false;
    for (NSView *hud in [webView _pdfHUDs]) {
        if (hud.frame.origin.x == 10) {
            checkFrame(hud.frame, 10, 33, 150, 100);
            hadLeftFrame = true;
        } else {
            checkFrame(hud.frame, 164, 10, 134, 123);
            hadRightFrame = true;
        }
    }
    EXPECT_TRUE(hadLeftFrame);
    EXPECT_TRUE(hadRightFrame);
}

UNIFIED_PDF_TEST(PDFHUDLoadPDFTypeWithPluginsBlocked)
{
    RetainPtr configuration = configurationForWebViewTestingUnifiedPDF(true);
    [configuration _setOverrideContentSecurityPolicy:@"object-src 'none'"];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView _setWindowOcclusionDetectionEnabled:NO];
    [webView loadData:testPDFData().get() MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:[NSURL URLWithString:@"https://www.apple.com/testPath"]];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 0, 0, 800, 600);
}

#endif // ENABLE(PDF_HUD)

UNIFIED_PDF_TEST(SnapshotsPaintPageContent)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configurationForWebViewTestingUnifiedPDF().get() addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:@"<embed src='multiple-pages.pdf' width='600' height='600'>"];
    [webView waitForNextPresentationUpdate];

    __block bool done = false;

    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:NSMakeRect(100, 100, 100, 100)];

    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);
        RetainPtr cgImage = Util::convertToCGImage(snapshotImage);

        CGImagePixelReader reader { cgImage.get() };

        bool foundNonWhitePixel = false;

        for (unsigned x = 0; x < reader.width(); x++) {
            for (unsigned y = 0; y < reader.height(); y++) {
                if (reader.at(x, y) != WebCore::Color::white) {
                    foundNonWhitePixel = true;
                    break;
                }
            }
        }

        EXPECT_TRUE(foundNonWhitePixel);

        done = true;
    }];

    Util::run(&done);
}

#if PLATFORM(IOS) || PLATFORM(VISION)

UNIFIED_PDF_TEST(StablePresentationUpdateCallback)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:TestWebKitAPI::configurationForWebViewTestingUnifiedPDF().get()]);

    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
    [webView loadRequest:request.get()];
    [webView _test_waitForDidFinishNavigation];

    __block bool finished;
    [webView _doAfterNextStablePresentationUpdate:^{
        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

#endif

UNIFIED_PDF_TEST(PasswordFormShouldDismissAfterNavigation)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configurationForWebViewTestingUnifiedPDF().get() addToWindow:YES]);

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"notEncrypted" withExtension:@"pdf"]]];
    auto colorsBefore = [webView sampleColorsWithInterval:2];

    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"encrypted" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];

    [webView synchronouslyGoBack];
    [webView synchronouslyGoForward];
    // FIXME: Perform the document unlock after detecting the plugin element, either through MutationObserver scripting or some TestWKWebView hook.
    Util::runFor(50_ms);

    [webView objectByEvaluatingJavaScript:@"internals.unlockPDFDocumentForTesting(document.querySelector('embed'), 'test')"];
    auto colorsAfter = [webView sampleColorsWithInterval:2];

    EXPECT_EQ(colorsBefore, colorsAfter);
}

UNIFIED_PDF_TEST(WebProcessShouldNotCrashWithUISideCompositingDisabled)
{
    UISideCompositingScope scope { UISideCompositingState::Disabled };

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configurationForWebViewTestingUnifiedPDF().get() addToWindow:YES]);
    RetainPtr delegate = adoptNS([[ObserveWebContentCrashNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
    [webView loadRequest:request.get()];

    Util::waitFor([delegate] {
        return [delegate webProcessCrashed] || [delegate navigationFinished];
    });
    EXPECT_FALSE([delegate webProcessCrashed]);
}

UNIFIED_PDF_TEST(SelectAllText)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]]];

    auto selectAllText = [](TestWKWebView *webView) {
#if PLATFORM(IOS_FAMILY)
        [webView selectTextInGranularity:UITextGranularityDocument atPoint:CGPointMake(100, 100)];
#else
        [[webView window] makeFirstResponder:webView];
        [[webView window] makeKeyAndOrderFront:nil];
        [[webView window] orderFrontRegardless];
        [webView sendClickAtPoint:NSMakePoint(100, 100)];
        [webView selectAll:nil];
#endif
    };

    selectAllText(webView.get());
    [webView waitForNextPresentationUpdate];

#if PLATFORM(IOS_FAMILY)
    RetainPtr contentView = [webView textInputContentView];
    RetainPtr selectedText = [contentView selectedText];
#else
    [webView copy:nil];
    [webView waitForNextPresentationUpdate];
    RetainPtr selectedText = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
#endif
    EXPECT_WK_STREQ(@"Test PDF Content\n555-555-1234", selectedText.get());
}

#if PLATFORM(IOS_FAMILY)

UNIFIED_PDF_TEST(SpeakSelection)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]]];

    EXPECT_WK_STREQ(@"Test PDF Content\n555-555-1234", [webView textForSpeakSelection]);
}

UNIFIED_PDF_TEST(CopySelectedText)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]]];

    [webView selectTextInGranularity:UITextGranularityWord atPoint:CGPointMake(100, 100)];
    [webView copy:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_WK_STREQ(@"Test", [[UIPasteboard generalPasteboard] string]);
}

UNIFIED_PDF_TEST(SelectTextInRotatedPage)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test-rotated-cw-90" withExtension:@"pdf"]]];
    [webView becomeFirstResponder];
    [webView waitForNextPresentationUpdate];

    [webView selectTextInGranularity:UITextGranularityWord atPoint:CGPointMake(350, 200)];
    [webView waitForNextPresentationUpdate];

    RetainPtr contentView = [webView textInputContentView];
    RetainPtr selectionRects = [contentView selectionRectsForRange:[contentView selectedTextRange]];
    auto firstSelectionRect = [[selectionRects firstObject] rect];

    EXPECT_EQ(1U, [selectionRects count]);
    EXPECT_GT(firstSelectionRect.size.height, firstSelectionRect.size.width); // Final selection rect should run vertically.
    EXPECT_WK_STREQ("Test", [contentView selectedText]);
}

UNIFIED_PDF_TEST(LookUpSelectedText)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]]];
    [webView selectTextInGranularity:UITextGranularityWord atPoint:CGPointMake(150, 100)];

#if HAVE(UI_WK_DOCUMENT_CONTEXT)
    RetainPtr request = adoptNS([[UIWKDocumentRequest alloc] init]);
    [request setFlags:UIWKDocumentRequestText];

    RetainPtr context = [webView synchronouslyRequestDocumentContext:request.get()];
    EXPECT_WK_STREQ(@"PDF", dynamic_objc_cast<NSString>([context selectedText]));
#endif

    bool done = false;
    RetainPtr<NSString> lookupContext;
    NSRange selectedRangeInLookupContext;

    auto lookupBlock = makeBlockPtr([&](id, NSString *context, NSRange range, CGRect) {
        lookupContext = context;
        selectedRangeInLookupContext = range;
        done = true;
    });

    InstanceMethodSwizzler lookupSwizzler {
#if USE(BROWSERENGINEKIT)
        [BETextInteraction class],
        @selector(showDictionaryForTextInContext:definingTextInRange:fromRect:),
#else
        [UIWKTextInteractionAssistant class],
        @selector(lookup:withRange:fromRect:),
#endif
        imp_implementationWithBlock(lookupBlock.get())
    };

    [webView defineSelection];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ(@"Test PDF Content\n555-555-1234", lookupContext.get());
    EXPECT_EQ(selectedRangeInLookupContext.location, 5U);
    EXPECT_EQ(selectedRangeInLookupContext.length, 3U);
}

UNIFIED_PDF_TEST(PrintPDFUsingPrintInteractionController)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);

    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-pages" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];
    [webView waitForNextPresentationUpdate];

    RetainPtr printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
    [printPageRenderer addPrintFormatter:[webView viewPrintFormatter] startingAtPageAtIndex:0];

    RetainPtr printInteractionController = adoptNS([[UIPrintInteractionController alloc] init]);
    [printInteractionController setPrintPageRenderer:printPageRenderer.get()];

    __block bool done = false;
    __block RetainPtr<NSData> pdfData;

    [printInteractionController _setupPrintPanel:nil];
    [printInteractionController _generatePrintPreview:^(NSURL *pdfURL, BOOL shouldRenderOnChosenPaper) {
        dispatch_async(mainDispatchQueueSingleton(), ^{
            pdfData = adoptNS([[NSData alloc] initWithContentsOfURL:pdfURL]);
            [printInteractionController _cleanPrintState];
            done = true;
        });
    }];

    TestWebKitAPI::Util::run(&done);
    EXPECT_NE([pdfData length], 0UL);

    RetainPtr pdf = adoptNS([[TestPDFDocument alloc] initFromData:pdfData.get()]);
    EXPECT_EQ([pdf pageCount], 16);
}

UNIFIED_PDF_TEST(ShouldNotRespectSetViewScale)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);

    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];
    [webView waitForNextPresentationUpdate];
    auto colorsBefore = [webView sampleColors];

    [webView _setViewScale:1.5];
    [webView waitForNextPresentationUpdate];
    auto colorsAfter = [webView sampleColors];

    EXPECT_EQ(colorsBefore, colorsAfter);
}

UNIFIED_PDF_TEST(KeepScrollPositionAtOriginAfterAnimatedResize)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 800) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-pages" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];
    [webView waitForNextPresentationUpdate];

    [[webView scrollView] setContentOffset:CGPointMake(0, 400)];
    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];

    auto contentOffsetAfterResizing = [webView](CGFloat width, CGFloat height) {
        [webView _beginAnimatedResizeWithUpdates:^{
            [webView setFrame:CGRectMake(0, 0, width, height)];
        }];
        [webView _endAnimatedResize];
        [webView waitForNextVisibleContentRectUpdate];
        [webView waitForNextPresentationUpdate];
        return [[webView scrollView] contentOffset];
    };

    auto checkOffsetsAreApproximatelyEqual = [](CGPoint offset, CGPoint otherOffset) {
        static constexpr auto epsilon = 3;

        EXPECT_LT(std::abs(offset.x - otherOffset.x), epsilon);
        EXPECT_LT(std::abs(offset.y - otherOffset.y), epsilon);
    };

    Vector<CGPoint, 4> offsetsAfterResizing;
    offsetsAfterResizing.append(contentOffsetAfterResizing(800, 600));
    offsetsAfterResizing.append(contentOffsetAfterResizing(600, 800));
    offsetsAfterResizing.append(contentOffsetAfterResizing(800, 600));
    offsetsAfterResizing.append(contentOffsetAfterResizing(600, 800));

    checkOffsetsAreApproximatelyEqual(offsetsAfterResizing[0], offsetsAfterResizing[2]);
    checkOffsetsAreApproximatelyEqual(offsetsAfterResizing[1], offsetsAfterResizing[3]);
}

UNIFIED_PDF_TEST(KeepRelativeScrollPositionAfterAnimatedResize)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 800) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-pages" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];
    [webView waitForNextPresentationUpdate];

    [[webView scrollView] setContentOffset:CGPointMake(0, 4000)];
    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];

    [webView _beginAnimatedResizeWithUpdates:^{
        [webView setFrame:CGRectMake(0, 0, 400, 800)];
    }];
    [webView _endAnimatedResize];
    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ([webView scrollView].contentOffset, CGPointMake(0, 2533));
}

UNIFIED_PDF_TEST(KeepRelativeScrollPositionAfterZoomingAndViewportUpdate)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 800) configuration:configurationForWebViewTestingUnifiedPDF().get()]);

    RetainPtr scrollView = [webView scrollView];

    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-pages" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];

    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];

    [webView setZoomScaleSimulatingUserTriggeredZoom:3];

    [scrollView setContentOffset:CGPointMake([scrollView contentSize].width - [scrollView frame].size.width, 12000)];
    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];

    CGSize originalSize = [webView frame].size;
    CGSize layoutSize = CGSizeMake(originalSize.width - 200, originalSize.height);
    [webView _overrideLayoutParametersWithMinimumLayoutSize:layoutSize minimumUnobscuredSizeOverride:layoutSize maximumUnobscuredSizeOverride:layoutSize];

    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ([webView scrollView].contentOffset, CGPointMake(600, 8002));
}

UNIFIED_PDF_TEST(ScrollOffsetResetWhenChangingPDF)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 800) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-pages" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];
    [webView waitForNextPresentationUpdate];

    [[webView scrollView] setContentOffset:CGPointMake(0, 4000)];
    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];

    request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-pages-colored" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];

    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ([webView scrollView].contentOffset, CGPointZero);
}

UNIFIED_PDF_TEST(ScrollOffsetUnchangedWithZeroSizeViewportUpdate)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 800) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-pages" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];
    [webView waitForNextPresentationUpdate];

    auto expectedScrollOffset = CGPointMake(0, 400);
    [[webView scrollView] setContentOffset:expectedScrollOffset];
    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ([webView scrollView].contentOffset, expectedScrollOffset);

    auto previousLayoutSize = [webView scrollView].bounds.size;
    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeZero minimumUnobscuredSizeOverride:CGSizeZero maximumUnobscuredSizeOverride:CGSizeZero];
    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ([webView scrollView].contentOffset, expectedScrollOffset);

    [webView _overrideLayoutParametersWithMinimumLayoutSize:previousLayoutSize minimumUnobscuredSizeOverride:previousLayoutSize maximumUnobscuredSizeOverride:previousLayoutSize];
    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ([webView scrollView].contentOffset, expectedScrollOffset);
}

UNIFIED_PDF_TEST(WebViewResizeShouldNotCrash)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:TestWebKitAPI::configurationForWebViewTestingUnifiedPDF().get() addToWindow:YES]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView setFrame:NSMakeRect(0, 0, 100, 100)];
    webView = nil;

    __block bool finishedDispatch = false;
    dispatch_async(mainDispatchQueueSingleton(), ^{
        finishedDispatch = true;
    });

    TestWebKitAPI::Util::run(&finishedDispatch);
}

static BOOL sIsBackground;
static BOOL isBackground(id self)
{
    return sIsBackground;
}

UNIFIED_PDF_TEST(WebViewLosesApplicationForegroundNotification)
{
    std::unique_ptr<InstanceMethodSwizzler> isInBackgroundSwizzler = makeUnique<InstanceMethodSwizzler>(NSClassFromString(@"WKApplicationStateTrackingView"), @selector(isBackground), reinterpret_cast<IMP>(isBackground));

    RetainPtr configuration = TestWebKitAPI::configurationForWebViewTestingUnifiedPDF();
    [configuration _setClientNavigationsRunAtForegroundPriority:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    [webView loadData:testPDFData().get() MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:[NSURL URLWithString:@"https://www.apple.com/0"]];
    [webView _test_waitForDidFinishNavigation];

    sIsBackground = YES;
    RetainPtr uiWindow = adoptNS([[UIWindow alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [uiWindow addSubview:webView.get()];
    [webView removeFromSuperview];

    [webView loadHTMLString:@"<meta name='viewport' content='width=device-width'><h1>hello world</h1>" baseURL:[NSURL URLWithString:@"https://www.apple.com/1"]];
    [webView _test_waitForDidFinishNavigationWithoutPresentationUpdate];

    sIsBackground = NO;
    [uiWindow addSubview:webView.get()];

    __block bool finished = false;
    // If the bug reproduces, no stable presentation update will ever come,
    // so the test times out here.
    [webView _doAfterNextPresentationUpdate:^{
        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

#if HAVE(UIFINDINTERACTION)

UNIFIED_PDF_TEST(WebViewFindAction)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:TestWebKitAPI::configurationForWebViewTestingUnifiedPDF().get() addToWindow:YES]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    EXPECT_FALSE([webView canPerformAction:@selector(find:) withSender:nil]);
    EXPECT_FALSE([webView canPerformAction:@selector(findNext:) withSender:nil]);
    EXPECT_FALSE([webView canPerformAction:@selector(findPrevious:) withSender:nil]);
    EXPECT_FALSE([webView canPerformAction:@selector(findAndReplace:) withSender:nil]);

    [webView setFindInteractionEnabled:YES];

    EXPECT_TRUE([webView canPerformAction:@selector(find:) withSender:nil]);
    EXPECT_TRUE([webView canPerformAction:@selector(findNext:) withSender:nil]);
    EXPECT_TRUE([webView canPerformAction:@selector(findPrevious:) withSender:nil]);
    EXPECT_FALSE([webView canPerformAction:@selector(findAndReplace:) withSender:nil]);
}

#endif

UNIFIED_PDF_TEST(WebViewBackgroundColor)
{
    RetainPtr sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    RetainPtr redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:TestWebKitAPI::configurationForWebViewTestingUnifiedPDF().get() addToWindow:YES]);

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"red" withExtension:@"html"]]];
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, redColor.get()));

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]]];
    EXPECT_FALSE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, redColor.get()));

    [webView synchronouslyGoBack];
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, redColor.get()));
}

#endif // PLATFORM(IOS_FAMILY)

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)

UNIFIED_PDF_TEST(MouseDidMoveOverPDF)
{
    TestWebKitAPI::Util::instantiateUIApplicationIfNeeded();

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
    RetainPtr delegate = adoptNS([MouseSupportUIDelegate new]);

    __block bool done = false;
    [delegate setMouseDidMoveOverElementHandler:^(_WKHitTestResult *) {
        done = true;
    }];

    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];
    [webView setUIDelegate:delegate.get()];

    TestWebKitAPI::MouseEventTestHarness { webView.get() }.mouseMove(50, 50);
    TestWebKitAPI::Util::run(&done);
}

#if ENABLE(IOS_TOUCH_EVENTS)
UNIFIED_PDF_TEST(SelectionClearsOnAnchorLinkTap)
{
    TestWebKitAPI::Util::instantiateUIApplicationIfNeeded();

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
    RetainPtr preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [preferences _setMouseEventPolicy:_WKWebsiteMouseEventPolicySynthesizeTouchEvents];
    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"anchorLink" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];
    RetainPtr contentView = [webView textInputContentView];

    [webView selectTextInGranularity:UITextGranularityWord atPoint:CGPointMake(224, 404)];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("Bye", [contentView selectedText]);

    TestWebKitAPI::MouseEventTestHarness testHarness { webView.get() };
    testHarness.mouseMove(224, 50);
    testHarness.mouseDown();
    testHarness.mouseUp();
    [webView waitForPendingMouseEvents];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("", [contentView selectedText]);
}
#endif

#endif

static HTTPServer pdfServerWithSandboxCSPDirective()
{
    RetainPtr pdfURL = [NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"];
    HTTPResponse response { [NSData dataWithContentsOfURL:pdfURL.get()] };
    response.headerFields.set("Content-Security-Policy"_s, "sandbox allow-scripts;"_s);
    return { { { "/"_s, response } } };
}

UNIFIED_PDF_TEST(LoadPDFWithSandboxCSPDirective)
{
    RetainPtr webView = [[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()];
    HTTPServer server { pdfServerWithSandboxCSPDirective() };

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]]];
    auto colorsWithPlainResponse = [webView sampleColors];

    [webView synchronouslyLoadRequest:server.request()];
    auto colorsWithCSPResponse = [webView sampleColors];

    EXPECT_EQ(colorsWithPlainResponse, colorsWithCSPResponse);
}

// FIXME: <https://webkit.org/b/287473> This test should be correct on iOS family, too.
#if PLATFORM(MAC)
UNIFIED_PDF_TEST(RespectsPageFragment)
#else
UNIFIED_PDF_TEST(DISABLED_RespectsPageFragment)
#endif
{
    static constexpr auto fileName = "multiple-pages-colored"_s;
    auto path = makeString('/', fileName, ".pdf"_s);
    auto pathWithFragment = makeString(path, "#page=2"_s);

    RetainPtr pdfURL = [NSBundle.test_resourcesBundle URLForResource:fileName.createNSString().get() withExtension:@"pdf"];
    HTTPResponse response { [NSData dataWithContentsOfURL:pdfURL.get()] };
    HTTPServer server { { { path, response }, { pathWithFragment, response } } };

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:pdfURL.get()]];
    auto colorsWithoutFragment = [webView sampleColors];

    [webView synchronouslyLoadRequest:server.request(pathWithFragment)];
    auto colorsWithFragment = [webView sampleColors];

    EXPECT_NE(colorsWithoutFragment, colorsWithFragment);
}

#if HAVE(UISCROLLVIEW_ALLOWS_KEYBOARD_SCROLLING)

static void checkKeyboardScrollability(TestWKWebView *webView)
{
    auto pressSpacebar = ^(void(^completionHandler)(void)) {
        RetainPtr firstWebEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:@" " charactersIgnoringModifiers:@" " modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:NO]);

        RetainPtr secondWebEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyUp timeStamp:CFAbsoluteTimeGetCurrent() characters:@" " charactersIgnoringModifiers:@" " modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:NO]);

        [webView handleKeyEvent:firstWebEvent.get() completion:^(WebEvent *theEvent, BOOL wasHandled) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), mainDispatchQueueSingleton(), ^{
                [webView handleKeyEvent:secondWebEvent.get() completion:^(WebEvent *theEvent, BOOL wasHandled) {
                    completionHandler();
                }];
            });
        }];
    };

    NSInteger scrollY = [[webView stringByEvaluatingJavaScript:@"window.scrollY"] integerValue];
    EXPECT_EQ(scrollY, 0);

    __block bool doneWaiting = false;

    pressSpacebar(^{
        NSInteger scrollY = [[webView stringByEvaluatingJavaScript:@"window.scrollY"] integerValue];
        EXPECT_GT(scrollY, 0);
        doneWaiting = true;
    });

    TestWebKitAPI::Util::run(&doneWaiting);
}

UNIFIED_PDF_TEST(MainFramePDFIsKeyboardScrollable)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 800) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-pages" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];
    [webView waitForNextPresentationUpdate];

    checkKeyboardScrollability(webView.get());
}

#if ENABLE(IOS_TOUCH_EVENTS)

UNIFIED_PDF_TEST(MainFramePDFIsKeyboardScrollableAfterTap)
{
    TestWebKitAPI::Util::instantiateUIApplicationIfNeeded();

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
    RetainPtr preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [preferences _setMouseEventPolicy:_WKWebsiteMouseEventPolicySynthesizeTouchEvents];
    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-pages" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];

    TestWebKitAPI::MouseEventTestHarness testHarness { webView.get() };
    testHarness.mouseMove(30, 30);
    testHarness.mouseDown();
    testHarness.mouseUp();
    [webView waitForPendingMouseEvents];
    [webView waitForNextPresentationUpdate];

    checkKeyboardScrollability(webView.get());
}

#endif

#endif

UNIFIED_PDF_TEST(WebViewIsDisplayingPDF)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForWebViewTestingUnifiedPDF().get() addToWindow:YES]);

    [webView loadData:testPDFData().get() MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:[NSURL URLWithString:@"https://www.apple.com/testPath"]];
    [webView _test_waitForDidFinishNavigation];
    EXPECT_TRUE([webView _isDisplayingPDF]);

    [webView loadHTMLString:@"<meta name='viewport' content='width=device-width'><h1>hello world</h1>" baseURL:[NSURL URLWithString:@"https://www.apple.com/1"]];
    [webView _test_waitForDidFinishNavigationWithoutPresentationUpdate];
    EXPECT_FALSE([webView _isDisplayingPDF]);
}

#if HAVE(LIQUID_GLASS)
UNIFIED_PDF_TEST(BackgroundAdaptsToColorScheme)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForWebViewTestingUnifiedPDF().get() addToWindow:YES]);
    [webView forceLightMode];

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]]];
    [webView waitForNextPresentationUpdate];

    RetainPtr pluginRootLayer = [webView firstLayerWithNameContaining:@"UnifiedPDFPlugin root"];
    RetainPtr initialBackgroundColor = [pluginRootLayer backgroundColor];

    [webView forceDarkMode];
    [webView waitForNextPresentationUpdate];

    RetainPtr backgroundColorAferAppearanceChange = [pluginRootLayer backgroundColor];
    EXPECT_FALSE(CGColorEqualToColor(backgroundColorAferAppearanceChange.get(), initialBackgroundColor.get()));

    [webView forceLightMode];
    [webView waitForNextPresentationUpdate];

    RetainPtr finalBackgroundColor = [pluginRootLayer backgroundColor];
    EXPECT_FALSE(CGColorEqualToColor(finalBackgroundColor.get(), backgroundColorAferAppearanceChange.get()));
    EXPECT_TRUE(CGColorEqualToColor(finalBackgroundColor.get(), initialBackgroundColor.get()));
}

UNIFIED_PDF_TEST(BackgroundDoesNotAdaptToColorSchemeOnEmbeddedDocuments)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
    [webView forceLightMode];

    [webView synchronouslyLoadHTMLString:@"<embed src='test.pdf' width='600' height='600'>"];
    [webView waitForNextPresentationUpdate];

    RetainPtr pluginRootLayer = [webView firstLayerWithNameContaining:@"UnifiedPDFPlugin root"];
    RetainPtr initialBackgroundColor = [pluginRootLayer backgroundColor];

    [webView forceDarkMode];
    [webView waitForNextPresentationUpdate];

    RetainPtr backgroundColorAferAppearanceChange = [pluginRootLayer backgroundColor];
    EXPECT_TRUE(CGColorEqualToColor(backgroundColorAferAppearanceChange.get(), initialBackgroundColor.get()));

    [webView forceLightMode];
    [webView waitForNextPresentationUpdate];

    RetainPtr finalBackgroundColor = [pluginRootLayer backgroundColor];
    EXPECT_TRUE(CGColorEqualToColor(finalBackgroundColor.get(), backgroundColorAferAppearanceChange.get()));
    EXPECT_TRUE(CGColorEqualToColor(finalBackgroundColor.get(), initialBackgroundColor.get()));
}
#endif

} // namespace TestWebKitAPI

#endif // ENABLE(UNIFIED_PDF)
