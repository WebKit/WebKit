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
#include "StyleContentAlignmentData.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
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

enum class ExtraSpaceDistributionTarget : bool { BaseSizes, GrowthLimits };

struct UnsizedTrack {
    LayoutUnit baseSize;
    LayoutUnit growthLimit;
    const TrackSizingFunctions trackSizingFunction;
    // https://drafts.csswg.org/css-grid-1/#infinitely-growable
    // FIXME: Add a RAII helper to set this flag when a track's growth limit is changed from infinite to finite
    // while resolving intrinsic maximums.
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
    // The limit at which this track's item-incurred increase freezes. For base sizes, that's the
    // growth limit. For growth limits, it's the growth limit if finite and not infinitely growable,
    // otherwise infinity.
    // FIXME: handle fit-content() argument if it has one.
    LayoutUnit freezeLimit(ExtraSpaceDistributionTarget spaceDistributionTarget) const
    {
        if (spaceDistributionTarget == ExtraSpaceDistributionTarget::BaseSizes)
            return growthLimit;
        // Growth limits: an infinitely-growable track has no ceiling; otherwise its growth limit.
        return infinitelyGrowable ? LayoutUnit::max() : growthLimit;
    }
};

using GridItemIndexes = Vector<size_t>;

struct InflexibleTrackState {
    BitVector inflexibleTracks;

    bool isFlexible(size_t trackIndex, const UnsizedTrack& track) const
    {
        return track.trackSizingFunction.max.isFlex()
            && !inflexibleTracks.get(trackIndex);
    }

    void markAsInflexible(size_t trackIndex)
    {
        inflexibleTracks.set(trackIndex);
    }
};

struct FrSizeComponents {
    LayoutUnit baseSizeSum;
    double flexFactorSum;
};

static PlacedGridItemSpanList spannedLinesList(const TrackSizingItemList& trackSizingItems)
{
    return trackSizingItems.map([](const TrackSizingItem& item) {
        return item.spannedLines;
    });
}

struct ResolveIntrinsicTrackSizesContext {
    ResolveIntrinsicTrackSizesContext(const TrackSizingItemList& trackSizingItems, const GridItemSizingFunctions& gridItemSizingFunctions, const TrackSizingFunctionsList& trackSizingFunctionsList, const AxisConstraint& axisConstraint, LayoutUnit gapSize)
        : trackSizingItems(trackSizingItems)
        , gridItemSizingFunctions(gridItemSizingFunctions)
        , trackSizingFunctionsList(trackSizingFunctionsList)
        , axisConstraint(axisConstraint)
        , gapSize(gapSize) { }

    const TrackSizingItemList& trackSizingItems;
    const GridItemSizingFunctions& gridItemSizingFunctions;
    const TrackSizingFunctionsList& trackSizingFunctionsList;
    const AxisConstraint& axisConstraint;
    const LayoutUnit gapSize;
};

static bool isSizedUnderMinOrMaxContentConstraint(const AxisConstraint& axisConstraint)
{
    auto scenario = axisConstraint.scenario();
    return scenario == AxisConstraint::FreeSpaceScenario::MinContent
        || scenario == AxisConstraint::FreeSpaceScenario::MaxContent;
}

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
static bool isValidFlexFactorUnit(const UnsizedTracks& tracks, double hypotheticalFrSize, InflexibleTrackState& state)
{
    bool hasInvalidTracks = false;
    for (auto [index, track] : indexedRange(tracks)) {
        if (!state.isFlexible(index, track))
            continue;

        auto flexFactor = track.trackSizingFunction.max.flex();
        double computedSize = hypotheticalFrSize * flexFactor.value;

        // If the product of the hypothetical fr size and a flexible track's flex factor is less
        // than the track's base size, we should treat this track as inflexible.
        if (computedSize < track.baseSize.toDouble()) {
            hasInvalidTracks = true;
            state.markAsInflexible(index);
        }
    }

    return !hasInvalidTracks;
}

static GridItemIndexes singleSpanningItemsWithinTrack(size_t trackIndex, const TrackSizingItemList& trackSizingItems)
{
    GridItemIndexes nonSpanningItems;
    for (auto [trackSizingItemIndex, trackSizingItem] : WTF::indexedRange(trackSizingItems)) {
        if (trackSizingItem.spannedLines.distance() == 1 && trackSizingItem.spannedLines.begin() == trackIndex)
            nonSpanningItems.append(trackSizingItemIndex);
    }
    return nonSpanningItems;
}

static bool itemCrossesFlexibleTrack(const UnsizedTracks& tracks, const WTF::Range<size_t>& span)
{
    for (size_t trackIndex = span.begin(); trackIndex < span.end(); ++trackIndex) {
        if (tracks[trackIndex].trackSizingFunction.max.isFlex())
            return true;
    }
    return false;
}

