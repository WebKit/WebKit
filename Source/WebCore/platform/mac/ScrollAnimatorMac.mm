/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#if ENABLE(SMOOTH_SCROLLING)

#include "ScrollAnimatorMac.h"

#include "BlockExceptions.h"
#include "FloatPoint.h"
#include "GraphicsLayer.h"
#include "NSScrollerImpDetails.h"
#include "PlatformWheelEvent.h"
#include "ScrollView.h"
#include "ScrollableArea.h"
#include "ScrollbarTheme.h"
#include "ScrollbarThemeMac.h"
#include "WebCoreSystemInterface.h"
#include <wtf/PassOwnPtr.h>

using namespace WebCore;

static bool supportsUIStateTransitionProgress()
{
    // FIXME: This is temporary until all platforms that support ScrollbarPainter support this part of the API.
    static bool globalSupportsUIStateTransitionProgress = [NSClassFromString(@"NSScrollerImp") instancesRespondToSelector:@selector(mouseEnteredScroller)];
    return globalSupportsUIStateTransitionProgress;
}

static bool supportsExpansionTransitionProgress()
{
    static bool globalSupportsExpansionTransitionProgress = [NSClassFromString(@"NSScrollerImp") instancesRespondToSelector:@selector(expansionTransitionProgress)];
    return globalSupportsExpansionTransitionProgress;
}

static bool supportsContentAreaScrolledInDirection()
{
    static bool globalSupportsContentAreaScrolledInDirection = [NSClassFromString(@"NSScrollerImpPair") instancesRespondToSelector:@selector(contentAreaScrolledInDirection:)];
    return globalSupportsContentAreaScrolledInDirection;
}

static ScrollbarThemeMac* macScrollbarTheme()
{
    ScrollbarTheme* scrollbarTheme = ScrollbarTheme::theme();
    return !scrollbarTheme->isMockTheme() ? static_cast<ScrollbarThemeMac*>(scrollbarTheme) : 0;
}

static ScrollbarPainter scrollbarPainterForScrollbar(Scrollbar* scrollbar)
{
    if (ScrollbarThemeMac* scrollbarTheme = macScrollbarTheme())
        return scrollbarTheme->painterForScrollbar(scrollbar);

    return nil;
}

@interface NSObject (ScrollAnimationHelperDetails)
- (id)initWithDelegate:(id)delegate;
- (void)_stopRun;
- (BOOL)_isAnimating;
- (NSPoint)targetOrigin;
- (CGFloat)_progress;
@end

@interface WebScrollAnimationHelperDelegate : NSObject
{
    WebCore::ScrollAnimatorMac* _animator;
}
- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorMac*)scrollAnimator;
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

- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorMac*)scrollAnimator
{
    self = [super init];
    if (!self)
        return nil;

    _animator = scrollAnimator;
    return self;
}

- (void)invalidate
{
    _animator = 0;
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
    _animator->immediateScrollToPointForScrollAnimation(newPosition);
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

@interface WebScrollbarPainterControllerDelegate : NSObject
{
    ScrollableArea* _scrollableArea;
}
- (id)initWithScrollableArea:(ScrollableArea*)scrollableArea;
@end

@implementation WebScrollbarPainterControllerDelegate

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

- (NSRect)contentAreaRectForScrollerImpPair:(id)scrollerImpPair
{
    UNUSED_PARAM(scrollerImpPair);
    if (!_scrollableArea)
        return NSZeroRect;

    WebCore::IntSize contentsSize = _scrollableArea->contentsSize();
    return NSMakeRect(0, 0, contentsSize.width(), contentsSize.height());
}

- (BOOL)inLiveResizeForScrollerImpPair:(id)scrollerImpPair
{
    UNUSED_PARAM(scrollerImpPair);
    if (!_scrollableArea)
        return NO;

    return _scrollableArea->inLiveResize();
}

- (NSPoint)mouseLocationInContentAreaForScrollerImpPair:(id)scrollerImpPair
{
    UNUSED_PARAM(scrollerImpPair);
    if (!_scrollableArea)
        return NSZeroPoint;

    return _scrollableArea->lastKnownMousePosition();
}

- (NSPoint)scrollerImpPair:(id)scrollerImpPair convertContentPoint:(NSPoint)pointInContentArea toScrollerImp:(id)scrollerImp
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

    ASSERT(scrollerImp == scrollbarPainterForScrollbar(scrollbar));

    return scrollbar->convertFromContainingView(WebCore::IntPoint(pointInContentArea));
}

- (void)scrollerImpPair:(id)scrollerImpPair setContentAreaNeedsDisplayInRect:(NSRect)rect
{
    UNUSED_PARAM(scrollerImpPair);
    UNUSED_PARAM(rect);

    if (!_scrollableArea)
        return;

    if ([scrollerImpPair overlayScrollerStateIsLocked])
        return;

    _scrollableArea->scrollAnimator()->contentAreaWillPaint();
}

- (void)scrollerImpPair:(id)scrollerImpPair updateScrollerStyleForNewRecommendedScrollerStyle:(NSScrollerStyle)newRecommendedScrollerStyle
{
    if (!_scrollableArea)
        return;

    [scrollerImpPair setScrollerStyle:newRecommendedScrollerStyle];

    static_cast<ScrollAnimatorMac*>(_scrollableArea->scrollAnimator())->updateScrollerStyle();
}

@end

enum FeatureToAnimate {
    ThumbAlpha,
    TrackAlpha,
    UIStateTransition,
    ExpansionTransition
};

