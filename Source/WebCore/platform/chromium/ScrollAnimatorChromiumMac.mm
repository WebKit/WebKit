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

#include <sys/time.h>
#include <sys/sysctl.h>

#include "ScrollAnimatorChromiumMac.h"

#include "FloatPoint.h"
#include "PlatformGestureEvent.h"
#include "PlatformWheelEvent.h"
#include "ScrollView.h"
#include "ScrollableArea.h"
#include "ScrollbarTheme.h"
#include "ScrollbarThemeChromiumMac.h"
#include "WebCoreSystemInterface.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/UnusedParam.h>
#include <QuartzCore/QuartzCore.h>

using namespace WebCore;
using namespace std;

#if !defined(MAC_OS_X_VERSION_10_6) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
@protocol NSAnimationDelegate
@end

@interface NSProcessInfo (ScrollAnimatorChromiumMacExt)
- (NSTimeInterval)systemUptime;
@end

@implementation NSProcessInfo (ScrollAnimatorChromiumMacExt)
- (NSTimeInterval)systemUptime
{
    // Get how long system has been up. Found by looking getting "boottime" from the kernel.
    static struct timeval boottime = {0};
    if (!boottime.tv_sec) {
        int mib[2] = {CTL_KERN, KERN_BOOTTIME};
        size_t size = sizeof(boottime);
        if (-1 == sysctl(mib, 2, &boottime, &size, 0, 0))
            boottime.tv_sec = 0;
    }
    struct timeval now;
    if (boottime.tv_sec && -1 != gettimeofday(&now, 0)) {
        struct timeval uptime;
        timersub(&now, &boottime, &uptime);
        NSTimeInterval result = uptime.tv_sec + (uptime.tv_usec / 1E+6);
        return result;
    }
    return 0;
}
@end
#endif

@interface NSObject (ScrollAnimationHelperDetails)
- (id)initWithDelegate:(id)delegate;
- (void)_stopRun;
- (BOOL)_isAnimating;
- (NSPoint)targetOrigin;
- (CGFloat)_progress;
@end

@interface ScrollAnimationHelperDelegate : NSObject
{
    WebCore::ScrollAnimatorChromiumMac* _animator;
}
- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorChromiumMac*)scrollAnimator;
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

- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorChromiumMac*)scrollAnimator
{
    self = [super init];
    if (!self)
        return nil;

    _animator = scrollAnimator;
    return self;
}

- (void)scrollAnimatorDestroyed
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

#if USE(WK_SCROLLBAR_PAINTER)

@interface ScrollbarPainterControllerDelegate : NSObject
{
    WebCore::ScrollAnimatorChromiumMac* _animator;
}
- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorChromiumMac*)scrollAnimator;
@end

@implementation ScrollbarPainterControllerDelegate

- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorChromiumMac*)scrollAnimator
{
    self = [super init];
    if (!self)
        return nil;
    
    _animator = scrollAnimator;
    return self;
}

- (void)scrollAnimatorDestroyed
{
    _animator = 0;
}

- (NSRect)contentAreaRectForScrollerImpPair:(id)scrollerImpPair
{
    UNUSED_PARAM(scrollerImpPair);
    if (!_animator)
        return NSZeroRect;

    WebCore::IntSize contentsSize = _animator->scrollableArea()->contentsSize();
    return NSMakeRect(0, 0, contentsSize.width(), contentsSize.height());
}

- (BOOL)inLiveResizeForScrollerImpPair:(id)scrollerImpPair
{
    UNUSED_PARAM(scrollerImpPair);
    if (!_animator)
        return NO;

    return _animator->scrollableArea()->inLiveResize();
}

- (NSPoint)mouseLocationInContentAreaForScrollerImpPair:(id)scrollerImpPair
{
    UNUSED_PARAM(scrollerImpPair);
    if (!_animator)
        return NSZeroPoint;

    return _animator->scrollableArea()->currentMousePosition();
}

- (NSPoint)scrollerImpPair:(id)scrollerImpPair convertContentPoint:(NSPoint)pointInContentArea toScrollerImp:(id)scrollerImp
{
    UNUSED_PARAM(scrollerImpPair);
    if (!_animator)
        return NSZeroPoint;

    WebCore::Scrollbar* scrollbar = 0;
    if (wkScrollbarPainterIsHorizontal((WKScrollbarPainterRef)scrollerImp))
        scrollbar = _animator->scrollableArea()->horizontalScrollbar();
    else 
        scrollbar = _animator->scrollableArea()->verticalScrollbar();

    // It is possible to have a null scrollbar here since it is possible for this delegate
    // method to be called between the moment when a scrollbar has been set to 0 and the
    // moment when its destructor has been called. We should probably de-couple some
    // of the clean-up work in ScrollbarThemeChromiumMac::unregisterScrollbar() to avoid this
    // issue.
    if (!scrollbar)
        return WebCore::IntPoint();
    
    return scrollbar->convertFromContainingView(WebCore::IntPoint(pointInContentArea));
}

