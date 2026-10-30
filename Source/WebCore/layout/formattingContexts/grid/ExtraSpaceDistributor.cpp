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

#include "config.h"
#include "ExtraSpaceDistributor.h"

#include "GridLayoutUtils.h"
#include "TrackSizingAlgorithm.h"
#include <wtf/ZippedRange.h>

namespace WebCore {
namespace Layout {

// https://drafts.csswg.org/css-grid-1/#extra-space
// The result of one of steps 2.2–2.4 for a single item: the space left to distribute, and the
// item-incurred increases after this step's distribution.
struct ItemSpaceDistribution {
    LayoutUnit remainingSpace;
    // Keyed by track index.
    Vector<LayoutUnit> itemIncurredIncreases;
};

// Used for space distribution.
static Vector<size_t> indexesForUnfrozenAffectedTracks(const Vector<size_t>& spannedAffectedTracks,
    const UnsizedTracks& unsizedTracks, const Vector<LayoutUnit>& itemIncurredIncreases, ExtraSpaceDistributionTarget target, SpaceDistributionLimit limit)
{
    Vector<size_t> indexes;
    for (auto trackIndex : spannedAffectedTracks) {
        auto& track = unsizedTracks[trackIndex];
        if (track.affectedSize(target) + itemIncurredIncreases[trackIndex] < track.freezeLimit(target, limit))
            indexes.append(trackIndex);
    }
    return indexes;
}

// Distributes space equally among trackIndexes in successive rounds, freezing tracks (per
// spaceDistributedToTrack, below) as needed, until space is exhausted or no tracks remain unfrozen.
static ItemSpaceDistribution distributeSpaceEquallyAmongTracks(LayoutUnit spaceToDistribute, const Vector<size_t>& trackIndexes,
    const UnsizedTracks& unsizedTracks, Vector<LayoutUnit>&& itemIncurredIncreases, ExtraSpaceDistributionTarget target, SpaceDistributionLimit limit)
{
    auto unfrozenTrackIndexes = indexesForUnfrozenAffectedTracks(trackIndexes, unsizedTracks, itemIncurredIncreases, target, limit);
    while (!unfrozenTrackIndexes.isEmpty() && spaceToDistribute > 0) {
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
            auto spaceDistributed = spaceToDistribute / tracksRemainingForDistributionCount;
            auto& track = unsizedTracks[trackIndex];
            auto spaceRemainingUntilLimit = track.freezeLimit(target, limit) - (track.affectedSize(target) + itemIncurredIncreases[trackIndex]);
            return std::min(spaceDistributed, spaceRemainingUntilLimit);
        };

        for (auto trackIndex : unfrozenTrackIndexes) {
            auto spaceDistributed = spaceDistributedToTrack(trackIndex);
            itemIncurredIncreases[trackIndex] += spaceDistributed;
            spaceToDistribute -= spaceDistributed;
            --tracksRemainingForDistributionCount;
        }

        unfrozenTrackIndexes = indexesForUnfrozenAffectedTracks(trackIndexes, unsizedTracks, itemIncurredIncreases, target, limit);
    }
    return { spaceToDistribute, WTF::move(itemIncurredIncreases) };
}

// https://drafts.csswg.org/css-grid-1/#algo-spanning-flex-items
static ItemSpaceDistribution distributeSpaceAmongFlexibleTracks(LayoutUnit spaceToDistribute, const TrackIndexes& spannedAffectedTracks,
    const UnsizedTracks& unsizedTracks, Vector<LayoutUnit>&& itemIncurredIncreases)
{
    if (spannedAffectedTracks.isEmpty())
        return { spaceToDistribute, WTF::move(itemIncurredIncreases) };

    auto flexFactor = [&](size_t trackIndex) {
        // Only flexible tracks are affected in this phase, per isTrackAffectedForSpaceDistributionInPhase.
        ASSERT(unsizedTracks[trackIndex].trackSizingFunction.max.isFlex());
        return unsizedTracks[trackIndex].trackSizingFunction.max.flex().value;
    };

    double flexFactorSum = 0;
    for (auto trackIndex : spannedAffectedTracks)
        flexFactorSum += flexFactor(trackIndex);

    // Track the difference between the ideal share (double, the track's fraction of the flex factor
    // sum) and the snapped share (LayoutUnit, rounding down from the ideal share).
    double lastTrackRoundingError = 0;
    auto distributeToTrack = [&](size_t trackIndex, double share) {
        // Carry the fraction lost when snapping the previous track so flooring errors don't accumulate.
        share += lastTrackRoundingError;

        LayoutUnit spaceDistributed { share };
        lastTrackRoundingError = share - spaceDistributed.toDouble();
        ASSERT(lastTrackRoundingError >= 0);

        itemIncurredIncreases[trackIndex] += spaceDistributed;
        spaceToDistribute -= spaceDistributed;
    };

    // "if the sum of the flexible sizing functions of all flexible tracks spanned by the item is
    // greater than or equal to one, distributing space to such tracks according to the ratios of
    // their flexible sizing functions rather than distributing space equally; and if the sum is less
    // than one, distributing that proportion of space according to the ratios of their flexible
    // sizing functions..."
    // A span of 0fr tracks has no ratios to distribute by, so all of its space is distributed equally below.
    if (flexFactorSum) {
        auto spaceDistributedByRatio = std::min(flexFactorSum, 1.0) * spaceToDistribute.toDouble();
        for (auto trackIndex : spannedAffectedTracks)
            distributeToTrack(trackIndex, spaceDistributedByRatio * flexFactor(trackIndex) / flexFactorSum);
    }

    // "...and the rest equally"
    if (flexFactorSum < 1) {
        auto equalShare = spaceToDistribute.toDouble() / spannedAffectedTracks.size();
        for (auto trackIndex : spannedAffectedTracks)
            distributeToTrack(trackIndex, equalShare);
    }
    // All space should be distributed to flexible tracks as they do not have growth limits.
    ASSERT(spaceToDistribute < LayoutUnit::epsilon());
    return { spaceToDistribute, WTF::move(itemIncurredIncreases) };
}

// https://drafts.csswg.org/css-grid-1/#extra-space
// 2.4: whether this track should be affected when distributing extra space beyond limits.
static bool LEGACY_NODELETE shouldTrackGrowBeyondGrowthLimits(const UnsizedTrack& track, AffectedTrackSizingFunction affectedTrackSizingFunction)
{
    auto& maxTrackSizingFunction = track.trackSizingFunction.max;
    switch (affectedTrackSizingFunction) {
    // "...when accommodating minimum contributions or accommodating min-content contributions into base
    // sizes: any affected track that happens to also have an intrinsic max track sizing function..."
    case AffectedTrackSizingFunction::IntrinsicMinimum:
    case AffectedTrackSizingFunction::ContentBasedMinimum:
        return maxTrackSizingFunction.isContentSized();
    // "...when accommodating max-content contributions into base sizes: any affected track that happens
    // to also have a max-content max track sizing function..."
    case AffectedTrackSizingFunction::AutoOrMaxContentMinimum:
    case AffectedTrackSizingFunction::MaxContentMinimum:
        if (auto breadth = maxTrackSizingFunction.tryBreadth())
            return breadth->isAuto() || (breadth->isLength() && breadth->length().isMaxContent());
        return true;
    // "...when accommodating any contribution into growth limits: any affected track that has an
    // intrinsic max track sizing function."
    case AffectedTrackSizingFunction::IntrinsicMaximum:
    case AffectedTrackSizingFunction::MaxContentMaximum:
        return maxTrackSizingFunction.isContentSized();
    }
    ASSERT_NOT_REACHED();
    return false;
}

// https://drafts.csswg.org/css-grid-1/#extra-space
static Vector<size_t> tracksToGrowBeyondGrowthLimits(const Vector<size_t>& spannedAffectedTracks,
    const UnsizedTracks& unsizedTracks, AffectedTrackSizingFunction affectedTrackSizingFunction, ExtraSpaceDistributionTarget spaceDistributionTarget)
{
    Vector<size_t> trackIndexes;
    for (auto trackIndex : spannedAffectedTracks) {
        if (shouldTrackGrowBeyondGrowthLimits(unsizedTracks[trackIndex], affectedTrackSizingFunction))
            trackIndexes.append(trackIndex);
    }

    // "...if there are no such tracks, then all affected tracks." Only the base sizes bullets specify
    // this fallback; the growth limits bullet does not.
    if (trackIndexes.isEmpty() && spaceDistributionTarget == ExtraSpaceDistributionTarget::BaseSizes)
        return spannedAffectedTracks;
    return trackIndexes;
}

// https://drafts.csswg.org/css-grid-1/#extra-space
// Some tracks may need their growth limit changed from infinite to finite while distributing to
// intrinsic maximums.
TrackIndexes ExtraSpaceDistributor::distributeExtraSpace(ExtraSpaceDistributionTarget spaceDistributionTarget, AffectedTrackSizingFunction affectedTrackSizingFunction,
    const TrackIndexes& affectedTracksIndexes, const Vector<LayoutUnit>& sizeContributions, const GridItemIndexes& accommodatedItemsIndexes,
    const PlacedGridItemSpanList& gridItemSpanList, UnsizedTracks& unsizedTracks, LayoutUnit gapSize, ResolveIntrinsicTrackSizesPhase phase)
{
    ASSERT(accommodatedItemsIndexes.size() == sizeContributions.size());

    // 1. Maintain separately for each affected track a planned increase, initially set to 0. The
    // vector is keyed by track index. Non-affected tracks that receive space in 2.3 also get a
    // planned increase, per https://github.com/w3c/csswg-drafts/issues/3648.
    Vector<LayoutUnit> plannedIncreases(unsizedTracks.size());

    TrackIndexes nonAffectedTracksIndexes;

    // 2. For each accommodated item...
    for (auto [contributionIndex, gridItemIndex] : WTF::indexedRange(accommodatedItemsIndexes)) {
        // considering only tracks the item spans:
        auto itemSpan = gridItemSpanList[gridItemIndex];

        // Partition the spanned tracks into the affected tracks, which receive space in 2.2, and
        // the non-affected tracks, which can only receive space in 2.3.
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
        auto spannedGutters = GridLayoutUtils::totalGuttersSize(itemSpan.distance(), { }, gapSize);
        auto initialSpaceToDistribute = std::max(0_lu, sizeContributions[contributionIndex] - spannedSizes - spannedGutters);
        if (!initialSpaceToDistribute)
            continue;

        // 2.2. Distribute space up to limits:
        auto spaceDistributionUpToLimits = [&] {
            if (phase == ResolveIntrinsicTrackSizesPhase::FlexibleTracks)
                return distributeSpaceAmongFlexibleTracks(initialSpaceToDistribute, spannedAffectedTracks, unsizedTracks, Vector<LayoutUnit>(unsizedTracks.size()));
            return distributeSpaceEquallyAmongTracks(initialSpaceToDistribute, spannedAffectedTracks, unsizedTracks, Vector<LayoutUnit>(unsizedTracks.size()), spaceDistributionTarget, SpaceDistributionLimit::UpToGrowthLimit);
        };
        auto distribution = spaceDistributionUpToLimits();

        // https://drafts.csswg.org/css-grid-1/#extra-space
        // 2.3. "Distribute space to non-affected tracks: If extra space remains at this point, and
        // the item spans both affected tracks and non-affected tracks, distribute space as for the
        // previous step, but into the non-affected tracks instead."
        if (distribution.remainingSpace > 0 && !spannedAffectedTracks.isEmpty() && !spannedNonAffectedTracks.isEmpty()) {
            distribution = distributeSpaceEquallyAmongTracks(distribution.remainingSpace, spannedNonAffectedTracks, unsizedTracks, WTF::move(distribution.itemIncurredIncreases), spaceDistributionTarget, SpaceDistributionLimit::UpToGrowthLimit);
            // Several items can span the same non-affected track, so only record it once.
            for (auto trackIndex : spannedNonAffectedTracks) {
                if (distribution.itemIncurredIncreases[trackIndex])
                    nonAffectedTracksIndexes.appendIfNotContains(trackIndex);
            }
        }

        // 2.4. Distribute space beyond limits: if extra space still remains, unfreeze and continue
        // distributing it to the item-incurred increases.
        if (distribution.remainingSpace > 0)
            distribution = distributeSpaceEquallyAmongTracks(distribution.remainingSpace, tracksToGrowBeyondGrowthLimits(spannedAffectedTracks, unsizedTracks, affectedTrackSizingFunction, spaceDistributionTarget), unsizedTracks, WTF::move(distribution.itemIncurredIncreases), spaceDistributionTarget, SpaceDistributionLimit::BeyondGrowthLimit);

        // 2.5. For each affected track, if its item-incurred increase is larger than its planned
        // increase, set the planned increase to that value. This covers every spanned track, since
        // non-affected tracks can also have received space in 2.3.
        for (size_t trackIndex = itemSpan.begin(); trackIndex < itemSpan.end(); ++trackIndex)
            plannedIncreases[trackIndex] = std::max(plannedIncreases[trackIndex], distribution.itemIncurredIncreases[trackIndex]);
    }

    // 3. "Update the tracks' affected sizes by adding in the planned increase [...] (If the affected
    // size is an infinite growth limit, set it to the track's base size plus the planned increase.)"
    if (spaceDistributionTarget == ExtraSpaceDistributionTarget::BaseSizes) {
        for (auto trackIndex : affectedTracksIndexes)
            unsizedTracks[trackIndex].baseSize += plannedIncreases[trackIndex];
        for (auto trackIndex : nonAffectedTracksIndexes)
            unsizedTracks[trackIndex].baseSize += plannedIncreases[trackIndex];
        return { };
    }

    ASSERT(spaceDistributionTarget == ExtraSpaceDistributionTarget::GrowthLimits);
    TrackIndexes tracksWhoseGrowthLimitBecameFinite;
    auto updateGrowthLimit = [&](size_t trackIndex) {
        auto& track = unsizedTracks[trackIndex];
        auto plannedIncrease = plannedIncreases[trackIndex];
        if (track.growthLimit != LayoutUnit::max())
            track.growthLimit += plannedIncrease;
        else {
            ASSERT(track.growthLimit == LayoutUnit::max());
            track.growthLimit = track.baseSize + plannedIncrease;
            if (affectedTrackSizingFunction == AffectedTrackSizingFunction::IntrinsicMaximum)
                tracksWhoseGrowthLimitBecameFinite.append(trackIndex);
        }
    };
    for (auto trackIndex : affectedTracksIndexes)
        updateGrowthLimit(trackIndex);
    for (auto trackIndex : nonAffectedTracksIndexes)
        updateGrowthLimit(trackIndex);
    if (affectedTrackSizingFunction != AffectedTrackSizingFunction::IntrinsicMaximum)
        ASSERT(tracksWhoseGrowthLimitBecameFinite.isEmpty());
    return tracksWhoseGrowthLimitBecameFinite;
}

} // namespace Layout
} // namespace WebCore
