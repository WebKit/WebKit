/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "ScrollLatchingController.h"

#if ENABLE(WHEEL_EVENT_LATCHING)

#include "Element.h"
#include "PlatformWheelEvent.h"
#include "ScrollableArea.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

// See also ScrollTreeLatchingController.cpp
static const Seconds resetLatchedStateTimeout { 100_ms };

ScrollLatchingController::ScrollLatchingController()
    : m_clearLatchingStateTimer(*this, &ScrollLatchingController::clearTimerFired)
{
}

ScrollLatchingController::~ScrollLatchingController() = default;

void ScrollLatchingController::clear()
{
    LOG_WITH_STREAM(ScrollLatching, stream << "ScrollLatchingController::clear()");
    m_cumulativeEventDelta = { };
    m_frameStateStack.clear();
}

// FIXME: This logic is different from ScrollingTreeLatchingController, which simply lets the latching state elapse after 100ms.
void ScrollLatchingController::clearOrScheduleClearIfNeeded(const PlatformWheelEvent& wheelEvent)
{
    if (wheelEvent.shouldResetLatching() || wheelEvent.isNonGestureEvent()) {
        clear();
        LOG_WITH_STREAM(ScrollLatching, stream << "ScrollLatchingController::clearOrScheduleClearingLatchedStateIfNeeded() - event" << wheelEvent << ", resetting latching");
        return;
    }

    if (m_clearLatchingStateTimer.isActive()) {
        // If another wheel event scrolling starts, stop the timer manually, and reset the latched state immediately.
        if (wheelEvent.isGestureStart()) {
            LOG_WITH_STREAM(ScrollLatching, stream << "ScrollLatchingController::clearOrScheduleClearingLatchedStateIfNeeded() - event" << wheelEvent << ", timer pending, another scroll starting");
            clear();
            m_clearLatchingStateTimer.stop();
        } else if (wheelEvent.isTransitioningToMomentumScroll()) {
            // Wheel events machinery is transitioning to momentum scrolling, so no need to reset latched state. Stop the timer.
            m_clearLatchingStateTimer.stop();
        }
        return;
    }

    if (wheelEvent.isEndOfNonMomentumScroll()) {
        LOG_WITH_STREAM(ScrollLatching, stream << "ScrollLatchingController::clearOrScheduleClearingLatchedStateIfNeeded() - event" << wheelEvent << ", scheduling clear timer");
        m_clearLatchingStateTimer.startOneShot(resetLatchedStateTimeout);
    }
}

void ScrollLatchingController::clearTimerFired()
{
    LOG_WITH_STREAM(ScrollLatching, stream << "ScrollLatchingController::clearTimerFired() - clearing state");
    clear();
}

void ScrollLatchingController::receivedWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    clearOrScheduleClearIfNeeded(wheelEvent);

    if (wheelEvent.isGestureStart() || wheelEvent.isNonGestureEvent())
        m_cumulativeEventDelta = wheelEvent.delta();
    else
        m_cumulativeEventDelta += wheelEvent.delta();
}

bool ScrollLatchingController::latchingAllowsScrollingInFrame(const Frame& frame, WeakPtr<ScrollableArea>& latchedScroller) const
{
    if (m_frameStateStack.isEmpty())
        return true;

    if (auto* frameState = stateForFrame(frame)) {
        latchedScroller = frameState->scrollableArea;
        return !!frameState->scrollableArea;
    }
    return false;
}

void ScrollLatchingController::updateAndFetchLatchingStateForFrame(Frame& frame, const PlatformWheelEvent& wheelEvent, RefPtr<Element>& latchedElement, WeakPtr<ScrollableArea>& scrollableArea, bool& isOverWidget)
{
    if (wheelEvent.isGestureStart()) {
        // We can have existing state here because state is cleared on a timer.
        if (!hasStateForFrame(frame)) {
            FrameState state;
            state.frame = &frame;
            state.wheelEventElement = makeWeakPtr(latchedElement.get());
            if (shouldLatchToScrollableArea(frame, scrollableArea.get(), m_cumulativeEventDelta))
                state.scrollableArea = scrollableArea;
            state.isOverWidget = isOverWidget;

            m_frameStateStack.append(WTFMove(state));
            return;
        }
    }

    if (wheelEvent.isGestureContinuation()) {
        auto* state = stateForFrame(frame);
        if (!state)
            return;

        // We may not have latched at gesture start because of small deltas. Re-evaluate latching based on accumulated delta.
        if (!state->scrollableArea && shouldLatchToScrollableArea(frame, scrollableArea.get(), m_cumulativeEventDelta))
            state->scrollableArea = scrollableArea;
    }

    if (!wheelEvent.useLatchedEventElement())
        return;

    for (const auto& state : m_frameStateStack) {
        if (state.frame == &frame) {
            latchedElement = state.wheelEventElement.get();
            scrollableArea = state.scrollableArea;
            isOverWidget = state.isOverWidget;
        }
    }
}

