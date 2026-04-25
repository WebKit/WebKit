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
#include "GridLayout.h"

#include "GridAreaLines.h"
#include "GridItemRect.h"
#include "GridLayoutState.h"
#include "GridLayoutUtils.h"
#include "ImplicitGrid.h"
#include "RenderStyle+GettersInlines.h"
#include "LayoutBoxGeometry.h"
#include "LayoutElementBox.h"
#include "NotImplemented.h"
#include "PlacedGridItem.h"
#include "TrackSizingAlgorithm.h"
#include "TrackSizingFunctions.h"
#include "UnplacedGridItem.h"
#include "UsedTrackSizes.h"
#include <wtf/Range.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace Layout {

struct UsedMargins {
    LayoutUnit marginStart;
    LayoutUnit marginEnd;
};

struct UsedGridItemSizes {
    LayoutUnit inlineAxisSize;
    LayoutUnit blockAxisSize;
};

struct GridAreaSizes {
    Vector<LayoutUnit> inlineSizes;
    Vector<LayoutUnit> blockSizes;
};

GridLayout::GridLayout(const GridFormattingContext& gridFormattingContext)
    : m_gridFormattingContext(gridFormattingContext)
{
}

GridDimensions GridLayout::calculateInitialImplicitGridDimensions(const UnplacedGridItems& unplacedGridItems, size_t explicitColumnsCount, size_t explicitRowsCount)
{
    int minimumRowIndex = 0;
    int minimumColumnIndex = 0;
    int maximumRowIndex = static_cast<int>(explicitRowsCount);
    int maximumColumnIndex = static_cast<int>(explicitColumnsCount);

    auto updateGridBounds = [&](const UnplacedGridItem& item) {
        if (item.hasDefiniteRowPosition()) {
            auto [rowStart, rowEnd] = item.definiteRowStartEnd();
            minimumRowIndex = std::min(minimumRowIndex, rowStart);
            minimumRowIndex = std::min(minimumRowIndex, rowEnd);
            maximumRowIndex = std::max(maximumRowIndex, rowStart);
            maximumRowIndex = std::max(maximumRowIndex, rowEnd);
        }

        if (item.hasDefiniteColumnPosition()) {
            auto [columnStart, columnEnd] = item.definiteColumnStartEnd();
            minimumColumnIndex = std::min(minimumColumnIndex, columnStart);
            minimumColumnIndex = std::min(minimumColumnIndex, columnEnd);
            maximumColumnIndex = std::max(maximumColumnIndex, columnStart);
            maximumColumnIndex = std::max(maximumColumnIndex, columnEnd);
        }
    };

    for (const auto& item : unplacedGridItems.nonAutoPositionedItems)
        updateGridBounds(item);
    for (const auto& item : unplacedGridItems.definiteRowPositionedItems)
        updateGridBounds(item);

    size_t rowOffset = minimumRowIndex < 0 ? static_cast<size_t>(-minimumRowIndex) : 0;
    size_t columnOffset = minimumColumnIndex < 0 ? static_cast<size_t>(-minimumColumnIndex) : 0;

    return {
        rowOffset,
        columnOffset,
        static_cast<size_t>(maximumColumnIndex) + columnOffset,
        static_cast<size_t>(maximumRowIndex) + rowOffset
    };
}