@interface WebScrollbarPartAnimation : NSAnimation
{
    Scrollbar* _scrollbar;
    RetainPtr<ScrollbarPainter> _scrollbarPainter;
    FeatureToAnimate _featureToAnimate;
    CGFloat _startValue;
    CGFloat _endValue;
}
- (id)initWithScrollbar:(Scrollbar*)scrollbar featureToAnimate:(FeatureToAnimate)featureToAnimate animateFrom:(CGFloat)startValue animateTo:(CGFloat)endValue duration:(NSTimeInterval)duration;
@end

@implementation WebScrollbarPartAnimation

- (id)initWithScrollbar:(Scrollbar*)scrollbar featureToAnimate:(FeatureToAnimate)featureToAnimate animateFrom:(CGFloat)startValue animateTo:(CGFloat)endValue duration:(NSTimeInterval)duration
{
    self = [super initWithDuration:duration animationCurve:NSAnimationEaseInOut];
    if (!self)
        return nil;

    _scrollbar = scrollbar;
    _featureToAnimate = featureToAnimate;
    _startValue = startValue;
    _endValue = endValue;

    [self setAnimationBlockingMode:NSAnimationNonblocking];

    return self;
}

- (void)startAnimation
{
    ASSERT(_scrollbar);

    _scrollbarPainter = scrollbarPainterForScrollbar(_scrollbar);

    [super startAnimation];
}

- (void)setStartValue:(CGFloat)startValue
{
    _startValue = startValue;
}

- (void)setEndValue:(CGFloat)endValue
{
    _endValue = endValue;
}

- (void)setCurrentProgress:(NSAnimationProgress)progress
{
    [super setCurrentProgress:progress];

    ASSERT(_scrollbar);

    CGFloat currentValue;
    if (_startValue > _endValue)
        currentValue = 1 - progress;
    else
        currentValue = progress;

    switch (_featureToAnimate) {
    case ThumbAlpha:
        [_scrollbarPainter setKnobAlpha:currentValue];
        break;
    case TrackAlpha:
        [_scrollbarPainter setTrackAlpha:currentValue];
        break;
    case UIStateTransition:
        [_scrollbarPainter setUiStateTransitionProgress:currentValue];
        break;
    case ExpansionTransition:
        [_scrollbarPainter setExpansionTransitionProgress:currentValue];
        break;
    }

    _scrollbar->invalidate();
}

- (void)invalidate
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [self stopAnimation];
    END_BLOCK_OBJC_EXCEPTIONS;
    _scrollbar = 0;
}

@end

@interface WebScrollbarPainterDelegate : NSObject<NSAnimationDelegate>
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

@implementation WebScrollbarPainterDelegate

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
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_knobAlphaAnimation stopAnimation];
    [_trackAlphaAnimation stopAnimation];
    [_uiStateTransitionAnimation stopAnimation];
    [_expansionTransitionAnimation stopAnimation];
    END_BLOCK_OBJC_EXCEPTIONS;
}

- (ScrollAnimatorMac*)scrollAnimator
{
    return static_cast<ScrollAnimatorMac*>(_scrollbar->scrollableArea()->scrollAnimator());
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
        layer = _scrollbar->scrollableArea()->layerForVerticalScrollbar();
    else
        layer = _scrollbar->scrollableArea()->layerForHorizontalScrollbar();

    static CALayer *dummyLayer = [[CALayer alloc] init];
    return layer ? layer->platformLayer() : dummyLayer;
}

- (NSPoint)mouseLocationInScrollerForScrollerImp:(id)scrollerImp
{
    if (!_scrollbar)
        return NSZeroPoint;

    ASSERT_UNUSED(scrollerImp, scrollerImp == scrollbarPainterForScrollbar(_scrollbar));

    return _scrollbar->convertFromContainingView(_scrollbar->scrollableArea()->lastKnownMousePosition());
}

- (NSRect)convertRectToLayer:(NSRect)rect
{
    return rect;
}

- (BOOL)shouldUseLayerPerPartForScrollerImp:(id)scrollerImp
{
    if (!_scrollbar)
        return false;

    ASSERT_UNUSED(scrollerImp, scrollerImp == scrollbarPainterForScrollbar(_scrollbar));

    return _scrollbar->supportsUpdateOnSecondaryThread();
}

