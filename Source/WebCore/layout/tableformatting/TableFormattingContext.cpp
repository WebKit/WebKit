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

    // Column preferred width computation as follows:
    // 1. Collect each cells' width constraints
    // 2. Collect fixed column widths set by <colgroup>'s and <col>s
    // 3. Find the min/max width for each columns using the cell constraints and the <col> fixed widths but ignore column spans.
    // 4. Distribute column spanning cells min/max widths.
    // 5. Add them all up and return the computed min/max widths.
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

    // 2. Collect the fixed width <col>s.
    auto& columnList = grid.columns().list();
    Vector<Optional<LayoutUnit>> fixedWidthColumns;
    for (auto& column : columnList) {
        auto fixedWidth = [&] () -> Optional<LayoutUnit> {
            auto* columnBox = column.box();
            if (!columnBox) {
                // Anoynmous columns don't have associated layout boxes and can't have fixed col size.
                return { };
            }
            if (auto width = columnBox->columnWidth())
                return width;
            return geometry().computedColumnWidth(*columnBox);
        };
        fixedWidthColumns.append(fixedWidth());
    }

    Vector<FormattingContext::IntrinsicWidthConstraints> columnIntrinsicWidths(columnList.size());
    // 3. Collect he min/max width for each column but ignore column spans for now.
    Vector<SlotPosition> spanningCellPositionList;
    for (size_t columnIndex = 0; columnIndex < columnList.size(); ++columnIndex) {
        for (size_t rowIndex = 0; rowIndex < grid.rows().size(); ++rowIndex) {
            auto& slot = *grid.slot({ columnIndex, rowIndex });
            if (slot.hasColumnSpan()) {
                spanningCellPositionList.append({ columnIndex, rowIndex });
                continue;
            }
            if (slot.isColumnSpanned())
                continue;
            auto columnFixedWidth = fixedWidthColumns[columnIndex];
            auto widthConstraints = !columnFixedWidth ? slot.widthConstraints() : FormattingContext::IntrinsicWidthConstraints { *columnFixedWidth, *columnFixedWidth };
            columnIntrinsicWidths[columnIndex].minimum = std::max(widthConstraints.minimum, columnIntrinsicWidths[columnIndex].minimum);
            columnIntrinsicWidths[columnIndex].maximum = std::max(widthConstraints.maximum, columnIntrinsicWidths[columnIndex].maximum);
        }
    }

    // 4. Distribute the spanning min/max widths.
    for (auto spanningCellPosition : spanningCellPositionList) {
        auto& slot = *grid.slot(spanningCellPosition);
        auto& cell = slot.cell();
        ASSERT(slot.hasColumnSpan());
        auto widthConstraintsToDistribute = slot.widthConstraints();
        for (size_t columnSpanIndex = cell.startColumn(); columnSpanIndex < cell.endColumn(); ++columnSpanIndex)
            widthConstraintsToDistribute -= columnIntrinsicWidths[columnSpanIndex];
        // FIXME: Check if fixed width columns should be skipped here.
        widthConstraintsToDistribute.minimum = std::max(LayoutUnit { }, widthConstraintsToDistribute.minimum / cell.columnSpan());
        widthConstraintsToDistribute.maximum = std::max(LayoutUnit { }, widthConstraintsToDistribute.maximum / cell.columnSpan());
        if (widthConstraintsToDistribute.minimum || widthConstraintsToDistribute.maximum) {
            for (size_t columnSpanIndex = cell.startColumn(); columnSpanIndex < cell.endColumn(); ++columnSpanIndex)
                columnIntrinsicWidths[columnSpanIndex] += widthConstraintsToDistribute;
        }
    }

    // 5. The final table min/max widths is just the accumulated column constraints.
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

    auto distributeExtraHorizontalSpace = [&] (auto horizontalSpaceToDistribute) {
        auto& columnList = grid.columns().list();
        ASSERT(!columnList.isEmpty());

        // 1. Collect minimum widths driven by <td> across columns but ignore spanning cells first.
        struct ColumnMinimumWidth {
            float value { 0 };
            bool isFixed { false };
        };
        Vector<ColumnMinimumWidth> columnMinimumWidths(columnList.size());
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
                columnMinimumWidths[columnIndex].value = std::max<float>(columnMinimumWidths[columnIndex].value, slot.widthConstraints().minimum);
            }
        }
        // 2. Adjust the <td> minimum widths with fixed column widths (<col> vs. <td>) and also manage all-fixed-width-column content.
        auto hasFixedColumnsOnly = columns.hasFixedColumnsOnly();
        for (size_t columnIndex = 0; columnIndex < columnList.size(); ++columnIndex) {
            auto& column = columnList[columnIndex];
            if (!column.isFixedWidth())
                continue;
            // This is the column width based on <col width=""> and not <td style="width: ">.
            auto columnFixedWidth = column.box() ? column.box()->columnWidth() : WTF::nullopt; 
            columnMinimumWidths[columnIndex].value = std::max(columnMinimumWidths[columnIndex].value, columnFixedWidth.valueOr(0).toFloat());
            // Fixed columns flex when there are no other flexing columns.
            columnMinimumWidths[columnIndex].isFixed = !hasFixedColumnsOnly;
        }

        // 3. Distribute the spanning cells' mimimum widths across the columns using the non-spanning minimum widths.
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
                currentSpanningMinimumWidth += columnMinimumWidths[columnIndex].value;
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
                columnMinimumWidths[columnIndex].value += spaceToDistribute / currentSpanningMinimumWidth * columnMinimumWidths[columnIndex].value;
        }
        // 3. Distribute the extra space using the final minimum widths.
        // Fixed width columns don't participate in available space distribution.
        // Unless there are no flexing column at all, then they start flexing as if they were not fixed at all.
        float adjustabledHorizontalSpace = 0;
        for (auto& columnMinimumWidth : columnMinimumWidths) {
            if (columnMinimumWidth.isFixed)
                continue;
            adjustabledHorizontalSpace += columnMinimumWidth.value;
        }
        if (!adjustabledHorizontalSpace)
            return;
        // FIXME: Implement overconstrained columns when fixed width content is wider than the table.
        for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
            auto& column = columns.list()[columnIndex];
            auto minimumWidth = columnMinimumWidths[columnIndex].value;
            if (columnMinimumWidths[columnIndex].isFixed) {
                column.setLogicalWidth(LayoutUnit { minimumWidth });
                continue;
            }
            auto columnExtraSpace = horizontalSpaceToDistribute / adjustabledHorizontalSpace * minimumWidth;
            column.setLogicalWidth(LayoutUnit { minimumWidth + columnExtraSpace });
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
