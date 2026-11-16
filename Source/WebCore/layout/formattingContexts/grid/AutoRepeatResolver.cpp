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
#include "AutoRepeatResolver.h"

#include "GridLayoutConstraints.h"
#include "StyleGridTemplateList.h"
#include "StyleGridTrackBreadth.h"
#include "StyleGridTrackSize.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StyleZoomPrimitives.h"

namespace WebCore {
namespace Layout {

enum class RepeatStrategy : bool {
    LargestWithoutOverflow,
    SmallestFulfillingSize
};

// "For this purpose, each track is treated as its max track sizing function if that is definite or else
// its min track sizing function if that is definite. If both are definite, floor the max track sizing
// function by the min track sizing function."
static std::optional<LayoutUnit> trackSizeForRepetitions(const Style::GridTrackSize& trackSize, LayoutUnit containerSizeForAutoRepeat, Style::ZoomFactor zoom)
{
    auto definiteBreadth = [&](const Style::GridTrackBreadth& breadth) -> std::optional<LayoutUnit> {
        if (!breadth.isLength() || breadth.isContentSized())
            return { };
        return Style::evaluate<LayoutUnit>(breadth.length(), containerSizeForAutoRepeat, zoom);
    };
    auto minBreadth = definiteBreadth(trackSize.minTrackBreadth());
    auto maxBreadth = definiteBreadth(trackSize.maxTrackBreadth());
    if (minBreadth && maxBreadth)
        return std::max(*minBreadth, *maxBreadth);
    return maxBreadth ? maxBreadth : minBreadth;
}

static LayoutUnit nonRepeatedTracksSpace(const Vector<Style::GridTrackSize>& nonRepeatedTrackSizes, LayoutUnit containerSizeForAutoRepeat, LayoutUnit usedGap, Style::ZoomFactor zoom)
{
    LayoutUnit nonRepeatedTracksSpaceSum;
    for (auto& trackSize : nonRepeatedTrackSizes) {
        // <auto-track-list> only allows <fixed-size> tracks alongside the auto-repeat, which always have a definite size.
        auto trackSizeForCount = trackSizeForRepetitions(trackSize, containerSizeForAutoRepeat, zoom);
        nonRepeatedTracksSpaceSum += *trackSizeForCount + usedGap;
    }
    return nonRepeatedTracksSpaceSum;
}

// containerSizeForAutoRepeat is the size of the grid container's content box that the repeat() is fit to,
// which is not necessarily the grid container's used size: when that size is indefinite, it is the definite
// maximum or minimum size instead. It is also the basis for resolving percentages in the track sizes.
static size_t repetitionsToFill(const Vector<Style::GridTrackSize>& autoRepeatTrackSizes, LayoutUnit containerSizeForAutoRepeat, LayoutUnit nonRepeatedTracksSpace, LayoutUnit usedGap, RepeatStrategy repeatStrategy, Style::ZoomFactor zoom)
{
    ASSERT(!autoRepeatTrackSizes.isEmpty());

    LayoutUnit autoRepeatTrackSizeSum;
    for (auto& autoRepeatTrackSize : autoRepeatTrackSizes) {
        auto trackSize = trackSizeForRepetitions(autoRepeatTrackSize, containerSizeForAutoRepeat, zoom);
        // "If neither are definite, the number of repetitions is one."
        if (!trackSize)
            return 1;
        autoRepeatTrackSizeSum += *trackSize;
    }
    // "For the purpose of finding the number of auto-repeated tracks, the UA must floor the track size to a
    // UA-specified value to avoid division by zero. It is suggested that this floor be 1px."
    autoRepeatTrackSizeSum = std::max(1_lu, autoRepeatTrackSizeSum);

    // Every track, repeated or not, is counted as its size plus the gutter after it. The grid has no gutter
    // after its last track, so add that one gutter back to the space before dividing it into repetitions.
    auto sizeOfRepetition = autoRepeatTrackSizeSum + usedGap * autoRepeatTrackSizes.size();
    auto spaceForRepetitions = containerSizeForAutoRepeat - nonRepeatedTracksSpace + usedGap;
    // "if any number of repetitions would overflow, then 1 repetition."
    if (spaceForRepetitions <= sizeOfRepetition)
        return 1;

    size_t repetitions = (spaceForRepetitions / sizeOfRepetition).toUnsigned();
    // Fulfilling the size needs one more repetition when the ones that fit leave space unfilled.
    if (repeatStrategy == RepeatStrategy::SmallestFulfillingSize && spaceForRepetitions > sizeOfRepetition * repetitions)
        ++repetitions;
    return repetitions;
}

// https://drafts.csswg.org/css-grid-1/#auto-repeat
size_t AutoRepeatResolver::resolveRepetitions(const Style::GridTemplateList& gridTemplateList, const AutoRepeatConstraint& autoRepeatConstraint, LayoutUnit usedGap, Style::ZoomFactor zoom)
{
    auto& gridTemplateListSizes = gridTemplateList.sizes;
    auto& autoRepeatSizes = gridTemplateList.autoRepeatSizes;
    // "if the grid container has a definite preferred size or maximum size in the relevant axis, then the
    // number of repetitions is the largest possible positive integer that does not cause the grid to overflow
    // the content box of its grid container taking gap into account"
    if (auto containerPreferredSize = autoRepeatConstraint.preferredSize)
        return repetitionsToFill(autoRepeatSizes, *containerPreferredSize, nonRepeatedTracksSpace(gridTemplateListSizes, *containerPreferredSize, usedGap, zoom), usedGap, RepeatStrategy::LargestWithoutOverflow, zoom);
    if (auto containerMaximumSize = autoRepeatConstraint.maximumSize) {
        // The minimum size wins when it is larger than the maximum size.
        auto containerSizeForAutoRepeat = std::max(*containerMaximumSize, autoRepeatConstraint.minimumSize.value_or(0_lu));
        return repetitionsToFill(autoRepeatSizes, containerSizeForAutoRepeat, nonRepeatedTracksSpace(gridTemplateListSizes, containerSizeForAutoRepeat, usedGap, zoom), usedGap, RepeatStrategy::LargestWithoutOverflow, zoom);
    }
    // "Otherwise, if the grid container has a definite minimum size in the relevant axis, the number of
    // repetitions is the smallest possible positive integer that fulfills that minimum requirement."
    if (auto containerMinimumSize = autoRepeatConstraint.minimumSize)
        return repetitionsToFill(autoRepeatSizes, *containerMinimumSize, nonRepeatedTracksSpace(gridTemplateListSizes, *containerMinimumSize, usedGap, zoom), usedGap, RepeatStrategy::SmallestFulfillingSize, zoom);
    // "Otherwise, the specified track list repeats only once."
    return 1;
}

} // namespace Layout
} // namespace WebCore
