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
#include "GridItemPlacer.h"
#include "GridTypeAliases.h"
#include "PlacedGridItem.h"
#include "StyleContentAlignmentData.h"
#include "StyleGridTrackSize.h"
#include "StyleGridTrackSizes.h"
#include "TrackSizingFunctions.h"
#include <wtf/BitVector.h>

namespace WebCore {
namespace Layout {

// Everything that precedes track sizing in https://drafts.csswg.org/css-grid-1/#layout-algorithm: the grid definition,
// the placement of the grid items and the inputs to the track sizing algorithm. It is built once per layout() or
// computeIntrinsicWidths() call and does not change afterwards. It is the same for both intrinsic sizing scenarios,
// so the intrinsic width computation builds one and sizes it under both. The size constraints
// are not part of it: each sizing pass gets its own.
struct GridLayoutContext {
    const LogicalGridItems logicalGridItems;
    const GridDefinition gridDefinition;
    const LeadingImplicitTracks leadingImplicitTracks;
    const GridItemPlacementResult placementResult;
    const PlacedGridItems placedGridItems;
    const LayoutUnit usedColumnGap;
    const LayoutUnit usedRowGap;
    const StyleContentAlignmentData usedJustifyContent;
    const StyleContentAlignmentData usedAlignContent;
    const BitVector collapsedColumnTracksList;
    const BitVector collapsedRowTracksList;
    const TrackSizingFunctionsList columnTrackSizingFunctions;
    const TrackSizingFunctionsList rowTrackSizingFunctions;

    // The row track sizes as specified, before their percentages were treated as auto. Only set when the block axis'
    // percentages were treated as auto and the input asked to keep them. layout() sizes the rows a second time against the resulting container size.
    struct SpecifiedRows {
        Vector<Style::GridTrackSize> trackSizes;
        Style::GridTrackSizes autoRows;
        bool hasPercentages { false };
    };
    const std::optional<SpecifiedRows> specifiedRows;

    // Only needed when the rows are sized a second time, so it is built on demand.
    TrackSizingFunctionsList rowTrackSizingFunctionsAsSpecified() const;
};

} // namespace Layout
} // namespace WebCore
