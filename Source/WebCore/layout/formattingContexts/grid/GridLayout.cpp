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
#include "LayoutBoxGeometry.h"
#include "LayoutElementBox.h"
#include "NotImplemented.h"
#include "PlacedGridItem.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "TrackSizingAlgorithm.h"
#include "TrackSizingFunctions.h"
#include "UnplacedGridItem.h"
#include "UsedTrackSizes.h"
#include <wtf/Range.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace Layout {

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

// FIXME: Try moving this to GridFormattingContext to simplify the layout code. The initial implicit
// grid dimensions depend only on the resolved definite item placements (from constructUnplacedGridItems),
// the explicit track counts (from style), and the leading implicit track counts, which are all
// available before layout, so it may be possible to compute them there instead of on GridLayout.
GridDimensions GridLayout::calculateInitialImplicitGridDimensions(const UnplacedGridItems& unplacedGridItems, LeadingImplicitTracks leadingImplicitTracks, size_t explicitColumnsCount, size_t explicitRowsCount)
{
    // The explicit grid is preceded by any leading implicit tracks generated for items placed with
    // a negative line that resolves before the grid start. Every item's line has already been
    // shifted forward by this amount, so include the leading tracks in the initial bounds.
    size_t maximumColumnIndex = explicitColumnsCount + leadingImplicitTracks.columnsCount;
    size_t maximumRowIndex = explicitRowsCount + leadingImplicitTracks.rowsCount;

    auto updateGridBounds = [&](const UnplacedGridItem& item) {
        if (item.hasDefiniteRowPosition()) {
            auto [rowStart, rowEnd] = item.definiteRowStartEnd();
            maximumRowIndex = std::max({ maximumRowIndex, rowStart, rowEnd });
        }

        if (item.hasDefiniteColumnPosition()) {
            auto [columnStart, columnEnd] = item.definiteColumnStartEnd();
            maximumColumnIndex = std::max({ maximumColumnIndex, columnStart, columnEnd });
        }
    };

    for (const auto& item : unplacedGridItems.nonAutoPositionedItems)
        updateGridBounds(item);
    for (const auto& item : unplacedGridItems.definiteRowPositionedItems)
        updateGridBounds(item);

    // The implicit grid always starts with at least one row. Grid coverage guarantees at least one
    // in-flow grid item, and every item occupies at least one row, so placement would end up
    // creating this row regardless. Starting with it means the grid matrix is never empty, which
    // lets the column count always be read from the matrix itself.
    maximumRowIndex = std::max<size_t>(maximumRowIndex, 1);

    return {
        maximumColumnIndex,
        maximumRowIndex
    };
}

