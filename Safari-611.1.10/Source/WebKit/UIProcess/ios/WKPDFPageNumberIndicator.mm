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
#import "WKPDFPageNumberIndicator.h"

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import <WebCore/LocalizedStrings.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

const CGFloat indicatorMargin = 20;
const CGFloat indicatorVerticalPadding = 6;
const CGFloat indicatorHorizontalPadding = 10;
const CGFloat indicatorCornerRadius = 7;
const CGFloat indicatorLabelOpacity = 0.4;
const CGFloat indicatorFontSize = 16;

const NSTimeInterval indicatorTimeout = 2;
const NSTimeInterval indicatorFadeDuration = 0.5;
const NSTimeInterval indicatorMoveDuration = 0.3;

@implementation WKPDFPageNumberIndicator {
    RetainPtr<UILabel> _label;
    RetainPtr<_UIBackdropView> _backdropView;
    RetainPtr<NSTimer> _timer;

    bool _hasValidPageCountAndCurrentPage;
}

- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;

    self.alpha = 0;
    self.layer.allowsGroupOpacity = NO;
    self.layer.allowsGroupBlending = NO;

    _UIBackdropViewSettings *backdropViewSettings = [_UIBackdropViewSettings settingsForPrivateStyle:_UIBackdropViewStyle_Light];
    backdropViewSettings.scale = 0.5;

    _backdropView = adoptNS([[_UIBackdropView alloc] initWithSettings:backdropViewSettings]);
    [_backdropView setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
    [self addSubview:_backdropView.get()];
    [self _makeRoundedCorners];

    _label = adoptNS([[UILabel alloc] initWithFrame:CGRectZero]);

    [_label setOpaque:NO];
    [_label setBackgroundColor:nil];
    [_label setTextAlignment:NSTextAlignmentCenter];
    [_label setFont:[UIFont boldSystemFontOfSize:indicatorFontSize]];
    [_label setTextColor:[UIColor blackColor]];
    [_label setAlpha:indicatorLabelOpacity];
    [_label setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
    [[_label layer] setCompositingFilter:[CAFilter filterWithType:kCAFilterPlusD]];

    [self addSubview:_label.get()];
    
    return self;
}

- (void)dealloc
{
    [_timer invalidate];
    [super dealloc];
}

- (void)setCurrentPageNumber:(unsigned)currentPageNumber
{
    if (_currentPageNumber == currentPageNumber)
        return;

    _currentPageNumber = currentPageNumber;
    [self _updateLabel];
}

- (void)setPageCount:(unsigned)pageCount
{
    if (_pageCount == pageCount)
        return;

    _pageCount = pageCount;
    [self _updateLabel];
}

- (void)show
{
    self.alpha = 1;

    if (_timer)
        [_timer setFireDate:[NSDate dateWithTimeIntervalSinceNow:indicatorTimeout]];
    else
        _timer = [NSTimer scheduledTimerWithTimeInterval:indicatorTimeout target:self selector:@selector(hide:) userInfo:nil repeats:NO];
}

- (void)hide
{
    [self hide:nil];
}

- (void)hide:(NSTimer *)timer
{
    [UIView animateWithDuration:indicatorFadeDuration animations:^{
        self.alpha = 0;
    }];

    [_timer invalidate];
    _timer = nullptr;
}

- (void)moveToPoint:(CGPoint)point animated:(BOOL)animated
{
    point.x += indicatorMargin;
    point.y += indicatorMargin;

    void (^animations)() = ^{
        self.frameOrigin = point;
    };
    if (animated)
        [UIView animateWithDuration:indicatorMoveDuration animations:animations];
    else
        animations();
}

- (CGSize)sizeThatFits:(CGSize)size
{
    CGSize labelSize = [_label sizeThatFits:[_label size]];
    labelSize.width += 2 * indicatorHorizontalPadding;
    labelSize.height += 2 * indicatorVerticalPadding;
    return labelSize;
}

- (void)_updateLabel
{
    [_label setText:[NSString localizedStringWithFormat:WEB_UI_STRING("%d of %d", "Label for PDF page number indicator."), _currentPageNumber, _pageCount]];
    [self sizeToFit];

    if (!_pageCount || !_currentPageNumber)
        return;

    // We don't want to show the indicator when the PDF loads, but rather on the first subsequent change.
    if (_hasValidPageCountAndCurrentPage)
        [self show];
    _hasValidPageCountAndCurrentPage = true;
}

- (void)_makeRoundedCorners
{
    CGSize cornerImageSize = CGSizeMake(2 * indicatorCornerRadius + 2, 2 * indicatorCornerRadius + 2);
    CGRect cornerImageBounds = CGRectMake(0, 0, cornerImageSize.width, cornerImageSize.height);

    UIGraphicsBeginImageContextWithOptions(cornerImageSize, NO, [UIScreen mainScreen].scale);

    CGContextRef context = UIGraphicsGetCurrentContext();
    CGContextSaveGState(context);
    CGContextAddRect(context, cornerImageBounds);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    UIBezierPath *cornerPath = [UIBezierPath bezierPathWithRoundedRect:cornerImageBounds byRoundingCorners:(UIRectCorner)UIAllCorners cornerRadii:CGSizeMake(indicatorCornerRadius, indicatorCornerRadius)];
    ALLOW_DEPRECATED_DECLARATIONS_END
    CGContextAddPath(context, [cornerPath CGPath]);
    CGContextEOClip(context);
    CGContextFillRect(context, cornerImageBounds);
    CGContextRestoreGState(context);

    UIImage *cornerImage = UIGraphicsGetImageFromCurrentImageContext();

    UIGraphicsEndImageContext();

    UIView *contentView = [_backdropView contentView];

    UIEdgeInsets capInsets = UIEdgeInsetsMake(indicatorCornerRadius, indicatorCornerRadius, indicatorCornerRadius, indicatorCornerRadius);
    cornerImage = [cornerImage resizableImageWithCapInsets:capInsets];

    RetainPtr<UIImageView> cornerMaskView = adoptNS([[UIImageView alloc] initWithImage:cornerImage]);
    [cornerMaskView setAlpha:0];
    [cornerMaskView _setBackdropMaskViewFlags:_UIBackdropMaskViewAll];
    [cornerMaskView setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
    [cornerMaskView setFrame:contentView.bounds];

    [contentView addSubview:cornerMaskView.get()];
}

@end

#endif /* PLATFORM(IOS_FAMILY) */
