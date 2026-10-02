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
#include "ExplicitGridResolver.h"

#include "AutoRepeatResolver.h"
#include "GridLayoutConstraints.h"
#include "GridLayoutUtils.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {
namespace Layout {

ExplicitGridTrackSizes ExplicitGridResolver::resolve(const Style::ComputedStyle& gridContainerStyle, const GridLayoutConstraints& layoutConstraints, LayoutUnit usedColumnGap, LayoutUnit usedRowGap)
{
    auto zoom = gridContainerStyle.usedZoomForLength();
    return {
        resolveTrackSizes(gridContainerStyle.gridTemplateColumns(), layoutConstraints.inlineAxis, usedColumnGap, zoom),
        resolveTrackSizes(gridContainerStyle.gridTemplateRows(), layoutConstraints.blockAxis, usedRowGap, zoom)
    };
}

// Style keeps the auto-repeated tracks apart from the rest of the track list, so splice
// each repetition in at the position the repeat() was specified.
static Vector<Style::GridTrackSize> trackSizesWithAutoRepetitions(const Style::GridTemplateList& gridTemplateList, size_t repetitionCount)
{
    auto& gridTemplateListSizes = gridTemplateList.sizes;
    auto& autoRepeatSizes = gridTemplateList.autoRepeatSizes;
    auto insertionPoint = gridTemplateList.autoRepeatInsertionPoint;
    ASSERT(insertionPoint <= gridTemplateListSizes.size());

    Vector<Style::GridTrackSize> trackSizes;
    trackSizes.reserveInitialCapacity(gridTemplateListSizes.size() + repetitionCount * autoRepeatSizes.size());
    trackSizes.append(gridTemplateListSizes.span().first(insertionPoint));
    for ([[maybe_unused]] auto repetition : std::views::iota(0uz, repetitionCount))
        trackSizes.append(autoRepeatSizes.span());
    trackSizes.append(gridTemplateListSizes.span().subspan(insertionPoint));
    return trackSizes;
}

Vector<Style::GridTrackSize> ExplicitGridResolver::resolveTrackSizes(const Style::GridTemplateList& gridTemplateList, const AxisConstraint& axisConstraint, LayoutUnit usedGap, Style::ZoomFactor zoom)
{
    bool hasAutoFillRepeat = gridTemplateList.autoRepeatType == AutoRepeatType::Fill && !gridTemplateList.autoRepeatSizes.isEmpty();
    auto trackSizes = hasAutoFillRepeat ? trackSizesWithAutoRepetitions(gridTemplateList, AutoRepeatResolver::resolveRepetitions(gridTemplateList, axisConstraint, usedGap, zoom)) : gridTemplateList.sizes;

    // https://drafts.csswg.org/css-grid-1/#track-sizes
    // "If the size of the grid container depends on the size of its tracks, then the <percentage> must
    // be treated as auto, for the purpose of calculating the intrinsic sizes of the grid container".
    if (axisConstraint.scenario() != AxisConstraint::FreeSpaceScenario::Definite)
        return trackSizes.map(GridLayoutUtils::trackSizeWithPercentagesConvertedToAuto);
    return trackSizes;
}

} // namespace Layout
} // namespace WebCore
