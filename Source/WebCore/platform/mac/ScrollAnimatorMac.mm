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
#include "NSScrollerImpDetails.h"
#include "PlatformGestureEvent.h"
#include "PlatformWheelEvent.h"
#include "ScrollView.h"
#include "ScrollableArea.h"
#include "ScrollbarTheme.h"
#include "ScrollbarThemeMac.h"
#include "WebCoreSystemInterface.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/UnusedParam.h>

using namespace WebCore;
using namespace std;

#if USE(SCROLLBAR_PAINTER)
static bool supportsUIStateTransitionProgress()
{
    // FIXME: This is temporary until all platforms that support ScrollbarPainter support this part of the API.
    static bool globalSupportsUIStateTransitionProgress = [NSClassFromString(@"NSScrollerImp") instancesRespondToSelector:@selector(mouseEnteredScroller)];
    return globalSupportsUIStateTransitionProgress;
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

#if USE(SCROLLBAR_PAINTER)

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

    return _scrollableArea->currentMousePosition();
}

- (NSPoint)scrollerImpPair:(id)scrollerImpPair convertContentPoint:(NSPoint)pointInContentArea toScrollerImp:(id)scrollerImp
{
    UNUSED_PARAM(scrollerImpPair);
    if (!_scrollableArea)
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
        return WebCore::IntPoint();

    ASSERT(scrollerImp == scrollbarPainterForScrollbar(scrollbar));

    return scrollbar->convertFromContainingView(WebCore::IntPoint(pointInContentArea));
}

- (void)scrollerImpPair:(id)scrollerImpPair setContentAreaNeedsDisplayInRect:(NSRect)rect
{
    UNUSED_PARAM(scrollerImpPair);
    UNUSED_PARAM(rect);
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
    UIStateTransition
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
        [_scrollbarPainter.get() setKnobAlpha:currentValue];
        break;
    case TrackAlpha:
        [_scrollbarPainter.get() setTrackAlpha:currentValue];
        break;
    case UIStateTransition:
        [_scrollbarPainter.get() setUiStateTransitionProgress:currentValue];
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
    [_knobAlphaAnimation.get() stopAnimation];
    [_trackAlphaAnimation.get() stopAnimation];
    [_uiStateTransitionAnimation.get() stopAnimation];
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

    if (![self scrollAnimator]->isDrawingIntoLayer())
        return nil;

    // FIXME: This should attempt to return an actual layer.
    static CALayer *dummyLayer = [[CALayer alloc] init];
    return dummyLayer;
}

- (NSPoint)mouseLocationInScrollerForScrollerImp:(id)scrollerImp
{
    if (!_scrollbar)
        return NSZeroPoint;

    ScrollbarPainter scrollerPainter = (ScrollbarPainter)scrollerImp;
    Scrollbar* scrollbar;
    if ([scrollerPainter isHorizontal])
        scrollbar = [self scrollAnimator]->scrollableArea()->horizontalScrollbar();
    else 
        scrollbar = [self scrollAnimator]->scrollableArea()->verticalScrollbar();

    if (!scrollbar)
        return NSZeroPoint;

    return scrollbar->convertFromContainingView([self scrollAnimator]->scrollableArea()->currentMousePosition());
}

- (void)setUpAlphaAnimation:(RetainPtr<WebScrollbarPartAnimation>&)scrollbarPartAnimation scrollerPainter:(ScrollbarPainter)scrollerPainter part:(WebCore::ScrollbarPart)part animateAlphaTo:(CGFloat)newAlpha duration:(NSTimeInterval)duration
{
    // If the user has scrolled the page, then the scrollbars must be animated here. 
    // This overrides the early returns.
    bool mustAnimate = [self scrollAnimator]->haveScrolledSincePageLoad();

    if ([self scrollAnimator]->scrollbarPaintTimerIsActive() && !mustAnimate)
        return;

    if ([self scrollAnimator]->scrollableArea()->shouldSuspendScrollAnimations() && !mustAnimate) {
        [self scrollAnimator]->startScrollbarPaintTimer();
        return;
    }

    // At this point, we are definitely going to animate now, so stop the timer.
    [self scrollAnimator]->stopScrollbarPaintTimer();

    // If we are currently animating, stop
    if (scrollbarPartAnimation) {
        [scrollbarPartAnimation.get() stopAnimation];
        scrollbarPartAnimation = nil;
    }

    if (part == WebCore::ThumbPart && ![scrollerPainter isHorizontal]) {
        if (newAlpha == 1) {
            IntRect thumbRect = IntRect([scrollerPainter rectForPart:NSScrollerKnob]);
            [self scrollAnimator]->setVisibleScrollerThumbRect(thumbRect);
        } else
            [self scrollAnimator]->setVisibleScrollerThumbRect(IntRect());
    }

    scrollbarPartAnimation.adoptNS([[WebScrollbarPartAnimation alloc] initWithScrollbar:_scrollbar 
                                                                       featureToAnimate:part == ThumbPart ? ThumbAlpha : TrackAlpha
                                                                            animateFrom:part == ThumbPart ? [scrollerPainter knobAlpha] : [scrollerPainter trackAlpha]
                                                                              animateTo:newAlpha 
                                                                               duration:duration]);
    [scrollbarPartAnimation.get() startAnimation];
}

