/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ScrollbarsControllerMac.h"

#if PLATFORM(MAC)

#import "GraphicsLayer.h"
#import "Logging.h"
#import "NSScrollerImpDetails.h"
#import "ScrollAnimator.h"
#import "ScrollableArea.h"
#import "Scrollbar.h"
#import "ScrollbarMac.h"
#import "ScrollbarThemeMac.h"
#import "TimingFunction.h"
#import "WheelEventTestMonitor.h" // FIXME: This is  layering violation.

#import <pal/spi/mac/NSScrollerImpSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/NakedPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollbarsController);
WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollbarsControllerMac);

static ScrollbarThemeMac* macScrollbarTheme()
{
    ScrollbarTheme& scrollbarTheme = ScrollbarTheme::theme();
    return !scrollbarTheme.isMockTheme() ? static_cast<ScrollbarThemeMac*>(&scrollbarTheme) : nullptr;
}

static NSScrollerImp *scrollerImpForScrollbar(Scrollbar& scrollbar)
{
    return ScrollbarThemeMac::scrollerImpForScrollbar(scrollbar);
}

} // namespace WebCore

@interface WebScrollerImpPairDelegate : NSObject <NSScrollerImpPairDelegate> {
    WebCore::ScrollableArea* _scrollableArea;
}
- (id)initWithScrollableArea:(WebCore::ScrollableArea*)scrollableArea;

@end

@implementation WebScrollerImpPairDelegate

- (id)initWithScrollableArea:(WebCore::ScrollableArea*)scrollableArea
{
    self = [super init];
    if (!self)
        return nil;
    
    _scrollableArea = scrollableArea;
    return self;
}

- (void)invalidate
{
    _scrollableArea = nullptr;
}

- (NSRect)contentAreaRectForScrollerImpPair:(NSScrollerImpPair *)scrollerImpPair
{
    UNUSED_PARAM(scrollerImpPair);
    if (!_scrollableArea)
        return NSZeroRect;

    WebCore::IntSize contentsSize = _scrollableArea->contentsSize();
    return NSMakeRect(0, 0, contentsSize.width(), contentsSize.height());
}

- (BOOL)inLiveResizeForScrollerImpPair:(NSScrollerImpPair *)scrollerImpPair
{
    UNUSED_PARAM(scrollerImpPair);
    if (!_scrollableArea)
        return NO;

    return _scrollableArea->inLiveResize();
}

- (NSPoint)mouseLocationInContentAreaForScrollerImpPair:(NSScrollerImpPair *)scrollerImpPair
{
    UNUSED_PARAM(scrollerImpPair);
    if (!_scrollableArea)
        return NSZeroPoint;

    // It's OK that this position isn't relative to this scroller (which might be an overflow scroller).
    // AppKit just takes the result and passes it back to -scrollerImpPair:convertContentPoint:toScrollerImp:.
    return _scrollableArea->lastKnownMousePositionInView();
}

- (NSPoint)scrollerImpPair:(NSScrollerImpPair *)scrollerImpPair convertContentPoint:(NSPoint)pointInContentArea toScrollerImp:(NSScrollerImp *)scrollerImp
{
    UNUSED_PARAM(scrollerImpPair);

    if (!_scrollableArea || !scrollerImp)
        return NSZeroPoint;

    WebCore::Scrollbar* scrollbar = 0;
    if ([scrollerImp isHorizontal])
        scrollbar = _scrollableArea->horizontalScrollbar();
    else
        scrollbar = _scrollableArea->verticalScrollbar();

    // It is possible to have a null scrollbar here since it is possible for this delegate
    // method to be called between the moment when a scrollbar has been set to 0 and the
    // moment when its destructor has been called. We should probably de-couple some
    // of the clean-up work in ScrollbarThemeMac::unregisterScrollbar() to avoid this
    // issue.
    if (!scrollbar)
        return NSZeroPoint;

    ASSERT(scrollerImp == scrollerImpForScrollbar(*scrollbar));

    return scrollbar->convertFromContainingView(WebCore::roundedIntPoint(pointInContentArea));
}

- (void)scrollerImpPair:(NSScrollerImpPair *)scrollerImpPair setContentAreaNeedsDisplayInRect:(NSRect)rect
{
    UNUSED_PARAM(scrollerImpPair);
    UNUSED_PARAM(rect);

    if (!_scrollableArea)
        return;

    if ([scrollerImpPair overlayScrollerStateIsLocked])
        return;

    _scrollableArea->scrollbarsController().contentAreaWillPaint();
}

- (void)scrollerImpPair:(NSScrollerImpPair *)scrollerImpPair updateScrollerStyleForNewRecommendedScrollerStyle:(NSScrollerStyle)newRecommendedScrollerStyle
{
    if (!_scrollableArea)
        return;

    [scrollerImpPair setScrollerStyle:newRecommendedScrollerStyle];

    static_cast<WebCore::ScrollbarsControllerMac&>(_scrollableArea->scrollbarsController()).updateScrollerStyle();
}

