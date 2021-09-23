/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
#include "ScrollingEffectsController.h"

#include "KeyboardScrollingAnimator.h"
#include "LayoutSize.h"
#include "Logging.h"
#include "PlatformWheelEvent.h"
#include "ScrollableArea.h"
#include "WheelEventTestMonitor.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

ScrollingEffectsController::ScrollingEffectsController(ScrollingEffectsControllerClient& client)
    : m_client(client)
{
}

void ScrollingEffectsController::animationCallback(MonotonicTime currentTime)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingEffectsController " << this << " animationCallback: isAnimatingRubberBand " << m_isAnimatingRubberBand << " isAnimatingScrollSnap " << m_isAnimatingScrollSnap << "isAnimatingKeyboardScrolling" << m_isAnimatingKeyboardScrolling);

    updateScrollSnapAnimatingState(currentTime);
    updateRubberBandAnimatingState(currentTime);
    updateKeyboardScrollingAnimatingState(currentTime);
}

void ScrollingEffectsController::startOrStopAnimationCallbacks()
{
    bool needsCallbacks = m_isAnimatingRubberBand || m_isAnimatingScrollSnap || m_isAnimatingKeyboardScrolling;
    if (needsCallbacks == m_isRunningAnimatingCallback)
        return;

    if (needsCallbacks) {
        m_client.startAnimationCallback(*this);
        m_isRunningAnimatingCallback = true;
        return;
    }

    m_client.stopAnimationCallback(*this);
    m_isRunningAnimatingCallback = false;
}

void ScrollingEffectsController::beginKeyboardScrolling()
{
    setIsAnimatingKeyboardScrolling(true);
}

void ScrollingEffectsController::stopKeyboardScrolling()
{
    setIsAnimatingKeyboardScrolling(false);
}

void ScrollingEffectsController::setIsAnimatingRubberBand(bool isAnimatingRubberBand)
{
    if (isAnimatingRubberBand == m_isAnimatingRubberBand)
        return;

    m_isAnimatingRubberBand = isAnimatingRubberBand;
    startOrStopAnimationCallbacks();
}

void ScrollingEffectsController::setIsAnimatingScrollSnap(bool isAnimatingScrollSnap)
{
    if (isAnimatingScrollSnap == m_isAnimatingScrollSnap)
        return;

    m_isAnimatingScrollSnap = isAnimatingScrollSnap;
    startOrStopAnimationCallbacks();
}

void ScrollingEffectsController::setIsAnimatingKeyboardScrolling(bool isAnimatingKeyboardScrolling)
{
    if (isAnimatingKeyboardScrolling == m_isAnimatingKeyboardScrolling)
        return;

    m_isAnimatingKeyboardScrolling = isAnimatingKeyboardScrolling;
    startOrStopAnimationCallbacks();
}

bool ScrollingEffectsController::usesScrollSnap() const
{
    return !!m_scrollSnapState;
}

void ScrollingEffectsController::setSnapOffsetsInfo(const LayoutScrollSnapOffsetsInfo& snapOffsetInfo)
{
    if (snapOffsetInfo.isEmpty()) {
        m_scrollSnapState = nullptr;
        return;
    }

    bool shouldComputeCurrentSnapIndices = !m_scrollSnapState;
    if (!m_scrollSnapState)
        m_scrollSnapState = makeUnique<ScrollSnapAnimatorState>();

    m_scrollSnapState->setSnapOffsetInfo(snapOffsetInfo);

    if (shouldComputeCurrentSnapIndices)
        updateActiveScrollSnapIndexForClientOffset();

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollingEffectsController " << this << " setSnapOffsetsInfo new state: " << ValueOrNull(m_scrollSnapState.get()));
}

const LayoutScrollSnapOffsetsInfo* ScrollingEffectsController::snapOffsetsInfo() const
{
    return m_scrollSnapState ? &m_scrollSnapState->snapOffsetInfo() : nullptr;
}

std::optional<unsigned> ScrollingEffectsController::activeScrollSnapIndexForAxis(ScrollEventAxis axis) const
{
    if (!usesScrollSnap())
        return std::nullopt;
    return m_scrollSnapState->activeSnapIndexForAxis(axis);
}

void ScrollingEffectsController::setActiveScrollSnapIndexForAxis(ScrollEventAxis axis, std::optional<unsigned> index)
{
    if (!usesScrollSnap())
        return;

    m_scrollSnapState->setActiveSnapIndexForAxis(axis, index);
}

float ScrollingEffectsController::adjustedScrollDestination(ScrollEventAxis axis, FloatPoint destinationOffset, float velocity, std::optional<float> originalOffset) const
{
    if (!usesScrollSnap())
        return axis == ScrollEventAxis::Horizontal ? destinationOffset.x() : destinationOffset.y();

    return m_scrollSnapState->adjustedScrollDestination(axis, destinationOffset, velocity, originalOffset, m_client.scrollExtents(), m_client.pageScaleFactor());
}

void ScrollingEffectsController::updateActiveScrollSnapIndexForClientOffset()
{
    if (!usesScrollSnap())
        return;

    ScrollOffset offset = roundedIntPoint(m_client.scrollOffset());
    if (m_scrollSnapState->setNearestScrollSnapIndexForOffset(offset, m_client.scrollExtents(), m_client.pageScaleFactor()))
        m_activeScrollSnapIndexDidChange = true;
}

void ScrollingEffectsController::resnapAfterLayout()
{
    if (!usesScrollSnap())
        return;

    // If we are already snapped in a particular axis, maintain that. Otherwise, snap to the nearest eligible snap point.
    ScrollOffset offset = roundedIntPoint(m_client.scrollOffset());
    if (m_scrollSnapState->resnapAfterLayout(offset, m_client.scrollExtents(), m_client.pageScaleFactor()))
        m_activeScrollSnapIndexDidChange = true;
}

void ScrollingEffectsController::updateKeyboardScrollingAnimatingState(MonotonicTime currentTime)
{
    if (!m_isAnimatingKeyboardScrolling)
        return;

    m_client.keyboardScrollingAnimator()->updateKeyboardScrollPosition(currentTime);
}

// Currently, only Mac supports momentum srolling-based scrollsnapping and rubber banding
// so all of these methods are a noop on non-Mac platforms.
#if !PLATFORM(MAC)
ScrollingEffectsController::~ScrollingEffectsController()
{
}

void ScrollingEffectsController::stopAllTimers()
{
}

void ScrollingEffectsController::scrollPositionChanged()
{
}

void ScrollingEffectsController::updateScrollSnapAnimatingState(MonotonicTime)
{

}

void ScrollingEffectsController::updateRubberBandAnimatingState(MonotonicTime)
{

}

#endif // PLATFORM(MAC)

} // namespace WebCore
