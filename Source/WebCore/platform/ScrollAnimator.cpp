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
#include "ScrollableArea.h"
#include <algorithm>

namespace WebCore {

#if !ENABLE(SMOOTH_SCROLLING) && !PLATFORM(IOS) && !PLATFORM(MAC)
std::unique_ptr<ScrollAnimator> ScrollAnimator::create(ScrollableArea& scrollableArea)
{
    return std::make_unique<ScrollAnimator>(scrollableArea);
}
#endif

ScrollAnimator::ScrollAnimator(ScrollableArea& scrollableArea)
    : m_scrollableArea(scrollableArea)
#if ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)
    , m_scrollController(*this)
#endif
    , m_currentPosX(0)
    , m_currentPosY(0)
{
}

ScrollAnimator::~ScrollAnimator()
{
}

bool ScrollAnimator::scroll(ScrollbarOrientation orientation, ScrollGranularity, float step, float multiplier)
{
    FloatPoint currentPosition(m_currentPosX, m_currentPosY);
    FloatSize delta;
    if (orientation == HorizontalScrollbar)
        delta.setWidth(step * multiplier);
    else
        delta.setHeight(step * multiplier);

    FloatPoint newPosition = FloatPoint(currentPosition + delta).constrainedBetween(m_scrollableArea.minimumScrollPosition(), m_scrollableArea.maximumScrollPosition());
    if (currentPosition == newPosition)
        return false;

    // FIXME: m_currentPosX/Y should just be a FloatPoint.
    m_currentPosX = newPosition.x();
    m_currentPosY = newPosition.y();

    notifyPositionChanged(newPosition - currentPosition);
    return true;
}

void ScrollAnimator::scrollToOffsetWithoutAnimation(const FloatPoint& offset)
{
    FloatSize delta = FloatSize(offset.x() - m_currentPosX, offset.y() - m_currentPosY);
    m_currentPosX = offset.x();
    m_currentPosY = offset.y();
    notifyPositionChanged(delta);
    updateActiveScrollSnapIndexForOffset();
}

#if ENABLE(CSS_SCROLL_SNAP)
#if PLATFORM(MAC)
bool ScrollAnimator::processWheelEventForScrollSnap(const PlatformWheelEvent& wheelEvent)
{
    return m_scrollController.processWheelEventForScrollSnap(wheelEvent);
}
#endif

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
#if ENABLE(CSS_SCROLL_SNAP) && PLATFORM(MAC)
    if (!m_scrollController.processWheelEventForScrollSnap(e))
        return false;
#endif
#if PLATFORM(COCOA)
    // Events in the PlatformWheelEventPhaseMayBegin phase have no deltas, and therefore never passes through the scroll handling logic below.
    // This causes us to return with an 'unhandled' return state, even though this event was successfully processed.
    //
    // We receive at least one PlatformWheelEventPhaseMayBegin when starting main-thread scrolling (see FrameView::wheelEvent), which can
    // fool the scrolling thread into attempting to handle the scroll, unless we treat the event as handled here.
    if (e.phase() == PlatformWheelEventPhaseMayBegin)
        return true;
#endif

    Scrollbar* horizontalScrollbar = m_scrollableArea.horizontalScrollbar();
    Scrollbar* verticalScrollbar = m_scrollableArea.verticalScrollbar();

    // Accept the event if we have a scrollbar in that direction and can still
    // scroll any further.
    float deltaX = horizontalScrollbar ? e.deltaX() : 0;
    float deltaY = verticalScrollbar ? e.deltaY() : 0;

    bool handled = false;