ImplicitGrid GridLayout::constructInitialImplicitGrid(UnplacedGridItems& unplacedGridItems, size_t explicitColumnsCount, size_t explicitRowsCount)
{
    // Calculate grid dimensions (offsets and total size) for negative grid line positions
    auto initialDimensions = calculateInitialImplicitGridDimensions(
        unplacedGridItems, explicitColumnsCount, explicitRowsCount);

    // Normalize all grid item positions by applying the offsets
    for (auto& item : unplacedGridItems.nonAutoPositionedItems)
        item.applyGridOffsets(initialDimensions.rowOffset, initialDimensions.columnOffset);
    for (auto& item : unplacedGridItems.definiteRowPositionedItems)
        item.applyGridOffsets(initialDimensions.rowOffset, initialDimensions.columnOffset);
    for (auto& item : unplacedGridItems.autoPositionedItems)
        item.applyGridOffsets(initialDimensions.rowOffset, initialDimensions.columnOffset);

    ImplicitGrid implicitGrid(initialDimensions.totalColumns, initialDimensions.totalRows);
    // 3. Determine the columns in the implicit grid.
    // Spec: "If the largest column span among all the items without a definite column position
    // is larger than the width of the implicit grid, add columns to the end of the implicit grid
    // to accommodate that column span."
    implicitGrid.determineImplicitGridColumns(unplacedGridItems.autoPositionedItems);

    return implicitGrid;
}

// 8.5. Grid Item Placement Algorithm.
// https://drafts.csswg.org/css-grid-1/#auto-placement-algo
auto GridLayout::placeGridItems(UnplacedGridItems& unplacedGridItems, const Vector<Style::GridTrackSize>& gridTemplateColumnsTrackSizes,
    const Vector<Style::GridTrackSize>& gridTemplateRowsTrackSizes, GridAutoFlowOptions autoFlowOptions)
{
    struct Result {
        GridAreas gridAreas;
        size_t columnsCount;
        size_t rowsCount;
    };

    auto implicitGrid = constructInitialImplicitGrid(unplacedGridItems, gridTemplateColumnsTrackSizes.size(), gridTemplateRowsTrackSizes.size());

    // 1. Position anything that's not auto-positioned.
    for (auto& nonAutoPositionedItem : unplacedGridItems.nonAutoPositionedItems)
        implicitGrid.insertUnplacedGridItem(nonAutoPositionedItem);

    // 2. Process the items locked to a given row.
    for (auto& definiteRowPositionedItem : unplacedGridItems.definiteRowPositionedItems)
        implicitGrid.insertDefiniteRowItem(definiteRowPositionedItem, autoFlowOptions);

    if (!unplacedGridItems.autoPositionedItems.isEmpty()) {
        // 4. Process auto-positioned items
        implicitGrid.insertAutoPositionedItems(unplacedGridItems.autoPositionedItems, autoFlowOptions);
    }

    return Result { implicitGrid.gridAreas(), implicitGrid.columnsCount(), implicitGrid.rowsCount() };
}

auto computeGridItemRects = [](const PlacedGridItems& placedGridItems, const BorderBoxPositions& inlineAxisPositions,
    const BorderBoxPositions& blockAxisPositions, const UsedInlineSizes& usedInlineSizes, const UsedBlockSizes& usedBlockSizes,
    const Vector<UsedMargins>& usedInlineMargins, const Vector<UsedMargins>& usedBlockMargins)
{
    GridItemRects gridItemRects;
    gridItemRects.reserveInitialCapacity(placedGridItems.size());

    for (size_t gridItemIndex = 0; gridItemIndex < placedGridItems.size(); ++gridItemIndex) {
        auto borderBoxRect = LayoutRect { inlineAxisPositions[gridItemIndex], blockAxisPositions[gridItemIndex],
            usedInlineSizes[gridItemIndex], usedBlockSizes[gridItemIndex]
        };

        auto& gridItemInlineMargins = usedInlineMargins[gridItemIndex];
        auto& gridItemBlockMargins = usedBlockMargins[gridItemIndex];
        auto marginEdges = RectEdges<LayoutUnit> {
            gridItemBlockMargins.marginStart,
            gridItemInlineMargins.marginEnd,
            gridItemBlockMargins.marginEnd,
            gridItemInlineMargins.marginStart
        };

        auto& placedGridItem = placedGridItems[gridItemIndex];
        gridItemRects.append({ borderBoxRect, marginEdges, placedGridItem.gridAreaLines(), placedGridItem.layoutBox() });
    }
    return gridItemRects;
};

