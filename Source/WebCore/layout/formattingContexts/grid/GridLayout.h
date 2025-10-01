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
};

namespace Layout {

class ImplicitGrid;

enum class PackingStrategy : bool {
    Sparse,
    Dense
};

enum class GridAutoFlowDirection : bool {
    Row,
    Column
};

struct GridAutoFlowOptions {
    PackingStrategy strategy;
    GridAutoFlowDirection direction;
};

struct UsedTrackSizes;
struct UsedMargins;

class GridLayout {
public:
    GridLayout(const GridFormattingContext&);

    std::pair<UsedTrackSizes, GridItemRects> layout(GridFormattingContext::GridLayoutConstraints, const UnplacedGridItems&);

private:

    auto placeGridItems(const UnplacedGridItems&, const Vector<Style::GridTrackSize>& gridTemplateColumnsTrackSizes,
        const Vector<Style::GridTrackSize>& gridTemplateRowsTrackSizes, GridAutoFlowOptions);
    static TrackSizingFunctionsList trackSizingFunctions(size_t implicitGridTracksCount, const Vector<Style::GridTrackSize> gridTemplateTrackSizes);

    static UsedTrackSizes performGridSizingAlgorithm(const PlacedGridItems&, const TrackSizingFunctionsList& columnTrackSizingFunctionsList, const TrackSizingFunctionsList& rowTrackSizingFunctionsList);

    using UsedInlineSizes = Vector<LayoutUnit>;
    using UsedBlockSizes = Vector<LayoutUnit>;
    std::pair<UsedInlineSizes, UsedBlockSizes> layoutGridItems(const PlacedGridItems&, const UsedTrackSizes&) const;

    static Vector<UsedMargins> computeInlineMargins(const PlacedGridItems&);
    static Vector<UsedMargins> computeBlockMargins(const PlacedGridItems&);

    using BorderBoxPositions = Vector<LayoutUnit>;
    static BorderBoxPositions performInlineAxisSelfAlignment(const PlacedGridItems&, const Vector<UsedMargins>&);
    static BorderBoxPositions performBlockAxisSelfAlignment(const PlacedGridItems&, const Vector<UsedMargins>&);

    const GridFormattingContext& formattingContext() const { return m_gridFormattingContext.get(); }

    const ElementBox& gridContainer() const;
    const RenderStyle& gridContainerStyle() const;

    const CheckedRef<const GridFormattingContext> m_gridFormattingContext;
};

}
}