- (void)scrollerImp:(id)scrollerImp animateKnobAlphaTo:(CGFloat)newKnobAlpha duration:(NSTimeInterval)duration
{
    if (!_scrollbar)
        return;

    ASSERT(scrollerImp == scrollbarPainterForScrollbar(_scrollbar));

    ScrollbarPainter scrollerPainter = (ScrollbarPainter)scrollerImp;
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

    ScrollbarPainter scrollerPainter = (ScrollbarPainter)scrollerImp;

    // UIStateTransition always animates to 1. In case an animation is in progress this avoids a hard transition.
    [scrollerPainter setUiStateTransitionProgress:1 - [scrollerPainter uiStateTransitionProgress]];

    if (!_uiStateTransitionAnimation)
        _uiStateTransitionAnimation.adoptNS([[WebScrollbarPartAnimation alloc] initWithScrollbar:_scrollbar 
                                                                                featureToAnimate:UIStateTransition
                                                                                     animateFrom:[scrollerPainter uiStateTransitionProgress]
                                                                                       animateTo:1.0
                                                                                        duration:duration]);
    else {
        // If we don't need to initialize the animation, just reset the values in case they have changed.
        [_uiStateTransitionAnimation.get() setStartValue:[scrollerPainter uiStateTransitionProgress]];
        [_uiStateTransitionAnimation.get() setEndValue:1.0];
        [_uiStateTransitionAnimation.get() setDuration:duration];
    }
    [_uiStateTransitionAnimation.get() startAnimation];
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
    [_knobAlphaAnimation.get() invalidate];
    [_trackAlphaAnimation.get() invalidate];
    [_uiStateTransitionAnimation.get() invalidate];
    END_BLOCK_OBJC_EXCEPTIONS;
}

@end

#endif // USE(SCROLLBAR_PAINTER)

namespace WebCore {

PassOwnPtr<ScrollAnimator> ScrollAnimator::create(ScrollableArea* scrollableArea)
{
    return adoptPtr(new ScrollAnimatorMac(scrollableArea));
}

ScrollAnimatorMac::ScrollAnimatorMac(ScrollableArea* scrollableArea)
    : ScrollAnimator(scrollableArea)
#if USE(SCROLLBAR_PAINTER)
    , m_initialScrollbarPaintTimer(this, &ScrollAnimatorMac::initialScrollbarPaintTimerFired)
#endif
#if ENABLE(RUBBER_BANDING)
    , m_scrollElasticityController(this)
    , m_snapRubberBandTimer(this, &ScrollAnimatorMac::snapRubberBandTimerFired)
#endif
    , m_drawingIntoLayer(false)
    , m_haveScrolledSincePageLoad(false)
    , m_needsScrollerStyleUpdate(false)
{
    m_scrollAnimationHelperDelegate.adoptNS([[WebScrollAnimationHelperDelegate alloc] initWithScrollAnimator:this]);
    m_scrollAnimationHelper.adoptNS([[NSClassFromString(@"NSScrollAnimationHelper") alloc] initWithDelegate:m_scrollAnimationHelperDelegate.get()]);

#if USE(SCROLLBAR_PAINTER)
    m_scrollbarPainterControllerDelegate.adoptNS([[WebScrollbarPainterControllerDelegate alloc] initWithScrollableArea:scrollableArea]);
    m_scrollbarPainterController = [[[NSClassFromString(@"NSScrollerImpPair") alloc] init] autorelease];
    [m_scrollbarPainterController.get() setDelegate:m_scrollbarPainterControllerDelegate.get()];
    [m_scrollbarPainterController.get() setScrollerStyle:wkRecommendedScrollerStyle()];
#endif
}

ScrollAnimatorMac::~ScrollAnimatorMac()
{
#if USE(SCROLLBAR_PAINTER)
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_scrollbarPainterControllerDelegate.get() invalidate];
    [m_scrollbarPainterController.get() setDelegate:nil];
    [m_horizontalScrollbarPainterDelegate.get() invalidate];
    [m_verticalScrollbarPainterDelegate.get() invalidate];
    [m_scrollAnimationHelperDelegate.get() invalidate];
    END_BLOCK_OBJC_EXCEPTIONS;
#endif
}

bool ScrollAnimatorMac::scroll(ScrollbarOrientation orientation, ScrollGranularity granularity, float step, float multiplier)
{
    m_haveScrolledSincePageLoad = true;

    if (![[NSUserDefaults standardUserDefaults] boolForKey:@"AppleScrollAnimationEnabled"] || !m_scrollableArea->scrollAnimatorEnabled())
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
    } else {
        newPoint = orientation == HorizontalScrollbar ? NSMakePoint(newPos, m_currentPosY) : NSMakePoint(m_currentPosX, newPos);
        m_scrollableArea->didStartAnimatedScroll();
    }

    [m_scrollAnimationHelper.get() scrollToPoint:newPoint];
    return true;
}

void ScrollAnimatorMac::scrollToOffsetWithoutAnimation(const FloatPoint& offset)
{
    [m_scrollAnimationHelper.get() _stopRun];
    immediateScrollToPoint(offset);
}

float ScrollAnimatorMac::adjustScrollXPositionIfNecessary(float position) const
{
    if (!m_scrollableArea->constrainsScrollingToContentEdge())
        return position;

    return max<float>(min<float>(position, m_scrollableArea->contentsSize().width() - m_scrollableArea->visibleWidth()), 0);
}

float ScrollAnimatorMac::adjustScrollYPositionIfNecessary(float position) const
{
    if (!m_scrollableArea->constrainsScrollingToContentEdge())
        return position;

    return max<float>(min<float>(position, m_scrollableArea->contentsSize().height() - m_scrollableArea->visibleHeight()), 0);
}

