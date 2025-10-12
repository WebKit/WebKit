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
#include "RenderStyleInlines.h"
#include "LayoutBoxGeometry.h"
#include "LayoutElementBox.h"
#include "NotImplemented.h"
#include "PlacedGridItem.h"
#include "TrackSizingAlgorithm.h"
#include "TrackSizingFunctions.h"
#include "UnplacedGridItem.h"
#include "UsedTrackSizes.h"
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

GridLayout::GridLayout(const GridFormattingContext& gridFormattingContext)
    : m_gridFormattingContext(gridFormattingContext)
{
}

// 8.5. Grid Item Placement Algorithm.
// https://drafts.csswg.org/css-grid-1/#auto-placement-algo
auto GridLayout::placeGridItems(const UnplacedGridItems& unplacedGridItems, const Vector<Style::GridTrackSize>& gridTemplateColumnsTrackSizes,
    const Vector<Style::GridTrackSize>& gridTemplateRowsTrackSizes, GridAutoFlowOptions autoFlowOptions)
{
    struct Result {
        GridAreas gridAreas;
        size_t implicitGridColumnsCount;
        size_t implicitGridRowsCount;
    };

    ImplicitGrid implicitGrid(gridTemplateColumnsTrackSizes.size(), gridTemplateRowsTrackSizes.size());

    // 1. Position anything that's not auto-positioned.
    auto& nonAutoPositionedGridItems = unplacedGridItems.nonAutoPositionedItems;
    for (auto& nonAutoPositionedItem : nonAutoPositionedGridItems)
        implicitGrid.insertUnplacedGridItem(nonAutoPositionedItem);

    // 2. Process the items locked to a given row.
    // Phase 1: Only single-cell items within explicit grid bounds
    auto& definiteRowPositionedGridItems = unplacedGridItems.definiteRowPositionedItems;
    HashMap<unsigned, unsigned, DefaultHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> rowCursors;
    for (auto& definiteRowPositionedItem : definiteRowPositionedGridItems)
        implicitGrid.insertDefiniteRowItem(definiteRowPositionedItem, autoFlowOptions, &rowCursors);

    // 3. FIXME: Process auto-positioned items (not implemented yet)
    ASSERT(unplacedGridItems.autoPositionedItems.isEmpty());

    ASSERT(implicitGrid.columnsCount() == gridTemplateColumnsTrackSizes.size() && implicitGrid.rowsCount() == gridTemplateRowsTrackSizes.size(),
        "Since we currently only support placing items which fit within the explicit grid, the size of the implicit grid should match the passed in sizes.");

    return Result { implicitGrid.gridAreas(), implicitGrid.columnsCount(), implicitGrid.rowsCount() };
}

