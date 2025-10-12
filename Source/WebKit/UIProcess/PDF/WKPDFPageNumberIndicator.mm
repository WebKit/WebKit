/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)

#import "UIKitSPI.h"
#import "WKWebViewIOS.h"
#import <WebCore/GraphicsTypes.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/PlatformCAFilters.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/system/ios/UserInterfaceIdiom.h>
#import <wtf/CompletionHandler.h>
#import <wtf/RetainPtr.h>
#import <wtf/Seconds.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

static constexpr CGFloat indicatorFontSize = 16;
static constexpr CGFloat indicatorCornerRadius = 7;
static constexpr CGFloat indicatorMargin = 20;
static constexpr CGFloat indicatorVerticalPadding = 6;
static constexpr CGFloat indicatorHorizontalPadding = 10;

static constexpr Seconds indicatorTimeout { 2_s };
static constexpr Seconds indicatorFadeInDuration { 0.1_s };
static constexpr Seconds indicatorFadeOutDuration { 0.75_s };
static constexpr Seconds indicatorMoveDuration { 0.3_s };

@implementation WKPDFPageNumberIndicator {
    RetainPtr<UILabel> _label;
    RetainPtr<UIVisualEffectView> _backdropView;
    RetainPtr<NSTimer> _timer;
    WeakObjCPtr<WKWebView> _webView;
}

- (instancetype)initWithFrame:(CGRect)frame view:(WKWebView *)view pageCount:(size_t)pageCount
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;

    _webView = view;

    self.alpha = 0;
    self.userInteractionEnabled = NO;
    self.layer.allowsGroupOpacity = NO;
    self.layer.allowsGroupBlending = NO;

#if HAVE(UI_GLASS_EFFECT)
    bool shouldUseBlurEffectForBackdrop = PAL::currentUserInterfaceIdiomIsVision();
#else
    bool shouldUseBlurEffectForBackdrop = true;
#endif

    RetainPtr<UIVisualEffect> visualEffect;
    if (shouldUseBlurEffectForBackdrop)
        visualEffect = [UIBlurEffect effectWithStyle:UIBlurEffectStyleLight];
    else {
#if HAVE(UI_GLASS_EFFECT)
        visualEffect = adoptNS([[UIGlassEffect alloc] init]);
#endif
    }

    _backdropView =  adoptNS([[UIVisualEffectView alloc] initWithEffect:visualEffect.get()]);
    [_backdropView setFrame:self.bounds];
    [_backdropView setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
    [[_backdropView layer] setCornerRadius:indicatorCornerRadius];
    [_backdropView setClipsToBounds:YES];
    [self addSubview:_backdropView.get()];

    _label = adoptNS([[UILabel alloc] initWithFrame:self.bounds]);
    [_label setOpaque:NO];
    [_label setBackgroundColor:nil];
    [_label setTextAlignment:NSTextAlignmentCenter];
    [_label setFont:[UIFont boldSystemFontOfSize:indicatorFontSize]];
    if (shouldUseBlurEffectForBackdrop)
        [_label setTextColor:[UIColor blackColor]];
    [_label setAdjustsFontSizeToFitWidth:YES];
    [_label setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
    WebCore::PlatformCAFilters::setBlendingFiltersOnLayer([_label layer], WebCore::BlendMode::PlusDarker);
    [[_backdropView contentView] addSubview:_label.get()];

    [self updatePosition:self.frame];
    [self setPageCount:pageCount];

    return self;
}

- (void)dealloc
{
    [_timer invalidate];
    [super dealloc];
}

- (void)updatePosition:(CGRect)frame
{
    RetainPtr webView = _webView.get();
    if (!webView)
        return;

    UIEdgeInsets insets = UIEdgeInsetsAdd([webView _computedUnobscuredSafeAreaInset], [webView _computedObscuredInset], UIRectEdgeAll);
    [self _moveToPoint:UIEdgeInsetsInsetRect(frame, insets).origin animated:NO completionHandler:[view = retainPtr(self)](bool originMoved, bool finished) {
        if (!originMoved || !finished)
            return;
        [view sizeToFit];
        [view show];
    }];
}

- (void)setCurrentPageNumber:(unsigned)currentPageNumber
{
    ASSERT(currentPageNumber);
    bool labelTextShouldChange = _currentPageNumber != currentPageNumber;
    if (labelTextShouldChange)
        _currentPageNumber = currentPageNumber;
    [self _updateLabel:labelTextShouldChange];
}

- (void)setPageCount:(unsigned)pageCount
{
    ASSERT(pageCount);
    bool labelTextShouldChange = _pageCount != pageCount;
    if (labelTextShouldChange)
        _pageCount = pageCount;
    [self _updateLabel:labelTextShouldChange];
}

- (void)show
{
    [UIView animateWithDuration:indicatorFadeInDuration.seconds() animations:[view = retainPtr(self)] {
        [view setAlpha:1];
    }];

    if (_timer)
        [_timer setFireDate:[NSDate dateWithTimeIntervalSinceNow:indicatorTimeout.seconds()]];
    else
        _timer = [NSTimer scheduledTimerWithTimeInterval:indicatorTimeout.seconds() target:self selector:@selector(hide:) userInfo:nil repeats:NO];
}

- (void)hide:(NSTimer *)timer
{
    auto animations = [view = retainPtr(self)] {
        [view setAlpha:0];
    };
#if HAVE(UI_VIEW_ANIMATION_OPTION_FLUSH_UPDATES)
    static constexpr auto animationOptions = UIViewAnimationOptionFlushUpdates;
#else
    static constexpr auto animationOptions = 0;
#endif
    [UIView animateWithDuration:indicatorFadeOutDuration.seconds() delay:0 options:animationOptions animations:animations completion:nil];

    [std::exchange(_timer, nil) invalidate];
}

- (void)_moveToPoint:(CGPoint)point animated:(BOOL)animated completionHandler:(WTF::CompletionHandler<void(bool, bool)>&&)completionHandler
{
    point.x += indicatorMargin;
    point.y += indicatorMargin;

    bool originMoved = false;

    auto animations = [view = retainPtr(self), point, &originMoved] {
        CGRect frame = [view frame];
        originMoved = !CGPointEqualToPoint(frame.origin, point);
        if (!originMoved)
            return;
        frame.origin = point;
        [view setFrame:frame];
    };

    if (animated) {
        [UIView animateWithDuration:indicatorMoveDuration.seconds() animations:animations completion:makeBlockPtr([&originMoved, completionHandler = WTFMove(completionHandler)](BOOL finished) mutable {
            completionHandler(originMoved, static_cast<bool>(finished));
        }).get()];
    } else {
        animations();
        completionHandler(originMoved, true);
    }
}

- (CGSize)sizeThatFits:(CGSize)size
{
    CGSize labelSize = [_label sizeThatFits:[_label bounds].size];
    labelSize.width += 2 * indicatorHorizontalPadding;
    labelSize.height += 2 * indicatorVerticalPadding;
    return labelSize;
}

- (void)_updateLabel:(bool)labelTextShouldChange
{
    if (labelTextShouldChange)
        [_label setText:[NSString localizedStringWithFormat:WEB_UI_NSSTRING(@"%1$d of %2$d", "Label for PDF page number indicator."), _currentPageNumber, _pageCount]];
    [self sizeToFit];

    if (!_pageCount || !_currentPageNumber)
        return;

    [self show];
}

@end

#endif