@end

enum FeatureToAnimate {
    ThumbAlpha,
    TrackAlpha,
    UIStateTransition,
    ExpansionTransition
};

#if !LOG_DISABLED
static TextStream& operator<<(TextStream& ts, FeatureToAnimate feature)
{
    switch (feature) {
    case ThumbAlpha: ts << "ThumbAlpha" ; break;
    case TrackAlpha: ts << "TrackAlpha" ; break;
    case UIStateTransition: ts << "UIStateTransition" ; break;
    case ExpansionTransition: ts << "ExpansionTransition" ; break;
    }
    return ts;
}

using WebCore::LogOverlayScrollbars;

#endif

@interface WebScrollbarPartAnimation : NSObject {
    WebCore::Scrollbar* _scrollbar;
    RetainPtr<NSScrollerImp> _scrollerImp;
    FeatureToAnimate _featureToAnimate;
    CGFloat _startValue;
    CGFloat _endValue;
    NSTimeInterval _duration;
    RetainPtr<NSTimer> _timer;
    RetainPtr<NSDate> _startDate;
    RefPtr<WebCore::CubicBezierTimingFunction> _timingFunction;
}
- (id)initWithScrollbar:(WebCore::Scrollbar*)scrollbar featureToAnimate:(FeatureToAnimate)featureToAnimate animateFrom:(CGFloat)startValue animateTo:(CGFloat)endValue duration:(NSTimeInterval)duration;
- (void)setCurrentProgress:(NSTimer *)timer;
- (void)setDuration:(NSTimeInterval)duration;
- (void)stopAnimation;
@end

@implementation WebScrollbarPartAnimation

- (id)initWithScrollbar:(WebCore::Scrollbar*)scrollbar featureToAnimate:(FeatureToAnimate)featureToAnimate animateFrom:(CGFloat)startValue animateTo:(CGFloat)endValue duration:(NSTimeInterval)duration
{
    self = [super init];
    if (!self)
        return nil;

    const NSTimeInterval timeInterval = 0.01;
    _timer = adoptNS([[NSTimer alloc] initWithFireDate:[NSDate dateWithTimeIntervalSinceNow:0] interval:timeInterval target:self selector:@selector(setCurrentProgress:) userInfo:nil repeats:YES]);
    _duration = duration;
    _timingFunction = WebCore::CubicBezierTimingFunction::create(WebCore::CubicBezierTimingFunction::TimingFunctionPreset::EaseInOut);

    LOG_WITH_STREAM(OverlayScrollbars, stream << "Creating WebScrollbarPartAnimation for " << featureToAnimate << " from " << startValue << " to " << endValue);

    _scrollbar = scrollbar;
    _featureToAnimate = featureToAnimate;
    _startValue = startValue;
    _endValue = endValue;

    return self;
}

- (void)startAnimation
{
    ASSERT(_scrollbar);

    _scrollerImp = scrollerImpForScrollbar(*_scrollbar);

    LOG_WITH_STREAM(OverlayScrollbars, stream << "-[WebScrollbarPartAnimation " << self << "startAnimation] for " << _featureToAnimate);

    [[NSRunLoop mainRunLoop] addTimer:_timer.get() forMode:NSDefaultRunLoopMode];
    _startDate = adoptNS([[NSDate alloc] initWithTimeIntervalSinceNow:0]);
}

- (void)setStartValue:(CGFloat)startValue
{
    _startValue = startValue;
}

- (void)setEndValue:(CGFloat)endValue
{
    _endValue = endValue;
}

- (void)setCurrentProgress:(NSTimer *)timer
{
    CGFloat progress = 0;
    NSDate *now = [NSDate dateWithTimeIntervalSinceNow:0];
    NSTimeInterval elapsed = [now timeIntervalSinceDate:_startDate.get()];
    if (elapsed > _duration) {
        progress = 1;
        [timer invalidate];
    } else {
        NSTimeInterval t = 1;
        if (_duration)
            t = elapsed / _duration;
        progress = _timingFunction->transformProgress(t, _duration);
    }
    ASSERT(_scrollbar);

    LOG_WITH_STREAM(OverlayScrollbars, stream << "-[" << self << " setCurrentProgress: " << progress << "] for " << _featureToAnimate);

    CGFloat currentValue;
    if (_startValue > _endValue)
        currentValue = 1 - progress;
    else
        currentValue = progress;

    switch (_featureToAnimate) {
    case ThumbAlpha:
        [_scrollerImp setKnobAlpha:currentValue];
        break;
    case TrackAlpha:
        [_scrollerImp setTrackAlpha:currentValue];
        break;
    case UIStateTransition:
        [_scrollerImp setUiStateTransitionProgress:currentValue];
        break;
    case ExpansionTransition:
        [_scrollerImp setExpansionTransitionProgress:currentValue];
        break;
    }

    if (_scrollbar && !_scrollbar->supportsUpdateOnSecondaryThread())
        _scrollbar->invalidate();
}