FloatPoint ScrollAnimatorMac::adjustScrollPositionIfNecessary(const FloatPoint& position) const
{
    if (!m_scrollableArea->constrainsScrollingToContentEdge())
        return position;

    float newX = max<float>(min<float>(position.x(), m_scrollableArea->contentsSize().width() - m_scrollableArea->visibleWidth()), 0);
    float newY = max<float>(min<float>(position.y(), m_scrollableArea->contentsSize().height() - m_scrollableArea->visibleHeight()), 0);

    return FloatPoint(newX, newY);
}

void ScrollAnimatorMac::immediateScrollToPoint(const FloatPoint& newPosition)
{
    FloatPoint adjustedPosition = adjustScrollPositionIfNecessary(newPosition);
 
    bool positionChanged = adjustedPosition.x() != m_currentPosX || adjustedPosition.y() != m_currentPosY;
    if (!positionChanged && !scrollableArea()->scrollOriginChanged())
        return;

    m_currentPosX = adjustedPosition.x();
    m_currentPosY = adjustedPosition.y();
    notifyPositionChanged();
}

void ScrollAnimatorMac::immediateScrollByDeltaX(float deltaX)
{
    float newPosX = adjustScrollXPositionIfNecessary(m_currentPosX + deltaX);
    
    if (newPosX == m_currentPosX)
        return;
    
    m_currentPosX = newPosX;
    notifyPositionChanged();
}

void ScrollAnimatorMac::immediateScrollByDeltaY(float deltaY)
{
    float newPosY = adjustScrollYPositionIfNecessary(m_currentPosY + deltaY);
    
    if (newPosY == m_currentPosY)
        return;
    
    m_currentPosY = newPosY;
    notifyPositionChanged();
}

void ScrollAnimatorMac::immediateScrollToPointForScrollAnimation(const FloatPoint& newPosition)
{
    ASSERT(m_scrollAnimationHelper);
    CGFloat progress = [m_scrollAnimationHelper.get() _progress];
    
    immediateScrollToPoint(newPosition);

    if (progress >= 1.0)
        m_scrollableArea->didCompleteAnimatedScroll();
}

void ScrollAnimatorMac::notifyPositionChanged()
{
#if USE(SCROLLBAR_PAINTER)
    // This function is called when a page is going into the page cache, but the page 
    // isn't really scrolling in that case. We should only pass the message on to the
    // ScrollbarPainterController when we're really scrolling on an active page.
    if (scrollableArea()->isOnActivePage())
        [m_scrollbarPainterController.get() contentAreaScrolled];
#endif
    ScrollAnimator::notifyPositionChanged();
}

void ScrollAnimatorMac::contentAreaWillPaint() const
{
    if (!scrollableArea()->isOnActivePage())
        return;
#if USE(SCROLLBAR_PAINTER)
    [m_scrollbarPainterController.get() contentAreaWillDraw];
#endif
}

void ScrollAnimatorMac::mouseEnteredContentArea() const
{
    if (!scrollableArea()->isOnActivePage())
        return;
#if USE(SCROLLBAR_PAINTER)
    [m_scrollbarPainterController.get() mouseEnteredContentArea];
#endif
}

void ScrollAnimatorMac::mouseExitedContentArea() const
{
    if (!scrollableArea()->isOnActivePage())
        return;
#if USE(SCROLLBAR_PAINTER)
    [m_scrollbarPainterController.get() mouseExitedContentArea];
#endif
}

void ScrollAnimatorMac::mouseMovedInContentArea() const
{
    if (!scrollableArea()->isOnActivePage())
        return;
#if USE(SCROLLBAR_PAINTER)
    [m_scrollbarPainterController.get() mouseMovedInContentArea];
#endif
}

void ScrollAnimatorMac::mouseEnteredScrollbar(Scrollbar* scrollbar) const
{
    if (!scrollableArea()->isOnActivePage())
        return;
#if USE(SCROLLBAR_PAINTER)
    if (!supportsUIStateTransitionProgress())
        return;
    if (ScrollbarPainter painter = scrollbarPainterForScrollbar(scrollbar))
        [painter mouseEnteredScroller];
#else
    UNUSED_PARAM(scrollbar);
#endif
}

void ScrollAnimatorMac::mouseExitedScrollbar(Scrollbar* scrollbar) const
{
    if (!scrollableArea()->isOnActivePage())
        return;
#if USE(SCROLLBAR_PAINTER)
    if (!supportsUIStateTransitionProgress())
        return;
    if (ScrollbarPainter painter = scrollbarPainterForScrollbar(scrollbar))
        [painter mouseExitedScroller];
#else
    UNUSED_PARAM(scrollbar);
#endif
}

void ScrollAnimatorMac::willStartLiveResize()
{
    if (!scrollableArea()->isOnActivePage())
        return;
#if USE(SCROLLBAR_PAINTER)
    [m_scrollbarPainterController.get() startLiveResize];
#endif
}

void ScrollAnimatorMac::contentsResized() const
{
    if (!scrollableArea()->isOnActivePage())
        return;
#if USE(SCROLLBAR_PAINTER)
    [m_scrollbarPainterController.get() contentAreaDidResize];
#endif
}

