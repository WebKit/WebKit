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

#include "FloatPoint.h"
#include "IntRect.h"
#include "PlatformGestureEvent.h"
#include "PlatformWheelEvent.h"
#include "ScrollView.h"
#include "ScrollableArea.h"
#include "ScrollbarTheme.h"
#include "ScrollbarThemeMac.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/UnusedParam.h>

@interface NSObject (NSScrollAnimationHelperDetails)
- (id)initWithDelegate:(id)delegate;
- (void)_stopRun;
- (BOOL)_isAnimating;
- (NSPoint)targetOrigin;
@end

@interface ScrollAnimationHelperDelegate : NSObject
{
    WebCore::ScrollAnimatorMac* _animator;
}

- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorMac*)scrollAnimator;

- (NSRect)bounds;
- (void)_immediateScrollToPoint:(NSPoint)newPosition;
- (NSSize)convertSizeToBase:(NSSize)size;
- (NSSize)convertSizeFromBase:(NSSize)size;

- (id)superview; // Return nil.
- (id)documentView; // Return nil.
- (id)window; // Return nil.
- (void)_recursiveRecomputeToolTips; // No-op.
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

@implementation ScrollAnimationHelperDelegate

- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorMac*)scrollAnimator
{
    self = [super init];
    if (!self)
        return nil;

    _animator = scrollAnimator;
    return self;
}

- (NSRect)bounds
{
    WebCore::FloatPoint currentPosition = _animator->currentPosition();
    return NSMakeRect(currentPosition.x(), currentPosition.y(), 0, 0);
}

- (void)_immediateScrollToPoint:(NSPoint)newPosition
{
    _animator->immediateScrollToPoint(newPosition);
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

#if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)
@interface ScrollbarPainterControllerDelegate : NSObject
{
    WebCore::ScrollAnimatorMac* _animator;
}

- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorMac*)scrollAnimator;

- (NSRect)contentAreaRectForScrollerImpPair:(id)scrollerImpPair;
- (BOOL)inLiveResizeForScrollerImpPair:(id)scrollerImpPair;
- (NSPoint)mouseLocationInContentAreaForScrollerImpPair:(id)scrollerImpPair;
- (NSPoint)scrollerImpPair:(id)scrollerImpPair convertContentPoint:(NSPoint)pointInContentArea toScrollerImp:(id)scrollerImp;
- (void)scrollerImpPair:(id)scrollerImpPair setContentAreaNeedsDisplayInRect:(NSRect)rect;
- (void)scrollerImpPair:(id)scrollerImpPair updateScrollerStyleForNewRecommendedScrollerStyle:(NSScrollerStyle)newRecommendedScrollerStyle;

@end

@implementation ScrollbarPainterControllerDelegate

- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorMac*)scrollAnimator
{
    self = [super init];
    if (!self)
        return nil;
    
    _animator = scrollAnimator;
    return self;
}

- (NSRect)contentAreaRectForScrollerImpPair:(id)scrollerImpPair
{
    UNUSED_PARAM(scrollerImpPair);
    WebCore::IntSize contentsSize = _animator->scrollableArea()->contentsSize();
    return CGRectMake(0, 0, contentsSize.width(), contentsSize.height());
}

- (BOOL)inLiveResizeForScrollerImpPair:(id)scrollerImpPair
{
    UNUSED_PARAM(scrollerImpPair);
    return _animator->scrollableArea()->inLiveResize();
}

- (NSPoint)mouseLocationInContentAreaForScrollerImpPair:(id)scrollerImpPair
{
    UNUSED_PARAM(scrollerImpPair);
    return _animator->scrollableArea()->currentMousePosition();
}

- (NSPoint)scrollerImpPair:(id)scrollerImpPair convertContentPoint:(NSPoint)pointInContentArea toScrollerImp:(id)scrollerImp
{
    UNUSED_PARAM(scrollerImpPair);
    UNUSED_PARAM(scrollerImp);
    return pointInContentArea;
}

- (void)scrollerImpPair:(id)scrollerImpPair setContentAreaNeedsDisplayInRect:(NSRect)rect
{
    UNUSED_PARAM(scrollerImpPair);
    UNUSED_PARAM(rect);
}