- (void)invalidate
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [self stopAnimation];
    END_BLOCK_OBJC_EXCEPTIONS
    _scrollbar = nullptr;
}

- (void)setDuration:(NSTimeInterval)duration
{
    _duration = duration;
}

- (void)stopAnimation
{
    [_timer invalidate];
}

@end

@interface WebScrollerImpDelegate : NSObject<NSAnimationDelegate, NSScrollerImpDelegate> {
    SingleThreadWeakPtr<WebCore::Scrollbar> _scrollbar;

    RetainPtr<WebScrollbarPartAnimation> _knobAlphaAnimation;
    RetainPtr<WebScrollbarPartAnimation> _trackAlphaAnimation;
    RetainPtr<WebScrollbarPartAnimation> _uiStateTransitionAnimation;
    RetainPtr<WebScrollbarPartAnimation> _expansionTransitionAnimation;
}
- (id)initWithScrollbar:(WebCore::Scrollbar*)scrollbar;
- (void)cancelAnimations;
@end

@implementation WebScrollerImpDelegate

- (id)initWithScrollbar:(WebCore::Scrollbar*)scrollbar
{
    self = [super init];
    if (!self)
        return nil;
    
    _scrollbar = scrollbar;
    return self;
}

- (void)cancelAnimations
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [_knobAlphaAnimation stopAnimation];
    [_trackAlphaAnimation stopAnimation];
    [_uiStateTransitionAnimation stopAnimation];
    [_expansionTransitionAnimation stopAnimation];
    END_BLOCK_OBJC_EXCEPTIONS
}

- (NakedPtr<WebCore::ScrollbarsControllerMac>)scrollbarsController
{
    return &static_cast<WebCore::ScrollbarsControllerMac&>(_scrollbar->scrollableArea().scrollbarsController());
}

- (NSRect)convertRectToBacking:(NSRect)aRect
{
    return aRect;
}

- (NSRect)convertRectFromBacking:(NSRect)aRect
{
    return aRect;
}

- (CALayer *)layer
{
    if (!_scrollbar)
        return nil;

    if (!WebCore::ScrollbarThemeMac::isCurrentlyDrawingIntoLayer())
        return nil;

    WebCore::GraphicsLayer* layer;
    if (_scrollbar->orientation() == WebCore::ScrollbarOrientation::Vertical)
        layer = _scrollbar->scrollableArea().layerForVerticalScrollbar();
    else
        layer = _scrollbar->scrollableArea().layerForHorizontalScrollbar();

    static CALayer *dummyLayer = [[CALayer alloc] init];
    return layer ? layer->platformLayer() : dummyLayer;
}

- (NSPoint)mouseLocationInScrollerForScrollerImp:(NSScrollerImp *)scrollerImp
{
    if (!_scrollbar)
        return NSZeroPoint;

    ASSERT_UNUSED(scrollerImp, scrollerImp == scrollerImpForScrollbar(*_scrollbar));

    auto positionInView = _scrollbar->scrollableArea().lastKnownMousePositionInView();
    return _scrollbar->convertFromContainingView(positionInView);
}

- (NSRect)convertRectToLayer:(NSRect)rect
{
    return rect;
}

- (BOOL)shouldUseLayerPerPartForScrollerImp:(NSScrollerImp *)scrollerImp
{
    UNUSED_PARAM(scrollerImp);

    if (!_scrollbar)
        return false;

    return _scrollbar->supportsUpdateOnSecondaryThread();
}

- (NSAppearance *)effectiveAppearanceForScrollerImp:(NSScrollerImp *)scrollerImp
{
    UNUSED_PARAM(scrollerImp);

    if (!_scrollbar)
        return [NSAppearance currentDrawingAppearance];

    // Keep this in sync with LocalFrameView::paintScrollCorner.
    // The base system does not support dark Aqua, so we might get a null result.
    bool useDarkAppearance = _scrollbar->scrollableArea().useDarkAppearanceForScrollbars();
    if (auto *appearance = [NSAppearance appearanceNamed:useDarkAppearance ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua])
        return appearance;
    return [NSAppearance currentDrawingAppearance];
}