static GridItemIndexes itemsSpanningFlexibleTracks(const UnsizedTracks& unsizedTracks, const PlacedGridItemSpanList& gridItemSpanList)
{
    GridItemIndexes spanningItems;
    for (auto [gridItemIndex, gridItemSpan] : WTF::indexedRange(gridItemSpanList)) {
        if (itemCrossesFlexibleTrack(unsizedTracks, gridItemSpan))
            spanningItems.append(gridItemIndex);
    }
    return spanningItems;
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

static Vector<LayoutUnit> minContentContributions(const TrackSizingItemList& trackSizingItems, const GridItemIndexes& gridItemIndexes,
    const GridItemSizingFunctions& gridItemSizingFunctions)
{
    return gridItemIndexes.map([&](size_t gridItemIndex) {
        return gridItemSizingFunctions.minContentContribution(trackSizingItems[gridItemIndex].gridItem, trackSizingItems[gridItemIndex].oppositeAxisConstraint);
    });
}

static Vector<LayoutUnit> maxContentContributions(const TrackSizingItemList& trackSizingItems, const GridItemIndexes& gridItemIndexes,
    const GridItemSizingFunctions& gridItemSizingFunctions)
{
    return gridItemIndexes.map([&](size_t gridItemIndex) {
        return gridItemSizingFunctions.maxContentContribution(trackSizingItems[gridItemIndex].gridItem, trackSizingItems[gridItemIndex].oppositeAxisConstraint);
    });
}

// https://www.w3.org/TR/css-grid-2/#algo-single-span-items
// The minimum contribution of an item is the smallest outer size it can have. Specifically, if the
// item's computed preferred size behaves as auto or depends on the size of its containing block in
// the relevant axis, its minimum contribution is the outer size that would result from assuming the
// item's used minimum size as its preferred size; else the item's minimum contribution is its
// min-content contribution.
static LayoutUnit minimumContribution(const TrackSizingItemList& trackSizingItems, size_t gridItemIndex,
    const GridItemSizingFunctions& gridItemSizingFunctions, const TrackSizingFunctionsList& trackSizingFunctions)
{
    auto& trackSizingItem = trackSizingItems[gridItemIndex];
    auto& preferredSize = trackSizingItem.computedSizes.preferredSize;
    if (GridLayoutUtils::preferredSizeBehavesAsAuto(preferredSize) || GridLayoutUtils::sizeDependsOnContainingBlockSize(preferredSize))
        return gridItemSizingFunctions.usedMinimumSize(trackSizingItem.gridItem, trackSizingFunctions, trackSizingItem.borderAndPadding, trackSizingItem.oppositeAxisConstraint);
    return gridItemSizingFunctions.minContentContribution(trackSizingItem.gridItem, trackSizingItem.oppositeAxisConstraint);
}

static Vector<LayoutUnit> minimumContributions(const TrackSizingItemList& trackSizingItems,
    const GridItemIndexes& gridItemIndexes, const GridItemSizingFunctions& gridItemSizingFunctions, const TrackSizingFunctionsList& trackSizingFunctions)
{
    return gridItemIndexes.map([&](size_t gridItemIndex) {
        return minimumContribution(trackSizingItems, gridItemIndex, gridItemSizingFunctions, trackSizingFunctions);
    });
}

// https://drafts.csswg.org/css-grid-1/#limited-contribution
// Since the sum is used for limited min/max content contributions, which are only computed
// when the grid is sized under a min/max content constraint, percentages cannot be resolved.
static std::optional<LayoutUnit> fixedMaxTrackSizingFunctionSum(const WTF::Range<size_t>& itemSpan, const UnsizedTracks& unsizedTracks)
{
    LayoutUnit sum;
    for (size_t trackIndex = itemSpan.begin(); trackIndex < itemSpan.end(); ++trackIndex) {
        auto& trackSizingFunction = unsizedTracks[trackIndex].trackSizingFunction;
        auto& maxTrackSizingFunction = trackSizingFunction.max;
        if (!maxTrackSizingFunction.isLength())
            return { };
        auto fixedValue = maxTrackSizingFunction.length().tryFixed();
        if (!fixedValue)
            return { };
        sum += Style::evaluate<LayoutUnit>(*fixedValue, trackSizingFunction.zoom);
    }
    return sum;
}

// https://drafts.csswg.org/css-grid-1/#limited-contribution
// The limited min-/max-content contribution of an item is its min-/max-content contribution,
// limited by the sum of the max track sizing function and ultimately floored by its minimum contribution.
//
// This is only computed when the grid container is being sized under a min-/max-content constraint.
static Vector<LayoutUnit> limitedContentContributions(const Vector<LayoutUnit>& contentContributions, const Vector<std::optional<LayoutUnit>>& fixedMaxTrackSizingFunctionSums, const Vector<LayoutUnit>& minimumContributions)
{
    Vector<LayoutUnit> limitedContributions;
    limitedContributions.reserveInitialCapacity(contentContributions.size());
    for (auto [index, contribution] : WTF::indexedRange(contentContributions)) {
        auto limitedContribution = contribution;
        if (auto fixedMaxSum = fixedMaxTrackSizingFunctionSums[index])
            limitedContribution = std::min(limitedContribution, *fixedMaxSum);
        limitedContributions.append(std::max(limitedContribution, minimumContributions[index]));
    }
    return limitedContributions;
}

// https://drafts.csswg.org/css-grid-1/#algo-single-span-items
static void sizeTracksToFitNonSpanningItems(const ResolveIntrinsicTrackSizesContext& resolveIntrinsicTrackSizesContext,
    UnsizedTracks& unsizedTracks)
{
    auto& trackSizingItems = resolveIntrinsicTrackSizesContext.trackSizingItems;
    auto& gridItemSizingFunctions = resolveIntrinsicTrackSizesContext.gridItemSizingFunctions;

    // For each track with an intrinsic track sizing function and not a flexible sizing function, consider the items in it with a span of 1:
    for (auto trackIndex : tracksWithIntrinsicSizingFunction(unsizedTracks)) {
        auto& track = unsizedTracks[trackIndex];
        auto singleSpanningItemsIndexes = singleSpanningItemsWithinTrack(trackIndex, trackSizingItems);

        auto& minimumTrackSizingFunction = track.trackSizingFunction.min;
        track.baseSize = WTF::switchOn(minimumTrackSizingFunction,
            [&](const CSS::Keyword::MinContent&) -> LayoutUnit {
                // If the track has a min-content min track sizing function, set its base size
                // to the maximum of the items’ min-content contributions, floored at zero.
                auto itemContributions = minContentContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions);
                ASSERT(itemContributions.size() == singleSpanningItemsIndexes.size());
                if (itemContributions.isEmpty())
                    return { };
                return std::max({ }, std::ranges::max(itemContributions));
            },
            [&](const CSS::Keyword::MaxContent&) -> LayoutUnit {
                // If the track has a max-content min track sizing function, set its base
                // size to the maximum of the items’ max-content contributions, floored at zero.
                auto itemContributions = maxContentContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions);
                ASSERT(itemContributions.size() == singleSpanningItemsIndexes.size());
                if (itemContributions.isEmpty())
                    return { };
                return std::max({ }, std::ranges::max(itemContributions));
            },
            [&](const CSS::Keyword::Auto&) -> LayoutUnit {
                // If the track has an auto min track sizing function and the grid container is
                // being sized under a min-/max-content constraint, its base size should be the
                // maximum of its items’ limited min-content contributions, floored at zero.
                if (isSizedUnderMinOrMaxContentConstraint(resolveIntrinsicTrackSizesContext.axisConstraint)) {
                    // The limited min-/max-content contribution of an item is (for this purpose) its min-/max-content contribution (accordingly),
                    auto minContentSizeContributions = minContentContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions);

                    // limited by the max track sizing function (which could be the argument to a fit-content() track sizing function) if that is fixed
                    auto fixedMaxTrackSizingFunctionSums = singleSpanningItemsIndexes.map([&](size_t gridItemIndex) {
                        return fixedMaxTrackSizingFunctionSum(trackSizingItems[gridItemIndex].spannedLines, unsizedTracks);
                    });

                    // and ultimately floored by its minimum contribution.
                    auto itemMinimumContributions = minimumContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions, resolveIntrinsicTrackSizesContext.trackSizingFunctionsList);

                    auto limitedContributions = limitedContentContributions(minContentSizeContributions, fixedMaxTrackSizingFunctionSums, itemMinimumContributions);
                    if (limitedContributions.isEmpty())
                        return { };
                    return std::max({ }, std::ranges::max(limitedContributions));
                }
                // Otherwise, set the track’s base size to the maximum of its items’ minimum
                // contributions, floored at zero.
                auto contributions = minimumContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions, resolveIntrinsicTrackSizesContext.trackSizingFunctionsList);
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
                auto itemContributions = minContentContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions);
                ASSERT(itemContributions.size() == singleSpanningItemsIndexes.size());
                if (itemContributions.isEmpty())
                    return { };
                return std::ranges::max(itemContributions);
            },
            [&](const CSS::Keyword::MaxContent&) -> LayoutUnit {
                // If the track has a max-content max track sizing function, set its growth
                // limit to the maximum of the items’ max-content contributions.
                auto itemContributions = maxContentContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions);
                auto maximumMaxContentContribution = itemContributions.isEmpty() ? 0_lu : std::ranges::max(itemContributions);
                return maximumMaxContentContribution;
            },
            [&](const CSS::Keyword::Auto&) -> LayoutUnit {
                // Since it is not explicitly stated otherwise in the spec, auto is treated as max-content:
                // If the track has a max-content max track sizing function, set its growth
                // limit to the maximum of the items’ max-content contributions.
                auto itemContributions = maxContentContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions);
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

