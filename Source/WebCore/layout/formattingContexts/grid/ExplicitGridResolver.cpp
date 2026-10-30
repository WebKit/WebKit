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

ExplicitGridTrackSizes ExplicitGridResolver::resolve(const Style::ComputedStyle& gridContainerStyle, const AutoRepeatConstraint& inlineAxisAutoRepeatConstraint, const AutoRepeatConstraint& blockAxisAutoRepeatConstraint, LayoutUnit usedColumnGap, LayoutUnit usedRowGap)
{
    auto zoom = gridContainerStyle.usedZoomForLength();
    auto resolvedColumns = resolveTrackSizes(gridContainerStyle.gridTemplateColumns(), inlineAxisAutoRepeatConstraint, usedColumnGap, zoom);
    auto resolvedRows = resolveTrackSizes(gridContainerStyle.gridTemplateRows(), blockAxisAutoRepeatConstraint, usedRowGap, zoom);
    return { WTF::move(resolvedColumns.trackSizes), WTF::move(resolvedRows.trackSizes), resolvedColumns.autoRepeatTracksIndexes, resolvedRows.autoRepeatTracksIndexes };
}

// Style keeps the auto-repeated tracks apart from the rest of the track list, so splice
// the auto-repeated tracks in at the position the repeat() was specified.
static Vector<Style::GridTrackSize> trackSizesWithAutoRepetitions(const Style::GridTemplateList& gridTemplateList, size_t autoRepeatTracksCount)
{
    auto& gridTemplateListSizes = gridTemplateList.sizes;
    auto& autoRepeatSizes = gridTemplateList.autoRepeatSizes;
    auto insertionPoint = gridTemplateList.autoRepeatInsertionPoint;
    ASSERT(insertionPoint <= gridTemplateListSizes.size());

    Vector<Style::GridTrackSize> trackSizes;
    trackSizes.reserveInitialCapacity(gridTemplateListSizes.size() + autoRepeatTracksCount);
    trackSizes.append(gridTemplateListSizes.span().first(insertionPoint));
    for (auto autoRepeatTrackIndex : std::views::iota(0uz, autoRepeatTracksCount))
        trackSizes.append(autoRepeatSizes[autoRepeatTrackIndex % autoRepeatSizes.size()]);
    trackSizes.append(gridTemplateListSizes.span().subspan(insertionPoint));

    // The tracks after the repeat() are dropped too once the auto-repeated tracks reach the limit.
    trackSizes.shrink(std::min<size_t>(trackSizes.size(), Style::GridPosition::max()));
    return trackSizes;
}

ExplicitGridResolver::ResolveTrackSizesResult ExplicitGridResolver::resolveTrackSizes(const Style::GridTemplateList& gridTemplateList, const AutoRepeatConstraint& autoRepeatConstraint, LayoutUnit usedGap, Style::ZoomFactor zoom)
{
    auto& autoRepeatSizes = gridTemplateList.autoRepeatSizes;
    if (gridTemplateList.autoRepeatType == AutoRepeatType::None || autoRepeatSizes.isEmpty())
        return { gridTemplateList.sizes, { } };

    size_t maximumTracksCount = Style::GridPosition::max();
    auto insertionPoint = gridTemplateList.autoRepeatInsertionPoint;
    auto repetitionCount = AutoRepeatResolver::resolveRepetitions(gridTemplateList, autoRepeatConstraint, usedGap, zoom);
    // The tracks before the repeat() can already reach the limit, which leaves no room for auto-repeated tracks.
    auto remainingTracksCount = insertionPoint < maximumTracksCount ? maximumTracksCount - insertionPoint : 0;
    auto autoRepeatTracksCount = std::min(repetitionCount * autoRepeatSizes.size(), remainingTracksCount);
    return { trackSizesWithAutoRepetitions(gridTemplateList, autoRepeatTracksCount), { insertionPoint, insertionPoint + autoRepeatTracksCount } };
}

} // namespace Layout
} // namespace WebCore