- (void)setUpAlphaAnimation:(RetainPtr<WebScrollbarPartAnimation>&)scrollbarPartAnimation scrollerPainter:(NSScrollerImp *)scrollerPainter part:(WebCore::ScrollbarPart)part animateAlphaTo:(CGFloat)newAlpha duration:(NSTimeInterval)duration
{
    // If the user has scrolled the page, then the scrollbars must be animated here.
    // This overrides the early returns.
    bool mustAnimate = ![self scrollbarsController]->scrollbarAnimationsUnsuspendedByUserInteraction();

    LOG_WITH_STREAM(OverlayScrollbars, stream << "WebScrollerImpDelegate for [" << _scrollbar->scrollableArea() << "] setUpAlphaAnimation: scrollbarAnimationsUnsuspendedByUserInteraction " << [self scrollbarsController]->scrollbarAnimationsUnsuspendedByUserInteraction() << " shouldSuspendScrollAnimations " << _scrollbar->scrollableArea().shouldSuspendScrollAnimations());

    if ([self scrollbarsController]->scrollbarPaintTimerIsActive() && !mustAnimate)
        return;

    if ([self scrollbarsController]->shouldSuspendScrollbarAnimations() && !mustAnimate) {
        [self scrollbarsController]->startScrollbarPaintTimer();
        return;
    }

    // At this point, we are definitely going to animate now, so stop the timer.
    [self scrollbarsController]->stopScrollbarPaintTimer();

    // If we are currently animating, stop
    if (scrollbarPartAnimation) {
        [scrollbarPartAnimation stopAnimation];
        scrollbarPartAnimation = nil;
    }

    if (auto* macTheme = WebCore::macScrollbarTheme())
        macTheme->setPaintCharacteristicsForScrollbar(*_scrollbar);

    if (part == WebCore::ThumbPart && _scrollbar->orientation() == WebCore::ScrollbarOrientation::Vertical) {
        if (newAlpha == 1) {
            auto thumbRect = WebCore::IntRect([scrollerPainter rectForPart:NSScrollerKnob]);
            [self scrollbarsController]->setVisibleScrollerThumbRect(thumbRect);
        } else
            [self scrollbarsController]->setVisibleScrollerThumbRect({ });
    }

    scrollbarPartAnimation = adoptNS([[WebScrollbarPartAnimation alloc] initWithScrollbar:_scrollbar.get()
                                                                       featureToAnimate:part == WebCore::ThumbPart ? ThumbAlpha : TrackAlpha
                                                                            animateFrom:part == WebCore::ThumbPart ? [scrollerPainter knobAlpha] : [scrollerPainter trackAlpha]
                                                                              animateTo:newAlpha
                                                                               duration:duration]);
    [scrollbarPartAnimation startAnimation];
}

- (void)scrollerImp:(NSScrollerImp *)scrollerImp animateKnobAlphaTo:(CGFloat)newKnobAlpha duration:(NSTimeInterval)duration
{
    if (!_scrollbar)
        return;

    ASSERT(scrollerImp == scrollerImpForScrollbar(*_scrollbar));

    NSScrollerImp *scrollerPainter = (NSScrollerImp *)scrollerImp;
    if (![self scrollbarsController]->scrollbarsCanBeActive()) {
        [scrollerImp setKnobAlpha:0];
        _scrollbar->invalidate();
        return;
    }

    // If we are fading the scrollbar away, that is a good indication that we are no longer going to
    // be moving it around on the scrolling thread. Calling [scrollerPainter setUsePresentationValue:NO]
    // will pass that information on to the NSScrollerImp API.
    if (newKnobAlpha == 0 && _scrollbar->supportsUpdateOnSecondaryThread())
        [scrollerPainter setUsePresentationValue:NO];

    [self setUpAlphaAnimation:_knobAlphaAnimation scrollerPainter:scrollerPainter part:WebCore::ThumbPart animateAlphaTo:newKnobAlpha duration:duration];
}

- (void)scrollerImp:(NSScrollerImp *)scrollerImp animateTrackAlphaTo:(CGFloat)newTrackAlpha duration:(NSTimeInterval)duration
{
    if (!_scrollbar)
        return;

    ASSERT(scrollerImp == scrollerImpForScrollbar(*_scrollbar));

    NSScrollerImp *scrollerPainter = (NSScrollerImp *)scrollerImp;
    [self setUpAlphaAnimation:_trackAlphaAnimation scrollerPainter:scrollerPainter part:WebCore::BackTrackPart animateAlphaTo:newTrackAlpha duration:duration];
}

- (void)scrollerImp:(NSScrollerImp *)scrollerImp animateUIStateTransitionWithDuration:(NSTimeInterval)duration
{
    if (!_scrollbar)
        return;

    ASSERT(scrollerImp == scrollerImpForScrollbar(*_scrollbar));

    // UIStateTransition always animates to 1. In case an animation is in progress this avoids a hard transition.
    [scrollerImp setUiStateTransitionProgress:1 - [scrollerImp uiStateTransitionProgress]];

    // If the UI state transition is happening, then we are no longer moving the scrollbar on the scrolling thread.
    if (_scrollbar->supportsUpdateOnSecondaryThread())
        [scrollerImp setUsePresentationValue:NO];

    if (!_uiStateTransitionAnimation) {
        _uiStateTransitionAnimation = adoptNS([[WebScrollbarPartAnimation alloc] initWithScrollbar:_scrollbar.get()
            featureToAnimate:UIStateTransition
            animateFrom:[scrollerImp uiStateTransitionProgress]
            animateTo:1.0
            duration:duration]);
    } else {
        // If we don't need to initialize the animation, just reset the values in case they have changed.
        [_uiStateTransitionAnimation setStartValue:[scrollerImp uiStateTransitionProgress]];
        [_uiStateTransitionAnimation setEndValue:1.0];
        [_uiStateTransitionAnimation setDuration:duration];
    }
    [_uiStateTransitionAnimation startAnimation];
}

