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
#include "GridItemPlacer.h"

#include "ImplicitGrid.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {
namespace Layout {

GridItemPlacer::GridItemPlacer(GridAutoFlowOptions autoFlowOptions)
    : m_autoFlowOptions(autoFlowOptions)
{
}

static UnplacedGridItems constructUnplacedGridItems(const LogicalGridItems& logicalGridItems, LeadingImplicitTracks leadingImplicitTracks, size_t explicitColumnsCount, size_t explicitRowsCount)
{
    UnplacedGridItems unplacedGridItems;
    for (auto& gridItem : logicalGridItems) {
        CheckedRef gridItemStyle = gridItem->style();

        auto gridItemColumnStart = gridItemStyle->gridItemColumnStart();
        auto gridItemColumnEnd = gridItemStyle->gridItemColumnEnd();
        auto gridItemRowStart = gridItemStyle->gridItemRowStart();
        auto gridItemRowEnd = gridItemStyle->gridItemRowEnd();

        UnplacedGridItem unplacedGridItem {
            gridItem,
            gridItemColumnStart,
            gridItemColumnEnd,
            gridItemRowStart,
            gridItemRowEnd,
            explicitColumnsCount,
            explicitRowsCount,
            leadingImplicitTracks.columnsCount,
            leadingImplicitTracks.rowsCount
        };

        // https://drafts.csswg.org/css-grid-1/#auto-placement-algo
        if (unplacedGridItem.hasDefiniteColumnPosition() && unplacedGridItem.hasDefiniteRowPosition())
            unplacedGridItems.nonAutoPositionedItems.append(unplacedGridItem);
        else if (unplacedGridItem.hasDefiniteRowPosition())
            unplacedGridItems.definiteRowPositionedItems.append(unplacedGridItem);
        else
            unplacedGridItems.autoPositionedItems.append(unplacedGridItem);
    }
    return unplacedGridItems;
}

// 8.5. Grid Item Placement Algorithm.
// https://drafts.csswg.org/css-grid-1/#auto-placement-algo
//
// Step 3 (determining the columns in the implicit grid) is handled while the grid is built, in
// ImplicitGrid::createInitialGrid().
GridItemPlacementResult GridItemPlacer::placeItems(const LogicalGridItems& logicalGridItems, LeadingImplicitTracks leadingImplicitTracks, size_t explicitColumnsCount, size_t explicitRowsCount) const
{
    auto unplacedGridItems = constructUnplacedGridItems(logicalGridItems, leadingImplicitTracks, explicitColumnsCount, explicitRowsCount);
    auto implicitGrid = ImplicitGrid::createInitialGrid(unplacedGridItems, leadingImplicitTracks, explicitColumnsCount, explicitRowsCount);

    GridAreas gridAreas;
    gridAreas.reserveInitialCapacity(unplacedGridItems.nonAutoPositionedItems.size()
        + unplacedGridItems.definiteRowPositionedItems.size()
        + unplacedGridItems.autoPositionedItems.size());

    // 1. Position anything that's not auto-positioned.
    for (auto& nonAutoPositionedItem : unplacedGridItems.nonAutoPositionedItems)
        gridAreas.constructAndAppend(nonAutoPositionedItem, implicitGrid.insertUnplacedGridItem(nonAutoPositionedItem));

    // 2. Process the items locked to a given row.
    for (auto& definiteRowPositionedItem : unplacedGridItems.definiteRowPositionedItems)
        gridAreas.constructAndAppend(definiteRowPositionedItem, implicitGrid.insertDefiniteRowItem(definiteRowPositionedItem, m_autoFlowOptions));

    // 4. Position the remaining grid items.
    for (auto& autoPositionedItem : unplacedGridItems.autoPositionedItems)
        gridAreas.constructAndAppend(autoPositionedItem, implicitGrid.insertAutoPositionedItem(autoPositionedItem, m_autoFlowOptions));

    return { WTF::move(gridAreas), implicitGrid.columnsCount(), implicitGrid.rowsCount() };
}

} // namespace Layout
} // namespace WebCore
