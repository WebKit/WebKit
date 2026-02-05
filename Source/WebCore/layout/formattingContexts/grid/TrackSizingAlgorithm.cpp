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

static Vector<LayoutUnit> minContentContributions(const PlacedGridItems& gridItems, const GridItemIndexes& gridItemIndexes,
    const IntegrationUtils& integrationUtils, const GridItemSizingFunctions& gridItemSizingFunctions)
{
    return gridItemIndexes.map([&](size_t gridItemIndex) {
        return gridItemSizingFunctions.minContentContribution(gridItems[gridItemIndex].layoutBox(), integrationUtils);
    });
}

static Vector<LayoutUnit> maxContentContributions(const PlacedGridItems& gridItems, const GridItemIndexes& gridItemIndexes,
    const IntegrationUtils& integrationUtils, const GridItemSizingFunctions& gridItemSizingFunctions)
{
    return gridItemIndexes.map([&](size_t gridItemIndex) {
        return gridItemSizingFunctions.maxContentContribution(gridItems[gridItemIndex].layoutBox(), integrationUtils);
    });
}

static Vector<LayoutUnit> minimumContributions(const PlacedGridItems& gridItems, const ComputedSizesList& gridItemComputedSizesList,
    const GridItemIndexes& gridItemIndexes, const IntegrationUtils& integrationUtils, const GridItemSizingFunctions& gridItemSizingFunctions)
{
    // The minimum contribution of an item is the smallest outer size it can have. Specifically,
    return gridItemIndexes.map([&](size_t gridItemIndex) -> LayoutUnit {
        // if the item’s computed preferred size behaves as auto or depends on the size of its
        // containing block in the relevant axis, its minimum contribution is the outer size
        // that would result from assuming the item’s used minimum size as its preferred size.
        auto& preferredSize = gridItemComputedSizesList[gridItemIndex].preferredSize;
        if (GridLayoutUtils::preferredSizeBehavesAsAuto(preferredSize) || GridLayoutUtils::preferredSizeDependsOnContainingBlockSize(preferredSize)) {
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        }
        // else the item’s minimum contribution is its min-content contribution.
        return gridItemSizingFunctions.minContentContribution(gridItems[gridItemIndex].layoutBox(), integrationUtils);
    });
}

// https://drafts.csswg.org/css-grid-1/#algo-single-span-items
static void sizeTracksToFitNonSpanningItems(UnsizedTracks& unsizedTracks, const PlacedGridItems& gridItems,
    const ComputedSizesList& gridItemComputedSizesList, const PlacedGridItemSpanList& gridItemSpanList,
    const IntegrationUtils& integrationUtils, const GridItemSizingFunctions& gridItemSizingFunctions)
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
                auto itemContributions = minContentContributions(gridItems, singleSpanningItemsIndexes, integrationUtils, gridItemSizingFunctions);
                ASSERT(itemContributions.size() == singleSpanningItemsIndexes.size());
                if (itemContributions.isEmpty())
                    return { };
                return std::max({ }, std::ranges::max(itemContributions));
            },
            [&](const CSS::Keyword::MaxContent&) -> LayoutUnit {
                // If the track has a max-content min track sizing function, set its base
                // size to the maximum of the items’ max-content contributions, floored at zero.
                auto itemContributions = maxContentContributions(gridItems, singleSpanningItemsIndexes, integrationUtils, gridItemSizingFunctions);
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
                auto contributions = minimumContributions(gridItems, gridItemComputedSizesList, singleSpanningItemsIndexes, integrationUtils, gridItemSizingFunctions);
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
                auto itemContributions = minContentContributions(gridItems, singleSpanningItemsIndexes, integrationUtils, gridItemSizingFunctions);
                ASSERT(itemContributions.size() == singleSpanningItemsIndexes.size());
                if (itemContributions.isEmpty())
                    return { };
                return std::ranges::max(itemContributions);
            },
            [&](const CSS::Keyword::MaxContent&) -> LayoutUnit {
                // If the track has a max-content max track sizing function, set its growth
                // limit to the maximum of the items’ max-content contributions.
                auto itemContributions = maxContentContributions(gridItems, singleSpanningItemsIndexes, integrationUtils, gridItemSizingFunctions);
                auto maximumMaxContentContribution = itemContributions.isEmpty() ? 0_lu : std::ranges::max(itemContributions);

                auto hasFitContentMaximum = [] {
                    ASSERT_NOT_IMPLEMENTED_YET();
                    return false;
                };
                // For fit-content() maximums, furthermore clamp this growth limit by the
                // fit-content() argument.
                if (hasFitContentMaximum())  {
                    ASSERT_NOT_IMPLEMENTED_YET();
                    return { };
                }
                return maximumMaxContentContribution;
            },
            [&](const CSS::Keyword::Auto&) -> LayoutUnit {
                ASSERT_NOT_IMPLEMENTED_YET();
                return { };
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
    const ComputedSizesList& gridItemComputedSizesList, const PlacedGridItemSpanList& gridItemSpanList,
    const IntegrationUtils& integrationUtils, const GridItemSizingFunctions& gridItemSizingFunctions)
{
    // 1. Shim baseline-aligned items so their intrinsic size contributions reflect their
    // baseline alignment.
    auto shimBaselineAlignedItems = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(shimBaselineAlignedItems);

    // 2. Size tracks to fit non-spanning items.
    sizeTracksToFitNonSpanningItems(unsizedTracks, gridItems, gridItemComputedSizesList, gridItemSpanList,
        integrationUtils, gridItemSizingFunctions);

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
    auto setInfiniteGrowthLimitsToBaseSize = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(setInfiniteGrowthLimitsToBaseSize);
}

