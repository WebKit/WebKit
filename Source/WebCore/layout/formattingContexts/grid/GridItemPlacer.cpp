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
#include "NamedLineResolver.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {
namespace Layout {

GridItemPlacer::GridItemPlacer(GridAutoFlowOptions autoFlowOptions)
    : m_autoFlowOptions(autoFlowOptions)
{
}

// The only place that maps between the columns and rows of the style and GridAreaLines and the non-auto-flow and auto-flow
// tracks the placement works in (see UnplacedGridItem.h). For grid-auto-flow: row the auto-flow tracks are the rows and for
// column they are the columns.
static UnplacedGridItems constructUnplacedGridItems(const LogicalGridItems& logicalGridItems, size_t leadingNonAutoFlowTracksCount, size_t leadingAutoFlowTracksCount, const NamedLineResolver& nonAutoFlowNamedLineResolver, const NamedLineResolver& autoFlowNamedLineResolver, size_t maxTracks, GridAutoFlowDirection direction)
{
    UnplacedGridItems unplacedGridItems;
    for (auto& gridItem : logicalGridItems) {
        CheckedRef gridItemStyle = gridItem->style();

        auto gridItemColumnStart = gridItemStyle->gridItemColumnStart();
        auto gridItemColumnEnd = gridItemStyle->gridItemColumnEnd();
        auto gridItemRowStart = gridItemStyle->gridItemRowStart();
        auto gridItemRowEnd = gridItemStyle->gridItemRowEnd();
        bool isColumnFlow = direction == GridAutoFlowDirection::Column;
        auto gridItemNonAutoFlowStart = isColumnFlow ? gridItemRowStart : gridItemColumnStart;
        auto gridItemNonAutoFlowEnd = isColumnFlow ? gridItemRowEnd : gridItemColumnEnd;
        auto gridItemAutoFlowStart = isColumnFlow ? gridItemColumnStart : gridItemRowStart;
        auto gridItemAutoFlowEnd = isColumnFlow ? gridItemColumnEnd : gridItemRowEnd;

        UnplacedGridItem unplacedGridItem {
            gridItem,
            gridItemNonAutoFlowStart,
            gridItemNonAutoFlowEnd,
            gridItemAutoFlowStart,
            gridItemAutoFlowEnd,
            nonAutoFlowNamedLineResolver,
            autoFlowNamedLineResolver,
            leadingNonAutoFlowTracksCount,
            leadingAutoFlowTracksCount,
            maxTracks
        };

        // https://drafts.csswg.org/css-grid-1/#auto-placement-algo
        if (unplacedGridItem.hasDefiniteNonAutoFlowPosition() && unplacedGridItem.hasDefiniteAutoFlowPosition())
            unplacedGridItems.nonAutoPositionedItems.append(WTF::move(unplacedGridItem));
        else if (unplacedGridItem.hasDefiniteAutoFlowPosition())
            unplacedGridItems.definiteAutoFlowPositionedItems.append(WTF::move(unplacedGridItem));
        else
            unplacedGridItems.autoPositionedItems.append(WTF::move(unplacedGridItem));
    }
    return unplacedGridItems;
}

static GridAreaLines toGridAreaLines(const FlowRelativeAreaLines& area, GridAutoFlowDirection direction)
{
    if (direction == GridAutoFlowDirection::Column)
        return { area.autoFlowStartLine, area.autoFlowEndLine, area.nonAutoFlowStartLine, area.nonAutoFlowEndLine };
    return { area.nonAutoFlowStartLine, area.nonAutoFlowEndLine, area.autoFlowStartLine, area.autoFlowEndLine };
}

// 8.5. Grid Item Placement Algorithm.
// https://drafts.csswg.org/css-grid-1/#auto-placement-algo
// "To aid in clarity, this algorithm is written with the assumption that grid-auto-flow has row specified.
// If it is instead set to column, swap all mentions of rows and columns, inline and block, etc. in this algorithm."
// The auto-flow tracks are the rows for row and the columns for column, so for column flow the item's column properties
// give its auto-flow position and its row properties the non-auto-flow position, and the same goes for the track counts.
//
// Step 3 (determining the non-auto-flow tracks in the implicit grid, the columns for grid-auto-flow: row) is handled while the grid is built, in
// ImplicitGrid::createInitialGrid().
GridItemPlacementResult GridItemPlacer::placeItems(const LogicalGridItems& logicalGridItems, LeadingImplicitTracks leadingImplicitTracks, const NamedLineResolver& columnNamedLineResolver, const NamedLineResolver& rowNamedLineResolver, size_t maxTracks) const
{
    auto direction = m_autoFlowOptions.direction;
    bool isColumnFlow = direction == GridAutoFlowDirection::Column;
    auto leadingNonAutoFlowTracksCount = isColumnFlow ? leadingImplicitTracks.rowsCount : leadingImplicitTracks.columnsCount;
    auto leadingAutoFlowTracksCount = isColumnFlow ? leadingImplicitTracks.columnsCount : leadingImplicitTracks.rowsCount;

    // The resolvers go with the style axis that the properties come from, like the track counts.
    auto& nonAutoFlowNamedLineResolver = isColumnFlow ? rowNamedLineResolver : columnNamedLineResolver;
    auto& autoFlowNamedLineResolver = isColumnFlow ? columnNamedLineResolver : rowNamedLineResolver;
    auto explicitNonAutoFlowTracksCount = nonAutoFlowNamedLineResolver.explicitTrackCount();
    auto explicitAutoFlowTracksCount = autoFlowNamedLineResolver.explicitTrackCount();

    auto unplacedGridItems = constructUnplacedGridItems(logicalGridItems, leadingNonAutoFlowTracksCount, leadingAutoFlowTracksCount, nonAutoFlowNamedLineResolver, autoFlowNamedLineResolver, maxTracks, direction);
    auto implicitGrid = ImplicitGrid::createInitialGrid(unplacedGridItems, leadingNonAutoFlowTracksCount, leadingAutoFlowTracksCount, explicitNonAutoFlowTracksCount, explicitAutoFlowTracksCount, maxTracks);

    GridAreas gridAreas;
    gridAreas.reserveInitialCapacity(unplacedGridItems.nonAutoPositionedItems.size()
        + unplacedGridItems.definiteAutoFlowPositionedItems.size()
        + unplacedGridItems.autoPositionedItems.size());

    // 1. Position anything that's not auto-positioned.
    for (auto& nonAutoPositionedItem : unplacedGridItems.nonAutoPositionedItems)
        gridAreas.constructAndAppend(nonAutoPositionedItem, toGridAreaLines(implicitGrid.insertUnplacedGridItem(nonAutoPositionedItem), direction));

    // 2. Process the items locked to a given auto-flow track.
    for (auto& definiteAutoFlowPositionedItem : unplacedGridItems.definiteAutoFlowPositionedItems)
        gridAreas.constructAndAppend(definiteAutoFlowPositionedItem, toGridAreaLines(implicitGrid.insertDefiniteAutoFlowItem(definiteAutoFlowPositionedItem, m_autoFlowOptions.strategy), direction));

    // 4. Position the remaining grid items.
    for (auto& autoPositionedItem : unplacedGridItems.autoPositionedItems)
        gridAreas.constructAndAppend(autoPositionedItem, toGridAreaLines(implicitGrid.insertAutoPositionedItem(autoPositionedItem, m_autoFlowOptions.strategy), direction));

    if (isColumnFlow)
        return { WTF::move(gridAreas), implicitGrid.autoFlowTracksCount(), implicitGrid.nonAutoFlowTracksCount() };
    return { WTF::move(gridAreas), implicitGrid.nonAutoFlowTracksCount(), implicitGrid.autoFlowTracksCount() };
}

} // namespace Layout
} // namespace WebCore
