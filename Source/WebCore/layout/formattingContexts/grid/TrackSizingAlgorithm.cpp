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

// https://drafts.csswg.org/css-grid-1/#algo-spanning-items
// "Next, consider the items with a span of 2 that do not span a track with a flexible sizing function."
static Vector<GridItemIndexes> spanGroupsNotCrossingFlexibleTracks(const UnsizedTracks& unsizedTracks, const PlacedGridItemSpanList& gridItemSpanList)
{
    GridItemIndexes itemsSortedByIncreasingSpan;
    for (auto [gridItemIndex, gridItemSpan] : WTF::indexedRange(gridItemSpanList)) {
        if (gridItemSpan.distance() > 1 && !itemCrossesFlexibleTrack(unsizedTracks, gridItemSpan))
            itemsSortedByIncreasingSpan.append(gridItemIndex);
    }
    std::ranges::stable_sort(itemsSortedByIncreasingSpan, { }, [&](size_t gridItemIndex) {
        return gridItemSpanList[gridItemIndex].distance();
    });

    // "Repeat incrementally for items with greater spans until all items have been considered."
    Vector<GridItemIndexes> spanGroups;
    size_t previousSpanSize = 0;
    for (auto gridItemIndex : itemsSortedByIncreasingSpan) {
        auto spanSize = gridItemSpanList[gridItemIndex].distance();
        if (spanSize != previousSpanSize)
            spanGroups.append({ });
        spanGroups.last().append(gridItemIndex);
        previousSpanSize = spanSize;
    }
    return spanGroups;
}

// https://drafts.csswg.org/css-grid-1/#algo-content
static Vector<GridItemIndexes> itemsToAccommodate(const UnsizedTracks& unsizedTracks, const PlacedGridItemSpanList& gridItemSpanList, ResolveIntrinsicTrackSizesPhase phase)
{
    if (phase == ResolveIntrinsicTrackSizesPhase::ContentSizedTracks) {
        // https://drafts.csswg.org/css-grid-1/#algo-spanning-items
        return spanGroupsNotCrossingFlexibleTracks(unsizedTracks, gridItemSpanList);
    }

    // https://drafts.csswg.org/css-grid-1/#algo-spanning-flex-items
    return { itemsSpanningFlexibleTracks(unsizedTracks, gridItemSpanList) };
}

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
        if (track.trackSizingFunction.max.isAuto())
            trackIndexes.append(trackIndex);
    }
    return trackIndexes;
}

static Vector<LayoutUnit> minContentContributions(const TrackSizingItemList& trackSizingItems, const GridItemIndexes& gridItemIndexes,
    const GridItemSizingFunctions& gridItemSizingFunctions)
{
    return gridItemIndexes.map([&](size_t gridItemIndex) {
        return gridItemSizingFunctions.minContentContribution(trackSizingItems[gridItemIndex].gridItem, trackSizingItems[gridItemIndex].oppositeAxisConstraint).value;
    });
}

static Vector<LayoutUnit> maxContentContributions(const TrackSizingItemList& trackSizingItems, const GridItemIndexes& gridItemIndexes,
    const GridItemSizingFunctions& gridItemSizingFunctions)
{
    return gridItemIndexes.map([&](size_t gridItemIndex) {
        return gridItemSizingFunctions.maxContentContribution(trackSizingItems[gridItemIndex].gridItem, trackSizingItems[gridItemIndex].oppositeAxisConstraint).value;
    });
}

// https://www.w3.org/TR/css-grid-2/#algo-single-span-items
// The minimum contribution of an item is the smallest outer size it can have. Specifically, if the
// item's computed preferred size behaves as auto or depends on the size of its containing block in
// the relevant axis, its minimum contribution is the outer size that would result from assuming the
// item's used minimum size as its preferred size; else the item's minimum contribution is its
// min-content contribution.
static LayoutUnit minimumContribution(const TrackSizingItemList& trackSizingItems, size_t gridItemIndex,
    const GridItemSizingFunctions& gridItemSizingFunctions, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit gapSize,
    const AxisConstraint& axisConstraint)
{
    auto& trackSizingItem = trackSizingItems[gridItemIndex];
    auto& preferredSize = trackSizingItem.computedSizes.preferredSize;
    if (GridLayoutUtils::preferredSizeBehavesAsAuto(preferredSize) || GridLayoutUtils::sizeDependsOnContainingBlockSize(preferredSize))
        return gridItemSizingFunctions.usedMinimumSize(trackSizingItem.gridItem, trackSizingFunctions, trackSizingItem.borderAndPadding, trackSizingItem.usedMargins, trackSizingItem.oppositeAxisConstraint, gapSize, axisConstraint, trackSizingItem.oppositeAxisGridAreaMaximumSize).value;
    return gridItemSizingFunctions.minContentContribution(trackSizingItem.gridItem, trackSizingItem.oppositeAxisConstraint).value;
}