- (void)scrollerImp:(NSScrollerImp *)scrollerImp animateExpansionTransitionWithDuration:(NSTimeInterval)duration
{
    if (!_scrollbar)
        return;

    ASSERT(scrollerImp == scrollerImpForScrollbar(*_scrollbar));

    // ExpansionTransition always animates to 1. In case an animation is in progress this avoids a hard transition.
    [scrollerImp setExpansionTransitionProgress:1 - [scrollerImp expansionTransitionProgress]];

    if (!_expansionTransitionAnimation) {
        _expansionTransitionAnimation = adoptNS([[WebScrollbarPartAnimation alloc] initWithScrollbar:_scrollbar.get()
            featureToAnimate:ExpansionTransition
            animateFrom:[scrollerImp expansionTransitionProgress]
            animateTo:1.0
            duration:duration]);
    } else {
        // If we don't need to initialize the animation, just reset the values in case they have changed.
        [_expansionTransitionAnimation setStartValue:[scrollerImp uiStateTransitionProgress]];
        [_expansionTransitionAnimation setEndValue:1.0];
        [_expansionTransitionAnimation setDuration:duration];
    }
    [_expansionTransitionAnimation startAnimation];
}

- (void)scrollerImp:(NSScrollerImp *)scrollerImp overlayScrollerStateChangedTo:(NSOverlayScrollerState)newOverlayScrollerState
{
    UNUSED_PARAM(scrollerImp);
    UNUSED_PARAM(newOverlayScrollerState);
}

- (void)invalidate
{
    _scrollbar = 0;
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [_knobAlphaAnimation invalidate];
    [_trackAlphaAnimation invalidate];
    [_uiStateTransitionAnimation invalidate];
    [_expansionTransitionAnimation invalidate];
    END_BLOCK_OBJC_EXCEPTIONS
}

@end

namespace WebCore {

std::unique_ptr<ScrollbarsController> ScrollbarsController::create(ScrollableArea& scrollableArea)
{
    return makeUnique<ScrollbarsControllerMac>(scrollableArea);
}

ScrollbarsControllerMac::ScrollbarsControllerMac(ScrollableArea& scrollableArea)
    : ScrollbarsController(scrollableArea)
    , m_initialScrollbarPaintTimer(*this, &ScrollbarsControllerMac::initialScrollbarPaintTimerFired)
    , m_sendContentAreaScrolledTimer(*this, &ScrollbarsControllerMac::sendContentAreaScrolledTimerFired)
{
    m_scrollerImpPairDelegate = adoptNS([[WebScrollerImpPairDelegate alloc] initWithScrollableArea:&scrollableArea]);
    m_scrollerImpPair = adoptNS([[NSScrollerImpPair alloc] init]);
    [m_scrollerImpPair setDelegate:m_scrollerImpPairDelegate.get()];
    [m_scrollerImpPair setScrollerStyle:ScrollerStyle::recommendedScrollerStyle()];
}

ScrollbarsControllerMac::~ScrollbarsControllerMac()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_scrollerImpPairDelegate invalidate];
    [m_scrollerImpPair setDelegate:nil];
    [m_horizontalScrollerImpDelegate invalidate];
    [m_verticalScrollerImpDelegate invalidate];
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollbarsControllerMac::cancelAnimations()
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollbarsControllerMac for [" << scrollableArea() << "] cancelAnimations");
    if (scrollbarPaintTimerIsActive())
        stopScrollbarPaintTimer();
    [m_horizontalScrollerImpDelegate cancelAnimations];
    [m_verticalScrollerImpDelegate cancelAnimations];

    ScrollbarsController::cancelAnimations();
}

void ScrollbarsControllerMac::setVisibleScrollerThumbRect(const IntRect& scrollerThumb)
{
    auto rectInViewCoordinates = scrollerThumb;
    if (auto* verticalScrollbar = scrollableArea().verticalScrollbar())
        rectInViewCoordinates = verticalScrollbar->convertToContainingView(scrollerThumb);

    if (rectInViewCoordinates == m_visibleScrollerThumbRect)
        return;

    scrollableArea().setVisibleScrollerThumbRect(rectInViewCoordinates);
    m_visibleScrollerThumbRect = rectInViewCoordinates;
}


void ScrollbarsControllerMac::contentAreaWillPaint() const
{
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [m_scrollerImpPair contentAreaWillDraw];
ALLOW_DEPRECATED_DECLARATIONS_END
}

void ScrollbarsControllerMac::mouseEnteredContentArea()
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollbarsControllerMac for [" << scrollableArea() << "] mouseEnteredContentArea");
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair mouseEnteredContentArea];
}

