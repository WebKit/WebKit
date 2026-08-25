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

// 8.5. Grid Item Placement Algorithm.
// https://drafts.csswg.org/css-grid-1/#auto-placement-algo
//
// Step 3 (determining the columns in the implicit grid) is handled while the grid is built, in
// ImplicitGrid::createInitialGrid().
GridItemPlacementResult GridItemPlacer::placeItems(const UnplacedGridItems& unplacedGridItems, ImplicitGrid& implicitGrid) const
{
    // 1. Position anything that's not auto-positioned.
    for (auto& nonAutoPositionedItem : unplacedGridItems.nonAutoPositionedItems)
        implicitGrid.insertUnplacedGridItem(nonAutoPositionedItem);

    // 2. Process the items locked to a given row.
    for (auto& definiteRowPositionedItem : unplacedGridItems.definiteRowPositionedItems)
        implicitGrid.insertDefiniteRowItem(definiteRowPositionedItem, m_autoFlowOptions);

    if (!unplacedGridItems.autoPositionedItems.isEmpty()) {
        // 4. Process auto-positioned items
        implicitGrid.insertAutoPositionedItems(unplacedGridItems.autoPositionedItems, m_autoFlowOptions);
    }

    return { implicitGrid.gridAreas(), implicitGrid.columnsCount(), implicitGrid.rowsCount() };
}

} // namespace Layout
} // namespace WebCore