static GridAreaSizes computeGridAreaSizes(const PlacedGridItems& gridItems, const LayoutUnit usedColumnGap, const LayoutUnit usedRowGap, const UsedTrackSizes& usedTrackSizes)
{
    auto gridItemsCount = gridItems.size();
    GridAreaSizes gridAreaSizes;
    gridAreaSizes.inlineSizes.reserveInitialCapacity(gridItemsCount);
    gridAreaSizes.blockSizes.reserveInitialCapacity(gridItemsCount);

    for (auto& gridItem : gridItems) {
        auto columnsSize = GridLayoutUtils::gridAreaDimensionSize(gridItem.columnStartLine(), gridItem.columnEndLine(), usedTrackSizes.columnSizes, usedColumnGap);
        auto rowsSize = GridLayoutUtils::gridAreaDimensionSize(gridItem.rowStartLine(), gridItem.rowEndLine(), usedTrackSizes.rowSizes, usedRowGap);
        gridAreaSizes.inlineSizes.append(columnsSize);
        gridAreaSizes.blockSizes.append(rowsSize);
    }
    return gridAreaSizes;
}

// https://drafts.csswg.org/css-grid-1/#layout-algorithm
std::pair<UsedTrackSizes, GridItemRects> GridLayout::layout(UnplacedGridItems& unplacedGridItems, const GridLayoutState& gridLayoutState)
{
    auto& gridDefinition = gridLayoutState.gridDefinition;
    auto& gridTemplateColumnsTrackSizes = gridDefinition.gridTemplateColumns.sizes;
    auto& gridTemplateRowsTrackSizes = gridDefinition.gridTemplateRows.sizes;

    auto& formattingContext = this->formattingContext();
    // 1. Run the Grid Item Placement Algorithm to resolve the placement of all grid items in the grid.
    auto [ gridAreas, columnsCount, rowsCount ] = placeGridItems(unplacedGridItems, gridTemplateColumnsTrackSizes, gridTemplateRowsTrackSizes, gridDefinition.autoFlowOptions);
    auto placedGridItems = formattingContext.constructPlacedGridItems(gridAreas);

    auto columnTrackSizingFunctionsList = trackSizingFunctions(columnsCount, gridTemplateColumnsTrackSizes, gridDefinition.gridAutoColumns);
    auto rowTrackSizingFunctionsList = trackSizingFunctions(rowsCount, gridTemplateRowsTrackSizes, gridDefinition.gridAutoRows);

    // 2. FIXME: Find the size of the grid container.

    // 3. Given the resulting grid container size, run the Grid Sizing Algorithm to size the grid.
    UsedTrackSizes usedTrackSizes = performGridSizingAlgorithm(gridLayoutState, placedGridItems, columnTrackSizingFunctionsList, rowTrackSizingFunctionsList);

    CheckedRef formattingContextRootStyle = formattingContext.root().style();
    auto gridAreaSizes = computeGridAreaSizes(placedGridItems, gridLayoutState.usedColumnGap, gridLayoutState.usedRowGap, usedTrackSizes);

    // 4. Lay out the grid items into their respective containing blocks. Each grid area’s
    // width and height are considered definite for this purpose.
    auto [ usedInlineSizes, usedBlockSizes ] = layoutGridItems(placedGridItems, gridAreaSizes, columnTrackSizingFunctionsList, rowTrackSizingFunctionsList);

    // https://drafts.csswg.org/css-grid-1/#alignment
    const auto& zoomFactor = formattingContext.zoomFactor();
    auto usedInlineMargins = computeInlineMargins(placedGridItems, zoomFactor);
    auto usedBlockMargins = computeBlockMargins(placedGridItems, zoomFactor);

    // https://drafts.csswg.org/css-grid-1/#alignment
    // After a grid container’s grid tracks have been sized, and the dimensions of all grid items
    // are finalized, grid items can be aligned within their grid areas.
    auto inlineAxisPositions = performInlineAxisSelfAlignment(placedGridItems, usedInlineMargins, usedInlineSizes, gridAreaSizes.inlineSizes);
    auto blockAxisPositions = performBlockAxisSelfAlignment(placedGridItems, usedBlockMargins, usedBlockSizes, gridAreaSizes.blockSizes);

    auto gridItemRects = computeGridItemRects(placedGridItems, inlineAxisPositions, blockAxisPositions, usedInlineSizes, usedBlockSizes, usedInlineMargins, usedBlockMargins);

    return { usedTrackSizes, gridItemRects };
}

