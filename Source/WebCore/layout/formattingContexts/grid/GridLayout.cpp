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

GridDimensions GridLayout::calculateGridDimensions(const UnplacedGridItems& unplacedGridItems, size_t explicitColumnsCount, size_t explicitRowsCount)
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

    // Calculate grid dimensions (offsets and total size) for negative grid line positions
    auto gridDimensions = calculateGridDimensions(
        unplacedGridItems, gridTemplateColumnsTrackSizes.size(), gridTemplateRowsTrackSizes.size());

    // Normalize all grid item positions by applying the offsets
    for (auto& item : unplacedGridItems.nonAutoPositionedItems)
        item.applyGridOffsets(gridDimensions.rowOffset, gridDimensions.columnOffset);
    for (auto& item : unplacedGridItems.definiteRowPositionedItems)
        item.applyGridOffsets(gridDimensions.rowOffset, gridDimensions.columnOffset);
    for (auto& item : unplacedGridItems.autoPositionedItems)
        item.applyGridOffsets(gridDimensions.rowOffset, gridDimensions.columnOffset);

    ImplicitGrid implicitGrid(gridDimensions.totalColumns, gridDimensions.totalRows);

    // 1. Position anything that's not auto-positioned.
    for (auto& nonAutoPositionedItem : unplacedGridItems.nonAutoPositionedItems)
        implicitGrid.insertUnplacedGridItem(nonAutoPositionedItem);

    // 2. Process the items locked to a given row.
    // Phase 1: Only single-cell items within explicit grid bounds
    HashMap<size_t, size_t, DefaultHash<size_t>, WTF::UnsignedWithZeroKeyHashTraits<size_t>> rowCursors;
    for (auto& definiteRowPositionedItem : unplacedGridItems.definiteRowPositionedItems)
        implicitGrid.insertDefiniteRowItem(definiteRowPositionedItem, autoFlowOptions, &rowCursors);

    // 3. FIXME: Process auto-positioned items (not implemented yet)
    ASSERT(unplacedGridItems.autoPositionedItems.isEmpty());


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

static GridAreaSizes computeGridAreaSizes(const PlacedGridItems& gridItems, const Style::GapGutter& columnGap, const Style::GapGutter& rowGap, const UsedTrackSizes& usedTrackSizes)
{
    auto gridItemsCount = gridItems.size();
    GridAreaSizes gridAreaSizes;
    gridAreaSizes.inlineSizes.reserveInitialCapacity(gridItemsCount);
    gridAreaSizes.blockSizes.reserveInitialCapacity(gridItemsCount);

    auto usedColumnGap = GridLayoutUtils::computeGapValue(columnGap);
    auto usedRowGap = GridLayoutUtils::computeGapValue(rowGap);

    for (auto& gridItem : gridItems) {
        auto columnsSize = GridLayoutUtils::gridAreaDimensionSize(gridItem.columnStartLine(), gridItem.columnEndLine(), usedTrackSizes.columnSizes, usedColumnGap);
        auto rowsSize = GridLayoutUtils::gridAreaDimensionSize(gridItem.rowStartLine(), gridItem.rowEndLine(), usedTrackSizes.rowSizes, usedRowGap);
        gridAreaSizes.inlineSizes.append(columnsSize);
        gridAreaSizes.blockSizes.append(rowsSize);
    }
    return gridAreaSizes;
}

