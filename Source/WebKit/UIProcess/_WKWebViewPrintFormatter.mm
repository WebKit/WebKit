/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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
#import "_WKWebViewPrintFormatterInternal.h"

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)

#import "WKWebViewInternal.h"
#import "_WKFrameHandle.h"
#import <wtf/RetainPtr.h>
#import <wtf/SetForScope.h>

@interface UIPrintPageRenderer ()
@property (nonatomic) CGRect paperRect;
@property (nonatomic) CGRect printableRect;
@end

@implementation _WKWebViewPrintFormatter {
    RetainPtr<_WKFrameHandle> _frameToPrint;
    RetainPtr<CGPDFDocumentRef> _printedDocument;
    BOOL _suppressPageCountRecalc;
}

- (_WKFrameHandle *)frameToPrint
{
    return _frameToPrint.get();
}

- (void)setFrameToPrint:(_WKFrameHandle *)frameToPrint
{
    _frameToPrint = frameToPrint;
}

- (WKWebView *)_webView
{
    UIView *view = self.view;
    ASSERT([view isKindOfClass:[WKWebView class]]);
    return static_cast<WKWebView *>(view);
}

- (void)_setSnapshotPaperRect:(CGRect)paperRect
{
    SetForScope<BOOL> suppressPageCountRecalc(_suppressPageCountRecalc, YES);
    UIPrintPageRenderer *printPageRenderer = self.printPageRenderer;
    printPageRenderer.paperRect = paperRect;
    printPageRenderer.printableRect = paperRect;
}

- (NSInteger)_recalcPageCount
{
    _printedDocument = nullptr;
    NSUInteger pageCount = [self._webView._printProvider _wk_pageCountForPrintFormatter:self];
    return std::min<NSUInteger>(pageCount, NSIntegerMax);
}

- (void)_setNeedsRecalc
{
    if (!_suppressPageCountRecalc)
        [super _setNeedsRecalc];
}

- (CGRect)rectForPageAtIndex:(NSInteger)pageIndex
{
    if (self.snapshotFirstPage)
        return self.printPageRenderer.paperRect;
    return [self _pageContentRect:pageIndex == self.startPage];
}

- (void)drawInRect:(CGRect)rect forPageAtIndex:(NSInteger)pageIndex
{
    if (!_printedDocument)
        _printedDocument = self._webView._printProvider._wk_printedDocument;

    NSInteger offsetFromStartPage = pageIndex - self.startPage;
    if (offsetFromStartPage < 0)
        return;

    CGPDFPageRef page = CGPDFDocumentGetPage(_printedDocument.get(), offsetFromStartPage + 1);
    if (!page)
        return;

    CGContextRef context = UIGraphicsGetCurrentContext();
    CGContextSaveGState(context);

    CGContextTranslateCTM(context, CGRectGetMinX(rect), CGRectGetMaxY(rect));
    CGContextScaleCTM(context, 1, -1);
    CGContextConcatCTM(context, CGPDFPageGetDrawingTransform(page, kCGPDFCropBox, CGRectMake(0, 0, CGRectGetWidth(rect), CGRectGetHeight(rect)), 0, true));
    CGContextClipToRect(context, CGPDFPageGetBoxRect(page, kCGPDFCropBox));
    CGContextDrawPDFPage(context, page);

    CGContextRestoreGState(context);
}

@end

#endif // PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
