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

#include "LayoutBox.h"
#include "TableFormattingState.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(TableFormattingContext);

// https://www.w3.org/TR/css-tables-3/#table-layout-algorithm
TableFormattingContext::TableFormattingContext(const Box& formattingContextRoot, TableFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
{
}

void TableFormattingContext::layout() const
{
    auto& grid = formattingState().tableGrid();
    auto& cellList = grid.cells();
    ASSERT(!cellList.isEmpty());
    // Layout and position each table cell (and compute row height as well).
    auto& layoutState = this->layoutState();
    auto& columnList = grid.columnsContext().columns();
    auto& rowList = grid.rows();
    for (auto& cell : cellList) {
        auto& cellLayoutBox = cell->tableCellBox;
        ASSERT(cellLayoutBox.establishesBlockFormattingContext());

        auto& cellDisplayBox = layoutState.displayBoxForLayoutBox(cellLayoutBox);
        // FIXME: Add support for column and row spanning.
        auto cellPosition = cell->position;
        auto& row = rowList.at(cellPosition.y());
        auto& column = columnList.at(cellPosition.x());
        cellDisplayBox.setContentBoxWidth(column.logicalWidth());
        // FIXME: Do not use blanks.
        cellDisplayBox.setBorder({ });
        cellDisplayBox.setPadding({ });
        cellDisplayBox.setHorizontalMargin({ });
        cellDisplayBox.setHorizontalComputedMargin({ });

        cellDisplayBox.setTopLeft({ column.logicalLeft(), row.logicalTop() });

        layoutState.createFormattingContext(cellLayoutBox)->layout();

        // FIXME: This requires a 2 pass layout.
        auto heightAndMargin = Geometry::tableCellHeightAndMargin(layoutState, cellLayoutBox);
        cellDisplayBox.setContentBoxHeight(heightAndMargin.height);
        cellDisplayBox.setVerticalMargin({ heightAndMargin.nonCollapsedMargin, { } });

        row.setLogicalHeight(std::max(row.logicalHeight(), heightAndMargin.height));
        // FIXME: This also requires spanning support/check.
        if (!cellPosition.x() && cellPosition.y()) {
            auto& previousRow = rowList.at(cellPosition.y() - 1);
            row.setLogicalTop(previousRow.logicalBottom());
        }
    }
}

FormattingContext::IntrinsicWidthConstraints TableFormattingContext::computedIntrinsicWidthConstraints() const
{
    // Tables have a slighty different concept of shrink to fit. It's really only different with non-auto "width" values, where
    // a generic shrink-to fit block level box like a float box would be just sized to the computed value of "width", tables
    // can actually be streched way over.

    // 1. Ensure each cell slot is occupied by at least one cell.
    ensureTableGrid();
    // 2. Compute the minimum width of each column.
    computePreferredWidthForColumns();
    // 3. Compute the width of the table.
    auto width = computedTableWidth();
    // This is the actual computed table width that we want to present as min/max width.
    return { width, width };
}

void TableFormattingContext::ensureTableGrid() const
{
    auto& tableWrapperBox = downcast<Container>(root());
    auto& tableGrid = formattingState().tableGrid();

    for (auto* section = tableWrapperBox.firstChild(); section; section = section->nextSibling()) {
        ASSERT(section->isTableHeader() || section->isTableBody() || section->isTableFooter());
        for (auto* row = downcast<Container>(*section).firstChild(); row; row = row->nextSibling()) {
            ASSERT(row->isTableRow());
            for (auto* cell = downcast<Container>(*row).firstChild(); cell; cell = cell->nextSibling()) {
                ASSERT(cell->isTableCell());
                tableGrid.appendCell(*cell);
            }
        }
    }
}

void TableFormattingContext::computePreferredWidthForColumns() const
{
    auto& formattingState = this->formattingState();
    auto& grid = formattingState.tableGrid();

    // 1. Calculate the minimum content width (MCW) of each cell: the formatted content may span any number of lines but may not overflow the cell box.
    //    If the specified 'width' (W) of the cell is greater than MCW, W is the minimum cell width. A value of 'auto' means that MCW is the minimum cell width.
    //    Also, calculate the "maximum" cell width of each cell: formatting the content without breaking lines other than where explicit line breaks occur.
    for (auto& cell : grid.cells()) {
        ASSERT(cell->tableCellBox.establishesFormattingContext());

        auto intrinsicWidth = layoutState().createFormattingContext(cell->tableCellBox)->computedIntrinsicWidthConstraints();
        intrinsicWidth = Geometry::constrainByMinMaxWidth(cell->tableCellBox, intrinsicWidth);
        formattingState.setIntrinsicWidthConstraints(intrinsicWidth);

        auto columnSpan = cell->size.width();
        auto slotIntrinsicWidth = FormattingContext::IntrinsicWidthConstraints { intrinsicWidth.minimum / columnSpan, intrinsicWidth.maximum / columnSpan };
        auto initialPosition = cell->position;
        for (auto i = 0; i < columnSpan; ++i)
            grid.slot({ initialPosition.x() + i, initialPosition.y() })->widthConstraints = slotIntrinsicWidth;
    }
    // 2. For each column, determine a maximum and minimum column width from the cells that span only that column.
    //    The minimum is that required by the cell with the largest minimum cell width (or the column 'width', whichever is larger).
    //    The maximum is that required by the cell with the largest maximum cell width (or the column 'width', whichever is larger).
    auto& columns = grid.columnsContext().columns();
    int numberOfRows = grid.rows().size();
    int numberOfColumns = columns.size();
    for (int columnIndex = 0; columnIndex < numberOfColumns; ++columnIndex) {
        auto columnIntrinsicWidths = FormattingContext::IntrinsicWidthConstraints { };
        for (int rowIndex = 0; rowIndex < numberOfRows; ++rowIndex) {
            auto* slot = grid.slot({ columnIndex, rowIndex });
            columnIntrinsicWidths.minimum = std::max(slot->widthConstraints.minimum, columnIntrinsicWidths.minimum);
            columnIntrinsicWidths.maximum = std::max(slot->widthConstraints.maximum, columnIntrinsicWidths.maximum);
        }
        columns[columnIndex].setWidthConstraints(columnIntrinsicWidths);
    }
    // FIXME: Take column group elements into account.
}

LayoutUnit TableFormattingContext::computedTableWidth() const
{
    // Column and caption widths influence the final table width as follows:
    // If the 'table' or 'inline-table' element's 'width' property has a computed value (W) other than 'auto', the used width is the greater of
    // W, CAPMIN, and the minimum width required by all the columns plus cell spacing or borders (MIN).
    // If the used width is greater than MIN, the extra width should be distributed over the columns.
    // If the 'table' or 'inline-table' element has 'width: auto', the used width is the greater of the table's containing block width,
    // CAPMIN, and MIN. However, if either CAPMIN or the maximum width required by the columns plus cell spacing or borders (MAX) is
    // less than that of the containing block, use max(MAX, CAPMIN).

    // FIXME: This kind of code usually lives in *FormattingContextGeometry class.
    auto& tableWrapperBox = root();
    auto& style = tableWrapperBox.style();
    auto& containingBlock = *tableWrapperBox.containingBlock();
    auto& containingBlockDisplayBox = layoutState().displayBoxForLayoutBox(containingBlock);
    auto containingBlockWidth = containingBlockDisplayBox.contentBoxWidth();

    auto& grid = formattingState().tableGrid();
    auto& columnsContext = grid.columnsContext();
    auto tableWidthConstraints = grid.widthConstraints();

    auto width = Geometry::computedValueIfNotAuto(style.width(), containingBlockWidth);
    LayoutUnit usedWidth;
    if (width) {
        if (*width > tableWidthConstraints.minimum) {
            distributeAvailableWidth(*width - tableWidthConstraints.minimum);
            usedWidth = *width;
        } else {
            usedWidth = tableWidthConstraints.minimum;
            columnsContext.useAsLogicalWidth(TableGrid::ColumnsContext::WidthConstraintsType::Minimum);
        }
    } else {
        if (tableWidthConstraints.minimum > containingBlockWidth) {
            usedWidth = tableWidthConstraints.minimum;
            columnsContext.useAsLogicalWidth(TableGrid::ColumnsContext::WidthConstraintsType::Minimum);
        } else if (tableWidthConstraints.maximum < containingBlockWidth) {
            usedWidth = tableWidthConstraints.maximum;
            columnsContext.useAsLogicalWidth(TableGrid::ColumnsContext::WidthConstraintsType::Maximum);
        } else {
            usedWidth = containingBlockWidth;
            distributeAvailableWidth(*width - tableWidthConstraints.minimum);
        }
    }
    // FIXME: This should also deal with collapsing borders etc.
    LayoutUnit columnLogicalLeft;
    auto& columns = columnsContext.columns();
    for (auto& column : columns) {
        column.setLogicalLeft(columnLogicalLeft);
        columnLogicalLeft += column.logicalWidth();
    }
    return usedWidth;
}

void TableFormattingContext::distributeAvailableWidth(LayoutUnit extraHorizontalSpace) const
{
    // FIXME: Right now just distribute the extra space equaly among the columns.
    auto& columns = formattingState().tableGrid().columnsContext().columns();
    ASSERT(!columns.isEmpty());

    auto columnExtraSpace = extraHorizontalSpace / columns.size();
    for (auto& column : columns)
        column.setLogicalWidth(column.widthConstraints().minimum + columnExtraSpace);
}

}
}

#endif
