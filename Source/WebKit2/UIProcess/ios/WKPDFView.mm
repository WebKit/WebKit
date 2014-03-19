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
#import "WKPDFView.h"

#if PLATFORM(IOS)

#import <CorePDF/UIPDFDocument.h>
#import <CorePDF/UIPDFPageView.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

const CGFloat pdfPageMargin = 8;
const CGFloat pdfMinimumZoomScale = 1;
const CGFloat pdfMaximumZoomScale = 5;

@implementation WKPDFView {
    RetainPtr<UIPDFDocument> _pdfDocument;
    Vector<UIPDFPageView*> _pageViews;
    CGSize _minimumSize;
    UIScrollView *_scrollView;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    self.backgroundColor = [UIColor grayColor];

    return self;
}

- (void)web_setContentProviderData:(NSData *)data
{
    for (auto& pageView : _pageViews)
        [pageView removeFromSuperview];

    _pageViews.clear();

    RetainPtr<CGDataProvider> dataProvider = adoptCF(CGDataProviderCreateWithCFData((CFDataRef)data));
    RetainPtr<CGPDFDocumentRef> cgPDFDocument = adoptCF(CGPDFDocumentCreateWithProvider(dataProvider.get()));
    _pdfDocument = adoptNS([[UIPDFDocument alloc] initWithCGPDFDocument:cgPDFDocument.get()]);

    for (NSUInteger page = 0; page < [_pdfDocument numberOfPages]; ++page) {
        UIPDFPage *pdfPage = [_pdfDocument pageAtIndex:page];

        if (!pdfPage)
            continue;

        // FIXME: We should create and destroy views instead of depending on tiling.
        RetainPtr<UIPDFPageView> pageView = adoptNS([[UIPDFPageView alloc] initWithPage:pdfPage tiledContent:YES]);
        [self addSubview:pageView.get()];
        _pageViews.append(pageView.get());

        [pageView contentLayer].contentsScale = self.window.screen.scale;
    }

    [self layoutViews];
}

- (void)web_setMinimumSize:(CGSize)size
{
    _minimumSize = size;

    [self layoutViews];
}

- (void)web_setScrollView:(UIScrollView *)scrollView
{
    _scrollView = scrollView;
}

- (void)layoutViews
{
    CGRect pageFrame = CGRectMake(0, 0, _minimumSize.width, _minimumSize.height);

    for (auto& pageView : _pageViews) {
        CGSize pageSize = [pageView.page size];

        pageFrame.size.height = pageSize.height / pageSize.width * pageFrame.size.width;

        CGRect pageFrameWithMarginApplied = CGRectInset(pageFrame, pdfPageMargin, pdfPageMargin);
        [pageView setFrame:pageFrameWithMarginApplied];

        pageFrame.origin.y += pageFrame.size.height - pdfPageMargin;
    }

    CGRect newFrame = [self frame];
    newFrame.size.width = _minimumSize.width;
    newFrame.size.height = std::max(pageFrame.origin.y + pdfPageMargin, _minimumSize.height);
    [self setFrame:newFrame];

    [_scrollView setContentSize:newFrame.size];
    [_scrollView setMinimumZoomScale:pdfMinimumZoomScale];
    [_scrollView setMaximumZoomScale:pdfMaximumZoomScale];
}

@end

#endif /* PLATFORM(IOS) */
