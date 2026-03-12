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

#include "config.h"
#include "TrackSizingAlgorithm.h"

#include "GridLayoutUtils.h"
#include "LayoutIntegrationUtils.h"
#include "NotImplemented.h"
#include "PlacedGridItem.h"
#include "TrackSizingFunctions.h"
#include <wtf/Range.h>
#include <wtf/Vector.h>
#include <wtf/ZippedRange.h>

namespace WebCore {
namespace Layout {

struct FlexTrack {
    size_t trackIndex;
    Style::GridTrackBreadth::Flex flexFactor;
    LayoutUnit baseSize;
    LayoutUnit growthLimit;

    constexpr FlexTrack(size_t index, Style::GridTrackBreadth::Flex factor, LayoutUnit base, LayoutUnit growth)
        : trackIndex(index)
        , flexFactor(factor)
        , baseSize(base)
        , growthLimit(growth)
    {
    }
};

struct UnsizedTrack {
    LayoutUnit baseSize;
    LayoutUnit growthLimit;
    const TrackSizingFunctions trackSizingFunction;
};

using GridItemIndexes = Vector<size_t>;

struct InflexibleTrackState {
    HashSet<size_t> inflexibleTracks;

    bool isFlexible(size_t trackIndex, const UnsizedTrack& track) const
    {
        return track.trackSizingFunction.max.isFlex()
            && !inflexibleTracks.contains(trackIndex);
    }