void ScrollAnimatorMac::willEndLiveResize()
{
    if (!scrollableArea()->isOnActivePage())
        return;
#if USE(SCROLLBAR_PAINTER)
    [m_scrollbarPainterController.get() endLiveResize];
#endif
}

void ScrollAnimatorMac::contentAreaDidShow() const
{
    if (!scrollableArea()->isOnActivePage())
        return;
#if USE(SCROLLBAR_PAINTER)
    [m_scrollbarPainterController.get() windowOrderedIn];
#endif
}

void ScrollAnimatorMac::contentAreaDidHide() const
{
    if (!scrollableArea()->isOnActivePage())
        return;
#if USE(SCROLLBAR_PAINTER)
    [m_scrollbarPainterController.get() windowOrderedOut];
#endif
}

void ScrollAnimatorMac::didBeginScrollGesture() const
{
    if (!scrollableArea()->isOnActivePage())
        return;
#if USE(SCROLLBAR_PAINTER)
    [m_scrollbarPainterController.get() beginScrollGesture];
#endif
}

void ScrollAnimatorMac::didEndScrollGesture() const
{
    if (!scrollableArea()->isOnActivePage())
        return;
#if USE(SCROLLBAR_PAINTER)
    [m_scrollbarPainterController.get() endScrollGesture];
#endif
}

void ScrollAnimatorMac::didAddVerticalScrollbar(Scrollbar* scrollbar)
{
#if USE(SCROLLBAR_PAINTER)
    ScrollbarPainter painter = scrollbarPainterForScrollbar(scrollbar);
    if (!painter)
        return;

    ASSERT(!m_verticalScrollbarPainterDelegate);
    m_verticalScrollbarPainterDelegate.adoptNS([[WebScrollbarPainterDelegate alloc] initWithScrollbar:scrollbar]);

    [painter setDelegate:m_verticalScrollbarPainterDelegate.get()];
    [m_scrollbarPainterController.get() setVerticalScrollerImp:painter];
    if (scrollableArea()->inLiveResize())
        [painter setKnobAlpha:1];

#else
    UNUSED_PARAM(scrollbar);
#endif
}

void ScrollAnimatorMac::willRemoveVerticalScrollbar(Scrollbar* scrollbar)
{
#if USE(SCROLLBAR_PAINTER)
    ScrollbarPainter painter = scrollbarPainterForScrollbar(scrollbar);
    if (!painter)
        return;

    ASSERT(m_verticalScrollbarPainterDelegate);
    [m_verticalScrollbarPainterDelegate.get() invalidate];
    m_verticalScrollbarPainterDelegate = nullptr;

    [painter setDelegate:nil];
    [m_scrollbarPainterController.get() setVerticalScrollerImp:nil];
#else
    UNUSED_PARAM(scrollbar);
#endif
}

void ScrollAnimatorMac::didAddHorizontalScrollbar(Scrollbar* scrollbar)
{
#if USE(SCROLLBAR_PAINTER)
    ScrollbarPainter painter = scrollbarPainterForScrollbar(scrollbar);
    if (!painter)
        return;

    ASSERT(!m_horizontalScrollbarPainterDelegate);
    m_horizontalScrollbarPainterDelegate.adoptNS([[WebScrollbarPainterDelegate alloc] initWithScrollbar:scrollbar]);

    [painter setDelegate:m_horizontalScrollbarPainterDelegate.get()];
    [m_scrollbarPainterController.get() setHorizontalScrollerImp:painter];
    if (scrollableArea()->inLiveResize())
        [painter setKnobAlpha:1];
#else
    UNUSED_PARAM(scrollbar);
#endif
}

void ScrollAnimatorMac::willRemoveHorizontalScrollbar(Scrollbar* scrollbar)
{
#if USE(SCROLLBAR_PAINTER)
    ScrollbarPainter painter = scrollbarPainterForScrollbar(scrollbar);
    if (!painter)
        return;

    ASSERT(m_horizontalScrollbarPainterDelegate);
    [m_horizontalScrollbarPainterDelegate.get() invalidate];
    m_horizontalScrollbarPainterDelegate = nullptr;

    [painter setDelegate:nil];
    [m_scrollbarPainterController.get() setHorizontalScrollerImp:nil];
#else
    UNUSED_PARAM(scrollbar);
#endif
}

