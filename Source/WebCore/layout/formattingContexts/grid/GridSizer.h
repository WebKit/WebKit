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
#include "GridTypeAliases.h"
#include "UsedTrackSizes.h"

namespace WebCore {
namespace Layout {

struct GridLayoutState;

class GridSizer {
public:
    GridSizer(const GridFormattingContext&, const GridLayoutState&);

    // Runs step 1 of the grid sizing algorithm alone. Steps 2-4 cannot change the column sizes, so
    // a caller that only needs those can stop here and skip row sizing, grid-item layout, and
    // alignment.
    TrackSizes sizeColumnsOnlyFastPath(const PlacedGridItems&, const TrackSizingFunctionsList& columnTrackSizingFunctions, const TrackSizingFunctionsList& rowTrackSizingFunctions) const;

    // Runs the whole grid sizing algorithm.
    UsedTrackSizes sizeGrid(const PlacedGridItems&, const TrackSizingFunctionsList& columnTrackSizingFunctions, const TrackSizingFunctionsList& rowTrackSizingFunctions) const;

private:
    TrackSizes sizeColumnTracks(const PlacedGridItems&, const TrackSizingFunctionsList& columnTrackSizingFunctions, const TrackSizingFunctionsList& rowTrackSizingFunctions) const;
    TrackSizes sizeRowTracks(const PlacedGridItems&, const TrackSizes& columnSizes, const TrackSizingFunctionsList& rowTrackSizingFunctions) const;

    const GridFormattingContext& formattingContext() const LIFETIME_BOUND { return m_gridFormattingContext; }
    const GridLayoutState& layoutState() const LIFETIME_BOUND { return m_gridLayoutState; }

    const GridFormattingContext& m_gridFormattingContext;
    const GridLayoutState& m_gridLayoutState;
};

} // namespace Layout
} // namespace WebCore
