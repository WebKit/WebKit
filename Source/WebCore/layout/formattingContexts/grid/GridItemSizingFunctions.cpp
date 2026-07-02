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

GridItemSizingFunctions GridItemSizingFunctions::inlineAxis(const IntegrationUtils& integrationUtils)
{
    return {
        [&integrationUtils](const PlacedGridItem& gridItem, LayoutUnit blockAxisConstraint) {
            return GridLayoutUtils::inlineAxisMinContentContribution(gridItem, blockAxisConstraint, integrationUtils);
        },
        [&integrationUtils](const PlacedGridItem& gridItem, LayoutUnit blockAxisConstraint) {
            return GridLayoutUtils::inlineAxisMaxContentContribution(gridItem, blockAxisConstraint, integrationUtils);
        },
        [&integrationUtils](const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit borderAndPadding, LayoutUnit availableSpace, LayoutUnit blockAxisConstraint) {
            UNUSED_PARAM(blockAxisConstraint);
            return GridLayoutUtils::inlineMinimumSize(gridItem, trackSizingFunctions, borderAndPadding, availableSpace, integrationUtils);
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