static Vector<LayoutUnit> minimumContributions(const TrackSizingItemList& trackSizingItems,
    const GridItemIndexes& gridItemIndexes, const GridItemSizingFunctions& gridItemSizingFunctions, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit gapSize,
    const AxisConstraint& axisConstraint)
{
    return gridItemIndexes.map([&](size_t gridItemIndex) {
        return minimumContribution(trackSizingItems, gridItemIndex, gridItemSizingFunctions, trackSizingFunctions, gapSize, axisConstraint);
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
        auto fixedMaximum = WTF::switchOn(trackSizingFunction.max,
            [](const Style::GridTrackBreadth& breadth) -> std::optional<Style::GridTrackBreadthLength::Fixed> {
                if (!breadth.isLength())
                    return { };
                return breadth.length().tryFixed();
            },
            // The limit "could be the argument to a fit-content() track sizing function".
            [](const Style::GridTrackSize::FitContent& fitContent) -> std::optional<Style::GridTrackBreadthLength::Fixed> {
                return fitContent->value.tryFixed();
            });
        if (!fixedMaximum)
            return { };
        sum += Style::evaluate<LayoutUnit>(*fixedMaximum, trackSizingFunction.zoom);
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
        // A track with no such items keeps the base size and the growth limit it was initialized with.
        if (singleSpanningItemsIndexes.isEmpty())
            continue;

        auto& minimumTrackSizingFunction = track.trackSizingFunction.min;
        track.baseSize = WTF::switchOn(minimumTrackSizingFunction,
            [&](const CSS::Keyword::MinContent&) -> LayoutUnit {
                // If the track has a min-content min track sizing function, set its base size
                // to the maximum of the items’ min-content contributions, floored at zero.
                auto itemContributions = minContentContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions);
                ASSERT(itemContributions.size() == singleSpanningItemsIndexes.size());
                return std::max({ }, std::ranges::max(itemContributions));
            },
            [&](const CSS::Keyword::MaxContent&) -> LayoutUnit {
                // If the track has a max-content min track sizing function, set its base
                // size to the maximum of the items’ max-content contributions, floored at zero.
                auto itemContributions = maxContentContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions);
                ASSERT(itemContributions.size() == singleSpanningItemsIndexes.size());
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
                    auto itemMinimumContributions = minimumContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions, resolveIntrinsicTrackSizesContext.trackSizingFunctionsList, resolveIntrinsicTrackSizesContext.gapSize, resolveIntrinsicTrackSizesContext.axisConstraint);

                    auto limitedContributions = limitedContentContributions(minContentSizeContributions, fixedMaxTrackSizingFunctionSums, itemMinimumContributions);
                    return std::max({ }, std::ranges::max(limitedContributions));
                }
                // Otherwise, set the track’s base size to the maximum of its items’ minimum
                // contributions, floored at zero.
                auto contributions = minimumContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions, resolveIntrinsicTrackSizesContext.trackSizingFunctionsList, resolveIntrinsicTrackSizesContext.gapSize, resolveIntrinsicTrackSizesContext.axisConstraint);
                return std::max({ }, std::ranges::max(contributions));
            },
            // A <length-percentage> min track sizing function was already resolved to an absolute
            // length by Initialize Track Sizes.
            [&](const Style::GridTrackBreadth::Fixed&) -> LayoutUnit {
                return track.baseSize;
            },
            [&](const Style::GridTrackBreadth::Percentage&) -> LayoutUnit {
                return track.baseSize;
            },
            [&](const Style::GridTrackBreadth::Calc&) -> LayoutUnit {
                return track.baseSize;
            },
            [&](const auto&) -> LayoutUnit {
                ASSERT_NOT_REACHED();
                return { };
            }
        );

        track.growthLimit = WTF::switchOn(track.trackSizingFunction.max,
            [&](const Style::GridTrackBreadth& maximumTrackSizingFunction) -> LayoutUnit {
                return WTF::switchOn(maximumTrackSizingFunction,
                    [&](const CSS::Keyword::MinContent&) -> LayoutUnit {
                        // If the track has a min-content max track sizing function, set its growth
                        // limit to the maximum of the items’ min-content contributions.
                        auto itemContributions = minContentContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions);
                        ASSERT(itemContributions.size() == singleSpanningItemsIndexes.size());
                        return std::ranges::max(itemContributions);
                    },
                    [&](const CSS::Keyword::MaxContent&) -> LayoutUnit {
                        // If the track has a max-content max track sizing function, set its growth
                        // limit to the maximum of the items’ max-content contributions.
                        auto itemContributions = maxContentContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions);
                        return std::ranges::max(itemContributions);
                    },
                    [&](const CSS::Keyword::Auto&) -> LayoutUnit {
                        // Since it is not explicitly stated otherwise in the spec, auto is treated as max-content:
                        // If the track has a max-content max track sizing function, set its growth
                        // limit to the maximum of the items’ max-content contributions.
                        auto itemContributions = maxContentContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions);
                        return std::ranges::max(itemContributions);
                    },
                    // A <length-percentage> max track sizing function was already resolved to an absolute
                    // length by Initialize Track Sizes.
                    [&](const Style::GridTrackBreadth::Fixed&) -> LayoutUnit {
                        return track.growthLimit;
                    },
                    [&](const Style::GridTrackBreadth::Percentage&) -> LayoutUnit {
                        return track.growthLimit;
                    },
                    [&](const Style::GridTrackBreadth::Calc&) -> LayoutUnit {
                        return track.growthLimit;
                    },
                    [&](const auto&) -> LayoutUnit {
                        ASSERT_NOT_REACHED();
                        return { };
                    }
                );
            },
            // For fit-content() maximums, furthermore clamp this growth limit by the fit-content()
            // argument.
            [&](const Style::GridTrackSize::FitContent&) -> LayoutUnit {
                auto itemContributions = maxContentContributions(trackSizingItems, singleSpanningItemsIndexes, gridItemSizingFunctions);
                ASSERT(track.fitContentLimit);
                return std::min(std::ranges::max(itemContributions), *track.fitContentLimit);
            }
        );

        // In all cases, if a track’s growth limit is now less than its base size, increase the
        // growth limit to match the base size.
        track.ensureGrowthLimitIsBiggerThanBaseSize();
    }
}

