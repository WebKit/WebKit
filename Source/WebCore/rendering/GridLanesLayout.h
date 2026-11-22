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

class GridLanesResult {
public:
    GridLanesResult() = default;
    GridLanesResult(HashMap<SingleThreadWeakRef<const RenderBox>, LayoutUnit>&& stackingAxisOffsets, LayoutUnit gridContentSize)
        : m_stackingAxisOffsets(WTF::move(stackingAxisOffsets))
        , m_gridContentSize(gridContentSize)
    {
    }

    NODELETE LayoutUnit stackingAxisOffsetForGridItem(const RenderBox&) const;

    LayoutUnit gridContentSize() const { return m_gridContentSize; }

private:
    // Offset of the item's margin box.
    HashMap<SingleThreadWeakRef<const RenderBox>, LayoutUnit> m_stackingAxisOffsets;
    LayoutUnit m_gridContentSize;
};

class GridLanesLayout {
public:
    // Construction repopulates the grid, so it has to happen immediately before placement.
    GridLanesLayout(RenderGrid&, unsigned gridAxisTracksCount, Style::GridTrackSizingDirection stackingAxisDirection);

    enum class Phase : bool {
        Layout,
        MaxContent // https://github.com/w3c/csswg-drafts/issues/12843
    };

    enum class Packing : bool {
        Sparse,
        Dense
    };

    using ResolvedFitTolerance = Variant<LayoutUnit, CSS::Keyword::Infinite>;

    // The space left in a track above an item placed below the track's running position.
    struct SkippedSpace {
        LayoutUnit start;
        LayoutUnit end;
    };

    GridLanesResult performGridLanesPlacement(const GridTrackSizingAlgorithm&, ResolvedFitTolerance, Phase, Packing);

private:
    GridArea gridAreaForIndefiniteGridAxisItem(const RenderBox& item, ResolvedFitTolerance);
    NODELETE GridArea gridAreaForDefiniteGridAxisItem(const RenderBox&) const;

    struct StackingAxisPlacement {
        LayoutUnit marginBoxStart;
        LayoutUnit marginBoxEnd;
    };

    struct ItemPlacement {
        GridArea area;
        StackingAxisPlacement placement;
    };

    // The skipped spaces of each grid axis track, from highest to lowest.
    using SkippedSpaces = Vector<Vector<SkippedSpace>>;

    GridLanesResult placeGridLanesItems(const GridTrackSizingAlgorithm&, ResolvedFitTolerance, Phase, Packing);
    void setItemContainingBlockToGridArea(const GridTrackSizingAlgorithm&, RenderBox&);
    void layoutGridItem(const GridTrackSizingAlgorithm&, RenderBox&, const GridArea&, Phase);
    LayoutUnit calculateGridLanesIntrinsicLogicalWidth(RenderBox&, Phase);

    LayoutUnit stackingAxisMarginBoxForItem(const RenderBox& gridItem);
    GridSpan clampedGridAxisTracks(const GridArea&) const;
    StackingAxisPlacement updateRunningPositions(const GridArea&, LayoutUnit marginBoxSize);

    ItemPlacement placeItemSparsely(const GridArea&, LayoutUnit marginBoxSize, bool isAutoPlacedInGridAxis);
    ItemPlacement placeItemDensely(SkippedSpaces&, const GridTrackSizingAlgorithm&, const GridArea& normalPlacementArea, LayoutUnit marginBoxSize, bool isAutoPlacedInGridAxis, ResolvedFitTolerance);
    std::optional<ItemPlacement> placeIntoSkippedSpace(SkippedSpaces&, const GridTrackSizingAlgorithm&, const GridArea& normalPlacementArea, LayoutUnit marginBoxSize, bool isAutoPlacedInGridAxis, ResolvedFitTolerance);
    Vector<std::optional<LayoutUnit>> skippedSpacePositions(const SkippedSpaces&, const GridTrackSizingAlgorithm&, const GridSpan& normalPlacementTracks, LayoutUnit spaceNeeded, bool isAutoPlacedInGridAxis) const;
    void recordSkippedSpaces(SkippedSpaces&, const GridSpan&) const;
    NODELETE LayoutUnit maxRunningPositionForSpan(unsigned startLine, unsigned spanLength) const;
    bool tracksAcceptAutoPlacedItem(unsigned startLine, unsigned trackCount) const;
    NODELETE inline Style::GridTrackSizingDirection gridAxisDirection() const;

    unsigned gridAxisTracksCount() const { return static_cast<unsigned>(m_runningPositions.size()); }

    bool hasDefiniteGridAxisPosition(const RenderBox& gridItem, Style::GridTrackSizingDirection gridAxisDirection) const;
    NODELETE GridArea gridAreaFromGridAxisSpan(const GridSpan&) const;
    NODELETE GridSpan gridAxisSpanFromArea(const GridArea&) const;

    Vector<LayoutUnit> m_runningPositions;
    const CheckedRef<RenderGrid> m_renderGrid;
    const LayoutUnit m_stackingAxisGridGap;

    const Style::GridTrackSizingDirection m_stackingAxisDirection;

    unsigned m_autoFlowNextCursor { 0 };
};

} // end namespace WebCore
