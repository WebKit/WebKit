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

bool ScrollController::usesScrollSnap() const
{
#if ENABLE(CSS_SCROLL_SNAP)
    return !!m_scrollSnapState;
#else
    return false;
#endif
}

#if ENABLE(CSS_SCROLL_SNAP)

void ScrollController::updateScrollSnapState(const ScrollableArea& scrollableArea)
{
    const auto* snapOffsetInfo = scrollableArea.snapOffsetInfo();
    if (!snapOffsetInfo) {
        m_scrollSnapState = nullptr;
        return;
    }

    updateScrollSnapPoints(*snapOffsetInfo);
}

void ScrollController::updateScrollSnapPoints(const ScrollSnapOffsetsInfo<LayoutUnit>& snapOffsetInfo)
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
        setActiveScrollSnapIndicesForOffset(roundedIntPoint(m_client.scrollOffset()));

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollController " << this << " updateScrollSnapState new state: " << ValueOrNull(m_scrollSnapState.get()));
}

unsigned ScrollController::activeScrollSnapIndexForAxis(ScrollEventAxis axis) const
{
    if (!usesScrollSnap())
        return 0;

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

    LayoutUnit clampedOffset = std::min(std::max(LayoutUnit(offset / scaleFactor), snapOffsets.first().offset), snapOffsets.last().offset);

    unsigned activeIndex = snapState.snapOffsetInfo().closestSnapOffset(axis, clampedOffset, 0).second;
    if (activeIndex == activeScrollSnapIndexForAxis(axis))
        return;

    m_activeScrollSnapIndexDidChange = true;
    setActiveScrollSnapIndexForAxis(axis, activeIndex);
}

float ScrollController::adjustScrollDestination(ScrollEventAxis axis, float destinationOffset, float velocity, Optional<float> originalOffset)
{
    if (!usesScrollSnap())
        return destinationOffset;

    ScrollSnapAnimatorState& snapState = *m_scrollSnapState;
    auto snapOffsets = snapState.snapOffsetsForAxis(axis);
    if (!snapOffsets.size())
        return destinationOffset;

    Optional<LayoutUnit> originalOffsetInLayoutUnits;
    if (originalOffset.hasValue())
        originalOffsetInLayoutUnits = LayoutUnit(*originalOffset / m_client.pageScaleFactor());
    LayoutUnit offset = snapState.snapOffsetInfo().closestSnapOffset(axis, LayoutUnit(destinationOffset / m_client.pageScaleFactor()), velocity, originalOffsetInLayoutUnits).first;
    return offset * m_client.pageScaleFactor();
}


void ScrollController::setActiveScrollSnapIndicesForOffset(ScrollOffset offset)
{
    if (!usesScrollSnap())
        return;

    setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis::Horizontal, offset.x());
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

#endif // PLATFORM(MAC)

} // namespace WebCore

#endif // ENABLE(RUBBER_BANDING) || ENABLE(CSS_SCROLL_SNAP)