- (void)scrollerImpPair:(id)scrollerImpPair setContentAreaNeedsDisplayInRect:(NSRect)rect
{
    UNUSED_PARAM(scrollerImpPair);
    UNUSED_PARAM(rect);
}

- (void)scrollerImpPair:(id)scrollerImpPair updateScrollerStyleForNewRecommendedScrollerStyle:(NSScrollerStyle)newRecommendedScrollerStyle
{
    if (!_animator)
        return;

    wkSetScrollbarPainterControllerStyle((WKScrollbarPainterControllerRef)scrollerImpPair, newRecommendedScrollerStyle);
    _animator->updateScrollerStyle();
}

@end

@interface ScrollbarPartAnimation : NSAnimation
{
    RetainPtr<WKScrollbarPainterRef> _scrollerPainter;
    WebCore::ScrollbarPart _part;
    WebCore::ScrollAnimatorChromiumMac* _animator;
    CGFloat _initialAlpha;
    CGFloat _newAlpha;
}
- (id)initWithScrollbarPainter:(WKScrollbarPainterRef)scrollerPainter part:(WebCore::ScrollbarPart)part scrollAnimator:(WebCore::ScrollAnimatorChromiumMac*)scrollAnimator animateAlphaTo:(CGFloat)newAlpha duration:(NSTimeInterval)duration;
@end

@implementation ScrollbarPartAnimation

- (id)initWithScrollbarPainter:(WKScrollbarPainterRef)scrollerPainter part:(WebCore::ScrollbarPart)part scrollAnimator:(WebCore::ScrollAnimatorChromiumMac*)scrollAnimator animateAlphaTo:(CGFloat)newAlpha duration:(NSTimeInterval)duration
{
    self = [super initWithDuration:duration animationCurve:NSAnimationEaseInOut];
    if (!self)
        return nil;
    
    _scrollerPainter = scrollerPainter;
    _part = part;
    _animator = scrollAnimator;
    _initialAlpha = _part == WebCore::ThumbPart ? wkScrollbarPainterKnobAlpha(_scrollerPainter.get()) : wkScrollbarPainterTrackAlpha(_scrollerPainter.get());
    _newAlpha = newAlpha;
    
    return self;    
}

- (void)setCurrentProgress:(NSAnimationProgress)progress
{
    [super setCurrentProgress:progress];

    if (!_animator)
        return;

    CGFloat currentAlpha;
    if (_initialAlpha > _newAlpha)
        currentAlpha = 1 - progress;
    else
        currentAlpha = progress;
    
    if (_part == WebCore::ThumbPart)
        wkSetScrollbarPainterKnobAlpha(_scrollerPainter.get(), currentAlpha);
    else
        wkSetScrollbarPainterTrackAlpha(_scrollerPainter.get(), currentAlpha);

    // Invalidate the scrollbars so that they paint the animation
    if (WebCore::Scrollbar* verticalScrollbar = _animator->scrollableArea()->verticalScrollbar())
        verticalScrollbar->invalidateRect(WebCore::IntRect(0, 0, verticalScrollbar->width(), verticalScrollbar->height()));
    if (WebCore::Scrollbar* horizontalScrollbar = _animator->scrollableArea()->horizontalScrollbar())
        horizontalScrollbar->invalidateRect(WebCore::IntRect(0, 0, horizontalScrollbar->width(), horizontalScrollbar->height()));
}

- (void)scrollAnimatorDestroyed
{
    [self stopAnimation];
    _animator = 0;
}

@end

@interface ScrollbarPainterDelegate : NSObject<NSAnimationDelegate>
{
    WebCore::ScrollAnimatorChromiumMac* _animator;

    RetainPtr<ScrollbarPartAnimation> _verticalKnobAnimation;
    RetainPtr<ScrollbarPartAnimation> _horizontalKnobAnimation;

    RetainPtr<ScrollbarPartAnimation> _verticalTrackAnimation;
    RetainPtr<ScrollbarPartAnimation> _horizontalTrackAnimation;
}
- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorChromiumMac*)scrollAnimator;
- (void)cancelAnimations;
@end

@implementation ScrollbarPainterDelegate

- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorChromiumMac*)scrollAnimator
{
    self = [super init];
    if (!self)
        return nil;
    
    _animator = scrollAnimator;
    return self;
}

