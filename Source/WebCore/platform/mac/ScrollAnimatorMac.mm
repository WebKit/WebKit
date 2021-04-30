/*
 * Copyright (C) 2010, 2011, 2015 Apple Inc. All rights reserved.
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
#import "ScrollAnimatorMac.h"

#if ENABLE(SMOOTH_SCROLLING)

#import "Gradient.h"
#import "GraphicsLayer.h"
#import "Logging.h"
#import "NSScrollerImpDetails.h"
#import "PlatformWheelEvent.h"
#import "ScrollView.h"
#import "ScrollableArea.h"
#import "ScrollbarTheme.h"
#import "ScrollbarThemeMac.h"
#import <pal/spi/mac/NSScrollerImpSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/NakedPtr.h>
#import <wtf/text/TextStream.h>

namespace WebCore {

static ScrollbarThemeMac* macScrollbarTheme()
{
    ScrollbarTheme& scrollbarTheme = ScrollbarTheme::theme();
    return !scrollbarTheme.isMockTheme() ? static_cast<ScrollbarThemeMac*>(&scrollbarTheme) : nullptr;
}

static NSScrollerImp *scrollerImpForScrollbar(Scrollbar& scrollbar)
{
    return ScrollbarThemeMac::painterForScrollbar(scrollbar);
}

} // namespace WebCore

using WebCore::ScrollableArea;
using WebCore::ScrollAnimatorMac;
using WebCore::Scrollbar;
using WebCore::ScrollbarThemeMac;
using WebCore::GraphicsLayer;
using WebCore::VerticalScrollbar;
using WebCore::macScrollbarTheme;
using WebCore::IntRect;
using WebCore::ThumbPart;
using WebCore::CubicBezierTimingFunction;

#if !LOG_DISABLED
using WebCore::LogOverlayScrollbars;
#endif

@interface NSObject (ScrollAnimationHelperDetails)
- (id)initWithDelegate:(id)delegate;
- (void)_stopRun;
- (BOOL)_isAnimating;
- (NSPoint)targetOrigin;
- (CGFloat)_progress;
@end

@interface WebScrollAnimationHelperDelegate : NSObject
{
    NakedPtr<WebCore::ScrollAnimatorMac> _animator;
}
- (id)initWithScrollAnimator:(NakedPtr<WebCore::ScrollAnimatorMac>)scrollAnimator;
@end

static NSSize abs(NSSize size)
{
    NSSize finalSize = size;
    if (finalSize.width < 0)
        finalSize.width = -finalSize.width;
    if (finalSize.height < 0)
        finalSize.height = -finalSize.height;
    return finalSize;    
}

@implementation WebScrollAnimationHelperDelegate

- (id)initWithScrollAnimator:(NakedPtr<WebCore::ScrollAnimatorMac>)scrollAnimator
{
    self = [super init];
    if (!self)
        return nil;

    _animator = scrollAnimator;
    return self;
}

- (void)invalidate
{
    _animator = nullptr;
}

- (NSRect)bounds
{
    if (!_animator)
        return NSZeroRect;

    WebCore::FloatPoint currentPosition = _animator->currentPosition();
    return NSMakeRect(currentPosition.x(), currentPosition.y(), 0, 0);
}

- (void)_immediateScrollToPoint:(NSPoint)newPosition
{
    if (!_animator)
        return;
    _animator->immediateScrollToPositionForScrollAnimation(newPosition);
}

- (NSPoint)_pixelAlignProposedScrollPosition:(NSPoint)newOrigin
{
    return newOrigin;
}

- (NSSize)convertSizeToBase:(NSSize)size
{
    return abs(size);
}

- (NSSize)convertSizeFromBase:(NSSize)size
{
    return abs(size);
}

- (NSSize)convertSizeToBacking:(NSSize)size
{
    return abs(size);
}

- (NSSize)convertSizeFromBacking:(NSSize)size
{
    return abs(size);
}

- (id)superview
{
    return nil;
}

- (id)documentView
{
    return nil;
}

- (id)window
{
    return nil;
}

- (void)_recursiveRecomputeToolTips
{
}

@end

@interface WebScrollerImpPairDelegate : NSObject <NSScrollerImpPairDelegate>
{
    ScrollableArea* _scrollableArea;
}
- (id)initWithScrollableArea:(ScrollableArea*)scrollableArea;
@end

@implementation WebScrollerImpPairDelegate

- (id)initWithScrollableArea:(ScrollableArea*)scrollableArea
{
    self = [super init];
    if (!self)
        return nil;
    
    _scrollableArea = scrollableArea;
    return self;
}

- (void)invalidate
{
    _scrollableArea = 0;
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

    _scrollableArea->scrollAnimator().contentAreaWillPaint();
}

- (void)scrollerImpPair:(NSScrollerImpPair *)scrollerImpPair updateScrollerStyleForNewRecommendedScrollerStyle:(NSScrollerStyle)newRecommendedScrollerStyle
{
    if (!_scrollableArea)
        return;

    [scrollerImpPair setScrollerStyle:newRecommendedScrollerStyle];

    static_cast<ScrollAnimatorMac&>(_scrollableArea->scrollAnimator()).updateScrollerStyle();
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
#endif

@interface WebScrollbarPartAnimation : NSObject
{
    Scrollbar* _scrollbar;
    RetainPtr<NSScrollerImp> _scrollerImp;
    FeatureToAnimate _featureToAnimate;
    CGFloat _startValue;
    CGFloat _endValue;
    NSTimeInterval _duration;
    RetainPtr<NSTimer> _timer;
    RetainPtr<NSDate> _startDate;
    RefPtr<CubicBezierTimingFunction> _timingFunction;
}
- (id)initWithScrollbar:(Scrollbar*)scrollbar featureToAnimate:(FeatureToAnimate)featureToAnimate animateFrom:(CGFloat)startValue animateTo:(CGFloat)endValue duration:(NSTimeInterval)duration;
- (void)setCurrentProgress:(NSTimer *)timer;
- (void)setDuration:(NSTimeInterval)duration;
- (void)stopAnimation;
@end

@implementation WebScrollbarPartAnimation

- (id)initWithScrollbar:(Scrollbar*)scrollbar featureToAnimate:(FeatureToAnimate)featureToAnimate animateFrom:(CGFloat)startValue animateTo:(CGFloat)endValue duration:(NSTimeInterval)duration
{
    self = [super init];
    if (!self)
        return nil;

    const NSTimeInterval timeInterval = 0.01;
    _timer = adoptNS([[NSTimer alloc] initWithFireDate:[NSDate dateWithTimeIntervalSinceNow:0] interval:timeInterval target:self selector:@selector(setCurrentProgress:) userInfo:nil repeats:YES]);
    _duration = duration;
    _timingFunction = CubicBezierTimingFunction::create(CubicBezierTimingFunction::EaseInOut);

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
        progress = _timingFunction->transformTime(t, _duration);
    }
    ASSERT(_scrollbar);

    LOG_WITH_STREAM(OverlayScrollbars, stream << "-[WebScrollbarPartAnimation " << self << "setCurrentProgress:" << progress <<"] for " << _featureToAnimate);

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

    if (!_scrollbar->supportsUpdateOnSecondaryThread())
        _scrollbar->invalidate();
}

- (void)invalidate
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [self stopAnimation];
    END_BLOCK_OBJC_EXCEPTIONS
    _scrollbar = 0;
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

@interface WebScrollerImpDelegate : NSObject<NSAnimationDelegate, NSScrollerImpDelegate>
{
    WebCore::Scrollbar* _scrollbar;

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

- (NakedPtr<ScrollAnimatorMac>)scrollAnimator
{
    return &static_cast<ScrollAnimatorMac&>(_scrollbar->scrollableArea().scrollAnimator());
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

    if (!ScrollbarThemeMac::isCurrentlyDrawingIntoLayer())
        return nil;

    GraphicsLayer* layer;
    if (_scrollbar->orientation() == VerticalScrollbar)
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

#if HAVE(OS_DARK_MODE_SUPPORT)
- (NSAppearance *)effectiveAppearanceForScrollerImp:(NSScrollerImp *)scrollerImp
{
    UNUSED_PARAM(scrollerImp);

    if (!_scrollbar)
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        return [NSAppearance currentAppearance];
        ALLOW_DEPRECATED_DECLARATIONS_END

    // Keep this in sync with FrameView::paintScrollCorner.
    // The base system does not support dark Aqua, so we might get a null result.
    bool useDarkAppearance = _scrollbar->scrollableArea().useDarkAppearanceForScrollbars();
    if (auto *appearance = [NSAppearance appearanceNamed:useDarkAppearance ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua])
        return appearance;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [NSAppearance currentAppearance];
    ALLOW_DEPRECATED_DECLARATIONS_END
}
#endif

- (void)setUpAlphaAnimation:(RetainPtr<WebScrollbarPartAnimation>&)scrollbarPartAnimation scrollerPainter:(NSScrollerImp *)scrollerPainter part:(WebCore::ScrollbarPart)part animateAlphaTo:(CGFloat)newAlpha duration:(NSTimeInterval)duration
{
    // If the user has scrolled the page, then the scrollbars must be animated here.
    // This overrides the early returns.
    bool mustAnimate = [self scrollAnimator]->haveScrolledSincePageLoad();

    if ([self scrollAnimator]->scrollbarPaintTimerIsActive() && !mustAnimate)
        return;

    if (_scrollbar->scrollableArea().shouldSuspendScrollAnimations() && !mustAnimate) {
        [self scrollAnimator]->startScrollbarPaintTimer();
        return;
    }

    // At this point, we are definitely going to animate now, so stop the timer.
    [self scrollAnimator]->stopScrollbarPaintTimer();

    // If we are currently animating, stop
    if (scrollbarPartAnimation) {
        [scrollbarPartAnimation stopAnimation];
        scrollbarPartAnimation = nil;
    }

    if (ScrollbarThemeMac* macTheme = macScrollbarTheme())
        macTheme->setPaintCharacteristicsForScrollbar(*_scrollbar);

    if (part == WebCore::ThumbPart && _scrollbar->orientation() == VerticalScrollbar) {
        if (newAlpha == 1) {
            IntRect thumbRect = IntRect([scrollerPainter rectForPart:NSScrollerKnob]);
            [self scrollAnimator]->setVisibleScrollerThumbRect(thumbRect);
        } else
            [self scrollAnimator]->setVisibleScrollerThumbRect(IntRect());
    }

    scrollbarPartAnimation = adoptNS([[WebScrollbarPartAnimation alloc] initWithScrollbar:_scrollbar
                                                                       featureToAnimate:part == ThumbPart ? ThumbAlpha : TrackAlpha
                                                                            animateFrom:part == ThumbPart ? [scrollerPainter knobAlpha] : [scrollerPainter trackAlpha]
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
    if (![self scrollAnimator]->scrollbarsCanBeActive()) {
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

    if (!_uiStateTransitionAnimation)
        _uiStateTransitionAnimation = adoptNS([[WebScrollbarPartAnimation alloc] initWithScrollbar:_scrollbar
            featureToAnimate:UIStateTransition
            animateFrom:[scrollerImp uiStateTransitionProgress]
            animateTo:1.0
            duration:duration]);
    else {
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
        _expansionTransitionAnimation = adoptNS([[WebScrollbarPartAnimation alloc] initWithScrollbar:_scrollbar
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

std::unique_ptr<ScrollAnimator> ScrollAnimator::create(ScrollableArea& scrollableArea)
{
    return makeUnique<ScrollAnimatorMac>(scrollableArea);
}

ScrollAnimatorMac::ScrollAnimatorMac(ScrollableArea& scrollableArea)
    : ScrollAnimator(scrollableArea)
    , m_initialScrollbarPaintTimer(*this, &ScrollAnimatorMac::initialScrollbarPaintTimerFired)
    , m_sendContentAreaScrolledTimer(*this, &ScrollAnimatorMac::sendContentAreaScrolledTimerFired)
    , m_haveScrolledSincePageLoad(false)
    , m_needsScrollerStyleUpdate(false)
{
    m_scrollAnimationHelperDelegate = adoptNS([[WebScrollAnimationHelperDelegate alloc] initWithScrollAnimator:this]);
    m_scrollAnimationHelper = adoptNS([[NSClassFromString(@"NSScrollAnimationHelper") alloc] initWithDelegate:m_scrollAnimationHelperDelegate.get()]);

    m_scrollerImpPairDelegate = adoptNS([[WebScrollerImpPairDelegate alloc] initWithScrollableArea:&scrollableArea]);
    m_scrollerImpPair = adoptNS([[NSScrollerImpPair alloc] init]);
    [m_scrollerImpPair setDelegate:m_scrollerImpPairDelegate.get()];
    [m_scrollerImpPair setScrollerStyle:ScrollerStyle::recommendedScrollerStyle()];
}

ScrollAnimatorMac::~ScrollAnimatorMac()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_scrollerImpPairDelegate invalidate];
    [m_scrollerImpPair setDelegate:nil];
    [m_horizontalScrollerImpDelegate invalidate];
    [m_verticalScrollerImpDelegate invalidate];
    [m_scrollAnimationHelperDelegate invalidate];
    END_BLOCK_OBJC_EXCEPTIONS
}

static bool scrollAnimationEnabledForSystem()
{
    NSString* scrollAnimationDefaultsKey = @"NSScrollAnimationEnabled";
    static bool enabled = [[NSUserDefaults standardUserDefaults] boolForKey:scrollAnimationDefaultsKey];
    return enabled;
}

#if ENABLE(RUBBER_BANDING)
static bool rubberBandingEnabledForSystem()
{
    static bool initialized = false;
    static bool enabled = true;
    // Caches the result, which is consistent with other apps like the Finder, which all
    // require a restart after changing this default.
    if (!initialized) {
        // Uses -objectForKey: and not -boolForKey: in order to default to true if the value wasn't set.
        id value = [[NSUserDefaults standardUserDefaults] objectForKey:@"NSScrollViewRubberbanding"];
        if ([value isKindOfClass:[NSNumber class]])
            enabled = [value boolValue];
        initialized = true;
    }
    return enabled;
}
#endif

bool ScrollAnimatorMac::scroll(ScrollbarOrientation orientation, ScrollGranularity granularity, float step, float multiplier, OptionSet<ScrollBehavior> behavior)
{
    m_haveScrolledSincePageLoad = true;

    // This method doesn't do directional snapping, but our base class does. It will call into
    // ScrollAnimatorMac::scroll again with the snapped positions and ScrollBehavior::Default.
    if (behavior.contains(ScrollBehavior::DoDirectionalSnapping))
        return ScrollAnimator::scroll(orientation, granularity, step, multiplier, behavior);

    bool shouldAnimate = scrollAnimationEnabledForSystem() && m_scrollableArea.scrollAnimatorEnabled() && granularity != ScrollByPixel
        && !behavior.contains(ScrollBehavior::NeverAnimate);
    FloatPoint newPosition = this->currentPosition() + deltaFromStep(orientation, step, multiplier);
    newPosition = newPosition.constrainedBetween(scrollableArea().minimumScrollPosition(), scrollableArea().maximumScrollPosition());

    LOG_WITH_STREAM(Scrolling, stream << "ScrollAnimatorMac::scroll from " << currentPosition() << " to " << newPosition);

    if (!shouldAnimate)
        return scrollToPositionWithoutAnimation(newPosition);

    if ([m_scrollAnimationHelper _isAnimating]) {
        NSPoint targetOrigin = [m_scrollAnimationHelper targetOrigin];
        if (orientation == HorizontalScrollbar)
            newPosition.setY(targetOrigin.y);
        else
            newPosition.setX(targetOrigin.x);
    }

    LOG_WITH_STREAM(Scrolling, stream << "ScrollAnimatorMac::scroll " << " from " << currentPosition() << " to " << newPosition);
    [m_scrollAnimationHelper scrollToPoint:newPosition];
    return true;
}

bool ScrollAnimatorMac::scrollToPositionWithAnimation(const FloatPoint& newPosition)
{
    bool positionChanged = newPosition != currentPosition();
    if (!positionChanged && !scrollableArea().scrollOriginChanged())
        return false;

    // FIXME: This is used primarily by smooth scrolling. This should ideally use native scroll animations.
    // See: https://bugs.webkit.org/show_bug.cgi?id=218857
    [m_scrollAnimationHelper _stopRun];
    return ScrollAnimator::scrollToPositionWithAnimation(newPosition);
}

bool ScrollAnimatorMac::scrollToPositionWithoutAnimation(const FloatPoint& position, ScrollClamping clamping)
{
    [m_scrollAnimationHelper _stopRun];
    return ScrollAnimator::scrollToPositionWithoutAnimation(position, clamping);
}

FloatPoint ScrollAnimatorMac::adjustScrollPositionIfNecessary(const FloatPoint& position) const
{
    if (!m_scrollableArea.constrainsScrollingToContentEdge())
        return position;

    return m_scrollableArea.constrainScrollPosition(ScrollPosition(position));
}

void ScrollAnimatorMac::adjustScrollPositionToBoundsIfNecessary()
{
    bool currentlyConstrainsToContentEdge = m_scrollableArea.constrainsScrollingToContentEdge();
    m_scrollableArea.setConstrainsScrollingToContentEdge(true);

    ScrollPosition currentScrollPosition = m_scrollableArea.scrollPosition();
    ScrollPosition constrainedPosition = m_scrollableArea.constrainScrollPosition(currentScrollPosition);
    immediateScrollBy(constrainedPosition - currentScrollPosition);

    m_scrollableArea.setConstrainsScrollingToContentEdge(currentlyConstrainsToContentEdge);
}

bool ScrollAnimatorMac::isUserScrollInProgress() const
{
    return m_scrollController.isUserScrollInProgress();
}

bool ScrollAnimatorMac::isRubberBandInProgress() const
{
#if !ENABLE(RUBBER_BANDING)
    return false;
#else
    return m_scrollController.isRubberBandInProgress();
#endif
}

bool ScrollAnimatorMac::isScrollSnapInProgress() const
{
#if ENABLE(CSS_SCROLL_SNAP)
    return m_scrollController.isScrollSnapInProgress();
#else
    return false;
#endif
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

    return result.toString();
}

String ScrollAnimatorMac::horizontalScrollbarStateForTesting() const
{
    return scrollbarState(m_scrollableArea.horizontalScrollbar());
}

String ScrollAnimatorMac::verticalScrollbarStateForTesting() const
{
    return scrollbarState(m_scrollableArea.verticalScrollbar());
}

void ScrollAnimatorMac::immediateScrollToPositionForScrollAnimation(const FloatPoint& newPosition)
{
    ASSERT(m_scrollAnimationHelper);
    ScrollAnimator::scrollToPositionWithoutAnimation(newPosition);
}

void ScrollAnimatorMac::notifyPositionChanged(const FloatSize& delta)
{
    notifyContentAreaScrolled(delta);
    ScrollAnimator::notifyPositionChanged(delta);
}

void ScrollAnimatorMac::contentAreaWillPaint() const
{
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [m_scrollerImpPair contentAreaWillDraw];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

void ScrollAnimatorMac::mouseEnteredContentArea()
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollAnimatorMac for [" << scrollableArea() << "] mouseEnteredContentArea");
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair mouseEnteredContentArea];
}

void ScrollAnimatorMac::mouseExitedContentArea()
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollAnimatorMac for [" << scrollableArea() << "] mouseExitedContentArea");
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair mouseExitedContentArea];
}

void ScrollAnimatorMac::mouseMovedInContentArea()
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollAnimatorMac for [" << scrollableArea() << "] mouseMovedInContentArea");
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair mouseMovedInContentArea];
}

void ScrollAnimatorMac::mouseEnteredScrollbar(Scrollbar* scrollbar) const
{
    // At this time, only legacy scrollbars needs to send notifications here.
    if (ScrollerStyle::recommendedScrollerStyle() != NSScrollerStyleLegacy)
        return;

    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    if (NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar))
        [painter mouseEnteredScroller];
}

void ScrollAnimatorMac::mouseExitedScrollbar(Scrollbar* scrollbar) const
{
    // At this time, only legacy scrollbars needs to send notifications here.
    if (ScrollerStyle::recommendedScrollerStyle() != NSScrollerStyleLegacy)
        return;

    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    if (NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar))
        [painter mouseExitedScroller];
}

void ScrollAnimatorMac::mouseIsDownInScrollbar(Scrollbar* scrollbar, bool mouseIsDown) const
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

void ScrollAnimatorMac::willStartLiveResize()
{
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair startLiveResize];
}

void ScrollAnimatorMac::contentsResized() const
{
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair contentAreaDidResize];
}

void ScrollAnimatorMac::willEndLiveResize()
{
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair endLiveResize];
}

void ScrollAnimatorMac::contentAreaDidShow()
{
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair windowOrderedIn];
}

void ScrollAnimatorMac::contentAreaDidHide()
{
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair windowOrderedOut];
}

void ScrollAnimatorMac::didBeginScrollGesture() const
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollAnimatorMac for [" << scrollableArea() << "] didBeginScrollGesture");

    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair beginScrollGesture];

#if ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)
    if (m_wheelEventTestMonitor)
        m_wheelEventTestMonitor->deferForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), WheelEventTestMonitor::ContentScrollInProgress);
#endif
}

void ScrollAnimatorMac::didEndScrollGesture() const
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollAnimatorMac for [" << scrollableArea() << "] didEndScrollGesture");

    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair endScrollGesture];

#if ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)
    if (m_wheelEventTestMonitor)
        m_wheelEventTestMonitor->removeDeferralForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), WheelEventTestMonitor::ContentScrollInProgress);
#endif
}

void ScrollAnimatorMac::mayBeginScrollGesture() const
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollAnimatorMac for [" << scrollableArea() << "] mayBeginScrollGesture");

    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    [m_scrollerImpPair beginScrollGesture];
    [m_scrollerImpPair contentAreaScrolled];
}

void ScrollAnimatorMac::lockOverlayScrollbarStateToHidden(bool shouldLockState)
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

bool ScrollAnimatorMac::scrollbarsCanBeActive() const
{
    return ![m_scrollerImpPair overlayScrollerStateIsLocked];
}

void ScrollAnimatorMac::didAddVerticalScrollbar(Scrollbar* scrollbar)
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

void ScrollAnimatorMac::willRemoveVerticalScrollbar(Scrollbar* scrollbar)
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

void ScrollAnimatorMac::didAddHorizontalScrollbar(Scrollbar* scrollbar)
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

void ScrollAnimatorMac::willRemoveHorizontalScrollbar(Scrollbar* scrollbar)
{
    NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar);
    if (!painter)
        return;

    ASSERT(m_horizontalScrollerImpDelegate);
    [m_horizontalScrollerImpDelegate invalidate];
    m_horizontalScrollerImpDelegate = nullptr;

    [painter setDelegate:nil];
    [m_scrollerImpPair setHorizontalScrollerImp:nil];
}

void ScrollAnimatorMac::invalidateScrollbarPartLayers(Scrollbar* scrollbar)
{
    NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar);
    [painter setNeedsDisplay:YES];
}

void ScrollAnimatorMac::verticalScrollbarLayerDidChange()
{
    GraphicsLayer* layer = m_scrollableArea.layerForVerticalScrollbar();
    Scrollbar* scrollbar = m_scrollableArea.verticalScrollbar();
    if (!scrollbar)
        return;

    NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar);
    if (!painter)
        return;

    [painter setLayer:layer ? layer->platformLayer() : nil];
}

void ScrollAnimatorMac::horizontalScrollbarLayerDidChange()
{
    GraphicsLayer* layer = m_scrollableArea.layerForHorizontalScrollbar();
    Scrollbar* scrollbar = m_scrollableArea.horizontalScrollbar();
    if (!scrollbar)
        return;

    NSScrollerImp *painter = scrollerImpForScrollbar(*scrollbar);
    if (!painter)
        return;

    [painter setLayer:layer ? layer->platformLayer() : nil];
}

bool ScrollAnimatorMac::shouldScrollbarParticipateInHitTesting(Scrollbar* scrollbar)
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

void ScrollAnimatorMac::notifyContentAreaScrolled(const FloatSize& delta)
{
    // This function is called when a page is going into the back/forward cache, but the page
    // isn't really scrolling in that case. We should only pass the message on to the
    // ScrollerImpPair when we're really scrolling on an active page.
    if ([m_scrollerImpPair overlayScrollerStateIsLocked])
        return;

    if (m_scrollableArea.isHandlingWheelEvent())
        sendContentAreaScrolled(delta);
    else
        sendContentAreaScrolledSoon(delta);
}

void ScrollAnimatorMac::cancelAnimations()
{
    ScrollAnimator::cancelAnimations();
    m_haveScrolledSincePageLoad = false;

    if (scrollbarPaintTimerIsActive())
        stopScrollbarPaintTimer();
    [m_horizontalScrollerImpDelegate cancelAnimations];
    [m_verticalScrollerImpDelegate cancelAnimations];
}

void ScrollAnimatorMac::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollAnimatorMac " << this << " scrollableArea " << m_scrollableArea << " handleWheelEventPhase " << phase);

    // This may not have been set to true yet if the wheel event was handled by the ScrollingTree,
    // So set it to true here.
    m_haveScrolledSincePageLoad = true;

    // FIXME: Need to ensure we get PlatformWheelEventPhase::Ended.
    if (phase == PlatformWheelEventPhase::Began)
        didBeginScrollGesture();
    else if (phase == PlatformWheelEventPhase::Ended || phase == PlatformWheelEventPhase::Cancelled)
        didEndScrollGesture();
    else if (phase == PlatformWheelEventPhase::MayBegin)
        mayBeginScrollGesture();
}

#if ENABLE(RUBBER_BANDING)

bool ScrollAnimatorMac::shouldForwardWheelEventsToParent(const PlatformWheelEvent& wheelEvent) const
{
    if (std::abs(wheelEvent.deltaY()) >= std::abs(wheelEvent.deltaX()))
        return !allowsVerticalStretching(wheelEvent);

    return !allowsHorizontalStretching(wheelEvent);
}
    
bool ScrollAnimatorMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    m_haveScrolledSincePageLoad = true;

    if (!wheelEvent.hasPreciseScrollingDeltas() || !rubberBandingEnabledForSystem())
        return ScrollAnimator::handleWheelEvent(wheelEvent);

    m_scrollController.updateGestureInProgressState(wheelEvent);

    // FIXME: This is somewhat roundabout hack to allow forwarding wheel events
    // up to the parent scrollable area. It takes advantage of the fact that
    // the base class implementation of handleWheelEvent will not accept the
    // wheel event if there is nowhere to scroll.
    if (shouldForwardWheelEventsToParent(wheelEvent)) {
        bool didHandleEvent = ScrollAnimator::handleWheelEvent(wheelEvent);
        if (didHandleEvent || (!wheelEvent.deltaX() && !wheelEvent.deltaY()))
            handleWheelEventPhase(wheelEvent.phase());
        return didHandleEvent;
    }

    bool didHandleEvent = m_scrollController.handleWheelEvent(wheelEvent);

    if (didHandleEvent)
        handleWheelEventPhase(wheelEvent.phase());

    return didHandleEvent;
}

RectEdges<bool> ScrollAnimatorMac::edgePinnedState() const
{
    return m_scrollableArea.edgePinnedState();
}

bool ScrollAnimatorMac::isPinnedForScrollDelta(const FloatSize& delta) const
{
    if (fabsf(delta.height()) >= fabsf(delta.width()))
        return m_scrollableArea.isPinnedForScrollDeltaOnAxis(delta.height(), ScrollEventAxis::Vertical);

    if (delta.width())
        return m_scrollableArea.isPinnedForScrollDeltaOnAxis(delta.width(), ScrollEventAxis::Horizontal);

    return false;
}

#if ENABLE(CSS_SCROLL_SNAP)
static bool gestureShouldBeginSnap(const PlatformWheelEvent& wheelEvent, ScrollEventAxis axis, const LayoutScrollSnapOffsetsInfo* offsetInfo)
{
    if (!offsetInfo)
        return false;

    if (offsetInfo->offsetsForAxis(axis).isEmpty())
        return false;

    if (wheelEvent.phase() != PlatformWheelEventPhase::Ended && !wheelEvent.isEndOfMomentumScroll())
        return false;

    return true;
}
#endif

bool ScrollAnimatorMac::allowsVerticalStretching(const PlatformWheelEvent& wheelEvent) const
{
    switch (m_scrollableArea.verticalScrollElasticity()) {
    case ScrollElasticityAutomatic: {
        Scrollbar* hScroller = m_scrollableArea.horizontalScrollbar();
        Scrollbar* vScroller = m_scrollableArea.verticalScrollbar();
        bool scrollbarsAllowStretching = ((vScroller && vScroller->enabled()) || (!hScroller || !hScroller->enabled()));
        bool eventPreventsStretching = m_scrollableArea.hasScrollableOrRubberbandableAncestor() && wheelEvent.isGestureStart() && m_scrollableArea.isPinnedForScrollDeltaOnAxis(-wheelEvent.deltaY(), ScrollEventAxis::Vertical);
#if ENABLE(CSS_SCROLL_SNAP)
        if (!eventPreventsStretching)
            eventPreventsStretching = gestureShouldBeginSnap(wheelEvent, ScrollEventAxis::Vertical, m_scrollableArea.snapOffsetInfo());
#endif
        return scrollbarsAllowStretching && !eventPreventsStretching;
    }
    case ScrollElasticityNone:
        return false;
    case ScrollElasticityAllowed:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool ScrollAnimatorMac::allowsHorizontalStretching(const PlatformWheelEvent& wheelEvent) const
{
    switch (m_scrollableArea.horizontalScrollElasticity()) {
    case ScrollElasticityAutomatic: {
        Scrollbar* hScroller = m_scrollableArea.horizontalScrollbar();
        Scrollbar* vScroller = m_scrollableArea.verticalScrollbar();
        bool scrollbarsAllowStretching = ((hScroller && hScroller->enabled()) || (!vScroller || !vScroller->enabled()));
        bool eventPreventsStretching = m_scrollableArea.hasScrollableOrRubberbandableAncestor() && wheelEvent.isGestureStart() && m_scrollableArea.isPinnedForScrollDeltaOnAxis(-wheelEvent.deltaX(), ScrollEventAxis::Horizontal);
#if ENABLE(CSS_SCROLL_SNAP)
        if (!eventPreventsStretching)
            eventPreventsStretching = gestureShouldBeginSnap(wheelEvent, ScrollEventAxis::Horizontal, m_scrollableArea.snapOffsetInfo());
#endif
        return scrollbarsAllowStretching && !eventPreventsStretching;
    }
    case ScrollElasticityNone:
        return false;
    case ScrollElasticityAllowed:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

IntSize ScrollAnimatorMac::stretchAmount() const
{
    return m_scrollableArea.overhangAmount();
}

bool ScrollAnimatorMac::allowsHorizontalScrolling() const
{
    return m_scrollableArea.allowsHorizontalScrolling();
}

bool ScrollAnimatorMac::allowsVerticalScrolling() const
{
    return m_scrollableArea.allowsVerticalScrolling();
}

bool ScrollAnimatorMac::shouldRubberBandInDirection(ScrollDirection) const
{
    return false;
}

void ScrollAnimatorMac::immediateScrollByWithoutContentEdgeConstraints(const FloatSize& delta)
{
    m_scrollableArea.setConstrainsScrollingToContentEdge(false);
    immediateScrollBy(delta);
    m_scrollableArea.setConstrainsScrollingToContentEdge(true);
}

void ScrollAnimatorMac::immediateScrollBy(const FloatSize& delta)
{
    FloatPoint currentPosition = this->currentPosition();
    FloatPoint newPosition = adjustScrollPositionIfNecessary(currentPosition + delta);
    if (newPosition == currentPosition)
        return;

    FloatSize adjustedDelta = newPosition - currentPosition;
    m_currentPosition = newPosition;
    notifyPositionChanged(adjustedDelta);
    updateActiveScrollSnapIndexForOffset();
}
#endif

void ScrollAnimatorMac::updateScrollerStyle()
{
    if ([m_scrollerImpPair overlayScrollerStateIsLocked]) {
        m_needsScrollerStyleUpdate = true;
        return;
    }

    ScrollbarThemeMac* macTheme = macScrollbarTheme();
    if (!macTheme) {
        m_needsScrollerStyleUpdate = false;
        return;
    }
    
    macTheme->usesOverlayScrollbarsChanged();

    NSScrollerStyle newStyle = [m_scrollerImpPair scrollerStyle];

    if (Scrollbar* verticalScrollbar = scrollableArea().verticalScrollbar()) {
        verticalScrollbar->invalidate();

        NSScrollerImp *oldVerticalPainter = [m_scrollerImpPair verticalScrollerImp];
        auto newVerticalPainter = retainPtr([NSScrollerImp scrollerImpWithStyle:newStyle controlSize:(NSControlSize)verticalScrollbar->controlSize() horizontal:NO replacingScrollerImp:oldVerticalPainter]);

        [m_scrollerImpPair setVerticalScrollerImp:newVerticalPainter.get()];
        macTheme->setNewPainterForScrollbar(*verticalScrollbar, WTFMove(newVerticalPainter));
        macTheme->didCreateScrollerImp(*verticalScrollbar);

        // The different scrollbar styles have different thicknesses, so we must re-set the 
        // frameRect to the new thickness, and the re-layout below will ensure the position
        // and length are properly updated.
        int thickness = macTheme->scrollbarThickness(verticalScrollbar->controlSize());
        verticalScrollbar->setFrameRect(IntRect(0, 0, thickness, thickness));
    }

    if (Scrollbar* horizontalScrollbar = scrollableArea().horizontalScrollbar()) {
        horizontalScrollbar->invalidate();

        NSScrollerImp *oldHorizontalPainter = [m_scrollerImpPair horizontalScrollerImp];
        auto newHorizontalPainter = retainPtr([NSScrollerImp scrollerImpWithStyle:newStyle controlSize:(NSControlSize)horizontalScrollbar->controlSize() horizontal:YES replacingScrollerImp:oldHorizontalPainter]);

        [m_scrollerImpPair setHorizontalScrollerImp:newHorizontalPainter.get()];
        macTheme->setNewPainterForScrollbar(*horizontalScrollbar, WTFMove(newHorizontalPainter));
        macTheme->didCreateScrollerImp(*horizontalScrollbar);

        // The different scrollbar styles have different thicknesses, so we must re-set the 
        // frameRect to the new thickness, and the re-layout below will ensure the position
        // and length are properly updated.
        int thickness = macTheme->scrollbarThickness(horizontalScrollbar->controlSize());
        horizontalScrollbar->setFrameRect(IntRect(0, 0, thickness, thickness));
    }

    // If m_needsScrollerStyleUpdate is true, then the page is restoring from the back/forward cache, and
    // a relayout will happen on its own. Otherwise, we must initiate a re-layout ourselves.
    scrollableArea().scrollbarStyleChanged(newStyle == NSScrollerStyleOverlay ? ScrollbarStyle::Overlay : ScrollbarStyle::AlwaysVisible, !m_needsScrollerStyleUpdate);

    m_needsScrollerStyleUpdate = false;
}

void ScrollAnimatorMac::startScrollbarPaintTimer()
{
    m_initialScrollbarPaintTimer.startOneShot(100_ms);
}

bool ScrollAnimatorMac::scrollbarPaintTimerIsActive() const
{
    return m_initialScrollbarPaintTimer.isActive();
}

void ScrollAnimatorMac::stopScrollbarPaintTimer()
{
    m_initialScrollbarPaintTimer.stop();
}

void ScrollAnimatorMac::initialScrollbarPaintTimerFired()
{
    // To force the scrollbars to flash, we have to call hide first. Otherwise, the ScrollerImpPair
    // might think that the scrollbars are already showing and bail early.
    [m_scrollerImpPair hideOverlayScrollers];
    [m_scrollerImpPair flashScrollers];
}

void ScrollAnimatorMac::sendContentAreaScrolledSoon(const FloatSize& delta)
{
    m_contentAreaScrolledTimerScrollDelta = delta;

    if (!m_sendContentAreaScrolledTimer.isActive())
        m_sendContentAreaScrolledTimer.startOneShot(0_s);

    if (m_wheelEventTestMonitor)
        m_wheelEventTestMonitor->deferForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), WheelEventTestMonitor::ContentScrollInProgress);
}

void ScrollAnimatorMac::sendContentAreaScrolled(const FloatSize& delta)
{
    [m_scrollerImpPair contentAreaScrolledInDirection:NSMakePoint(delta.width(), delta.height())];
}

void ScrollAnimatorMac::sendContentAreaScrolledTimerFired()
{
    sendContentAreaScrolled(m_contentAreaScrolledTimerScrollDelta);
    m_contentAreaScrolledTimerScrollDelta = FloatSize();

    if (m_wheelEventTestMonitor)
        m_wheelEventTestMonitor->removeDeferralForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), WheelEventTestMonitor::ContentScrollInProgress);
}

void ScrollAnimatorMac::setVisibleScrollerThumbRect(const IntRect& scrollerThumb)
{
    IntRect rectInViewCoordinates = scrollerThumb;
    if (Scrollbar* verticalScrollbar = m_scrollableArea.verticalScrollbar())
        rectInViewCoordinates = verticalScrollbar->convertToContainingView(scrollerThumb);

    if (rectInViewCoordinates == m_visibleScrollerThumbRect)
        return;

    m_scrollableArea.setVisibleScrollerThumbRect(rectInViewCoordinates);
    m_visibleScrollerThumbRect = rectInViewCoordinates;
}

bool ScrollAnimatorMac::processWheelEventForScrollSnap(const PlatformWheelEvent& wheelEvent)
{
    return m_scrollController.processWheelEventForScrollSnap(wheelEvent);
}

} // namespace WebCore

#endif // ENABLE(SMOOTH_SCROLLING)
