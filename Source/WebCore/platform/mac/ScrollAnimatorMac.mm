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
#import "PlatformWheelEvent.h"
#import "ScrollView.h"
#import "ScrollableArea.h"
#import "ScrollbarsController.h"
#import <wtf/BlockObjCExceptions.h>
#import <wtf/NakedPtr.h>
#import <wtf/text/TextStream.h>

using WebCore::ScrollableArea;
using WebCore::ScrollAnimatorMac;
using WebCore::Scrollbar;
using WebCore::GraphicsLayer;
using WebCore::VerticalScrollbar;
using WebCore::IntRect;

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

namespace WebCore {

std::unique_ptr<ScrollAnimator> ScrollAnimator::create(ScrollableArea& scrollableArea)
{
    return makeUnique<ScrollAnimatorMac>(scrollableArea);
}

ScrollAnimatorMac::ScrollAnimatorMac(ScrollableArea& scrollableArea)
    : ScrollAnimator(scrollableArea)
{
    m_scrollAnimationHelperDelegate = adoptNS([[WebScrollAnimationHelperDelegate alloc] initWithScrollAnimator:this]);
    m_scrollAnimationHelper = adoptNS([[NSClassFromString(@"NSScrollAnimationHelper") alloc] initWithDelegate:m_scrollAnimationHelperDelegate.get()]);

}

ScrollAnimatorMac::~ScrollAnimatorMac() = default;

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
    setHaveScrolledSincePageLoad(true);

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
    return m_scrollController.isScrollSnapInProgress();
}

void ScrollAnimatorMac::immediateScrollToPositionForScrollAnimation(const FloatPoint& newPosition)
{
    ASSERT(m_scrollAnimationHelper);
    ScrollAnimator::scrollToPositionWithoutAnimation(newPosition);
}

void ScrollAnimatorMac::notifyPositionChanged(const FloatSize& delta)
{
    m_scrollableArea.scrollbarsController().notifyContentAreaScrolled(delta);
    ScrollAnimator::notifyPositionChanged(delta);
}

void ScrollAnimatorMac::cancelAnimations()
{
    ScrollAnimator::cancelAnimations();
    setHaveScrolledSincePageLoad(false);
    m_scrollableArea.scrollbarsController().cancelAnimations();
}

void ScrollAnimatorMac::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollAnimatorMac " << this << " scrollableArea " << m_scrollableArea << " handleWheelEventPhase " << phase);

    // This may not have been set to true yet if the wheel event was handled by the ScrollingTree,
    // So set it to true here.
    setHaveScrolledSincePageLoad(true);

    // FIXME: Need to ensure we get PlatformWheelEventPhase::Ended.
    if (phase == PlatformWheelEventPhase::Began)
        m_scrollableArea.scrollbarsController().didBeginScrollGesture();
    else if (phase == PlatformWheelEventPhase::Ended || phase == PlatformWheelEventPhase::Cancelled)
        m_scrollableArea.scrollbarsController().didEndScrollGesture();
    else if (phase == PlatformWheelEventPhase::MayBegin)
        m_scrollableArea.scrollbarsController().mayBeginScrollGesture();
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
    setHaveScrolledSincePageLoad(true);

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

bool ScrollAnimatorMac::allowsVerticalStretching(const PlatformWheelEvent& wheelEvent) const
{
    switch (m_scrollableArea.verticalScrollElasticity()) {
    case ScrollElasticityAutomatic: {
        Scrollbar* hScroller = m_scrollableArea.horizontalScrollbar();
        Scrollbar* vScroller = m_scrollableArea.verticalScrollbar();
        bool scrollbarsAllowStretching = ((vScroller && vScroller->enabled()) || (!hScroller || !hScroller->enabled()));
        bool eventPreventsStretching = m_scrollableArea.hasScrollableOrRubberbandableAncestor() && wheelEvent.isGestureStart() && m_scrollableArea.isPinnedForScrollDeltaOnAxis(-wheelEvent.deltaY(), ScrollEventAxis::Vertical);
        if (!eventPreventsStretching)
            eventPreventsStretching = gestureShouldBeginSnap(wheelEvent, ScrollEventAxis::Vertical, m_scrollableArea.snapOffsetsInfo());
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
        if (!eventPreventsStretching)
            eventPreventsStretching = gestureShouldBeginSnap(wheelEvent, ScrollEventAxis::Horizontal, m_scrollableArea.snapOffsetsInfo());
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

bool ScrollAnimatorMac::processWheelEventForScrollSnap(const PlatformWheelEvent& wheelEvent)
{
    return m_scrollController.processWheelEventForScrollSnap(wheelEvent);
}

} // namespace WebCore

#endif // ENABLE(SMOOTH_SCROLLING)