- (void)cancelAnimations
{
    [_verticalKnobAnimation.get() stopAnimation];
    [_horizontalKnobAnimation.get() stopAnimation];
    [_verticalTrackAnimation.get() stopAnimation];
    [_horizontalTrackAnimation.get() stopAnimation];
}

- (NSRect)convertRectToBacking:(NSRect)aRect
{
    return aRect;
}

- (NSRect)convertRectFromBacking:(NSRect)aRect
{
    return aRect;
}

#if !PLATFORM(CHROMIUM)
- (CALayer *)layer
{
    if (!_animator)
        return nil;
    if (!_animator->isDrawingIntoLayer())
        return nil;

    // FIXME: This should attempt to return an actual layer.
    static CALayer *dummyLayer = [[CALayer alloc] init];
    return dummyLayer;
}
#endif

- (void)setUpAnimation:(RetainPtr<ScrollbarPartAnimation>&)scrollbarPartAnimation scrollerPainter:(WKScrollbarPainterRef)scrollerPainter part:(WebCore::ScrollbarPart)part animateAlphaTo:(CGFloat)newAlpha duration:(NSTimeInterval)duration
{
    // If the user has scrolled the page, then the scrollbars must be animated here. 
    // This overrides the early returns.
    bool mustAnimate = _animator->haveScrolledSincePageLoad();

    if (_animator->scrollbarPaintTimerIsActive() && !mustAnimate)
        return;

    if (_animator->scrollableArea()->shouldSuspendScrollAnimations() && !mustAnimate) {
        _animator->startScrollbarPaintTimer();
        return;
    }

    // At this point, we are definitely going to animate now, so stop the timer.
    _animator->stopScrollbarPaintTimer();

    // If we are currently animating, stop
    if (scrollbarPartAnimation) {
        [scrollbarPartAnimation.get() stopAnimation];
        scrollbarPartAnimation = nil;
    }

    if (part == WebCore::ThumbPart && !wkScrollbarPainterIsHorizontal(scrollerPainter)) {
        if (newAlpha == 1) {
            IntRect thumbRect = IntRect(wkScrollbarPainterKnobRect(scrollerPainter));
            _animator->setVisibleScrollerThumbRect(thumbRect);
        } else
            _animator->setVisibleScrollerThumbRect(IntRect());
    }

    [NSAnimationContext beginGrouping];
    [[NSAnimationContext currentContext] setDuration:duration];
    scrollbarPartAnimation.adoptNS([[ScrollbarPartAnimation alloc] initWithScrollbarPainter:scrollerPainter 
                                                                    part:part
                                                                    scrollAnimator:_animator 
                                                                    animateAlphaTo:newAlpha 
                                                                    duration:duration]);
    [scrollbarPartAnimation.get() setAnimationBlockingMode:NSAnimationNonblocking];
    [scrollbarPartAnimation.get() startAnimation];
    [NSAnimationContext endGrouping];
}

- (void)scrollerImp:(id)scrollerImp animateKnobAlphaTo:(CGFloat)newKnobAlpha duration:(NSTimeInterval)duration
{
    if (!_animator)
        return;

    WKScrollbarPainterRef scrollerPainter = (WKScrollbarPainterRef)scrollerImp;
    if (wkScrollbarPainterIsHorizontal(scrollerPainter))
        [self setUpAnimation:_horizontalKnobAnimation scrollerPainter:scrollerPainter part:WebCore::ThumbPart animateAlphaTo:newKnobAlpha duration:duration];
    else
        [self setUpAnimation:_verticalKnobAnimation scrollerPainter:scrollerPainter part:WebCore::ThumbPart animateAlphaTo:newKnobAlpha duration:duration];
}

- (void)scrollerImp:(id)scrollerImp animateTrackAlphaTo:(CGFloat)newTrackAlpha duration:(NSTimeInterval)duration
{
    if (!_animator)
        return;

    WKScrollbarPainterRef scrollerPainter = (WKScrollbarPainterRef)scrollerImp;
    if (wkScrollbarPainterIsHorizontal(scrollerPainter))
        [self setUpAnimation:_horizontalTrackAnimation scrollerPainter:scrollerPainter part:WebCore::BackTrackPart animateAlphaTo:newTrackAlpha duration:duration];
    else
        [self setUpAnimation:_verticalTrackAnimation scrollerPainter:scrollerPainter part:WebCore::BackTrackPart animateAlphaTo:newTrackAlpha duration:duration];
}

