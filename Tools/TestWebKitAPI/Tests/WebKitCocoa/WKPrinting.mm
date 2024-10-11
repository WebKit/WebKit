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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferences.h>
#import <WebKit/_WKFrameHandle.h>
#import <wtf/RetainPtr.h>

typedef void (^CallCompletionBlock)();

@interface PrintWithSimulatedPageComputationUIDelegate : NSObject <WKUIDelegate>

- (void)waitForPagination;

@end

@implementation PrintWithSimulatedPageComputationUIDelegate {
    bool _isDone;
}

- (void)callBlockAsync:(CallCompletionBlock)callCompletionBlock
{
    dispatch_async(dispatch_get_main_queue(), ^{
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

TEST(Printing, PrintWithDelayedCompletion)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([PrintWithSimulatedPageComputationUIDelegate new]);
    [webView setUIDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.print()" completionHandler:nil];
    [delegate waitForPagination];
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
    auto printInfo = adoptNS([[NSPrintInfo alloc] init]);

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
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto webView = adoptNS([[WKPrintPageBordersWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([PrintShowingPrintPanelUIDelegate new]);
    [webView setUIDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.print()" completionHandler:nil];
    [webView _waitUntilPageBorderDrawn];
}

@interface TestPDFPrintDelegate : NSObject <WKUIDelegatePrivate>
- (void)waitForPrintFrameCall;
@end

@implementation TestPDFPrintDelegate {
    bool _printFrameCalled;
}

- (void)_webView:(WKWebView *)webView printFrame:(_WKFrameHandle *)frame pdfFirstPageSize:(CGSize)size completionHandler:(void (^)(void))completionHandler
{
    completionHandler();
    _printFrameCalled = true;
}

- (void)waitForPrintFrameCall
{
    while (!_printFrameCalled)
        TestWebKitAPI::Util::spinRunLoop();
}

@end

class PrintWithJSExecutionOptionTests : public ::testing::TestWithParam<bool> {
public:
    bool allowsContentJavascript() const { return GetParam(); }

    static NSURLRequest *pdfRequest()
    {
        return [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test_print" withExtension:@"pdf"]];
    }
};

TEST_P(PrintWithJSExecutionOptionTests, PDFWithWindowPrintEmbeddedJS)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    RetainPtr delegate = adoptNS([TestPDFPrintDelegate new]);
    [webView setUIDelegate:delegate.get()];

    RetainPtr preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [preferences setAllowsContentJavaScript:allowsContentJavascript()];

    [webView synchronouslyLoadRequest:pdfRequest() preferences:preferences.get()];

    [delegate waitForPrintFrameCall];
}

INSTANTIATE_TEST_SUITE_P(Printing,
    PrintWithJSExecutionOptionTests,
    testing::Bool(),
    [](testing::TestParamInfo<bool> info) { return std::string { "allowsContentJavascript_is_" } + (info.param ? "true" : "false"); }
);
#endif
