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

namespace WebCore {
class RenderStyle;

namespace Style {
struct GridTrackSize;
struct ZoomFactor;
};

namespace Layout {

class ImplicitGrid;

struct GridAreaSizes;
struct GridLayoutState;
struct UsedTrackSizes;
struct UsedMargins;

struct GridDimensions {
    size_t rowOffset { 0 };
    size_t columnOffset { 0 };
    size_t totalColumns { 0 };
    size_t totalRows { 0 };
};

class GridLayout {
public:
    GridLayout(const GridFormattingContext&);

    std::pair<UsedTrackSizes, GridItemRects> layout(UnplacedGridItems&, const GridLayoutState&);

private:

    auto placeGridItems(UnplacedGridItems&, const Vector<Style::GridTrackSize>& gridTemplateColumnsTrackSizes,
        const Vector<Style::GridTrackSize>& gridTemplateRowsTrackSizes, GridAutoFlowOptions);

    GridDimensions calculateInitialImplicitGridDimensions(const UnplacedGridItems&, size_t explicitColumnsCount, size_t explicitRowsCount);
    ImplicitGrid constructInitialImplicitGrid(UnplacedGridItems&, size_t explicitColumnsCount, size_t explicitRowsCount);

    static TrackSizingFunctions convertGridTrackSizeToTrackSizingFunctions(const Style::GridTrackSize&);
    static TrackSizingFunctionsList generateImplicitTrackSizingFunctions(size_t explicitTracksCount, size_t totalTracksCount, const Style::GridTrackSizes& gridAutoTrackSizes);
    static TrackSizingFunctionsList trackSizingFunctions(size_t totalTracksCount, const Vector<Style::GridTrackSize>& gridTemplateTrackSizes, const Style::GridTrackSizes& gridAutoTrackSizes);

    UsedTrackSizes performGridSizingAlgorithm(const GridLayoutState&, const PlacedGridItems&, const TrackSizingFunctionsList&, const TrackSizingFunctionsList&) const;

    std::pair<UsedInlineSizes, UsedBlockSizes> layoutGridItems(const PlacedGridItems&, const GridAreaSizes&,
        const TrackSizingFunctionsList& columnTrackSizingFunctions, const TrackSizingFunctionsList& rowTrackSizingFunctions) const;

    static Vector<UsedMargins> computeInlineMargins(const PlacedGridItems&, const Style::ZoomFactor&);
    static Vector<UsedMargins> computeBlockMargins(const PlacedGridItems&, const Style::ZoomFactor&);

    BorderBoxPositions performInlineAxisSelfAlignment(const PlacedGridItems&, const Vector<UsedMargins>&, const UsedInlineSizes&, const Vector<LayoutUnit>& gridAreasInlineSizeList);
    BorderBoxPositions performBlockAxisSelfAlignment(const PlacedGridItems&, const Vector<UsedMargins>&, const UsedBlockSizes&, const Vector<LayoutUnit>& gridAreasBlockSizeList);

    const GridFormattingContext& formattingContext() const { return m_gridFormattingContext; }

    const GridFormattingContext& m_gridFormattingContext;
};

}
}