- (void)scrollerImp:(id)scrollerImp overlayScrollerStateChangedTo:(NSUInteger)newOverlayScrollerState
{
    UNUSED_PARAM(scrollerImp);
    UNUSED_PARAM(newOverlayScrollerState);
}

- (void)scrollAnimatorDestroyed
{
    _animator = 0;
    [_verticalKnobAnimation.get() scrollAnimatorDestroyed];
    [_horizontalKnobAnimation.get() scrollAnimatorDestroyed];
    [_verticalTrackAnimation.get() scrollAnimatorDestroyed];
    [_horizontalTrackAnimation.get() scrollAnimatorDestroyed];
}

@end

#endif // USE(WK_SCROLLBAR_PAINTER)

namespace WebCore {

PassOwnPtr<ScrollAnimator> ScrollAnimator::create(ScrollableArea* scrollableArea)
{
    return adoptPtr(new ScrollAnimatorChromiumMac(scrollableArea));
}

static ScrollbarThemeChromiumMac* chromiumScrollbarTheme()
{
    ScrollbarTheme* scrollbarTheme = ScrollbarTheme::theme();
    return !scrollbarTheme->isMockTheme() ? static_cast<ScrollbarThemeChromiumMac*>(scrollbarTheme) : 0;
}

ScrollAnimatorChromiumMac::ScrollAnimatorChromiumMac(ScrollableArea* scrollableArea)
    : ScrollAnimator(scrollableArea)
#if USE(WK_SCROLLBAR_PAINTER)
    , m_initialScrollbarPaintTimer(this, &ScrollAnimatorChromiumMac::initialScrollbarPaintTimerFired)
#endif
#if ENABLE(RUBBER_BANDING)
    , m_inScrollGesture(false)
    , m_momentumScrollInProgress(false)
    , m_ignoreMomentumScrolls(false)
    , m_lastMomentumScrollTimestamp(0)
    , m_startTime(0)
    , m_snapRubberBandTimer(this, &ScrollAnimatorChromiumMac::snapRubberBandTimerFired)
#endif
    , m_drawingIntoLayer(false)
    , m_haveScrolledSincePageLoad(false)
    , m_needsScrollerStyleUpdate(false)
{
    m_scrollAnimationHelperDelegate.adoptNS([[ScrollAnimationHelperDelegate alloc] initWithScrollAnimator:this]);
    m_scrollAnimationHelper.adoptNS([[NSClassFromString(@"NSScrollAnimationHelper") alloc] initWithDelegate:m_scrollAnimationHelperDelegate.get()]);

    if (isScrollbarOverlayAPIAvailable()) {
        m_scrollbarPainterControllerDelegate.adoptNS([[ScrollbarPainterControllerDelegate alloc] initWithScrollAnimator:this]);
        m_scrollbarPainterController = wkMakeScrollbarPainterController(m_scrollbarPainterControllerDelegate.get());
        m_scrollbarPainterDelegate.adoptNS([[ScrollbarPainterDelegate alloc] initWithScrollAnimator:this]);
    }
}

ScrollAnimatorChromiumMac::~ScrollAnimatorChromiumMac()
{
    if (isScrollbarOverlayAPIAvailable()) {
        [m_scrollbarPainterControllerDelegate.get() scrollAnimatorDestroyed];
        [(id)m_scrollbarPainterController.get() setDelegate:nil];
        [m_scrollbarPainterDelegate.get() scrollAnimatorDestroyed];
        [m_scrollAnimationHelperDelegate.get() scrollAnimatorDestroyed];
    }
}

bool ScrollAnimatorChromiumMac::scroll(ScrollbarOrientation orientation, ScrollGranularity granularity, float step, float multiplier)
{
    m_haveScrolledSincePageLoad = true;

#if !ENABLE(SMOOTH_SCROLLING)
    return ScrollAnimator::scroll(orientation, granularity, step, multiplier);
#endif

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

void ScrollAnimatorChromiumMac::scrollToOffsetWithoutAnimation(const FloatPoint& offset)
{
    [m_scrollAnimationHelper.get() _stopRun];
    immediateScrollToPoint(offset);
}

float ScrollAnimatorChromiumMac::adjustScrollXPositionIfNecessary(float position) const
{
    if (!m_scrollableArea->constrainsScrollingToContentEdge())
        return position;

    return max<float>(min<float>(position, m_scrollableArea->contentsSize().width() - m_scrollableArea->visibleWidth()), 0);
}

float ScrollAnimatorChromiumMac::adjustScrollYPositionIfNecessary(float position) const
{
    if (!m_scrollableArea->constrainsScrollingToContentEdge())
        return position;

    return max<float>(min<float>(position, m_scrollableArea->contentsSize().height() - m_scrollableArea->visibleHeight()), 0);
}

FloatPoint ScrollAnimatorChromiumMac::adjustScrollPositionIfNecessary(const FloatPoint& position) const
{
    if (!m_scrollableArea->constrainsScrollingToContentEdge())
        return position;

    float newX = max<float>(min<float>(position.x(), m_scrollableArea->contentsSize().width() - m_scrollableArea->visibleWidth()), 0);
    float newY = max<float>(min<float>(position.y(), m_scrollableArea->contentsSize().height() - m_scrollableArea->visibleHeight()), 0);

    return FloatPoint(newX, newY);
}

void ScrollAnimatorChromiumMac::immediateScrollToPoint(const FloatPoint& newPosition)
{
    FloatPoint adjustedPosition = adjustScrollPositionIfNecessary(newPosition);
 
    m_currentPosX = adjustedPosition.x();
    m_currentPosY = adjustedPosition.y();
    notifyPositionChanged();
}

void ScrollAnimatorChromiumMac::immediateScrollByDeltaX(float deltaX)
{
    float newPosX = adjustScrollXPositionIfNecessary(m_currentPosX + deltaX);
    
    if (newPosX == m_currentPosX)
        return;
    
    m_currentPosX = newPosX;
    notifyPositionChanged();
}

void ScrollAnimatorChromiumMac::immediateScrollByDeltaY(float deltaY)
{
    float newPosY = adjustScrollYPositionIfNecessary(m_currentPosY + deltaY);
    
    if (newPosY == m_currentPosY)
        return;
    
    m_currentPosY = newPosY;
    notifyPositionChanged();
}

void ScrollAnimatorChromiumMac::immediateScrollToPointForScrollAnimation(const FloatPoint& newPosition)
{
    ASSERT(m_scrollAnimationHelper);
    CGFloat progress = [m_scrollAnimationHelper.get() _progress];
    
    immediateScrollToPoint(newPosition);

    if (progress >= 1.0)
        m_scrollableArea->didCompleteAnimatedScroll();
}

void ScrollAnimatorChromiumMac::notifyPositionChanged()
{
    if (isScrollbarOverlayAPIAvailable())
        wkContentAreaScrolled(m_scrollbarPainterController.get());
    ScrollAnimator::notifyPositionChanged();
}

void ScrollAnimatorChromiumMac::contentAreaWillPaint() const
{
    if (isScrollbarOverlayAPIAvailable())
        wkContentAreaWillPaint(m_scrollbarPainterController.get());
}

void ScrollAnimatorChromiumMac::mouseEnteredContentArea() const
{
    if (isScrollbarOverlayAPIAvailable())
        wkMouseEnteredContentArea(m_scrollbarPainterController.get());
}

void ScrollAnimatorChromiumMac::mouseExitedContentArea() const
{
    if (isScrollbarOverlayAPIAvailable())
        wkMouseExitedContentArea(m_scrollbarPainterController.get());
}

void ScrollAnimatorChromiumMac::mouseMovedInContentArea() const
{
    if (isScrollbarOverlayAPIAvailable())
        wkMouseMovedInContentArea(m_scrollbarPainterController.get());
}

void ScrollAnimatorChromiumMac::willStartLiveResize()
{
    if (isScrollbarOverlayAPIAvailable())
        wkWillStartLiveResize(m_scrollbarPainterController.get());
}

void ScrollAnimatorChromiumMac::contentsResized() const
{
    if (isScrollbarOverlayAPIAvailable())
        wkContentAreaResized(m_scrollbarPainterController.get());
}

void ScrollAnimatorChromiumMac::willEndLiveResize()
{
    if (isScrollbarOverlayAPIAvailable())
        wkWillEndLiveResize(m_scrollbarPainterController.get());
}

void ScrollAnimatorChromiumMac::contentAreaDidShow() const
{
    if (isScrollbarOverlayAPIAvailable())
        wkContentAreaDidShow(m_scrollbarPainterController.get());
}

void ScrollAnimatorChromiumMac::contentAreaDidHide() const
{
    if (isScrollbarOverlayAPIAvailable())
        wkContentAreaDidHide(m_scrollbarPainterController.get());
}

void ScrollAnimatorChromiumMac::didBeginScrollGesture() const
{
    if (isScrollbarOverlayAPIAvailable())
        wkDidBeginScrollGesture(m_scrollbarPainterController.get());
}

void ScrollAnimatorChromiumMac::didEndScrollGesture() const
{
    if (isScrollbarOverlayAPIAvailable())
        wkDidEndScrollGesture(m_scrollbarPainterController.get());
}

void ScrollAnimatorChromiumMac::didAddVerticalScrollbar(Scrollbar* scrollbar)
{
    if (!isScrollbarOverlayAPIAvailable())
        return;
        
    if (ScrollbarThemeChromiumMac* theme = chromiumScrollbarTheme()) {
        WKScrollbarPainterRef painter = theme->painterForScrollbar(scrollbar);
        wkScrollbarPainterSetDelegate(painter, m_scrollbarPainterDelegate.get());
        wkSetPainterForPainterController(m_scrollbarPainterController.get(), painter, false);
        if (scrollableArea()->inLiveResize())
            wkSetScrollbarPainterKnobAlpha(painter, 1);
    }
}

void ScrollAnimatorChromiumMac::willRemoveVerticalScrollbar(Scrollbar* scrollbar)
{
    if (!isScrollbarOverlayAPIAvailable())
        return;

    if (ScrollbarThemeChromiumMac* theme = chromiumScrollbarTheme()) {
        WKScrollbarPainterRef painter = theme->painterForScrollbar(scrollbar);
        wkScrollbarPainterSetDelegate(painter, nil);
        wkSetPainterForPainterController(m_scrollbarPainterController.get(), nil, false);
    }
}

void ScrollAnimatorChromiumMac::didAddHorizontalScrollbar(Scrollbar* scrollbar)
{
    if (!isScrollbarOverlayAPIAvailable())
        return;

    if (ScrollbarThemeChromiumMac* theme = chromiumScrollbarTheme()) {
        WKScrollbarPainterRef painter = theme->painterForScrollbar(scrollbar);
        wkScrollbarPainterSetDelegate(painter, m_scrollbarPainterDelegate.get());
        wkSetPainterForPainterController(m_scrollbarPainterController.get(), painter, true);
        if (scrollableArea()->inLiveResize())
            wkSetScrollbarPainterKnobAlpha(painter, 1);
    }
}

void ScrollAnimatorChromiumMac::willRemoveHorizontalScrollbar(Scrollbar* scrollbar)
{
    if (!isScrollbarOverlayAPIAvailable())
        return;

    if (ScrollbarThemeChromiumMac* theme = chromiumScrollbarTheme()) {
        WKScrollbarPainterRef painter = theme->painterForScrollbar(scrollbar);
        wkScrollbarPainterSetDelegate(painter, nil);
        wkSetPainterForPainterController(m_scrollbarPainterController.get(), nil, true);
    }
}

void ScrollAnimatorChromiumMac::cancelAnimations()
{
    m_haveScrolledSincePageLoad = false;

    if (isScrollbarOverlayAPIAvailable()) {
        if (scrollbarPaintTimerIsActive())
            stopScrollbarPaintTimer();
        [m_scrollbarPainterDelegate.get() cancelAnimations];
    }
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

bool ScrollAnimatorChromiumMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    m_haveScrolledSincePageLoad = true;

    if (!wheelEvent.hasPreciseScrollingDeltas())
        return ScrollAnimator::handleWheelEvent(wheelEvent);

    // FIXME: This is somewhat roundabout hack to allow forwarding wheel events
    // up to the parent scrollable area. It takes advantage of the fact that
    // the base class implemenatation of handleWheelEvent will not accept the
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
            m_cumulativeHorizontalScroll += wheelEvent.deltaX();
            if (m_scrollerInitiallyPinnedOnLeft && m_cumulativeHorizontalScroll < 0)
                m_didCumulativeHorizontalScrollEverSwitchToOppositeDirectionOfPin = true;
            if (m_scrollerInitiallyPinnedOnRight && m_cumulativeHorizontalScroll > 0)
                m_didCumulativeHorizontalScrollEverSwitchToOppositeDirectionOfPin = true;
        }

        // After a gesture begins, we go through:
        // 1+ PlatformWheelEventPhaseNone
        // 0+ PlatformWheelEventPhaseChanged
        // 1 PlatformWheelEventPhaseEnded if there was at least one changed event
        if (wheelEvent.momentumPhase() == PlatformWheelEventPhaseNone && !m_didCumulativeHorizontalScrollEverSwitchToOppositeDirectionOfPin) {
            if ((isScrollingLeftAndShouldNotRubberBand(wheelEvent, m_scrollableArea) &&
                m_scrollerInitiallyPinnedOnLeft &&
                m_scrollableArea->isHorizontalScrollerPinnedToMinimumPosition()) ||
                (isScrollingRightAndShouldNotRubberBand(wheelEvent, m_scrollableArea) &&
                m_scrollerInitiallyPinnedOnRight &&
                m_scrollableArea->isHorizontalScrollerPinnedToMaximumPosition())) {
                return ScrollAnimator::handleWheelEvent(wheelEvent);
            }
        }
    }