void ScrollLatchingController::removeLatchingStateForTarget(const Element& element)
{
    if (m_frameStateStack.isEmpty())
        return;

    auto findResult = m_frameStateStack.findMatching([&element] (const auto& state) {
        auto* wheelElement = state.wheelEventElement.get();
        return wheelElement && element.isEqualNode(wheelElement);
    });
    
    // If this element is found in the latching stack, just clear the whole stack. We can't just remove one entry,
    // since the stack has to match the frame hierarchy.
    if (findResult != notFound)
        m_frameStateStack.clear();
}

void ScrollLatchingController::removeLatchingStateForFrame(const Frame& frame)
{
    if (m_frameStateStack.isEmpty())
        return;

    // If the frame was in the latching stack, just clear state.
    if (auto* frameState = stateForFrame(frame))
        clear();
}

static bool deltaIsPredominantlyVertical(FloatSize delta)
{
    return std::abs(delta.height()) > std::abs(delta.width());
}

bool ScrollLatchingController::shouldLatchToScrollableArea(const Frame& frame, ScrollableArea* scrollableArea, FloatSize scrollDelta) const
{
    if (!scrollableArea)
        return false;

    // We always allow the main frame to receive wheel events to permit rubber-banding.
    if (frame.isMainFrame() && scrollableArea == frame.view())
        return true;

    if (!scrollableArea->canHaveScrollbars())
        return false;

    if (scrollDelta.isZero())
        return false;

    if (!deltaIsPredominantlyVertical(scrollDelta) && scrollDelta.width()) {
        if (!scrollableArea->horizontalScrollbar())
            return false;

        if (scrollDelta.width() < 0)
            return !scrollableArea->scrolledToRight();
        
        return !scrollableArea->scrolledToLeft();
    }

    if (!scrollableArea->verticalScrollbar())
        return false;

    if (scrollDelta.height() < 0)
        return !scrollableArea->scrolledToBottom();

    return !scrollableArea->scrolledToTop();
}

bool ScrollLatchingController::hasStateForFrame(const Frame& frame) const
{
    for (const auto& state : m_frameStateStack) {
        if (state.frame == &frame)
            return true;
    }
    return false;
}

ScrollLatchingController::FrameState* ScrollLatchingController::stateForFrame(const Frame& frame)
{
    for (auto& state : m_frameStateStack) {
        if (state.frame == &frame)
            return &state;
    }
    return nullptr;
}

const ScrollLatchingController::FrameState* ScrollLatchingController::stateForFrame(const Frame& frame) const
{
    for (const auto& state : m_frameStateStack) {
        if (state.frame == &frame)
            return &state;
    }
    return nullptr;
}

void ScrollLatchingController::dump(WTF::TextStream& ts) const
{
    TextStream multilineStream;
    multilineStream.setIndent(ts.indent() + 2);

    for (const auto& state : m_frameStateStack) {
        TextStream::GroupScope groupScope(multilineStream);
        multilineStream.dumpProperty("frame", ValueOrNull(state.frame));
        multilineStream.dumpProperty("element", ValueOrNull(state.wheelEventElement.get()));
        multilineStream.dumpProperty("scrollable area", ValueOrNull(state.scrollableArea.get()));
        multilineStream.dumpProperty("is over widget", state.isOverWidget);
    }

    ts << "ScrollLatchingController state " << multilineStream.release();
}

TextStream& operator<<(TextStream& ts, const ScrollLatchingController& controller)
{
    controller.dump(ts);
    return ts;
}

} // namespace WebCore

#endif // ENABLE(WHEEL_EVENT_LATCHING)
