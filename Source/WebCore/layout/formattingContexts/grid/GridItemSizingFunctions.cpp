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

#include "AxisConstraint.h"
#include "GridLayoutUtils.h"
#include "GridSizeTypes.h"
#include "PlacedGridItem.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "TrackSizingAlgorithm.h"
#include "TrackSizingFunctions.h"

namespace WebCore {
namespace Layout {

GridItemSizingFunctions::ContentContributionItem::ContentContributionItem(const TrackSizingItem& trackSizingItem)
    : gridItem(trackSizingItem.gridItem)
    , oppositeAxisConstraint(trackSizingItem.oppositeAxisConstraint)
{
}

GridItemSizingFunctions::UsedMinimumSizeItem::UsedMinimumSizeItem(const TrackSizingItem& trackSizingItem)
    : gridItem(trackSizingItem.gridItem)
    , borderAndPadding(trackSizingItem.borderAndPadding)
    , usedMargins(trackSizingItem.usedMargins)
    , oppositeAxisConstraint(trackSizingItem.oppositeAxisConstraint)
    , oppositeAxisGridAreaMaximumSize(trackSizingItem.oppositeAxisGridAreaMaximumSize)
{
}

GridItemSizingFunctions GridItemSizingFunctions::inlineAxis(const IntegrationUtils& integrationUtils)
{
    return {
        [&integrationUtils](const ContentContributionItem& contentContributionItem) {
            return GridLayoutUtils::inlineAxisMinContentContribution(contentContributionItem.gridItem, integrationUtils);
        },
        [&integrationUtils](const ContentContributionItem& contentContributionItem) {
            return GridLayoutUtils::inlineAxisMaxContentContribution(contentContributionItem.gridItem, integrationUtils);
        },
        // This mirrors GridLayoutUtils::inlineMinimumSize(), but is evaluated while track sizing is in
        // progress: the available space is not yet known, so it is absent for the automatic minimum size
        // and percentage/calc() minimum sizes resolve against a zero containing block size.
        [&integrationUtils](const UsedMinimumSizeItem& usedMinimumSizeItem, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit gapSize, const AxisConstraint& axisConstraint) {
            auto& gridItem = usedMinimumSizeItem.gridItem;
            auto borderAndPadding = usedMinimumSizeItem.borderAndPadding;
            auto gridAreaBlockSize = usedMinimumSizeItem.oppositeAxisConstraint;
            auto gridAreaMaximumBlockSize = usedMinimumSizeItem.oppositeAxisGridAreaMaximumSize;
            auto& inlineAxisSizes = gridItem.inlineAxisSizes();
            auto borderBoxSize = WTF::switchOn(inlineAxisSizes.minimumSize,
                [&](const Style::MinimumSize::Fixed& fixed) {
                    return GridLayoutUtils::borderBoxSizeForSpecifiedSize(gridItem.boxSizing(), Style::evaluate<LayoutUnit>(fixed, gridItem.usedZoom()), borderAndPadding);
                },
                [&](const Style::MinimumSize::Percentage& percentage) {
                    return GridLayoutUtils::borderBoxSizeForSpecifiedSize(gridItem.boxSizing(), Style::evaluate<LayoutUnit>(percentage, 0_lu), borderAndPadding);
                },
                [&](const Style::MinimumSize::Calc& calculated) {
                    return GridLayoutUtils::borderBoxSizeForSpecifiedSize(gridItem.boxSizing(), Style::evaluate<LayoutUnit>(calculated, 0_lu, gridItem.usedZoom()), borderAndPadding);
                },
                [&](const CSS::Keyword::Auto&) {
                    auto gridAreaMaximumInlineSize = GridLayoutUtils::gridAreaMaximumSize(gridItem.columnStartLine(), gridItem.columnEndLine(), trackSizingFunctions, gapSize, axisConstraint);
                    return GridLayoutUtils::automaticMinimumInlineSize(gridItem, borderAndPadding, trackSizingFunctions, { }, gridAreaMaximumInlineSize, integrationUtils, gridAreaBlockSize, gridAreaMaximumBlockSize);
                },
                [](const auto&) {
                    ASSERT_NOT_IMPLEMENTED_YET();
                    return BorderBoxSize::zeroSized();
                });

            // "its minimum contribution is the outer size that would result from assuming the item's
            // used minimum size as its preferred size"
            return MarginBoxSize { borderBoxSize, usedMinimumSizeItem.usedMargins.marginStart + usedMinimumSizeItem.usedMargins.marginEnd };
        }
    };
}

GridItemSizingFunctions GridItemSizingFunctions::blockAxis(const GridFormattingContext& formattingContext)
{
    return {
        // The row pass runs after the columns are sized, so the grid area inline size is always definite.
        [&formattingContext](const ContentContributionItem& contentContributionItem) {
            auto gridAreaInlineSize = contentContributionItem.oppositeAxisConstraint;
            ASSERT(gridAreaInlineSize);
            return GridLayoutUtils::blockAxisMinContentContribution(contentContributionItem.gridItem, *gridAreaInlineSize, formattingContext);
        },
        [&formattingContext](const ContentContributionItem& contentContributionItem) {
            auto gridAreaInlineSize = contentContributionItem.oppositeAxisConstraint;
            ASSERT(gridAreaInlineSize);
            return GridLayoutUtils::blockAxisMaxContentContribution(contentContributionItem.gridItem, *gridAreaInlineSize, formattingContext);
        },
        // This mirrors GridLayoutUtils::blockMinimumSize(), but is evaluated while track sizing is in
        // progress: the available space is not yet known, so it is absent for the automatic minimum size
        // and percentage/calc() minimum sizes resolve against a zero containing block size.
        [&formattingContext](const UsedMinimumSizeItem& usedMinimumSizeItem, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit gapSize, const AxisConstraint& axisConstraint) {
            auto& gridItem = usedMinimumSizeItem.gridItem;
            auto borderAndPadding = usedMinimumSizeItem.borderAndPadding;
            auto gridAreaInlineSize = usedMinimumSizeItem.oppositeAxisConstraint;
            auto gridAreaMaximumInlineSize = usedMinimumSizeItem.oppositeAxisGridAreaMaximumSize;
            ASSERT(gridAreaInlineSize);
            auto& blockAxisSizes = gridItem.blockAxisSizes();
            auto borderBoxSize = WTF::switchOn(blockAxisSizes.minimumSize,
                [&](const Style::MinimumSize::Fixed& fixed) {
                    return GridLayoutUtils::borderBoxSizeForSpecifiedSize(gridItem.boxSizing(), Style::evaluate<LayoutUnit>(fixed, gridItem.usedZoom()), borderAndPadding);
                },
                [&](const Style::MinimumSize::Percentage& percentage) {
                    return GridLayoutUtils::borderBoxSizeForSpecifiedSize(gridItem.boxSizing(), Style::evaluate<LayoutUnit>(percentage, 0_lu), borderAndPadding);
                },
                [&](const Style::MinimumSize::Calc& calculated) {
                    return GridLayoutUtils::borderBoxSizeForSpecifiedSize(gridItem.boxSizing(), Style::evaluate<LayoutUnit>(calculated, 0_lu, gridItem.usedZoom()), borderAndPadding);
                },
                [&](const CSS::Keyword::Auto&) {
                    auto gridAreaMaximumBlockSize = GridLayoutUtils::gridAreaMaximumSize(gridItem.rowStartLine(), gridItem.rowEndLine(), trackSizingFunctions, gapSize, axisConstraint);
                    return GridLayoutUtils::automaticMinimumBlockSize(gridItem, borderAndPadding, trackSizingFunctions, { }, gridAreaMaximumBlockSize, formattingContext, *gridAreaInlineSize, gridAreaMaximumInlineSize);
                },
                [](const auto&) {
                    ASSERT_NOT_IMPLEMENTED_YET();
                    return BorderBoxSize::zeroSized();
                });

            // "its minimum contribution is the outer size that would result from assuming the item's
            // used minimum size as its preferred size"
            return MarginBoxSize { borderBoxSize, usedMinimumSizeItem.usedMargins.marginStart + usedMinimumSizeItem.usedMargins.marginEnd };
        }
    };
}

} // namespace Layout
} // namespace WebCore