void ScrollAnimatorMac::cancelAnimations()
{
    m_haveScrolledSincePageLoad = false;

#if USE(SCROLLBAR_PAINTER)
    if (scrollbarPaintTimerIsActive())
        stopScrollbarPaintTimer();
    [m_horizontalScrollbarPainterDelegate.get() cancelAnimations];
    [m_verticalScrollbarPainterDelegate.get() cancelAnimations];
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

static inline bool isScrollingLeftAndShouldNotRubberBand(const PlatformWheelEvent& wheelEvent, ScrollableArea* scrollableArea)
{
    return wheelEvent.deltaX() > 0 && !scrollableArea->shouldRubberBandInDirection(ScrollLeft);
}

static inline bool isScrollingRightAndShouldNotRubberBand(const PlatformWheelEvent& wheelEvent, ScrollableArea* scrollableArea)
{
    return wheelEvent.deltaX() < 0 && !scrollableArea->shouldRubberBandInDirection(ScrollRight);
}

bool ScrollAnimatorMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    m_haveScrolledSincePageLoad = true;

    if (!wheelEvent.hasPreciseScrollingDeltas())
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
        
        if (m_scrollableArea->horizontalScrollbar()) {
            // If there is a scrollbar, we aggregate the wheel events to get an
            // overall trend of the scroll. If the direction of the scroll is ever
            // in the opposite direction of the pin location, then we switch the
            // boolean, and rubber band. That is, if we were pinned to the left,
            // and we ended up scrolling to the right, we rubber band.
            m_scrollElasticityController.m_cumulativeHorizontalScroll += wheelEvent.deltaX();
            if (m_scrollElasticityController.m_scrollerInitiallyPinnedOnLeft && m_scrollElasticityController.m_cumulativeHorizontalScroll < 0)
                m_scrollElasticityController.m_didCumulativeHorizontalScrollEverSwitchToOppositeDirectionOfPin = true;
            if (m_scrollElasticityController.m_scrollerInitiallyPinnedOnRight && m_scrollElasticityController.m_cumulativeHorizontalScroll > 0)
                m_scrollElasticityController.m_didCumulativeHorizontalScrollEverSwitchToOppositeDirectionOfPin = true;
        }

        // After a gesture begins, we go through:
        // 1+ PlatformWheelEventPhaseNone
        // 0+ PlatformWheelEventPhaseChanged
        // 1 PlatformWheelEventPhaseEnded if there was at least one changed event
        if (wheelEvent.momentumPhase() == PlatformWheelEventPhaseNone && !m_scrollElasticityController.m_didCumulativeHorizontalScrollEverSwitchToOppositeDirectionOfPin) {
            if ((isScrollingLeftAndShouldNotRubberBand(wheelEvent, m_scrollableArea) &&
                m_scrollElasticityController.m_scrollerInitiallyPinnedOnLeft &&
                m_scrollableArea->isHorizontalScrollerPinnedToMinimumPosition()) ||
                (isScrollingRightAndShouldNotRubberBand(wheelEvent, m_scrollableArea) &&
                m_scrollElasticityController.m_scrollerInitiallyPinnedOnRight &&
                m_scrollableArea->isHorizontalScrollerPinnedToMaximumPosition())) {
                return ScrollAnimator::handleWheelEvent(wheelEvent);
            }
        }
    }

    bool isMomentumScrollEvent = (wheelEvent.momentumPhase() != PlatformWheelEventPhaseNone);
    if (m_scrollElasticityController.m_ignoreMomentumScrolls && (isMomentumScrollEvent || m_scrollElasticityController.m_snapRubberbandTimerIsActive)) {
        if (wheelEvent.momentumPhase() == PlatformWheelEventPhaseEnded) {
            m_scrollElasticityController.m_ignoreMomentumScrolls = false;
            return true;
        }
        return false;
    }

    smoothScrollWithEvent(wheelEvent);
    return true;
}

void ScrollAnimatorMac::handleGestureEvent(const PlatformGestureEvent& gestureEvent)
{
    if (gestureEvent.type() == PlatformGestureEvent::ScrollBeginType)
        beginScrollGesture();
    else if (gestureEvent.type() == PlatformGestureEvent::ScrollEndType)
        endScrollGesture();
}

bool ScrollAnimatorMac::pinnedInDirection(float deltaX, float deltaY)
{
    FloatSize limitDelta;
    if (fabsf(deltaY) >= fabsf(deltaX)) {
        if (deltaY < 0) {
            // We are trying to scroll up.  Make sure we are not pinned to the top
            limitDelta.setHeight(m_scrollableArea->visibleContentRect().y() + + m_scrollableArea->scrollOrigin().y());
        } else {
            // We are trying to scroll down.  Make sure we are not pinned to the bottom
            limitDelta.setHeight(m_scrollableArea->contentsSize().height() - (m_scrollableArea->visibleContentRect().maxY() + m_scrollableArea->scrollOrigin().y()));
        }
    } else if (deltaX != 0) {
        if (deltaX < 0) {
            // We are trying to scroll left.  Make sure we are not pinned to the left
            limitDelta.setWidth(m_scrollableArea->visibleContentRect().x() + m_scrollableArea->scrollOrigin().x());
        } else {
            // We are trying to scroll right.  Make sure we are not pinned to the right
            limitDelta.setWidth(m_scrollableArea->contentsSize().width() - (m_scrollableArea->visibleContentRect().maxX() + m_scrollableArea->scrollOrigin().x()));
        }
    }
    
    if ((deltaX != 0 || deltaY != 0) && (limitDelta.width() < 1 && limitDelta.height() < 1))
        return true;
    return false;
}

bool ScrollAnimatorMac::isHorizontalScrollerPinnedToMinimumPosition()
{
    return m_scrollableArea->isHorizontalScrollerPinnedToMinimumPosition();
}

bool ScrollAnimatorMac::isHorizontalScrollerPinnedToMaximumPosition()
{
    return m_scrollableArea->isHorizontalScrollerPinnedToMaximumPosition();
}

IntSize ScrollAnimatorMac::stretchAmount()
{
    return m_scrollableArea->overhangAmount();
}

void ScrollAnimatorMac::startSnapRubberbandTimer()
{
    m_snapRubberBandTimer.startRepeating(1.0 / 60.0);
}

void ScrollAnimatorMac::stopSnapRubberbandTimer()
{
    m_snapRubberBandTimer.stop();
}

bool ScrollAnimatorMac::allowsVerticalStretching() const
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

bool ScrollAnimatorMac::allowsHorizontalStretching() const
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

