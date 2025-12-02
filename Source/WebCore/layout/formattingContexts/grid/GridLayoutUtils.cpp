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
#include "GridLayoutUtils.h"

namespace WebCore {
namespace Layout {
namespace GridLayoutUtils {

LayoutUnit computeGapValue(const Style::GapGutter& gap)
{
    if (gap.isNormal())
        return { };

    // Only handle fixed length gaps for now
    if (auto fixedGap = gap.tryFixed())
        return Style::evaluate<LayoutUnit>(*fixedGap, 0_lu, Style::ZoomNeeded { });

    ASSERT_NOT_REACHED();
    return { };
}

LayoutUnit usedInlineSizeForGridItem(const PlacedGridItem& placedGridItem)
{
    auto& inlineAxisSizes = placedGridItem.inlineAxisSizes();
    if (auto fixedInlineSize = inlineAxisSizes.preferredSize.tryFixed())
        return LayoutUnit { fixedInlineSize->resolveZoom(placedGridItem.usedZoom()) };

    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

LayoutUnit usedBlockSizeForGridItem(const PlacedGridItem& placedGridItem)
{
    auto& blockAxisSizes = placedGridItem.blockAxisSizes();
    if (auto fixedBlockSize = blockAxisSizes.preferredSize.tryFixed())
        return LayoutUnit { fixedBlockSize->resolveZoom(placedGridItem.usedZoom()) };

    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}


LayoutUnit computeGridLinePosition(size_t gridLineIndex, const TrackSizes& trackSizes, LayoutUnit gap)
{
    auto trackSizesBefore = trackSizes.subspan(0, gridLineIndex);
    auto sumOfTrackSizes = std::reduce(trackSizesBefore.begin(), trackSizesBefore.end());

    // For grid line i, there are i-1 gaps before it (between the i tracks)
    auto numberOfGaps = gridLineIndex > 0 ? gridLineIndex - 1 : 0;

    return sumOfTrackSizes + (numberOfGaps * gap);
}

}
}
}
