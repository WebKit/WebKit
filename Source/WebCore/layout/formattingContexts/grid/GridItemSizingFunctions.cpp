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
#include "TrackSizingFunctions.h"

namespace WebCore {
namespace Layout {

// https://drafts.csswg.org/css-grid-2/#min-size-auto
// The maximum size of the grid area an item is placed in, as represented by the sum of its grid
// tracks' max track sizing functions plus any intervening fixed gutters. Absent unless every one of
// those tracks has a fixed max track sizing function, since the automatic minimum size is only
// clamped to the grid area in that case.
static std::optional<LayoutUnit> gridAreaMaximumSize(size_t startLine, size_t endLine, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit gapSize,
    const AxisConstraint& axisConstraint)
{
    LayoutUnit maximumSize;
    for (auto trackIndex : std::views::iota(startLine, endLine)) {
        auto& trackSizingFunction = trackSizingFunctions[trackIndex];
        if (!trackSizingFunction.max.isLength())
            return { };

        auto& maximumBreadth = trackSizingFunction.max.length();
        if (auto fixedMaximumBreadth = maximumBreadth.tryFixed()) {
            maximumSize += Style::evaluate<LayoutUnit>(*fixedMaximumBreadth, trackSizingFunction.zoom);
            continue;
        }

        if (!maximumBreadth.isPercentOrCalculated())
            return { };
        // When the constraint is indefinite, GridFormattingContext will transform any percent and
        // calc functions into auto.
        if (axisConstraint.scenario() != AxisConstraint::FreeSpaceScenario::Definite) {
            ASSERT_NOT_REACHED();
            return { };
        }
        maximumSize += Style::evaluate<LayoutUnit>(maximumBreadth, axisConstraint.availableSpace(), trackSizingFunction.zoom);
    }
    return maximumSize + GridLayoutUtils::totalGuttersSize(endLine - startLine, gapSize);
}

GridItemSizingFunctions GridItemSizingFunctions::inlineAxis(const IntegrationUtils& integrationUtils)
{
    // The opposite-axis constraint is part of the shared callback signature (blockAxis() uses it), but
    // inline-axis intrinsic sizes don't depend on it while grid items with a preferred aspect ratio are unsupported.
    return {
        [&integrationUtils](const PlacedGridItem& gridItem, LayoutUnit) {
            return GridLayoutUtils::inlineAxisMinContentContribution(gridItem, integrationUtils);
        },
        [&integrationUtils](const PlacedGridItem& gridItem, LayoutUnit) {
            return GridLayoutUtils::inlineAxisMaxContentContribution(gridItem, integrationUtils);
        },
        // This mirrors GridLayoutUtils::inlineMinimumSize(), but is evaluated while track sizing is in
        // progress: the available space is not yet known, so it is absent for the automatic minimum size
        // and percentage/calc() minimum sizes resolve against a zero containing block size.
        [&integrationUtils](const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit borderAndPadding, LayoutUnit, LayoutUnit gapSize, const AxisConstraint& axisConstraint) {
            auto& minimumSize = gridItem.inlineAxisSizes().minimumSize;
            return WTF::switchOn(minimumSize,
                [&](const Style::MinimumSize::Fixed& fixed) {
                    return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(fixed, gridItem.usedZoom()) }, borderAndPadding }.value;
                },
                [&](const Style::MinimumSize::Percentage& percentage) {
                    return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(percentage, 0_lu) }, borderAndPadding }.value;
                },
                [&](const Style::MinimumSize::Calc& calculated) {
                    return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(calculated, 0_lu, gridItem.usedZoom()) }, borderAndPadding }.value;
                },
                [&](const CSS::Keyword::Auto&) -> LayoutUnit {
                    auto gridAreaMaximumInlineSize = gridAreaMaximumSize(gridItem.columnStartLine(), gridItem.columnEndLine(), trackSizingFunctions, gapSize, axisConstraint);
                    return GridLayoutUtils::automaticMinimumInlineSize(gridItem, borderAndPadding, trackSizingFunctions, { }, gridAreaMaximumInlineSize, integrationUtils).value;
                },
                [](const auto&) -> LayoutUnit {
                    ASSERT_NOT_IMPLEMENTED_YET();
                    return { };
                });
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
        // This mirrors GridLayoutUtils::blockMinimumSize(), but is evaluated while track sizing is in
        // progress: the available space is not yet known, so it is absent for the automatic minimum size
        // and percentage/calc() minimum sizes resolve against a zero containing block size.
        [&formattingContext](const PlacedGridItem& gridItem, const TrackSizingFunctionsList& trackSizingFunctions, LayoutUnit borderAndPadding, LayoutUnit inlineAxisConstraint, LayoutUnit gapSize, const AxisConstraint& axisConstraint) {
            auto& minimumSize = gridItem.blockAxisSizes().minimumSize;
            return WTF::switchOn(minimumSize,
                [&](const Style::MinimumSize::Fixed& fixed) {
                    return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(fixed, gridItem.usedZoom()) }, borderAndPadding }.value;
                },
                [&](const Style::MinimumSize::Percentage& percentage) {
                    return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(percentage, 0_lu) }, borderAndPadding }.value;
                },
                [&](const Style::MinimumSize::Calc& calculated) {
                    return BorderBoxSize { ContentBoxSize { Style::evaluate<LayoutUnit>(calculated, 0_lu, gridItem.usedZoom()) }, borderAndPadding }.value;
                },
                [&](const CSS::Keyword::Auto&) -> LayoutUnit {
                    auto gridAreaMaximumBlockSize = gridAreaMaximumSize(gridItem.rowStartLine(), gridItem.rowEndLine(), trackSizingFunctions, gapSize, axisConstraint);
                    return GridLayoutUtils::automaticMinimumBlockSize(gridItem, borderAndPadding, trackSizingFunctions, { }, gridAreaMaximumBlockSize, formattingContext, inlineAxisConstraint).value;
                },
                [](const auto&) -> LayoutUnit {
                    ASSERT_NOT_IMPLEMENTED_YET();
                    return { };
                });
        }
    };
}

} // namespace Layout
} // namespace WebCore
