/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "TableFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBox.h"
#include "InlineFormattingState.h"
#include "LayoutContext.h"
#include "LayoutDescendantIterator.h"
#include "LayoutInitialContainingBlock.h"
#include "TableFormattingState.h"

namespace WebCore {
namespace Layout {

LayoutUnit TableFormattingContext::Geometry::cellHeigh(const ContainerBox& cellBox) const
{
    ASSERT(cellBox.isInFlow());
    return std::max(computedHeight(cellBox).valueOr(0_lu), contentHeightForFormattingContextRoot(cellBox));
}

Edges TableFormattingContext::Geometry::computedCellBorder(const TableGrid::Cell& cell) const
{
    auto& cellBox = cell.box();
    auto border = computedBorder(cellBox);
    auto collapsedBorder = m_grid.collapsedBorder();
    if (!collapsedBorder)
        return border;

    // We might want to cache these collapsed borders on the grid.
    auto cellPosition = cell.position();
    // Collapsed border left from table and adjacent cells.
    if (!cellPosition.column)
        border.horizontal.left = collapsedBorder->horizontal.left / 2;
    else {
        auto adjacentBorderRight = computedBorder(m_grid.slot({ cellPosition.column - 1, cellPosition.row })->cell().box()).horizontal.right;
        border.horizontal.left = std::max(border.horizontal.left, adjacentBorderRight) / 2;
    }
    // Collapsed border right from table and adjacent cells.
    if (cellPosition.column == m_grid.columns().size() - 1)
        border.horizontal.right = collapsedBorder->horizontal.right / 2;
    else {
        auto adjacentBorderLeft = computedBorder(m_grid.slot({ cellPosition.column + 1, cellPosition.row })->cell().box()).horizontal.left;
        border.horizontal.right = std::max(border.horizontal.right, adjacentBorderLeft) / 2;
    }
    // Collapsed border top from table, row and adjacent cells.
    auto& rows = m_grid.rows().list();
    if (!cellPosition.row)
        border.vertical.top = collapsedBorder->vertical.top / 2;
    else {
        auto adjacentBorderBottom = computedBorder(m_grid.slot({ cellPosition.column, cellPosition.row - 1 })->cell().box()).vertical.bottom;
        auto adjacentRowBottom = computedBorder(rows[cellPosition.row - 1].box()).vertical.bottom;
        auto adjacentCollapsedBorder = std::max(adjacentBorderBottom, adjacentRowBottom);
        border.vertical.top = std::max(border.vertical.top, adjacentCollapsedBorder) / 2;
    }
    // Collapsed border bottom from table, row and adjacent cells.
    if (cellPosition.row == m_grid.rows().size() - 1)
        border.vertical.bottom = collapsedBorder->vertical.bottom / 2;
    else {
        auto adjacentBorderTop = computedBorder(m_grid.slot({ cellPosition.column, cellPosition.row + 1 })->cell().box()).vertical.top;
        auto adjacentRowTop = computedBorder(rows[cellPosition.row + 1].box()).vertical.top;
        auto adjacentCollapsedBorder = std::max(adjacentBorderTop, adjacentRowTop);
        border.vertical.bottom = std::max(border.vertical.bottom, adjacentCollapsedBorder) / 2;
    }
    return border;
}

Optional<LayoutUnit> TableFormattingContext::Geometry::computedColumnWidth(const ContainerBox& columnBox)
{
    // Check both style and <col>'s width attribute.
    // FIXME: Figure out what to do with calculated values, like <col style="width: 10%">.
    if (auto computedWidthValue = computedWidth(columnBox, { }))
        return computedWidthValue;
    return columnBox.columnWidth();
}

FormattingContext::IntrinsicWidthConstraints TableFormattingContext::Geometry::intrinsicWidthConstraintsForCell(const TableGrid::Cell& cell)
{
    auto& cellBox = cell.box();
    auto& style = cellBox.style();

    auto computedIntrinsicWidthConstraints = [&]() -> FormattingContext::IntrinsicWidthConstraints {
        // Even fixed width cells expand to their minimum content width
        // <td style="width: 10px">test_content</td> will size to max(minimum content width, computed width).
        auto intrinsicWidthConstraints = FormattingContext::IntrinsicWidthConstraints { };
        if (cellBox.hasChild())
            intrinsicWidthConstraints = LayoutContext::createFormattingContext(cellBox, layoutState())->computedIntrinsicWidthConstraints();
        if (auto fixedWidth = fixedValue(style.logicalWidth()))
            return { std::max(intrinsicWidthConstraints.minimum, *fixedWidth), std::max(intrinsicWidthConstraints.minimum, *fixedWidth) };
        return intrinsicWidthConstraints;
    };
    // FIXME Check for box-sizing: border-box;
    auto intrinsicWidthConstraints = constrainByMinMaxWidth(cellBox, computedIntrinsicWidthConstraints());
    // Expand with border
    intrinsicWidthConstraints.expand(computedCellBorder(cell).width());
    // padding
    intrinsicWidthConstraints.expand(fixedValue(style.paddingLeft()).valueOr(0) + fixedValue(style.paddingRight()).valueOr(0));
    // and margin
    intrinsicWidthConstraints.expand(fixedValue(style.marginStart()).valueOr(0) + fixedValue(style.marginEnd()).valueOr(0));
    return intrinsicWidthConstraints;
}


InlineLayoutUnit TableFormattingContext::Geometry::usedBaselineForCell(const ContainerBox& cellBox)
{
    // The baseline of a cell is defined as the baseline of the first in-flow line box in the cell,
    // or the first in-flow table-row in the cell, whichever comes first.
    // If there is no such line box, the baseline is the bottom of content edge of the cell box.
    for (auto& cellDescendant : descendantsOfType<ContainerBox>(cellBox)) {
        if (cellDescendant.establishesInlineFormattingContext())
            return layoutState().establishedInlineFormattingState(cellDescendant).displayInlineContent()->lineBoxes[0].baselineOffset();
        if (cellDescendant.establishesTableFormattingContext())
            return layoutState().establishedTableFormattingState(cellDescendant).tableGrid().rows().list()[0].baselineOffset();
    }
    return formattingContext().geometryForBox(cellBox).contentBoxBottom();
}

}
}

#endif
