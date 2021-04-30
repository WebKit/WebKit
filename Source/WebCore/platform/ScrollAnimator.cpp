/*
 * Copyright (C) 2014-2015 Apple Inc.  All rights reserved.
 * Copyright (c) 2010, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollAnimator.h"

#include "FloatPoint.h"
#include "LayoutSize.h"
#include "PlatformWheelEvent.h"
#include "ScrollAnimationSmooth.h"
#include "ScrollableArea.h"
#include <algorithm>

#if ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)
#include "ScrollController.h"
#endif

namespace WebCore {

#if !ENABLE(SMOOTH_SCROLLING) && !PLATFORM(IOS_FAMILY) && !PLATFORM(MAC) && !PLATFORM(WPE)
std::unique_ptr<ScrollAnimator> ScrollAnimator::create(ScrollableArea& scrollableArea)
{
    return makeUnique<ScrollAnimator>(scrollableArea);
}
#endif

ScrollAnimator::ScrollAnimator(ScrollableArea& scrollableArea)
    : m_scrollableArea(scrollableArea)
#if ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)
    , m_scrollController(*this)
#endif
    , m_scrollAnimation(makeUnique<ScrollAnimationSmooth>(
        [this]() -> ScrollExtents {
            return { m_scrollableArea.minimumScrollPosition(), m_scrollableArea.maximumScrollPosition(), m_scrollableArea.visibleSize() };
        },
        m_currentPosition,
        [this](FloatPoint&& position) {
            FloatSize delta = position - m_currentPosition;
            m_currentPosition = WTFMove(position);
            notifyPositionChanged(delta);
            updateActiveScrollSnapIndexForOffset();
        },
        [this] {
            m_scrollableArea.setScrollBehaviorStatus(ScrollBehaviorStatus::NotInAnimation);
        }))
{
}

ScrollAnimator::~ScrollAnimator()
{
#if ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)
    m_scrollController.stopAllTimers();
#endif
}

bool ScrollAnimator::scroll(ScrollbarOrientation orientation, ScrollGranularity granularity, float step, float multiplier, OptionSet<ScrollBehavior> behavior)
{
    auto delta = deltaFromStep(orientation, step, multiplier);
#if ENABLE(CSS_SCROLL_SNAP)
    if (behavior.contains(ScrollBehavior::DoDirectionalSnapping)) {
        behavior.remove(ScrollBehavior::DoDirectionalSnapping);
        if (!m_scrollController.usesScrollSnap())
            return scroll(orientation, granularity, step, multiplier, behavior);

        auto currentOffset = offsetFromPosition(currentPosition());
        auto newOffset = currentOffset + delta;
        if (orientation == HorizontalScrollbar)
            newOffset.setX(m_scrollController.adjustScrollDestination(ScrollEventAxis::Horizontal, newOffset.x(), multiplier, currentOffset.x()));
        else
            newOffset.setY(m_scrollController.adjustScrollDestination(ScrollEventAxis::Vertical, newOffset.y(), multiplier, currentOffset.y()));
        auto newDelta = newOffset - currentOffset;

        if (orientation == HorizontalScrollbar)
            return scroll(HorizontalScrollbar, granularity, newDelta.width(), 1.0, behavior);
        return scroll(VerticalScrollbar, granularity, newDelta.height(), 1.0, behavior);
    }
#else
    UNUSED_PARAM(granularity);
    UNUSED_PARAM(behavior);
#endif

#if ENABLE(SMOOTH_SCROLLING) && !PLATFORM(IOS_FAMILY)
    if (m_scrollableArea.scrollAnimatorEnabled() && !behavior.contains(ScrollBehavior::NeverAnimate)) {
        if (!m_scrollAnimation->isActive())
            m_scrollAnimation->setCurrentPosition(m_currentPosition);
        return m_scrollAnimation->scroll(orientation, granularity, step, multiplier);
    }
#endif

    return scrollToPositionWithoutAnimation(currentPosition() + delta);
}

bool ScrollAnimator::scrollToOffsetWithoutAnimation(const FloatPoint& offset, ScrollClamping clamping)
{
    return scrollToPositionWithoutAnimation(ScrollableArea::scrollPositionFromOffset(offset, toFloatSize(scrollableArea().scrollOrigin())), clamping);
}

bool ScrollAnimator::scrollToPositionWithoutAnimation(const FloatPoint& position, ScrollClamping clamping)
{
    FloatPoint currentPosition = this->currentPosition();
    auto adjustedPosition = clamping == ScrollClamping::Clamped ? position.constrainedBetween(scrollableArea().minimumScrollPosition(), scrollableArea().maximumScrollPosition()) : position;

    // FIXME: In some cases on iOS the ScrollableArea position is out of sync with the ScrollAnimator position.
    // When these cases are fixed, this extra check against the ScrollableArea position can be removed.
    if (adjustedPosition == currentPosition && adjustedPosition == scrollableArea().scrollPosition() && !scrollableArea().scrollOriginChanged())
        return false;

    m_scrollAnimation->setCurrentPosition(adjustedPosition);
    m_currentPosition = adjustedPosition;
    notifyPositionChanged(adjustedPosition - currentPosition);
    updateActiveScrollSnapIndexForOffset();
    return true;
}

bool ScrollAnimator::scrollToOffsetWithAnimation(const FloatPoint& offset)
{
    return scrollToPositionWithAnimation(ScrollableArea::scrollPositionFromOffset(offset, toFloatSize(scrollableArea().scrollOrigin())));
}

bool ScrollAnimator::scrollToPositionWithAnimation(const FloatPoint& newPosition)
{
    bool positionChanged = newPosition != currentPosition();
    if (!positionChanged && !scrollableArea().scrollOriginChanged())
        return false;

    m_scrollAnimation->setCurrentPosition(m_currentPosition);
    m_scrollAnimation->scroll(newPosition);
    scrollableArea().setScrollBehaviorStatus(ScrollBehaviorStatus::InNonNativeAnimation);
    return true;
}

FloatPoint ScrollAnimator::offsetFromPosition(const FloatPoint& position)
{
    return ScrollableArea::scrollOffsetFromPosition(position, toFloatSize(m_scrollableArea.scrollOrigin()));
}

FloatPoint ScrollAnimator::positionFromOffset(const FloatPoint& offset)
{
    return ScrollableArea::scrollPositionFromOffset(offset, toFloatSize(m_scrollableArea.scrollOrigin()));
}

FloatSize ScrollAnimator::deltaFromStep(ScrollbarOrientation orientation, float step, float multiplier)
{
    FloatSize delta;
    if (orientation == HorizontalScrollbar)
        delta.setWidth(step * multiplier);
    else
        delta.setHeight(step * multiplier);
    return delta;
}

#if ENABLE(CSS_SCROLL_SNAP)
bool ScrollAnimator::activeScrollSnapIndexDidChange() const
{
    return m_scrollController.activeScrollSnapIndexDidChange();
}

unsigned ScrollAnimator::activeScrollSnapIndexForAxis(ScrollEventAxis axis) const
{
    return m_scrollController.activeScrollSnapIndexForAxis(axis);
}
#endif

bool ScrollAnimator::handleWheelEvent(const PlatformWheelEvent& e)
{
#if ENABLE(CSS_SCROLL_SNAP)
    if (processWheelEventForScrollSnap(e))
        return false;
#endif

#if PLATFORM(COCOA)
    // Events in the PlatformWheelEventPhase::MayBegin phase have no deltas, and therefore never passes through the scroll handling logic below.
    // This causes us to return with an 'unhandled' return state, even though this event was successfully processed.
    //
    // We receive at least one PlatformWheelEventPhase::MayBegin when starting main-thread scrolling (see FrameView::wheelEvent), which can
    // fool the scrolling thread into attempting to handle the scroll, unless we treat the event as handled here.
    if (e.phase() == PlatformWheelEventPhase::MayBegin)
        return true;
#endif

    Scrollbar* horizontalScrollbar = m_scrollableArea.horizontalScrollbar();
    Scrollbar* verticalScrollbar = m_scrollableArea.verticalScrollbar();

    // Accept the event if we have a scrollbar in that direction and can still
    // scroll any further.
    float deltaX = horizontalScrollbar ? e.deltaX() : 0;
    float deltaY = verticalScrollbar ? e.deltaY() : 0;

    bool handled = false;

    IntSize maxForwardScrollDelta = m_scrollableArea.maximumScrollPosition() - m_scrollableArea.scrollPosition();
    IntSize maxBackwardScrollDelta = m_scrollableArea.scrollPosition() - m_scrollableArea.minimumScrollPosition();
    if ((deltaX < 0 && maxForwardScrollDelta.width() > 0)
        || (deltaX > 0 && maxBackwardScrollDelta.width() > 0)
        || (deltaY < 0 && maxForwardScrollDelta.height() > 0)
        || (deltaY > 0 && maxBackwardScrollDelta.height() > 0)) {
        handled = true;

        OptionSet<ScrollBehavior> behavior(ScrollBehavior::DoDirectionalSnapping);
        if (e.hasPreciseScrollingDeltas())
            behavior.add(ScrollBehavior::NeverAnimate);

        if (deltaY) {
            if (e.granularity() == ScrollByPageWheelEvent) {
                bool negative = deltaY < 0;
                deltaY = Scrollbar::pageStepDelta(m_scrollableArea.visibleHeight());
                if (negative)
                    deltaY = -deltaY;
            }
            scroll(VerticalScrollbar, ScrollByPixel, verticalScrollbar->pixelStep(), -deltaY, behavior);
        }

        if (deltaX) {
            if (e.granularity() == ScrollByPageWheelEvent) {
                bool negative = deltaX < 0;
                deltaX = Scrollbar::pageStepDelta(m_scrollableArea.visibleWidth());
                if (negative)
                    deltaX = -deltaX;
            }
            scroll(HorizontalScrollbar, ScrollByPixel, horizontalScrollbar->pixelStep(), -deltaX, behavior);
        }
    }
    return handled;
}

#if ENABLE(TOUCH_EVENTS)
bool ScrollAnimator::handleTouchEvent(const PlatformTouchEvent&)
{
    return false;
}
#endif

void ScrollAnimator::setCurrentPosition(const FloatPoint& position)
{
    m_currentPosition = position;
    updateActiveScrollSnapIndexForOffset();
}

void ScrollAnimator::updateActiveScrollSnapIndexForOffset()
{
#if ENABLE(CSS_SCROLL_SNAP)
    auto scrollOffset = m_scrollableArea.scrollOffsetFromPosition(roundedIntPoint(currentPosition()));
    m_scrollController.setActiveScrollSnapIndicesForOffset(scrollOffset);
    if (m_scrollController.activeScrollSnapIndexDidChange()) {
        m_scrollableArea.setCurrentHorizontalSnapPointIndex(m_scrollController.activeScrollSnapIndexForAxis(ScrollEventAxis::Horizontal));
        m_scrollableArea.setCurrentVerticalSnapPointIndex(m_scrollController.activeScrollSnapIndexForAxis(ScrollEventAxis::Vertical));
    }
#endif
}

void ScrollAnimator::notifyPositionChanged(const FloatSize& delta)
{
    UNUSED_PARAM(delta);
    m_scrollableArea.setScrollPositionFromAnimation(roundedIntPoint(currentPosition()));

#if ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)
    m_scrollController.scrollPositionChanged();
#endif
}

#if ENABLE(CSS_SCROLL_SNAP)
void ScrollAnimator::updateScrollSnapState()
{
    m_scrollController.updateScrollSnapState(m_scrollableArea);
}

FloatPoint ScrollAnimator::scrollOffset() const
{
    return m_scrollableArea.scrollOffsetFromPosition(roundedIntPoint(currentPosition()));
}

void ScrollAnimator::immediateScrollOnAxis(ScrollEventAxis axis, float delta)
{
    FloatSize deltaSize;
    if (axis == ScrollEventAxis::Horizontal)
        deltaSize.setWidth(delta);
    else
        deltaSize.setHeight(delta);

    scrollToOffsetWithoutAnimation(currentPosition() + deltaSize);
}

LayoutSize ScrollAnimator::scrollExtent() const
{
    return m_scrollableArea.contentsSize();
}

FloatSize ScrollAnimator::viewportSize() const
{
    return m_scrollableArea.visibleSize();
}

float ScrollAnimator::pageScaleFactor() const
{
    return m_scrollableArea.pageScaleFactor();
}
#endif

#if ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)
std::unique_ptr<ScrollControllerTimer> ScrollAnimator::createTimer(Function<void()>&& function)
{
    return WTF::makeUnique<ScrollControllerTimer>(RunLoop::current(), [function = WTFMove(function), weakScrollableArea = makeWeakPtr(m_scrollableArea)] {
        if (!weakScrollableArea)
            return;
        function();
    });
}
#endif

#if (ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)) && PLATFORM(MAC)
void ScrollAnimator::deferWheelEventTestCompletionForReason(WheelEventTestMonitor::ScrollableAreaIdentifier identifier, WheelEventTestMonitor::DeferReason reason) const
{
    if (!m_wheelEventTestMonitor)
        return;

    m_wheelEventTestMonitor->deferForReason(identifier, reason);
}

void ScrollAnimator::removeWheelEventTestCompletionDeferralForReason(WheelEventTestMonitor::ScrollableAreaIdentifier identifier, WheelEventTestMonitor::DeferReason reason) const
{
    if (!m_wheelEventTestMonitor)
        return;
    
    m_wheelEventTestMonitor->removeDeferralForReason(identifier, reason);
}
#endif

void ScrollAnimator::cancelAnimations()
{
#if !USE(REQUEST_ANIMATION_FRAME_TIMER)
    m_scrollAnimation->stop();
#endif
}

void ScrollAnimator::willEndLiveResize()
{
    m_scrollAnimation->updateVisibleLengths();
}

void ScrollAnimator::didAddVerticalScrollbar(Scrollbar*)
{
    m_scrollAnimation->updateVisibleLengths();
}

void ScrollAnimator::didAddHorizontalScrollbar(Scrollbar*)
{
    m_scrollAnimation->updateVisibleLengths();
}

FloatPoint ScrollAnimator::adjustScrollOffsetForSnappingIfNeeded(const FloatPoint& offset, ScrollSnapPointSelectionMethod method)
{
#if ENABLE(CSS_SCROLL_SNAP)
    if (!m_scrollController.usesScrollSnap())
        return offset;

    FloatPoint newOffset = offset;
    newOffset.setX(adjustScrollOffsetForSnappingIfNeeded(ScrollEventAxis::Horizontal, newOffset.x(), method));
    newOffset.setY(adjustScrollOffsetForSnappingIfNeeded(ScrollEventAxis::Vertical, newOffset.y(), method));
    return newOffset;
#else
    UNUSED_PARAM(method);
    return offset;
#endif
}

float ScrollAnimator::adjustScrollOffsetForSnappingIfNeeded(ScrollEventAxis axis, float newOffset, ScrollSnapPointSelectionMethod method)
{
#if ENABLE(CSS_SCROLL_SNAP)
    if (!m_scrollController.usesScrollSnap())
        return newOffset;

    Optional<float> originalOffset;
    float velocity = 0.;
    if (method == ScrollSnapPointSelectionMethod::Directional) {
        FloatSize scrollOrigin = toFloatSize(m_scrollableArea.scrollOrigin());
        auto currentOffset = ScrollableArea::scrollOffsetFromPosition(this->currentPosition(), scrollOrigin);
        originalOffset = axis == ScrollEventAxis::Horizontal ? currentOffset.x() : currentOffset.y();
        velocity = newOffset - (axis == ScrollEventAxis::Horizontal ? currentOffset.x() : currentOffset.y());
    }

    return m_scrollController.adjustScrollDestination(axis, newOffset, velocity, originalOffset);
#else
    UNUSED_PARAM(method);
    UNUSED_PARAM(axis);
    return newOffset;
#endif
}


} // namespace WebCore