void ScrollbarsControllerMac::mouseExitedContentArea()
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollbarsControllerMac for [" << scrollableArea() << "] mouseExitedContentArea");
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair mouseExitedContentArea];
}

void ScrollbarsControllerMac::mouseMovedInContentArea()
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollbarsControllerMac for [" << scrollableArea() << "] mouseMovedInContentArea");
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair mouseMovedInContentArea];
}

void ScrollbarsControllerMac::mouseEnteredScrollbar(Scrollbar* scrollbar) const
{
    // At this time, only legacy scrollbars needs to send notifications here.
    if (ScrollerStyle::recommendedScrollerStyle() != NSScrollerStyleLegacy)
        return;

    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    if (NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar))
        [painter mouseEnteredScroller];
}

void ScrollbarsControllerMac::mouseExitedScrollbar(Scrollbar* scrollbar) const
{
    // At this time, only legacy scrollbars needs to send notifications here.
    if (ScrollerStyle::recommendedScrollerStyle() != NSScrollerStyleLegacy)
        return;

    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    if (NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar))
        [painter mouseExitedScroller];
}

void ScrollbarsControllerMac::mouseIsDownInScrollbar(Scrollbar* scrollbar, bool mouseIsDown) const
{
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    if (NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar)) {
        [painter setTracking:mouseIsDown];
        if (mouseIsDown)
            [m_scrollerImpPair beginScrollGesture];
        else
            [m_scrollerImpPair endScrollGesture];
    }
}

void ScrollbarsControllerMac::willStartLiveResize()
{
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair startLiveResize];
}

void ScrollbarsControllerMac::contentsSizeChanged() const
{
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair contentAreaDidResize];
}

void ScrollbarsControllerMac::willEndLiveResize()
{
    ScrollbarsController::willEndLiveResize();

    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair endLiveResize];
}

void ScrollbarsControllerMac::contentAreaDidShow()
{
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair windowOrderedIn];
}

void ScrollbarsControllerMac::contentAreaDidHide()
{
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair windowOrderedOut];
}

void ScrollbarsControllerMac::didBeginScrollGesture()
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollbarsControllerMac for [" << scrollableArea() << "] didBeginScrollGesture");

    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair beginScrollGesture];

    if (auto* monitor = wheelEventTestMonitor())
        monitor->deferForReason(scrollableArea().scrollingNodeIDForTesting(), WheelEventTestMonitor::DeferReason::ContentScrollInProgress);

    ScrollbarsController::didBeginScrollGesture();
}

void ScrollbarsControllerMac::didEndScrollGesture()
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollbarsControllerMac for [" << scrollableArea() << "] didEndScrollGesture");

    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair endScrollGesture];

    if (auto* monitor = wheelEventTestMonitor())
        monitor->removeDeferralForReason(scrollableArea().scrollingNodeIDForTesting(), WheelEventTestMonitor::DeferReason::ContentScrollInProgress);

    ScrollbarsController::didEndScrollGesture();
}

void ScrollbarsControllerMac::mayBeginScrollGesture()
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollbarsControllerMac for [" << scrollableArea() << "] mayBeginScrollGesture");

    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair beginScrollGesture];
    [m_scrollerImpPair contentAreaScrolled];

    ScrollbarsController::mayBeginScrollGesture();
}

void ScrollbarsControllerMac::lockOverlayScrollbarStateToHidden(bool shouldLockState)
{
    if (shouldLockState)
        [m_scrollerImpPair lockOverlayScrollerState:NSOverlayScrollerStateHidden];
    else {
        [m_scrollerImpPair unlockOverlayScrollerState];

        // We never update scroller style for PainterControllers that are locked. If we have a pending
        // need to update the style, do it once we've unlocked the scroller state.
        if (m_needsScrollerStyleUpdate)
            updateScrollerStyle();
    }
}

bool ScrollbarsControllerMac::scrollbarsCanBeActive() const
{
    return ![m_scrollerImpPair overlayScrollerStateIsLocked];
}

void ScrollbarsControllerMac::didAddVerticalScrollbar(Scrollbar* scrollbar)
{
    NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar);
    if (!painter)
        return;

    ASSERT(!m_verticalScrollerImpDelegate);
    m_verticalScrollerImpDelegate = adoptNS([[WebScrollerImpDelegate alloc] initWithScrollbar:scrollbar]);

    [painter setDelegate:m_verticalScrollerImpDelegate.get()];
    if (GraphicsLayer* layer = scrollbar->scrollableArea().layerForVerticalScrollbar())
        [painter setLayer:layer->platformLayer()];

    [m_scrollerImpPair setVerticalScrollerImp:painter];
    if (scrollableArea().inLiveResize())
        [painter setKnobAlpha:1];
}