// https://drafts.csswg.org/css-grid-1/#extra-space
// 2.2: Distribute space up to limits: UpToGrowthLimit
// 2.3: Distribute space to non-affected tracks: UpToGrowthLimit
// 2.4: Distribute space beyond limits: BeyondGrowthLimit
enum class SpaceDistributionLimit : bool { UpToGrowthLimit, BeyondGrowthLimit };

// Used for space distribution.
static Vector<size_t> indexesForUnfrozenAffectedTracks(const Vector<size_t>& spannedAffectedTracks,
    const UnsizedTracks& unsizedTracks, const Vector<LayoutUnit>& itemIncurredIncreases, ExtraSpaceDistributionTarget target, SpaceDistributionLimit limit)
{
    if (limit == SpaceDistributionLimit::BeyondGrowthLimit)
        return spannedAffectedTracks;
    Vector<size_t> indexes;
    for (auto trackIndex : spannedAffectedTracks) {
        if (unsizedTracks[trackIndex].affectedSize(target) + itemIncurredIncreases[trackIndex] < unsizedTracks[trackIndex].freezeLimit(target))
            indexes.append(trackIndex);
    }
    return indexes;
}

// Distributes space equally among trackIndexes in successive rounds, freezing tracks (per
// spaceDistributedToTrack, below) as needed, until space is exhausted or no tracks remain unfrozen.
static void distributeSpaceEquallyAmongTracks(LayoutUnit& space, const Vector<size_t>& trackIndexes,
    const UnsizedTracks& unsizedTracks, Vector<LayoutUnit>& itemIncurredIncreases, ExtraSpaceDistributionTarget target, SpaceDistributionLimit limit)
{
    auto unfrozenTrackIndexes = indexesForUnfrozenAffectedTracks(trackIndexes, unsizedTracks, itemIncurredIncreases, target, limit);
    while (!unfrozenTrackIndexes.isEmpty() && space > 0) {
        auto tracksRemainingForDistributionCount = unfrozenTrackIndexes.size();

        // https://drafts.csswg.org/css-grid-1/#extra-space
        // Step 2.2, "Distribute space up to limits" (SpaceDistributionLimit::UpToGrowthLimit): "Find the
        // item-incurred increase for each affected track by: distributing the space equally among these
        // tracks, freezing a track's item-incurred increase as its affected size + item-incurred increase
        // reaches its limit (and continuing to grow the unfrozen tracks as needed)."
        // Step 2.4, "Distribute space beyond limits" (SpaceDistributionLimit::BeyondGrowthLimit): "If extra
        // space remains at this point, unfreeze and continue to distribute space to the item-incurred
        // increase of…"
        auto spaceDistributedToTrack = [&](size_t trackIndex) {
            auto spaceDistributed = space / tracksRemainingForDistributionCount;
            if (limit == SpaceDistributionLimit::BeyondGrowthLimit)
                return spaceDistributed;
            auto& track = unsizedTracks[trackIndex];
            auto spaceRemainingUntilLimit = track.freezeLimit(target) - (track.affectedSize(target) + itemIncurredIncreases[trackIndex]);
            return std::min(spaceDistributed, spaceRemainingUntilLimit);
        };

        for (auto trackIndex : unfrozenTrackIndexes) {
            auto spaceDistributed = spaceDistributedToTrack(trackIndex);
            itemIncurredIncreases[trackIndex] += spaceDistributed;
            space -= spaceDistributed;
            --tracksRemainingForDistributionCount;
        }

        // BeyondGrowthLimit distributes without a per-track cap, so all space should be distributed.
        if (limit == SpaceDistributionLimit::BeyondGrowthLimit) {
            ASSERT(!space);
            break;
        }
        unfrozenTrackIndexes = indexesForUnfrozenAffectedTracks(trackIndexes, unsizedTracks, itemIncurredIncreases, target, limit);
    }
}

