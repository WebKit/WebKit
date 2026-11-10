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

#if HAVE(UIKIT_PRINTING)

#import "UIKitSPI.h"
#import "WKContentView.h"
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
    std::optional<UIEdgeInsets> _explicitPageMargin;
    RetainPtr<CGImageRef> _printPreviewImage;
}

- (BOOL)requiresMainThread
{
    return NO;
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

// The content view is never replaced after init, so this is safe off the main thread.
- (WKContentView *)_contentView
{
    WKWebView *webView = self._webView;
    return webView->_contentView.get();
}

- (BOOL)_shouldDrawUsingBitmap
{
    if (self.snapshotFirstPage)
        return NO;

    if (self.printPageRenderer.requestedRenderingQuality == UIPrintRenderingQualityBest)
        return NO;

    return YES;
}

- (CGPDFDocumentRef)_printedDocument
{
    Locker locker { _printLock };
    return _printedDocument.get();
}

- (void)_setPrintedDocument:(CGPDFDocumentRef)printedDocument
{
    Locker locker { _printLock };
    _printedDocument = printedDocument;
    _printCompletionCondition.notifyOne();
}

- (std::optional<UIEdgeInsets>)_explicitPageMargin
{
    Locker locker { _printLock };
    return _explicitPageMargin;
}

- (void)_setExplicitPageMargin:(std::optional<UIEdgeInsets>)explicitPageMargin
{
    Locker locker { _printLock };
    _explicitPageMargin = explicitPageMargin;
}

- (CGImageRef)_printPreviewImage
{
    Locker locker { _printLock };
    return _printPreviewImage.get();
}

- (void)_setPrintPreviewImage:(CGImageRef)printPreviewImage
{
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
    [self _setExplicitPageMargin:std::nullopt];
}

- (NSInteger)_recalcPageCount
{
    [self _invalidatePrintRenderingState];
    NSUInteger pageCount = [self._contentView _wk_pageCountForPrintFormatter:self];
    RELEASE_LOG(Printing, "Recalculated page count. Page count = %zu", static_cast<size_t>(pageCount));
    return std::min<NSUInteger>(pageCount, NSIntegerMax);
}

- (void)_setNeedsRecalc
{
    if (!_suppressPageCountRecalc)
        [super _setNeedsRecalc];
}

- (CGRect)rectForPageAtIndex:(NSInteger)pageIndex
{
    CGRect paperRect = self.printPageRenderer.paperRect;
    if (self.snapshotFirstPage)
        return paperRect;
    if (auto explicitPageMargin = [self _explicitPageMargin]; explicitPageMargin && !CGRectIsEmpty(paperRect)) {
        // Negative margins are shrunk to fit the paper. Margins that leave no page area fall back to UIKit's content rect.
        CGRect pageAreaRect = UIEdgeInsetsInsetRect(paperRect, *explicitPageMargin);
        if (pageAreaRect.size.width > 0 && pageAreaRect.size.height > 0)
            return CGRectIntersection(paperRect, pageAreaRect);
    }
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
        [self._contentView _wk_requestImageForPrintFormatter:self];
        printPreviewImage = [self _printPreviewImage];
        if (!printPreviewImage)
            return;
    }

    if (!self.pageCount)
        return;

    RetainPtr context = UIGraphicsGetCurrentContext();
    CGContextSaveGState(context.get());

    CGFloat pageImageWidth = CGImageGetWidth(printPreviewImage.get());
    CGFloat pageImageHeight = CGImageGetHeight(printPreviewImage.get()) / self.pageCount;

    if (!pageImageWidth || !pageImageHeight) {
        CGContextRestoreGState(context.get());
        return;
    }

    RetainPtr pageImage = adoptCF(CGImageCreateWithImageInRect(printPreviewImage.get(), CGRectMake(0, pageIndex * pageImageHeight, pageImageWidth, pageImageHeight)));

    CGFloat scale = std::min(CGRectGetWidth(rect) / pageImageWidth, CGRectGetHeight(rect) / pageImageHeight);
    CGContextTranslateCTM(context.get(), CGRectGetMinX(rect), CGRectGetMinY(rect) + scale * pageImageHeight);
    CGContextScaleCTM(context.get(), scale, -scale);
    CGContextDrawImage(context.get(), CGRectMake(0, 0, pageImageWidth, pageImageHeight), pageImage.get());

    CGContextRestoreGState(context.get());
}

- (void)_drawInRectUsingPDF:(CGRect)rect forPageAtIndex:(NSInteger)pageIndex
{
    RetainPtr<CGPDFDocumentRef> printedDocument = [self _printedDocument];
    if (!printedDocument) {
        [self._contentView _wk_requestDocumentForPrintFormatter:self];
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

    CGRect pageRect = CGPDFPageGetBoxRect(page.get(), kCGPDFCropBox);
    if (CGRectIsEmpty(pageRect))
        return;

    // Pages are in points, so only shrink pages that don't fit.
    CGFloat scale = std::min<CGFloat>({ 1, CGRectGetWidth(rect) / CGRectGetWidth(pageRect), CGRectGetHeight(rect) / CGRectGetHeight(pageRect) });

    RetainPtr context = UIGraphicsGetCurrentContext();
    CGContextSaveGState(context.get());

    CGContextTranslateCTM(context.get(), CGRectGetMinX(rect), CGRectGetMinY(rect) + scale * CGRectGetHeight(pageRect));
    CGContextScaleCTM(context.get(), scale, -scale);
    CGContextTranslateCTM(context.get(), -CGRectGetMinX(pageRect), -CGRectGetMinY(pageRect));
    CGContextClipToRect(context.get(), pageRect);
    CGContextDrawPDFPage(context.get(), page.get());

    CGContextRestoreGState(context.get());
}

@end

#endif // HAVE(UIKIT_PRINTING)
