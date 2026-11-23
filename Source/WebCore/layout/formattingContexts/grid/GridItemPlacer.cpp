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

namespace WebCore::Layout {

GridItemPlacer::GridItemPlacer(GridAutoFlowOptions autoFlowOptions)
    : m_autoFlowOptions(autoFlowOptions)
{
}

static UnplacedGridItems constructUnplacedGridItems(const LogicalGridItems& logicalGridItems, LeadingImplicitTracks leadingImplicitTracks, size_t explicitColumnsCount, size_t explicitRowsCount)
{
    return WTF::map(logicalGridItems, [&](auto& gridItem) {
        CheckedRef gridItemStyle = gridItem->style();
        return UnplacedGridItem {
            gridItem,
            gridItemStyle->gridItemColumnStart(),
            gridItemStyle->gridItemColumnEnd(),
            gridItemStyle->gridItemRowStart(),
            gridItemStyle->gridItemRowEnd(),
            explicitColumnsCount,
            explicitRowsCount,
            leadingImplicitTracks.columnsCount,
            leadingImplicitTracks.rowsCount
        };
    });
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

    // Grid areas are kept in order-modified document order. The steps below must run in spec order,
    // since each one depends on the cells occupied by the ones before it, so each step fills in the
    // lines of its own items.
    auto gridAreas = WTF::map(WTF::move(unplacedGridItems), [](UnplacedGridItem&& item) {
        return std::pair { WTF::move(item), GridAreaLines { } };
    });
    auto gridAreasFor = [&](auto predicate) {
        return gridAreas | std::views::filter([predicate](auto& gridArea) {
            return predicate(gridArea.first);
        });
    };

    // 1. Position anything that's not auto-positioned.
    for (auto& [item, lines] : gridAreasFor(isNonAutoPositioned))
        lines = implicitGrid.insertUnplacedGridItem(item);

    // 2. Process the items locked to a given row.
    for (auto& [item, lines] : gridAreasFor(isDefiniteRowPositioned))
        lines = implicitGrid.insertDefiniteRowItem(item, m_autoFlowOptions);

    // 4. Position the remaining grid items.
    for (auto& [item, lines] : gridAreasFor(isAutoPositioned))
        lines = implicitGrid.insertAutoPositionedItem(item, m_autoFlowOptions);

    // Every item belongs to exactly one of the steps above, so none is left with its empty
    // placeholder lines.
    ASSERT(std::ranges::all_of(gridAreas, [](auto& gridArea) {
        auto& lines = gridArea.second;
        return lines.columnEndLine > lines.columnStartLine && lines.rowEndLine > lines.rowStartLine;
    }));

    return { WTF::move(gridAreas), implicitGrid.columnsCount(), implicitGrid.rowsCount() };
}

} // namespace WebCore::Layout