void ScrollbarsControllerMac::willRemoveVerticalScrollbar(Scrollbar* scrollbar)
{
    NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar);
    if (!painter)
        return;

    ASSERT(m_verticalScrollerImpDelegate);
    [m_verticalScrollerImpDelegate invalidate];
    m_verticalScrollerImpDelegate = nullptr;

    [painter setDelegate:nil];
    [m_scrollerImpPair setVerticalScrollerImp:nil];
}

void ScrollbarsControllerMac::didAddHorizontalScrollbar(Scrollbar* scrollbar)
{
    NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar);
    if (!painter)
        return;

    ASSERT(!m_horizontalScrollerImpDelegate);
    m_horizontalScrollerImpDelegate = adoptNS([[WebScrollerImpDelegate alloc] initWithScrollbar:scrollbar]);

    [painter setDelegate:m_horizontalScrollerImpDelegate.get()];
    if (GraphicsLayer* layer = scrollbar->scrollableArea().layerForHorizontalScrollbar())
        [painter setLayer:layer->platformLayer()];

    [m_scrollerImpPair setHorizontalScrollerImp:painter];
    if (scrollableArea().inLiveResize())
        [painter setKnobAlpha:1];
}

void ScrollbarsControllerMac::willRemoveHorizontalScrollbar(Scrollbar* scrollbar)
{
    NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar);
    if (!painter)
        return;

    [m_horizontalScrollerImpDelegate invalidate];
    m_horizontalScrollerImpDelegate = nullptr;

    [painter setDelegate:nil];
    [m_scrollerImpPair setHorizontalScrollerImp:nil];
}

void ScrollbarsControllerMac::invalidateScrollbarPartLayers(Scrollbar* scrollbar)
{
    NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar);
    [painter setNeedsDisplay:YES];
}

void ScrollbarsControllerMac::verticalScrollbarLayerDidChange()
{
    GraphicsLayer* layer = scrollableArea().layerForVerticalScrollbar();
    Scrollbar* scrollbar = scrollableArea().verticalScrollbar();
    if (!scrollbar)
        return;

    NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar);
    if (!painter)
        return;

    [painter setLayer:layer ? layer->platformLayer() : nil];
}

void ScrollbarsControllerMac::horizontalScrollbarLayerDidChange()
{
    GraphicsLayer* layer = scrollableArea().layerForHorizontalScrollbar();
    Scrollbar* scrollbar = scrollableArea().horizontalScrollbar();
    if (!scrollbar)
        return;

    NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar);
    if (!painter)
        return;

    [painter setLayer:layer ? layer->platformLayer() : nil];
}

bool ScrollbarsControllerMac::shouldScrollbarParticipateInHitTesting(Scrollbar* scrollbar)
{
    // Non-overlay scrollbars should always participate in hit testing.
    if (ScrollerStyle::recommendedScrollerStyle() != NSScrollerStyleOverlay)
        return true;

    // Overlay scrollbars should participate in hit testing whenever they are at all visible.
    NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar);
    if (!painter)
        return false;
    return [painter knobAlpha] > 0;
}

void ScrollbarsControllerMac::notifyContentAreaScrolled(const FloatSize& delta)
{
    // This function is called when a page is going into the back/forward cache, but the page
    // isn't really scrolling in that case. We should only pass the message on to the
    // ScrollerImpPair when we're really scrolling on an active page.
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    if (scrollableArea().isHandlingWheelEvent())
        sendContentAreaScrolled(delta);
    else
        sendContentAreaScrolledSoon(delta);
}

void ScrollbarsControllerMac::updateScrollerStyle()
{
    if ([m_scrollerImpPair overlayScrollerStateIsLocked]) {
        m_needsScrollerStyleUpdate = true;
        return;
    }

    auto* macTheme = macScrollbarTheme();
    if (!macTheme) {
        m_needsScrollerStyleUpdate = false;
        return;
    }
    
    macTheme->usesOverlayScrollbarsChanged();

    NSScrollerStyle newStyle = [m_scrollerImpPair scrollerStyle];

    Scrollbar* verticalScrollbar = scrollableArea().verticalScrollbar();
    if (verticalScrollbar && !verticalScrollbar->isCustomScrollbar()) {
        verticalScrollbar->invalidate();

        NSScrollerImp *oldVerticalPainter = [m_scrollerImpPair verticalScrollerImp];
        auto* verticalScrollbarMac = dynamicDowncast<ScrollbarMac>(verticalScrollbar);
        verticalScrollbarMac->createScrollerImp(WTFMove(oldVerticalPainter));
        [m_scrollerImpPair setVerticalScrollerImp:verticalScrollbarMac->scrollerImp()];
    }

    Scrollbar* horizontalScrollbar = scrollableArea().horizontalScrollbar();
    if (horizontalScrollbar && !horizontalScrollbar->isCustomScrollbar()) {
        horizontalScrollbar->invalidate();

        NSScrollerImp *oldHorizontalPainter = [m_scrollerImpPair horizontalScrollerImp];
        auto* horizontalScrollbarMac = dynamicDowncast<ScrollbarMac>(horizontalScrollbar);
        horizontalScrollbarMac->createScrollerImp(WTFMove(oldHorizontalPainter));
        [m_scrollerImpPair setHorizontalScrollerImp:horizontalScrollbarMac->scrollerImp()];
    }

    // The different scrollbar styles have different thicknesses, so we must re-set the
    // frameRect to the new thickness, and the re-layout below will ensure the position
    // and length are properly updated.
    updateScrollbarsThickness();

    // If m_needsScrollerStyleUpdate is true, then the page is restoring from the back/forward cache, and
    // a relayout will happen on its own. Otherwise, we must initiate a re-layout ourselves.
    scrollableArea().scrollbarStyleChanged(newStyle == NSScrollerStyleOverlay ? ScrollbarStyle::Overlay : ScrollbarStyle::AlwaysVisible, !m_needsScrollerStyleUpdate);

    m_needsScrollerStyleUpdate = false;
}

