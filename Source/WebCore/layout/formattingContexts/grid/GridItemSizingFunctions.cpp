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
#include "GridItemSizingFunctions.h"

#include "GridLayoutUtils.h"

namespace WebCore {
namespace Layout {

GridItemSizingFunctions GridItemSizingFunctions::inlineAxis(const IntegrationUtils& integrationUtils, const TrackSizingFunctionsList& columnTrackSizingFunctions, LayoutUnit columnGap)
{
    return {
        [&integrationUtils, &columnTrackSizingFunctions, columnGap](const PlacedGridItem& gridItem, LayoutUnit) {
            auto gridAreaInlineSize = GridLayoutUtils::definiteInlineGridAreaSize(gridItem, columnTrackSizingFunctions, columnGap);
            return GridLayoutUtils::inlineAxisMinContentContribution(gridItem, gridAreaInlineSize, integrationUtils);
        },
        [&integrationUtils, &columnTrackSizingFunctions, columnGap](const PlacedGridItem& gridItem, LayoutUnit) {
            auto gridAreaInlineSize = GridLayoutUtils::definiteInlineGridAreaSize(gridItem, columnTrackSizingFunctions, columnGap);
            return GridLayoutUtils::inlineAxisMaxContentContribution(gridItem, gridAreaInlineSize, integrationUtils);
        },
        [&integrationUtils, columnGap](const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit borderAndPadding, LayoutUnit availableSpace, LayoutUnit) {
            // The item's inline grid area is definite when it spans only definitely-sized columns; its
            // percentages then resolve against that size, and against zero (std::nullopt) otherwise.
            auto gridAreaInlineSize = GridLayoutUtils::definiteInlineGridAreaSize(gridItem, trackSizingFunctions, columnGap);
            return GridLayoutUtils::inlineMinimumSize(gridItem, trackSizingFunctions, borderAndPadding, availableSpace, gridAreaInlineSize, integrationUtils);
        }
    };
}

GridItemSizingFunctions GridItemSizingFunctions::blockAxis(const GridFormattingContext& formattingContext)
{
    return {
        [&formattingContext](const PlacedGridItem& gridItem, LayoutUnit inlineAxisConstraint) {
            return GridLayoutUtils::blockAxisMinContentContribution(gridItem, inlineAxisConstraint, formattingContext);
        },
        [&formattingContext](const PlacedGridItem& gridItem, LayoutUnit inlineAxisConstraint) {
            return GridLayoutUtils::blockAxisMaxContentContribution(gridItem, inlineAxisConstraint, formattingContext);
        },
        [&formattingContext](const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit borderAndPadding, LayoutUnit availableSpace, LayoutUnit inlineAxisConstraint) {
            return GridLayoutUtils::blockMinimumSize(gridItem, trackSizingFunctions, borderAndPadding, availableSpace, formattingContext, inlineAxisConstraint);
        }
    };
}

} // namespace Layout
} // namespace WebCore
