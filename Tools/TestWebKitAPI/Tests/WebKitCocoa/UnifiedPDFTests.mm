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
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
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
#import <WebKit/_WKFeature.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

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

UNIFIED_PDF_TEST(KeyboardScrollingInSinglePageMode)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configurationForWebViewTestingUnifiedPDF().get() addToWindow:YES]);
    [webView setForceWindowToBecomeKey:YES];

    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-pages" withExtension:@"pdf"]];
    [webView synchronouslyLoadRequest:request.get()];
    [[webView window] makeFirstResponder:webView.get()];
    [[webView window] makeKeyAndOrderFront:nil];
    [[webView window] orderFrontRegardless];
    [webView objectByEvaluatingJavaScript:@"internals.setPDFDisplayModeForTesting(document.querySelector('embed'), 'SinglePageDiscrete')"];
    [webView waitForNextPresentationUpdate];
    [webView setMagnification:2];

    auto colorsBeforeScrolling = [webView sampleColors];
    Vector<WebCore::Color> colorsAfterScrollingDown;
    while (true) {
        [webView sendKey:@"ArrowDown" code:NSDownArrowFunctionKey isDown:YES modifiers:0];
        Util::runFor(200_ms);
        [webView sendKey:@"ArrowDown" code:NSDownArrowFunctionKey isDown:NO modifiers:0];
        Util::runFor(50_ms);
        colorsAfterScrollingDown = [webView sampleColors];
        if (colorsBeforeScrolling != colorsAfterScrollingDown)
            break;
    }

    Vector<WebCore::Color> colorsAfterScrollingRight;
    while (true) {
        [webView sendKey:@"ArrowRight" code:NSRightArrowFunctionKey isDown:YES modifiers:0];
        Util::runFor(200_ms);
        [webView sendKey:@"ArrowRight" code:NSRightArrowFunctionKey isDown:NO modifiers:0];
        Util::runFor(50_ms);
        colorsAfterScrollingRight = [webView sampleColors];
        if (colorsAfterScrollingDown != colorsAfterScrollingRight)
            break;
    }
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

#endif // PLATFORM(IOS_FAMILY)

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)

UNIFIED_PDF_TEST(MouseDidMoveOverPDF)
{
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

#endif

UNIFIED_PDF_TEST(LoadPDFWithSandboxCSPDirective)
{
    runLoadPDFWithSandboxCSPDirectiveTest([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForWebViewTestingUnifiedPDF().get()]);
}

} // namespace TestWebKitAPI

#endif // ENABLE(UNIFIED_PDF)
