/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "GridLayoutConstraints.h"
#include "GridTypeAliases.h"
#include "UsedTrackSizes.h"

namespace WebCore {
namespace Layout {

struct GridLayoutContext;

class GridSizer {
public:
    GridSizer(const GridFormattingContext&, const GridLayoutContext&, const GridLayoutConstraints&);

    // Runs step 1 of the grid sizing algorithm alone. When no grid item's inline-axis contributions
    // depend on the available space in the block axis, steps 2-4 cannot change the column sizes, so
    // a caller that only needs those can stop here and skip row sizing, grid-item layout, and
    // alignment.
    TrackSizes sizeColumnsOnlyFastPath(const PlacedGridItems&, const TrackSizingFunctionsList& columnTrackSizingFunctions, const TrackSizingFunctionsList& rowTrackSizingFunctions) const;

    // Runs the whole grid sizing algorithm.
    UsedTrackSizes sizeGrid(const PlacedGridItems&, const TrackSizingFunctionsList& columnTrackSizingFunctions, const TrackSizingFunctionsList& rowTrackSizingFunctions) const;

private:
    Vector<std::optional<LayoutUnit>> rowSizesForFirstIterationColumnSizing(const TrackSizingFunctionsList& rowTrackSizingFunctions) const;
    TrackSizes sizeColumnTracks(const PlacedGridItems&, const Vector<std::optional<LayoutUnit>>& rowSizes, const TrackSizingFunctionsList& columnTrackSizingFunctions, const TrackSizingFunctionsList& rowTrackSizingFunctions) const;
    TrackSizes sizeRowTracks(const PlacedGridItems&, const TrackSizes& columnSizes, const TrackSizingFunctionsList& columnTrackSizingFunctions, const TrackSizingFunctionsList& rowTrackSizingFunctions) const;

    const GridFormattingContext& formattingContext() const LIFETIME_BOUND { return m_gridFormattingContext; }
    const GridLayoutContext& layoutContext() const LIFETIME_BOUND { return m_gridLayoutContext; }
    const GridLayoutConstraints& layoutConstraints() const LIFETIME_BOUND { return m_layoutConstraints; }

    const GridFormattingContext& m_gridFormattingContext;
    const GridLayoutContext& m_gridLayoutContext;
    const GridLayoutConstraints m_layoutConstraints;
};

} // namespace Layout
} // namespace WebCore