    bool isMomentumScrollEvent = (wheelEvent.momentumPhase() != PlatformWheelEventPhaseNone);
    if (m_ignoreMomentumScrolls && (isMomentumScrollEvent || m_snapRubberBandTimer.isActive())) {
        if (wheelEvent.momentumPhase() == PlatformWheelEventPhaseEnded) {
            m_ignoreMomentumScrolls = false;
            return true;
        }
        return false;
    }

    smoothScrollWithEvent(wheelEvent);
    return true;
}

void ScrollAnimatorChromiumMac::handleGestureEvent(const PlatformGestureEvent& gestureEvent)
{
    if (gestureEvent.type() == PlatformEvent::GestureScrollBegin)
        beginScrollGesture();
    else if (gestureEvent.type() == PlatformEvent::GestureScrollEnd)
        endScrollGesture();
}

bool ScrollAnimatorChromiumMac::pinnedInDirection(float deltaX, float deltaY)
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

bool ScrollAnimatorChromiumMac::allowsVerticalStretching() const
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

bool ScrollAnimatorChromiumMac::allowsHorizontalStretching() const
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

void ScrollAnimatorChromiumMac::smoothScrollWithEvent(const PlatformWheelEvent& wheelEvent)
{
    m_haveScrolledSincePageLoad = true;

    float deltaX = m_overflowScrollDelta.width();
    float deltaY = m_overflowScrollDelta.height();

    // Reset overflow values because we may decide to remove delta at various points and put it into overflow.
    m_overflowScrollDelta = FloatSize();

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
    if (!m_momentumScrollInProgress && (phase == PlatformWheelEventPhaseBegan || phase == PlatformWheelEventPhaseChanged))
        m_momentumScrollInProgress = true;

    CFTimeInterval timeDelta = wheelEvent.timestamp() - m_lastMomentumScrollTimestamp;
    if (m_inScrollGesture || m_momentumScrollInProgress) {
        if (m_lastMomentumScrollTimestamp && timeDelta > 0 && timeDelta < scrollVelocityZeroingTimeout) {
            m_momentumVelocity.setWidth(eventCoalescedDeltaX / (float)timeDelta);
            m_momentumVelocity.setHeight(eventCoalescedDeltaY / (float)timeDelta);
            m_lastMomentumScrollTimestamp = wheelEvent.timestamp();
        } else {
            m_lastMomentumScrollTimestamp = wheelEvent.timestamp();
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
        
            if (m_momentumScrollInProgress) {
                if ((pinnedInDirection(eventCoalescedDeltaX, eventCoalescedDeltaY) || (fabsf(eventCoalescedDeltaX) + fabsf(eventCoalescedDeltaY) <= 0)) && m_lastMomentumScrollTimestamp) {
                    m_ignoreMomentumScrolls = true;
                    m_momentumScrollInProgress = false;
                    snapRubberBand();
                }
            }

            m_stretchScrollForce.setWidth(m_stretchScrollForce.width() + deltaX);
            m_stretchScrollForce.setHeight(m_stretchScrollForce.height() + deltaY);

            FloatSize dampedDelta(ceilf(elasticDeltaForReboundDelta(m_stretchScrollForce.width())), ceilf(elasticDeltaForReboundDelta(m_stretchScrollForce.height())));
            FloatPoint origOrigin = (m_scrollableArea->visibleContentRect().location() + m_scrollableArea->scrollOrigin()) - stretchAmount;
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
        m_lastMomentumScrollTimestamp = 0;
    }
}

void ScrollAnimatorChromiumMac::beginScrollGesture()
{
    didBeginScrollGesture();

    m_haveScrolledSincePageLoad = true;
    m_inScrollGesture = true;
    m_momentumScrollInProgress = false;
    m_ignoreMomentumScrolls = false;
    m_lastMomentumScrollTimestamp = 0;
    m_momentumVelocity = FloatSize();
    m_scrollerInitiallyPinnedOnLeft = m_scrollableArea->isHorizontalScrollerPinnedToMinimumPosition();
    m_scrollerInitiallyPinnedOnRight = m_scrollableArea->isHorizontalScrollerPinnedToMaximumPosition();
    m_cumulativeHorizontalScroll = 0;
    m_didCumulativeHorizontalScrollEverSwitchToOppositeDirectionOfPin = false;
    
    IntSize stretchAmount = m_scrollableArea->overhangAmount();
    m_stretchScrollForce.setWidth(reboundDeltaForElasticDelta(stretchAmount.width()));
    m_stretchScrollForce.setHeight(reboundDeltaForElasticDelta(stretchAmount.height()));

    m_overflowScrollDelta = FloatSize();
    
    if (m_snapRubberBandTimer.isActive())
        m_snapRubberBandTimer.stop();
}

void ScrollAnimatorChromiumMac::endScrollGesture()
{
    didEndScrollGesture();

    snapRubberBand();
}

void ScrollAnimatorChromiumMac::snapRubberBand()
{
    CFTimeInterval timeDelta = [[NSProcessInfo processInfo] systemUptime] - m_lastMomentumScrollTimestamp;
    if (m_lastMomentumScrollTimestamp && timeDelta >= scrollVelocityZeroingTimeout)
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

void ScrollAnimatorChromiumMac::snapRubberBandTimerFired(Timer<ScrollAnimatorChromiumMac>*)
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

            m_scrollableArea->didStartRubberBand(roundedIntSize(m_startStretch));

            m_origOrigin = (m_scrollableArea->visibleContentRect().location() + m_scrollableArea->scrollOrigin()) - m_startStretch;
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

            m_scrollableArea->didCompleteRubberBand(roundedIntSize(m_startStretch));

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

void ScrollAnimatorChromiumMac::setIsActive()
{
    if (isScrollbarOverlayAPIAvailable()) {
        if (needsScrollerStyleUpdate())
            updateScrollerStyle();
    }
}

#if USE(WK_SCROLLBAR_PAINTER)
void ScrollAnimatorChromiumMac::updateScrollerStyle()
{
    if (!scrollableArea()->isOnActivePage()) {
        setNeedsScrollerStyleUpdate(true);
        return;
    }

    ScrollbarThemeChromiumMac* macTheme = chromiumScrollbarTheme();
    if (!macTheme) {
        setNeedsScrollerStyleUpdate(false);
        return;
    }

    int newStyle = wkScrollbarPainterControllerStyle(m_scrollbarPainterController.get());

    if (Scrollbar* verticalScrollbar = scrollableArea()->verticalScrollbar()) {
        verticalScrollbar->invalidate();

        WKScrollbarPainterRef oldVerticalPainter = wkVerticalScrollbarPainterForController(m_scrollbarPainterController.get());
        WKScrollbarPainterRef newVerticalPainter = wkMakeScrollbarReplacementPainter(oldVerticalPainter,
                                                                                     newStyle,
                                                                                     verticalScrollbar->controlSize(),
                                                                                     false);
        macTheme->setNewPainterForScrollbar(verticalScrollbar, newVerticalPainter);
        wkSetPainterForPainterController(m_scrollbarPainterController.get(), newVerticalPainter, false);

        // The different scrollbar styles have different thicknesses, so we must re-set the 
        // frameRect to the new thickness, and the re-layout below will ensure the position
        // and length are properly updated.
        int thickness = macTheme->scrollbarThickness(verticalScrollbar->controlSize());
        verticalScrollbar->setFrameRect(IntRect(0, 0, thickness, thickness));
    }

    if (Scrollbar* horizontalScrollbar = scrollableArea()->horizontalScrollbar()) {
        horizontalScrollbar->invalidate();

        WKScrollbarPainterRef oldHorizontalPainter = wkHorizontalScrollbarPainterForController(m_scrollbarPainterController.get());
        WKScrollbarPainterRef newHorizontalPainter = wkMakeScrollbarReplacementPainter(oldHorizontalPainter,
                                                                                       newStyle,
                                                                                       horizontalScrollbar->controlSize(),
                                                                                       true);
        macTheme->setNewPainterForScrollbar(horizontalScrollbar, newHorizontalPainter);
        wkSetPainterForPainterController(m_scrollbarPainterController.get(), newHorizontalPainter, true);

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

void ScrollAnimatorChromiumMac::startScrollbarPaintTimer()
{
    m_initialScrollbarPaintTimer.startOneShot(0.1);
}

bool ScrollAnimatorChromiumMac::scrollbarPaintTimerIsActive() const
{
    return m_initialScrollbarPaintTimer.isActive();
}

void ScrollAnimatorChromiumMac::stopScrollbarPaintTimer()
{
    m_initialScrollbarPaintTimer.stop();
}

void ScrollAnimatorChromiumMac::initialScrollbarPaintTimerFired(Timer<ScrollAnimatorChromiumMac>*)
{
    wkScrollbarPainterForceFlashScrollers(m_scrollbarPainterController.get());
}
#endif

void ScrollAnimatorChromiumMac::setVisibleScrollerThumbRect(const IntRect& scrollerThumb)
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
