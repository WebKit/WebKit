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

#include "AxisConstraint.h"
#include "ExtraSpaceDistributor.h"
#include "GridItemSizingFunctions.h"
#include "GridLayoutUtils.h"
#include "GridTypeAliases.h"
#include "LayoutUnit.h"
#include "PlacedGridItem.h"
#include "TrackSizingFunctions.h"
#include <wtf/Function.h>
#include <wtf/Range.h>

namespace WebCore {

class StyleContentAlignmentData;

namespace Layout {

class IntegrationUtils;

struct TrackSizingItem {
    const PlacedGridItem& gridItem;
    const ComputedSizes computedSizes;
    const LayoutUnit borderAndPadding;
    const UsedMargins usedMargins;
    const WTF::Range<size_t> spannedLines;
    const std::optional<LayoutUnit> oppositeAxisConstraint;
};

struct UnsizedTrack {
    LayoutUnit baseSize;
    LayoutUnit growthLimit;
    const TrackSizingFunctions trackSizingFunction;
    // https://drafts.csswg.org/css-grid-1/#extra-space
    // trackSizingFunction's fit-content() argument, resolved against the available grid space by
    // TrackSizingFunctions::fitContentLimit() in initializeTrackSizes(). nullopt for a track without
    // a fit-content() maximum.
    const std::optional<LayoutUnit> fitContentLimit;
    // https://drafts.csswg.org/css-grid-1/#infinitely-growable
    bool infinitelyGrowable { false };

    // https://drafts.csswg.org/css-grid-1/#extra-space
    // This track's affected size: its base size when affecting base sizes, its growth limit when
    // affecting growth limits. Per the spec: "For infinite growth limits, substitute the track's base
    // size."
    LayoutUnit affectedSize(ExtraSpaceDistributionTarget spaceDistributionTarget) const
    {
        if (spaceDistributionTarget == ExtraSpaceDistributionTarget::BaseSizes)
            return baseSize;
        // Growth limits: substitute the base size for an infinite growth limit.
        return growthLimit == LayoutUnit::max() ? baseSize : growthLimit;
    }

    // https://drafts.csswg.org/css-grid-1/#extra-space
    // The limit at which this track's item-incurred increase freezes.
    LayoutUnit freezeLimit(ExtraSpaceDistributionTarget spaceDistributionTarget, SpaceDistributionLimit spaceDistributionLimit) const
    {
        switch (spaceDistributionLimit) {
        case SpaceDistributionLimit::UpToGrowthLimit: {
            // 11.5.1.2 Distribute space up to limits
            // "For base sizes, the limit is its growth limit, capped by its fit-content() argument
            // if any."
            if (spaceDistributionTarget == ExtraSpaceDistributionTarget::BaseSizes) {
                if (fitContentLimit)
                    return std::min(growthLimit, *fitContentLimit);
                return growthLimit;
            }

            // "For growth limits, the limit is the growth limit if the growth limit is finite and
            // the track is not infinitely growable,"
            if (growthLimit != LayoutUnit::max() && !infinitelyGrowable)
                return growthLimit;

            // "...otherwise its fit-content() argument if it has a fit-content() track sizing
            // function, and infinity otherwise."
            return fitContentLimit.value_or(LayoutUnit::max());
        }
        case SpaceDistributionLimit::BeyondGrowthLimit: {
            // 11.5.1.4 Distribute space Beyond limits
            // "For this purpose, the max track sizing function of a fit-content() track is treated
            // as max-content until the track reaches the limit specified as the fit-content()
            // argument, after which its max track sizing function is treated as being a fixed
            // sizing function of that argument"
            return fitContentLimit.value_or(LayoutUnit::max());
        }
        }
        ASSERT_NOT_REACHED();
        return LayoutUnit::max();
    }

    // https://drafts.csswg.org/css-grid-1/#algo-init
    // https://drafts.csswg.org/css-grid-1/#algo-single-span-items
    // "In all cases, if a track's growth limit is now less than its base size,
    // increase the growth limit to match the base size."
    void ensureGrowthLimitIsBiggerThanBaseSize()
    {
        growthLimit = std::max(growthLimit, baseSize);
    }
};

class TrackSizingAlgorithm {
public:
    static TrackSizes sizeTracks(const TrackSizingItemList&, const TrackSizingFunctionsList&,
        const AxisConstraint&, const GridItemSizingFunctions&,
        LayoutUnit gapSize, const BitVector& collapsedTracksList, const StyleContentAlignmentData& usedContentAlignment);

};

} // namespace Layout
} // namespace WebCore