ImplicitGrid GridLayout::constructInitialImplicitGrid(const UnplacedGridItems& unplacedGridItems, LeadingImplicitTracks leadingImplicitTracks, size_t explicitColumnsCount, size_t explicitRowsCount)
{
    auto initialDimensions = calculateInitialImplicitGridDimensions(
        unplacedGridItems, leadingImplicitTracks, explicitColumnsCount, explicitRowsCount);

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
auto GridLayout::placeGridItems(UnplacedGridItems& unplacedGridItems, LeadingImplicitTracks leadingImplicitTracks, const Vector<Style::GridTrackSize>& gridTemplateColumnsTrackSizes,
    const Vector<Style::GridTrackSize>& gridTemplateRowsTrackSizes, GridAutoFlowOptions autoFlowOptions)
{
    struct Result {
        GridAreas gridAreas;
        size_t columnsCount;
        size_t rowsCount;
    };

    auto implicitGrid = constructInitialImplicitGrid(unplacedGridItems, leadingImplicitTracks, gridTemplateColumnsTrackSizes.size(), gridTemplateRowsTrackSizes.size());

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
GridLayoutResult GridLayout::layout(UnplacedGridItems& unplacedGridItems, LeadingImplicitTracks leadingImplicitTracks, const GridLayoutState& gridLayoutState, GridLayoutScope scope)
{
    auto& gridDefinition = gridLayoutState.gridDefinition;
    auto& gridTemplateColumnsTrackSizes = gridDefinition.gridTemplateColumns.sizes;
    auto& gridTemplateRowsTrackSizes = gridDefinition.gridTemplateRows.sizes;

    auto& formattingContext = this->formattingContext();
    // 1. Run the Grid Item Placement Algorithm to resolve the placement of all grid items in the grid.
    auto [ gridAreas, columnsCount, rowsCount ] = placeGridItems(unplacedGridItems, leadingImplicitTracks, gridTemplateColumnsTrackSizes, gridTemplateRowsTrackSizes, gridDefinition.autoFlowOptions);
    auto placedGridItems = formattingContext.constructPlacedGridItems(gridAreas);

    auto columnTrackSizingFunctionsList = trackSizingFunctions(columnsCount, leadingImplicitTracks.columnsCount, gridTemplateColumnsTrackSizes, gridDefinition.gridAutoColumns, gridDefinition.zoom);
    auto rowTrackSizingFunctionsList = trackSizingFunctions(rowsCount, leadingImplicitTracks.rowsCount, gridTemplateRowsTrackSizes, gridDefinition.gridAutoRows, gridDefinition.zoom);

    // https://drafts.csswg.org/css-grid-1/#algo-grid-sizing
    // Fast path: the caller only needs the column sizes resolved by step 1 of the grid sizing
    // algorithm (e.g. intrinsic width computation where no grid item's inline contribution depends
    // on its block size). Steps 2-4 cannot change the column sizes, so size the columns alone and
    // skip row sizing, grid-item layout, and alignment.
    if (scope == GridLayoutScope::ColumnSizingOnly) {
        TrackSizes columnSizes = sizeColumnTracks(placedGridItems, columnTrackSizingFunctionsList, rowTrackSizingFunctionsList, gridLayoutState);
        return { { columnSizes, { } }, { } };
    }

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

    return { WTF::move(usedTrackSizes), WTF::move(gridItemRects) };
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

        // Safe alignment must never overflow the start edge, so clamp any negative start-edge offset back to the start.
        if (gridItem.inlineAxisAlignment().overflow() == OverflowAlignment::Safe)
            marginBoxPosition = std::max(0_lu, marginBoxPosition);

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

        // Safe alignment must never overflow the start edge, so clamp any negative start-edge offset back to the start.
        if (gridItem.blockAxisAlignment().overflow() == OverflowAlignment::Safe)
            marginBoxPosition = std::max(0_lu, marginBoxPosition);

        borderBoxPositions.append(marginBoxPosition + blockMargins[gridItemIndex].marginStart);
    }

    return borderBoxPositions;
}

TrackSizingFunctions GridLayout::convertGridTrackSizeToTrackSizingFunctions(const Style::GridTrackSize& gridTrackSize, const Style::ZoomFactor& zoom)
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

    return TrackSizingFunctions { minTrackSizingFunction(), maxTrackSizingFunction(), zoom };
}

// Generates track sizing functions for implicitTracksCount implicit tracks using
// grid-auto-{columns,rows}, cycling forwards through the provided sizes starting from the first.
// FIXME: This produces the correct sizes for trailing implicit tracks (after the explicit grid) and
// for any single-value grid-auto-{columns,rows}, but not for leading implicit tracks (before the
// explicit grid) when grid-auto-{columns,rows} lists multiple track sizes. Per spec the leading
// tracks cycle backwards -- "the last implicit grid track before the explicit grid receives the
// last specified size, and so on backwards" -- whereas this always cycles forwards from the first
// size. https://drafts.csswg.org/css-grid-1/#auto-tracks
TrackSizingFunctionsList GridLayout::generateImplicitTrackSizingFunctions(size_t implicitTracksCount, const Style::GridTrackSizes& gridAutoTrackSizes, const Style::ZoomFactor& zoom)
{
    TrackSizingFunctionsList trackSizingFunctionsForImplicitGrid;
    trackSizingFunctionsForImplicitGrid.reserveInitialCapacity(implicitTracksCount);

    // Cycle through grid-auto-{columns,rows} values using modulo.
    for (size_t i = 0; i < implicitTracksCount; ++i) {
        size_t autoTrackIndex = i % gridAutoTrackSizes.size();
        trackSizingFunctionsForImplicitGrid.append(convertGridTrackSizeToTrackSizingFunctions(gridAutoTrackSizes[autoTrackIndex], zoom));
    }

    return trackSizingFunctionsForImplicitGrid;
}

