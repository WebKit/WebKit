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

#include <WebCore/FreeSpaceScenario.h>
#include <WebCore/GridTypeAliases.h>
#include <WebCore/LayoutUnit.h>

namespace WebCore {

namespace Layout {

class IntegrationUtils;

struct GridItemSizingFunctions {
    GridItemSizingFunctions(Function<LayoutUnit(const ElementBox& gridItem, const IntegrationUtils&)> minContentContributionFunction, Function<LayoutUnit(const ElementBox& gridItem, const IntegrationUtils&)> maxContentContributionFunction)
        : minContentContribution(WTF::move(minContentContributionFunction))
        , maxContentContribution(WTF::move(maxContentContributionFunction))
    {
    }

    Function<LayoutUnit(const ElementBox& gridItem, const IntegrationUtils&)> minContentContribution;
    Function<LayoutUnit(const ElementBox& gridItem, const IntegrationUtils&)> maxContentContribution;
};

class TrackSizingAlgorithm {
public:
    static TrackSizes sizeTracks(const PlacedGridItems&, const ComputedSizesList&, const PlacedGridItemSpanList&,
    const TrackSizingFunctionsList&, std::optional<LayoutUnit> availableSpace,
    const GridItemSizingFunctions&, const IntegrationUtils&, const FreeSpaceScenario&, const LayoutUnit& gapSize);

private:

    static UnsizedTracks initializeTrackSizes(const TrackSizingFunctionsList&);

    // Flex track infrastructure
    static FlexTracks collectFlexTracks(const UnsizedTracks&);
    static bool hasFlexTracks(const UnsizedTracks&);
    static double flexFactorSum(const FlexTracks&);
    static LayoutUnit findSizeOfFr(const UnsizedTracks&, const LayoutUnit& availableSpace, const LayoutUnit& gapSize);
};

} // namespace WebCore
} // namespace Layout