// https://drafts.csswg.org/css-grid-1/#algo-spanning-items
static bool hasAffectedTrackSizingFunction(const UnsizedTrack& track, AffectedTrackSizingFunction affectedTrackSizingFunction)
{
    auto& minTrackSizingFunction = track.trackSizingFunction.min;
    auto& maxTrackSizingFunction = track.trackSizingFunction.max;
    switch (affectedTrackSizingFunction) {
    case AffectedTrackSizingFunction::IntrinsicMinimum:
        return minTrackSizingFunction.isContentSized();
    case AffectedTrackSizingFunction::ContentBasedMinimum:
        return minTrackSizingFunction.isLength() && (minTrackSizingFunction.length().isMinContent() || minTrackSizingFunction.length().isMaxContent());
    case AffectedTrackSizingFunction::AutoOrMaxContentMinimum:
        return minTrackSizingFunction.isAuto() || (minTrackSizingFunction.isLength() && minTrackSizingFunction.length().isMaxContent());
    case AffectedTrackSizingFunction::MaxContentMinimum:
        return minTrackSizingFunction.isLength() && minTrackSizingFunction.length().isMaxContent();
    case AffectedTrackSizingFunction::IntrinsicMaximum:
        return maxTrackSizingFunction.isContentSized();
    case AffectedTrackSizingFunction::MaxContentMaximum:
        // https://drafts.csswg.org/css-grid-1/#track-sizing
        // As a maximum, auto "represents the largest max-content contribution of the grid items
        // occupying the grid track", so it is accommodated together with max-content here.
        if (auto breadth = maxTrackSizingFunction.tryBreadth())
            return breadth->isAuto() || (breadth->isLength() && breadth->length().isMaxContent());
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

// Whether a track participates in the given phase: the content sized phase distributes space to
// tracks which are not flexible, the flexible phase only to flexible tracks.
// https://drafts.csswg.org/css-grid-1/#algo-spanning-flex-items
// "...distributing space only to flexible tracks (i.e. treating all other tracks as having a fixed
// sizing function)."
static bool isTrackAffectedForSpaceDistributionInPhase(const UnsizedTrack& track, ResolveIntrinsicTrackSizesPhase phase)
{
    switch (phase) {
    case ResolveIntrinsicTrackSizesPhase::ContentSizedTracks:
        return !track.trackSizingFunction.max.isFlex();
    case ResolveIntrinsicTrackSizesPhase::FlexibleTracks:
        return track.trackSizingFunction.max.isFlex();
    }
    ASSERT_NOT_REACHED();
    return false;
}

// Collects the tracks the given pass distributes space to.
static TrackIndexes affectedTracks(const UnsizedTracks& unsizedTracks, AffectedTrackSizingFunction affectedTrackSizingFunction, ResolveIntrinsicTrackSizesPhase phase)
{
    TrackIndexes trackIndexes;
    for (auto [trackIndex, track] : WTF::indexedRange(unsizedTracks)) {
        if (hasAffectedTrackSizingFunction(track, affectedTrackSizingFunction) && isTrackAffectedForSpaceDistributionInPhase(track, phase))
            trackIndexes.append(trackIndex);
    }
    return trackIndexes;
}

// https://drafts.csswg.org/css-grid-1/#infinitely-growable
// The marking is for item 6 of https://drafts.csswg.org/css-grid-1/#algo-spanning-items, so it is
// undone when this scope ends.
class ScopedInfinitelyGrowableTracks {
public:
    ScopedInfinitelyGrowableTracks(UnsizedTracks& unsizedTracks, const TrackIndexes& tracksWhoseGrowthLimitBecameFinite)
        : m_unsizedTracks(unsizedTracks)
        , m_markedTracks(tracksWhoseGrowthLimitBecameFinite)
    {
        for (auto trackIndex : m_markedTracks)
            m_unsizedTracks[trackIndex].infinitelyGrowable = true;
    }

    ~ScopedInfinitelyGrowableTracks()
    {
        for (auto trackIndex : m_markedTracks)
            m_unsizedTracks[trackIndex].infinitelyGrowable = false;
    }

private:
    UnsizedTracks& m_unsizedTracks;
    TrackIndexes m_markedTracks;
};

// https://drafts.csswg.org/css-grid-1/#algo-spanning-items
// https://drafts.csswg.org/css-grid-1/#algo-spanning-flex-items
static void resolveIntrinsicTrackSizesWithSpanningItems(const ResolveIntrinsicTrackSizesContext& resolveIntrinsicTrackSizesContext,
    UnsizedTracks& unsizedTracks, ResolveIntrinsicTrackSizesPhase phase, const GridItemIndexes& spanningItems, const PlacedGridItemSpanList& gridItemSpanList)
{
    auto scenario = resolveIntrinsicTrackSizesContext.axisConstraint.scenario();

    auto& trackSizingItems = resolveIntrinsicTrackSizesContext.trackSizingItems;
    auto& gridItemSizingFunctions = resolveIntrinsicTrackSizesContext.gridItemSizingFunctions;
    auto& trackSizingFunctions = resolveIntrinsicTrackSizesContext.trackSizingFunctionsList;
    auto gapSize = resolveIntrinsicTrackSizesContext.gapSize;

    auto minimumContributionsList = minimumContributions(trackSizingItems, spanningItems, gridItemSizingFunctions, trackSizingFunctions, gapSize, resolveIntrinsicTrackSizesContext.axisConstraint);
    Vector<std::optional<LayoutUnit>> fixedMaxTrackSizingFunctionSums;
    if (isSizedUnderMinOrMaxContentConstraint(resolveIntrinsicTrackSizesContext.axisConstraint)) {
        fixedMaxTrackSizingFunctionSums = spanningItems.map([&](size_t gridItemIndex) {
            return fixedMaxTrackSizingFunctionSum(trackSizingItems[gridItemIndex].spannedLines, unsizedTracks);
        });
    }

    // 1. For intrinsic minimums: distribute extra space to the base sizes of tracks with an intrinsic
    // min track sizing function, to accommodate these items' minimum contributions. If the grid
    // container is being sized under a min-/max-content constraint, use the items' limited min-content
    // contributions instead.
    auto tracksWithIntrinsicMinimums = affectedTracks(unsizedTracks, AffectedTrackSizingFunction::IntrinsicMinimum, phase);
    auto minimumSizeContributions = isSizedUnderMinOrMaxContentConstraint(resolveIntrinsicTrackSizesContext.axisConstraint)
        ? limitedContentContributions(minContentContributions(trackSizingItems, spanningItems, gridItemSizingFunctions), fixedMaxTrackSizingFunctionSums, minimumContributionsList)
        : minimumContributionsList;
    ExtraSpaceDistributor::distributeExtraSpace(ExtraSpaceDistributionTarget::BaseSizes, AffectedTrackSizingFunction::IntrinsicMinimum, tracksWithIntrinsicMinimums, minimumSizeContributions, spanningItems, gridItemSpanList, unsizedTracks, gapSize, phase);

    // 2. For content-based minimums: continue to distribute extra space to the base sizes of tracks with
    // a min track sizing function of min-content or max-content, to accommodate the items' min-content
    // contributions.
    auto minContentSizeContributions = minContentContributions(trackSizingItems, spanningItems, gridItemSizingFunctions);
    auto tracksWithContentBasedMinimums = affectedTracks(unsizedTracks, AffectedTrackSizingFunction::ContentBasedMinimum, phase);
    ExtraSpaceDistributor::distributeExtraSpace(ExtraSpaceDistributionTarget::BaseSizes, AffectedTrackSizingFunction::ContentBasedMinimum, tracksWithContentBasedMinimums, minContentSizeContributions, spanningItems, gridItemSpanList, unsizedTracks, gapSize, phase);

    // 3. For max-content minimums: if the grid container is being sized under a max-content constraint
    if (scenario == AxisConstraint::FreeSpaceScenario::MaxContent) {
        // continue to distribute extra space to the base sizes of tracks with a min track sizing function
        // of auto or max-content, to accommodate the items' limited max-content contributions...
        auto tracksWithAutoOrMaxContentMinimums = affectedTracks(unsizedTracks, AffectedTrackSizingFunction::AutoOrMaxContentMinimum, phase);
        auto limitedMaxContentSizeContributions = limitedContentContributions(maxContentContributions(trackSizingItems, spanningItems, gridItemSizingFunctions), fixedMaxTrackSizingFunctionSums, minimumContributionsList);
        ExtraSpaceDistributor::distributeExtraSpace(ExtraSpaceDistributionTarget::BaseSizes, AffectedTrackSizingFunction::AutoOrMaxContentMinimum, tracksWithAutoOrMaxContentMinimums, limitedMaxContentSizeContributions, spanningItems, gridItemSpanList, unsizedTracks, gapSize, phase);
    }
    // ...In all cases, distribute to tracks with a max-content min track sizing function
    // to accommodate the items' max-content contributions.
    auto tracksWithMaxContentMinimums = affectedTracks(unsizedTracks, AffectedTrackSizingFunction::MaxContentMinimum, phase);
    auto maxContentSizeContributions = maxContentContributions(trackSizingItems, spanningItems, gridItemSizingFunctions);
    ExtraSpaceDistributor::distributeExtraSpace(ExtraSpaceDistributionTarget::BaseSizes, AffectedTrackSizingFunction::MaxContentMinimum, tracksWithMaxContentMinimums, maxContentSizeContributions, spanningItems, gridItemSpanList, unsizedTracks, gapSize, phase);

    // 4. If at this point any track's growth limit is now less than its base size, increase its
    //    growth limit to match its base size.
    for (auto& track : unsizedTracks)
        track.ensureGrowthLimitIsBiggerThanBaseSize();

    // Scope tracksWhoseGrowthLimitBecameFinite to this block, since it is only used for the next step.
    {
        // 5. For intrinsic maximums: distribute extra space to the growth limits of tracks with an
        //    intrinsic max track sizing function, to accommodate these items' min-content contributions.
        auto tracksWithIntrinsicMaximums = affectedTracks(unsizedTracks, AffectedTrackSizingFunction::IntrinsicMaximum, phase);
        auto tracksWhoseGrowthLimitBecameFinite = ExtraSpaceDistributor::distributeExtraSpace(ExtraSpaceDistributionTarget::GrowthLimits, AffectedTrackSizingFunction::IntrinsicMaximum, tracksWithIntrinsicMaximums, minContentSizeContributions, spanningItems, gridItemSpanList, unsizedTracks, gapSize, phase);
        // "Mark any tracks whose growth limit changed from infinite to finite in this step as
        // infinitely growable for the next step."
        auto infinitelyGrowableTracks = ScopedInfinitelyGrowableTracks { unsizedTracks, tracksWhoseGrowthLimitBecameFinite };

        // 6. For max-content maximums: distribute extra space to the growth limits of tracks with a
        //    max-content max track sizing function, to accommodate these items' max-content contributions.
        auto tracksWithMaxContentMaximums = affectedTracks(unsizedTracks, AffectedTrackSizingFunction::MaxContentMaximum, phase);
        ExtraSpaceDistributor::distributeExtraSpace(ExtraSpaceDistributionTarget::GrowthLimits, AffectedTrackSizingFunction::MaxContentMaximum, tracksWithMaxContentMaximums, maxContentSizeContributions, spanningItems, gridItemSpanList, unsizedTracks, gapSize, phase);
    }
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

    auto gridItemSpanList = spannedLinesList(resolveIntrinsicTrackSizesContext.trackSizingItems);

    // 3. Increase sizes to accommodate spanning items crossing content-sized tracks:
    // Next, consider the items with a span of 2 that do not span a track with a flexible
    // sizing function.
    for (auto& spanningItems : itemsToAccommodate(unsizedTracks, gridItemSpanList, ResolveIntrinsicTrackSizesPhase::ContentSizedTracks))
        resolveIntrinsicTrackSizesWithSpanningItems(resolveIntrinsicTrackSizesContext, unsizedTracks, ResolveIntrinsicTrackSizesPhase::ContentSizedTracks, spanningItems, gridItemSpanList);

    // 4. Increase sizes to accommodate spanning items crossing flexible tracks:
    for (auto& spanningItems : itemsToAccommodate(unsizedTracks, gridItemSpanList, ResolveIntrinsicTrackSizesPhase::FlexibleTracks))
        resolveIntrinsicTrackSizesWithSpanningItems(resolveIntrinsicTrackSizesContext, unsizedTracks, ResolveIntrinsicTrackSizesPhase::FlexibleTracks, spanningItems, gridItemSpanList);

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
static std::optional<LayoutUnit> computeFreeSpace(std::optional<LayoutUnit> availableGridSpace, const UnsizedTracks& unsizedTracks, const BitVector& collapsedTracksList, LayoutUnit gapSize)
{
    if (!availableGridSpace)
        return { };

    auto sumOfBaseSizes = std::accumulate(unsizedTracks.begin(), unsizedTracks.end(), 0_lu, [](LayoutUnit sum, const UnsizedTrack& unsizedTrack) {
        return unsizedTrack.baseSize + sum;
    });
    auto guttersSize = GridLayoutUtils::totalGuttersSize(unsizedTracks.size(), collapsedTracksList, gapSize);

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
static void maximizeTracks(UnsizedTracks& unsizedTracks, const AxisConstraint& axisConstraint, const BitVector& collapsedTracksList, LayoutUnit gapSize)
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
        auto freeSpace = computeFreeSpace(availableGridSpace, unsizedTracks, collapsedTracksList, gapSize);
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
            freeSpace = computeFreeSpace(availableGridSpace, unsizedTracks, collapsedTracksList, gapSize);
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
            if (auto breadth = maxTrackSizingFunction.tryBreadth(); breadth && breadth->isLength()) {
                auto trackBreadthLength = breadth->length();
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

        auto unsizedTrack = UnsizedTrack { baseSize(), growthLimit(), trackSizingFunctions, trackSizingFunctions.fitContentLimit(availableGridSpace) };

        // In all cases, if the growth limit is less than the base size, increase the growth limit to
        // match the base size.
        unsizedTrack.ensureGrowthLimitIsBiggerThanBaseSize();
        return unsizedTrack;
    });
}

static FlexTracks collectFlexTracks(const UnsizedTracks& unsizedTracks)
{
    FlexTracks flexTracks;

    for (auto [trackIndex, track] : indexedRange(unsizedTracks)) {
        if (track.trackSizingFunction.max.isFlex()) {
            auto flexFactor = track.trackSizingFunction.max.flex();
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
static double findSizeOfFr(const UnsizedTracks& tracks, const LayoutUnit availableSpace, const BitVector& collapsedTracksList, const LayoutUnit gapSize)
{
    ASSERT(availableSpace >= 0_lu);

    // https://www.w3.org/TR/css-grid-1/#algo-terms
    // free space = available grid space - sum of base sizes - gutters.
    LayoutUnit totalGutters = GridLayoutUtils::totalGuttersSize(tracks.size(), collapsedTracksList, gapSize);

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
static double LEGACY_NODELETE flexFractionFromTrackBaseSize(const FlexTrack& flexTrack)
{
    if (flexTrack.flexFactor.value > 1.0)
        return flexTrack.baseSize / flexTrack.flexFactor.value;
    return flexTrack.baseSize.toDouble();
}

// https://drafts.csswg.org/css-grid-2/#algo-flex-tracks
// Implements the final step of the spec section:
// "For each flexible track, if the product of the used flex fraction and the track's
// flex factor is greater than the track's base size, set its base size to that product."
// The growth is returned instead of applied so that the size the grid would end up with can be
// measured before a flex fraction is committed to.
static Vector<LayoutUnit> flexTrackIncrementsForFlexFraction(const FlexTracks& flexTracks, double flexFraction)
{
    // Track the difference between the ideal size (float, product of the used flex fraction and the track's flex factor)
    // and the snapped size (LayoutUnit, rounding down from ideal size).
    double lastTrackRoundingError = 0;
    return flexTracks.map([&](const FlexTrack& flexTrack) -> LayoutUnit {
        // The target size of the flex track is the product of the used flex fraction and the track's flex factor.
        // Carry the fraction lost when snapping the previous track so flooring errors don't accumulate.
        double targetSize = flexFraction * flexTrack.flexFactor.value + lastTrackRoundingError;

        LayoutUnit snappedSize { targetSize };
        lastTrackRoundingError = targetSize - snappedSize.toDouble();
        ASSERT(lastTrackRoundingError >= 0);

        // Grow the track to the product of the used flex fraction and the track's flex factor,
        // or leave it alone when its base size is already larger than that product.
        return std::max(0_lu, snappedSize - flexTrack.baseSize);
    });
}

static void applyFlexFractionToTracks(UnsizedTracks& unsizedTracks, const FlexTracks& flexTracks, double flexFraction)
{
    auto flexTrackIncrements = flexTrackIncrementsForFlexFraction(flexTracks, flexFraction);

    for (auto [flexTrackIndex, flexTrack] : indexedRange(flexTracks))
        unsizedTracks[flexTrack.trackIndex].baseSize += flexTrackIncrements[flexTrackIndex];
}

// https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
// Otherwise, if sizing the grid container under a max-content constraint:
// The used flex fraction is the maximum of:
// * For each flexible track, if the flexible track's flex factor is greater than one,
//   the result of dividing the track's base size by its flex factor; otherwise, the track's base size.
// * For each grid item that crosses a flexible track, the result of finding the size of an fr
//   using all the grid tracks that the item crosses and a space to fill of the item's max-content contribution.
static double usedFlexFractionForMaxContent(const UnsizedTracks& unsizedTracks, const FlexTracks& flexTracks,
    const AxisConstraint& axisConstraint, LayoutUnit gapSize, const BitVector& collapsedTracksList, const TrackSizingItemList& trackSizingItems,
    const PlacedGridItemSpanList& gridItemSpanList, const GridItemSizingFunctions& gridItemSizingFunctions)
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

        auto maxContentContribution = gridItemSizingFunctions.maxContentContribution(trackSizingItems[gridItemIndex].gridItem, trackSizingItems[gridItemIndex].oppositeAxisConstraint).value;
        auto itemTracks = unsizedTracks.subspan(gridItemSpan.begin(), gridItemSpan.distance());
        double candidateFlexFraction = findSizeOfFr(itemTracks, maxContentContribution, { }, gapSize);

        usedFlexFraction = std::max(usedFlexFraction, candidateFlexFraction);
    }

    // https://drafts.csswg.org/css-grid-2/#algo-flex-tracks
    // "If using this flex fraction would cause the grid to be smaller than the grid container’s
    // min-width/height (or larger than the grid container’s max-width/height), then redo this step,
    // treating the free space as definite and the available grid space as equal to the grid container’s
    // content box size when it’s sized to its min-width/height (max-width/height)."
    auto flexTrackIncrements = flexTrackIncrementsForFlexFraction(flexTracks, usedFlexFraction);
    auto sumOfBaseSizes = std::accumulate(unsizedTracks.begin(), unsizedTracks.end(), 0_lu, [](LayoutUnit sum, const UnsizedTrack& unsizedTrack) {
        return unsizedTrack.baseSize + sum;
    });
    auto gridSizeForCandidateFlexFraction = sumOfBaseSizes + std::accumulate(flexTrackIncrements.begin(), flexTrackIncrements.end(), 0_lu)
        + GridLayoutUtils::totalGuttersSize(unsizedTracks.size(), collapsedTracksList, gapSize);

    auto containerMinimumSize = axisConstraint.containerMinimumSize();

    if (auto containerMaximumSize = axisConstraint.containerMaximumSize(); containerMaximumSize && gridSizeForCandidateFlexFraction > *containerMaximumSize) {
        // A minimum size larger than the maximum size wins over it.
        return findSizeOfFr(unsizedTracks, std::max(*containerMaximumSize, containerMinimumSize.value_or(0_lu)), collapsedTracksList, gapSize);
    }

    if (containerMinimumSize && gridSizeForCandidateFlexFraction < *containerMinimumSize)
        return findSizeOfFr(unsizedTracks, *containerMinimumSize, collapsedTracksList, gapSize);

    return usedFlexFraction;
}

// https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
// Otherwise, if the free space is a definite length:
// The used flex fraction is the result of finding the size of an fr using all of the
// grid tracks and a space to fill of the available grid space (minus gutters).
static double usedFlexFractionForDefiniteLength(const UnsizedTracks& unsizedTracks, LayoutUnit availableGridSpace, const BitVector& collapsedTracksList, const LayoutUnit gapSize)
{
    // https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
    // "If the free space is zero...the used flex fraction is zero."
    // If availableSpace is zero, free space must also be 0.
    if (availableGridSpace == 0_lu)
        return { };

    return findSizeOfFr(unsizedTracks, availableGridSpace, collapsedTracksList, gapSize);
}

// https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
static void expandFlexibleTracks(UnsizedTracks& unsizedTracks, const AxisConstraint& axisConstraint,
    LayoutUnit gapSize, const BitVector& collapsedTracksList, const TrackSizingItemList& trackSizingItems, const GridItemSizingFunctions& gridItemSizingFunctions)
{
    if (!hasFlexTracks(unsizedTracks))
        return;
    auto flexTracks = collectFlexTracks(unsizedTracks);
    double totalFlex = flexFactorSum(flexTracks);
    if (!totalFlex)
        return;

    // First, find the used flex fraction:
    auto usedFlexFraction = [&] -> double {
        switch (axisConstraint.scenario()) {
        // "If...sizing the grid container under a min-content constraint, the used flex fraction is zero."
        case AxisConstraint::FreeSpaceScenario::MinContent:
            return { };

        // Otherwise, if the free space is a definite length:
        case AxisConstraint::FreeSpaceScenario::Definite:
            return usedFlexFractionForDefiniteLength(unsizedTracks, axisConstraint.availableSpace(), collapsedTracksList, gapSize);

        // Otherwise, if sizing the grid container under a max-content constraint:
        case AxisConstraint::FreeSpaceScenario::MaxContent:
            return usedFlexFractionForMaxContent(unsizedTracks, flexTracks, axisConstraint, gapSize, collapsedTracksList, trackSizingItems, spannedLinesList(trackSizingItems), gridItemSizingFunctions);
        }

        ASSERT_NOT_REACHED();
        return { };
    }();

    // For each flexible track, if the product of the used flex fraction and the track's flex factor
    // is greater than the track's base size, set its base size to that product.
    applyFlexFractionToTracks(unsizedTracks, flexTracks, usedFlexFraction);
}

// https://drafts.csswg.org/css-grid-1/#algo-stretch
// If the free space is indefinite, but the grid container has a definite min-width/height,
// use that size to calculate the free space for this step instead.
static std::optional<LayoutUnit> freeSpaceForStretchAutoTracks(const AxisConstraint& axisConstraint, std::optional<LayoutUnit> availableGridSpace, const UnsizedTracks& unsizedTracks, const BitVector& collapsedTracksList, LayoutUnit gapSize)
{
    auto containerMinimumSize = axisConstraint.containerMinimumSize();
    switch (axisConstraint.scenario()) {
    case AxisConstraint::FreeSpaceScenario::Definite:
        return computeFreeSpace(availableGridSpace, unsizedTracks, collapsedTracksList, gapSize);
    case AxisConstraint::FreeSpaceScenario::MinContent:
    case AxisConstraint::FreeSpaceScenario::MaxContent:
        if (containerMinimumSize)
            return computeFreeSpace(containerMinimumSize, unsizedTracks, collapsedTracksList, gapSize);
        return computeFreeSpace(availableGridSpace, unsizedTracks, collapsedTracksList, gapSize);
    }
    ASSERT_NOT_REACHED();
    return { };
}

// https://drafts.csswg.org/css-grid-1/#algo-track-sizing
TrackSizes TrackSizingAlgorithm::sizeTracks(const TrackSizingItemList& trackSizingItems, const TrackSizingFunctionsList& trackSizingFunctions,
    const AxisConstraint& axisConstraint, const GridItemSizingFunctions& gridItemSizingFunctions,
    LayoutUnit gapSize, const BitVector& collapsedTracksList, const StyleContentAlignmentData& usedContentAlignment)
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
    maximizeTracks(unsizedTracks, axisConstraint, collapsedTracksList, gapSize);

    // 4. Expand Flexible Tracks
    // https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
    expandFlexibleTracks(unsizedTracks, axisConstraint, gapSize, collapsedTracksList, trackSizingItems, gridItemSizingFunctions);

    // https://drafts.csswg.org/css-grid-1/#algo-stretch
    // 5. Stretch ‘auto’ Tracks
    stretchAutoTracks(freeSpaceForStretchAutoTracks(axisConstraint, availableGridSpace, unsizedTracks, collapsedTracksList, gapSize), unsizedTracks, usedContentAlignment);

    // Each track has a base size, a <length> which grows throughout the algorithm and
    // which will eventually be the track’s final size...
    return unsizedTracks.map([](const UnsizedTrack& unsizedTrack) {
        return unsizedTrack.baseSize;
    });
}

} // namespace Layout
} // namespace WebCore