TrackSizingFunctionsList GridLayout::trackSizingFunctions(size_t totalTracksCount, size_t leadingImplicitTracksCount, const Vector<Style::GridTrackSize>& gridTemplateTrackSizes, const Style::GridTrackSizes& gridAutoTrackSizes, const Style::ZoomFactor& zoom)
{
    auto explicitTracksCount = gridTemplateTrackSizes.size();
    ASSERT_WITH_MESSAGE(totalTracksCount >= leadingImplicitTracksCount + explicitTracksCount, "Total tracks should be at least as many as the leading implicit tracks plus the explicit tracks");

    TrackSizingFunctionsList trackSizingFunctions;
    trackSizingFunctions.reserveInitialCapacity(totalTracksCount);

    // https://drafts.csswg.org/css-grid-1/#auto-tracks
    // Leading implicit tracks are generated before the start of the explicit grid (for items placed
    // with a negative line that resolves before line 1) and are sized by grid-auto-{columns,rows}.
    trackSizingFunctions.appendVector(generateImplicitTrackSizingFunctions(leadingImplicitTracksCount, gridAutoTrackSizes, zoom));

    // https://drafts.csswg.org/css-grid-1/#algo-terms
    // Map explicit tracks from grid-template-{columns,rows}
    for (auto& gridTrackSize : gridTemplateTrackSizes)
        trackSizingFunctions.append(convertGridTrackSizeToTrackSizingFunctions(gridTrackSize, zoom));

    // Generate trailing implicit tracks using grid-auto-{columns,rows}
    // https://drafts.csswg.org/css-grid-1/#auto-tracks
    // "The first track after the last explicitly-sized track receives the first specified size, and so on forwards"
    auto trailingImplicitTracksCount = totalTracksCount - leadingImplicitTracksCount - explicitTracksCount;
    trackSizingFunctions.appendVector(generateImplicitTrackSizingFunctions(trailingImplicitTracksCount, gridAutoTrackSizes, zoom));

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
            [&](const Style::GridTrackBreadthLength::Fixed& fixedValue) {
                return Style::evaluate<LayoutUnit>(fixedValue, trackSizingFunctions.zoom);
            },
            [&](const Style::GridTrackBreadthLength::Percentage& percentageValue) {
                ASSERT(gridContainerInnerInlineSize, "The formatting context should have transformed this track size to auto");
                return Style::evaluate<LayoutUnit>(percentageValue, *gridContainerInnerInlineSize);
            },
            [&](const Style::GridTrackBreadth::Calc calculatedValue) -> LayoutUnit {
                ASSERT(gridContainerInnerInlineSize, "The formatting context should have transformed this track size to auto");
                return Style::evaluate<LayoutUnit>(calculatedValue, *gridContainerInnerInlineSize, trackSizingFunctions.zoom);
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
static LayoutUnit NODELETE oppositeAxisConstraintForTrackSizing(const Vector<LayoutUnit>& oppositeAxisTrackSizes, const WTF::Range<size_t> oppositeAxisSpan)
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

// 1. https://www.w3.org/TR/css-grid-1/#algo-grid-sizing — step 1.
// First, the track sizing algorithm is used to resolve the sizes of the grid columns.
// If calculating the layout of a grid item in this step depends on the available space in the block axis,
// assume the available space that it would have if any row with a definite max track sizing function had
// that size and all other rows were infinite. If both the grid container and all tracks have definite sizes,
// also apply align-content to find the final effective size of any gaps spanned by such items; otherwise
// ignore the effects of track alignment in this estimation.
TrackSizes GridLayout::sizeColumnTracks(const PlacedGridItems& placedGridItems, const TrackSizingFunctionsList& columnTrackSizingFunctionsList,
    const TrackSizingFunctionsList& rowTrackSizingFunctionsList, const GridLayoutState& layoutState) const
{
    auto& layoutConstraints = layoutState.gridLayoutConstraints;

    auto columnFreeSpaceScenario = layoutConstraints.inlineAxis.scenario();
    std::optional<LayoutUnit> inlineAxisAvailableSpace = columnFreeSpaceScenario == AxisConstraint::FreeSpaceScenario::Definite
        ? std::optional(layoutConstraints.inlineAxis.availableSpace())
        : std::nullopt;
    auto rowSizesForFirstColumnSizing = rowSizesForFirstIterationColumnSizing(rowTrackSizingFunctionsList, inlineAxisAvailableSpace);

    auto columnTrackSizingItems = placedGridItems.map([&](const PlacedGridItem& gridItem) -> TrackSizingItem {
        auto rowSpan = WTF::Range<size_t> { gridItem.rowStartLine(), gridItem.rowEndLine() };
        // The inline grid area is indefinite while sizing columns, so the item's cyclic percentage padding resolves against zero.
        auto usedInlineBorderAndPadding = formattingContext().integrationUtils().borderAndPaddingForGridItem(gridItem.layoutBox(), 0_lu).first;
        return { gridItem, gridItem.inlineAxisSizes(), usedInlineBorderAndPadding,
            { gridItem.columnStartLine(), gridItem.columnEndLine() }, oppositeAxisConstraintForTrackSizing(rowSizesForFirstColumnSizing, rowSpan) };
    });

    return TrackSizingAlgorithm::sizeTracks(columnTrackSizingItems, columnTrackSizingFunctionsList,
        layoutConstraints.inlineAxis, GridItemSizingFunctions::inlineAxis(formattingContext().integrationUtils()),
        layoutState.usedColumnGap, layoutState.usedJustifyContent);
}

// 2. https://www.w3.org/TR/css-grid-1/#algo-grid-sizing — step 2.
// Next, the track sizing algorithm resolves the sizes of the grid rows.
// To find the inline-axis available space for any items whose block-axis size contributions
// require it, use the grid column sizes calculated in the previous step.
TrackSizes GridLayout::sizeRowTracks(const PlacedGridItems& placedGridItems, const TrackSizes& columnSizes,
    const TrackSizingFunctionsList& rowTrackSizingFunctionsList, const GridLayoutState& layoutState) const
{
    auto& layoutConstraints = layoutState.gridLayoutConstraints;

    auto rowTrackSizingItems = placedGridItems.map([&](const PlacedGridItem& gridItem) -> TrackSizingItem {
        auto columnSpan = WTF::Range<size_t> { gridItem.columnStartLine(), gridItem.columnEndLine() };
        auto columnConstraint = oppositeAxisConstraintForTrackSizing(columnSizes, columnSpan);
        auto gridAreaInlineSize = GridLayoutUtils::gridAreaDimensionSize(gridItem.columnStartLine(), gridItem.columnEndLine(), columnSizes, layoutState.usedColumnGap);
        auto usedBlockBorderAndPadding = formattingContext().integrationUtils().borderAndPaddingForGridItem(gridItem.layoutBox(), gridAreaInlineSize).second;
        return { gridItem, gridItem.blockAxisSizes(), usedBlockBorderAndPadding,
            { gridItem.rowStartLine(), gridItem.rowEndLine() }, columnConstraint };
    });

    return TrackSizingAlgorithm::sizeTracks(rowTrackSizingItems, rowTrackSizingFunctionsList,
        layoutConstraints.blockAxis, GridItemSizingFunctions::blockAxis(formattingContext()),
        layoutState.usedRowGap, layoutState.usedAlignContent);
}

// https://www.w3.org/TR/css-grid-1/#algo-grid-sizing
UsedTrackSizes GridLayout::performGridSizingAlgorithm(const GridLayoutState& layoutState, const PlacedGridItems& placedGridItems,
    const TrackSizingFunctionsList& columnTrackSizingFunctionsList, const TrackSizingFunctionsList& rowTrackSizingFunctionsList) const
{
    // 1. First, the track sizing algorithm is used to resolve the sizes of the grid columns.
    // If both the grid container and all tracks have definite sizes, also apply align-content
    // to find the final effective size of any gaps spanned by such items; otherwise ignore
    // the effects of track alignment in this estimation.
    auto columnSizes = sizeColumnTracks(placedGridItems, columnTrackSizingFunctionsList, rowTrackSizingFunctionsList, layoutState);

    // 2. Next, the track sizing algorithm resolves the sizes of the grid rows.
    auto rowSizes = sizeRowTracks(placedGridItems, columnSizes, rowTrackSizingFunctionsList, layoutState);

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

// Resolves a grid item's used margins in one axis. This is intended to be used only after track
// sizing is complete — i.e. for grid item sizing and alignment — since the grid area sizes it
// relies on are not known until then.
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

        auto inlineMargins = computeMarginsForAxis(gridItem.inlineAxisSizes(), gridItem.usedZoom());
        auto blockMargins = computeMarginsForAxis(gridItem.blockAxisSizes(), gridItem.usedZoom());

        auto [inlineBorderAndPadding, blockBorderAndPadding] = integrationUtils.borderAndPaddingForGridItem(gridItem.layoutBox(), gridAreaInlineSize);

        auto inlineUsedSize = GridLayoutUtils::inlineUsedSize(gridItem, columnTrackSizingFunctions, inlineBorderAndPadding, gridAreaInlineSize, integrationUtils, inlineMargins);
        usedInlineSizes.append(inlineUsedSize);

        // FIXME: investigate to check if we should use the inlineUsedSize or the size of the grid area in the inline direction.
        auto blockUsedSize = GridLayoutUtils::blockUsedSize(gridItem, rowTrackSizingFunctions, blockBorderAndPadding, gridAreaBlockSize, formattingContext, inlineUsedSize, blockMargins);
        usedBlockSizes.append(blockUsedSize);

        integrationUtils.layoutGridItem(gridItem.layoutBox(), inlineUsedSize, blockUsedSize, gridAreaInlineSize);
    }
    return { usedInlineSizes, usedBlockSizes };
}

} // namespace Layout
} // namespace WebCore