BorderBoxPositions GridLayout::performInlineAxisSelfAlignment(const PlacedGridItems& placedGridItems, const Vector<UsedMargins>& inlineMargins, const UsedInlineSizes& borderBoxSizes,
    const Vector<LayoutUnit>& gridAreasInlineSizeList)
{
    BorderBoxPositions borderBoxPositions;
    borderBoxPositions.reserveInitialCapacity(placedGridItems.size());

    auto& formattingContextWritingMode = formattingContext().writingMode();
    for (size_t gridItemIndex = 0; gridItemIndex < placedGridItems.size(); ++gridItemIndex) {
        auto& gridItem = placedGridItems[gridItemIndex];

        auto& [marginStart, marginEnd] = inlineMargins[gridItemIndex];
        auto marginBoxSize = marginStart + borderBoxSizes[gridItemIndex] + marginEnd;
        auto remainingSpace = gridAreasInlineSizeList[gridItemIndex] - marginBoxSize;

        // Normal behavior:
        // https://www.w3.org/TR/css-align-3/#justify-grid
        // Sizes as either stretch (typical non-replaced elements) or start (typical replaced elements);
        // see Grid Item Sizing in [CSS-GRID-1]. The resulting box is then start-aligned.
        //
        // Stretching should be handled by GridLayout::layoutGridItems.
        auto marginBoxPosition = StyleSelfAlignmentData::adjustmentFromStartEdge(remainingSpace, gridItem.inlineAxisAlignment().position(), LogicalBoxAxis::Inline, formattingContextWritingMode, gridItem.writingMode());

        borderBoxPositions.append(marginBoxPosition + inlineMargins[gridItemIndex].marginStart);
    }

    return borderBoxPositions;
}

BorderBoxPositions GridLayout::performBlockAxisSelfAlignment(const PlacedGridItems& placedGridItems, const Vector<UsedMargins>& blockMargins, const UsedBlockSizes& borderBoxSizes,
    const Vector<LayoutUnit>& gridAreasBlockSizeList)
{
    BorderBoxPositions borderBoxPositions;
    borderBoxPositions.reserveInitialCapacity(placedGridItems.size());

    auto& formattingContextWritingMode = formattingContext().writingMode();
    for (size_t gridItemIndex = 0; gridItemIndex < placedGridItems.size(); ++gridItemIndex) {
        auto& gridItem = placedGridItems[gridItemIndex];

        auto& [marginStart, marginEnd] = blockMargins[gridItemIndex];
        auto marginBoxSize = marginStart + borderBoxSizes[gridItemIndex] + marginEnd;
        auto remainingSpace = gridAreasBlockSizeList[gridItemIndex] - marginBoxSize;

        // Normal behavior:
        // https://www.w3.org/TR/css-align-3/#align-grid
        // Sizes as either stretch (typical non-replaced elements) or start (typical replaced
        // elements); see Grid Item Sizing in [CSS-GRID-1]. The resulting box is then start-aligned.
        //
        // Stretching should be handled by GridLayout::layoutGridItems.
        auto marginBoxPosition = StyleSelfAlignmentData::adjustmentFromStartEdge(remainingSpace, gridItem.blockAxisAlignment().position(), LogicalBoxAxis::Block, formattingContextWritingMode, gridItem.writingMode());

        borderBoxPositions.append(marginBoxPosition + blockMargins[gridItemIndex].marginStart);
    }

    return borderBoxPositions;
}