    void markAsInflexible(size_t trackIndex)
    {
        inflexibleTracks.add(trackIndex);
    }
};

struct FrSizeComponents {
    LayoutUnit baseSizeSum;
    double flexFactorSum;
};

// https://drafts.csswg.org/css-grid-1/#algo-find-fr-size
// Step 1-3: Compute Hypothetical fr Size
static FrSizeComponents computeFRSizeComponents(const UnsizedTracks& tracks, const InflexibleTrackState& state)
{
    // Sum the base sizes of the non-flexible grid tracks.
    LayoutUnit baseSizeSum = 0;
    // Let flex factor sum be the sum of the flex factors of the flexible tracks.
    double flexFactorSum = 0.0;

    for (auto [index, track] : indexedRange(tracks)) {
        if (state.isFlexible(index, track))
            flexFactorSum += track.trackSizingFunction.max.flex().value;
        else
            baseSizeSum += track.baseSize;
    }

    return { baseSizeSum, flexFactorSum };
}

// https://drafts.csswg.org/css-grid-1/#algo-find-fr-size
// Step 4: If the product of the hypothetical fr size and a flexible track’s flex factor is less than
// the track’s base size, restart this algorithm treating all such tracks as inflexible.
static bool isValidFlexFactorUnit(const UnsizedTracks& tracks, LayoutUnit hypotheticalFrSize, InflexibleTrackState& state)
{
    bool hasInvalidTracks = false;
    for (auto [index, track] : indexedRange(tracks)) {
        if (!state.isFlexible(index, track))
            continue;

        auto flexFactor = track.trackSizingFunction.max.flex();
        LayoutUnit computedSize = hypotheticalFrSize * LayoutUnit(flexFactor.value);

        // If the product of the hypothetical fr size and a flexible track's flex factor is less
        // than the track's base size, we should treat this track as inflexible.
        if (computedSize < track.baseSize) {
            hasInvalidTracks = true;
            state.markAsInflexible(index);
        }
    }

    return !hasInvalidTracks;
}

static GridItemIndexes singleSpanningItemsWithinTrack(size_t trackIndex, const PlacedGridItemSpanList& gridItemSpanList)
{
    GridItemIndexes nonSpanningItems;
    for (auto [gridItemIndex, gridItemSpan] : WTF::indexedRange(gridItemSpanList)) {
        if (gridItemSpan.distance() == 1 && gridItemSpan.begin() == trackIndex)
            nonSpanningItems.append(gridItemIndex);
    }
    return nonSpanningItems;
}

using TrackIndexes = Vector<size_t>;
static TrackIndexes tracksWithIntrinsicSizingFunction(const UnsizedTracks& unsizedTracks)
{
    TrackIndexes trackList;
    for (auto [trackIndex, track] : WTF::indexedRange(unsizedTracks)) {
        auto& minimumTrackSizingFunction = track.trackSizingFunction.min;
        auto& maximumTrackSizingFunction = track.trackSizingFunction.max;
        if (minimumTrackSizingFunction.isFlex() || maximumTrackSizingFunction.isFlex())
            continue;

        if (minimumTrackSizingFunction.isContentSized() || maximumTrackSizingFunction.isContentSized())
            trackList.append(trackIndex);
    }
    return trackList;
}

static TrackIndexes tracksWithAutoMaxTrackSizingFunction(const UnsizedTracks& unsizedTracks)
{
    TrackIndexes trackIndexes;
    for (auto [trackIndex, track] : WTF::indexedRange(unsizedTracks)) {
        auto& maxTrackSizingFunction = track.trackSizingFunction.max;
        if (maxTrackSizingFunction.isAuto())
            trackIndexes.append(trackIndex);
    }
    return trackIndexes;
}

static Vector<LayoutUnit> minContentContributions(const PlacedGridItems& gridItems, const GridItemIndexes& gridItemIndexes,
    const TrackSizingGridItemConstraintList& oppositeAxisConstraints, const GridItemSizingFunctions& gridItemSizingFunctions)
{
    return gridItemIndexes.map([&](size_t gridItemIndex) {
        return gridItemSizingFunctions.minContentContribution(gridItems[gridItemIndex], oppositeAxisConstraints[gridItemIndex]);
    });
}

static Vector<LayoutUnit> maxContentContributions(const PlacedGridItems& gridItems, const GridItemIndexes& gridItemIndexes,
    const TrackSizingGridItemConstraintList& oppositeAxisConstraints, const GridItemSizingFunctions& gridItemSizingFunctions)
{
    return gridItemIndexes.map([&](size_t gridItemIndex) {
        return gridItemSizingFunctions.maxContentContribution(gridItems[gridItemIndex], oppositeAxisConstraints[gridItemIndex]);
    });
}

static Vector<LayoutUnit> minimumContributions(const PlacedGridItems& gridItems, const ComputedSizesList& gridItemComputedSizesList, const UsedBorderAndPaddingList& borderAndPaddingList,
    const GridItemIndexes& gridItemIndexes, const TrackSizingGridItemConstraintList& oppositeAxisConstraints, const GridItemSizingFunctions& gridItemSizingFunctions, const TrackSizingFunctionsList& trackSizingFunctions)
{
    // The minimum contribution of an item is the smallest outer size it can have. Specifically,
    return gridItemIndexes.map([&](size_t gridItemIndex) -> LayoutUnit {
        // if the item’s computed preferred size behaves as auto or depends on the size of its
        // containing block in the relevant axis, its minimum contribution is the outer size
        // that would result from assuming the item’s used minimum size as its preferred size.
        auto& preferredSize = gridItemComputedSizesList[gridItemIndex].preferredSize;
        if (GridLayoutUtils::preferredSizeBehavesAsAuto(preferredSize) || GridLayoutUtils::preferredSizeDependsOnContainingBlockSize(preferredSize))
            return gridItemSizingFunctions.usedMinimumSize(gridItems[gridItemIndex], trackSizingFunctions, borderAndPaddingList[gridItemIndex], { });
        // else the item’s minimum contribution is its min-content contribution.
        return gridItemSizingFunctions.minContentContribution(gridItems[gridItemIndex], oppositeAxisConstraints[gridItemIndex]);
    });
}

// https://drafts.csswg.org/css-grid-1/#algo-single-span-items
static void sizeTracksToFitNonSpanningItems(UnsizedTracks& unsizedTracks, const PlacedGridItems& gridItems,
    const ComputedSizesList& gridItemComputedSizesList, const UsedBorderAndPaddingList& borderAndPaddingList, const PlacedGridItemSpanList& gridItemSpanList,
    const TrackSizingGridItemConstraintList& oppositeAxisConstraints, const GridItemSizingFunctions& gridItemSizingFunctions, const TrackSizingFunctionsList& trackSizingFunctionsList)
{
    // For each track with an intrinsic track sizing function and not a flexible sizing function, consider the items in it with a span of 1:
    for (auto trackIndex : tracksWithIntrinsicSizingFunction(unsizedTracks)) {
        auto& track = unsizedTracks[trackIndex];
        auto singleSpanningItemsIndexes = singleSpanningItemsWithinTrack(trackIndex, gridItemSpanList);

        auto& minimumTrackSizingFunction = track.trackSizingFunction.min;
        track.baseSize = WTF::switchOn(minimumTrackSizingFunction,
            [&](const CSS::Keyword::MinContent&) -> LayoutUnit {
                // If the track has a min-content min track sizing function, set its base size
                // to the maximum of the items’ min-content contributions, floored at zero.
                auto itemContributions = minContentContributions(gridItems, singleSpanningItemsIndexes, oppositeAxisConstraints, gridItemSizingFunctions);
                ASSERT(itemContributions.size() == singleSpanningItemsIndexes.size());
                if (itemContributions.isEmpty())
                    return { };
                return std::max({ }, std::ranges::max(itemContributions));
            },
            [&](const CSS::Keyword::MaxContent&) -> LayoutUnit {
                // If the track has a max-content min track sizing function, set its base
                // size to the maximum of the items’ max-content contributions, floored at zero.
                auto itemContributions = maxContentContributions(gridItems, singleSpanningItemsIndexes, oppositeAxisConstraints, gridItemSizingFunctions);
                ASSERT(itemContributions.size() == singleSpanningItemsIndexes.size());
                if (itemContributions.isEmpty())
                    return { };
                return std::max({ }, std::ranges::max(itemContributions));
            },
            [&](const CSS::Keyword::Auto&) -> LayoutUnit {
                auto isBeingSizedUnderMinOrMaxContentConstraint = [] {
                    notImplemented();
                    return false;
                };
                // If the track has an auto min track sizing function and the grid container
                // is being sized under a min-/max-content constraint, set the track’s base
                // size to the maximum of its items’ limited min-content
                // contributions, floored at zero.
                if (isBeingSizedUnderMinOrMaxContentConstraint()) {
                    ASSERT_NOT_IMPLEMENTED_YET();
                    return { };
                }
                // Otherwise, set the track’s base size to the maximum of its items’ minimum
                // contributions, floored at zero.
                auto contributions = minimumContributions(gridItems, gridItemComputedSizesList, borderAndPaddingList, singleSpanningItemsIndexes, oppositeAxisConstraints, gridItemSizingFunctions, trackSizingFunctionsList);
                if (contributions.isEmpty())
                    return { };
                return std::max({ }, std::ranges::max(contributions));
            },
            [&](const auto&) -> LayoutUnit {
                ASSERT_NOT_REACHED();
                return { };
            }
        );

        auto& maximumTrackSizingFunction = track.trackSizingFunction.max;
        track.growthLimit = WTF::switchOn(maximumTrackSizingFunction,
            [&](const CSS::Keyword::MinContent&) -> LayoutUnit {
                // If the track has a min-content max track sizing function, set its growth
                // limit to the maximum of the items’ min-content contributions.
                auto itemContributions = minContentContributions(gridItems, singleSpanningItemsIndexes, oppositeAxisConstraints, gridItemSizingFunctions);
                ASSERT(itemContributions.size() == singleSpanningItemsIndexes.size());
                if (itemContributions.isEmpty())
                    return { };
                return std::ranges::max(itemContributions);
            },
            [&](const CSS::Keyword::MaxContent&) -> LayoutUnit {
                // If the track has a max-content max track sizing function, set its growth
                // limit to the maximum of the items’ max-content contributions.
                auto itemContributions = maxContentContributions(gridItems, singleSpanningItemsIndexes, oppositeAxisConstraints, gridItemSizingFunctions);
                auto maximumMaxContentContribution = itemContributions.isEmpty() ? 0_lu : std::ranges::max(itemContributions);
                return maximumMaxContentContribution;
            },
            [&](const CSS::Keyword::Auto&) -> LayoutUnit {
                // Since it is not explicitly stated otherwise in the spec, auto is treated as max-content:
                // If the track has a max-content max track sizing function, set its growth
                // limit to the maximum of the items’ max-content contributions.
                auto itemContributions = maxContentContributions(gridItems, singleSpanningItemsIndexes, oppositeAxisConstraints, gridItemSizingFunctions);
                auto maximumMaxContentContribution = itemContributions.isEmpty() ? 0_lu : std::ranges::max(itemContributions);
                return maximumMaxContentContribution;
            },
            [&](const auto&) -> LayoutUnit {
                ASSERT_NOT_REACHED();
                return { };
            }
        );
    }
}

// https://drafts.csswg.org/css-grid-1/#algo-content
static void resolveIntrinsicTrackSizes(UnsizedTracks& unsizedTracks, const PlacedGridItems& gridItems,
    const ComputedSizesList& gridItemComputedSizesList, const UsedBorderAndPaddingList& borderAndPaddingList, const PlacedGridItemSpanList& gridItemSpanList,
    const TrackSizingGridItemConstraintList& oppositeAxisConstraints, const GridItemSizingFunctions& gridItemSizingFunctions, const TrackSizingFunctionsList& trackSizingFunctionsList)
{
    // 1. Shim baseline-aligned items so their intrinsic size contributions reflect their
    // baseline alignment.
    auto shimBaselineAlignedItems = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(shimBaselineAlignedItems);

    // 2. Size tracks to fit non-spanning items.
    sizeTracksToFitNonSpanningItems(unsizedTracks, gridItems, gridItemComputedSizesList, borderAndPaddingList,
        gridItemSpanList, oppositeAxisConstraints, gridItemSizingFunctions, trackSizingFunctionsList);

    // 3. Increase sizes to accommodate spanning items crossing content-sized tracks:
    // Next, consider the items with a span of 2 that do not span a track with a flexible
    // sizing function.
    auto increaseSizesToAccommodateSpanningItemsCrossingContentSizedTracks = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(increaseSizesToAccommodateSpanningItemsCrossingContentSizedTracks);

    // 4. Increase sizes to accommodate spanning items crossing flexible tracks:
    auto increaseSizesToAccommodateSpanningItemsCrossingFlexibleTracks = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(increaseSizesToAccommodateSpanningItemsCrossingFlexibleTracks);

    // 5. If any track still has an infinite growth limit, set its growth limit to its base size.
    for (auto& unsizedTrack : unsizedTracks) {
        auto& growthLimit = unsizedTrack.growthLimit;
        if (growthLimit == LayoutUnit::max())
            growthLimit = unsizedTrack.baseSize;
    }
}

// https://drafts.csswg.org/css-grid-1/#algo-terms
// Equal to the available grid space minus the sum of the base sizes of all the grid tracks (including gutters),
// floored at zero. If available grid space is indefinite, the free space is indefinite as well.
static std::optional<LayoutUnit> computeFreeSpace(std::optional<LayoutUnit> availableGridSpace, const UnsizedTracks& unsizedTracks, LayoutUnit gapSize)
{
    if (!availableGridSpace)
        return { };

    auto sumOfBaseSizes = std::accumulate(unsizedTracks.begin(), unsizedTracks.end(), 0_lu, [](LayoutUnit sum, const UnsizedTrack& unsizedTrack) {
        return unsizedTrack.baseSize + sum;
    });
    auto guttersSize = GridLayoutUtils::totalGuttersSize(unsizedTracks.size(), gapSize);

    return std::max({ }, *availableGridSpace - (sumOfBaseSizes + guttersSize));
}

// https://drafts.csswg.org/css-grid-1/#algo-stretch
static void stretchAutoTracks(std::optional<LayoutUnit> freeSpace, UnsizedTracks& unsizedTracks, const StyleContentAlignmentData& usedContentAlignment)
{
    ASSERT(!unsizedTracks.isEmpty());
    if (unsizedTracks.isEmpty())
        return;

    bool hasFreeSpaceToDistribute = freeSpace > 0;
    if (!hasFreeSpaceToDistribute)
        return;

    // When the content-distribution property of the grid container is normal or stretch in this axis...
    if (!usedContentAlignment.isNormal() && usedContentAlignment.distribution() != ContentDistribution::Stretch)
        return;

    // this step expands tracks that have an auto max track sizing function...
    auto tracksWithMaxTrackSizingFunctionIndexes = tracksWithAutoMaxTrackSizingFunction(unsizedTracks);
    if (tracksWithMaxTrackSizingFunctionIndexes.isEmpty())
        return;

    // by dividing any remaining positive, definite free space equally amongst them.
    auto spacePerTrack = *freeSpace / tracksWithMaxTrackSizingFunctionIndexes.size();

    for (auto trackIndex : tracksWithMaxTrackSizingFunctionIndexes)
        unsizedTracks[trackIndex].baseSize += spacePerTrack;
}

// https://drafts.csswg.org/css-grid-1/#algo-grow-tracks
static void maximizeTracks(UnsizedTracks& unsizedTracks, std::optional<LayoutUnit> availableGridSpace, const AxisConstraint::FreeSpaceScenario& freeSpaceScenario, LayoutUnit gapSize)
{
    switch (freeSpaceScenario) {
    case AxisConstraint::FreeSpaceScenario::MaxContent:
        // If sizing the grid container under a max-content constraint, the free space is infinite.
        // Set each track's base size to its growth limit.
        for (auto& track : unsizedTracks)
            track.baseSize = track.growthLimit;
        break;
    case AxisConstraint::FreeSpaceScenario::MinContent:
        // if sizing under a min-content constraint, the free space is zero, and the track sizes are not increased beyond their base sizes.
        return;
    case AxisConstraint::FreeSpaceScenario::Definite: {
        auto determineUnfrozenTracks = [&]() {
            Vector<size_t> unfrozenTrackIndexes;
            for (auto [trackIndex, unsizedTrack] : indexedRange(unsizedTracks)) {
                ASSERT_WITH_MESSAGE(unsizedTrack.growthLimit != LayoutUnit::max(), "Infinite growth limits should have been resolved by the end of ResolveIntrinsicTrackSizes");
                if (unsizedTrack.baseSize < unsizedTrack.growthLimit)
                    unfrozenTrackIndexes.append(trackIndex);
            }
            return unfrozenTrackIndexes;
        };

        auto freeSpace = computeFreeSpace(availableGridSpace, unsizedTracks, gapSize);
        auto unfrozenTrackIndexes = determineUnfrozenTracks();
        // If the free space is positive...
        while (!unfrozenTrackIndexes.isEmpty() && freeSpace > 0) {
            // distribute it equally to the base sizes of all tracks, freezing tracks as
            // they reach their growth limits (and continuing to grow the unfrozen tracks as needed).
            auto spaceToDistribute = *freeSpace / unfrozenTrackIndexes.size();
            if (!spaceToDistribute)
                break;

            for (auto trackIndex : unfrozenTrackIndexes) {
                auto& unfrozenTrack = unsizedTracks[trackIndex];
                auto spaceRemainingUntilGrowthLimit = unfrozenTrack.growthLimit - unfrozenTrack.baseSize;
                if (spaceRemainingUntilGrowthLimit >= spaceToDistribute)
                    unfrozenTrack.baseSize += spaceToDistribute;
                else
                    unfrozenTrack.baseSize += spaceRemainingUntilGrowthLimit;
            }
            freeSpace = computeFreeSpace(availableGridSpace, unsizedTracks, gapSize);
            unfrozenTrackIndexes = determineUnfrozenTracks();
        }
    }
    }
}

// https://drafts.csswg.org/css-grid-1/#algo-track-sizing
TrackSizes TrackSizingAlgorithm::sizeTracks(const PlacedGridItems& gridItems, const ComputedSizesList& gridItemComputedSizesList,
    const UsedBorderAndPaddingList& borderAndPaddingList, const PlacedGridItemSpanList& gridItemSpanList, const TrackSizingFunctionsList& trackSizingFunctions,
    std::optional<LayoutUnit> availableGridSpace, const TrackSizingGridItemConstraintList& oppositeAxisConstraints, const GridItemSizingFunctions& gridItemSizingFunctions,
    const AxisConstraint::FreeSpaceScenario& freeSpaceScenario, const LayoutUnit gapSize, const StyleContentAlignmentData& usedContentAlignment,
    std::optional<LayoutUnit> containerMinimumSize)
{
    ASSERT(gridItems.size() == gridItemSpanList.size());

    // 1. Initialize Track Sizes
    // GridFormattingContext should have transformed a percentage track to auto if there was no
    // available space so it should not matter what the alternate value we pass in here is.
    auto unsizedTracks = initializeTrackSizes(trackSizingFunctions, availableGridSpace.value_or(0_lu));

    // 2. Resolve Intrinsic Track Sizes
    resolveIntrinsicTrackSizes(unsizedTracks, gridItems, gridItemComputedSizesList, borderAndPaddingList, gridItemSpanList, oppositeAxisConstraints, gridItemSizingFunctions, trackSizingFunctions);

    // 3. Maximize Tracks
    maximizeTracks(unsizedTracks, availableGridSpace, freeSpaceScenario, gapSize);

    // 4. Expand Flexible Tracks
    // https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
    expandFlexibleTracks(unsizedTracks, freeSpaceScenario, availableGridSpace, gapSize, gridItems, gridItemSpanList, oppositeAxisConstraints, gridItemSizingFunctions);

    // https://drafts.csswg.org/css-grid-1/#algo-stretch
    // 5. Stretch 'auto' Tracks
    // If the free space is indefinite, but the grid container has a definite min-width/height,
    // use that size to calculate the free space for this step instead.
    auto freeSpaceForAutoStretchTracks = [&]() -> std::optional<LayoutUnit> {
        switch (freeSpaceScenario) {
        case AxisConstraint::FreeSpaceScenario::Definite:
            return computeFreeSpace(availableGridSpace, unsizedTracks, gapSize);
        case AxisConstraint::FreeSpaceScenario::MinContent:
        case AxisConstraint::FreeSpaceScenario::MaxContent:
            // If the free space is indefinite, but the grid container has a definite min-width/height, use that size to calculate the free space for this step instead.
            if (containerMinimumSize)
                return computeFreeSpace(containerMinimumSize, unsizedTracks, gapSize);
            return computeFreeSpace(availableGridSpace, unsizedTracks, gapSize);
        }
        ASSERT_NOT_REACHED();
        return { };
    };
    stretchAutoTracks(freeSpaceForAutoStretchTracks(), unsizedTracks, usedContentAlignment);

    // Each track has a base size, a <length> which grows throughout the algorithm and
    // which will eventually be the track’s final size...
    return unsizedTracks.map([](const UnsizedTrack& unsizedTrack) {
        return unsizedTrack.baseSize;
    });
}

// https://www.w3.org/TR/css-grid-1/#algo-init
UnsizedTracks TrackSizingAlgorithm::initializeTrackSizes(const TrackSizingFunctionsList& trackSizingFunctionsList, LayoutUnit availableGridSpace)
{
    return trackSizingFunctionsList.map([&availableGridSpace](const TrackSizingFunctions& trackSizingFunctions) -> UnsizedTrack {
        // For each track, if the track’s min track sizing function is:
        auto baseSize = [&] -> LayoutUnit {
            auto& minTrackSizingFunction = trackSizingFunctions.min;

            // A fixed sizing function
            // Resolve to an absolute length and use that size as the track’s initial base size.
            if (minTrackSizingFunction.isLength()) {
                auto& trackBreadthLength = minTrackSizingFunction.length();
                if (auto fixedValue = trackBreadthLength.tryFixed())
                    return LayoutUnit { fixedValue->resolveZoom(Style::ZoomNeeded { }) };
                if (trackBreadthLength.isPercentOrCalculated())
                    return Style::evaluate<LayoutUnit>(trackBreadthLength, availableGridSpace, Style::ZoomNeeded { });

            }

            // An intrinsic sizing function
            // Use an initial base size of zero.
            if (minTrackSizingFunction.isContentSized())
                return { };

            ASSERT_NOT_REACHED();
            return { };
        };

        // For each track, if the track’s max track sizing function is:
        auto growthLimit = [&] -> LayoutUnit {
            auto& maxTrackSizingFunction = trackSizingFunctions.max;

            // A fixed sizing function
            // Resolve to an absolute length and use that size as the track’s initial growth limit.
            if (maxTrackSizingFunction.isLength()) {
                auto trackBreadthLength = maxTrackSizingFunction.length();
                if (auto fixedValue = trackBreadthLength.tryFixed())
                    return LayoutUnit { fixedValue->resolveZoom(Style::ZoomNeeded { }) };
                if (trackBreadthLength.isPercentOrCalculated())
                    return Style::evaluate<LayoutUnit>(trackBreadthLength, availableGridSpace, Style::ZoomNeeded { });
            }

            // An intrinsic sizing function
            // A flexible sizing function
            // Use an initial growth limit of infinity.
            if (maxTrackSizingFunction.isContentSized() || maxTrackSizingFunction.isFlex())
                return LayoutUnit::max();

            ASSERT_NOT_REACHED();
            return { };
        };

        return { baseSize(), growthLimit(), trackSizingFunctions };
    });
}

FlexTracks TrackSizingAlgorithm::collectFlexTracks(const UnsizedTracks& unsizedTracks)
{
    FlexTracks flexTracks;

    for (auto [trackIndex, track] : indexedRange(unsizedTracks)) {
        const auto& maxTrackSizingFunction = track.trackSizingFunction.max;

        if (maxTrackSizingFunction.isFlex()) {
            auto flexFactor = maxTrackSizingFunction.flex();
            flexTracks.append(FlexTrack(trackIndex, flexFactor, track.baseSize, track.growthLimit));
        }
    }

    return flexTracks;
}

bool TrackSizingAlgorithm::hasFlexTracks(const UnsizedTracks& unsizedTracks)
{
    return std::ranges::any_of(unsizedTracks, [](auto& track) {
        return track.trackSizingFunction.max.isFlex();
    });
}

double TrackSizingAlgorithm::flexFactorSum(const FlexTracks& flexTracks)
{
    double total = 0.0;
    for (auto& track : flexTracks)
        total += track.flexFactor.value;
    return total;
}

// https://drafts.csswg.org/css-grid-1/#algo-find-fr-size
LayoutUnit TrackSizingAlgorithm::findSizeOfFr(const UnsizedTracks& tracks, const LayoutUnit availableSpace, const LayoutUnit gapSize)
{
    ASSERT(availableSpace >= 0_lu);

    // https://www.w3.org/TR/css-grid-1/#algo-terms
    // free space = available grid space - sum of base sizes - gutters.
    LayoutUnit totalGutters = GridLayoutUtils::totalGuttersSize(tracks.size(), gapSize);

    InflexibleTrackState state;
    FrSizeComponents components;
    LayoutUnit freeSpace;
    double flexFactorSum;
    LayoutUnit hypotheticalFrSize;

    while (true) {
        components = computeFRSizeComponents(tracks, state);

        // free space = available grid space - sum of base sizes - gutters.
        freeSpace = availableSpace - components.baseSizeSum - totalGutters;

        // If leftover space is negative, the non-flexible tracks have already exceeded the space to fill; flex tracks should be sized to zero.
        // https://www.w3.org/TR/css-grid-1/#grid-track-concept
        if (freeSpace <= 0_lu)
            return 0_lu;

        // https://drafts.csswg.org/css-grid-1/#typedef-flex
        // Values between 0fr and 1fr have a somewhat special behavior: when the sum of the
        // flex factors is less than 1, they take up less than 100% of the leftover space.
        // Handle this by clamping flex factor sum to at least 1.0. Thus, a grid with a single
        // 0.5fr track will have a hypothetical fr size of leftoverSpace / 1.0, and the track will use
        // (0.5 * leftoverSpace) total.
        flexFactorSum = std::max(1.0, components.flexFactorSum);

        // Let the hypothetical fr size be the leftover space divided by the flex factor sum.
        hypotheticalFrSize = freeSpace / LayoutUnit(flexFactorSum);

        // If the hypothetical fr size is valid for all flexible tracks, return that size.
        // Otherwise, restart the algorithm treating the invalid tracks as inflexible.
        if (isValidFlexFactorUnit(tracks, hypotheticalFrSize, state))
            break;
    }

    return hypotheticalFrSize;
}

// "... if the flexible track's flex factor is greater than one,
// the result of dividing the track's base size by its flex factor; otherwise, the track's base size."
static LayoutUnit flexFractionFromTrackBaseSize(const FlexTrack& flexTrack)
{
    if (flexTrack.flexFactor.value > 1.0)
        return flexTrack.baseSize / LayoutUnit(flexTrack.flexFactor.value);
    return flexTrack.baseSize;
}

static bool NODELETE itemCrossesFlexibleTrack(const UnsizedTracks& tracks, const WTF::Range<size_t>& span)
{
    for (size_t trackIndex = span.begin(); trackIndex < span.end(); ++trackIndex) {
        if (tracks[trackIndex].trackSizingFunction.max.isFlex())
            return true;
    }
    return false;
}

// Implements the final step of spec section 11.7:
// "For each flexible track, if the product of the used flex fraction and the track's
// flex factor is greater than the track's base size, set its base size to that product."
static void applyFlexFractionToTracks(UnsizedTracks& unsizedTracks, const FlexTracks& flexTracks, LayoutUnit flexFraction)
{
    for (const auto& flexTrack : flexTracks) {
        LayoutUnit flexSize = flexFraction * LayoutUnit(flexTrack.flexFactor.value);
        if (flexSize > unsizedTracks[flexTrack.trackIndex].baseSize)
            unsizedTracks[flexTrack.trackIndex].baseSize = flexSize;
    }
}

// https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
// "If...sizing the grid container under a min-content constraint, the used flex fraction is zero."
void TrackSizingAlgorithm::expandFlexibleTracksForMinContent(UnsizedTracks&)
{
    // The used flex fraction is zero - no changes to track sizes needed.
}

// https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
// Otherwise, if sizing the grid container under a max-content constraint:
// The used flex fraction is the maximum of:
// * For each flexible track, if the flexible track's flex factor is greater than one,
//   the result of dividing the track's base size by its flex factor; otherwise, the track's base size.
// * For each grid item that crosses a flexible track, the result of finding the size of an fr
//   using all the grid tracks that the item crosses and a space to fill of the item's max-content contribution.
void TrackSizingAlgorithm::expandFlexibleTracksForMaxContent(UnsizedTracks& unsizedTracks, const FlexTracks& flexTracks,
    const LayoutUnit gapSize, const PlacedGridItems& gridItems, const PlacedGridItemSpanList& gridItemSpanList,
    const TrackSizingGridItemConstraintList& oppositeAxisConstraints, const GridItemSizingFunctions& gridItemSizingFunctions)
{
    // The used flex fraction is the maximum of:
    LayoutUnit usedFlexFraction = 0_lu;

    // For each flexible track, if the flexible track's flex factor is greater than one,
    // the result of dividing the track's base size by its flex factor; otherwise, the track's base size.
    for (const auto& flexTrack : flexTracks)
        usedFlexFraction = std::max(usedFlexFraction, flexFractionFromTrackBaseSize(flexTrack));

    // For each grid item that crosses a flexible track, the result of finding the size of an fr
    // using all the grid tracks that the item crosses and a space to fill of the item's max-content contribution.
    for (auto [gridItemIndex, gridItemSpan] : indexedRange(gridItemSpanList)) {
        if (!itemCrossesFlexibleTrack(unsizedTracks, gridItemSpan))
            continue;

        auto maxContentContribution = gridItemSizingFunctions.maxContentContribution(gridItems[gridItemIndex], oppositeAxisConstraints[gridItemIndex]);
        auto itemTracks = unsizedTracks.subspan(gridItemSpan.begin(), gridItemSpan.distance());
        auto candidateFlexFraction = findSizeOfFr(itemTracks, maxContentContribution, gapSize);

        usedFlexFraction = std::max(usedFlexFraction, candidateFlexFraction);
    }

    // For each flexible track, if the product of the used flex fraction and the track's flex factor
    // is greater than the track's base size, set its base size to that product.
    applyFlexFractionToTracks(unsizedTracks, flexTracks, usedFlexFraction);
}

// https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
// Otherwise, if the free space is a definite length:
// The used flex fraction is the result of finding the size of an fr using all of the
// grid tracks and a space to fill of the available grid space (minus gutters).
void TrackSizingAlgorithm::expandFlexibleTracksForDefiniteLength(UnsizedTracks& unsizedTracks, const FlexTracks& flexTracks, std::optional<LayoutUnit> availableGridSpace, const LayoutUnit gapSize)
{
    ASSERT(availableGridSpace.has_value());

    // https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
    // "If the free space is zero...the used flex fraction is zero."
    // If availableSpace is zero, free space must also be 0.
    if (availableGridSpace.value() == 0_lu)
        return;

    // https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
    // Otherwise, if the free space is a definite length:
    // The used flex fraction is the result of finding the size of an fr using all of the
    // grid tracks and a space to fill of the available grid space (minus gutters).
    auto frSize = findSizeOfFr(unsizedTracks, availableGridSpace.value(), gapSize);

    // For each flexible track, if the product of the used flex fraction and the track's flex factor is greater than the track's base size, set its base size to that product.
    applyFlexFractionToTracks(unsizedTracks, flexTracks, frSize);
}

// https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
void TrackSizingAlgorithm::expandFlexibleTracks(UnsizedTracks& unsizedTracks, const AxisConstraint::FreeSpaceScenario& freeSpaceScenario,
    std::optional<LayoutUnit> availableGridSpace, const LayoutUnit gapSize, const PlacedGridItems& gridItems,
    const PlacedGridItemSpanList& gridItemSpanList, const TrackSizingGridItemConstraintList& oppositeAxisConstraints,
    const GridItemSizingFunctions& gridItemSizingFunctions)
{
    if (!hasFlexTracks(unsizedTracks))
        return;
    auto flexTracks = collectFlexTracks(unsizedTracks);
    double totalFlex = flexFactorSum(flexTracks);
    if (!totalFlex)
        return;

    // https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
    // "If...sizing the grid container under a min-content constraint, the used flex fraction is zero."
    if (freeSpaceScenario == AxisConstraint::FreeSpaceScenario::MinContent) {
        expandFlexibleTracksForMinContent(unsizedTracks);
        return;
    }

    // Otherwise, if sizing the grid container under a max-content constraint:
    if (freeSpaceScenario == AxisConstraint::FreeSpaceScenario::MaxContent) {
        ASSERT(!availableGridSpace);
        expandFlexibleTracksForMaxContent(unsizedTracks, flexTracks, gapSize, gridItems, gridItemSpanList, oppositeAxisConstraints, gridItemSizingFunctions);
        return;
    }

    ASSERT(freeSpaceScenario == AxisConstraint::FreeSpaceScenario::Definite);
    expandFlexibleTracksForDefiniteLength(unsizedTracks, flexTracks, availableGridSpace, gapSize);
}

} // namespace Layout
} // namespace WebCore
