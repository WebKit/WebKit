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
#include "ScrollController.h"

#include "LayoutSize.h"
#include "Logging.h"
#include "PlatformWheelEvent.h"
#include "WheelEventTestMonitor.h"
#include <wtf/text/TextStream.h>

#if ENABLE(CSS_SCROLL_SNAP)
#include "ScrollSnapAnimatorState.h"
#include "ScrollableArea.h"
#endif

#if ENABLE(RUBBER_BANDING) || ENABLE(CSS_SCROLL_SNAP)

namespace WebCore {

ScrollController::ScrollController(ScrollControllerClient& client)
    : m_client(client)
{
}

void ScrollController::animationCallback(MonotonicTime currentTime)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollController " << this << " animationCallback: isAnimatingRubberBand " << m_isAnimatingRubberBand << " isAnimatingScrollSnap " << m_isAnimatingScrollSnap);

    updateScrollSnapAnimatingState(currentTime);
    updateRubberBandAnimatingState(currentTime);
}

void ScrollController::startOrStopAnimationCallbacks()
{
    bool needsCallbacks = m_isAnimatingRubberBand || m_isAnimatingScrollSnap;
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

void ScrollController::setIsAnimatingRubberBand(bool isAnimatingRubberBand)
{
    if (isAnimatingRubberBand == m_isAnimatingRubberBand)
        return;
        
    m_isAnimatingRubberBand = isAnimatingRubberBand;
    startOrStopAnimationCallbacks();
}

void ScrollController::setIsAnimatingScrollSnap(bool isAnimatingScrollSnap)
{
    if (isAnimatingScrollSnap == m_isAnimatingScrollSnap)
        return;
        
    m_isAnimatingScrollSnap = isAnimatingScrollSnap;
    startOrStopAnimationCallbacks();
}

bool ScrollController::usesScrollSnap() const
{
#if ENABLE(CSS_SCROLL_SNAP)
    return !!m_scrollSnapState;
#else
    return false;
#endif
}

#if ENABLE(CSS_SCROLL_SNAP)

void ScrollController::setSnapOffsetsInfo(const LayoutScrollSnapOffsetsInfo& snapOffsetInfo)
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

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollController " << this << " setSnapOffsetsInfo new state: " << ValueOrNull(m_scrollSnapState.get()));
}

const LayoutScrollSnapOffsetsInfo* ScrollController::snapOffsetsInfo() const
{
    return m_scrollSnapState ? &m_scrollSnapState->snapOffsetInfo() : nullptr;
}

unsigned ScrollController::activeScrollSnapIndexForAxis(ScrollEventAxis axis) const
{
    if (!usesScrollSnap())
        return invalidSnapOffsetIndex;

    return m_scrollSnapState->activeSnapIndexForAxis(axis);
}

void ScrollController::setActiveScrollSnapIndexForAxis(ScrollEventAxis axis, unsigned index)
{
    if (!usesScrollSnap())
        return;

    m_scrollSnapState->setActiveSnapIndexForAxis(axis, index);
}

void ScrollController::setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis axis, int offset)
{
    if (!usesScrollSnap())
        return;

    float scaleFactor = m_client.pageScaleFactor();
    ScrollSnapAnimatorState& snapState = *m_scrollSnapState;

    auto snapOffsets = snapState.snapOffsetsForAxis(axis);
    if (!snapOffsets.size())
        return;

    LayoutSize viewportSize(m_client.viewportSize().width(), m_client.viewportSize().height());
    unsigned activeIndex = snapState.snapOffsetInfo().closestSnapOffset(axis, viewportSize, LayoutUnit(offset / scaleFactor), 0).second;
    if (activeIndex == activeScrollSnapIndexForAxis(axis))
        return;

    m_activeScrollSnapIndexDidChange = true;
    setActiveScrollSnapIndexForAxis(axis, activeIndex);
}

float ScrollController::adjustScrollDestination(ScrollEventAxis axis, float destinationOffset, float velocity, std::optional<float> originalOffset)
{
    if (!usesScrollSnap())
        return destinationOffset;

    ScrollSnapAnimatorState& snapState = *m_scrollSnapState;
    auto snapOffsets = snapState.snapOffsetsForAxis(axis);
    if (!snapOffsets.size())
        return destinationOffset;

    std::optional<LayoutUnit> originalOffsetInLayoutUnits;
    if (originalOffset)
        originalOffsetInLayoutUnits = LayoutUnit(*originalOffset / m_client.pageScaleFactor());
    LayoutSize viewportSize(m_client.viewportSize().width(), m_client.viewportSize().height());
    LayoutUnit offset = snapState.snapOffsetInfo().closestSnapOffset(axis, viewportSize, LayoutUnit(destinationOffset / m_client.pageScaleFactor()), velocity, originalOffsetInLayoutUnits).first;
    return offset * m_client.pageScaleFactor();
}


void ScrollController::updateActiveScrollSnapIndexForClientOffset()
{
    if (!usesScrollSnap())
        return;

    ScrollOffset offset = roundedIntPoint(m_client.scrollOffset());
    setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis::Horizontal, offset.x());
    setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis::Vertical, offset.y());
}

void ScrollController::resnapAfterLayout()
{
    if (!usesScrollSnap())
        return;

    // If we are already snapped in a particular axis, maintain that. Otherwise, snap to the nearest eligible snap point.
    ScrollOffset offset = roundedIntPoint(m_client.scrollOffset());
    size_t activeHorizontalIndex = m_scrollSnapState->activeSnapIndexForAxis(ScrollEventAxis::Horizontal);
    if (activeHorizontalIndex == invalidSnapOffsetIndex)
        setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis::Horizontal, offset.x());
    size_t activeVerticalIndex = m_scrollSnapState->activeSnapIndexForAxis(ScrollEventAxis::Vertical);
    if (activeVerticalIndex == invalidSnapOffsetIndex)
        setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis::Vertical, offset.y());

}
#endif

// Currently, only Mac supports momentum srolling-based scrollsnapping and rubber banding
// so all of these methods are a noop on non-Mac platforms.
#if !PLATFORM(MAC)
ScrollController::~ScrollController()
{
}

void ScrollController::stopAllTimers()
{
}

void ScrollController::scrollPositionChanged()
{
}

void ScrollController::updateScrollSnapAnimatingState(MonotonicTime)
{

}

void ScrollController::updateRubberBandAnimatingState(MonotonicTime)
{

}

#endif // PLATFORM(MAC)

} // namespace WebCore

#endif // ENABLE(RUBBER_BANDING) || ENABLE(CSS_SCROLL_SNAP)