TrackSizingFunctions GridLayout::convertGridTrackSizeToTrackSizingFunctions(const Style::GridTrackSize& gridTrackSize)
{
    auto minTrackSizingFunction = [&]() {
        // If the track was sized with a minmax() function, this is the first argument to that function.
        if (gridTrackSize.isMinMax())
            return gridTrackSize.minTrackBreadth();

        // If the track was sized with a <flex> value or fit-content() function, auto.
        if (gridTrackSize.isFitContent() || gridTrackSize.minTrackBreadth().isFlex())
            return Style::GridTrackBreadth { CSS::Keyword::Auto { } };

        // Otherwise, the track's sizing function.
        return gridTrackSize.minTrackBreadth();
    };

    auto maxTrackSizingFunction = [&]() {
        // If the track was sized with a minmax() function, this is the second argument to that function.
        if (gridTrackSize.isMinMax())
            return gridTrackSize.maxTrackBreadth();

        // Otherwise, the track’s sizing function. In all cases, treat auto and fit-content() as max-content,
        // except where specified otherwise for fit-content().
        // Note: This special treatment is handled inside of TrackSizingAlgorithm.
        return gridTrackSize.maxTrackBreadth();
    };

    return TrackSizingFunctions { minTrackSizingFunction(), maxTrackSizingFunction() };
}

// Generates track sizing functions for implicit tracks using grid-auto-{columns,rows}
// FIXME: This function only supports appended tracks but not prepended tracks.
TrackSizingFunctionsList GridLayout::generateImplicitTrackSizingFunctions(size_t explicitTracksCount, size_t totalTracksCount, const Style::GridTrackSizes& gridAutoTrackSizes)
{
    // https://drafts.csswg.org/css-grid-1/#auto-tracks
    size_t implicitTracksCount = totalTracksCount - explicitTracksCount;

    TrackSizingFunctionsList trackSizingFunctionsForImplicitGrid;
    trackSizingFunctionsForImplicitGrid.reserveInitialCapacity(implicitTracksCount);

    // Cycle through grid-auto-{columns,rows} values using modulo.
    for (size_t i = 0; i < implicitTracksCount; ++i) {
        size_t autoTrackIndex = i % gridAutoTrackSizes.size();
        trackSizingFunctionsForImplicitGrid.append(convertGridTrackSizeToTrackSizingFunctions(gridAutoTrackSizes[autoTrackIndex]));
    }

    return trackSizingFunctionsForImplicitGrid;
}

TrackSizingFunctionsList GridLayout::trackSizingFunctions(size_t totalTracksCount, const Vector<Style::GridTrackSize>& gridTemplateTrackSizes, const Style::GridTrackSizes& gridAutoTrackSizes)
{
    // FIXME: This function only supports appended tracks but not prepended tracks.
    // Per spec, we should support both forward and backward implicit tracks.
    ASSERT_WITH_MESSAGE(totalTracksCount >= gridTemplateTrackSizes.size(), "Total tracks should be at least as many as explicit tracks");

    TrackSizingFunctionsList trackSizingFunctions;
    trackSizingFunctions.reserveInitialCapacity(totalTracksCount);

    // https://drafts.csswg.org/css-grid-1/#algo-terms
    // Map explicit tracks from grid-template-{columns,rows}
    for (auto& gridTrackSize : gridTemplateTrackSizes)
        trackSizingFunctions.append(convertGridTrackSizeToTrackSizingFunctions(gridTrackSize));

    // Generate implicit tracks using grid-auto-{columns,rows}
    // https://drafts.csswg.org/css-grid-1/#auto-tracks
    // "The first track after the last explicitly-sized track receives the first specified size, and so on forwards"
    auto implicitTrackSizingFunctions = generateImplicitTrackSizingFunctions(gridTemplateTrackSizes.size(), totalTracksCount, gridAutoTrackSizes);
    trackSizingFunctions.appendVector(implicitTrackSizingFunctions);

    ASSERT(trackSizingFunctions.size() == totalTracksCount);
    return trackSizingFunctions;
}

