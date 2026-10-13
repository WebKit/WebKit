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
#include "GridSizer.h"

#include "GridLayoutState.h"
#include "GridLayoutUtils.h"
#include "NotImplemented.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "TrackSizingAlgorithm.h"
#include "TrackSizingFunctions.h"
#include <wtf/Vector.h>

namespace WebCore {
namespace Layout {

GridSizer::GridSizer(const GridFormattingContext& gridFormattingContext, const GridLayoutState& gridLayoutState)
    : m_gridFormattingContext(gridFormattingContext)
    , m_gridLayoutState(gridLayoutState)
{
}

// If calculating the layout of a grid item in this step depends on the available space in the block axis,
// assume the available space that it would have if any row with a definite max track sizing function
// had that size and all other rows were infinite.
static Vector<std::optional<LayoutUnit>> definiteRowSizesForFirstIterationColumnSizing(const TrackSizingFunctionsList& rowTrackSizingFunctionsList, std::optional<LayoutUnit> gridContainerInnerBlockSize)
{
    return rowTrackSizingFunctionsList.map([&gridContainerInnerBlockSize](const TrackSizingFunctions& trackSizingFunctions) {
        return WTF::switchOn(trackSizingFunctions.max,
            [&](const Style::GridTrackBreadth& maxTrackSizingFunction) {
                return WTF::switchOn(maxTrackSizingFunction,
                    [&](const Style::GridTrackBreadthLength::Fixed& fixedValue) -> std::optional<LayoutUnit> {
                        return Style::evaluate<LayoutUnit>(fixedValue, trackSizingFunctions.zoom);
                    },
                    [&](const Style::GridTrackBreadthLength::Percentage& percentageValue) -> std::optional<LayoutUnit> {
                        ASSERT_WITH_MESSAGE(gridContainerInnerBlockSize, "The formatting context should have transformed this track size to auto");
                        return Style::evaluate<LayoutUnit>(percentageValue, *gridContainerInnerBlockSize);
                    },
                    [&](const Style::GridTrackBreadth::Calc calculatedValue) -> std::optional<LayoutUnit> {
                        ASSERT_WITH_MESSAGE(gridContainerInnerBlockSize, "The formatting context should have transformed this track size to auto");
                        return Style::evaluate<LayoutUnit>(calculatedValue, *gridContainerInnerBlockSize, trackSizingFunctions.zoom);
                    },
                    [](const CSS::Keyword::MinContent&) -> std::optional<LayoutUnit> {
                        return { };
                    },
                    [](const CSS::Keyword::MaxContent&) -> std::optional<LayoutUnit> {
                        return { };
                    },
                    [](const CSS::Keyword::Auto&) -> std::optional<LayoutUnit> {
                        return { };
                    },
                    [](const Style::GridTrackBreadth::Flex&) -> std::optional<LayoutUnit> {
                        return { };
                    },
                    [](const auto&) -> std::optional<LayoutUnit> {
                        ASSERT_NOT_IMPLEMENTED_YET();
                        return { };
                    });
            },
            [](const Style::GridTrackSize::FitContent&) -> std::optional<LayoutUnit> {
                return { };
            });
    });
}

static std::optional<LayoutUnit> gridAreaBlockSizeForFirstIterationColumnSizing(const PlacedGridItem& gridItem, const Vector<std::optional<LayoutUnit>>& definiteRowSizes, LayoutUnit rowGap)
{
    auto spannedRowSizes = definiteRowSizes.subspan(gridItem.rowStartLine(), gridItem.rowEndLine() - gridItem.rowStartLine());
    if (std::ranges::any_of(spannedRowSizes, [](auto& rowSize) { return !rowSize; }))
        return { };

    auto sumOfRowSizes = std::accumulate(spannedRowSizes.begin(), spannedRowSizes.end(), 0_lu, [](LayoutUnit sum, auto& rowSize) {
        return sum + *rowSize;
    });
    return sumOfRowSizes + GridLayoutUtils::totalGuttersSize(spannedRowSizes.size(), rowGap);
}

// Runs the track sizing algorithm over the grid columns. Grid items whose inline-axis contribution
// depends on the available space in the block axis are given an estimate derived from the row track
// sizing functions, since the row sizes are not resolved yet.
TrackSizes GridSizer::sizeColumnTracks(const PlacedGridItems& placedGridItems, const TrackSizingFunctionsList& columnTrackSizingFunctionsList,
    const TrackSizingFunctionsList& rowTrackSizingFunctionsList) const
{
    auto& layoutState = this->layoutState();
    auto& layoutConstraints = layoutState.gridLayoutConstraints;

    std::optional<LayoutUnit> blockAxisAvailableSpace = layoutConstraints.blockAxis.scenario() == AxisConstraint::FreeSpaceScenario::Definite
        ? std::optional(layoutConstraints.blockAxis.availableSpace())
        : std::nullopt;
    auto definiteRowSizesForFirstColumnSizing = definiteRowSizesForFirstIterationColumnSizing(rowTrackSizingFunctionsList, blockAxisAvailableSpace);

    auto columnTrackSizingItems = placedGridItems.map([&](const PlacedGridItem& gridItem) -> TrackSizingItem {
        auto usedInlineBorderAndPadding = formattingContext().integrationUtils().borderAndPaddingForGridItem(gridItem.layoutBox(), { }).first;
        auto gridAreaBlockSize = gridAreaBlockSizeForFirstIterationColumnSizing(gridItem, definiteRowSizesForFirstColumnSizing, layoutState.usedRowGap);
        return { gridItem, gridItem.inlineAxisSizes(), usedInlineBorderAndPadding, GridLayoutUtils::usedMarginsForAxis(gridItem, gridItem.inlineAxisSizes()),
            { gridItem.columnStartLine(), gridItem.columnEndLine() }, gridAreaBlockSize };
    });

    return TrackSizingAlgorithm::sizeTracks(columnTrackSizingItems, columnTrackSizingFunctionsList,
        layoutConstraints.inlineAxis, GridItemSizingFunctions::inlineAxis(formattingContext().integrationUtils()),
        layoutState.usedColumnGap, layoutState.usedJustifyContent);
}

// Runs the track sizing algorithm over the grid rows. The given column sizes supply the inline-axis
// available space for grid items whose block-axis contribution needs it.
TrackSizes GridSizer::sizeRowTracks(const PlacedGridItems& placedGridItems, const TrackSizes& columnSizes,
    const TrackSizingFunctionsList& rowTrackSizingFunctionsList) const
{
    auto& layoutState = this->layoutState();

    auto rowTrackSizingItems = placedGridItems.map([&](const PlacedGridItem& gridItem) -> TrackSizingItem {
        auto gridAreaInlineSize = GridLayoutUtils::gridAreaDimensionSize(gridItem.columnStartLine(), gridItem.columnEndLine(), columnSizes, layoutState.usedColumnGap);
        auto usedBlockBorderAndPadding = formattingContext().integrationUtils().borderAndPaddingForGridItem(gridItem.layoutBox(), gridAreaInlineSize).second;
        return { gridItem, gridItem.blockAxisSizes(), usedBlockBorderAndPadding, GridLayoutUtils::usedMarginsForAxis(gridItem, gridItem.blockAxisSizes()),
            { gridItem.rowStartLine(), gridItem.rowEndLine() }, gridAreaInlineSize };
    });

    return TrackSizingAlgorithm::sizeTracks(rowTrackSizingItems, rowTrackSizingFunctionsList,
        layoutState.gridLayoutConstraints.blockAxis, GridItemSizingFunctions::blockAxis(formattingContext()),
        layoutState.usedRowGap, layoutState.usedAlignContent);
}

// https://drafts.csswg.org/css-grid-1/#algo-grid-sizing
// Steps 2-4 cannot change the column sizes, so a caller that only needs those can stop after
// step 1 and skip row sizing, grid-item layout, and alignment.
TrackSizes GridSizer::sizeColumnsOnlyFastPath(const PlacedGridItems& placedGridItems, const TrackSizingFunctionsList& columnTrackSizingFunctionsList,
    const TrackSizingFunctionsList& rowTrackSizingFunctionsList) const
{
    return sizeColumnTracks(placedGridItems, columnTrackSizingFunctionsList, rowTrackSizingFunctionsList);
}

// https://drafts.csswg.org/css-grid-1/#algo-grid-sizing
UsedTrackSizes GridSizer::sizeGrid(const PlacedGridItems& placedGridItems, const TrackSizingFunctionsList& columnTrackSizingFunctionsList,
    const TrackSizingFunctionsList& rowTrackSizingFunctionsList) const
{
    // 1. First, the track sizing algorithm is used to resolve the sizes of the grid columns.
    // If calculating the layout of a grid item in this step depends on the available space in the
    // block axis, assume the available space that it would have if any row with a definite max
    // track sizing function had that size and all other rows were infinite. If both the grid
    // container and all tracks have definite sizes, also apply align-content to find the final
    // effective size of any gaps spanned by such items; otherwise ignore the effects of track
    // alignment in this estimation.
    auto columnSizes = sizeColumnTracks(placedGridItems, columnTrackSizingFunctionsList, rowTrackSizingFunctionsList);

    // 2. Next, the track sizing algorithm resolves the sizes of the grid rows. To find the
    // inline-axis available space for any items whose block-axis size contributions require it,
    // use the grid column sizes calculated in the previous step.
    auto rowSizes = sizeRowTracks(placedGridItems, columnSizes, rowTrackSizingFunctionsList);

    // 3. Then, if the min-content contribution of any grid item has changed based on the
    // row sizes and alignment calculated in step 2, re-resolve the sizes of the grid
    // columns with the new min-content and max-content contributions (once only).
    auto resolveGridColumnSizesIfAnyMinContentContributionChanged = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(resolveGridColumnSizesIfAnyMinContentContributionChanged);

    // 4. Next, if the min-content contribution of any grid item has changed based on the
    // column sizes and alignment calculated in step 3, re-resolve the sizes of the grid
    // rows with the new min-content and max-content contributions (once only).
    auto resolveGridRowSizesIfAnyMinContentContributionChanged = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(resolveGridRowSizesIfAnyMinContentContributionChanged);

    return { columnSizes, rowSizes };
}

} // namespace Layout
} // namespace WebCore
