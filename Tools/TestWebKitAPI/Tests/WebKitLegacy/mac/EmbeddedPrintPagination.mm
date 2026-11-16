/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "Helpers/DeprecatedGlobalValues.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestPDFDocument.h"
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebView.h>
#import <wtf/RetainPtr.h>

@interface EmbeddedPrintPaginationLoadDelegate : NSObject <WebFrameLoadDelegate>
@end

@implementation EmbeddedPrintPaginationLoadDelegate
- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}
@end

namespace TestWebKitAPI {

// Regression test for webkit.org/b/303713 follow-up: when a WebHTMLView
// is a subview of a larger view hierarchy being printed, AppKit invokes
// -adjustPageHeightNew:top:bottom:limit: on it, which calls
// -_setPrinting: with all page dimensions equal to zero. After
// 304253@main that bottomed out in LocalFrame::setPrinting, which ran
// forceLayoutForPagination with a zero page size — asserting in debug
// and silently dropping text runs from the CG print context in release.
// This test exercises the same entry point and asserts that it does not
// crash.
TEST(WebKitLegacy, EmbeddedPrintPaginationWithZeroPageSize)
{
    RetainPtr webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) frameName:nil groupName:nil]);
    auto *webHTMLView = (WebHTMLView *)[[[webView mainFrame] frameView] documentView];

    RetainPtr loadDelegate = adoptNS([[EmbeddedPrintPaginationLoadDelegate alloc] init]);
    [webView setFrameLoadDelegate:loadDelegate.get()];

    didFinishLoad = false;
    [[webView mainFrame] loadHTMLString:@"<html><body><p>Hello, printing world.</p></body></html>" baseURL:nil];
    Util::run(&didFinishLoad);

    // This is the AppKit-driven call path used when a WebHTMLView is a
    // subview of a larger view hierarchy being printed. Before the fix
    // this asserted in debug (LocalFrame.cpp:resizePageRectsKeepingRatio)
    // and silently dropped text runs in release; reaching the
    // EXPECT_GT below without crashing is the pass condition.
    CGFloat newBottom = 0;
    [webHTMLView adjustPageHeightNew:&newBottom top:0 bottom:400 limit:400];
    EXPECT_GT(newBottom, 0);
}

#if HAVE(PDFKIT)
static RetainPtr<TestPDFDocument> printWebViewToPDF(NSString *styleSheet)
{
    RetainPtr webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) frameName:nil groupName:nil]);
    RetainPtr loadDelegate = adoptNS([[EmbeddedPrintPaginationLoadDelegate alloc] init]);
    [webView setFrameLoadDelegate:loadDelegate.get()];

    didFinishLoad = false;
    [[webView mainFrame] loadHTMLString:[NSString stringWithFormat:@"<style>%@ body { margin: 0 }</style><div>Hello</div>", styleSheet] baseURL:nil];
    Util::run(&didFinishLoad);

    RetainPtr printInfo = adoptNS([[NSPrintInfo alloc] init]);
    [printInfo setPaperSize:NSMakeSize(612, 792)];
    [printInfo setTopMargin:0];
    [printInfo setBottomMargin:0];
    [printInfo setLeftMargin:0];
    [printInfo setRightMargin:0];
    // AppKit centers the page in the printer's imageable area, which is off center when the
    // printer's top and bottom margins differ, and can move text near the top off the paper.
    [printInfo setHorizontallyCentered:NO];
    [printInfo setVerticallyCentered:NO];

    RetainPtr pdfURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"%@.pdf", NSUUID.UUID.UUIDString]] isDirectory:NO];
    [printInfo setJobDisposition:NSPrintSaveJob];
    [printInfo dictionary][NSPrintJobSavingURL] = pdfURL.get();

    RetainPtr operation = [[[webView mainFrame] frameView] printOperationWithPrintInfo:printInfo.get()];
    [operation setShowsPrintPanel:NO];
    [operation setShowsProgressPanel:NO];
    EXPECT_TRUE([operation runOperation]);

    RetainPtr data = adoptNS([[NSData alloc] initWithContentsOfURL:pdfURL.get()]);
    [NSFileManager.defaultManager removeItemAtURL:pdfURL.get() error:nil];
    EXPECT_NOT_NULL(data.get());
    if (!data)
        return nil;
    return adoptNS([[TestPDFDocument alloc] initFromData:data.get()]);
}

// Measured from the top left of the paper.
static CGPoint firstCharacterPositionInPrintedWebView(NSString *styleSheet)
{
    RetainPtr document = printWebViewToPDF(styleSheet);
    EXPECT_EQ([document pageCount], 1);
    RetainPtr page = [document pageAtIndex:0];
    CGRect characterRect = [page rectForCharacterAtIndex:0];
    return CGPointMake(CGRectGetMinX(characterRect), CGRectGetMaxY([page bounds]) - CGRectGetMaxY(characterRect));
}

TEST(WebKitLegacy, PrintedContentUsesCSSUnits)
{
    // 1in = 96px = 72pt.
    auto position = firstCharacterPositionInPrintedWebView(@"div { margin-left: 1in; padding-top: 1in }");
    auto referencePosition = firstCharacterPositionInPrintedWebView(@"");
    EXPECT_NEAR(position.x - referencePosition.x, 72, 0.05);
    EXPECT_NEAR(position.y - referencePosition.y, 72, 0.05);
}
#endif // HAVE(PDFKIT)

} // namespace TestWebKitAPI
