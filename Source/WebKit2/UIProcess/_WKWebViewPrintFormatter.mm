/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "_WKWebViewPrintFormatter.h"

#if PLATFORM(IOS)

#import "PrintInfo.h"
#import "WKWebViewInternal.h"
#import <wtf/RetainPtr.h>

@interface UIPrintFormatter (Details)
- (CGRect)_pageContentRect:(BOOL)firstPage;
- (void)_recalcIfNecessary;
@end

@interface _WKWebViewPrintFormatter ()
@property (nonatomic, readonly) WKWebView *webView;
@end

@implementation _WKWebViewPrintFormatter {
    double _totalScaleFactor;
    WebKit::PrintInfo _printInfo;
}

- (void)dealloc
{
    [self.webView _endPrinting];
    [super dealloc];
}

- (WKWebView *)webView
{
    ASSERT([self.view isKindOfClass:[WKWebView class]]);
    return static_cast<WKWebView *>(self.view);
}

- (NSInteger)_recalcPageCount
{
    ASSERT([self respondsToSelector:@selector(_pageContentRect:)]);

    CGRect firstRect = [self _pageContentRect:YES];
    CGRect nextRect = [self _pageContentRect:NO];
    if (CGRectIsEmpty(firstRect) || CGRectIsEmpty(nextRect))
        return 0;

    // The first page can have a smaller content rect than subsequent pages if a top content inset is specified. Since
    // WebKit requires a uniform content rect for each page during layout, use the first page rect for all pages if it's
    // smaller. This is what UIWebView's print formatter does.
    ASSERT(firstRect.size.width == nextRect.size.width);
    ASSERT(firstRect.origin.y >= nextRect.origin.y);
    _printInfo.pageSetupScaleFactor = 1;
    _printInfo.availablePaperWidth = nextRect.size.width;
    _printInfo.availablePaperHeight = nextRect.size.height - (firstRect.origin.y - nextRect.origin.y);

    return [self.webView _computePageCountAndStartDrawingToPDFForFrame:_frameToPrint printInfo:_printInfo firstPage:self.startPage computedTotalScaleFactor:_totalScaleFactor];
}

- (CGRect)rectForPageAtIndex:(NSInteger)pageIndex
{
    ASSERT([self respondsToSelector:@selector(_recalcIfNecessary)]);
    [self _recalcIfNecessary];
    return [self _pageContentRect:pageIndex == self.startPage];
}

- (void)drawInRect:(CGRect)rect forPageAtIndex:(NSInteger)pageIndex
{
    // CGPDFDocuments use 1-based page indexing.
    CGPDFPageRef pdfPage = CGPDFDocumentGetPage(self.webView._printedDocument, pageIndex - self.startPage + 1);

    CGContextRef context = UIGraphicsGetCurrentContext();
    CGContextSaveGState(context);

    CGContextTranslateCTM(context, rect.origin.x, rect.origin.y);
    CGContextScaleCTM(context, _totalScaleFactor, -_totalScaleFactor);
    CGContextTranslateCTM(context, 0, -CGPDFPageGetBoxRect(pdfPage, kCGPDFMediaBox).size.height);
    CGContextDrawPDFPage(context, pdfPage);

    CGContextRestoreGState(context);
}

@end

#endif // PLATFORM(IOS)