// https://drafts.csswg.org/css-grid-1/#layout-algorithm
std::pair<UsedTrackSizes, GridItemRects> GridLayout::layout(const GridFormattingContext::GridLayoutConstraints& layoutConstraints, UnplacedGridItems& unplacedGridItems,
    const GridDefinition& gridDefinition)
{
    auto& formattingContext = this->formattingContext();
    auto& gridTemplateColumnsTrackSizes = gridDefinition.gridTemplateColumns.sizes;
    auto& gridTemplateRowsTrackSizes = gridDefinition.gridTemplateRows.sizes;

    // 1. Run the Grid Item Placement Algorithm to resolve the placement of all grid items in the grid.
    auto [ gridAreas, columnsCount, rowsCount ] = placeGridItems(unplacedGridItems, gridTemplateColumnsTrackSizes, gridTemplateRowsTrackSizes, gridDefinition.autoFlowOptions);
    auto placedGridItems = formattingContext.constructPlacedGridItems(gridAreas);

    auto columnTrackSizingFunctionsList = trackSizingFunctions(columnsCount, gridTemplateColumnsTrackSizes);
    auto rowTrackSizingFunctionsList = trackSizingFunctions(rowsCount, gridTemplateRowsTrackSizes);

    // 2. FIXME: Find the size of the grid container.

    // 3. Given the resulting grid container size, run the Grid Sizing Algorithm to size the grid.
    // FIXME: Handle FreeSpaceScenario::MinContent and FreeSpaceScenario::Indefinite once intrinsic sizing is supported.
    auto columnFreeSpaceScenario = layoutConstraints.inlineAxisAvailableSpace.has_value()
        ? FreeSpaceScenario::Definite
        : FreeSpaceScenario::Indefinite;
    auto rowFreeSpaceScenario = layoutConstraints.blockAxisAvailableSpace.has_value()
        ? FreeSpaceScenario::Definite
        : FreeSpaceScenario::Indefinite;
    UsedTrackSizes usedTrackSizes = performGridSizingAlgorithm(placedGridItems, columnTrackSizingFunctionsList, rowTrackSizingFunctionsList, layoutConstraints, columnFreeSpaceScenario, rowFreeSpaceScenario);

    CheckedRef formattingContextRootStyle = formattingContext.root().style();
    auto& columnGap = formattingContextRootStyle->columnGap();
    auto& rowGap = formattingContextRootStyle->rowGap();
    auto gridAreaSizes = computeGridAreaSizes(placedGridItems, columnGap, rowGap, usedTrackSizes);

    // 4. Lay out the grid items into their respective containing blocks. Each grid area’s
    // width and height are considered definite for this purpose.
    auto [ usedInlineSizes, usedBlockSizes ] = layoutGridItems(placedGridItems, gridAreaSizes);

    // https://drafts.csswg.org/css-grid-1/#alignment
    const auto& zoomFactor = formattingContext.zoomFactor();
    auto usedInlineMargins = computeInlineMargins(placedGridItems, zoomFactor);
    auto usedBlockMargins = computeBlockMargins(placedGridItems, zoomFactor);

    // https://drafts.csswg.org/css-grid-1/#alignment
    // After a grid container’s grid tracks have been sized, and the dimensions of all grid items
    // are finalized, grid items can be aligned within their grid areas.
    auto inlineAxisPositions = performInlineAxisSelfAlignment(placedGridItems, usedInlineMargins, gridAreaSizes.inlineSizes);
    auto blockAxisPositions = performBlockAxisSelfAlignment(placedGridItems, usedBlockMargins, gridAreaSizes.blockSizes);

    auto gridItemRects = computeGridItemRects(placedGridItems, inlineAxisPositions, blockAxisPositions, usedInlineSizes, usedBlockSizes, usedInlineMargins, usedBlockMargins);

    return { usedTrackSizes, gridItemRects };
}

BorderBoxPositions GridLayout::performInlineAxisSelfAlignment(const PlacedGridItems& placedGridItems, const Vector<UsedMargins>& inlineMargins, const Vector<LayoutUnit>& gridAreaInlineSizes)
{
    UNUSED_PARAM(gridAreaInlineSizes);
    BorderBoxPositions borderBoxPositions;
    borderBoxPositions.reserveInitialCapacity(placedGridItems.size());

    auto computeMarginBoxPosition = [](const PlacedGridItem& placedGridItem) -> LayoutUnit {
        switch (placedGridItem.inlineAxisAlignment().position()) {
        case ItemPosition::FlexStart:
        case ItemPosition::SelfStart:
        case ItemPosition::Start:
            return { };

        // https://www.w3.org/TR/css-align-3/#justify-grid
        // Sizes as either stretch (typical non-replaced elements) or start (typical replaced elements);
        // see Grid Item Sizing in [CSS-GRID-1]. The resulting box is then start-aligned.
        //
        // Stretching should be handled by GridLayout::layoutGridItems.
        case ItemPosition::Normal:
            return { };
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        }
    };

    for (size_t gridItemIndex = 0; gridItemIndex < placedGridItems.size(); ++gridItemIndex) {
        auto& gridItem = placedGridItems[gridItemIndex];
        auto marginBoxPosition = computeMarginBoxPosition(gridItem);
        borderBoxPositions.append(marginBoxPosition + inlineMargins[gridItemIndex].marginStart);
    }

    return borderBoxPositions;
}

BorderBoxPositions GridLayout::performBlockAxisSelfAlignment(const PlacedGridItems& placedGridItems, const Vector<UsedMargins>& blockMargins, const Vector<LayoutUnit>& gridAreaBlockSizes)
{
    UNUSED_PARAM(gridAreaBlockSizes);
    BorderBoxPositions borderBoxPositions;
    borderBoxPositions.reserveInitialCapacity(placedGridItems.size());

    auto computeMarginBoxPosition = [](const PlacedGridItem& placedGridItem) -> LayoutUnit {
        switch (placedGridItem.blockAxisAlignment().position()) {
        case ItemPosition::FlexStart:
        case ItemPosition::SelfStart:
        case ItemPosition::Start:
            return { };

        // https://www.w3.org/TR/css-align-3/#align-grid
        // Sizes as either stretch (typical non-replaced elements) or start (typical replaced
        // elements); see Grid Item Sizing in [CSS-GRID-1]. The resulting box is then start-aligned.
        //
        // Stretching should be handled by GridLayout::layoutGridItems.
        case ItemPosition::Normal:
            return { };
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        }
    };

    for (size_t gridItemIndex = 0; gridItemIndex < placedGridItems.size(); ++gridItemIndex) {
        auto& gridItem = placedGridItems[gridItemIndex];
        auto marginBoxPosition = computeMarginBoxPosition(gridItem);
        borderBoxPositions.append(marginBoxPosition + blockMargins[gridItemIndex].marginStart);
    }

    return borderBoxPositions;
}