- (void)scrollerImpPair:(id)scrollerImpPair updateScrollerStyleForNewRecommendedScrollerStyle:(NSScrollerStyle)newRecommendedScrollerStyle
{
    WKScrollbarPainterControllerRef painterController = (WKScrollbarPainterControllerRef)scrollerImpPair;
    WebCore::ScrollbarThemeMac* macTheme = (WebCore::ScrollbarThemeMac*)WebCore::ScrollbarTheme::nativeTheme();

    WKScrollbarPainterRef oldVerticalPainter = wkVerticalScrollbarPainterForController(painterController);
    if (oldVerticalPainter) {
        WebCore::Scrollbar* verticalScrollbar = _animator->scrollableArea()->verticalScrollbar();
        WKScrollbarPainterRef newVerticalPainter = wkMakeScrollbarReplacementPainter(oldVerticalPainter,
                                                                                     newRecommendedScrollerStyle,
                                                                                     verticalScrollbar->controlSize(),
                                                                                     false);
        macTheme->setNewPainterForScrollbar(verticalScrollbar, newVerticalPainter);
    }

    WKScrollbarPainterRef oldHorizontalPainter = wkHorizontalScrollbarPainterForController(painterController);
    if (oldHorizontalPainter) {
        WebCore::Scrollbar* horizontalScrollbar = _animator->scrollableArea()->horizontalScrollbar();
        WKScrollbarPainterRef newHorizontalPainter = wkMakeScrollbarReplacementPainter(oldHorizontalPainter,
                                                                                       newRecommendedScrollerStyle,
                                                                                       horizontalScrollbar->controlSize(),
                                                                                       true);
        macTheme->setNewPainterForScrollbar(horizontalScrollbar, newHorizontalPainter);
    }

    wkSetScrollbarPainterControllerStyle(painterController, newRecommendedScrollerStyle);
}

@end

@interface ScrollKnobAnimation : NSAnimation
{
    RetainPtr<WKScrollbarPainterRef> _scrollerPainter;
    WebCore::ScrollAnimatorMac* _animator;
    CGFloat _initialKnobAlpha;
    CGFloat _newKnobAlpha;
}

- (id)initWithScrollbarPainter:(WKScrollbarPainterRef)scrollerPainter forScrollAnimator:(WebCore::ScrollAnimatorMac*)scrollAnimator animateKnobAlphaTo:(CGFloat)newKnobAlpha duration:(NSTimeInterval)duration;

@end

@implementation ScrollKnobAnimation

- (id)initWithScrollbarPainter:(WKScrollbarPainterRef)scrollerPainter forScrollAnimator:(WebCore::ScrollAnimatorMac*)scrollAnimator animateKnobAlphaTo:(CGFloat)newKnobAlpha duration:(NSTimeInterval)duration
{
    self = [super initWithDuration:duration animationCurve:NSAnimationEaseInOut];
    if (!self)
        return nil;
    
    _scrollerPainter = scrollerPainter;
    _animator = scrollAnimator;
    _initialKnobAlpha = wkScrollbarPainterKnobAlpha(_scrollerPainter.get());
    _newKnobAlpha = newKnobAlpha;
    
    return self;    
}

- (void)setCurrentProgress:(NSAnimationProgress)progress
{
    [super setCurrentProgress:progress];

    CGFloat currentAlpha;
    if (_initialKnobAlpha > _newKnobAlpha)
        currentAlpha = 1 - progress;
    else
        currentAlpha = progress;
    wkSetScrollbarPainterKnobAlpha(_scrollerPainter.get(), currentAlpha);

    // Invalidate the scrollbars so that they paint the animation
    if (WebCore::Scrollbar* verticalScrollbar = _animator->scrollableArea()->verticalScrollbar())
        _animator->scrollableArea()->invalidateScrollbarRect(verticalScrollbar, WebCore::IntRect(0, 0, verticalScrollbar->width(), verticalScrollbar->height()));
    if (WebCore::Scrollbar* horizontalScrollbar = _animator->scrollableArea()->horizontalScrollbar())
        _animator->scrollableArea()->invalidateScrollbarRect(horizontalScrollbar, WebCore::IntRect(0, 0, horizontalScrollbar->width(), horizontalScrollbar->height()));
}

@end

