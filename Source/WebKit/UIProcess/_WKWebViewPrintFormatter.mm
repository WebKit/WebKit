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

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import "WKWebViewInternal.h"
#import "_WKFrameHandle.h"
#import <wtf/Condition.h>
#import <wtf/Locker.h>
#import <wtf/RetainPtr.h>
#import <wtf/SetForScope.h>

@interface UIPrintPageRenderer ()
@property (nonatomic) CGRect paperRect;
@property (nonatomic) CGRect printableRect;
@end

@implementation _WKWebViewPrintFormatter {
    RetainPtr<_WKFrameHandle> _frameToPrint;
    BOOL _suppressPageCountRecalc;

    Lock _printLock;
    Condition _printCompletionCondition;
    RetainPtr<CGPDFDocumentRef> _printedDocument;
    RetainPtr<CGImageRef> _printPreviewImage;
}

- (BOOL)requiresMainThread
{
    return self._webView._printProvider._wk_printFormatterRequiresMainThread;
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
    return checked_objc_cast<WKWebView>(self.view);
}

- (BOOL)_shouldDrawUsingBitmap
{
    if (self.snapshotFirstPage)
        return NO;

    if (![self._webView._printProvider respondsToSelector:@selector(_wk_requestImageForPrintFormatter:)])
        return NO;

    if (self.printPageRenderer.requestedRenderingQuality == UIPrintRenderingQualityBest)
        return NO;

    return YES;
}

- (CGPDFDocumentRef)_printedDocument
{
    if (self.requiresMainThread)
        return _printedDocument.get();

    Locker locker { _printLock };
    return _printedDocument.get();
}

- (void)_setPrintedDocument:(CGPDFDocumentRef)printedDocument
{
    if (self.requiresMainThread) {
        _printedDocument = printedDocument;
        return;
    }

    Locker locker { _printLock };
    _printedDocument = printedDocument;
    _printCompletionCondition.notifyOne();
}

- (CGImageRef)_printPreviewImage
{
    if (self.requiresMainThread)
        return _printPreviewImage.get();

    Locker locker { _printLock };
    return _printPreviewImage.get();
}

- (void)_setPrintPreviewImage:(CGImageRef)printPreviewImage
{
    if (self.requiresMainThread) {
        _printPreviewImage = printPreviewImage;
        return;
    }

    Locker locker { _printLock };
    _printPreviewImage = printPreviewImage;
    _printCompletionCondition.notifyOne();
}

- (void)_waitForPrintedDocumentOrImage
{
    Locker locker { _printLock };
    _printCompletionCondition.wait(_printLock);
}

- (void)_setSnapshotPaperRect:(CGRect)paperRect
{
    SetForScope suppressPageCountRecalc(_suppressPageCountRecalc, YES);
    UIPrintPageRenderer *printPageRenderer = self.printPageRenderer;
    printPageRenderer.paperRect = paperRect;
    printPageRenderer.printableRect = paperRect;
}

- (void)_invalidatePrintRenderingState
{
    [self _setPrintPreviewImage:nullptr];
    [self _setPrintedDocument:nullptr];
}

- (NSInteger)_recalcPageCount
{
    [self _invalidatePrintRenderingState];
    NSUInteger pageCount = [self._webView._printProvider _wk_pageCountForPrintFormatter:self];
    RELEASE_LOG(Printing, "Recalculated page count. Page count = %zu", pageCount);
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
    if ([self _shouldDrawUsingBitmap])
        [self _drawInRectUsingBitmap:rect forPageAtIndex:pageIndex];
    else
        [self _drawInRectUsingPDF:rect forPageAtIndex:pageIndex];
}

- (void)_drawInRectUsingBitmap:(CGRect)rect forPageAtIndex:(NSInteger)pageIndex
{
    RetainPtr printPreviewImage = [self _printPreviewImage];
    if (!printPreviewImage) {
        [self._webView._printProvider _wk_requestImageForPrintFormatter:self];
        printPreviewImage = [self _printPreviewImage];
        if (!printPreviewImage)
            return;
    }

    if (!self.pageCount)
        return;

    RetainPtr context = UIGraphicsGetCurrentContext();
    CGContextSaveGState(context.get());

    RetainPtr documentImage = _printPreviewImage.get();

    CGFloat pageImageWidth = CGImageGetWidth(documentImage.get());
    CGFloat pageImageHeight = CGImageGetHeight(documentImage.get()) / self.pageCount;

    if (!pageImageWidth || !pageImageHeight) {
        CGContextRestoreGState(context.get());
        return;
    }

    RetainPtr pageImage = adoptCF(CGImageCreateWithImageInRect(documentImage.get(), CGRectMake(0, pageIndex * pageImageHeight, pageImageWidth, pageImageHeight)));

    CGContextTranslateCTM(context.get(), CGRectGetMinX(rect), CGRectGetMaxY(rect));
    CGContextScaleCTM(context.get(), 1, -1);
    CGContextScaleCTM(context.get(), CGRectGetWidth(rect) / pageImageWidth, CGRectGetHeight(rect) / pageImageHeight);
    CGContextDrawImage(context.get(), CGRectMake(0, 0, pageImageWidth, pageImageHeight), pageImage.get());

    CGContextRestoreGState(context.get());
}

- (void)_drawInRectUsingPDF:(CGRect)rect forPageAtIndex:(NSInteger)pageIndex
{
    RetainPtr<CGPDFDocumentRef> printedDocument = [self _printedDocument];
    if (!printedDocument) {
        [self._webView._printProvider _wk_requestDocumentForPrintFormatter:self];
        printedDocument = [self _printedDocument];
        if (!printedDocument)
            return;
    }

    NSInteger offsetFromStartPage = pageIndex - self.startPage;
    if (offsetFromStartPage < 0)
        return;

    RetainPtr page = CGPDFDocumentGetPage(printedDocument.get(), offsetFromStartPage + 1);
    if (!page)
        return;

    RetainPtr context = UIGraphicsGetCurrentContext();
    CGContextSaveGState(context.get());

    CGContextTranslateCTM(context.get(), CGRectGetMinX(rect), CGRectGetMaxY(rect));
    CGContextScaleCTM(context.get(), 1, -1);
    CGContextConcatCTM(context.get(), CGPDFPageGetDrawingTransform(page.get(), kCGPDFCropBox, CGRectMake(0, 0, CGRectGetWidth(rect), CGRectGetHeight(rect)), 0, true));
    CGContextClipToRect(context.get(), CGPDFPageGetBoxRect(page.get(), kCGPDFCropBox));
    CGContextDrawPDFPage(context.get(), page.get());

    CGContextRestoreGState(context.get());
}

@end

#endif // PLATFORM(IOS_FAMILY)
