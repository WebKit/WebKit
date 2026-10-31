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
#import "Helpers/cocoa/PrintingTestUtilities.h"

#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if PLATFORM(MAC) && HAVE(PDFKIT)
#import "Helpers/cocoa/TestPDFDocument.h"
#import <AppKit/AppKit.h>

@interface PrintOperationObserver : NSObject
- (void)waitForPrintOperation;
@end

@implementation PrintOperationObserver {
    bool _didRun;
}

- (void)printOperationDidRun:(NSPrintOperation *)printOperation success:(BOOL)success contextInfo:(void *)contextInfo
{
    EXPECT_TRUE(success);
    _didRun = true;
}

- (void)waitForPrintOperation
{
    TestWebKitAPI::Util::run(&_didRun);
}

@end
#endif // PLATFORM(MAC) && HAVE(PDFKIT)

namespace TestWebKitAPI {

RetainPtr<NSData> drawPagesToPDF(WKWebView *webView, _WKFrameHandle *frame, NSUInteger pageCount)
{
    __block bool computedPages = false;
    [webView _computePagesForPrinting:frame completionHandler:^{
        computedPages = true;
    }];
    Util::run(&computedPages);

    return [webView _drawPagesToPDFSynchronouslyForTesting:frame pageCount:pageCount];
}

// Unlike TestPDFPage's colorAtPoint:, this can tell apart colors at different heights.
std::optional<std::array<uint8_t, 3>> pdfPageColorAtCSSPixel(NSData *data, size_t pageNumber, CGPoint pointInCSSPixels)
{
    RetainPtr provider = adoptCF(CGDataProviderCreateWithCFData(bridge_cast(data)));
    RetainPtr document = adoptCF(CGPDFDocumentCreateWithProvider(provider.get()));
    CGPDFPageRef page = CGPDFDocumentGetPage(document.get(), pageNumber);
    if (!page)
        return std::nullopt;
    auto bounds = CGPDFPageGetBoxRect(page, kCGPDFMediaBox);
    size_t width = bounds.size.width;
    size_t height = bounds.size.height;
    RetainPtr colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    RetainPtr context = adoptCF(CGBitmapContextCreate(nullptr, width, height, 8, width * 4, colorSpace.get(), kCGImageAlphaPremultipliedLast));
    CGContextSetRGBFillColor(context.get(), 1, 1, 1, 1);
    CGContextFillRect(context.get(), CGRectMake(0, 0, width, height));
    CGContextDrawPDFPage(context.get(), page);
    auto pixels = unsafeMakeSpan(static_cast<const uint8_t*>(CGBitmapContextGetData(context.get())), width * height * 4);
    size_t x = pointsFromCSSPixels(pointInCSSPixels.x);
    size_t y = pointsFromCSSPixels(pointInCSSPixels.y);
    auto pixel = pixels.subspan((y * width + x) * 4, 3);
    return std::array { pixel[0], pixel[1], pixel[2] };
}

bool pdfPageHasColorAtCSSPixel(NSData *data, size_t pageNumber, CGPoint pointInCSSPixels, std::array<uint8_t, 3> color, uint8_t tolerance)
{
    auto actualColor = pdfPageColorAtCSSPixel(data, pageNumber, pointInCSSPixels);
    if (!actualColor)
        return false;
    for (size_t i = 0; i < 3; ++i) {
        if (std::abs((*actualColor)[i] - color[i]) > tolerance)
            return false;
    }
    return true;
}

#if PLATFORM(MAC) && HAVE(PDFKIT)

RetainPtr<TestPDFDocument> printToPDF(MakePrintOperation makePrintOperation, NSWindow *window)
{
    RetainPtr printInfo = adoptNS([[NSPrintInfo alloc] init]);
    [printInfo setPaperSize:usLetterPaperSizeInPoints];

    RetainPtr pdfURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"%@.pdf", NSUUID.UUID.UUIDString]] isDirectory:NO];
    [printInfo setJobDisposition:NSPrintSaveJob];
    [printInfo dictionary][NSPrintJobSavingURL] = pdfURL.get();

    RetainPtr operation = makePrintOperation(printInfo.get());
    [operation setShowsPrintPanel:NO];
    [operation setShowsProgressPanel:NO];

    if (window) {
        RetainPtr observer = adoptNS([PrintOperationObserver new]);
        [operation runOperationModalForWindow:window delegate:observer.get() didRunSelector:@selector(printOperationDidRun:success:contextInfo:) contextInfo:nullptr];
        [observer waitForPrintOperation];
    } else
        EXPECT_TRUE([operation runOperation]);

    RetainPtr data = adoptNS([[NSData alloc] initWithContentsOfURL:pdfURL.get()]);
    [NSFileManager.defaultManager removeItemAtURL:pdfURL.get() error:nil];
    EXPECT_NOT_NULL(data.get());
    if (!data)
        return nil;
    return adoptNS([[TestPDFDocument alloc] initFromData:data.get()]);
}

CGPoint firstCharacterPosition(TestPDFDocument *document)
{
    EXPECT_EQ([document pageCount], 1);
    RetainPtr page = [document pageAtIndex:0];
    CGRect characterRect = [page rectForCharacterAtIndex:0];
    return CGPointMake(CGRectGetMinX(characterRect), CGRectGetMaxY([page bounds]) - CGRectGetMaxY(characterRect));
}

#endif // PLATFORM(MAC) && HAVE(PDFKIT)

} // namespace TestWebKitAPI
