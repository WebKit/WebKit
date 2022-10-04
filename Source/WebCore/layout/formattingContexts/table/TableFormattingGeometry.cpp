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
#include "TableFormattingGeometry.h"

#include "InlineFormattingState.h"
#include "LayoutBoxGeometry.h"
#include "LayoutContext.h"
#include "LayoutDescendantIterator.h"
#include "LayoutInitialContainingBlock.h"
#include "TableFormattingContext.h"
#include "TableFormattingQuirks.h"

namespace WebCore {
namespace Layout {

TableFormattingGeometry::TableFormattingGeometry(const TableFormattingContext& tableFormattingContext)
    : FormattingGeometry(tableFormattingContext)
{
}

LayoutUnit TableFormattingGeometry::cellBoxContentHeight(const ElementBox& cellBox) const
{
    ASSERT(cellBox.isInFlow());
    if (layoutState().inQuirksMode() && TableFormattingQuirks::shouldIgnoreChildContentVerticalMargin(cellBox)) {
        ASSERT(cellBox.firstInFlowChild());
        auto formattingContext = this->formattingContext();
        auto& firstInFlowChild = *cellBox.firstInFlowChild();
        auto& lastInFlowChild = *cellBox.lastInFlowChild();
        auto& firstInFlowChildGeometry = formattingContext.geometryForBox(firstInFlowChild, FormattingContext::EscapeReason::TableQuirkNeedsGeometryFromEstablishedFormattingContext);
        auto& lastInFlowChildGeometry = formattingContext.geometryForBox(lastInFlowChild, FormattingContext::EscapeReason::TableQuirkNeedsGeometryFromEstablishedFormattingContext);

        auto top = firstInFlowChild.style().hasMarginBeforeQuirk() ? BoxGeometry::borderBoxRect(firstInFlowChildGeometry).top() : BoxGeometry::marginBoxRect(firstInFlowChildGeometry).top();
        auto bottom = lastInFlowChild.style().hasMarginAfterQuirk() ? BoxGeometry::borderBoxRect(lastInFlowChildGeometry).bottom() : BoxGeometry::marginBoxRect(lastInFlowChildGeometry).bottom();
        return bottom - top;
    }
    return contentHeightForFormattingContextRoot(cellBox);
}

Edges TableFormattingGeometry::computedCellBorder(const TableGrid::Cell& cell) const
{
    auto& grid = formattingContext().formattingState().tableGrid();
    auto& cellBox = cell.box();
    auto border = computedBorder(cellBox);
    auto collapsedBorder = grid.collapsedBorder();
    if (!collapsedBorder)
        return border;

    // We might want to cache these collapsed borders on the grid.
    auto cellPosition = cell.position();
    // Collapsed border left from table and adjacent cells.
    if (!cellPosition.column)
        border.horizontal.left = collapsedBorder->horizontal.left / 2;
    else {
        auto adjacentBorderRight = computedBorder(grid.slot({ cellPosition.column - 1, cellPosition.row })->cell().box()).horizontal.right;
        border.horizontal.left = std::max(border.horizontal.left, adjacentBorderRight) / 2;
    }
    // Collapsed border right from table and adjacent cells.
    if (cellPosition.column == grid.columns().size() - 1)
        border.horizontal.right = collapsedBorder->horizontal.right / 2;
    else {
        auto adjacentBorderLeft = computedBorder(grid.slot({ cellPosition.column + 1, cellPosition.row })->cell().box()).horizontal.left;
        border.horizontal.right = std::max(border.horizontal.right, adjacentBorderLeft) / 2;
    }
    // Collapsed border top from table, row and adjacent cells.
    auto& rows = grid.rows().list();
    if (!cellPosition.row)
        border.vertical.top = collapsedBorder->vertical.top / 2;
    else {
        auto adjacentBorderBottom = computedBorder(grid.slot({ cellPosition.column, cellPosition.row - 1 })->cell().box()).vertical.bottom;
        auto adjacentRowBottom = computedBorder(rows[cellPosition.row - 1].box()).vertical.bottom;
        auto adjacentCollapsedBorder = std::max(adjacentBorderBottom, adjacentRowBottom);
        border.vertical.top = std::max(border.vertical.top, adjacentCollapsedBorder) / 2;
    }
    // Collapsed border bottom from table, row and adjacent cells.
    if (cellPosition.row == grid.rows().size() - 1)
        border.vertical.bottom = collapsedBorder->vertical.bottom / 2;
    else {
        auto adjacentBorderTop = computedBorder(grid.slot({ cellPosition.column, cellPosition.row + 1 })->cell().box()).vertical.top;
        auto adjacentRowTop = computedBorder(rows[cellPosition.row + 1].box()).vertical.top;
        auto adjacentCollapsedBorder = std::max(adjacentBorderTop, adjacentRowTop);
        border.vertical.bottom = std::max(border.vertical.bottom, adjacentCollapsedBorder) / 2;
    }
    return border;
}

std::optional<LayoutUnit> TableFormattingGeometry::computedColumnWidth(const ElementBox& columnBox) const
{
    // Check both style and <col>'s width attribute.
    // FIXME: Figure out what to do with calculated values, like <col style="width: 10%">.
    if (auto computedWidthValue = computedWidth(columnBox, { }))
        return computedWidthValue;
    return columnBox.columnWidth();
}

IntrinsicWidthConstraints TableFormattingGeometry::intrinsicWidthConstraintsForCellContent(const TableGrid::Cell& cell) const
{
    auto& cellBox = cell.box();
    if (!cellBox.hasInFlowOrFloatingChild())
        return { };
    auto& layoutState = this->layoutState();
    return LayoutContext::createFormattingContext(cellBox, const_cast<LayoutState&>(layoutState))->computedIntrinsicWidthConstraints();
}

InlineLayoutUnit TableFormattingGeometry::usedBaselineForCell(const ElementBox& cellBox) const
{
    // The baseline of a cell is defined as the baseline of the first in-flow line box in the cell,
    // or the first in-flow table-row in the cell, whichever comes first.
    // If there is no such line box, the baseline is the bottom of content edge of the cell box.
    if (cellBox.establishesInlineFormattingContext())
        return layoutState().formattingStateForInlineFormattingContext(cellBox).lines()[0].baseline();
    for (auto& cellDescendant : descendantsOfType<ElementBox>(cellBox)) {
        if (cellDescendant.establishesInlineFormattingContext()) {
            auto& inlineFormattingStateForCell = layoutState().formattingStateForInlineFormattingContext(cellDescendant);
            if (!inlineFormattingStateForCell.lines().isEmpty())
                return inlineFormattingStateForCell.lines()[0].baseline();
        }
        if (cellDescendant.establishesTableFormattingContext())
            return layoutState().formattingStateForTableFormattingContext(cellDescendant).tableGrid().rows().list()[0].baseline();
    }
    return formattingContext().geometryForBox(cellBox).contentBoxBottom();
}

LayoutUnit TableFormattingGeometry::horizontalSpaceForCellContent(const TableGrid::Cell& cell) const
{
    auto& grid = formattingContext().formattingState().tableGrid();
    auto& columnList = grid.columns().list();
    auto logicalWidth = LayoutUnit { };
    for (auto columnIndex = cell.startColumn(); columnIndex < cell.endColumn(); ++columnIndex)
        logicalWidth += columnList.at(columnIndex).usedLogicalWidth();
    // No column spacing when spanning.
    logicalWidth += (cell.columnSpan() - 1) * grid.horizontalSpacing();
    auto& cellBoxGeometry = formattingContext().geometryForBox(cell.box());
    logicalWidth -= (cellBoxGeometry.horizontalBorder() + cellBoxGeometry.horizontalPadding().value_or(0));
    return logicalWidth;
}

LayoutUnit TableFormattingGeometry::verticalSpaceForCellContent(const TableGrid::Cell& cell, std::optional<LayoutUnit> availableVerticalSpace) const
{
    auto& cellBox = cell.box();
    auto contentHeight = cellBoxContentHeight(cellBox);
    auto computedHeight = this->computedHeight(cellBox, availableVerticalSpace);
    if (!computedHeight)
        return contentHeight;
    auto heightUsesBorderBox = layoutState().inQuirksMode() || cellBox.style().boxSizing() == BoxSizing::BorderBox;
    if (heightUsesBorderBox) {
        auto& cellBoxGeometry = formattingContext().geometryForBox(cell.box());
        *computedHeight -= (cellBoxGeometry.verticalBorder() + cellBoxGeometry.verticalPadding().value_or(0));
    }
    return std::max(contentHeight, *computedHeight);
}

}
}