void ScrollAnimatorMac::smoothScrollWithEvent(const PlatformWheelEvent& wheelEvent)
{
    m_haveScrolledSincePageLoad = true;

    float deltaX = m_scrollElasticityController.m_overflowScrollDelta.width();
    float deltaY = m_scrollElasticityController.m_overflowScrollDelta.height();

    // Reset overflow values because we may decide to remove delta at various points and put it into overflow.
    m_scrollElasticityController.m_overflowScrollDelta = FloatSize();

    float eventCoalescedDeltaX = -wheelEvent.deltaX();
    float eventCoalescedDeltaY = -wheelEvent.deltaY();

    deltaX += eventCoalescedDeltaX;
    deltaY += eventCoalescedDeltaY;

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

    PlatformWheelEventPhase phase = wheelEvent.momentumPhase();

    // If we are starting momentum scrolling then do some setup.
    if (!m_scrollElasticityController.m_momentumScrollInProgress && (phase == PlatformWheelEventPhaseBegan || phase == PlatformWheelEventPhaseChanged))
        m_scrollElasticityController.m_momentumScrollInProgress = true;

    CFTimeInterval timeDelta = wheelEvent.timestamp() - m_scrollElasticityController.m_lastMomentumScrollTimestamp;
    if (m_scrollElasticityController.m_inScrollGesture || m_scrollElasticityController.m_momentumScrollInProgress) {
        if (m_scrollElasticityController.m_lastMomentumScrollTimestamp && timeDelta > 0 && timeDelta < scrollVelocityZeroingTimeout) {
            m_scrollElasticityController.m_momentumVelocity.setWidth(eventCoalescedDeltaX / (float)timeDelta);
            m_scrollElasticityController.m_momentumVelocity.setHeight(eventCoalescedDeltaY / (float)timeDelta);
            m_scrollElasticityController.m_lastMomentumScrollTimestamp = wheelEvent.timestamp();
        } else {
            m_scrollElasticityController.m_lastMomentumScrollTimestamp = wheelEvent.timestamp();
            m_scrollElasticityController.m_momentumVelocity = FloatSize();
        }

        if (isVerticallyStretched) {
            if (!isHorizontallyStretched && pinnedInDirection(deltaX, 0)) {                
                // Stretching only in the vertical.
                if (deltaY != 0 && (fabsf(deltaX / deltaY) < rubberbandDirectionLockStretchRatio))
                    deltaX = 0;
                else if (fabsf(deltaX) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                    m_scrollElasticityController.m_overflowScrollDelta.setWidth(m_scrollElasticityController.m_overflowScrollDelta.width() + deltaX);
                    deltaX = 0;
                } else
                    m_scrollElasticityController.m_overflowScrollDelta.setWidth(m_scrollElasticityController.m_overflowScrollDelta.width() + deltaX);
            }
        } else if (isHorizontallyStretched) {
            // Stretching only in the horizontal.
            if (pinnedInDirection(0, deltaY)) {
                if (deltaX != 0 && (fabsf(deltaY / deltaX) < rubberbandDirectionLockStretchRatio))
                    deltaY = 0;
                else if (fabsf(deltaY) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                    m_scrollElasticityController.m_overflowScrollDelta.setHeight(m_scrollElasticityController.m_overflowScrollDelta.height() + deltaY);
                    deltaY = 0;
                } else
                    m_scrollElasticityController.m_overflowScrollDelta.setHeight(m_scrollElasticityController.m_overflowScrollDelta.height() + deltaY);
            }
        } else {
            // Not stretching at all yet.
            if (pinnedInDirection(deltaX, deltaY)) {
                if (fabsf(deltaY) >= fabsf(deltaX)) {
                    if (fabsf(deltaX) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                        m_scrollElasticityController.m_overflowScrollDelta.setWidth(m_scrollElasticityController.m_overflowScrollDelta.width() + deltaX);
                        deltaX = 0;
                    } else
                        m_scrollElasticityController.m_overflowScrollDelta.setWidth(m_scrollElasticityController.m_overflowScrollDelta.width() + deltaX);
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
                eventCoalescedDeltaX = 0;
            } else if ((deltaX != 0) && !isHorizontallyStretched && !pinnedInDirection(deltaX, 0)) {
                deltaX *= scrollWheelMultiplier();

                m_scrollableArea->setConstrainsScrollingToContentEdge(false);
                immediateScrollByDeltaX(deltaX);
                m_scrollableArea->setConstrainsScrollingToContentEdge(true);

                deltaX = 0;
            }
            
            if (!allowsVerticalStretching()) {
                deltaY = 0;
                eventCoalescedDeltaY = 0;
            } else if ((deltaY != 0) && !isVerticallyStretched && !pinnedInDirection(0, deltaY)) {
                deltaY *= scrollWheelMultiplier();

                m_scrollableArea->setConstrainsScrollingToContentEdge(false);
                immediateScrollByDeltaY(deltaY);
                m_scrollableArea->setConstrainsScrollingToContentEdge(true);

                deltaY = 0;
            }
            
            IntSize stretchAmount = m_scrollableArea->overhangAmount();
        
            if (m_scrollElasticityController.m_momentumScrollInProgress) {
                if ((pinnedInDirection(eventCoalescedDeltaX, eventCoalescedDeltaY) || (fabsf(eventCoalescedDeltaX) + fabsf(eventCoalescedDeltaY) <= 0)) && m_scrollElasticityController.m_lastMomentumScrollTimestamp) {
                    m_scrollElasticityController.m_ignoreMomentumScrolls = true;
                    m_scrollElasticityController.m_momentumScrollInProgress = false;
                    snapRubberBand();
                }
            }

            m_scrollElasticityController.m_stretchScrollForce.setWidth(m_scrollElasticityController.m_stretchScrollForce.width() + deltaX);
            m_scrollElasticityController.m_stretchScrollForce.setHeight(m_scrollElasticityController.m_stretchScrollForce.height() + deltaY);

            FloatSize dampedDelta(ceilf(elasticDeltaForReboundDelta(m_scrollElasticityController.m_stretchScrollForce.width())), ceilf(elasticDeltaForReboundDelta(m_scrollElasticityController.m_stretchScrollForce.height())));
            FloatPoint origOrigin = (m_scrollableArea->visibleContentRect().location() + m_scrollableArea->scrollOrigin()) - stretchAmount;
            FloatPoint newOrigin = origOrigin + dampedDelta;

            if (origOrigin != newOrigin) {
                m_scrollableArea->setConstrainsScrollingToContentEdge(false);
                immediateScrollToPoint(newOrigin);
                m_scrollableArea->setConstrainsScrollingToContentEdge(true);
            }
        }
    }

    if (m_scrollElasticityController.m_momentumScrollInProgress && phase == PlatformWheelEventPhaseEnded) {
        m_scrollElasticityController.m_momentumScrollInProgress = false;
        m_scrollElasticityController.m_ignoreMomentumScrolls = false;
        m_scrollElasticityController.m_lastMomentumScrollTimestamp = 0;
    }
}

void ScrollAnimatorMac::beginScrollGesture()
{
    didBeginScrollGesture();
    m_haveScrolledSincePageLoad = true;

    m_scrollElasticityController.beginScrollGesture();
}

void ScrollAnimatorMac::endScrollGesture()
{
    didEndScrollGesture();

    snapRubberBand();
}

void ScrollAnimatorMac::snapRubberBand()
{
    CFTimeInterval timeDelta = [[NSProcessInfo processInfo] systemUptime] - m_scrollElasticityController.m_lastMomentumScrollTimestamp;
    if (m_scrollElasticityController.m_lastMomentumScrollTimestamp && timeDelta >= scrollVelocityZeroingTimeout)
        m_scrollElasticityController.m_momentumVelocity = FloatSize();

    m_scrollElasticityController.m_inScrollGesture = false;

    if (m_scrollElasticityController.m_snapRubberbandTimerIsActive)
        return;

    m_scrollElasticityController.m_startTime = [NSDate timeIntervalSinceReferenceDate];
    m_scrollElasticityController.m_startStretch = FloatSize();
    m_scrollElasticityController.m_origOrigin = FloatPoint();
    m_scrollElasticityController.m_origVelocity = FloatSize();

    m_snapRubberBandTimer.startRepeating(1.0/60.0);
    m_scrollElasticityController.m_snapRubberbandTimerIsActive = true;
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
    if (!m_scrollElasticityController.m_momentumScrollInProgress || m_scrollElasticityController.m_ignoreMomentumScrolls) {
        CFTimeInterval timeDelta = [NSDate timeIntervalSinceReferenceDate] - m_scrollElasticityController.m_startTime;

        if (m_scrollElasticityController.m_startStretch == FloatSize()) {
            m_scrollElasticityController.m_startStretch = m_scrollableArea->overhangAmount();
            if (m_scrollElasticityController.m_startStretch == FloatSize()) {    
                m_snapRubberBandTimer.stop();

                m_scrollElasticityController.m_stretchScrollForce = FloatSize();
                m_scrollElasticityController.m_startTime = 0;
                m_scrollElasticityController.m_startStretch = FloatSize();
                m_scrollElasticityController.m_origOrigin = FloatPoint();
                m_scrollElasticityController.m_origVelocity = FloatSize();
                m_scrollElasticityController.m_snapRubberbandTimerIsActive = false;

                return;
            }

            m_scrollableArea->didStartRubberBand(roundedIntSize(m_scrollElasticityController.m_startStretch));
            
            m_scrollElasticityController.m_origOrigin = (m_scrollableArea->visibleContentRect().location() + m_scrollableArea->scrollOrigin()) - m_scrollElasticityController.m_startStretch;
            m_scrollElasticityController.m_origVelocity = m_scrollElasticityController.m_momentumVelocity;

            // Just like normal scrolling, prefer vertical rubberbanding
            if (fabsf(m_scrollElasticityController.m_origVelocity.height()) >= fabsf(m_scrollElasticityController.m_origVelocity.width()))
                m_scrollElasticityController.m_origVelocity.setWidth(0);
            
            // Don't rubber-band horizontally if it's not possible to scroll horizontally
            Scrollbar* hScroller = m_scrollableArea->horizontalScrollbar();
            if (!hScroller || !hScroller->enabled())
                m_scrollElasticityController.m_origVelocity.setWidth(0);
            
            // Don't rubber-band vertically if it's not possible to scroll horizontally
            Scrollbar* vScroller = m_scrollableArea->verticalScrollbar();
            if (!vScroller || !vScroller->enabled())
                m_scrollElasticityController.m_origVelocity.setHeight(0);
        }

        FloatPoint delta(roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_scrollElasticityController.m_startStretch.width(), -m_scrollElasticityController.m_origVelocity.width(), (float)timeDelta)),
                         roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_scrollElasticityController.m_startStretch.height(), -m_scrollElasticityController.m_origVelocity.height(), (float)timeDelta)));

        if (fabs(delta.x()) >= 1 || fabs(delta.y()) >= 1) {
            FloatPoint newOrigin = m_scrollElasticityController.m_origOrigin + delta;

            m_scrollableArea->setConstrainsScrollingToContentEdge(false);
            immediateScrollToPoint(newOrigin);
            m_scrollableArea->setConstrainsScrollingToContentEdge(true);

            FloatSize newStretch = m_scrollableArea->overhangAmount();
            
            m_scrollElasticityController.m_stretchScrollForce.setWidth(reboundDeltaForElasticDelta(newStretch.width()));
            m_scrollElasticityController.m_stretchScrollForce.setHeight(reboundDeltaForElasticDelta(newStretch.height()));
        } else {
            immediateScrollToPoint(m_scrollElasticityController.m_origOrigin);

            m_scrollableArea->didCompleteRubberBand(roundedIntSize(m_scrollElasticityController.m_startStretch));

            m_snapRubberBandTimer.stop();

            m_scrollElasticityController.m_stretchScrollForce = FloatSize();
            m_scrollElasticityController.m_startTime = 0;
            m_scrollElasticityController.m_startStretch = FloatSize();
            m_scrollElasticityController.m_origOrigin = FloatPoint();
            m_scrollElasticityController.m_origVelocity = FloatSize();
            m_scrollElasticityController.m_snapRubberbandTimerIsActive = false;
        }
    } else {
        m_scrollElasticityController.m_startTime = [NSDate timeIntervalSinceReferenceDate];
        m_scrollElasticityController.m_startStretch = FloatSize();
    }
}
#endif