// https://drafts.csswg.org/css-grid-1/#extra-space
static void distributeExtraSpace(ExtraSpaceDistributionTarget spaceDistributionTarget, const TrackIndexes& affectedTracksIndexes,
    const Vector<LayoutUnit>& sizeContributions, const GridItemIndexes& accommodatedItemsIndexes,
    const PlacedGridItemSpanList& gridItemSpanList, UnsizedTracks& unsizedTracks, LayoutUnit gapSize)
{
    ASSERT(accommodatedItemsIndexes.size() == sizeContributions.size());

    // 1. Maintain separately for each affected track a planned increase, initially set to 0. The
    // vector is keyed by track index.
    Vector<LayoutUnit> plannedIncreases(unsizedTracks.size());

    // 2. For each accommodated item...
    for (auto [contributionIndex, gridItemIndex] : WTF::indexedRange(accommodatedItemsIndexes)) {
        // considering only tracks the item spans:
        auto itemSpan = gridItemSpanList[gridItemIndex];

        // Partition the spanned tracks into the affected tracks, which receive space in 2.2, and
        // the non-affected tracks, which never receive space but reduce spaceToDistribute in 2.3.
        Vector<size_t> spannedAffectedTracks;
        Vector<size_t> spannedNonAffectedTracks;
        for (size_t trackIndex = itemSpan.begin(); trackIndex < itemSpan.end(); ++trackIndex) {
            if (affectedTracksIndexes.contains(trackIndex))
                spannedAffectedTracks.append(trackIndex);
            else
                spannedNonAffectedTracks.append(trackIndex);
        }

        // https://drafts.csswg.org/css-grid-1/#extra-space
        // 2.1. "Find the space to distribute: Subtract the affected size of every spanned track
        // (not just the affected tracks) from the item's size contribution, flooring it at
        // zero. (For infinite growth limits, substitute the track's base size.)
        //  This remaining size contribution is the space to distribute."
        // https://drafts.csswg.org/css-grid-1/#gutters
        // For the purpose of track sizing, each gutter is treated as an extra, empty,
        // fixed-size track of the specified size, which is spanned by any
        // grid items that span across its corresponding grid line.
        LayoutUnit spannedSizes;
        for (size_t trackIndex = itemSpan.begin(); trackIndex < itemSpan.end(); ++trackIndex)
            spannedSizes += unsizedTracks[trackIndex].affectedSize(spaceDistributionTarget);
        auto spannedGutters = GridLayoutUtils::totalGuttersSize(itemSpan.distance(), gapSize);
        auto spaceToDistribute = std::max(0_lu, sizeContributions[contributionIndex] - spannedSizes - spannedGutters);
        if (!spaceToDistribute)
            continue;

        // 2.2. Distribute space up to limits:
        Vector<LayoutUnit> itemIncurredIncreases(unsizedTracks.size());
        distributeSpaceEquallyAmongTracks(spaceToDistribute, spannedAffectedTracks, unsizedTracks, itemIncurredIncreases, spaceDistributionTarget, SpaceDistributionLimit::UpToGrowthLimit);

        // https://drafts.csswg.org/css-grid-1/#extra-space
        // 2.3. "Distribute space to non-affected tracks: If extra space remains at this point, and
        // the item spans both affected tracks and non-affected tracks, distribute space as for the
        // previous step, but into the non-affected tracks instead."
        if (spaceToDistribute > 0 && !spannedAffectedTracks.isEmpty() && !spannedNonAffectedTracks.isEmpty()) {
            // These tracks are not affected, so we don't need to track their item-incurred increases for later.
            // The purpose of this step is to reduce spaceToDistribute, so we can just use a throwaway vector.
            Vector<LayoutUnit> unappliedIncreases(unsizedTracks.size());
            // In step 2.1. we subtracted the size of non-affected tracks from the item's size contribution.
            // In this step, we are subtracting extra space up to the growth limit of the non-affected tracks.
            distributeSpaceEquallyAmongTracks(spaceToDistribute, spannedNonAffectedTracks, unsizedTracks, unappliedIncreases, spaceDistributionTarget, SpaceDistributionLimit::UpToGrowthLimit);
        }

        // 2.4. Distribute space beyond limits: if extra space still remains, unfreeze and continue
        // distributing it to the item-incurred increases.
        auto distributeSpaceBeyondLimits = [] {
            notImplemented();
        };
        if (spaceToDistribute > 0)
            distributeSpaceBeyondLimits();

        // 2.5. For each affected track, if its item-incurred increase is larger than its planned
        // increase, set the planned increase to that value.
        for (auto trackIndex : spannedAffectedTracks)
            plannedIncreases[trackIndex] = std::max(plannedIncreases[trackIndex], itemIncurredIncreases[trackIndex]);
    }

    // 3. "Update the tracks' affected sizes by adding in the planned increase [...] (If the affected
    // size is an infinite growth limit, set it to the track's base size plus the planned increase.)"
    if (spaceDistributionTarget == ExtraSpaceDistributionTarget::BaseSizes) {
        for (auto trackIndex : affectedTracksIndexes)
            unsizedTracks[trackIndex].baseSize += plannedIncreases[trackIndex];
    } else {
        for (auto trackIndex : affectedTracksIndexes) {
            auto& track = unsizedTracks[trackIndex];
            auto plannedIncrease = plannedIncreases[trackIndex];
            if (track.growthLimit != LayoutUnit::max())
                track.growthLimit += plannedIncrease;
            else
                track.growthLimit = track.baseSize + plannedIncrease;
        }
    }
}