// https://drafts.csswg.org/css-grid-1/#layout-algorithm
std::pair<UsedTrackSizes, GridItemRects> GridLayout::layout(GridFormattingContext::GridLayoutConstraints, const UnplacedGridItems& unplacedGridItems)
{
    CheckedRef gridContainerStyle = this->gridContainerStyle();
    auto& gridTemplateColumnsTrackSizes = gridContainerStyle->gridTemplateColumns().sizes;
    auto& gridTemplateRowsTrackSizes = gridContainerStyle->gridTemplateRows().sizes;

    // 1. Run the Grid Item Placement Algorithm to resolve the placement of all grid items in the grid.
    GridAutoFlowOptions autoFlowOptions {
        .strategy = gridContainerStyle->isGridAutoFlowAlgorithmDense() ? PackingStrategy::Dense : PackingStrategy::Sparse,
        .direction = gridContainerStyle->isGridAutoFlowDirectionRow() ? GridAutoFlowDirection::Row : GridAutoFlowDirection::Column
    };
    auto [ gridAreas, implicitGridColumnsCount, implicitGridRowsCount ] = placeGridItems(unplacedGridItems, gridTemplateColumnsTrackSizes, gridTemplateRowsTrackSizes, autoFlowOptions);
    auto placedGridItems = formattingContext().constructPlacedGridItems(gridAreas);

    auto columnTrackSizingFunctionsList = trackSizingFunctions(implicitGridColumnsCount, gridTemplateColumnsTrackSizes);
    auto rowTrackSizingFunctionsList = trackSizingFunctions(implicitGridRowsCount, gridTemplateRowsTrackSizes);

    // 3. Given the resulting grid container size, run the Grid Sizing Algorithm to size the grid.
    UsedTrackSizes usedTrackSizes = performGridSizingAlgorithm(placedGridItems, columnTrackSizingFunctionsList, rowTrackSizingFunctionsList);

    // 4. Lay out the grid items into their respective containing blocks. Each grid area’s
    // width and height are considered definite for this purpose.
    auto [ usedInlineSizes, usedBlockSizes ] = layoutGridItems(placedGridItems, usedTrackSizes);

    // https://drafts.csswg.org/css-grid-1/#alignment
    auto usedInlineMargins = computeInlineMargins(placedGridItems);
    auto usedBlockMargins = computeBlockMargins(placedGridItems);

    // https://drafts.csswg.org/css-grid-1/#alignment
    // After a grid container’s grid tracks have been sized, and the dimensions of all grid items
    // are finalized, grid items can be aligned within their grid areas.
    auto inlineAxisPositions = performInlineAxisSelfAlignment(placedGridItems, usedInlineMargins);
    auto blockAxisPositions = performBlockAxisSelfAlignment(placedGridItems, usedBlockMargins);

    GridItemRects gridItemRects;
    gridItemRects.reserveInitialCapacity(placedGridItems.size());

    for (size_t gridItemIndex = 0; gridItemIndex < placedGridItems.size(); ++gridItemIndex) {
        auto borderBoxRect =  LayoutRect { inlineAxisPositions[gridItemIndex], blockAxisPositions[gridItemIndex],
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

    return { usedTrackSizes, gridItemRects };
}

GridLayout::BorderBoxPositions GridLayout::performInlineAxisSelfAlignment(const PlacedGridItems& placedGridItems, const Vector<UsedMargins>& inlineMargins)
{
    BorderBoxPositions borderBoxPositions;
    borderBoxPositions.reserveInitialCapacity(placedGridItems.size());

    auto computeMarginBoxPosition = [](const PlacedGridItem& placedGridItem) -> LayoutUnit {
        switch (placedGridItem.inlineAxisAlignment().position()) {
        case ItemPosition::FlexStart:
        case ItemPosition::SelfStart:
        case ItemPosition::Start:
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

GridLayout::BorderBoxPositions GridLayout::performBlockAxisSelfAlignment(const PlacedGridItems& placedGridItems, const Vector<UsedMargins>& blockMargins)
{
    BorderBoxPositions borderBoxPositions;
    borderBoxPositions.reserveInitialCapacity(placedGridItems.size());

    auto computeMarginBoxPosition = [](const PlacedGridItem& placedGridItem) -> LayoutUnit {
        switch (placedGridItem.blockAxisAlignment().position()) {
        case ItemPosition::FlexStart:
        case ItemPosition::SelfStart:
        case ItemPosition::Start:
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
    const TrackSizingFunctionsList& columnTrackSizingFunctionsList, const TrackSizingFunctionsList& rowTrackSizingFunctionsList)
{
    // 1. First, the track sizing algorithm is used to resolve the sizes of the grid columns.
    auto columnSizes = TrackSizingAlgorithm::sizeTracks(placedGridItems, columnTrackSizingFunctionsList);

    // 2. Next, the track sizing algorithm resolves the sizes of the grid rows.
    auto rowSizes = TrackSizingAlgorithm::sizeTracks(placedGridItems, rowTrackSizingFunctionsList);

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
Vector<UsedMargins> GridLayout::computeInlineMargins(const PlacedGridItems& placedGridItems)
{
    return placedGridItems.map([](const PlacedGridItem& placedGridItem) {
        auto& inlineAxisSizes = placedGridItem.inlineAxisSizes();

        auto marginStart = [&] -> LayoutUnit {
            if (auto fixedMarginStart = inlineAxisSizes.marginStart.tryFixed())
                return LayoutUnit { fixedMarginStart->resolveZoom(Style::ZoomNeeded { }) };

            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        };

        auto marginEnd = [&] -> LayoutUnit {
            if (auto fixedMarginEnd = inlineAxisSizes.marginEnd.tryFixed())
                return LayoutUnit { fixedMarginEnd->resolveZoom(Style::ZoomNeeded { }) };

            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        };

        return UsedMargins { marginStart(), marginEnd() };
    });
}

// https://drafts.csswg.org/css-grid-1/#auto-margins
Vector<UsedMargins> GridLayout::computeBlockMargins(const PlacedGridItems& placedGridItems)
{
    return placedGridItems.map([](const PlacedGridItem& placedGridItem) {
        auto& blockAxisSizes = placedGridItem.blockAxisSizes();

        auto marginStart = [&] -> LayoutUnit {
            if (auto fixedMarginStart = blockAxisSizes.marginStart.tryFixed())
                return LayoutUnit { fixedMarginStart->resolveZoom(Style::ZoomNeeded { }) };

            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        };

        auto marginEnd = [&] -> LayoutUnit {
            if (auto fixedMarginEnd = blockAxisSizes.marginEnd.tryFixed())
                return LayoutUnit { fixedMarginEnd->resolveZoom(Style::ZoomNeeded { }) };

            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        };

        return UsedMargins { marginStart(), marginEnd() };
    });
}

// https://drafts.csswg.org/css-grid-1/#grid-item-sizing
std::pair<GridLayout::UsedInlineSizes, GridLayout::UsedBlockSizes> GridLayout::layoutGridItems(const PlacedGridItems& placedGridItems, const UsedTrackSizes&) const
{
    UsedInlineSizes usedInlineSizes;
    UsedBlockSizes usedBlockSizes;
    auto gridItemsCount = placedGridItems.size();
    usedInlineSizes.reserveInitialCapacity(gridItemsCount);
    usedBlockSizes.reserveInitialCapacity(gridItemsCount);

    auto& formattingContext = this->formattingContext();
    auto& integrationUtils = formattingContext.integrationUtils();
    for (auto& gridItem : placedGridItems) {
        auto& gridItemBoxGeometry = formattingContext.geometryForGridItem(gridItem.layoutBox());

        auto usedInlineSizeForGridItem = GridLayoutUtils::usedInlineSizeForGridItem(gridItem) + gridItemBoxGeometry.horizontalBorderAndPadding();
        usedInlineSizes.append(usedInlineSizeForGridItem);

        auto usedBlockSizeForGridItem = GridLayoutUtils::usedBlockSizeForGridItem(gridItem) + gridItemBoxGeometry.verticalBorderAndPadding();
        usedBlockSizes.append(usedBlockSizeForGridItem);

        auto& layoutBox = gridItem.layoutBox();
        integrationUtils.layoutWithFormattingContextForBox(layoutBox, usedInlineSizeForGridItem, usedBlockSizeForGridItem);
    }
    return { usedInlineSizes, usedBlockSizes };
}

const ElementBox& GridLayout::gridContainer() const
{
    return m_gridFormattingContext->root();
}

const RenderStyle& GridLayout::gridContainerStyle() const
{
    return gridContainer().style();
}

} // namespace Layout
} // namespace WebCore