void ScrollAnimatorMac::setIsActive()
{
#if USE(SCROLLBAR_PAINTER)
    if (needsScrollerStyleUpdate())
        updateScrollerStyle();
#endif
}

#if USE(SCROLLBAR_PAINTER)
void ScrollAnimatorMac::updateScrollerStyle()
{
    if (!scrollableArea()->isOnActivePage()) {
        setNeedsScrollerStyleUpdate(true);
        return;
    }

    ScrollbarThemeMac* macTheme = macScrollbarTheme();
    if (!macTheme) {
        setNeedsScrollerStyleUpdate(false);
        return;
    }

    NSScrollerStyle newStyle = [m_scrollbarPainterController.get() scrollerStyle];

    if (Scrollbar* verticalScrollbar = scrollableArea()->verticalScrollbar()) {
        verticalScrollbar->invalidate();

        ScrollbarPainter oldVerticalPainter = [m_scrollbarPainterController.get() verticalScrollerImp];
        ScrollbarPainter newVerticalPainter = [NSClassFromString(@"NSScrollerImp") scrollerImpWithStyle:newStyle 
                                                                                    controlSize:(NSControlSize)verticalScrollbar->controlSize() 
                                                                                    horizontal:NO 
                                                                                    replacingScrollerImp:oldVerticalPainter];
        macTheme->setNewPainterForScrollbar(verticalScrollbar, newVerticalPainter);
        [m_scrollbarPainterController.get() setVerticalScrollerImp:newVerticalPainter];

        // The different scrollbar styles have different thicknesses, so we must re-set the 
        // frameRect to the new thickness, and the re-layout below will ensure the position
        // and length are properly updated.
        int thickness = macTheme->scrollbarThickness(verticalScrollbar->controlSize());
        verticalScrollbar->setFrameRect(IntRect(0, 0, thickness, thickness));
    }

    if (Scrollbar* horizontalScrollbar = scrollableArea()->horizontalScrollbar()) {
        horizontalScrollbar->invalidate();

        ScrollbarPainter oldHorizontalPainter = [m_scrollbarPainterController.get() horizontalScrollerImp];
        ScrollbarPainter newHorizontalPainter = [NSClassFromString(@"NSScrollerImp") scrollerImpWithStyle:newStyle 
                                                                                    controlSize:(NSControlSize)horizontalScrollbar->controlSize() 
                                                                                    horizontal:YES 
                                                                                    replacingScrollerImp:oldHorizontalPainter];
        macTheme->setNewPainterForScrollbar(horizontalScrollbar, newHorizontalPainter);
        [m_scrollbarPainterController.get() setHorizontalScrollerImp:newHorizontalPainter];

        // The different scrollbar styles have different thicknesses, so we must re-set the 
        // frameRect to the new thickness, and the re-layout below will ensure the position
        // and length are properly updated.
        int thickness = macTheme->scrollbarThickness(horizontalScrollbar->controlSize());
        horizontalScrollbar->setFrameRect(IntRect(0, 0, thickness, thickness));
    }

    // If needsScrollerStyleUpdate() is true, then the page is restoring from the page cache, and 
    // a relayout will happen on its own. Otherwise, we must initiate a re-layout ourselves.
    scrollableArea()->scrollbarStyleChanged(newStyle, !needsScrollerStyleUpdate());

    setNeedsScrollerStyleUpdate(false);
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

void ScrollAnimatorMac::initialScrollbarPaintTimerFired(Timer<ScrollAnimatorMac>*)
{
    // To force the scrollbars to flash, we have to call hide first. Otherwise, the ScrollbarPainterController
    // might think that the scrollbars are already showing and bail early.
    [m_scrollbarPainterController.get() hideOverlayScrollers];
    [m_scrollbarPainterController.get() flashScrollers];
}
#endif

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
