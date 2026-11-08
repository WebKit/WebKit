/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#include "GridTypeAliases.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

namespace Layout {

struct GridAutoFlowOptions;
struct UnplacedGridItems;

// https://drafts.csswg.org/css-grid-1/#implicit-grids
class ImplicitGrid {
public:
    // maxTracks is the limit on the number of tracks around the explicit grid (https://drafts.csswg.org/css-grid-1/#overlarge-grids).
    // Builds the implicit grid that the grid item placement algorithm starts from: the explicit grid,
    // grown to cover every definite item placement that falls outside of it and wide enough for the
    // largest span in the non-auto-flow tracks among the items without a definite position there.
    static ImplicitGrid createInitialGrid(const UnplacedGridItems&, size_t leadingNonAutoFlowTracksCount, size_t leadingAutoFlowTracksCount, size_t explicitNonAutoFlowTracksCount, size_t explicitAutoFlowTracksCount, size_t maxTracks);

    ImplicitGrid(size_t totalNonAutoFlowTracksCount, size_t totalAutoFlowTracksCount, size_t maxNonAutoFlowTracksCount, size_t maxAutoFlowTracksCount);

    size_t autoFlowTracksCount() const { return m_gridMatrix.size(); }
    size_t nonAutoFlowTracksCount() const { return m_nonAutoFlowTracksCount; }

    FlowRelativeAreaLines insertUnplacedGridItem(const UnplacedGridItem&);
    FlowRelativeAreaLines insertDefiniteAutoFlowItem(const UnplacedGridItem&, GridAutoFlowOptions);
    void determineImplicitGridNonAutoFlowTracks(const Vector<UnplacedGridItem>&);
    FlowRelativeAreaLines insertAutoPositionedItem(const UnplacedGridItem&, GridAutoFlowOptions);

private:
    using AutoFlowTrackCursors = HashMap<size_t, size_t, WTF::DefaultHash<size_t>, WTF::UnsignedWithZeroKeyHashTraits<size_t>>;
    NODELETE std::optional<size_t> findFirstAvailableNonAutoFlowPosition(WTF::Range<size_t> autoFlowRange, size_t nonAutoFlowSpan, size_t startSearchPosition) const;
    std::optional<size_t> findNonAutoFlowPositionForDefiniteAutoFlowItem(WTF::Range<size_t> autoFlowRange, size_t nonAutoFlowSpan, GridAutoFlowOptions) const;
    size_t growNonAutoFlowTracksForDefiniteAutoFlowItem(size_t nonAutoFlowSpan, WTF::Range<size_t> autoFlowRange);
    NODELETE bool isCellRangeEmpty(WTF::Range<size_t> nonAutoFlowRange, WTF::Range<size_t> autoFlowRange) const;
    NODELETE FlowRelativeAreaLines markAreaAsOccupied(WTF::Range<size_t> nonAutoFlowRange, WTF::Range<size_t> autoFlowRange);

    size_t nonAutoFlowSpanLimitedToGrid(const UnplacedGridItem&) const;

    // Helper functions for auto-positioned items
    void growNonAutoFlowTracksToFit(size_t requiredCount);
    void growAutoFlowTracksToFit(size_t requiredAutoFlowIndex);
    FlowRelativeAreaLines placeAutoPositionedItemWithDefiniteNonAutoFlowPosition(const UnplacedGridItem&, GridAutoFlowOptions);
    FlowRelativeAreaLines placeAutoPositionedItemWithAutoPositionInBothAxes(const UnplacedGridItem&, GridAutoFlowOptions);

    GridMatrix m_gridMatrix;

    // The number of cells in every auto-flow track. BitVector::size() reports the inline bit capacity rather than the
    // number of bits asked for, so the grid has to remember how many it actually has.
    size_t m_nonAutoFlowTracksCount { 0 };

    // https://drafts.csswg.org/css-grid-1/#overlarge-grids
    // The most non-auto-flow and auto-flow tracks the grid may have: the leading implicit tracks plus the limit on the tracks after the explicit grid start.
    // Growing stops here, and an item that would go past it gets a clamped grid area.
    size_t m_maxNonAutoFlowTracksCount { 0 };
    size_t m_maxAutoFlowTracksCount { 0 };

    // Per-auto-flow-track cursors for sparse packing in Step 2 (items with a definite auto-flow position only).
    AutoFlowTrackCursors m_autoFlowTrackCursors;

    // Global cursor for Step 4 (auto-positioned items with both axes automatic).
    // Tracks the current insertion point as (auto-flow position, non-auto-flow position) to ensure monotonic placement.
    size_t m_autoPlacementCursorAutoFlowPosition { 0 };
    size_t m_autoPlacementCursorNonAutoFlowPosition { 0 };
};

} // namespace Layout

} // namespace WebCore
