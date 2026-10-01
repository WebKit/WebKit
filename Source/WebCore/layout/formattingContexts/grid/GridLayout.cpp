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
#include "LayoutBoxGeometry.h"
#include "LayoutElementBox.h"
#include "PlacedGridItem.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "TrackSizingFunctions.h"
#include "UnplacedGridItem.h"
#include "UsedTrackSizes.h"
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

GridItemRects GridLayout::layout(const PlacedGridItems& placedGridItems, const TrackSizingFunctionsList& columnTrackSizingFunctionsList,
    const TrackSizingFunctionsList& rowTrackSizingFunctionsList, const UsedTrackSizes& usedTrackSizes, const GridLayoutState& gridLayoutState)
{
    auto& formattingContext = this->formattingContext();

    CheckedRef formattingContextRootStyle = formattingContext.root().style();
    auto gridAreaSizes = computeGridAreaSizes(placedGridItems, gridLayoutState.usedColumnGap, gridLayoutState.usedRowGap, usedTrackSizes);

    auto [ usedInlineSizes, usedBlockSizes ] = layoutGridItems(placedGridItems, gridAreaSizes, columnTrackSizingFunctionsList, rowTrackSizingFunctionsList);

    // https://drafts.csswg.org/css-grid-1/#alignment
    auto usedInlineMargins = computeInlineMargins(placedGridItems);
    auto usedBlockMargins = computeBlockMargins(placedGridItems);

    // https://drafts.csswg.org/css-grid-1/#alignment
    // After a grid container’s grid tracks have been sized, and the dimensions of all grid items
    // are finalized, grid items can be aligned within their grid areas.
    auto inlineAxisPositions = performInlineAxisSelfAlignment(placedGridItems, usedInlineMargins, usedInlineSizes, gridAreaSizes.inlineSizes);
    auto blockAxisPositions = performBlockAxisSelfAlignment(placedGridItems, usedBlockMargins, usedBlockSizes, gridAreaSizes.blockSizes);

    return computeGridItemRects(placedGridItems, inlineAxisPositions, blockAxisPositions, usedInlineSizes, usedBlockSizes, usedInlineMargins, usedBlockMargins);
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

// https://drafts.csswg.org/css-grid-1/#auto-margins
Vector<UsedMargins> GridLayout::computeInlineMargins(const PlacedGridItems& placedGridItems)
{
    return placedGridItems.map([](const PlacedGridItem& placedGridItem) {
        return GridLayoutUtils::usedMarginsForAxis(placedGridItem, placedGridItem.inlineAxisSizes());
    });
}

// https://drafts.csswg.org/css-grid-1/#auto-margins
Vector<UsedMargins> GridLayout::computeBlockMargins(const PlacedGridItems& placedGridItems)
{
    return placedGridItems.map([](const PlacedGridItem& placedGridItem) {
        return GridLayoutUtils::usedMarginsForAxis(placedGridItem, placedGridItem.blockAxisSizes());
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

        auto inlineMargins = GridLayoutUtils::usedMarginsForAxis(gridItem, gridItem.inlineAxisSizes());
        auto blockMargins = GridLayoutUtils::usedMarginsForAxis(gridItem, gridItem.blockAxisSizes());

        auto [inlineBorderAndPadding, blockBorderAndPadding] = integrationUtils.borderAndPaddingForGridItem(gridItem.layoutBox(), gridAreaInlineSize);

        auto inlineUsedSize = GridLayoutUtils::inlineUsedSize(gridItem, columnTrackSizingFunctions, inlineBorderAndPadding, gridAreaInlineSize, integrationUtils, inlineMargins);
        usedInlineSizes.append(inlineUsedSize);

        // When the grid item has a fit-content block height RenderGrid just calls layout on it, so
        // we do the same to match behavior instead of running layout to compute the min-content and
        // max-content sizes as well which is what blockUsedSize does.
        bool hasFitContentBlockSize = GridLayoutUtils::hasFitContentBlockSize(gridItem);
        if (!hasFitContentBlockSize) {
            auto usedBlockSize = GridLayoutUtils::blockUsedSize(gridItem, rowTrackSizingFunctions, blockBorderAndPadding, gridAreaBlockSize, formattingContext, gridAreaInlineSize, blockMargins);
            integrationUtils.layoutGridItem(gridItem.layoutBox(), inlineUsedSize, usedBlockSize, gridAreaInlineSize);
            usedBlockSizes.append(usedBlockSize);
        } else {
            integrationUtils.layoutGridItem(gridItem.layoutBox(), inlineUsedSize, { }, gridAreaInlineSize);
            // The item's padding in its box geometry is resolved against the grid container rather than
            // the grid area, so only take the content box size from it.
            usedBlockSizes.append(formattingContext.geometryForGridItem(gridItem.layoutBox()).contentBoxHeight() + blockBorderAndPadding);
        }
    }
    return { usedInlineSizes, usedBlockSizes };
}

} // namespace Layout
} // namespace WebCore