// https://drafts.csswg.org/css-grid-1/#algo-track-sizing
TrackSizes TrackSizingAlgorithm::sizeTracks(const PlacedGridItems& gridItems, const ComputedSizesList& gridItemComputedSizesList,
    const PlacedGridItemSpanList& gridItemSpanList, const TrackSizingFunctionsList& trackSizingFunctions,
    std::optional<LayoutUnit> availableSpace, const GridItemSizingFunctions& gridItemSizingFunctions,
    const IntegrationUtils& integrationUtils, const FreeSpaceScenario& freeSpaceScenario, const LayoutUnit& gapSize)
{
    ASSERT(gridItems.size() == gridItemSpanList.size());

    // 1. Initialize Track Sizes
    auto unsizedTracks = initializeTrackSizes(trackSizingFunctions);

    // 2. Resolve Intrinsic Track Sizes
    resolveIntrinsicTrackSizes(unsizedTracks, gridItems, gridItemComputedSizesList, gridItemSpanList, integrationUtils, gridItemSizingFunctions);

    // 3. Maximize Tracks
    auto maximizeTracks = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(maximizeTracks);

    // 4. Expand Flexible Tracks
    // https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
    auto expandFlexibleTracks = [&] {
        if (!hasFlexTracks(unsizedTracks))
            return;
        auto flexTracks = collectFlexTracks(unsizedTracks);
        double totalFlex = flexFactorSum(flexTracks);
        if (!totalFlex)
            return;

        // https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
        // "If...sizing the grid container under a min-content
        // constraint, the used flex fraction is zero."
        if (freeSpaceScenario == FreeSpaceScenario::MinContent)
            return;

        // Otherwise, if the free space is an indefinite length:
        if (freeSpaceScenario == FreeSpaceScenario::Indefinite) {
            // FIXME: Implement indefinite free space (spec §11.7 Scenario 3).
            // Compute flex fraction based on max-content contributions.
            ASSERT(!availableSpace);
            notImplemented();
            return;
        }

        ASSERT(freeSpaceScenario == FreeSpaceScenario::Definite);
        ASSERT(availableSpace.has_value());

        // https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
        // "If the free space is zero...the used flex fraction is zero."
        // If availableSpace is zero, free space must also be 0.
        if (availableSpace.value() == 0_lu)
            return;

        // https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
        // Otherwise, if the free space is a definite length:
        // The used flex fraction is the result of finding the size of an fr using all of the
        // grid tracks and a space to fill of the available grid space (minus gutters).
        auto frSize = findSizeOfFr(unsizedTracks, availableSpace.value(), gapSize);

        // For each flexible track, if the product of the used flex fraction and the track's flex factor is greater than the track's base size, set its base size to that product.
        for (auto& flexTrack : flexTracks) {
            LayoutUnit flexSize = frSize * LayoutUnit(flexTrack.flexFactor.value);
            if (flexSize > unsizedTracks[flexTrack.trackIndex].baseSize)
                unsizedTracks[flexTrack.trackIndex].baseSize = flexSize;
        }
    };
    expandFlexibleTracks();

    // 5. Expand Stretched auto Tracks
    auto expandStretchedAutoTracks = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(expandStretchedAutoTracks);

    // Each track has a base size, a <length> which grows throughout the algorithm and
    // which will eventually be the track’s final size...
    return unsizedTracks.map([](const UnsizedTrack& unsizedTrack) {
        return unsizedTrack.baseSize;
    });
}

// https://www.w3.org/TR/css-grid-1/#algo-init
UnsizedTracks TrackSizingAlgorithm::initializeTrackSizes(const TrackSizingFunctionsList& trackSizingFunctionsList)
{
    return trackSizingFunctionsList.map([](const TrackSizingFunctions& trackSizingFunctions) -> UnsizedTrack {
        // For each track, if the track’s min track sizing function is:
        auto baseSize = [&] -> LayoutUnit {
            auto& minTrackSizingFunction = trackSizingFunctions.min;

            // A fixed sizing function
            // Resolve to an absolute length and use that size as the track’s initial base size.
            if (minTrackSizingFunction.isLength()) {
                auto& trackBreadthLength = minTrackSizingFunction.length();
                if (auto fixedValue = trackBreadthLength.tryFixed())
                    return LayoutUnit { fixedValue->resolveZoom(Style::ZoomNeeded { }) };

                if (auto percentValue = trackBreadthLength.tryPercentage()) {
                    ASSERT_NOT_IMPLEMENTED_YET();
                    return { };
                }

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
LayoutUnit TrackSizingAlgorithm::findSizeOfFr(const UnsizedTracks& tracks, const LayoutUnit& availableSpace, const LayoutUnit& gapSize)
{
    ASSERT(availableSpace >= 0_lu);

    // https://www.w3.org/TR/css-grid-1/#algo-terms
    // free space = available grid space - sum of base sizes - gutters.
    LayoutUnit totalGutters = tracks.size() > 1 ? gapSize * LayoutUnit(tracks.size() - 1) : 0_lu;

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

} // namespace Layout
} // namespace WebCore
