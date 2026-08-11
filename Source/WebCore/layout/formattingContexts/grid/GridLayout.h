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

#include "GridFormattingContext.h"
#include "GridTypeAliases.h"
#include "StyleGridTrackBreadth.h"
#include "UsedTrackSizes.h"

namespace WebCore {

namespace Style {
class ComputedStyle;
struct GridTrackSize;
struct ZoomFactor;
}

namespace Layout {

class ImplicitGrid;

struct GridAreaSizes;
struct GridLayoutState;
struct UsedMargins;

struct GridDimensions {
    size_t totalColumns { 0 };
    size_t totalRows { 0 };
};

enum class GridLayoutScope : bool {
    Full, // Run the whole grid sizing algorithm, lay out the grid items, and align them.
    ColumnSizingOnly // Run only the first step of the grid sizing algorithm (size the columns); skip row sizing, grid-item layout, and alignment.
};

class GridLayout {
public:
    GridLayout(const GridFormattingContext&);

    GridLayoutResult layout(UnplacedGridItems&, LeadingImplicitTracks, const GridLayoutState&, GridLayoutScope = GridLayoutScope::Full);

private:

    auto placeGridItems(UnplacedGridItems&, LeadingImplicitTracks, const Vector<Style::GridTrackSize>& gridTemplateColumnsTrackSizes,
        const Vector<Style::GridTrackSize>& gridTemplateRowsTrackSizes, GridAutoFlowOptions);

    static GridDimensions calculateInitialImplicitGridDimensions(const UnplacedGridItems&, LeadingImplicitTracks, size_t explicitColumnsCount, size_t explicitRowsCount);
    ImplicitGrid constructInitialImplicitGrid(const UnplacedGridItems&, LeadingImplicitTracks, size_t explicitColumnsCount, size_t explicitRowsCount);

    static TrackSizingFunctions convertGridTrackSizeToTrackSizingFunctions(const Style::GridTrackSize&, const Style::ZoomFactor&);
    static TrackSizingFunctionsList generateImplicitTrackSizingFunctions(size_t implicitTracksCount, const Style::GridTrackSizes& gridAutoTrackSizes, const Style::ZoomFactor&);
    static TrackSizingFunctionsList trackSizingFunctions(size_t totalTracksCount, size_t leadingImplicitTracksCount, const Vector<Style::GridTrackSize>& gridTemplateTrackSizes, const Style::GridTrackSizes& gridAutoTrackSizes, const Style::ZoomFactor&);

    UsedTrackSizes performGridSizingAlgorithm(const GridLayoutState&, const PlacedGridItems&, const TrackSizingFunctionsList&, const TrackSizingFunctionsList&) const;
    TrackSizes sizeColumnTracks(const PlacedGridItems&, const TrackSizingFunctionsList& columnTrackSizingFunctions, const TrackSizingFunctionsList& rowTrackSizingFunctions, const GridLayoutState&) const;
    TrackSizes sizeRowTracks(const PlacedGridItems&, const TrackSizes& columnSizes, const TrackSizingFunctionsList& rowTrackSizingFunctions, const GridLayoutState&) const;

    std::pair<UsedInlineSizes, UsedBlockSizes> layoutGridItems(const PlacedGridItems&, const GridAreaSizes&,
        const TrackSizingFunctionsList& columnTrackSizingFunctions, const TrackSizingFunctionsList& rowTrackSizingFunctions) const;

    static Vector<UsedMargins> computeInlineMargins(const PlacedGridItems&, const Style::ZoomFactor&);
    static Vector<UsedMargins> computeBlockMargins(const PlacedGridItems&, const Style::ZoomFactor&);

    BorderBoxPositions performInlineAxisSelfAlignment(const PlacedGridItems&, const Vector<UsedMargins>&, const UsedInlineSizes&, const Vector<LayoutUnit>& gridAreasInlineSizeList);
    BorderBoxPositions performBlockAxisSelfAlignment(const PlacedGridItems&, const Vector<UsedMargins>&, const UsedBlockSizes&, const Vector<LayoutUnit>& gridAreasBlockSizeList);

    const GridFormattingContext& formattingContext() const LIFETIME_BOUND { return m_gridFormattingContext; }

    const GridFormattingContext& m_gridFormattingContext;
};

}
}