TrackSizingFunctionsList GridLayout::trackSizingFunctions(size_t implicitGridTracksCount, const Vector<Style::GridTrackSize> gridTemplateTrackSizes)
{
    // FIXME: Support implicit tracks (both before and after the explicit grid)
    ASSERT(implicitGridTracksCount == gridTemplateTrackSizes.size(), "Currently only support mapping track sizes from explicit grid from grid-template-{columns, rows}");
    UNUSED_VARIABLE(implicitGridTracksCount);

    // https://drafts.csswg.org/css-grid-1/#algo-terms
    return gridTemplateTrackSizes.map([](const Style::GridTrackSize& gridTrackSize) {
        auto minTrackSizingFunction = [&]() {
            // If the track was sized with a minmax() function, this is the first argument to that function.
            if (gridTrackSize.isMinMax())
                return gridTrackSize.minTrackBreadth();

            // If the track was sized with a <flex> value or fit-content() function, auto.
            if (gridTrackSize.isFitContent() || gridTrackSize.minTrackBreadth().isFlex())
                return Style::GridTrackBreadth { CSS::Keyword::Auto { } };

            // Otherwise, the track’s sizing function.
            return gridTrackSize.minTrackBreadth();
        };

        auto maxTrackSizingFunction = [&]() {
            // If the track was sized with a minmax() function, this is the second argument to that function.
            if (gridTrackSize.isMinMax())
                return gridTrackSize.maxTrackBreadth();

            // Otherwise, the track’s sizing function. In all cases, treat auto and fit-content() as max-content,
            // except where specified otherwise for fit-content().
            if (gridTrackSize.maxTrackBreadth().isAuto())
                return Style::GridTrackBreadth { CSS::Keyword::MaxContent { } };

            if (gridTrackSize.isFitContent()) {
                ASSERT_NOT_IMPLEMENTED_YET();
                return Style::GridTrackBreadth { CSS::Keyword::MaxContent { } };
            }

            return gridTrackSize.maxTrackBreadth();
        };

        return TrackSizingFunctions { minTrackSizingFunction(), maxTrackSizingFunction() };
    });
}