void ScrollbarsControllerMac::startScrollbarPaintTimer()
{
    m_initialScrollbarPaintTimer.startOneShot(100_ms);
}

bool ScrollbarsControllerMac::scrollbarPaintTimerIsActive() const
{
    return m_initialScrollbarPaintTimer.isActive();
}

void ScrollbarsControllerMac::stopScrollbarPaintTimer()
{
    m_initialScrollbarPaintTimer.stop();
}

void ScrollbarsControllerMac::initialScrollbarPaintTimerFired()
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "WebScrollerImpDelegate for [" << scrollableArea() << "] initialScrollbarPaintTimerFired - flashing scrollers");

    // To force the scrollbars to flash, we have to call hide first. Otherwise, the ScrollerImpPair
    // might think that the scrollbars are already showing and bail early.
    [m_scrollerImpPair hideOverlayScrollers];
    [m_scrollerImpPair flashScrollers];
}

void ScrollbarsControllerMac::sendContentAreaScrolledTimerFired()
{
    sendContentAreaScrolled(m_contentAreaScrolledTimerScrollDelta);
    m_contentAreaScrolledTimerScrollDelta = { };

    if (auto* monitor = wheelEventTestMonitor())
        monitor->removeDeferralForReason(scrollableArea().scrollingNodeIDForTesting(), WheelEventTestMonitor::DeferReason::ContentScrollInProgress);
}

void ScrollbarsControllerMac::sendContentAreaScrolledSoon(const FloatSize& delta)
{
    m_contentAreaScrolledTimerScrollDelta = delta;

    if (!m_sendContentAreaScrolledTimer.isActive())
        m_sendContentAreaScrolledTimer.startOneShot(0_s);

    if (auto* monitor = wheelEventTestMonitor())
        monitor->deferForReason(scrollableArea().scrollingNodeIDForTesting(), WheelEventTestMonitor::DeferReason::ContentScrollInProgress);
}

void ScrollbarsControllerMac::sendContentAreaScrolled(const FloatSize& delta)
{
    [m_scrollerImpPair contentAreaScrolledInDirection:NSMakePoint(delta.width(), delta.height())];
}

void ScrollbarsControllerMac::scrollbarWidthChanged(WebCore::ScrollbarWidth)
{
    updateScrollbarsThickness();
    updateScrollerStyle();
}

static String scrollbarState(Scrollbar* scrollbar)
{
    if (!scrollbar)
        return "none"_s;

    StringBuilder result;
    result.append(scrollbar->enabled() ? "enabled"_s : "disabled"_s);

    if (!scrollbar->isOverlayScrollbar())
        return result.toString();

    NSScrollerImp *scrollerImp = scrollerImpForScrollbar(*scrollbar);
    if (!scrollerImp)
        return result.toString();

    if (scrollerImp.expanded)
        result.append(",expanded"_s);

    if (scrollerImp.trackAlpha > 0)
        result.append(",visible_track"_s);

    if (scrollerImp.knobAlpha > 0)
        result.append(",visible_thumb"_s);

    if (scrollerImp.userInterfaceLayoutDirection == NSUserInterfaceLayoutDirectionRightToLeft)
        result.append(",RTL"_s);

    if (scrollerImp.controlSize != NSControlSizeRegular)
        result.append(",thin"_s);

    return result.toString();
}

String ScrollbarsControllerMac::horizontalScrollbarStateForTesting() const
{
    return scrollbarState(scrollableArea().horizontalScrollbar());
}

String ScrollbarsControllerMac::verticalScrollbarStateForTesting() const
{
    return scrollbarState(scrollableArea().verticalScrollbar());
}

WheelEventTestMonitor* ScrollbarsControllerMac::wheelEventTestMonitor() const
{
    return scrollableArea().scrollAnimator().wheelEventTestMonitor();
}


} // namespace WebCore

#endif // PLATFORM(MAC)
