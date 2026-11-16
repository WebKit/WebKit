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

#include "GridTypeAliases.h"
#include "LayoutUnit.h"

namespace WebCore {
namespace Layout {

enum class ExtraSpaceDistributionTarget : bool { BaseSizes, GrowthLimits };

// https://drafts.csswg.org/css-grid-1/#extra-space
// 2.2: Distribute space up to limits: UpToGrowthLimit
// 2.3: Distribute space to non-affected tracks: UpToGrowthLimit
// 2.4: Distribute space beyond limits: BeyondGrowthLimit
enum class SpaceDistributionLimit : bool { UpToGrowthLimit, BeyondGrowthLimit };

// Content and flexible tracks are resolved in separate phases.
enum class ResolveIntrinsicTrackSizesPhase : bool { ContentSizedTracks, FlexibleTracks };

// https://drafts.csswg.org/css-grid-1/#extra-space
enum class AffectedTrackSizingFunction : uint8_t {
    IntrinsicMinimum, // Item 1: "tracks with an intrinsic min track sizing function".
    ContentBasedMinimum, // Item 2: "a min track sizing function of min-content or max-content".
    AutoOrMaxContentMinimum, // Item 3, under a max-content constraint: "a min track sizing function of auto or max-content".
    MaxContentMinimum, // Item 3, in all cases: "a min track sizing function of max-content".
    IntrinsicMaximum, // Item 5: "tracks with intrinsic max track sizing function".
    MaxContentMaximum // Item 6: "tracks with a max track sizing function of max-content".
};

// https://drafts.csswg.org/css-grid-1/#extra-space
class ExtraSpaceDistributor {
public:
    static TrackIndexes distributeExtraSpace(ExtraSpaceDistributionTarget, AffectedTrackSizingFunction,
        const TrackIndexes& affectedTracksIndexes, const Vector<LayoutUnit>& sizeContributions, const GridItemIndexes& accommodatedItemsIndexes,
        const PlacedGridItemSpanList&, UnsizedTracks&, LayoutUnit gapSize, ResolveIntrinsicTrackSizesPhase);
};

} // namespace Layout
} // namespace WebCore