@interface ScrollbarPainterDelegate : NSObject<NSAnimationDelegate>
{
    WebCore::ScrollAnimatorMac* _animator;
    RetainPtr<ScrollKnobAnimation> _verticalKnobAnimation;
    RetainPtr<ScrollKnobAnimation> _horizontalKnobAnimation;
}

- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorMac*)scrollAnimator;

- (NSRect)convertRectToBacking:(NSRect)aRect;
- (NSRect)convertRectFromBacking:(NSRect)aRect;
- (CALayer *)layer;

- (void)scrollerImp:(id)scrollerImp animateKnobAlphaTo:(CGFloat)newKnobAlpha duration:(NSTimeInterval)duration;
- (void)scrollerImp:(id)scrollerImp animateTrackAlphaTo:(CGFloat)newTrackAlpha duration:(NSTimeInterval)duration;
- (void)scrollerImp:(id)scrollerImp overlayScrollerStateChangedTo:(NSUInteger)newOverlayScrollerState;

@end

@implementation ScrollbarPainterDelegate

- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorMac*)scrollAnimator
{
    self = [super init];
    if (!self)
        return nil;
    
    _animator = scrollAnimator;
    return self;
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
    return nil;
}

- (void)setUpAnimation:(RetainPtr<ScrollKnobAnimation>)scrollKnobAnimation scrollerPainter:(WKScrollbarPainterRef)scrollerPainter animateKnobAlphaTo:(CGFloat)newKnobAlpha duration:(NSTimeInterval)duration
{
    // If we are currently animating, stop
    if (scrollKnobAnimation) {
        [scrollKnobAnimation.get() stopAnimation];
        scrollKnobAnimation = nil;
    }
    
    scrollKnobAnimation.adoptNS([[ScrollKnobAnimation alloc] initWithScrollbarPainter:scrollerPainter 
                                                                    forScrollAnimator:_animator 
                                                                    animateKnobAlphaTo:newKnobAlpha 
                                                                    duration:duration]);
    [scrollKnobAnimation.get() setAnimationBlockingMode:NSAnimationNonblocking];
    [scrollKnobAnimation.get() startAnimation];
}

- (void)scrollerImp:(id)scrollerImp animateKnobAlphaTo:(CGFloat)newKnobAlpha duration:(NSTimeInterval)duration
{
    WKScrollbarPainterRef scrollerPainter = (WKScrollbarPainterRef)scrollerImp;

    if (wkScrollbarPainterIsHorizontal(scrollerPainter))
        [self setUpAnimation:_horizontalKnobAnimation scrollerPainter:scrollerPainter animateKnobAlphaTo:newKnobAlpha duration:duration];
    else
        [self setUpAnimation:_verticalKnobAnimation scrollerPainter:scrollerPainter animateKnobAlphaTo:newKnobAlpha duration:duration];
}

- (void)scrollerImp:(id)scrollerImp animateTrackAlphaTo:(CGFloat)newTrackAlpha duration:(NSTimeInterval)duration
{
    // FIXME: Implement.
    UNUSED_PARAM(scrollerImp);
    UNUSED_PARAM(newTrackAlpha);
    UNUSED_PARAM(duration);
}

- (void)scrollerImp:(id)scrollerImp overlayScrollerStateChangedTo:(NSUInteger)newOverlayScrollerState
{
    wkScrollbarPainterSetOverlayState((WKScrollbarPainterRef)scrollerImp, newOverlayScrollerState);
}

@end
#endif // #if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)