- (void)setUpAlphaAnimation:(RetainPtr<WebScrollbarPartAnimation>&)scrollbarPartAnimation scrollerPainter:(ScrollbarPainter)scrollerPainter part:(WebCore::ScrollbarPart)part animateAlphaTo:(CGFloat)newAlpha duration:(NSTimeInterval)duration
{
    // If the user has scrolled the page, then the scrollbars must be animated here.
    // This overrides the early returns.
    bool mustAnimate = [self scrollAnimator]->haveScrolledSincePageLoad();

    if ([self scrollAnimator]->scrollbarPaintTimerIsActive() && !mustAnimate)
        return;

    if (_scrollbar->scrollableArea()->shouldSuspendScrollAnimations() && !mustAnimate) {
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
        macTheme->setPaintCharacteristicsForScrollbar(_scrollbar);

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

- (void)scrollerImp:(id)scrollerImp animateKnobAlphaTo:(CGFloat)newKnobAlpha duration:(NSTimeInterval)duration
{
    if (!_scrollbar)
        return;

    ASSERT(scrollerImp == scrollbarPainterForScrollbar(_scrollbar));

    ScrollbarPainter scrollerPainter = (ScrollbarPainter)scrollerImp;
    if (![self scrollAnimator]->scrollbarsCanBeActive()) {
        [scrollerImp setKnobAlpha:0];
        _scrollbar->invalidate();
        return;
    }

    // If we are fading the scrollbar away, that is a good indication that we are no longer going to
    // be moving it around on the scrolling thread. Calling [scrollerPainter setUsePresentationValue:NO]
    // will pass that information on to the ScrollbarPainter API.
    if (newKnobAlpha == 0 && _scrollbar->supportsUpdateOnSecondaryThread())
        [scrollerPainter setUsePresentationValue:NO];

    [self setUpAlphaAnimation:_knobAlphaAnimation scrollerPainter:scrollerPainter part:WebCore::ThumbPart animateAlphaTo:newKnobAlpha duration:duration];
}

- (void)scrollerImp:(id)scrollerImp animateTrackAlphaTo:(CGFloat)newTrackAlpha duration:(NSTimeInterval)duration
{
    if (!_scrollbar)
        return;

    ASSERT(scrollerImp == scrollbarPainterForScrollbar(_scrollbar));

    ScrollbarPainter scrollerPainter = (ScrollbarPainter)scrollerImp;
    [self setUpAlphaAnimation:_trackAlphaAnimation scrollerPainter:scrollerPainter part:WebCore::BackTrackPart animateAlphaTo:newTrackAlpha duration:duration];
}

- (void)scrollerImp:(id)scrollerImp animateUIStateTransitionWithDuration:(NSTimeInterval)duration
{
    if (!_scrollbar)
        return;

    if (!supportsUIStateTransitionProgress())
        return;

    ASSERT(scrollerImp == scrollbarPainterForScrollbar(_scrollbar));

    ScrollbarPainter scrollbarPainter = (ScrollbarPainter)scrollerImp;

    // UIStateTransition always animates to 1. In case an animation is in progress this avoids a hard transition.
    [scrollbarPainter setUiStateTransitionProgress:1 - [scrollerImp uiStateTransitionProgress]];

    // If the UI state transition is happening, then we are no longer moving the scrollbar on the scrolling thread.
    if (_scrollbar->supportsUpdateOnSecondaryThread())
        [scrollbarPainter setUsePresentationValue:NO];

    if (!_uiStateTransitionAnimation)
        _uiStateTransitionAnimation = adoptNS([[WebScrollbarPartAnimation alloc] initWithScrollbar:_scrollbar 
                                                                                featureToAnimate:UIStateTransition
                                                                                     animateFrom:[scrollbarPainter uiStateTransitionProgress]
                                                                                       animateTo:1.0
                                                                                        duration:duration]);
    else {
        // If we don't need to initialize the animation, just reset the values in case they have changed.
        [_uiStateTransitionAnimation setStartValue:[scrollbarPainter uiStateTransitionProgress]];
        [_uiStateTransitionAnimation setEndValue:1.0];
        [_uiStateTransitionAnimation setDuration:duration];
    }
    [_uiStateTransitionAnimation startAnimation];
}

- (void)scrollerImp:(id)scrollerImp animateExpansionTransitionWithDuration:(NSTimeInterval)duration
{
    if (!_scrollbar)
        return;

    if (!supportsExpansionTransitionProgress())
        return;

    ASSERT(scrollerImp == scrollbarPainterForScrollbar(_scrollbar));

    ScrollbarPainter scrollbarPainter = (ScrollbarPainter)scrollerImp;

    // ExpansionTransition always animates to 1. In case an animation is in progress this avoids a hard transition.
    [scrollbarPainter setExpansionTransitionProgress:1 - [scrollerImp expansionTransitionProgress]];

    if (!_expansionTransitionAnimation) {
        _expansionTransitionAnimation = adoptNS([[WebScrollbarPartAnimation alloc] initWithScrollbar:_scrollbar
                                                                                  featureToAnimate:ExpansionTransition
                                                                                       animateFrom:[scrollbarPainter expansionTransitionProgress]
                                                                                         animateTo:1.0
                                                                                          duration:duration]);
    } else {
        // If we don't need to initialize the animation, just reset the values in case they have changed.
        [_expansionTransitionAnimation setStartValue:[scrollbarPainter uiStateTransitionProgress]];
        [_expansionTransitionAnimation setEndValue:1.0];
        [_expansionTransitionAnimation setDuration:duration];
    }
    [_expansionTransitionAnimation startAnimation];
}

- (void)scrollerImp:(id)scrollerImp overlayScrollerStateChangedTo:(NSUInteger)newOverlayScrollerState
{
    UNUSED_PARAM(scrollerImp);
    UNUSED_PARAM(newOverlayScrollerState);
}

- (void)invalidate
{
    _scrollbar = 0;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_knobAlphaAnimation invalidate];
    [_trackAlphaAnimation invalidate];
    [_uiStateTransitionAnimation invalidate];
    [_expansionTransitionAnimation invalidate];
    END_BLOCK_OBJC_EXCEPTIONS;
}

@end

namespace WebCore {

PassOwnPtr<ScrollAnimator> ScrollAnimator::create(ScrollableArea* scrollableArea)
{
    return adoptPtr(new ScrollAnimatorMac(scrollableArea));
}

ScrollAnimatorMac::ScrollAnimatorMac(ScrollableArea* scrollableArea)
    : ScrollAnimator(scrollableArea)
    , m_initialScrollbarPaintTimer(this, &ScrollAnimatorMac::initialScrollbarPaintTimerFired)
    , m_sendContentAreaScrolledTimer(this, &ScrollAnimatorMac::sendContentAreaScrolledTimerFired)
#if ENABLE(RUBBER_BANDING)
    , m_scrollElasticityController(this)
    , m_snapRubberBandTimer(this, &ScrollAnimatorMac::snapRubberBandTimerFired)
#endif
    , m_haveScrolledSincePageLoad(false)
    , m_needsScrollerStyleUpdate(false)
{
    m_scrollAnimationHelperDelegate = adoptNS([[WebScrollAnimationHelperDelegate alloc] initWithScrollAnimator:this]);
    m_scrollAnimationHelper = adoptNS([[NSClassFromString(@"NSScrollAnimationHelper") alloc] initWithDelegate:m_scrollAnimationHelperDelegate.get()]);

    m_scrollbarPainterControllerDelegate = adoptNS([[WebScrollbarPainterControllerDelegate alloc] initWithScrollableArea:scrollableArea]);
    m_scrollbarPainterController = [[[NSClassFromString(@"NSScrollerImpPair") alloc] init] autorelease];
    [m_scrollbarPainterController setDelegate:(id)m_scrollbarPainterControllerDelegate.get()];
    [m_scrollbarPainterController setScrollerStyle:recommendedScrollerStyle()];
}

ScrollAnimatorMac::~ScrollAnimatorMac()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_scrollbarPainterControllerDelegate invalidate];
    [m_scrollbarPainterController setDelegate:nil];
    [m_horizontalScrollbarPainterDelegate invalidate];
    [m_verticalScrollbarPainterDelegate invalidate];
    [m_scrollAnimationHelperDelegate invalidate];
    END_BLOCK_OBJC_EXCEPTIONS;
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

bool ScrollAnimatorMac::scroll(ScrollbarOrientation orientation, ScrollGranularity granularity, float step, float multiplier)
{
    m_haveScrolledSincePageLoad = true;

    if (!scrollAnimationEnabledForSystem() || !m_scrollableArea->scrollAnimatorEnabled())
        return ScrollAnimator::scroll(orientation, granularity, step, multiplier);

    if (granularity == ScrollByPixel)
        return ScrollAnimator::scroll(orientation, granularity, step, multiplier);

    float currentPos = orientation == HorizontalScrollbar ? m_currentPosX : m_currentPosY;
    float newPos = std::max<float>(std::min<float>(currentPos + (step * multiplier), static_cast<float>(m_scrollableArea->scrollSize(orientation))), 0);
    if (currentPos == newPos)
        return false;

    NSPoint newPoint;
    if ([m_scrollAnimationHelper _isAnimating]) {
        NSPoint targetOrigin = [m_scrollAnimationHelper targetOrigin];
        newPoint = orientation == HorizontalScrollbar ? NSMakePoint(newPos, targetOrigin.y) : NSMakePoint(targetOrigin.x, newPos);
    } else
        newPoint = orientation == HorizontalScrollbar ? NSMakePoint(newPos, m_currentPosY) : NSMakePoint(m_currentPosX, newPos);

    [m_scrollAnimationHelper scrollToPoint:newPoint];
    return true;
}

void ScrollAnimatorMac::scrollToOffsetWithoutAnimation(const FloatPoint& offset)
{
    [m_scrollAnimationHelper _stopRun];
    immediateScrollTo(offset);
}

FloatPoint ScrollAnimatorMac::adjustScrollPositionIfNecessary(const FloatPoint& position) const
{
    if (!m_scrollableArea->constrainsScrollingToContentEdge())
        return position;

    float newX = std::max<float>(std::min<float>(position.x(), m_scrollableArea->totalContentsSize().width() - m_scrollableArea->visibleWidth()), 0);
    float newY = std::max<float>(std::min<float>(position.y(), m_scrollableArea->totalContentsSize().height() - m_scrollableArea->visibleHeight()), 0);

    return FloatPoint(newX, newY);
}

void ScrollAnimatorMac::adjustScrollPositionToBoundsIfNecessary()
{
    bool currentlyConstrainsToContentEdge = m_scrollableArea->constrainsScrollingToContentEdge();
    m_scrollableArea->setConstrainsScrollingToContentEdge(true);

    IntPoint currentScrollPosition = absoluteScrollPosition();
    FloatPoint nearestPointWithinBounds = adjustScrollPositionIfNecessary(absoluteScrollPosition());
    immediateScrollBy(nearestPointWithinBounds - currentScrollPosition);

    m_scrollableArea->setConstrainsScrollingToContentEdge(currentlyConstrainsToContentEdge);
}

void ScrollAnimatorMac::immediateScrollTo(const FloatPoint& newPosition)
{
    FloatPoint adjustedPosition = adjustScrollPositionIfNecessary(newPosition);
 
    bool positionChanged = adjustedPosition.x() != m_currentPosX || adjustedPosition.y() != m_currentPosY;
    if (!positionChanged && !scrollableArea()->scrollOriginChanged())
        return;

    FloatSize delta = FloatSize(adjustedPosition.x() - m_currentPosX, adjustedPosition.y() - m_currentPosY);

    m_currentPosX = adjustedPosition.x();
    m_currentPosY = adjustedPosition.y();
    notifyPositionChanged(delta);
}

bool ScrollAnimatorMac::isRubberBandInProgress() const
{
#if !ENABLE(RUBBER_BANDING)
    return false;
#else
    return m_scrollElasticityController.isRubberBandInProgress();
#endif
}

void ScrollAnimatorMac::immediateScrollToPointForScrollAnimation(const FloatPoint& newPosition)
{
    ASSERT(m_scrollAnimationHelper);
    immediateScrollTo(newPosition);
}

void ScrollAnimatorMac::notifyPositionChanged(const FloatSize& delta)
{
    notifyContentAreaScrolled(delta);
    ScrollAnimator::notifyPositionChanged(delta);
}

void ScrollAnimatorMac::contentAreaWillPaint() const
{
    if ([m_scrollbarPainterController overlayScrollerStateIsLocked])
        return;

    [m_scrollbarPainterController contentAreaWillDraw];
}

void ScrollAnimatorMac::mouseEnteredContentArea() const
{
    if ([m_scrollbarPainterController overlayScrollerStateIsLocked])
        return;

    [m_scrollbarPainterController mouseEnteredContentArea];
}

void ScrollAnimatorMac::mouseExitedContentArea() const
{
    if ([m_scrollbarPainterController overlayScrollerStateIsLocked])
        return;

    [m_scrollbarPainterController mouseExitedContentArea];
}

void ScrollAnimatorMac::mouseMovedInContentArea() const
{
    if ([m_scrollbarPainterController overlayScrollerStateIsLocked])
        return;

    [m_scrollbarPainterController mouseMovedInContentArea];
}

void ScrollAnimatorMac::mouseEnteredScrollbar(Scrollbar* scrollbar) const
{
    // At this time, only legacy scrollbars needs to send notifications here.
    if (recommendedScrollerStyle() != NSScrollerStyleLegacy)
        return;

    if ([m_scrollbarPainterController overlayScrollerStateIsLocked])
        return;

    if (!supportsUIStateTransitionProgress())
        return;
    if (ScrollbarPainter painter = scrollbarPainterForScrollbar(scrollbar))
        [painter mouseEnteredScroller];
}

void ScrollAnimatorMac::mouseExitedScrollbar(Scrollbar* scrollbar) const
{
    // At this time, only legacy scrollbars needs to send notifications here.
    if (recommendedScrollerStyle() != NSScrollerStyleLegacy)
        return;

    if ([m_scrollbarPainterController overlayScrollerStateIsLocked])
        return;

    if (!supportsUIStateTransitionProgress())
        return;
    if (ScrollbarPainter painter = scrollbarPainterForScrollbar(scrollbar))
        [painter mouseExitedScroller];
}

void ScrollAnimatorMac::willStartLiveResize()
{
    if ([m_scrollbarPainterController overlayScrollerStateIsLocked])
        return;

    [m_scrollbarPainterController startLiveResize];
}

void ScrollAnimatorMac::contentsResized() const
{
    if ([m_scrollbarPainterController overlayScrollerStateIsLocked])
        return;

    [m_scrollbarPainterController contentAreaDidResize];
}

void ScrollAnimatorMac::willEndLiveResize()
{
    if ([m_scrollbarPainterController overlayScrollerStateIsLocked])
        return;

    [m_scrollbarPainterController endLiveResize];
}

void ScrollAnimatorMac::contentAreaDidShow() const
{
    if ([m_scrollbarPainterController overlayScrollerStateIsLocked])
        return;

    [m_scrollbarPainterController windowOrderedIn];
}

void ScrollAnimatorMac::contentAreaDidHide() const
{
    if ([m_scrollbarPainterController overlayScrollerStateIsLocked])
        return;

    [m_scrollbarPainterController windowOrderedOut];
}

void ScrollAnimatorMac::didBeginScrollGesture() const
{
    if ([m_scrollbarPainterController overlayScrollerStateIsLocked])
        return;

    [m_scrollbarPainterController beginScrollGesture];
}

void ScrollAnimatorMac::didEndScrollGesture() const
{
    if ([m_scrollbarPainterController overlayScrollerStateIsLocked])
        return;

    [m_scrollbarPainterController endScrollGesture];
}

void ScrollAnimatorMac::mayBeginScrollGesture() const
{
    if ([m_scrollbarPainterController overlayScrollerStateIsLocked])
        return;

    [m_scrollbarPainterController beginScrollGesture];
    [m_scrollbarPainterController contentAreaScrolled];
}

void ScrollAnimatorMac::lockOverlayScrollbarStateToHidden(bool shouldLockState)
{
    if (shouldLockState)
        [m_scrollbarPainterController lockOverlayScrollerState:ScrollbarOverlayStateHidden];
    else {
        [m_scrollbarPainterController unlockOverlayScrollerState];

        // We never update scroller style for PainterControllers that are locked. If we have a pending
        // need to update the style, do it once we've unlocked the scroller state.
        if (m_needsScrollerStyleUpdate)
            updateScrollerStyle();
    }
}

bool ScrollAnimatorMac::scrollbarsCanBeActive() const
{
    return ![m_scrollbarPainterController overlayScrollerStateIsLocked];
}

void ScrollAnimatorMac::didAddVerticalScrollbar(Scrollbar* scrollbar)
{
    ScrollbarPainter painter = scrollbarPainterForScrollbar(scrollbar);
    if (!painter)
        return;

    ASSERT(!m_verticalScrollbarPainterDelegate);
    m_verticalScrollbarPainterDelegate = adoptNS([[WebScrollbarPainterDelegate alloc] initWithScrollbar:scrollbar]);

    [painter setDelegate:(id)m_verticalScrollbarPainterDelegate.get()];
    if (GraphicsLayer* layer = scrollbar->scrollableArea()->layerForVerticalScrollbar())
        [painter setLayer:layer->platformLayer()];

    [m_scrollbarPainterController setVerticalScrollerImp:painter];
    if (scrollableArea()->inLiveResize())
        [painter setKnobAlpha:1];
}

void ScrollAnimatorMac::willRemoveVerticalScrollbar(Scrollbar* scrollbar)
{
    ScrollbarPainter painter = scrollbarPainterForScrollbar(scrollbar);
    if (!painter)
        return;

    ASSERT(m_verticalScrollbarPainterDelegate);
    [m_verticalScrollbarPainterDelegate invalidate];
    m_verticalScrollbarPainterDelegate = nullptr;

    [painter setDelegate:nil];
    [m_scrollbarPainterController setVerticalScrollerImp:nil];
}

void ScrollAnimatorMac::didAddHorizontalScrollbar(Scrollbar* scrollbar)
{
    ScrollbarPainter painter = scrollbarPainterForScrollbar(scrollbar);
    if (!painter)
        return;

    ASSERT(!m_horizontalScrollbarPainterDelegate);
    m_horizontalScrollbarPainterDelegate = adoptNS([[WebScrollbarPainterDelegate alloc] initWithScrollbar:scrollbar]);

    [painter setDelegate:(id)m_horizontalScrollbarPainterDelegate.get()];
    if (GraphicsLayer* layer = scrollbar->scrollableArea()->layerForHorizontalScrollbar())
        [painter setLayer:layer->platformLayer()];

    [m_scrollbarPainterController setHorizontalScrollerImp:painter];
    if (scrollableArea()->inLiveResize())
        [painter setKnobAlpha:1];
}

void ScrollAnimatorMac::willRemoveHorizontalScrollbar(Scrollbar* scrollbar)
{
    ScrollbarPainter painter = scrollbarPainterForScrollbar(scrollbar);
    if (!painter)
        return;

    ASSERT(m_horizontalScrollbarPainterDelegate);
    [m_horizontalScrollbarPainterDelegate invalidate];
    m_horizontalScrollbarPainterDelegate = nullptr;

    [painter setDelegate:nil];
    [m_scrollbarPainterController setHorizontalScrollerImp:nil];
}

void ScrollAnimatorMac::verticalScrollbarLayerDidChange()
{
    GraphicsLayer* layer = m_scrollableArea->layerForVerticalScrollbar();
    Scrollbar* scrollbar = m_scrollableArea->verticalScrollbar();
    if (!scrollbar)
        return;

    ScrollbarPainter painter = scrollbarPainterForScrollbar(scrollbar);
    if (!painter)
        return;

    [painter setLayer:layer ? layer->platformLayer() : nil];
}

void ScrollAnimatorMac::horizontalScrollbarLayerDidChange()
{
    GraphicsLayer* layer = m_scrollableArea->layerForHorizontalScrollbar();
    Scrollbar* scrollbar = m_scrollableArea->horizontalScrollbar();
    if (!scrollbar)
        return;

    ScrollbarPainter painter = scrollbarPainterForScrollbar(scrollbar);
    if (!painter)
        return;

    [painter setLayer:layer ? layer->platformLayer() : nil];
}

bool ScrollAnimatorMac::shouldScrollbarParticipateInHitTesting(Scrollbar* scrollbar)
{
    // Non-overlay scrollbars should always participate in hit testing.
    if (recommendedScrollerStyle() != NSScrollerStyleOverlay)
        return true;

    if (scrollbar->isAlphaLocked())
        return true;

    // Overlay scrollbars should participate in hit testing whenever they are at all visible.
    ScrollbarPainter painter = scrollbarPainterForScrollbar(scrollbar);
    if (!painter)
        return false;
    return [painter knobAlpha] > 0;
}

void ScrollAnimatorMac::notifyContentAreaScrolled(const FloatSize& delta)
{
    // This function is called when a page is going into the page cache, but the page
    // isn't really scrolling in that case. We should only pass the message on to the
    // ScrollbarPainterController when we're really scrolling on an active page.
    if ([m_scrollbarPainterController overlayScrollerStateIsLocked])
        return;

    if (m_scrollableArea->isHandlingWheelEvent())
        sendContentAreaScrolled(delta);
    else
        sendContentAreaScrolledSoon(delta);
}

void ScrollAnimatorMac::cancelAnimations()
{
    m_haveScrolledSincePageLoad = false;

    if (scrollbarPaintTimerIsActive())
        stopScrollbarPaintTimer();
    [m_horizontalScrollbarPainterDelegate cancelAnimations];
    [m_verticalScrollbarPainterDelegate cancelAnimations];
}

void ScrollAnimatorMac::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    // This may not have been set to true yet if the wheel event was handled by the ScrollingTree,
    // So set it to true here.
    m_haveScrolledSincePageLoad = true;

    if (phase == PlatformWheelEventPhaseBegan)
        didBeginScrollGesture();
    else if (phase == PlatformWheelEventPhaseEnded || phase == PlatformWheelEventPhaseCancelled)
        didEndScrollGesture();
    else if (phase == PlatformWheelEventPhaseMayBegin)
        mayBeginScrollGesture();
}

