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
#include "GridFormattingContext.h"

#include "GridLayout.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"
#include "PlacedGridItem.h"
#include "RenderStyleInlines.h"
#include "StylePrimitiveNumeric.h"
#include "UnplacedGridItem.h"

#include <wtf/Vector.h>

namespace WebCore {
namespace Layout {

GridFormattingContext::GridFormattingContext(const ElementBox& gridBox, LayoutState& layoutState)
    : m_gridBox(gridBox)
    , m_globalLayoutState(layoutState)
    , m_integrationUtils(layoutState)
{
}

UnplacedGridItems GridFormattingContext::constructUnplacedGridItems() const
{
    struct GridItem {
        CheckedRef<const ElementBox> layoutBox;
        int order;
    };

    Vector<GridItem> gridItems;
    for (CheckedRef gridItem : childrenOfType<ElementBox>(m_gridBox)) {
        if (gridItem->isOutOfFlowPositioned())
            continue;

        gridItems.append({ gridItem, gridItem->style().order().value });
    }

    std::ranges::stable_sort(gridItems, { }, &GridItem::order);

    UnplacedGridItems unplacedGridItems;
    for (auto& gridItem : gridItems) {
        CheckedRef gridItemStyle = gridItem.layoutBox->style();

        auto gridItemColumnStart = gridItemStyle->gridItemColumnStart();
        auto gridItemColumnEnd = gridItemStyle->gridItemColumnEnd();
        auto gridItemRowStart = gridItemStyle->gridItemRowStart();
        auto gridItemRowEnd = gridItemStyle->gridItemRowEnd();

        UnplacedGridItem unplacedGridItem {
            gridItem.layoutBox,
            gridItemColumnStart,
            gridItemColumnEnd,
            gridItemRowStart,
            gridItemRowEnd
        };

        // Check if this item is fully explicitly positioned
        bool fullyExplicitlyPositionedItem = gridItemColumnStart.isExplicit()
            && gridItemColumnEnd.isExplicit()
            && gridItemRowStart.isExplicit()
            && gridItemRowEnd.isExplicit();

        // FIXME: support definite row/column positioning
        // We should place items with definite row or column positions
        // but currently we only support fully explicitly positioned items.
        // See: https://www.w3.org/TR/css-grid-1/#auto-placement-algo
        if (fullyExplicitlyPositionedItem) {
            unplacedGridItems.nonAutoPositionedItems.append(unplacedGridItem);
        } else if (unplacedGridItem.hasDefiniteRowPosition()) {
            unplacedGridItems.definiteRowPositionedItems.append(unplacedGridItem);
        } else {
            unplacedGridItems.autoPositionedItems.append(unplacedGridItem);
        }
    }
    return unplacedGridItems;
}

void GridFormattingContext::layout(GridLayoutConstraints layoutConstraints)
{
    auto unplacedGridItems = constructUnplacedGridItems();
    GridLayout { *this }.layout(layoutConstraints, unplacedGridItems);
}

PlacedGridItems GridFormattingContext::constructPlacedGridItems(const GridAreas& gridAreas) const
{
    PlacedGridItems placedGridItems;
    placedGridItems.reserveInitialCapacity(gridAreas.size());
    for (auto [ unplacedGridItem, gridAreaLines ] : gridAreas) {

        CheckedRef gridItemStyle = unplacedGridItem.m_layoutBox->style();

        auto usedJustifySelf = [&] {
            if (auto gridItemJustifySelf = gridItemStyle->justifySelf(); gridItemJustifySelf.position() != ItemPosition::Auto)
                return gridItemJustifySelf;
            auto& formattingContextRootStyle = root().style();
            return formattingContextRootStyle.justifyItems();
        };

        auto usedAlignSelf = [&] {
            if (auto gridItemAlignSelf = gridItemStyle->alignSelf(); gridItemAlignSelf.position() != ItemPosition::Auto)
                return gridItemAlignSelf;
            auto& formattingContextRootStyle = root().style();
            return formattingContextRootStyle.alignItems();
        };

        PlacedGridItem::ComputedSizes inlineAxisSizes {
            gridItemStyle->width(),
            gridItemStyle->minWidth(),
            gridItemStyle->maxWidth(),
            gridItemStyle->marginLeft(),
            gridItemStyle->marginRight()
        };

        PlacedGridItem::ComputedSizes blockAxisSizes {
            gridItemStyle->height(),
            gridItemStyle->minHeight(),
            gridItemStyle->maxHeight(),
            gridItemStyle->marginTop(),
            gridItemStyle->marginBottom()
        };

        placedGridItems.constructAndAppend(unplacedGridItem, gridAreaLines, inlineAxisSizes, blockAxisSizes, usedJustifySelf(), usedAlignSelf());
    }
    return placedGridItems;
}

const BoxGeometry GridFormattingContext::geometryForGridItem(const ElementBox& gridItem) const
{
    ASSERT(gridItem.isGridItem());
    return layoutState().geometryForBox(gridItem);
}

} // namespace Layout
} // namespace WebCore