namespace WebCore {

PassOwnPtr<ScrollAnimator> ScrollAnimator::create(ScrollableArea* scrollableArea)
{
    return adoptPtr(new ScrollAnimatorMac(scrollableArea));
}

ScrollAnimatorMac::ScrollAnimatorMac(ScrollableArea* scrollableArea)
    : ScrollAnimator(scrollableArea)
#if ENABLE(RUBBER_BANDING)
    , m_inScrollGesture(false)
    , m_momentumScrollInProgress(false)
    , m_ignoreMomentumScrolls(false)
    , m_lastMomemtumScrollTimestamp(0)
    , m_startTime(0)
    , m_snapRubberBandTimer(this, &ScrollAnimatorMac::snapRubberBandTimerFired)
#endif
{
    m_scrollAnimationHelperDelegate.adoptNS([[ScrollAnimationHelperDelegate alloc] initWithScrollAnimator:this]);
    m_scrollAnimationHelper.adoptNS([[NSClassFromString(@"NSScrollAnimationHelper") alloc] initWithDelegate:m_scrollAnimationHelperDelegate.get()]);

#if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)
    m_scrollbarPainterControllerDelegate.adoptNS([[ScrollbarPainterControllerDelegate alloc] initWithScrollAnimator:this]);
    m_scrollbarPainterController = wkMakeScrollbarPainterController(m_scrollbarPainterControllerDelegate.get());
    m_scrollbarPainterDelegate.adoptNS([[ScrollbarPainterDelegate alloc] initWithScrollAnimator:this]);
#endif
}

ScrollAnimatorMac::~ScrollAnimatorMac()
{
}

id ScrollAnimatorMac::scrollbarPainterDelegate()
{
#if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)
    return m_scrollbarPainterDelegate.get();
#else
    return nil;
#endif
}

#if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)
void ScrollAnimatorMac::setPainterForPainterController(WKScrollbarPainterRef painter, bool isHorizontal)
{
    wkSetPainterForPainterController(m_scrollbarPainterController.get(), painter, isHorizontal);
}

void ScrollAnimatorMac::removePainterFromPainterController(ScrollbarOrientation orientation)
{
    wkSetPainterForPainterController(m_scrollbarPainterController.get(), nil, orientation == HorizontalScrollbar);
}
#endif // #if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)

bool ScrollAnimatorMac::scroll(ScrollbarOrientation orientation, ScrollGranularity granularity, float step, float multiplier)
{
    if (![[NSUserDefaults standardUserDefaults] boolForKey:@"AppleScrollAnimationEnabled"])
        return ScrollAnimator::scroll(orientation, granularity, step, multiplier);

    if (granularity == ScrollByPixel)
        return ScrollAnimator::scroll(orientation, granularity, step, multiplier);

    float currentPos = orientation == HorizontalScrollbar ? m_currentPosX : m_currentPosY;
    float newPos = std::max<float>(std::min<float>(currentPos + (step * multiplier), static_cast<float>(m_scrollableArea->scrollSize(orientation))), 0);
    if (currentPos == newPos)
        return false;

    NSPoint newPoint;
    if ([m_scrollAnimationHelper.get() _isAnimating]) {
        NSPoint targetOrigin = [m_scrollAnimationHelper.get() targetOrigin];
        newPoint = orientation == HorizontalScrollbar ? NSMakePoint(newPos, targetOrigin.y) : NSMakePoint(targetOrigin.x, newPos);
    } else
        newPoint = orientation == HorizontalScrollbar ? NSMakePoint(newPos, m_currentPosY) : NSMakePoint(m_currentPosX, newPos);

    [m_scrollAnimationHelper.get() scrollToPoint:newPoint];
    return true;
}

void ScrollAnimatorMac::scrollToOffsetWithoutAnimation(const FloatPoint& offset)
{
    [m_scrollAnimationHelper.get() _stopRun];
    ScrollAnimator::scrollToOffsetWithoutAnimation(offset);
}

void ScrollAnimatorMac::immediateScrollToPoint(const FloatPoint& newPosition)
{
    m_currentPosX = newPosition.x();
    m_currentPosY = newPosition.y();
    notityPositionChanged();
}

void ScrollAnimatorMac::immediateScrollByDeltaX(float deltaX)
{
    m_currentPosX += deltaX;
    notityPositionChanged();
}

void ScrollAnimatorMac::immediateScrollByDeltaY(float deltaY)
{
    m_currentPosY += deltaY;
    notityPositionChanged();
}

void ScrollAnimatorMac::notityPositionChanged()
{
#if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)
    wkContentAreaScrolled(m_scrollbarPainterController.get());
#endif
    ScrollAnimator::notityPositionChanged();
}

void ScrollAnimatorMac::contentAreaWillPaint() const
{
#if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)
    wkContentAreaWillPaint(m_scrollbarPainterController.get());
#endif
}

void ScrollAnimatorMac::mouseEnteredContentArea() const
{
#if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)
    wkMouseEnteredContentArea(m_scrollbarPainterController.get());
#endif
}

void ScrollAnimatorMac::mouseExitedContentArea() const
{
#if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)
    wkMouseExitedContentArea(m_scrollbarPainterController.get());
#endif
}

void ScrollAnimatorMac::mouseMovedInContentArea() const
{
#if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)
    wkMouseMovedInContentArea(m_scrollbarPainterController.get());
#endif
}

void ScrollAnimatorMac::willStartLiveResize()
{
#if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)
    wkWillStartLiveResize(m_scrollbarPainterController.get());
#endif
}

void ScrollAnimatorMac::contentsResized() const
{
#if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)
    wkContentAreaResized(m_scrollbarPainterController.get());
#endif
}

void ScrollAnimatorMac::willEndLiveResize()
{
#if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)
    wkWillEndLiveResize(m_scrollbarPainterController.get());
#endif
}

void ScrollAnimatorMac::contentAreaDidShow() const
{
#if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)
    wkContentAreaDidShow(m_scrollbarPainterController.get());
#endif
}

void ScrollAnimatorMac::contentAreaDidHide() const
{
#if defined(USE_WK_SCROLLBAR_PAINTER_AND_CONTROLLER)
    wkContentAreaDidHide(m_scrollbarPainterController.get());
#endif
}

#if ENABLE(RUBBER_BANDING)

static const float scrollVelocityZeroingTimeout = 0.10f;
static const float rubberbandStiffness = 20;
static const float rubberbandDirectionLockStretchRatio = 1;
static const float rubberbandMinimumRequiredDeltaBeforeStretch = 10;
static const float rubberbandAmplitude = 0.31f;
static const float rubberbandPeriod = 1.6f;

static float elasticDeltaForTimeDelta(float initialPosition, float initialVelocity, float elapsedTime)
{
    float amplitude = rubberbandAmplitude;
    float period = rubberbandPeriod;
    float criticalDampeningFactor = expf((-elapsedTime * rubberbandStiffness) / period);
             
    return (initialPosition + (-initialVelocity * elapsedTime * amplitude)) * criticalDampeningFactor;
}

static float elasticDeltaForReboundDelta(float delta)
{
    float stiffness = std::max(rubberbandStiffness, 1.0f);
    return delta / stiffness;
}

static float reboundDeltaForElasticDelta(float delta)
{
    return delta * rubberbandStiffness;
}

static float scrollWheelMultiplier()
{
    static float multiplier = -1;
    if (multiplier < 0) {
        multiplier = [[NSUserDefaults standardUserDefaults] floatForKey:@"NSScrollWheelMultiplier"];
        if (multiplier <= 0)
            multiplier = 1;
    }
    return multiplier;
}

void ScrollAnimatorMac::handleWheelEvent(PlatformWheelEvent& wheelEvent)
{
    if (!wheelEvent.hasPreciseScrollingDeltas()) {
        ScrollAnimator::handleWheelEvent(wheelEvent);
        return;
    }

    wheelEvent.accept();

    bool isMometumScrollEvent = (wheelEvent.phase() != PlatformWheelEventPhaseNone);
    if (m_ignoreMomentumScrolls && (isMometumScrollEvent || m_snapRubberBandTimer.isActive())) {
        if (wheelEvent.phase() == PlatformWheelEventPhaseEnded)
            m_ignoreMomentumScrolls = false;
        return;
    }

    smoothScrollWithEvent(wheelEvent);
}

void ScrollAnimatorMac::handleGestureEvent(const PlatformGestureEvent& gestureEvent)
{
    if (gestureEvent.type() == PlatformGestureEvent::ScrollBeginType)
        beginScrollGesture();
    else
        endScrollGesture();
}

bool ScrollAnimatorMac::pinnedInDirection(float deltaX, float deltaY)
{
    FloatSize limitDelta;
    if (fabsf(deltaY) >= fabsf(deltaX)) {
        if (deltaY < 0) {
            // We are trying to scroll up.  Make sure we are not pinned to the top
            limitDelta.setHeight(m_scrollableArea->visibleContentRect().y());
        } else {
            // We are trying to scroll down.  Make sure we are not pinned to the bottom
            limitDelta.setHeight(m_scrollableArea->contentsSize().height() - m_scrollableArea->visibleContentRect().bottom());
        }
    } else if (deltaX != 0) {
        if (deltaX < 0) {
            // We are trying to scroll left.  Make sure we are not pinned to the left
            limitDelta.setWidth(m_scrollableArea->visibleContentRect().x());
        } else {
            // We are trying to scroll right.  Make sure we are not pinned to the right
            limitDelta.setWidth(m_scrollableArea->contentsSize().width() - m_scrollableArea->visibleContentRect().right());
        }
    }
    
    if ((deltaX != 0 || deltaY != 0) && (limitDelta.width() < 1 && limitDelta.height() < 1))
        return true;
    return false;
}

bool ScrollAnimatorMac::allowsVerticalStretching() const
{
    Scrollbar* hScroller = m_scrollableArea->horizontalScrollbar();
    Scrollbar* vScroller = m_scrollableArea->verticalScrollbar();
    if (((vScroller && vScroller->enabled()) || (!hScroller || !hScroller->enabled())))
        return true;

    return false;
}

bool ScrollAnimatorMac::allowsHorizontalStretching() const
{
    Scrollbar* hScroller = m_scrollableArea->horizontalScrollbar();
    Scrollbar* vScroller = m_scrollableArea->verticalScrollbar();
    if (((hScroller && hScroller->enabled()) || (!vScroller || !vScroller->enabled())))
        return true;

    return false;
}

void ScrollAnimatorMac::smoothScrollWithEvent(PlatformWheelEvent& wheelEvent)
{
    float deltaX = m_overflowScrollDelta.width();
    float deltaY = m_overflowScrollDelta.height();

    // Reset overflow values because we may decide to remove delta at various points and put it into overflow.
    m_overflowScrollDelta = FloatSize();

    float eventCoallescedDeltaX = -wheelEvent.deltaX();
    float eventCoallescedDeltaY = -wheelEvent.deltaY();

    deltaX += eventCoallescedDeltaX;
    deltaY += eventCoallescedDeltaY;

    // Slightly prefer scrolling vertically by applying the = case to deltaY
    if (fabsf(deltaY) >= fabsf(deltaX))
        deltaX = 0;
    else
        deltaY = 0;
    
    bool isVerticallyStretched = false;
    bool isHorizontallyStretched = false;
    bool shouldStretch = false;
    
    IntSize stretchAmount = m_scrollableArea->overhangAmount();

    isHorizontallyStretched = stretchAmount.width();
    isVerticallyStretched = stretchAmount.height();

    PlatformWheelEventPhase phase = wheelEvent.phase();

    // If we are starting momentum scrolling then do some setup.
    if (!m_momentumScrollInProgress && (phase == PlatformWheelEventPhaseBegan || phase == PlatformWheelEventPhaseChanged))
        m_momentumScrollInProgress = true;

    CFTimeInterval timeDelta = wheelEvent.timestamp() - m_lastMomemtumScrollTimestamp;
    if (m_inScrollGesture || m_momentumScrollInProgress) {
        if (m_lastMomemtumScrollTimestamp && timeDelta > 0 && timeDelta < scrollVelocityZeroingTimeout) {
            m_momentumVelocity.setWidth(eventCoallescedDeltaX / (float)timeDelta);
            m_momentumVelocity.setHeight(eventCoallescedDeltaY / (float)timeDelta);
            m_lastMomemtumScrollTimestamp = wheelEvent.timestamp();
        } else {
            m_lastMomemtumScrollTimestamp = wheelEvent.timestamp();
            m_momentumVelocity = FloatSize();
        }

        if (isVerticallyStretched) {
            if (!isHorizontallyStretched && pinnedInDirection(deltaX, 0)) {                
                // Stretching only in the vertical.
                if (deltaY != 0 && (fabsf(deltaX / deltaY) < rubberbandDirectionLockStretchRatio))
                    deltaX = 0;
                else if (fabsf(deltaX) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                    m_overflowScrollDelta.setWidth(m_overflowScrollDelta.width() + deltaX);
                    deltaX = 0;
                } else
                    m_overflowScrollDelta.setWidth(m_overflowScrollDelta.width() + deltaX);
            }
        } else if (isHorizontallyStretched) {
            // Stretching only in the horizontal.
            if (pinnedInDirection(0, deltaY)) {
                if (deltaX != 0 && (fabsf(deltaY / deltaX) < rubberbandDirectionLockStretchRatio))
                    deltaY = 0;
                else if (fabsf(deltaY) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                    m_overflowScrollDelta.setHeight(m_overflowScrollDelta.height() + deltaY);
                    deltaY = 0;
                } else
                    m_overflowScrollDelta.setHeight(m_overflowScrollDelta.height() + deltaY);
            }
        } else {
            // Not stretching at all yet.
            if (pinnedInDirection(deltaX, deltaY)) {
                if (fabsf(deltaY) >= fabsf(deltaX)) {
                    if (fabsf(deltaX) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                        m_overflowScrollDelta.setWidth(m_overflowScrollDelta.width() + deltaX);
                        deltaX = 0;
                    } else
                        m_overflowScrollDelta.setWidth(m_overflowScrollDelta.width() + deltaX);
                }
                shouldStretch = true;
            }
        }
    }

    if (deltaX != 0 || deltaY != 0) {
        if (!(shouldStretch || isVerticallyStretched || isHorizontallyStretched)) {
            if (deltaY != 0) {
                deltaY *= scrollWheelMultiplier();
                immediateScrollByDeltaY(deltaY);
            }
            if (deltaX != 0) {
                deltaX *= scrollWheelMultiplier();
                immediateScrollByDeltaX(deltaX);
            }
        } else {
            if (!allowsHorizontalStretching()) {
                deltaX = 0;
                eventCoallescedDeltaX = 0;
            } else if ((deltaX != 0) && !isHorizontallyStretched && !pinnedInDirection(deltaX, 0)) {
                deltaX *= scrollWheelMultiplier();

                m_scrollableArea->setConstrainsScrollingToContentEdge(false);
                immediateScrollByDeltaX(deltaX);
                m_scrollableArea->setConstrainsScrollingToContentEdge(true);

                deltaX = 0;
            }
            
            if (!allowsVerticalStretching()) {
                deltaY = 0;
                eventCoallescedDeltaY = 0;
            } else if ((deltaY != 0) && !isVerticallyStretched && !pinnedInDirection(0, deltaY)) {
                deltaY *= scrollWheelMultiplier();

                m_scrollableArea->setConstrainsScrollingToContentEdge(false);
                immediateScrollByDeltaY(deltaY);
                m_scrollableArea->setConstrainsScrollingToContentEdge(true);

                deltaY = 0;
            }
            
            IntSize stretchAmount = m_scrollableArea->overhangAmount();
        
            if (m_momentumScrollInProgress) {
                if ((pinnedInDirection(eventCoallescedDeltaX, eventCoallescedDeltaY) || (fabsf(eventCoallescedDeltaX) + fabsf(eventCoallescedDeltaY) <= 0)) && m_lastMomemtumScrollTimestamp) {
                    m_ignoreMomentumScrolls = true;
                    m_momentumScrollInProgress = false;
                    snapRubberBand();
                }
            }

            m_stretchScrollForce.setWidth(m_stretchScrollForce.width() + deltaX);
            m_stretchScrollForce.setHeight(m_stretchScrollForce.height() + deltaY);

            FloatSize dampedDelta(ceilf(elasticDeltaForReboundDelta(m_stretchScrollForce.width())), ceilf(elasticDeltaForReboundDelta(m_stretchScrollForce.height())));
            FloatPoint origOrigin = m_scrollableArea->visibleContentRect().location() - stretchAmount;
            FloatPoint newOrigin = origOrigin + dampedDelta;

            if (origOrigin != newOrigin) {
                m_scrollableArea->setConstrainsScrollingToContentEdge(false);
                immediateScrollToPoint(newOrigin);
                m_scrollableArea->setConstrainsScrollingToContentEdge(true);
            }
        }
    }

    if (m_momentumScrollInProgress && phase == PlatformWheelEventPhaseEnded) {
        m_momentumScrollInProgress = false;
        m_ignoreMomentumScrolls = false;
        m_lastMomemtumScrollTimestamp = 0;
    }
}

void ScrollAnimatorMac::beginScrollGesture()
{
    m_inScrollGesture = true;
    m_momentumScrollInProgress = false;
    m_ignoreMomentumScrolls = false;
    m_lastMomemtumScrollTimestamp = 0;
    m_momentumVelocity = FloatSize();

    IntSize stretchAmount = m_scrollableArea->overhangAmount();
    m_stretchScrollForce.setWidth(reboundDeltaForElasticDelta(stretchAmount.width()));
    m_stretchScrollForce.setHeight(reboundDeltaForElasticDelta(stretchAmount.height()));

    m_overflowScrollDelta = FloatSize();
    
    if (m_snapRubberBandTimer.isActive())
        m_snapRubberBandTimer.stop();
}

void ScrollAnimatorMac::endScrollGesture()
{
    snapRubberBand();
}

void ScrollAnimatorMac::snapRubberBand()
{
    CFTimeInterval timeDelta = [[NSProcessInfo processInfo] systemUptime] - m_lastMomemtumScrollTimestamp;
    if (m_lastMomemtumScrollTimestamp && timeDelta >= scrollVelocityZeroingTimeout)
        m_momentumVelocity = FloatSize();

    m_inScrollGesture = false;

    if (m_snapRubberBandTimer.isActive())
        return;

    m_startTime = [NSDate timeIntervalSinceReferenceDate];
    m_startStretch = FloatSize();
    m_origOrigin = FloatPoint();
    m_origVelocity = FloatSize();

    m_snapRubberBandTimer.startRepeating(1.0/60.0);
}

static inline float roundTowardZero(float num)
{
    return num > 0 ? ceilf(num - 0.5f) : floorf(num + 0.5f);
}

static inline float roundToDevicePixelTowardZero(float num)
{
    float roundedNum = roundf(num);
    if (fabs(num - roundedNum) < 0.125)
        num = roundedNum;

    return roundTowardZero(num);
}

void ScrollAnimatorMac::snapRubberBandTimerFired(Timer<ScrollAnimatorMac>*)
{
    if (!m_momentumScrollInProgress || m_ignoreMomentumScrolls) {
        CFTimeInterval timeDelta = [NSDate timeIntervalSinceReferenceDate] - m_startTime;

        if (m_startStretch == FloatSize()) {
            m_startStretch = m_scrollableArea->overhangAmount();
            if (m_startStretch == FloatSize()) {    
                m_snapRubberBandTimer.stop();
                m_stretchScrollForce = FloatSize();
                m_startTime = 0;
                m_startStretch = FloatSize();
                m_origOrigin = FloatPoint();
                m_origVelocity = FloatSize();

                return;
            }

            m_origOrigin = m_scrollableArea->visibleContentRect().location() - m_startStretch;
            m_origVelocity = m_momentumVelocity;

            // Just like normal scrolling, prefer vertical rubberbanding
            if (fabsf(m_origVelocity.height()) >= fabsf(m_origVelocity.width()))
                m_origVelocity.setWidth(0);
            
            // Don't rubber-band horizontally if it's not possible to scroll horizontally
            Scrollbar* hScroller = m_scrollableArea->horizontalScrollbar();
            if (!hScroller || !hScroller->enabled())
                m_origVelocity.setWidth(0);
            
            // Don't rubber-band vertically if it's not possible to scroll horizontally
            Scrollbar* vScroller = m_scrollableArea->verticalScrollbar();
            if (!vScroller || !vScroller->enabled())
                m_origVelocity.setHeight(0);
        }

        FloatPoint delta(roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_startStretch.width(), -m_origVelocity.width(), (float)timeDelta)),
                         roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_startStretch.height(), -m_origVelocity.height(), (float)timeDelta)));

        if (fabs(delta.x()) >= 1 || fabs(delta.y()) >= 1) {
            FloatPoint newOrigin = m_origOrigin + delta;

            m_scrollableArea->setConstrainsScrollingToContentEdge(false);
            immediateScrollToPoint(newOrigin);
            m_scrollableArea->setConstrainsScrollingToContentEdge(true);

            FloatSize newStretch = m_scrollableArea->overhangAmount();
            
            m_stretchScrollForce.setWidth(reboundDeltaForElasticDelta(newStretch.width()));
            m_stretchScrollForce.setHeight(reboundDeltaForElasticDelta(newStretch.height()));
        } else {
            immediateScrollToPoint(m_origOrigin);

            m_snapRubberBandTimer.stop();
            m_stretchScrollForce = FloatSize();
            
            m_startTime = 0;
            m_startStretch = FloatSize();
            m_origOrigin = FloatPoint();
            m_origVelocity = FloatSize();
        }
    } else {
        m_startTime = [NSDate timeIntervalSinceReferenceDate];
        m_startStretch = FloatSize();
    }
}
#endif

} // namespace WebCore

#endif // ENABLE(SMOOTH_SCROLLING)