#if ENABLE(RUBBER_BANDING)
bool ScrollAnimatorMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    m_haveScrolledSincePageLoad = true;

    if (!wheelEvent.hasPreciseScrollingDeltas() || !rubberBandingEnabledForSystem())
        return ScrollAnimator::handleWheelEvent(wheelEvent);

    // FIXME: This is somewhat roundabout hack to allow forwarding wheel events
    // up to the parent scrollable area. It takes advantage of the fact that
    // the base class implementation of handleWheelEvent will not accept the
    // wheel event if there is nowhere to scroll.
    if (fabsf(wheelEvent.deltaY()) >= fabsf(wheelEvent.deltaX())) {
        if (!allowsVerticalStretching())
            return ScrollAnimator::handleWheelEvent(wheelEvent);
    } else {
        if (!allowsHorizontalStretching())
            return ScrollAnimator::handleWheelEvent(wheelEvent);
    }

    bool didHandleEvent = m_scrollElasticityController.handleWheelEvent(wheelEvent);

    if (didHandleEvent)
        handleWheelEventPhase(wheelEvent.phase());

    return didHandleEvent;
}

bool ScrollAnimatorMac::pinnedInDirection(float deltaX, float deltaY)
{
    FloatSize limitDelta;
    if (fabsf(deltaY) >= fabsf(deltaX)) {
        if (deltaY < 0) {
            // We are trying to scroll up.  Make sure we are not pinned to the top
            limitDelta.setHeight(m_scrollableArea->visibleContentRect().y() + m_scrollableArea->scrollOrigin().y());
        } else {
            // We are trying to scroll down.  Make sure we are not pinned to the bottom
            limitDelta.setHeight(m_scrollableArea->totalContentsSize().height() - (m_scrollableArea->visibleContentRect().maxY() + m_scrollableArea->scrollOrigin().y()));
        }
    } else if (deltaX != 0) {
        if (deltaX < 0) {
            // We are trying to scroll left.  Make sure we are not pinned to the left
            limitDelta.setWidth(m_scrollableArea->visibleContentRect().x() + m_scrollableArea->scrollOrigin().x());
        } else {
            // We are trying to scroll right.  Make sure we are not pinned to the right
            limitDelta.setWidth(m_scrollableArea->totalContentsSize().width() - (m_scrollableArea->visibleContentRect().maxX() + m_scrollableArea->scrollOrigin().x()));
        }
    }
    
    if ((deltaX != 0 || deltaY != 0) && (limitDelta.width() < 1 && limitDelta.height() < 1))
        return true;
    return false;
}

