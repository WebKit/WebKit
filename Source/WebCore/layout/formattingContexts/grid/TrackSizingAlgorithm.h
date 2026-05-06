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
#include "GridTypeAliases.h"
#include "LayoutUnit.h"
#include "PlacedGridItem.h"
#include <wtf/Function.h>
#include <wtf/Range.h>

namespace WebCore {

class StyleContentAlignmentData;

namespace Layout {

class IntegrationUtils;

struct GridItemSizingFunctions {
    GridItemSizingFunctions(Function<LayoutUnit(const PlacedGridItem&, LayoutUnit oppositeAxisConstraint)> minContentContributionFunction, Function<LayoutUnit(const PlacedGridItem&, LayoutUnit oppositeAxisConstraint)> maxContentContributionFunction,
        Function<LayoutUnit(const PlacedGridItem&, const TrackSizingFunctionsList&, LayoutUnit borderAndPadding, LayoutUnit availableSpace)> usedMinimumSizeFunction)
            : minContentContribution(WTF::move(minContentContributionFunction))
            , maxContentContribution(WTF::move(maxContentContributionFunction))
            , usedMinimumSize(WTF::move(usedMinimumSizeFunction))
    {
    }

    Function<LayoutUnit(const PlacedGridItem&, LayoutUnit oppositeAxisConstraint)> minContentContribution;
    Function<LayoutUnit(const PlacedGridItem&, LayoutUnit oppositeAxisConstraint)> maxContentContribution;
    Function<LayoutUnit(const PlacedGridItem&, const TrackSizingFunctionsList&, LayoutUnit borderAndPadding, LayoutUnit availableSpace)> usedMinimumSize;
};

struct TrackSizingItem {
    const PlacedGridItem& gridItem;
    ComputedSizes computedSizes;
    LayoutUnit borderAndPadding;
    WTF::Range<size_t> spannedLines;
    LayoutUnit oppositeAxisConstraint;
};

class TrackSizingAlgorithm {
public:
    static TrackSizes sizeTracks(const TrackSizingItemList&, const TrackSizingFunctionsList&,
        const AxisConstraint&, const GridItemSizingFunctions&,
        LayoutUnit gapSize, const StyleContentAlignmentData& usedContentAlignment);

};

} // namespace Layout
} // namespace WebCore