template<typename MinTrackSizingFunctionPredicate>
static TrackIndexes flexibleTracksMatching(const UnsizedTracks& unsizedTracks, MinTrackSizingFunctionPredicate&& minMatches)
{
    TrackIndexes trackIndexes;
    for (auto [trackIndex, track] : WTF::indexedRange(unsizedTracks)) {
        if (track.trackSizingFunction.max.isFlex() && minMatches(track.trackSizingFunction.min))
            trackIndexes.append(trackIndex);
    }
    return trackIndexes;
}

// https://drafts.csswg.org/css-grid-1/#algo-spanning-flex-items
// Increase sizes to accommodate spanning items crossing flexible tracks: repeat the previous step
// (https://drafts.csswg.org/css-grid-1/#algo-spanning-items) considering, together, all items that
// span a track with a flexible sizing function, while distributing space only to flexible tracks.
static void increaseSizesToAccommodateSpanningItemsCrossingFlexibleTracks(const ResolveIntrinsicTrackSizesContext& resolveIntrinsicTrackSizesContext,
    UnsizedTracks& unsizedTracks)
{
    auto gridItemSpanList = spannedLinesList(resolveIntrinsicTrackSizesContext.trackSizingItems);
    auto spanningItems = itemsSpanningFlexibleTracks(unsizedTracks, gridItemSpanList);
    if (spanningItems.isEmpty())
        return;

    auto scenario = resolveIntrinsicTrackSizesContext.axisConstraint.scenario();

    auto& trackSizingItems = resolveIntrinsicTrackSizesContext.trackSizingItems;
    auto& gridItemSizingFunctions = resolveIntrinsicTrackSizesContext.gridItemSizingFunctions;
    auto& trackSizingFunctions = resolveIntrinsicTrackSizesContext.trackSizingFunctionsList;
    auto gapSize = resolveIntrinsicTrackSizesContext.gapSize;

    auto minimumContributionsList = minimumContributions(trackSizingItems, spanningItems, gridItemSizingFunctions, trackSizingFunctions);
    Vector<std::optional<LayoutUnit>> fixedMaxTrackSizingFunctionSums;
    if (isSizedUnderMinOrMaxContentConstraint(resolveIntrinsicTrackSizesContext.axisConstraint)) {
        fixedMaxTrackSizingFunctionSums = spanningItems.map([&](size_t gridItemIndex) {
            return fixedMaxTrackSizingFunctionSum(trackSizingItems[gridItemIndex].spannedLines, unsizedTracks);
        });
    }

    // For intrinsic minimums: distribute extra space to the base sizes of tracks with an intrinsic
    // min track sizing function, to accommodate these items' minimum contributions. If the grid
    // container is being sized under a min-/max-content constraint, use the items' limited min-content
    // contributions instead.
    auto flexibleTracksWithIntrinsicMinimums = flexibleTracksMatching(unsizedTracks, [](const auto& trackSize) {
        return trackSize.isContentSized();
    });
    auto minimumSizeContributions = isSizedUnderMinOrMaxContentConstraint(resolveIntrinsicTrackSizesContext.axisConstraint)
        ? limitedContentContributions(minContentContributions(trackSizingItems, spanningItems, gridItemSizingFunctions), fixedMaxTrackSizingFunctionSums, minimumContributionsList)
        : minimumContributionsList;
    distributeExtraSpace(ExtraSpaceDistributionTarget::BaseSizes, flexibleTracksWithIntrinsicMinimums, minimumSizeContributions, spanningItems, gridItemSpanList, unsizedTracks, gapSize);

    // For content-based minimums: continue to distribute extra space to the base sizes of tracks with
    // a min track sizing function of min-content or max-content, to accommodate the items' min-content
    // contributions.
    auto minContentSizeContributions = minContentContributions(trackSizingItems, spanningItems, gridItemSizingFunctions);
    auto flexibleTracksWithContentBasedMinimums = flexibleTracksMatching(unsizedTracks, [](const auto& trackSize) {
        return trackSize.isLength() && (trackSize.length().isMinContent() || trackSize.length().isMaxContent());
    });
    distributeExtraSpace(ExtraSpaceDistributionTarget::BaseSizes, flexibleTracksWithContentBasedMinimums, minContentSizeContributions, spanningItems, gridItemSpanList, unsizedTracks, gapSize);

    // For max-content minimums: if the grid container is being sized under a max-content constraint
    if (scenario == AxisConstraint::FreeSpaceScenario::MaxContent) {
        // continue to distribute extra space to the base sizes of tracks with a min track sizing function
        // of auto or max-content, to accommodate the items' limited max-content contributions...
        auto flexibleTracksWithAutoOrMaxContentMinimums = flexibleTracksMatching(unsizedTracks, [](const auto& trackSize) {
            return trackSize.isAuto() || (trackSize.isLength() && trackSize.length().isMaxContent());
        });
        auto limitedMaxContentSizeContributions = limitedContentContributions(maxContentContributions(trackSizingItems, spanningItems, gridItemSizingFunctions), fixedMaxTrackSizingFunctionSums, minimumContributionsList);
        distributeExtraSpace(ExtraSpaceDistributionTarget::BaseSizes, flexibleTracksWithAutoOrMaxContentMinimums, limitedMaxContentSizeContributions, spanningItems, gridItemSpanList, unsizedTracks, gapSize);
    }
    // ...In all cases, distribute to tracks with a max-content min track sizing function
    // to accommodate the items' max-content contributions.
    auto flexibleTracksWithMaxContentMinimums = flexibleTracksMatching(unsizedTracks, [](const auto& trackSize) {
        return trackSize.isLength() && trackSize.length().isMaxContent();
    });
    auto maxContentSizeContributions = maxContentContributions(trackSizingItems, spanningItems, gridItemSizingFunctions);
    distributeExtraSpace(ExtraSpaceDistributionTarget::BaseSizes, flexibleTracksWithMaxContentMinimums, maxContentSizeContributions, spanningItems, gridItemSpanList, unsizedTracks, gapSize);

    // 4. If at this point any track's growth limit is now less than its base size, increase its
    //    growth limit to match its base size.
    //    Not applicable: a flexible track's growth limit is still infinite here (initialized from its
    //    <flex> max and untouched by the base-size passes above), so it can never be less than the
    //    base size. It is set to the base size later by the finite-growth-limit step below.
    ASSERT(std::ranges::all_of(unsizedTracks, [](const auto& track) {
        return !track.trackSizingFunction.max.isFlex() || track.baseSize <= track.growthLimit;
    }));

    // 5. For intrinsic maximums: distribute extra space to the growth limits of tracks with an
    //    intrinsic max track sizing function, to accommodate these items' min-content contributions.
    //    Not applicable: a flexible track's max track sizing function is <flex>, which is not an
    //    intrinsic max track sizing function, so no flexible track is affected.

    // 6. For max-content maximums: distribute extra space to the growth limits of tracks with a
    //    max-content max track sizing function, to accommodate these items' max-content contributions.
    //    Not applicable: a flexible track's max track sizing function is <flex>, not max-content, so no
    //    flexible track is affected.
}

