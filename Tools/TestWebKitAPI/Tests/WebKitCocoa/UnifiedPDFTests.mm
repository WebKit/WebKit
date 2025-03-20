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

#import "CGImagePixelReader.h"
#import "ContentSecurityPolicyTestHelpers.h"
#import "HTTPServer.h"
#import "IOSMouseEventTestHarness.h"
#import "InstanceMethodSwizzler.h"
#import "MouseSupportUIDelegate.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestCocoa.h"
#import "TestNavigationDelegate.h"
#import "TestPDFDocument.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "UISideCompositingScope.h"
#import "UnifiedPDFTestHelpers.h"
#import "WKPrinting.h"
#import "WKWebViewConfigurationExtras.h"
#import "WKWebViewForTestingImmediateActions.h"
#import <WebCore/Color.h>
#import <WebCore/ColorSerialization.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/_WKFeature.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/MakeString.h>

#if PLATFORM(IOS_FAMILY)
@interface UIPrintInteractionController ()
- (BOOL)_setupPrintPanel:(void (^)(UIPrintInteractionController *printInteractionController, BOOL completed, NSError *error))completion;
- (void)_generatePrintPreview:(void (^)(NSURL *previewPDF, BOOL shouldRenderOnChosenPaper))completionHandler;
- (void)_cleanPrintState;
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
        RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:documentName withExtension:@"pdf"]];
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

#endif // PLATFORM(MAC)

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
        dispatch_async(dispatch_get_main_queue(), ^{
            pdfData = adoptNS([[NSData alloc] initWithContentsOfURL:pdfURL]);
            [printInteractionController _cleanPrintState];
            done = true;
        });
    }];

    TestWebKitAPI::Util::run(&done);
    EXPECT_NE([pdfData length], 0UL);

    Ref pdf = TestWebKitAPI::TestPDFDocument::createFromData(pdfData.get());
    EXPECT_EQ(pdf->pageCount(), 16UL);
}

// FIXME: <webkit.org/b/288401> [ iOS Debug ] TestWebKitAPI.UnifiedPDF.ShouldNotRespectSetViewScale(api-test) is a constant timeout
#if !defined(NDEBUG)
UNIFIED_PDF_TEST(DISABLED_ShouldNotRespectSetViewScale)
#else
UNIFIED_PDF_TEST(ShouldNotRespectSetViewScale)
#endif
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

UNIFIED_PDF_TEST(LoadPDFWithSandboxCSPDirective)
{
    runLoadPDFWithSandboxCSPDirectiveTest([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
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

    RetainPtr pdfURL = [NSBundle.test_resourcesBundle URLForResource:String { fileName } withExtension:@"pdf"];
    HTTPResponse response { [NSData dataWithContentsOfURL:pdfURL.get()] };
    HTTPServer server { { { path, response }, { pathWithFragment, response } } };

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:pdfURL.get()]];
    auto colorsWithoutFragment = [webView sampleColors];

    [webView synchronouslyLoadRequest:server.request(pathWithFragment)];
    auto colorsWithFragment = [webView sampleColors];

    EXPECT_NE(colorsWithoutFragment, colorsWithFragment);
}

} // namespace TestWebKitAPI

#endif // ENABLE(UNIFIED_PDF)
