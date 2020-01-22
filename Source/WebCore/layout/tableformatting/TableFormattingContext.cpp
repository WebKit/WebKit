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
#include "InvalidationState.h"
#include "LayoutBox.h"
#include "LayoutChildIterator.h"
#include "LayoutContext.h"
#include "TableFormattingState.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(TableFormattingContext);

// FIXME: This is temporary. Remove this function when table formatting is complete.
void TableFormattingContext::initializeDisplayBoxToBlank(Display::Box& displayBox) const
{
    displayBox.setBorder({ });
    displayBox.setPadding({ });
    displayBox.setHorizontalMargin({ });
    displayBox.setHorizontalComputedMargin({ });
    displayBox.setVerticalMargin({ { }, { } });
    displayBox.setTopLeft({ });
    displayBox.setContentBoxWidth({ });
    displayBox.setContentBoxHeight({ });
}

// https://www.w3.org/TR/css-tables-3/#table-layout-algorithm
TableFormattingContext::TableFormattingContext(const Container& formattingContextRoot, TableFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
{
}

void TableFormattingContext::layoutInFlowContent(InvalidationState& invalidationState, const HorizontalConstraints& horizontalConstraints, const VerticalConstraints&)
{
    auto& grid = formattingState().tableGrid();
    auto& columnsContext = grid.columnsContext();

    computeAndDistributeExtraHorizontalSpace();
    // 1. Position each column.
    // FIXME: This should also deal with collapsing borders etc.
    auto horizontalSpacing = grid.horizontalSpacing();
    auto columnLogicalLeft = horizontalSpacing;
    for (auto& column : columnsContext.columns()) {
        column.setLogicalLeft(columnLogicalLeft);
        columnLogicalLeft += (column.logicalWidth() + horizontalSpacing);
    }

    // 2. Layout each table cell (and compute row height as well).
    auto& columnList = columnsContext.columns();
    auto& cellList = grid.cells();
    ASSERT(!cellList.isEmpty());
    for (auto& cell : cellList) {
        auto& cellLayoutBox = cell->tableCellBox;
        layoutTableCellBox(cellLayoutBox, columnList.at(cell->position.x()), invalidationState, horizontalConstraints);
        // FIXME: Add support for column and row spanning and this requires a 2 pass layout.
        auto& row = grid.rows().at(cell->position.y());
        row.setLogicalHeight(std::max(row.logicalHeight(), geometryForBox(cellLayoutBox).marginBoxHeight()));
    }
    // This is after the second pass when cell heights are fully computed.
    auto rowLogicalTop = grid.verticalSpacing();
    for (auto& row : grid.rows()) {
        row.setLogicalTop(rowLogicalTop);
        rowLogicalTop += (row.logicalHeight() + grid.verticalSpacing());
    }

    // 3. Finalize size and position.
    positionTableCells();
    setComputedGeometryForSections();
    setComputedGeometryForRows();
}

void TableFormattingContext::layoutTableCellBox(const Box& cellLayoutBox, const TableGrid::Column& column, InvalidationState& invalidationState, const HorizontalConstraints& horizontalConstraints)
{
    computeBorderAndPadding(cellLayoutBox, horizontalConstraints);
    // Margins do not apply to internal table elements.
    auto& cellDisplayBox = formattingState().displayBox(cellLayoutBox);
    cellDisplayBox.setHorizontalMargin({ });
    cellDisplayBox.setHorizontalComputedMargin({ });
    // Don't know the actual position yet.
    cellDisplayBox.setTopLeft({ });
    cellDisplayBox.setContentBoxWidth(column.logicalWidth() - cellDisplayBox.horizontalMarginBorderAndPadding());

    ASSERT(cellLayoutBox.establishesBlockFormattingContext());
    if (is<Container>(cellLayoutBox) && downcast<Container>(cellLayoutBox).hasInFlowOrFloatingChild())
        LayoutContext::createFormattingContext(downcast<Container>(cellLayoutBox), layoutState())->layoutInFlowContent(invalidationState, Geometry::horizontalConstraintsForInFlow(cellDisplayBox), Geometry::verticalConstraintsForInFlow(cellDisplayBox));
    cellDisplayBox.setVerticalMargin({ { }, { } });
    cellDisplayBox.setContentBoxHeight(geometry().tableCellHeightAndMargin(cellLayoutBox).contentHeight);
    // FIXME: Check what to do with out-of-flow content.
}

void TableFormattingContext::positionTableCells()
{
    auto& grid = formattingState().tableGrid();
    auto& rowList = grid.rows();
    auto& columnList = grid.columnsContext().columns();
    for (auto& cell : grid.cells()) {
        auto& cellDisplayBox = formattingState().displayBox(cell->tableCellBox);
        cellDisplayBox.setTop(rowList.at(cell->position.y()).logicalTop());
        cellDisplayBox.setLeft(columnList.at(cell->position.x()).logicalLeft());
    }
}

void TableFormattingContext::setComputedGeometryForRows()
{
    auto& grid = formattingState().tableGrid();
    auto rowWidth = grid.columnsContext().logicalWidth() + 2 * grid.horizontalSpacing();

    auto& rowList = grid.rows();
    for (auto& row : rowList) {
        auto& rowDisplayBox = formattingState().displayBox(row.box());
        initializeDisplayBoxToBlank(rowDisplayBox);
        rowDisplayBox.setContentBoxHeight(row.logicalHeight());
        rowDisplayBox.setContentBoxWidth(rowWidth);
        rowDisplayBox.setTop(row.logicalTop());
    }
}

void TableFormattingContext::setComputedGeometryForSections()
{
    auto& grid = formattingState().tableGrid();
    auto sectionWidth = grid.columnsContext().logicalWidth() + 2 * grid.horizontalSpacing();

    for (auto& section : childrenOfType<Box>(root())) {
        auto& sectionDisplayBox = formattingState().displayBox(section);
        initializeDisplayBoxToBlank(sectionDisplayBox);
        // FIXME: Size table sections properly.
        sectionDisplayBox.setContentBoxWidth(sectionWidth);
        sectionDisplayBox.setContentBoxHeight(grid.rows().last().logicalBottom() + grid.verticalSpacing());
    }
}

FormattingContext::IntrinsicWidthConstraints TableFormattingContext::computedIntrinsicWidthConstraints()
{
    // Tables have a slighty different concept of shrink to fit. It's really only different with non-auto "width" values, where
    // a generic shrink-to fit block level box like a float box would be just sized to the computed value of "width", tables
    // can actually be streched way over.
    auto& grid = formattingState().tableGrid();
    if (!grid.hasComputedWidthConstraints()) {
        // 1. Ensure each cell slot is occupied by at least one cell.
        ensureTableGrid();
        // 2. Compute the minimum/maximum width of each column.
        computePreferredWidthForColumns();
    }
    return grid.widthConstraints();
}

void TableFormattingContext::ensureTableGrid()
{
    auto& tableBox = root();
    auto& tableGrid = formattingState().tableGrid();
    tableGrid.setHorizontalSpacing(LayoutUnit { tableBox.style().horizontalBorderSpacing() });
    tableGrid.setVerticalSpacing(LayoutUnit { tableBox.style().verticalBorderSpacing() });

    auto* firstChild = tableBox.firstChild();
    const Box* tableCaption = nullptr;
    const Box* colgroup = nullptr;
    // Table caption is an optional element; if used, it is always the first child of a <table>.
    if (firstChild->isTableCaption())
        tableCaption = firstChild;
    // The <colgroup> must appear after any optional <caption> element but before any <thead>, <th>, <tbody>, <tfoot> and <tr> element.
    auto* colgroupCandidate = firstChild;
    if (tableCaption)
        colgroupCandidate = tableCaption->nextSibling();
    if (colgroupCandidate->isTableColumnGroup())
        colgroup = colgroupCandidate;

    if (colgroup) {
        auto& columnsContext = tableGrid.columnsContext();
        for (auto* column = downcast<Container>(*colgroup).firstChild(); column; column = column->nextSibling()) {
            ASSERT(column->isTableColumn());
            auto columnSpanCount = column->columnSpan();
            ASSERT(columnSpanCount > 0);
            while (columnSpanCount--)
                columnsContext.addColumn(column);
        }
    }

    auto* firstSection = colgroup ? colgroup->nextSibling() : tableCaption ? tableCaption->nextSibling() : firstChild;
    for (auto* section = firstSection; section; section = section->nextSibling()) {
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

void TableFormattingContext::computePreferredWidthForColumns()
{
    auto& formattingState = this->formattingState();
    auto& grid = formattingState.tableGrid();
    ASSERT(!grid.hasComputedWidthConstraints());

    // 1. Calculate the minimum content width (MCW) of each cell: the formatted content may span any number of lines but may not overflow the cell box.
    //    If the specified 'width' (W) of the cell is greater than MCW, W is the minimum cell width. A value of 'auto' means that MCW is the minimum cell width.
    //    Also, calculate the "maximum" cell width of each cell: formatting the content without breaking lines other than where explicit line breaks occur.
    for (auto& cell : grid.cells()) {
        auto& tableCellBox = cell->tableCellBox;
        ASSERT(tableCellBox.establishesFormattingContext());

        auto intrinsicWidth = formattingState.intrinsicWidthConstraintsForBox(tableCellBox);
        if (!intrinsicWidth) {
            intrinsicWidth = IntrinsicWidthConstraints { };
            if (is<Container>(tableCellBox))
                intrinsicWidth = LayoutContext::createFormattingContext(downcast<Container>(tableCellBox), layoutState())->computedIntrinsicWidthConstraints();
            intrinsicWidth = geometry().constrainByMinMaxWidth(tableCellBox, *intrinsicWidth);
            auto border = geometry().computedBorder(tableCellBox);
            auto padding = *geometry().computedPadding(tableCellBox, { });

            intrinsicWidth->expand(border.horizontal.width() + padding.horizontal.width());
            formattingState.setIntrinsicWidthConstraintsForBox(tableCellBox, *intrinsicWidth);
        }

        auto columnSpan = cell->size.width();
        auto slotIntrinsicWidth = FormattingContext::IntrinsicWidthConstraints { intrinsicWidth->minimum / columnSpan, intrinsicWidth->maximum / columnSpan };
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
        // Now that we have the content driven min/max widths, check if <col> sets a preferred width on this column.
        if (auto* columnBox = columns[columnIndex].columnBox()) {
            if (auto columnPreferredWidth = geometry().computedColumnWidth(*columnBox)) {
                // Let's stay at least as wide as the preferred width.
                columnIntrinsicWidths.minimum = std::max(columnIntrinsicWidths.minimum, *columnPreferredWidth);
            }
        }
        columns[columnIndex].setWidthConstraints(columnIntrinsicWidths);
    }
}

void TableFormattingContext::computeAndDistributeExtraHorizontalSpace()
{
    auto& grid = formattingState().tableGrid();
    ASSERT(grid.hasComputedWidthConstraints());
    auto tableWidthConstraints = grid.widthConstraints();

    // Column and caption widths influence the final table width as follows:
    // If the 'table' or 'inline-table' element's 'width' property has a computed value (W) other than 'auto', the used width is the greater of
    // W, CAPMIN, and the minimum width required by all the columns plus cell spacing or borders (MIN).
    // If the used width is greater than MIN, the extra width should be distributed over the columns.
    // If the 'table' or 'inline-table' element has 'width: auto', the used width is the greater of the table's containing block width,
    // CAPMIN, and MIN. However, if either CAPMIN or the maximum width required by the columns plus cell spacing or borders (MAX) is
    // less than that of the containing block, use max(MAX, CAPMIN).
    auto distributeExtraHorizontalSpace = [&](auto extraHorizontalSpace) {
        auto& columns = grid.columnsContext().columns();
        ASSERT(!columns.isEmpty());

        auto tableMinimumContentWidth = tableWidthConstraints.minimum - grid.totalHorizontalSpacing();
        auto adjustabledHorizontalSpace = tableMinimumContentWidth;
        auto numberOfColumns = columns.size();
        // Fixed width columns don't participate in available space distribution.
        for (auto& column : columns) {
            if (!column.hasFixedWidth())
                continue;
            auto columnFixedWidth = *column.columnBox()->columnWidth();
            column.setLogicalWidth(columnFixedWidth);

            --numberOfColumns;
            adjustabledHorizontalSpace -= columnFixedWidth;
        }
        if (!numberOfColumns || !adjustabledHorizontalSpace)
            return;
        // FIXME: Right now just distribute the extra space equaly among the columns using the minimum width.
        ASSERT(adjustabledHorizontalSpace > 0);
        for (auto& column : columns) {
            if (column.hasFixedWidth())
                continue;
            auto columnExtraSpace = extraHorizontalSpace / adjustabledHorizontalSpace * column.widthConstraints().minimum;
            column.setLogicalWidth(column.widthConstraints().minimum + columnExtraSpace);
        }
    };

    auto& tableBox = root();
    auto containingBlockWidth = geometryForBox(*tableBox.containingBlock(), EscapeReason::TableNeedsAccessToTableWrapper).contentBoxWidth();
    auto contentWidth = geometry().computedContentWidth(tableBox, containingBlockWidth);
    if (contentWidth) {
        if (*contentWidth > tableWidthConstraints.minimum)
            distributeExtraHorizontalSpace(*contentWidth - tableWidthConstraints.minimum);
        else
            useAsContentLogicalWidth(WidthConstraintsType::Minimum);
    } else {
        if (tableWidthConstraints.minimum > containingBlockWidth)
            useAsContentLogicalWidth(WidthConstraintsType::Minimum);
        else if (tableWidthConstraints.maximum < containingBlockWidth)
            useAsContentLogicalWidth(WidthConstraintsType::Maximum);
        else
            distributeExtraHorizontalSpace(containingBlockWidth - tableWidthConstraints.minimum);
    }
}

void TableFormattingContext::useAsContentLogicalWidth(WidthConstraintsType type)
{
    auto& columns = formattingState().tableGrid().columnsContext().columns();
    ASSERT(!columns.isEmpty());

    for (auto& column : columns)
        column.setLogicalWidth(type == WidthConstraintsType::Minimum ? column.widthConstraints().minimum : column.widthConstraints().maximum);
}

}
}

#endif