bool ScrollAnimatorMac::allowsVerticalStretching()
{
    switch (m_scrollableArea->verticalScrollElasticity()) {
    case ScrollElasticityAutomatic: {
        Scrollbar* hScroller = m_scrollableArea->horizontalScrollbar();
        Scrollbar* vScroller = m_scrollableArea->verticalScrollbar();
        return (((vScroller && vScroller->enabled()) || (!hScroller || !hScroller->enabled())));
    }
    case ScrollElasticityNone:
        return false;
    case ScrollElasticityAllowed:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool ScrollAnimatorMac::allowsHorizontalStretching()
{
    switch (m_scrollableArea->horizontalScrollElasticity()) {
    case ScrollElasticityAutomatic: {
        Scrollbar* hScroller = m_scrollableArea->horizontalScrollbar();
        Scrollbar* vScroller = m_scrollableArea->verticalScrollbar();
        return (((hScroller && hScroller->enabled()) || (!vScroller || !vScroller->enabled())));
    }
    case ScrollElasticityNone:
        return false;
    case ScrollElasticityAllowed:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

IntSize ScrollAnimatorMac::stretchAmount()
{
    return m_scrollableArea->overhangAmount();
}

bool ScrollAnimatorMac::pinnedInDirection(const FloatSize& direction)
{
    return pinnedInDirection(direction.width(), direction.height());
}

bool ScrollAnimatorMac::canScrollHorizontally()
{
    Scrollbar* scrollbar = m_scrollableArea->horizontalScrollbar();
    if (!scrollbar)
        return false;
    return scrollbar->enabled();
}

bool ScrollAnimatorMac::canScrollVertically()
{
    Scrollbar* scrollbar = m_scrollableArea->verticalScrollbar();
    if (!scrollbar)
        return false;
    return scrollbar->enabled();
}

bool ScrollAnimatorMac::shouldRubberBandInDirection(ScrollDirection)
{
    return false;
}

IntPoint ScrollAnimatorMac::absoluteScrollPosition()
{
    return m_scrollableArea->visibleContentRect().location() + m_scrollableArea->scrollOrigin();
}

void ScrollAnimatorMac::immediateScrollByWithoutContentEdgeConstraints(const FloatSize& delta)
{
    m_scrollableArea->setConstrainsScrollingToContentEdge(false);
    immediateScrollBy(delta);
    m_scrollableArea->setConstrainsScrollingToContentEdge(true);
}

void ScrollAnimatorMac::immediateScrollBy(const FloatSize& delta)
{
    FloatPoint newPos = adjustScrollPositionIfNecessary(FloatPoint(m_currentPosX, m_currentPosY) + delta);
    if (newPos.x() == m_currentPosX && newPos.y() == m_currentPosY)
        return;

    FloatSize adjustedDelta = FloatSize(newPos.x() - m_currentPosX, newPos.y() - m_currentPosY);

    m_currentPosX = newPos.x();
    m_currentPosY = newPos.y();
    notifyPositionChanged(adjustedDelta);
}

void ScrollAnimatorMac::startSnapRubberbandTimer()
{
    m_snapRubberBandTimer.startRepeating(1.0 / 60.0);
}

void ScrollAnimatorMac::stopSnapRubberbandTimer()
{
    m_snapRubberBandTimer.stop();
}

void ScrollAnimatorMac::snapRubberBandTimerFired(Timer<ScrollAnimatorMac>&)
{
    m_scrollElasticityController.snapRubberBandTimerFired();
}
#endif

void ScrollAnimatorMac::updateScrollerStyle()
{
    if ([m_scrollbarPainterController overlayScrollerStateIsLocked]) {
        m_needsScrollerStyleUpdate = true;
        return;
    }

    ScrollbarThemeMac* macTheme = macScrollbarTheme();
    if (!macTheme) {
        m_needsScrollerStyleUpdate = false;
        return;
    }
    
    macTheme->usesOverlayScrollbarsChanged();

    NSScrollerStyle newStyle = [m_scrollbarPainterController scrollerStyle];

    if (Scrollbar* verticalScrollbar = scrollableArea()->verticalScrollbar()) {
        verticalScrollbar->invalidate();

        ScrollbarPainter oldVerticalPainter = [m_scrollbarPainterController verticalScrollerImp];
        ScrollbarPainter newVerticalPainter = [NSClassFromString(@"NSScrollerImp") scrollerImpWithStyle:newStyle 
                                                                                    controlSize:(NSControlSize)verticalScrollbar->controlSize() 
                                                                                    horizontal:NO 
                                                                                    replacingScrollerImp:oldVerticalPainter];
        [m_scrollbarPainterController setVerticalScrollerImp:newVerticalPainter];
        macTheme->setNewPainterForScrollbar(verticalScrollbar, newVerticalPainter);

        // The different scrollbar styles have different thicknesses, so we must re-set the 
        // frameRect to the new thickness, and the re-layout below will ensure the position
        // and length are properly updated.
        int thickness = macTheme->scrollbarThickness(verticalScrollbar->controlSize());
        verticalScrollbar->setFrameRect(IntRect(0, 0, thickness, thickness));
    }

    if (Scrollbar* horizontalScrollbar = scrollableArea()->horizontalScrollbar()) {
        horizontalScrollbar->invalidate();

        ScrollbarPainter oldHorizontalPainter = [m_scrollbarPainterController horizontalScrollerImp];
        ScrollbarPainter newHorizontalPainter = [NSClassFromString(@"NSScrollerImp") scrollerImpWithStyle:newStyle 
                                                                                    controlSize:(NSControlSize)horizontalScrollbar->controlSize() 
                                                                                    horizontal:YES 
                                                                                    replacingScrollerImp:oldHorizontalPainter];
        [m_scrollbarPainterController setHorizontalScrollerImp:newHorizontalPainter];
        macTheme->setNewPainterForScrollbar(horizontalScrollbar, newHorizontalPainter);

        // The different scrollbar styles have different thicknesses, so we must re-set the 
        // frameRect to the new thickness, and the re-layout below will ensure the position
        // and length are properly updated.
        int thickness = macTheme->scrollbarThickness(horizontalScrollbar->controlSize());
        horizontalScrollbar->setFrameRect(IntRect(0, 0, thickness, thickness));
    }

    // If m_needsScrollerStyleUpdate is true, then the page is restoring from the page cache, and 
    // a relayout will happen on its own. Otherwise, we must initiate a re-layout ourselves.
    scrollableArea()->scrollbarStyleChanged(newStyle, !m_needsScrollerStyleUpdate);

    m_needsScrollerStyleUpdate = false;
}

void ScrollAnimatorMac::startScrollbarPaintTimer()
{
    m_initialScrollbarPaintTimer.startOneShot(0.1);
}

bool ScrollAnimatorMac::scrollbarPaintTimerIsActive() const
{
    return m_initialScrollbarPaintTimer.isActive();
}

void ScrollAnimatorMac::stopScrollbarPaintTimer()
{
    m_initialScrollbarPaintTimer.stop();
}

void ScrollAnimatorMac::initialScrollbarPaintTimerFired(Timer<ScrollAnimatorMac>&)
{
    // To force the scrollbars to flash, we have to call hide first. Otherwise, the ScrollbarPainterController
    // might think that the scrollbars are already showing and bail early.
    [m_scrollbarPainterController hideOverlayScrollers];
    [m_scrollbarPainterController flashScrollers];
}

void ScrollAnimatorMac::sendContentAreaScrolledSoon(const FloatSize& delta)
{
    m_contentAreaScrolledTimerScrollDelta = delta;

    if (!m_sendContentAreaScrolledTimer.isActive())
        m_sendContentAreaScrolledTimer.startOneShot(0);
}

void ScrollAnimatorMac::sendContentAreaScrolled(const FloatSize& delta)
{
    if (supportsContentAreaScrolledInDirection())
        [m_scrollbarPainterController contentAreaScrolledInDirection:NSMakePoint(delta.width(), delta.height())];
    else
        [m_scrollbarPainterController contentAreaScrolled];
}

void ScrollAnimatorMac::sendContentAreaScrolledTimerFired(Timer<ScrollAnimatorMac>&)
{
    sendContentAreaScrolled(m_contentAreaScrolledTimerScrollDelta);
    m_contentAreaScrolledTimerScrollDelta = FloatSize();
}

void ScrollAnimatorMac::setVisibleScrollerThumbRect(const IntRect& scrollerThumb)
{
    IntRect rectInViewCoordinates = scrollerThumb;
    if (Scrollbar* verticalScrollbar = m_scrollableArea->verticalScrollbar())
        rectInViewCoordinates = verticalScrollbar->convertToContainingView(scrollerThumb);

    if (rectInViewCoordinates == m_visibleScrollerThumbRect)
        return;

    m_scrollableArea->setVisibleScrollerThumbRect(rectInViewCoordinates);
    m_visibleScrollerThumbRect = rectInViewCoordinates;
}

} // namespace WebCore

#endif // ENABLE(SMOOTH_SCROLLING)