// https://www.w3.org/TR/css-grid-1/#algo-grid-sizing
UsedTrackSizes GridLayout::performGridSizingAlgorithm(const PlacedGridItems& placedGridItems,
    const TrackSizingFunctionsList& columnTrackSizingFunctionsList, const TrackSizingFunctionsList& rowTrackSizingFunctionsList, const GridFormattingContext::GridLayoutConstraints& layoutConstraints, FreeSpaceScenario columnFreeSpaceScenario, FreeSpaceScenario rowFreeSpaceScenario) const
{
    auto& integrationUtils = formattingContext().integrationUtils();
    auto gridItemsCount = placedGridItems.size();

    Vector<WTF::Range<size_t>> columnSpanList;
    columnSpanList.reserveInitialCapacity(gridItemsCount);
    ComputedSizesList inlineAxisComputedSizesList;
    inlineAxisComputedSizesList.reserveInitialCapacity(gridItemsCount);

    Vector<WTF::Range<size_t>> rowSpanList;
    rowSpanList.reserveInitialCapacity(gridItemsCount);
    ComputedSizesList blockAxisComputedSizesList;
    blockAxisComputedSizesList.reserveInitialCapacity(gridItemsCount);

    for (auto& gridItem : placedGridItems) {
        columnSpanList.append({ gridItem.columnStartLine(), gridItem.columnEndLine() });
        inlineAxisComputedSizesList.append(gridItem.inlineAxisSizes());

        rowSpanList.append({ gridItem.rowStartLine(), gridItem.rowEndLine() });
        blockAxisComputedSizesList.append(gridItem.blockAxisSizes());
    }

    CheckedRef formattingContextRootStyle = formattingContext().root().style();
    auto columnsGap = GridLayoutUtils::computeGapValue(formattingContextRootStyle->columnGap());
    auto rowsGap = GridLayoutUtils::computeGapValue(formattingContextRootStyle->rowGap());

    // 1. First, the track sizing algorithm is used to resolve the sizes of the grid columns.
    auto columnSizes = TrackSizingAlgorithm::sizeTracks(placedGridItems, inlineAxisComputedSizesList, columnSpanList,
        columnTrackSizingFunctionsList, layoutConstraints.inlineAxisAvailableSpace, GridLayoutUtils::inlineAxisGridItemSizingFunctions(), integrationUtils, columnFreeSpaceScenario, columnsGap);

    // 2. Next, the track sizing algorithm resolves the sizes of the grid rows.
    auto rowSizes = TrackSizingAlgorithm::sizeTracks(placedGridItems, blockAxisComputedSizesList, rowSpanList,
        rowTrackSizingFunctionsList, layoutConstraints.blockAxisAvailableSpace, GridLayoutUtils::blockAxisGridItemSizingFunctions(), integrationUtils, rowFreeSpaceScenario, rowsGap);

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

// https://drafts.csswg.org/css-grid-1/#auto-margins
Vector<UsedMargins> GridLayout::computeInlineMargins(const PlacedGridItems& placedGridItems, const Style::ZoomFactor& zoomFactor)
{
    return placedGridItems.map([&zoomFactor](const PlacedGridItem& placedGridItem) {
        auto& inlineAxisSizes = placedGridItem.inlineAxisSizes();

        auto marginStart = [&] -> LayoutUnit {
            if (auto fixedMarginStart = inlineAxisSizes.marginStart.tryFixed())
                return LayoutUnit { fixedMarginStart->resolveZoom(zoomFactor) };

            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        };

        auto marginEnd = [&] -> LayoutUnit {
            if (auto fixedMarginEnd = inlineAxisSizes.marginEnd.tryFixed())
                return LayoutUnit { fixedMarginEnd->resolveZoom(zoomFactor) };

            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        };

        return UsedMargins { marginStart(), marginEnd() };
    });
}

// https://drafts.csswg.org/css-grid-1/#auto-margins
Vector<UsedMargins> GridLayout::computeBlockMargins(const PlacedGridItems& placedGridItems, const Style::ZoomFactor& zoomFactor)
{
    return placedGridItems.map([&zoomFactor](const PlacedGridItem& placedGridItem) {
        auto& blockAxisSizes = placedGridItem.blockAxisSizes();

        auto marginStart = [&] -> LayoutUnit {
            if (auto fixedMarginStart = blockAxisSizes.marginStart.tryFixed())
                return LayoutUnit { fixedMarginStart->resolveZoom(zoomFactor) };

            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        };

        auto marginEnd = [&] -> LayoutUnit {
            if (auto fixedMarginEnd = blockAxisSizes.marginEnd.tryFixed())
                return LayoutUnit { fixedMarginEnd->resolveZoom(zoomFactor) };

            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        };

        return UsedMargins { marginStart(), marginEnd() };
    });
}

// https://drafts.csswg.org/css-grid-1/#grid-item-sizing
std::pair<UsedInlineSizes, UsedBlockSizes> GridLayout::layoutGridItems(const PlacedGridItems& placedGridItems, const GridAreaSizes& gridAreaSizes) const
{
    auto gridItemsCount = placedGridItems.size();
    UsedInlineSizes usedInlineSizes(gridItemsCount);
    UsedBlockSizes usedBlockSizes(gridItemsCount);

    auto& formattingContext = this->formattingContext();
    auto& integrationUtils = formattingContext.integrationUtils();
    for (auto [gridItemIndex, gridItem] : WTF::indexedRange(placedGridItems)) {
        auto& gridItemBoxGeometry = formattingContext.geometryForGridItem(gridItem.layoutBox());
        auto& gridAreaInlineSize = gridAreaSizes.inlineSizes[gridItemIndex];
        auto& gridAreaBlockSize = gridAreaSizes.blockSizes[gridItemIndex];

        auto usedInlineSizeForGridItem = GridLayoutUtils::usedInlineSizeForGridItem(gridItem, gridItemBoxGeometry.horizontalBorderAndPadding(), gridAreaInlineSize);
        usedInlineSizes.append(usedInlineSizeForGridItem);

        auto usedBlockSizeForGridItem = GridLayoutUtils::usedBlockSizeForGridItem(gridItem, gridItemBoxGeometry.verticalBorderAndPadding(), gridAreaBlockSize);
        usedBlockSizes.append(usedBlockSizeForGridItem);

        auto& layoutBox = gridItem.layoutBox();
        integrationUtils.layoutWithFormattingContextForBox(layoutBox, usedInlineSizeForGridItem, usedBlockSizeForGridItem);
    }
    return { usedInlineSizes, usedBlockSizes };
}

} // namespace Layout
} // namespace WebCore