// https://drafts.csswg.org/css-grid-1/#algo-content
static void resolveIntrinsicTrackSizes(const ResolveIntrinsicTrackSizesContext& resolveIntrinsicTrackSizesContext,
    UnsizedTracks& unsizedTracks)
{
    // 1. Shim baseline-aligned items so their intrinsic size contributions reflect their
    // baseline alignment.
    auto shimBaselineAlignedItems = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(shimBaselineAlignedItems);

    // 2. Size tracks to fit non-spanning items.
    sizeTracksToFitNonSpanningItems(resolveIntrinsicTrackSizesContext, unsizedTracks);

    // 3. Increase sizes to accommodate spanning items crossing content-sized tracks:
    // Next, consider the items with a span of 2 that do not span a track with a flexible
    // sizing function.
    auto increaseSizesToAccommodateSpanningItemsCrossingContentSizedTracks = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(increaseSizesToAccommodateSpanningItemsCrossingContentSizedTracks);

    // 4. Increase sizes to accommodate spanning items crossing flexible tracks:
    increaseSizesToAccommodateSpanningItemsCrossingFlexibleTracks(resolveIntrinsicTrackSizesContext, unsizedTracks);

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
static void maximizeTracks(UnsizedTracks& unsizedTracks, const AxisConstraint& axisConstraint, LayoutUnit gapSize)
{
    switch (axisConstraint.scenario()) {
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

        auto availableGridSpace = axisConstraint.scenario() == AxisConstraint::FreeSpaceScenario::Definite
            ? std::optional(axisConstraint.availableSpace()) : std::nullopt;
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

// https://www.w3.org/TR/css-grid-1/#algo-init
static UnsizedTracks initializeTrackSizes(const TrackSizingFunctionsList& trackSizingFunctionsList, LayoutUnit availableGridSpace)
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
                    return Style::evaluate<LayoutUnit>(*fixedValue, trackSizingFunctions.zoom);
                if (trackBreadthLength.isPercentOrCalculated())
                    return Style::evaluate<LayoutUnit>(trackBreadthLength, availableGridSpace, trackSizingFunctions.zoom);
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
                    return Style::evaluate<LayoutUnit>(*fixedValue, trackSizingFunctions.zoom);
                if (trackBreadthLength.isPercentOrCalculated())
                    return Style::evaluate<LayoutUnit>(trackBreadthLength, availableGridSpace, trackSizingFunctions.zoom);
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

static FlexTracks collectFlexTracks(const UnsizedTracks& unsizedTracks)
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

static bool hasFlexTracks(const UnsizedTracks& unsizedTracks)
{
    return std::ranges::any_of(unsizedTracks, [](auto& track) {
        return track.trackSizingFunction.max.isFlex();
    });
}

static double flexFactorSum(const FlexTracks& flexTracks)
{
    double total = 0.0;
    for (auto& track : flexTracks)
        total += track.flexFactor.value;
    return total;
}

// https://drafts.csswg.org/css-grid-1/#algo-find-fr-size
static double findSizeOfFr(const UnsizedTracks& tracks, const LayoutUnit availableSpace, const LayoutUnit gapSize)
{
    ASSERT(availableSpace >= 0_lu);

    // https://www.w3.org/TR/css-grid-1/#algo-terms
    // free space = available grid space - sum of base sizes - gutters.
    LayoutUnit totalGutters = GridLayoutUtils::totalGuttersSize(tracks.size(), gapSize);

    InflexibleTrackState state;
    FrSizeComponents components;
    LayoutUnit freeSpace;
    double flexFactorSum = 0;
    double hypotheticalFrSize = 0;

    while (true) {
        components = computeFRSizeComponents(tracks, state);

        // free space = available grid space - sum of base sizes - gutters.
        freeSpace = availableSpace - components.baseSizeSum - totalGutters;

        // If leftover space is negative, the non-flexible tracks have already exceeded the space to fill; flex tracks should be sized to zero.
        // https://www.w3.org/TR/css-grid-1/#grid-track-concept
        if (freeSpace <= 0_lu)
            return 0;

        // https://drafts.csswg.org/css-grid-1/#typedef-flex
        // Values between 0fr and 1fr have a somewhat special behavior: when the sum of the
        // flex factors is less than 1, they take up less than 100% of the leftover space.
        // Handle this by clamping flex factor sum to at least 1.0. Thus, a grid with a single
        // 0.5fr track will have a hypothetical fr size of leftoverSpace / 1.0, and the track will use
        // (0.5 * leftoverSpace) total.
        flexFactorSum = std::max(1.0, components.flexFactorSum);

        // Let the hypothetical fr size be the leftover space divided by the flex factor sum.
        hypotheticalFrSize = freeSpace / flexFactorSum;

        // If the hypothetical fr size is valid for all flexible tracks, return that size.
        // Otherwise, restart the algorithm treating the invalid tracks as inflexible.
        if (isValidFlexFactorUnit(tracks, hypotheticalFrSize, state))
            break;
    }

    return hypotheticalFrSize;
}

// "... if the flexible track's flex factor is greater than one,
// the result of dividing the track's base size by its flex factor; otherwise, the track's base size."
static double NODELETE flexFractionFromTrackBaseSize(const FlexTrack& flexTrack)
{
    if (flexTrack.flexFactor.value > 1.0)
        return flexTrack.baseSize / flexTrack.flexFactor.value;
    return flexTrack.baseSize.toDouble();
}

// Implements the final step of spec section 11.7:
// "For each flexible track, if the product of the used flex fraction and the track's
// flex factor is greater than the track's base size, set its base size to that product."
static void applyFlexFractionToTracks(UnsizedTracks& unsizedTracks, const FlexTracks& flexTracks, double flexFraction)
{
    // Track the difference between the ideal size (float, product of the used flex fraction and the track's flex factor)
    // and the snapped size (LayoutUnit, rounding down from ideal size).
    double lastTrackRoundingError = 0;
    for (const auto& flexTrack : flexTracks) {
        // The target size of the flex track is the product of the used flex fraction and the track's flex factor.
        // Carry the fraction lost when snapping the previous track so flooring errors don't accumulate.
        double targetSize = flexFraction * flexTrack.flexFactor.value + lastTrackRoundingError;

        // https://drafts.csswg.org/css-grid-2/#algo-flex-tracks
        // For each flexible track, if the product of the used flex fraction and the track’s flex factor
        // is greater than the track’s base size, set its base size to that product.
        LayoutUnit snappedSize { targetSize };
        LayoutUnit& baseSize = unsizedTracks[flexTrack.trackIndex].baseSize;
        if (snappedSize > baseSize)
            baseSize = snappedSize;

        lastTrackRoundingError = targetSize - snappedSize.toDouble();
        ASSERT(lastTrackRoundingError >= 0);
    }
}

// https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
// "If...sizing the grid container under a min-content constraint, the used flex fraction is zero."
static void expandFlexibleTracksForMinContent(UnsizedTracks&)
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
static void expandFlexibleTracksForMaxContent(UnsizedTracks& unsizedTracks, const FlexTracks& flexTracks,
    LayoutUnit gapSize, const TrackSizingItemList& trackSizingItems, const PlacedGridItemSpanList& gridItemSpanList,
    const GridItemSizingFunctions& gridItemSizingFunctions)
{
    // The used flex fraction is the maximum of:
    double usedFlexFraction = 0;

    // For each flexible track, if the flexible track's flex factor is greater than one,
    // the result of dividing the track's base size by its flex factor; otherwise, the track's base size.
    for (const auto& flexTrack : flexTracks)
        usedFlexFraction = std::max(usedFlexFraction, flexFractionFromTrackBaseSize(flexTrack));

    // For each grid item that crosses a flexible track, the result of finding the size of an fr
    // using all the grid tracks that the item crosses and a space to fill of the item's max-content contribution.
    for (auto [gridItemIndex, gridItemSpan] : indexedRange(gridItemSpanList)) {
        if (!itemCrossesFlexibleTrack(unsizedTracks, gridItemSpan))
            continue;

        auto maxContentContribution = gridItemSizingFunctions.maxContentContribution(trackSizingItems[gridItemIndex].gridItem, trackSizingItems[gridItemIndex].oppositeAxisConstraint);
        auto itemTracks = unsizedTracks.subspan(gridItemSpan.begin(), gridItemSpan.distance());
        double candidateFlexFraction = findSizeOfFr(itemTracks, maxContentContribution, gapSize);

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
static void expandFlexibleTracksForDefiniteLength(UnsizedTracks& unsizedTracks, const FlexTracks& flexTracks, std::optional<LayoutUnit> availableGridSpace, const LayoutUnit gapSize)
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
    double frSize = findSizeOfFr(unsizedTracks, availableGridSpace.value(), gapSize);

    // For each flexible track, if the product of the used flex fraction and the track's flex factor is greater than the track's base size, set its base size to that product.
    applyFlexFractionToTracks(unsizedTracks, flexTracks, frSize);
}

// https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
static void expandFlexibleTracks(UnsizedTracks& unsizedTracks, const AxisConstraint& axisConstraint,
    LayoutUnit gapSize, const TrackSizingItemList& trackSizingItems, const GridItemSizingFunctions& gridItemSizingFunctions)
{
    if (!hasFlexTracks(unsizedTracks))
        return;
    auto flexTracks = collectFlexTracks(unsizedTracks);
    double totalFlex = flexFactorSum(flexTracks);
    if (!totalFlex)
        return;

    auto freeSpaceScenario = axisConstraint.scenario();
    auto availableGridSpace = freeSpaceScenario == AxisConstraint::FreeSpaceScenario::Definite
        ? std::optional(axisConstraint.availableSpace()) : std::nullopt;

    // https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
    // "If...sizing the grid container under a min-content constraint, the used flex fraction is zero."
    if (freeSpaceScenario == AxisConstraint::FreeSpaceScenario::MinContent) {
        expandFlexibleTracksForMinContent(unsizedTracks);
        return;
    }

    // Otherwise, if sizing the grid container under a max-content constraint:
    if (freeSpaceScenario == AxisConstraint::FreeSpaceScenario::MaxContent) {
        ASSERT(!availableGridSpace);
        expandFlexibleTracksForMaxContent(unsizedTracks, flexTracks, gapSize, trackSizingItems, spannedLinesList(trackSizingItems), gridItemSizingFunctions);
        return;
    }

    ASSERT(freeSpaceScenario == AxisConstraint::FreeSpaceScenario::Definite);
    expandFlexibleTracksForDefiniteLength(unsizedTracks, flexTracks, availableGridSpace, gapSize);
}

// https://drafts.csswg.org/css-grid-1/#algo-stretch
// If the free space is indefinite, but the grid container has a definite min-width/height,
// use that size to calculate the free space for this step instead.
static std::optional<LayoutUnit> freeSpaceForStretchAutoTracks(const AxisConstraint& axisConstraint, std::optional<LayoutUnit> availableGridSpace, const UnsizedTracks& unsizedTracks, LayoutUnit gapSize)
{
    auto containerMinimumSize = axisConstraint.containerMinimumSize();
    switch (axisConstraint.scenario()) {
    case AxisConstraint::FreeSpaceScenario::Definite:
        return computeFreeSpace(availableGridSpace, unsizedTracks, gapSize);
    case AxisConstraint::FreeSpaceScenario::MinContent:
    case AxisConstraint::FreeSpaceScenario::MaxContent:
        if (containerMinimumSize)
            return computeFreeSpace(containerMinimumSize, unsizedTracks, gapSize);
        return computeFreeSpace(availableGridSpace, unsizedTracks, gapSize);
    }
    ASSERT_NOT_REACHED();
    return { };
}

// https://drafts.csswg.org/css-grid-1/#algo-track-sizing
TrackSizes TrackSizingAlgorithm::sizeTracks(const TrackSizingItemList& trackSizingItems, const TrackSizingFunctionsList& trackSizingFunctions,
    const AxisConstraint& axisConstraint, const GridItemSizingFunctions& gridItemSizingFunctions,
    LayoutUnit gapSize, const StyleContentAlignmentData& usedContentAlignment)
{
    auto freeSpaceScenario = axisConstraint.scenario();
    auto availableGridSpace = freeSpaceScenario == AxisConstraint::FreeSpaceScenario::Definite
        ? std::optional(axisConstraint.availableSpace()) : std::nullopt;

    // 1. Initialize Track Sizes
    // GridFormattingContext should have transformed a percentage track to auto if there was no
    // available space so it should not matter what the alternate value we pass in here is.
    auto unsizedTracks = initializeTrackSizes(trackSizingFunctions, availableGridSpace.value_or(0_lu));

    // 2. Resolve Intrinsic Track Sizes
    resolveIntrinsicTrackSizes(ResolveIntrinsicTrackSizesContext(trackSizingItems, gridItemSizingFunctions, trackSizingFunctions, axisConstraint, gapSize), unsizedTracks);

    // 3. Maximize Tracks
    maximizeTracks(unsizedTracks, axisConstraint, gapSize);

    // 4. Expand Flexible Tracks
    // https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
    expandFlexibleTracks(unsizedTracks, axisConstraint, gapSize, trackSizingItems, gridItemSizingFunctions);

    // https://drafts.csswg.org/css-grid-1/#algo-stretch
    // 5. Stretch ‘auto’ Tracks
    stretchAutoTracks(freeSpaceForStretchAutoTracks(axisConstraint, availableGridSpace, unsizedTracks, gapSize), unsizedTracks, usedContentAlignment);

    // Each track has a base size, a <length> which grows throughout the algorithm and
    // which will eventually be the track’s final size...
    return unsizedTracks.map([](const UnsizedTrack& unsizedTrack) {
        return unsizedTrack.baseSize;
    });
}

} // namespace Layout
} // namespace WebCore