// If calculating the layout of a grid item in this step depends on the available space in the block axis,
// assume the available space that it would have if any row with a definite max track sizing function
// had that size and all other rows were infinite.
static Vector<LayoutUnit> rowSizesForFirstIterationColumnSizing(const TrackSizingFunctionsList& rowTrackSizingFunctionsList, std::optional<LayoutUnit> gridContainerInnerInlineSize)
{
    return rowTrackSizingFunctionsList.map([&gridContainerInnerInlineSize](const TrackSizingFunctions& trackSizingFunctions) {
        return WTF::switchOn(trackSizingFunctions.max,
            [](const Style::GridTrackBreadthLength::Fixed& fixedValue) {
                return Style::evaluate<LayoutUnit>(fixedValue, Style::ZoomNeeded { });
            },
            [&gridContainerInnerInlineSize](const Style::GridTrackBreadthLength::Percentage& percentageValue) {
                ASSERT(gridContainerInnerInlineSize, "The formatting context should have transformed this track size to auto");
                return Style::evaluate<LayoutUnit>(percentageValue, *gridContainerInnerInlineSize);
            },
            [&gridContainerInnerInlineSize](const Style::GridTrackBreadth::Calc calculatedValue) -> LayoutUnit {
                ASSERT(gridContainerInnerInlineSize, "The formatting context should have transformed this track size to auto");
                return Style::evaluate<LayoutUnit>(calculatedValue, *gridContainerInnerInlineSize, Style::ZoomNeeded { });
            },
            [](const CSS::Keyword::MinContent&) -> LayoutUnit {
                return LayoutUnit::max();
            },
            [](const CSS::Keyword::MaxContent&) {
                return LayoutUnit::max();
            },
            [](const CSS::Keyword::Auto&) -> LayoutUnit {
                return LayoutUnit::max();
            },
            [](const Style::GridTrackBreadth::Flex&) -> LayoutUnit {
                return LayoutUnit::max();
            },
            [](const auto&) -> LayoutUnit {
                ASSERT_NOT_IMPLEMENTED_YET();
                return { };
            });
    });
}

// During track sizing we may need to get different types of size contributions for a grid item.
// Getting a contribution in a specific dimension may require knowing the available space in
// the opposite dimension. For each of these cases, the spec defines how to compute the available space.
static LayoutUnit NODELETE oppositeAxisConstraintForTrackSizing(Vector<LayoutUnit> oppositeAxisTrackSizes, const WTF::Range<size_t> oppositeAxisSpan)
{
    auto totalAvailableSpaceFromSpannedTracks = 0_lu;
    for (auto oppositeAxisLineIndex : std::views::iota(oppositeAxisSpan.begin(), oppositeAxisSpan.end())) {
        auto& oppositeAxisTrackSize = oppositeAxisTrackSizes[oppositeAxisLineIndex];
        if (oppositeAxisTrackSize == LayoutUnit::max())
            return oppositeAxisTrackSize;

        totalAvailableSpaceFromSpannedTracks += oppositeAxisTrackSize;
    }
    return totalAvailableSpaceFromSpannedTracks;
}

