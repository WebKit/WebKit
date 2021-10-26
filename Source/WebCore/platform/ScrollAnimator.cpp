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
#include "KeyboardScrollingAnimator.h"
#include "LayoutSize.h"
#include "PlatformWheelEvent.h"
#include "ScrollExtents.h"
#include "ScrollableArea.h"
#include "ScrollbarsController.h"
#include "ScrollingEffectsController.h"
#include <algorithm>

namespace WebCore {

#if !PLATFORM(IOS_FAMILY) && !PLATFORM(MAC)
std::unique_ptr<ScrollAnimator> ScrollAnimator::create(ScrollableArea& scrollableArea)
{
    return makeUnique<ScrollAnimator>(scrollableArea);
}
#endif

ScrollAnimator::ScrollAnimator(ScrollableArea& scrollableArea)
    : m_scrollableArea(scrollableArea)
    , m_scrollController(*this)
    , m_scrollControllerAnimationTimer(*this, &ScrollAnimator::scrollControllerAnimationTimerFired)
    , m_keyboardScrollingAnimator(makeUnique<KeyboardScrollingAnimator>(*this, m_scrollController))
{
}

ScrollAnimator::~ScrollAnimator()
{
    m_scrollController.stopAllTimers();
}

bool ScrollAnimator::scroll(ScrollbarOrientation orientation, ScrollGranularity granularity, float step, float multiplier, OptionSet<ScrollBehavior> behavior)
{
    m_scrollableArea.scrollbarsController().setScrollbarAnimationsUnsuspendedByUserInteraction(true);

    auto delta = deltaFromStep(orientation, step, multiplier);
    if (behavior.contains(ScrollBehavior::DoDirectionalSnapping)) {
        behavior.remove(ScrollBehavior::DoDirectionalSnapping);
        if (!m_scrollController.usesScrollSnap())
            return ScrollAnimator::scroll(orientation, granularity, step, multiplier, behavior);

        auto currentOffset = offsetFromPosition(currentPosition());
        auto newOffset = currentOffset + delta;
        if (orientation == HorizontalScrollbar)
            newOffset.setX(m_scrollController.adjustedScrollDestination(ScrollEventAxis::Horizontal, newOffset, multiplier, currentOffset.x()));
        else
            newOffset.setY(m_scrollController.adjustedScrollDestination(ScrollEventAxis::Vertical, newOffset, multiplier, currentOffset.y()));

        auto newDelta = newOffset - currentOffset;
        if (orientation == HorizontalScrollbar)
            return scroll(HorizontalScrollbar, granularity, newDelta.width(), 1.0, behavior);

        return scroll(VerticalScrollbar, granularity, newDelta.height(), 1.0, behavior);
    }

    if (m_scrollableArea.scrollAnimatorEnabled() && platformAllowsScrollAnimation() && !behavior.contains(ScrollBehavior::NeverAnimate)) {
        if (m_scrollController.retargetAnimatedScrollBy(delta))
            return true;

        auto startOffset = offsetFromPosition(m_currentPosition);
        auto extents = scrollExtents();
        auto destinationOffset = (startOffset + delta).constrainedBetween(extents.minimumScrollOffset(), extents.maximumScrollOffset());
        return m_scrollController.startAnimatedScrollToDestination(startOffset, destinationOffset);
    }

    return scrollToPositionWithoutAnimation(currentPosition() + delta);
}

bool ScrollAnimator::scrollToPositionWithoutAnimation(const FloatPoint& position, ScrollClamping clamping)
{
    FloatPoint currentPosition = this->currentPosition();
    auto adjustedPosition = clamping == ScrollClamping::Clamped ? position.constrainedBetween(scrollableArea().minimumScrollPosition(), scrollableArea().maximumScrollPosition()) : position;

    // FIXME: In some cases on iOS the ScrollableArea position is out of sync with the ScrollAnimator position.
    // When these cases are fixed, this extra check against the ScrollableArea position can be removed.
    if (adjustedPosition == currentPosition && adjustedPosition == scrollableArea().scrollPosition() && !scrollableArea().scrollOriginChanged())
        return false;

    m_scrollController.stopAnimatedScroll();

    setCurrentPosition(adjustedPosition, NotifyScrollableArea::Yes);
    return true;
}

bool ScrollAnimator::scrollToPositionWithAnimation(const FloatPoint& newPosition)
{
    bool positionChanged = newPosition != currentPosition();
    if (!positionChanged && !scrollableArea().scrollOriginChanged())
        return false;

    return m_scrollController.startAnimatedScrollToDestination(offsetFromPosition(m_currentPosition), offsetFromPosition(newPosition));
}

void ScrollAnimator::retargetRunningAnimation(const FloatPoint& newPosition)
{
    ASSERT(scrollableArea().scrollAnimationStatus() == ScrollAnimationStatus::Animating);
    m_scrollController.retargetAnimatedScroll(offsetFromPosition(newPosition));
}

FloatPoint ScrollAnimator::offsetFromPosition(const FloatPoint& position) const
{
    return ScrollableArea::scrollOffsetFromPosition(position, toFloatSize(m_scrollableArea.scrollOrigin()));
}

FloatPoint ScrollAnimator::positionFromOffset(const FloatPoint& offset) const
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

bool ScrollAnimator::activeScrollSnapIndexDidChange() const
{
    return m_scrollController.activeScrollSnapIndexDidChange();
}

std::optional<unsigned> ScrollAnimator::activeScrollSnapIndexForAxis(ScrollEventAxis axis) const
{
    return m_scrollController.activeScrollSnapIndexForAxis(axis);
}

void ScrollAnimator::setActiveScrollSnapIndexForAxis(ScrollEventAxis axis, std::optional<unsigned> index)
{
    return m_scrollController.setActiveScrollSnapIndexForAxis(axis, index);
}

void ScrollAnimator::resnapAfterLayout()
{
    m_scrollController.resnapAfterLayout();
}

bool ScrollAnimator::handleWheelEvent(const PlatformWheelEvent& e)
{
    if (processWheelEventForScrollSnap(e))
        return false;

#if PLATFORM(COCOA)
    // Events in the PlatformWheelEventPhase::MayBegin phase have no deltas, and therefore never passes through the scroll handling logic below.
    // This causes us to return with an 'unhandled' return state, even though this event was successfully processed.
    //
    // We receive at least one PlatformWheelEventPhase::MayBegin when starting main-thread scrolling (see FrameView::wheelEvent), which can
    // fool the scrolling thread into attempting to handle the scroll, unless we treat the event as handled here.
    if (e.phase() == PlatformWheelEventPhase::MayBegin)
        return true;
#endif

#if PLATFORM(MAC)
    // FIXME: We should be able to remove this code, but Mac's handleWheelEvent relies on this somehow.
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
#else
    return m_scrollController.handleWheelEvent(e);
#endif
}

void ScrollAnimator::stopKeyboardScrollAnimation()
{
    m_scrollController.stopKeyboardScrolling();
}

#if ENABLE(TOUCH_EVENTS)
bool ScrollAnimator::handleTouchEvent(const PlatformTouchEvent&)
{
    return false;
}
#endif

void ScrollAnimator::setCurrentPosition(const FloatPoint& position, NotifyScrollableArea notify)
{
    // FIXME: An early return here if the position is not changing triggers test failures because of adjustForIOSCaretWhenScrolling()
    // code in RenderLayerScrollableArea. We can early return when webkit.org/b/230454 is fixed.
    auto delta = position - m_currentPosition;
    m_currentPosition = position;
    
    if (notify == NotifyScrollableArea::Yes)
        notifyPositionChanged(delta);
    
    updateActiveScrollSnapIndexForOffset();
}

void ScrollAnimator::updateActiveScrollSnapIndexForOffset()
{
    m_scrollController.updateActiveScrollSnapIndexForClientOffset();
}

void ScrollAnimator::notifyPositionChanged(const FloatSize& delta)
{
    m_scrollableArea.scrollbarsController().notifyContentAreaScrolled(delta);
    m_scrollableArea.setScrollPositionFromAnimation(roundedIntPoint(m_currentPosition));
    m_scrollController.scrollPositionChanged();
}

void ScrollAnimator::setSnapOffsetsInfo(const LayoutScrollSnapOffsetsInfo& info)
{
    m_scrollController.setSnapOffsetsInfo(info);
}

const LayoutScrollSnapOffsetsInfo* ScrollAnimator::snapOffsetsInfo() const
{
    return m_scrollController.snapOffsetsInfo();
}

FloatPoint ScrollAnimator::scrollOffset() const
{
    return m_scrollableArea.scrollOffsetFromPosition(roundedIntPoint(currentPosition()));
}

bool ScrollAnimator::allowsHorizontalScrolling() const
{
    return m_scrollableArea.allowsHorizontalScrolling();
}

bool ScrollAnimator::allowsVerticalScrolling() const
{
    return m_scrollableArea.allowsVerticalScrolling();
}

void ScrollAnimator::willStartAnimatedScroll()
{
    m_scrollableArea.setScrollAnimationStatus(ScrollAnimationStatus::Animating);
}

void ScrollAnimator::didStopAnimatedScroll()
{
    m_scrollableArea.setScrollAnimationStatus(ScrollAnimationStatus::NotAnimating);
}

#if HAVE(RUBBER_BANDING)
IntSize ScrollAnimator::stretchAmount() const
{
    return m_scrollableArea.overhangAmount();
}

RectEdges<bool> ScrollAnimator::edgePinnedState() const
{
    return m_scrollableArea.edgePinnedState();
}

bool ScrollAnimator::isPinnedOnSide(BoxSide side) const
{
    return m_scrollableArea.isPinnedOnSide(side);
}

#endif

void ScrollAnimator::adjustScrollPositionToBoundsIfNecessary()
{
    auto previousClamping = m_scrollableArea.scrollClamping();
    m_scrollableArea.setScrollClamping(ScrollClamping::Clamped);

    auto currentScrollPosition = m_scrollableArea.scrollPosition();
    auto constrainedPosition = m_scrollableArea.constrainScrollPosition(currentScrollPosition);
    immediateScrollBy(constrainedPosition - currentScrollPosition);

    m_scrollableArea.setScrollClamping(previousClamping);
}

FloatPoint ScrollAnimator::adjustScrollPositionIfNecessary(const FloatPoint& position) const
{
    if (m_scrollableArea.scrollClamping() == ScrollClamping::Unclamped)
        return position;

    return m_scrollableArea.constrainScrollPosition(ScrollPosition(position));
}

void ScrollAnimator::immediateScrollBy(const FloatSize& delta, ScrollClamping clamping)
{
    auto previousClamping = m_scrollableArea.scrollClamping();
    m_scrollableArea.setScrollClamping(clamping);

    auto currentPosition = this->currentPosition();
    auto newPosition = adjustScrollPositionIfNecessary(currentPosition + delta);
    if (newPosition != currentPosition)
        setCurrentPosition(newPosition, NotifyScrollableArea::Yes);

    m_scrollableArea.setScrollClamping(previousClamping);
}

ScrollExtents ScrollAnimator::scrollExtents() const
{
    return {
        m_scrollableArea.totalContentsSize(),
        m_scrollableArea.visibleSize()
    };
}

float ScrollAnimator::pageScaleFactor() const
{
    return m_scrollableArea.pageScaleFactor();
}

std::unique_ptr<ScrollingEffectsControllerTimer> ScrollAnimator::createTimer(Function<void()>&& function)
{
    return makeUnique<ScrollingEffectsControllerTimer>(RunLoop::current(), [function = WTFMove(function), weakScrollableArea = WeakPtr { m_scrollableArea }] {
        if (!weakScrollableArea)
            return;
        function();
    });
}

void ScrollAnimator::startAnimationCallback(ScrollingEffectsController&)
{
    if (m_scrollControllerAnimationTimer.isActive())
        return;

    m_scrollControllerAnimationTimer.startRepeating(1_s / 60.);
}

void ScrollAnimator::stopAnimationCallback(ScrollingEffectsController&)
{
    m_scrollControllerAnimationTimer.stop();
}

void ScrollAnimator::scrollControllerAnimationTimerFired()
{
    m_scrollController.animationCallback(MonotonicTime::now());
}

#if PLATFORM(MAC)
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

#if PLATFORM(GTK) || USE(NICOSIA)
bool ScrollAnimator::scrollAnimationEnabled() const
{
    return m_scrollableArea.scrollAnimatorEnabled() && platformAllowsScrollAnimation();
}
#endif

void ScrollAnimator::cancelAnimations()
{
    m_scrollController.stopAnimatedScroll();
    m_scrollableArea.scrollbarsController().cancelAnimations();
}

void ScrollAnimator::contentsSizeChanged()
{
    m_scrollController.contentsSizeChanged();
}

FloatPoint ScrollAnimator::scrollOffsetAdjustedForSnapping(const FloatPoint& offset, ScrollSnapPointSelectionMethod method) const
{
    if (!m_scrollController.usesScrollSnap())
        return offset;

    return {
        scrollOffsetAdjustedForSnapping(ScrollEventAxis::Horizontal, offset, method),
        scrollOffsetAdjustedForSnapping(ScrollEventAxis::Vertical, offset, method)
    };
}

float ScrollAnimator::scrollOffsetAdjustedForSnapping(ScrollEventAxis axis, const FloatPoint& newOffset, ScrollSnapPointSelectionMethod method) const
{
    if (!m_scrollController.usesScrollSnap())
        return axis == ScrollEventAxis::Horizontal ? newOffset.x() : newOffset.y();

    std::optional<float> originalOffset;
    float velocityInScrollAxis = 0.;
    if (method == ScrollSnapPointSelectionMethod::Directional) {
        FloatSize scrollOrigin = toFloatSize(m_scrollableArea.scrollOrigin());
        auto currentOffset = ScrollableArea::scrollOffsetFromPosition(this->currentPosition(), scrollOrigin);
        auto velocity = newOffset - currentOffset;
        originalOffset = axis == ScrollEventAxis::Horizontal ? currentOffset.x() : currentOffset.y();
        velocityInScrollAxis = axis == ScrollEventAxis::Horizontal ? velocity.width() : velocity.height();
    }

    return m_scrollController.adjustedScrollDestination(axis, newOffset, velocityInScrollAxis, originalOffset);
}

} // namespace WebCore
