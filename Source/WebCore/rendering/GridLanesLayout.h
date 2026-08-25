/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#include "GridArea.h"
#include "GridTrackSizingAlgorithm.h"
#include "LayoutUnit.h"
#include "RenderBox.h"
#include <wtf/CheckedRef.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

namespace Style {
enum class GridTrackSizingDirection : bool;
}

class RenderGrid;

class GridLanesLayout {
public:
    // Construction repopulates the grid, so it has to happen immediately before placement.
    GridLanesLayout(RenderGrid&, unsigned gridAxisTracksCount, Style::GridTrackSizingDirection stackingAxisDirection);

    enum class Phase : uint8_t {
        Layout,
        MinContent,
        MaxContent
    };

    using ResolvedFitTolerance = Variant<LayoutUnit, CSS::Keyword::Infinite>;

    void performGridLanesPlacement(const GridTrackSizingAlgorithm&, ResolvedFitTolerance, Phase);
    LayoutUnit NODELETE offsetForGridItem(const RenderBox&) const;
    LayoutUnit gridContentSize() const { return m_gridContentSize; };

private:
    GridArea gridAreaForIndefiniteGridAxisItem(const RenderBox& item, ResolvedFitTolerance);
    GridArea gridAreaForDefiniteGridAxisItem(const RenderBox&) const;

    void placeGridLanesItems(const GridTrackSizingAlgorithm&, ResolvedFitTolerance, Phase);
    void setItemContainingBlockToGridArea(const GridTrackSizingAlgorithm&, RenderBox&);
    void insertIntoGridAndLayoutItem(const GridTrackSizingAlgorithm&, RenderBox&, const GridArea&, Phase);
    LayoutUnit calculateGridLanesIntrinsicLogicalWidth(RenderBox&, Phase);

    LayoutUnit stackingAxisMarginBoxForItem(const RenderBox& gridItem);
    void updateRunningPositions(const RenderBox& gridItem, const GridArea&);
    void updateItemOffset(const RenderBox& gridItem, LayoutUnit offset);
    LayoutUnit maxRunningPositionForSpan(unsigned startLine, unsigned spanLength) const;
    inline Style::GridTrackSizingDirection NODELETE gridAxisDirection() const;

    bool hasDefiniteGridAxisPosition(const RenderBox& gridItem, Style::GridTrackSizingDirection gridAxisDirection) const;
    GridArea NODELETE gridAreaFromGridAxisSpan(const GridSpan&) const;
    GridSpan NODELETE gridAxisSpanFromArea(const GridArea&) const;

    const unsigned m_gridAxisTracksCount;

    Vector<LayoutUnit> m_runningPositions;
    HashMap<SingleThreadWeakRef<const RenderBox>, LayoutUnit> m_itemOffsets;
    const CheckedRef<RenderGrid> m_renderGrid;
    const LayoutUnit m_stackingAxisGridGap;
    LayoutUnit m_gridContentSize;

    const Style::GridTrackSizingDirection m_stackingAxisDirection;
    const GridSpan m_stackingAxisSpan = GridSpan::stackingAxisTranslatedDefiniteGridSpan();

    unsigned m_autoFlowNextCursor { 0 };
};

} // end namespace WebCore