// https://www.w3.org/TR/css-grid-1/#algo-grid-sizing
UsedTrackSizes GridLayout::performGridSizingAlgorithm(const GridLayoutState& layoutState, const PlacedGridItems& placedGridItems,
    const TrackSizingFunctionsList& columnTrackSizingFunctionsList, const TrackSizingFunctionsList& rowTrackSizingFunctionsList) const
{
    auto gridItemsCount = placedGridItems.size();

    Vector<WTF::Range<size_t>> columnSpanList;
    columnSpanList.reserveInitialCapacity(gridItemsCount);
    ComputedSizesList inlineAxisComputedSizesList;
    inlineAxisComputedSizesList.reserveInitialCapacity(gridItemsCount);
    UsedBorderAndPaddingList inlineBorderAndPaddingList;
    inlineBorderAndPaddingList.reserveInitialCapacity(gridItemsCount);
    TrackSizingGridItemConstraintList blockAxisConstraintList;
    blockAxisConstraintList.reserveInitialCapacity(gridItemsCount);

    Vector<WTF::Range<size_t>> rowSpanList;
    rowSpanList.reserveInitialCapacity(gridItemsCount);
    ComputedSizesList blockAxisComputedSizesList;
    blockAxisComputedSizesList.reserveInitialCapacity(gridItemsCount);
    UsedBorderAndPaddingList blockBorderAndPaddingList;
    blockBorderAndPaddingList.reserveInitialCapacity(gridItemsCount);

    // Extract scenarios from constraints
    auto& layoutConstraints = layoutState.gridLayoutConstraints;
    auto columnFreeSpaceScenario = layoutConstraints.inlineAxis.scenario();
    auto rowFreeSpaceScenario = layoutConstraints.blockAxis.scenario();

    // Convert constraints to optional available space for track sizing algorithm
    std::optional<LayoutUnit> inlineAxisAvailableSpace = columnFreeSpaceScenario == AxisConstraint::FreeSpaceScenario::Definite
        ? std::optional(layoutConstraints.inlineAxis.availableSpace())
        : std::nullopt;
    auto blockAxisAvailableSpace = rowFreeSpaceScenario == AxisConstraint::FreeSpaceScenario::Definite
        ? std::optional(layoutConstraints.blockAxis.availableSpace())
        : std::nullopt;
    auto rowSizesForFirstColumnSizing = rowSizesForFirstIterationColumnSizing(rowTrackSizingFunctionsList, inlineAxisAvailableSpace);

    for (auto& gridItem : placedGridItems) {
        columnSpanList.append({ gridItem.columnStartLine(), gridItem.columnEndLine() });
        inlineAxisComputedSizesList.append(gridItem.inlineAxisSizes());
        inlineBorderAndPaddingList.append(gridItem.usedInlineBorderAndPadding());

        auto rowSpan = WTF::Range<size_t> { gridItem.rowStartLine(), gridItem.rowEndLine() };
        rowSpanList.append(rowSpan);
        blockAxisComputedSizesList.append(gridItem.blockAxisSizes());
        blockBorderAndPaddingList.append(gridItem.usedBlockBorderAndPadding());
        blockAxisConstraintList.append(oppositeAxisConstraintForTrackSizing(rowSizesForFirstColumnSizing, rowSpan));
    }

    auto& formattingContext = this->formattingContext();
    // 1. First, the track sizing algorithm is used to resolve the sizes of the grid columns.
    auto columnSizes = TrackSizingAlgorithm::sizeTracks(placedGridItems, inlineAxisComputedSizesList, inlineBorderAndPaddingList, columnSpanList,
        columnTrackSizingFunctionsList, inlineAxisAvailableSpace, blockAxisConstraintList, GridLayoutUtils::inlineAxisGridItemSizingFunctions(formattingContext.integrationUtils()),
        columnFreeSpaceScenario, layoutState.usedColumnGap, layoutState.usedJustifyContent, layoutConstraints.inlineAxis.containerMinimumSize());

    // To find the inline-axis available space for any items whose block-axis size contributions
    // require it, use the grid column sizes calculated in the previous step.
    TrackSizingGridItemConstraintList inlineAxisConstraintList;
    inlineAxisConstraintList.reserveInitialCapacity(gridItemsCount);
    for (auto [gridItemIndex, gridItem] : WTF::indexedRange(placedGridItems))
        inlineAxisConstraintList.append(oppositeAxisConstraintForTrackSizing(columnSizes, columnSpanList[gridItemIndex]));

    // 2. Next, the track sizing algorithm resolves the sizes of the grid rows.
    auto rowSizes = TrackSizingAlgorithm::sizeTracks(placedGridItems, blockAxisComputedSizesList, blockBorderAndPaddingList, rowSpanList,
        rowTrackSizingFunctionsList, blockAxisAvailableSpace, inlineAxisConstraintList, GridLayoutUtils::blockAxisGridItemSizingFunctions(formattingContext),
        rowFreeSpaceScenario, layoutState.usedRowGap, layoutState.usedAlignContent, layoutConstraints.blockAxis.containerMinimumSize());

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

// Helper to compute margins from axis sizes
static UsedMargins computeMarginsForAxis(const ComputedSizes& axisSizes, const Style::ZoomFactor& zoomFactor)
{
    auto marginStart = [&] -> LayoutUnit {
        if (auto fixedMarginStart = axisSizes.marginStart.tryFixed())
            return LayoutUnit { fixedMarginStart->resolveZoom(zoomFactor) };

        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    };

    auto marginEnd = [&] -> LayoutUnit {
        if (auto fixedMarginEnd = axisSizes.marginEnd.tryFixed())
            return LayoutUnit { fixedMarginEnd->resolveZoom(zoomFactor) };

        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    };

    return UsedMargins { marginStart(), marginEnd() };
}

// https://drafts.csswg.org/css-grid-1/#auto-margins
Vector<UsedMargins> GridLayout::computeInlineMargins(const PlacedGridItems& placedGridItems, const Style::ZoomFactor& zoomFactor)
{
    return placedGridItems.map([&zoomFactor](const PlacedGridItem& placedGridItem) {
        return computeMarginsForAxis(placedGridItem.inlineAxisSizes(), zoomFactor);
    });
}

// https://drafts.csswg.org/css-grid-1/#auto-margins
Vector<UsedMargins> GridLayout::computeBlockMargins(const PlacedGridItems& placedGridItems, const Style::ZoomFactor& zoomFactor)
{
    return placedGridItems.map([&zoomFactor](const PlacedGridItem& placedGridItem) {
        return computeMarginsForAxis(placedGridItem.blockAxisSizes(), zoomFactor);
    });
}

// https://drafts.csswg.org/css-grid-1/#grid-item-sizing
std::pair<UsedInlineSizes, UsedBlockSizes> GridLayout::layoutGridItems(const PlacedGridItems& placedGridItems, const GridAreaSizes& gridAreaSizes,
    const TrackSizingFunctionsList& columnTrackSizingFunctions, const TrackSizingFunctionsList& rowTrackSizingFunctions) const
{
    auto gridItemsCount = placedGridItems.size();
    UsedInlineSizes usedInlineSizes;
    usedInlineSizes.reserveInitialCapacity(gridItemsCount);
    UsedBlockSizes usedBlockSizes;
    usedBlockSizes.reserveInitialCapacity(gridItemsCount);

    auto& formattingContext = this->formattingContext();
    auto& integrationUtils = formattingContext.integrationUtils();
    for (auto [gridItemIndex, gridItem] : WTF::indexedRange(placedGridItems)) {
        auto& gridAreaInlineSize = gridAreaSizes.inlineSizes[gridItemIndex];
        auto& gridAreaBlockSize = gridAreaSizes.blockSizes[gridItemIndex];

        auto usedInlineSizeForGridItem = GridLayoutUtils::usedInlineSizeForGridItem(gridItem, gridItem.usedInlineBorderAndPadding(), columnTrackSizingFunctions, gridAreaInlineSize, integrationUtils);
        usedInlineSizes.append(usedInlineSizeForGridItem);

        auto usedBlockSizeForGridItem = GridLayoutUtils::usedBlockSizeForGridItem(gridItem, gridItem.usedBlockBorderAndPadding(), rowTrackSizingFunctions, gridAreaBlockSize, integrationUtils);
        usedBlockSizes.append(usedBlockSizeForGridItem);

        auto& layoutBox = gridItem.layoutBox();
        integrationUtils.layoutWithFormattingContextForBox(layoutBox, usedInlineSizeForGridItem, usedBlockSizeForGridItem);
    }
    return { usedInlineSizes, usedBlockSizes };
}

} // namespace Layout
} // namespace WebCore
