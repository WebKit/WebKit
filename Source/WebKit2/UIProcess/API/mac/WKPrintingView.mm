/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#import "WKPrintingView.h"

#import "Logging.h"
#import "PrintInfo.h"
#import "WebPageProxy.h"

@interface NSView (WebNSViewDetails)
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView;
@end

using namespace WebKit;
using namespace WebCore;

NSString * const WebKitOriginalTopPrintingMarginKey = @"WebKitOriginalTopMargin";
NSString * const WebKitOriginalBottomPrintingMarginKey = @"WebKitOriginalBottomMargin";

@implementation WKPrintingView

- (id)initWithFrameProxy:(WebFrameProxy*)frame
{
    self = [super init]; // No frame rect to pass to NSView.
    if (!self)
        return nil;

    _webFrame = frame;

    return self;
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)_adjustPrintingMarginsForHeaderAndFooter
{
    NSPrintOperation *printOperation = [NSPrintOperation currentOperation];
    NSPrintInfo *info = [printOperation printInfo];
    NSMutableDictionary *infoDictionary = [info dictionary];

    // We need to modify the top and bottom margins in the NSPrintInfo to account for the space needed by the
    // header and footer. Because this method can be called more than once on the same NSPrintInfo (see 5038087),
    // we stash away the unmodified top and bottom margins the first time this method is called, and we read from
    // those stashed-away values on subsequent calls.
    double originalTopMargin;
    double originalBottomMargin;
    NSNumber *originalTopMarginNumber = [infoDictionary objectForKey:WebKitOriginalTopPrintingMarginKey];
    if (!originalTopMarginNumber) {
        ASSERT(![infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey]);
        originalTopMargin = [info topMargin];
        originalBottomMargin = [info bottomMargin];
        [infoDictionary setObject:[NSNumber numberWithDouble:originalTopMargin] forKey:WebKitOriginalTopPrintingMarginKey];
        [infoDictionary setObject:[NSNumber numberWithDouble:originalBottomMargin] forKey:WebKitOriginalBottomPrintingMarginKey];
    } else {
        ASSERT([originalTopMarginNumber isKindOfClass:[NSNumber class]]);
        ASSERT([[infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey] isKindOfClass:[NSNumber class]]);
        originalTopMargin = [originalTopMarginNumber doubleValue];
        originalBottomMargin = [[infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey] doubleValue];
    }
    
    CGFloat scale = [info scalingFactor];
    [info setTopMargin:originalTopMargin + _webFrame->page()->headerHeight(_webFrame.get()) * scale];
    [info setBottomMargin:originalBottomMargin + _webFrame->page()->footerHeight(_webFrame.get()) * scale];
}

- (BOOL)knowsPageRange:(NSRangePointer)range
{
    LOG(View, "knowsPageRange:");
    [self _adjustPrintingMarginsForHeaderAndFooter];

    _webFrame->page()->computePagesForPrinting(_webFrame.get(), PrintInfo([[NSPrintOperation currentOperation] printInfo]), _webPrintingPageRects, _webTotalScaleFactorForPrinting);

    *range = NSMakeRange(1, _webPrintingPageRects.size());
    return YES;
}

// Take over printing. AppKit applies incorrect clipping, and doesn't print pages beyond the first one.
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView
{
    LOG(View, "Printing rect x:%g, y:%g, width:%g, height:%g", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);

    ASSERT(self == visibleView);

    _webFrame->page()->beginPrinting(_webFrame.get(), PrintInfo([[NSPrintOperation currentOperation] printInfo]));

    // FIXME: This is optimized for print preview. Get the whole document at once when actually printing.
    Vector<uint8_t> pdfData;
    _webFrame->page()->drawRectToPDF(_webFrame.get(), IntRect(rect), pdfData);

    RetainPtr<CGDataProviderRef> pdfDataProvider(AdoptCF, CGDataProviderCreateWithData(0, pdfData.data(), pdfData.size(), 0));
    RetainPtr<CGPDFDocumentRef> pdfDocument(AdoptCF, CGPDFDocumentCreateWithProvider(pdfDataProvider.get()));
    if (!pdfDocument) {
        LOG_ERROR("Couldn't create a PDF document with data passed for printing");
        return;
    }

    CGPDFPageRef pdfPage = CGPDFDocumentGetPage(pdfDocument.get(), 1);
    if (!pdfPage) {
        LOG_ERROR("Printing data doesn't have page 1");
        return;
    }

    NSGraphicsContext *nsGraphicsContext = [NSGraphicsContext currentContext];
    CGContextRef context = static_cast<CGContextRef>([nsGraphicsContext graphicsPort]);

    CGContextSaveGState(context);
    // Flip the destination.
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, 0, -CGPDFPageGetBoxRect(pdfPage, kCGPDFMediaBox).size.height);
    CGContextDrawPDFPage(context, pdfPage);
    CGContextRestoreGState(context);
}

- (void)drawPageBorderWithSize:(NSSize)borderSize
{
    ASSERT(NSEqualSizes(borderSize, [[[NSPrintOperation currentOperation] printInfo] paperSize]));    

    // The header and footer rect height scales with the page, but the width is always
    // all the way across the printed page (inset by printing margins).
    NSPrintOperation *printOperation = [NSPrintOperation currentOperation];
    NSPrintInfo *printInfo = [printOperation printInfo];
    CGFloat scale = [printInfo scalingFactor];
    NSSize paperSize = [printInfo paperSize];
    CGFloat headerFooterLeft = [printInfo leftMargin] / scale;
    CGFloat headerFooterWidth = (paperSize.width - ([printInfo leftMargin] + [printInfo rightMargin])) / scale;
    NSRect footerRect = NSMakeRect(headerFooterLeft, [printInfo bottomMargin] / scale - _webFrame->page()->footerHeight(_webFrame.get()), headerFooterWidth, _webFrame->page()->footerHeight(_webFrame.get()));
    NSRect headerRect = NSMakeRect(headerFooterLeft, (paperSize.height - [printInfo topMargin]) / scale, headerFooterWidth, _webFrame->page()->headerHeight(_webFrame.get()));

    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    [currentContext saveGraphicsState];
    NSRectClip(headerRect);
    _webFrame->page()->drawHeader(_webFrame.get(), headerRect);
    [currentContext restoreGraphicsState];

    [currentContext saveGraphicsState];
    NSRectClip(footerRect);
    _webFrame->page()->drawFooter(_webFrame.get(), footerRect);
    [currentContext restoreGraphicsState];
}

// FIXME 3491344: This is an AppKit-internal method that we need to override in order
// to get our shrink-to-fit to work with a custom pagination scheme. We can do this better
// if AppKit makes it SPI/API.
- (CGFloat)_provideTotalScaleFactorForPrintOperation:(NSPrintOperation *)printOperation 
{
    return _webTotalScaleFactorForPrinting;
}

// Return the drawing rectangle for a particular page number
- (NSRect)rectForPage:(NSInteger)page
{
    LOG(View, "rectForPage:%d -> x %d, y %d, width %d, height %d\n", (int)page, _webPrintingPageRects[page - 1].x(), _webPrintingPageRects[page - 1].y(), _webPrintingPageRects[page - 1].width(), _webPrintingPageRects[page - 1].height());
    return _webPrintingPageRects[page - 1];
}

@end