    ScrollGranularity granularity = ScrollByPixel;
    IntSize maxForwardScrollDelta = m_scrollableArea.maximumScrollPosition() - m_scrollableArea.scrollPosition();
    IntSize maxBackwardScrollDelta = m_scrollableArea.scrollPosition() - m_scrollableArea.minimumScrollPosition();
    if ((deltaX < 0 && maxForwardScrollDelta.width() > 0)
        || (deltaX > 0 && maxBackwardScrollDelta.width() > 0)
        || (deltaY < 0 && maxForwardScrollDelta.height() > 0)
        || (deltaY > 0 && maxBackwardScrollDelta.height() > 0)) {
        handled = true;

        if (deltaY) {
            if (e.granularity() == ScrollByPageWheelEvent) {
                bool negative = deltaY < 0;
                deltaY = Scrollbar::pageStepDelta(m_scrollableArea.visibleHeight());
                if (negative)
                    deltaY = -deltaY;
            }
            scroll(VerticalScrollbar, granularity, verticalScrollbar->pixelStep(), -deltaY);
        }

        if (deltaX) {
            if (e.granularity() == ScrollByPageWheelEvent) {
                bool negative = deltaX < 0;
                deltaX = Scrollbar::pageStepDelta(m_scrollableArea.visibleWidth());
                if (negative)
                    deltaX = -deltaX;
            }
            scroll(HorizontalScrollbar, granularity, horizontalScrollbar->pixelStep(), -deltaX);
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
    m_currentPosX = position.x();
    m_currentPosY = position.y();
    updateActiveScrollSnapIndexForOffset();
}

FloatPoint ScrollAnimator::currentPosition() const
{
    return FloatPoint(m_currentPosX, m_currentPosY);
}

void ScrollAnimator::updateActiveScrollSnapIndexForOffset()
{
#if ENABLE(CSS_SCROLL_SNAP)
    m_scrollController.setActiveScrollSnapIndicesForOffset(m_currentPosX, m_currentPosY);
    if (m_scrollController.activeScrollSnapIndexDidChange()) {
        m_scrollableArea.setCurrentHorizontalSnapPointIndex(m_scrollController.activeScrollSnapIndexForAxis(ScrollEventAxis::Horizontal));
        m_scrollableArea.setCurrentVerticalSnapPointIndex(m_scrollController.activeScrollSnapIndexForAxis(ScrollEventAxis::Vertical));
    }
#endif
}

void ScrollAnimator::notifyPositionChanged(const FloatSize& delta)
{
    UNUSED_PARAM(delta);
    // FIXME: need to not map back and forth all the time.
    m_scrollableArea.setScrollOffsetFromAnimation(m_scrollableArea.scrollOffsetFromPosition(IntPoint(m_currentPosX, m_currentPosY)));
}

#if ENABLE(CSS_SCROLL_SNAP)
void ScrollAnimator::updateScrollSnapState()
{
    m_scrollController.updateScrollSnapState(m_scrollableArea);
}

LayoutUnit ScrollAnimator::scrollOffsetOnAxis(ScrollEventAxis axis) const
{
    return axis == ScrollEventAxis::Horizontal ? m_currentPosX : m_currentPosY;
}

void ScrollAnimator::immediateScrollOnAxis(ScrollEventAxis axis, float delta)
{
    FloatPoint currentPosition = this->currentPosition();
    if (axis == ScrollEventAxis::Horizontal)
        scrollToOffsetWithoutAnimation(FloatPoint(currentPosition.x() + delta, currentPosition.y()));
    else
        scrollToOffsetWithoutAnimation(FloatPoint(currentPosition.x(), currentPosition.y() + delta));
}

LayoutSize ScrollAnimator::scrollExtent() const
{
    return m_scrollableArea.contentsSize();
}
#endif

#if (ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)) && PLATFORM(MAC)
void ScrollAnimator::deferTestsForReason(WheelEventTestTrigger::ScrollableAreaIdentifier identifier, WheelEventTestTrigger::DeferTestTriggerReason reason) const
{
    if (!m_wheelEventTestTrigger)
        return;

    m_wheelEventTestTrigger->deferTestsForReason(identifier, reason);
}

void ScrollAnimator::removeTestDeferralForReason(WheelEventTestTrigger::ScrollableAreaIdentifier identifier, WheelEventTestTrigger::DeferTestTriggerReason reason) const
{
    if (!m_wheelEventTestTrigger)
        return;
    
    m_wheelEventTestTrigger->removeTestDeferralForReason(identifier, reason);
}
#endif

} // namespace WebCore
