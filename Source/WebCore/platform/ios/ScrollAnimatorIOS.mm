/*
 * Copyright (C) 2011, 2015 Apple Inc. All rights reserved.
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
#import "ScrollAnimatorIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "Frame.h"
#import "RenderLayer.h"
#import "ScrollableArea.h"

#if ENABLE(TOUCH_EVENTS)
#import "PlatformTouchEventIOS.h"
#endif

namespace WebCore {

std::unique_ptr<ScrollAnimator> ScrollAnimator::create(ScrollableArea& scrollableArea)
{
    return makeUnique<ScrollAnimatorIOS>(scrollableArea);
}

ScrollAnimatorIOS::ScrollAnimatorIOS(ScrollableArea& scrollableArea)
    : ScrollAnimator(scrollableArea)
#if ENABLE(TOUCH_EVENTS)
    , m_touchScrollAxisLatch(AxisLatchNotComputed)
    , m_inTouchSequence(false)
    , m_committedToScrollAxis(false)
    , m_startedScroll(false)
    , m_scrollableAreaForTouchSequence(0)
#endif
{
}

ScrollAnimatorIOS::~ScrollAnimatorIOS()
{
}

#if ENABLE(TOUCH_EVENTS)
bool ScrollAnimatorIOS::handleTouchEvent(const PlatformTouchEvent& touchEvent)
{
    if (touchEvent.type() == PlatformEvent::TouchStart && touchEvent.touchCount() == 1) {
        m_firstTouchPoint = touchEvent.touchLocationAtIndex(0);
        m_lastTouchPoint = m_firstTouchPoint;
        m_inTouchSequence = true;
        m_committedToScrollAxis = false;
        m_startedScroll = false;
        m_touchScrollAxisLatch = AxisLatchNotComputed;
        // Never claim to have handled the TouchStart, because that will kill default scrolling behavior.
        return false;
    }

    if (!m_inTouchSequence)
        return false;

    if (touchEvent.type() == PlatformEvent::TouchEnd || touchEvent.type() == PlatformEvent::TouchCancel) {
        m_inTouchSequence = false;
        m_scrollableAreaForTouchSequence = 0;
        if (m_startedScroll)
            scrollableArea().didEndScroll();
        return false;
    }

    // If a second touch appears, assume that the user is trying to zoom, and bail on the scrolling sequence.
    // FIXME: if that second touch is inside the scrollable area, should we keep scrolling?
    if (touchEvent.touchCount() != 1) {
        m_inTouchSequence = false;
        m_scrollableAreaForTouchSequence = 0;
        if (m_startedScroll)
            scrollableArea().didEndScroll();
        return false;
    }
    
    IntPoint currentPoint = touchEvent.touchLocationAtIndex(0);

    IntSize touchDelta = m_lastTouchPoint - currentPoint;
    m_lastTouchPoint = currentPoint;

    if (!m_scrollableAreaForTouchSequence)
        determineScrollableAreaForTouchSequence(touchDelta);

    if (!m_committedToScrollAxis) {
        auto scrollSize = m_scrollableArea.maximumScrollPosition() - m_scrollableArea.minimumScrollPosition();
        bool horizontallyScrollable = scrollSize.width();
        bool verticallyScrollable = scrollSize.height();

        if (!horizontallyScrollable && !verticallyScrollable)
            return false;

        IntSize deltaFromStart = m_firstTouchPoint - currentPoint;
    
        const int latchAxisMovementThreshold = 10;
        if (!horizontallyScrollable && verticallyScrollable) {
            m_touchScrollAxisLatch = AxisLatchVertical;
            if (abs(deltaFromStart.height()) >= latchAxisMovementThreshold)
                m_committedToScrollAxis = true;
        } else if (horizontallyScrollable && !verticallyScrollable) {
            m_touchScrollAxisLatch = AxisLatchHorizontal;
            if (abs(deltaFromStart.width()) >= latchAxisMovementThreshold)
                m_committedToScrollAxis = true;
        } else {
            m_committedToScrollAxis = true;

            if (m_touchScrollAxisLatch == AxisLatchNotComputed) {
                const float lockAngleDegrees = 20;
                if (deltaFromStart.width() && deltaFromStart.height()) {
                    float dragAngle = atanf(static_cast<float>(abs(deltaFromStart.height())) / abs(deltaFromStart.width()));
                    if (dragAngle <= deg2rad(lockAngleDegrees))
                        m_touchScrollAxisLatch = AxisLatchHorizontal;
                    else if (dragAngle >= deg2rad(90 - lockAngleDegrees))
                        m_touchScrollAxisLatch = AxisLatchVertical;
                }
            }
        }
        
        if (!m_committedToScrollAxis)
            return false;
    }

    bool handled = false;
    
    // Horizontal
    if (m_touchScrollAxisLatch != AxisLatchVertical) {
        int delta = touchDelta.width();
        handled |= m_scrollableAreaForTouchSequence->scroll(delta < 0 ? ScrollLeft : ScrollRight, ScrollByPixel, abs(delta));
    }
    
    // Vertical
    if (m_touchScrollAxisLatch != AxisLatchHorizontal) {
        int delta = touchDelta.height();
        handled |= m_scrollableAreaForTouchSequence->scroll(delta < 0 ? ScrollUp : ScrollDown, ScrollByPixel, abs(delta));
    }
    
    // Return false until we manage to scroll at all, and then keep returning true until the gesture ends.
    if (!m_startedScroll) {
        if (!handled)
            return false;
        m_startedScroll = true;
        scrollableArea().didStartScroll();
    } else if (handled)
        scrollableArea().didUpdateScroll();
    
    return true;
}

void ScrollAnimatorIOS::determineScrollableAreaForTouchSequence(const IntSize& scrollDelta)
{
    ASSERT(!m_scrollableAreaForTouchSequence);

    ScrollableArea* scrollableArea = &m_scrollableArea;
    while (true) {
        if (!scrollableArea->isPinnedInBothDirections(scrollDelta))
            break;

        ScrollableArea* enclosingArea = scrollableArea->enclosingScrollableArea();
        if (!enclosingArea)
            break;

        scrollableArea = enclosingArea;
    }

    ASSERT(scrollableArea);
    m_scrollableAreaForTouchSequence = scrollableArea;
}
#endif

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
