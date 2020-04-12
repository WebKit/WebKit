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
#include "LayoutInitialContainingBlock.h"
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
TableFormattingContext::TableFormattingContext(const ContainerBox& formattingContextRoot, TableFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
{
}

void TableFormattingContext::layoutInFlowContent(InvalidationState& invalidationState, const HorizontalConstraints& horizontalConstraints, const VerticalConstraints&)
{
    auto& grid = formattingState().tableGrid();
    auto& columns = grid.columns();

    computeAndDistributeExtraHorizontalSpace(horizontalConstraints.logicalWidth);
    // 1. Position each column.
    // FIXME: This should also deal with collapsing borders etc.
    auto horizontalSpacing = grid.horizontalSpacing();
    auto columnLogicalLeft = horizontalSpacing;
    for (auto& column : columns.list()) {
        column.setLogicalLeft(columnLogicalLeft);
        columnLogicalLeft += (column.logicalWidth() + horizontalSpacing);
    }

    // 2. Layout each table cell (and compute row height as well).
    auto& cellList = grid.cells();
    ASSERT(!cellList.isEmpty());
    for (auto& cell : cellList) {
        auto& cellBox = cell->box();
        layoutCell(*cell, invalidationState, horizontalConstraints);
        // FIXME: Add support for column and row spanning and this requires a 2 pass layout.
        auto& row = grid.rows().list().at(cell->startRow());
        row.setLogicalHeight(std::max(row.logicalHeight(), geometryForBox(cellBox).marginBoxHeight()));
    }
    // This is after the second pass when cell heights are fully computed.
    auto rowLogicalTop = grid.verticalSpacing();
    for (auto& row : grid.rows().list()) {
        row.setLogicalTop(rowLogicalTop);
        rowLogicalTop += (row.logicalHeight() + grid.verticalSpacing());
    }

    // 3. Finalize size and position.
    positionTableCells();
    setComputedGeometryForSections();
    setComputedGeometryForRows();
}

void TableFormattingContext::layoutCell(const TableGrid::Cell& cell, InvalidationState& invalidationState, const HorizontalConstraints& horizontalConstraints)
{
    auto& cellBox = cell.box();
    computeBorderAndPadding(cellBox, horizontalConstraints);
    // Margins do not apply to internal table elements.
    auto& cellDisplayBox = formattingState().displayBox(cellBox);
    cellDisplayBox.setHorizontalMargin({ });
    cellDisplayBox.setHorizontalComputedMargin({ });
    // Don't know the actual position yet.
    cellDisplayBox.setTopLeft({ });
    auto contentWidth = [&] {
        auto& grid = formattingState().tableGrid();
        auto& columnList = grid.columns().list();
        auto logicalWidth = LayoutUnit { };
        for (auto columnIndex = cell.startColumn(); columnIndex < cell.endColumn(); ++columnIndex)
            logicalWidth += columnList.at(columnIndex).logicalWidth();
        // No column spacing when spanning.
        logicalWidth += (cell.columnSpan() - 1) * grid.horizontalSpacing();
        return logicalWidth - cellDisplayBox.horizontalMarginBorderAndPadding();
    }();
    cellDisplayBox.setContentBoxWidth(contentWidth);

    ASSERT(cellBox.establishesBlockFormattingContext());
    if (cellBox.hasInFlowOrFloatingChild()) {
        auto formattingContextForCellContent = LayoutContext::createFormattingContext(cellBox, layoutState());
        auto horizontalConstraintsForCellContent = Geometry::horizontalConstraintsForInFlow(cellDisplayBox);
        auto verticalConstraintsForCellContent = Geometry::verticalConstraintsForInFlow(cellDisplayBox);
        formattingContextForCellContent->layoutInFlowContent(invalidationState, horizontalConstraintsForCellContent, verticalConstraintsForCellContent);
    }
    cellDisplayBox.setVerticalMargin({ { }, { } });
    cellDisplayBox.setContentBoxHeight(geometry().tableCellHeightAndMargin(cellBox).contentHeight);
    // FIXME: Check what to do with out-of-flow content.
}

void TableFormattingContext::positionTableCells()
{
    auto& grid = formattingState().tableGrid();
    auto& rowList = grid.rows().list();
    auto& columnList = grid.columns().list();
    for (auto& cell : grid.cells()) {
        auto& cellDisplayBox = formattingState().displayBox(cell->box());
        cellDisplayBox.setTop(rowList.at(cell->startRow()).logicalTop());
        cellDisplayBox.setLeft(columnList.at(cell->startColumn()).logicalLeft());
    }
}

void TableFormattingContext::setComputedGeometryForRows()
{
    auto& grid = formattingState().tableGrid();
    auto rowWidth = grid.columns().logicalWidth() + 2 * grid.horizontalSpacing();

    for (auto& row : grid.rows().list()) {
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
    auto sectionWidth = grid.columns().logicalWidth() + 2 * grid.horizontalSpacing();

    for (auto& section : childrenOfType<Box>(root())) {
        auto& sectionDisplayBox = formattingState().displayBox(section);
        initializeDisplayBoxToBlank(sectionDisplayBox);
        // FIXME: Size table sections properly.
        sectionDisplayBox.setContentBoxWidth(sectionWidth);
        sectionDisplayBox.setContentBoxHeight(grid.rows().list().last().logicalBottom() + grid.verticalSpacing());
    }
}

FormattingContext::IntrinsicWidthConstraints TableFormattingContext::computedIntrinsicWidthConstraints()
{
    // Tables have a slighty different concept of shrink to fit. It's really only different with non-auto "width" values, where
    // a generic shrink-to fit block level box like a float box would be just sized to the computed value of "width", tables
    // can actually be streched way over.
    auto& grid = formattingState().tableGrid();
    if (auto computedWidthConstraints = grid.widthConstraints())
        return *computedWidthConstraints;

    // 1. Ensure each cell slot is occupied by at least one cell.
    ensureTableGrid();
    // 2. Compute the minimum/maximum width of each column.
    auto computedWidthConstraints = computedPreferredWidthForColumns();
    computedWidthConstraints.expand(grid.totalHorizontalSpacing());
    grid.setWidthConstraints(computedWidthConstraints);
    return computedWidthConstraints;
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
        auto& columns = tableGrid.columns();
        for (auto* column = downcast<ContainerBox>(*colgroup).firstChild(); column; column = column->nextSibling()) {
            ASSERT(column->isTableColumn());
            auto columnSpanCount = column->columnSpan();
            ASSERT(columnSpanCount > 0);
            while (columnSpanCount--)
                columns.addColumn(downcast<ContainerBox>(*column));
        }
    }

    auto* firstSection = colgroup ? colgroup->nextSibling() : tableCaption ? tableCaption->nextSibling() : firstChild;
    for (auto* section = firstSection; section; section = section->nextSibling()) {
        ASSERT(section->isTableHeader() || section->isTableBody() || section->isTableFooter());
        for (auto* row = downcast<ContainerBox>(*section).firstChild(); row; row = row->nextSibling()) {
            ASSERT(row->isTableRow());
            for (auto* cell = downcast<ContainerBox>(*row).firstChild(); cell; cell = cell->nextSibling()) {
                ASSERT(cell->isTableCell());
                tableGrid.appendCell(downcast<ContainerBox>(*cell));
            }
        }
    }
}

FormattingContext::IntrinsicWidthConstraints TableFormattingContext::computedPreferredWidthForColumns()
{
    auto& formattingState = this->formattingState();
    auto& grid = formattingState.tableGrid();
    ASSERT(!grid.widthConstraints());

    // 1. Calculate the minimum content width (MCW) of each cell: the formatted content may span any number of lines but may not overflow the cell box.
    //    If the specified 'width' (W) of the cell is greater than MCW, W is the minimum cell width. A value of 'auto' means that MCW is the minimum cell width.
    //    Also, calculate the "maximum" cell width of each cell: formatting the content without breaking lines other than where explicit line breaks occur.
    for (auto& cell : grid.cells()) {
        auto& cellBox = cell->box();
        ASSERT(cellBox.establishesBlockFormattingContext());

        auto intrinsicWidth = formattingState.intrinsicWidthConstraintsForBox(cellBox);
        if (!intrinsicWidth) {
            intrinsicWidth = geometry().intrinsicWidthConstraintsForCell(cellBox);
            formattingState.setIntrinsicWidthConstraintsForBox(cellBox, *intrinsicWidth);
        }
        // Spanner cells put their intrinsic widths on the initial slots.
        grid.slot(cell->position())->setWidthConstraints(*intrinsicWidth);
    }

    auto& columnList = grid.columns().list();
    Vector<Optional<LayoutUnit>> fixedWidthColumns;
    for (size_t columnIndex = 0; columnIndex < columnList.size(); ++columnIndex) {
        auto fixedWidth = [&] () -> Optional<LayoutUnit> {
            auto* columnBox = columnList[columnIndex].box();
            if (!columnBox) {
                // Anoynmous columns don't have associated layout boxes.
                return { };
            }
            if (auto width = columnBox->columnWidth())
                return width;
            return geometry().computedColumnWidth(*columnBox);
        };
        fixedWidthColumns.append(fixedWidth());
    }

    Vector<FormattingContext::IntrinsicWidthConstraints> columnIntrinsicWidths;
    // Collect he min/max width for each column and finalize the preferred width for the table.
    for (size_t columnIndex = 0; columnIndex < columnList.size(); ++columnIndex) {
        columnIntrinsicWidths.append(FormattingContext::IntrinsicWidthConstraints { });
        for (size_t rowIndex = 0; rowIndex < grid.rows().size(); ++rowIndex) {
            auto& slot = *grid.slot({ columnIndex, rowIndex });
            if (slot.hasColumnSpan()) {
                auto& cell = slot.cell();
                auto widthConstraintsToDistribute = slot.widthConstraints();
                auto numberOfNonFixedSpannedColumns = cell.columnSpan();
                for (size_t columnSpanIndex = cell.startColumn(); columnSpanIndex < cell.endColumn(); ++columnSpanIndex) {
                    if (auto fixedWidth = fixedWidthColumns[columnSpanIndex]) {
                        widthConstraintsToDistribute -= *fixedWidth;
                        --numberOfNonFixedSpannedColumns;
                    }
                }
                for (size_t columnSpanIndex = cell.startColumn(); columnSpanIndex < cell.endColumn(); ++columnSpanIndex) {
                    columnIntrinsicWidths[columnIndex].minimum = std::max(widthConstraintsToDistribute.minimum / numberOfNonFixedSpannedColumns, columnIntrinsicWidths[columnIndex].minimum);
                    columnIntrinsicWidths[columnIndex].maximum = std::max(widthConstraintsToDistribute.maximum / numberOfNonFixedSpannedColumns, columnIntrinsicWidths[columnIndex].maximum);
                }
            } else {
                auto columnFixedWidth = fixedWidthColumns[columnIndex];
                auto widthConstraints = !columnFixedWidth ? slot.widthConstraints() : FormattingContext::IntrinsicWidthConstraints { *columnFixedWidth, *columnFixedWidth };
                columnIntrinsicWidths[columnIndex].minimum = std::max(widthConstraints.minimum, columnIntrinsicWidths[columnIndex].minimum);
                columnIntrinsicWidths[columnIndex].maximum = std::max(widthConstraints.maximum, columnIntrinsicWidths[columnIndex].maximum);
            }
        }
    }
    auto tableWidthConstraints = IntrinsicWidthConstraints { };
    for (auto& columnIntrinsicWidth : columnIntrinsicWidths)
        tableWidthConstraints += columnIntrinsicWidth;
    return tableWidthConstraints;
}

void TableFormattingContext::computeAndDistributeExtraHorizontalSpace(LayoutUnit availableHorizontalSpace)
{
    auto& grid = formattingState().tableGrid();
    auto& columns = grid.columns();
    auto& rows = grid.rows();
    auto tableWidthConstraints = *grid.widthConstraints();

    // Column and caption widths influence the final table width as follows:
    // If the 'table' or 'inline-table' element's 'width' property has a computed value (W) other than 'auto', the used width is the greater of
    // W, CAPMIN, and the minimum width required by all the columns plus cell spacing or borders (MIN).
    // If the used width is greater than MIN, the extra width should be distributed over the columns.
    // If the 'table' or 'inline-table' element has 'width: auto', the used width is the greater of the table's containing block width,
    // CAPMIN, and MIN. However, if either CAPMIN or the maximum width required by the columns plus cell spacing or borders (MAX) is
    // less than that of the containing block, use max(MAX, CAPMIN).
    auto distributeExtraHorizontalSpace = [&] (auto horizontalSpaceToDistribute) {
        auto& columnList = grid.columns().list();
        ASSERT(!columnList.isEmpty());

        // 1. Collect minimum widths across columns but ignore spanning cells first.
        Vector<float> columnMinimumWidths;
        Vector<SlotPosition> spanningCellPositionList;
        for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
            for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
                auto& slot = *grid.slot({ columnIndex, rowIndex });
                if (slot.hasColumnSpan()) {
                    spanningCellPositionList.append({ columnIndex, rowIndex });
                    continue;
                }
                if (slot.isColumnSpanned())
                    continue;
                if (!rowIndex)
                    columnMinimumWidths.append(slot.widthConstraints().minimum);
                else
                    columnMinimumWidths[columnIndex] = std::max<float>(columnMinimumWidths[columnIndex], slot.widthConstraints().minimum);
            }
        }
        // 2. Distribute the spanning cells' mimimum widths across the columns using the non-spanning minium widths.
        // e.g. [ 1 ][ 5 ][ 1 ]
        //      [    9   ][ 1 ]
        // The minimum widths are: [ 2 ][ 7 ][ 1 ]
        for (auto spanningCellPosition : spanningCellPositionList) {
            auto& slot = *grid.slot(spanningCellPosition);
            auto& cell = slot.cell();
            ASSERT(slot.hasColumnSpan());
            float currentSpanningMinimumWidth = 0;
            // 1. Collect the non-spaning minimum widths.
            for (auto columnIndex = cell.startColumn(); columnIndex < cell.endColumn(); ++columnIndex)
                currentSpanningMinimumWidth += columnMinimumWidths[columnIndex];
            float spanningMinimumWidth = slot.widthConstraints().minimum;
            if (currentSpanningMinimumWidth >= spanningMinimumWidth) {
                // The spanning cell fits the spanned columns just fine. Nothing to distribute.
                continue;
            }
            // 2. Distribute the extra minimum width among the spanned columns based on the minimum colmn width.
            // e.g. spanning mimimum width: [   9   ]. Current minimum widths for the spanned columns: [ 1 ] [ 2 ]
            // New minimum widths: [ 3 ] [ 6 ].
            auto spaceToDistribute = spanningMinimumWidth - currentSpanningMinimumWidth;
            for (auto columnIndex = cell.startColumn(); columnIndex < cell.endColumn(); ++columnIndex)
                columnMinimumWidths[columnIndex] += spaceToDistribute / currentSpanningMinimumWidth * columnMinimumWidths[columnIndex];
        }
        // 3. Distribute the extra space using the final minimum widths.
        float adjustabledHorizontalSpace = tableWidthConstraints.minimum - grid.totalHorizontalSpacing();
        auto numberOfColumns = columnList.size();
        // Fixed width columns don't participate in available space distribution.
        for (auto& column : columnList) {
            if (!column.isFixedWidth())
                continue;
            auto columnFixedWidth = *column.box()->columnWidth();
            column.setLogicalWidth(columnFixedWidth);

            --numberOfColumns;
            adjustabledHorizontalSpace -= columnFixedWidth;
        }
        if (!numberOfColumns || !adjustabledHorizontalSpace)
            return;
        ASSERT(adjustabledHorizontalSpace > 0);
        for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
            auto& column = columns.list()[columnIndex];
            if (column.isFixedWidth())
                continue;
            auto columnExtraSpace = horizontalSpaceToDistribute / adjustabledHorizontalSpace * columnMinimumWidths[columnIndex];
            column.setLogicalWidth(LayoutUnit { columnMinimumWidths[columnIndex] + columnExtraSpace });
        }
    };

    enum class WidthConstraintsType { Minimum, Maximum };
    auto distributeMinOrMax = [&] (WidthConstraintsType type) {
        for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
            auto logicalWidth = LayoutUnit { };
            for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
                auto widthConstraints = grid.slot({ columnIndex, rowIndex })->widthConstraints();
                logicalWidth = std::max(logicalWidth, type == WidthConstraintsType::Minimum ? widthConstraints.minimum : widthConstraints.maximum); 
            }
            columns.list()[columnIndex].setLogicalWidth(logicalWidth);
        }
    };

    ASSERT(availableHorizontalSpace >= tableWidthConstraints.minimum);
    if (availableHorizontalSpace == tableWidthConstraints.minimum)
        distributeMinOrMax(WidthConstraintsType::Minimum);
    else if (availableHorizontalSpace == tableWidthConstraints.maximum)
        distributeMinOrMax(WidthConstraintsType::Maximum);
    else
        distributeExtraHorizontalSpace(availableHorizontalSpace - tableWidthConstraints.minimum);
}

}
}

#endif
