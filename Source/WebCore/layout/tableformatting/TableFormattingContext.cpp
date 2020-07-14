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

#include "BlockFormattingState.h"
#include "DisplayBox.h"
#include "FloatingState.h"
#include "InlineFormattingState.h"
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

// https://www.w3.org/TR/css-tables-3/#table-layout-algorithm
TableFormattingContext::TableFormattingContext(const ContainerBox& formattingContextRoot, TableFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
{
}

void TableFormattingContext::layoutInFlowContent(InvalidationState&, const ConstraintsForInFlowContent& constraints)
{
    auto availableHorizontalSpace = constraints.horizontal.logicalWidth;
    auto availableVerticalSpace = constraints.vertical.logicalHeight;
    // 1. Compute width and height for the grid.
    computeAndDistributeExtraSpace(availableHorizontalSpace, availableVerticalSpace);
    // 2. Finalize cells.
    setUsedGeometryForCells(availableHorizontalSpace);
    // 3. Finalize rows.
    setUsedGeometryForRows(availableHorizontalSpace);
    // 4. Finalize sections.
    setUsedGeometryForSections(constraints);
}

void TableFormattingContext::setUsedGeometryForCells(LayoutUnit availableHorizontalSpace)
{
    auto& grid = formattingState().tableGrid();
    auto& columnList = grid.columns().list();
    auto& rowList = grid.rows().list();
    // Final table cell layout. At this point all percentage values can be resolved.
    auto sectionOffset = LayoutUnit { };
    auto* currentSection = &rowList.first().box().parent();
    for (auto& cell : grid.cells()) {
        auto& cellBox = cell->box();
        auto& cellDisplayBox = formattingState().displayBox(cellBox);
        auto& section = rowList[cell->startRow()].box().parent();
        if (&section != currentSection) {
            currentSection = &section;
            // While the grid is a continuous flow of rows, in the display tree they are relative to their sections.
            sectionOffset = rowList[cell->startRow()].logicalTop();
        }
        cellDisplayBox.setTop(rowList[cell->startRow()].logicalTop() - sectionOffset);
        cellDisplayBox.setLeft(columnList[cell->startColumn()].logicalLeft());
        auto availableVerticalSpace = rowList[cell->startRow()].logicalHeight();
        for (size_t rowIndex = cell->startRow() + 1; rowIndex < cell->endRow(); ++rowIndex)
            availableVerticalSpace += rowList[rowIndex].logicalHeight();
        availableVerticalSpace += (cell->rowSpan() - 1) * grid.verticalSpacing();
        layoutCell(*cell, availableHorizontalSpace, availableVerticalSpace);
        // FIXME: Find out if it is ok to use the regular padding here to align the content box inside a tall cell or we need to 
        // use some kind of intrinsic padding similar to RenderTableCell.
        auto paddingTop = cellDisplayBox.paddingTop().valueOr(LayoutUnit { });
        auto paddingBottom = cellDisplayBox.paddingBottom().valueOr(LayoutUnit { });
        auto intrinsicPaddingTop = LayoutUnit { };
        auto intrinsicPaddingBottom = LayoutUnit { };

        switch (cellBox.style().verticalAlign()) {
        case VerticalAlign::Middle: {
            auto intrinsicVerticalPadding = std::max(0_lu, availableVerticalSpace - cellDisplayBox.verticalMarginBorderAndPadding() - cellDisplayBox.contentBoxHeight());
            intrinsicPaddingTop = intrinsicVerticalPadding / 2;
            intrinsicPaddingBottom = intrinsicVerticalPadding / 2;
            break;
        }
        case VerticalAlign::Baseline: {
            auto rowBaselineOffset = LayoutUnit { rowList[cell->startRow()].baselineOffset() };
            auto cellBaselineOffset = LayoutUnit { cell->baselineOffset() };
            intrinsicPaddingTop = std::max(0_lu, rowBaselineOffset - cellBaselineOffset - cellDisplayBox.borderTop());
            intrinsicPaddingBottom = std::max(0_lu, availableVerticalSpace - cellDisplayBox.verticalMarginBorderAndPadding() - intrinsicPaddingTop - cellDisplayBox.contentBoxHeight());
            break;
        }
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        if (intrinsicPaddingTop && cellBox.hasInFlowOrFloatingChild()) {
            auto adjustCellContentWithInstrinsicPaddingBefore = [&] {
                // Child boxes (and runs) are always in the coordinate system of the containing block's border box.
                // The content box (where the child content lives) is inside the padding box, which is inside the border box.
                // In order to compute the child box top/left position, we need to know both the padding and the border offsets.  
                // Normally by the time we start positioning the child content, we already have computed borders and paddings for the containing block.
                // This is different with table cells where the final padding offset depends on the content height as we use
                // the padding box to vertically align the table cell content.
                auto& formattingState = layoutState().establishedFormattingState(cellBox);
                for (auto* child = cellBox.firstInFlowOrFloatingChild(); child; child = child->nextInFlowOrFloatingSibling()) {
                    if (child->isAnonymous() || child->isLineBreakBox())
                        continue;
                    formattingState.displayBox(*child).moveVertically(intrinsicPaddingTop);
                }
                if (cellBox.establishesInlineFormattingContext()) {
                    auto& displayContent = layoutState().establishedInlineFormattingState(cellBox).ensureDisplayInlineContent();
                    for (auto& run : displayContent.runs)
                        run.moveVertically(intrinsicPaddingTop);
                    for (auto& lineBox : displayContent.lineBoxes)
                        lineBox.moveVertically(intrinsicPaddingTop);
                }
            };
            adjustCellContentWithInstrinsicPaddingBefore();
        }
        cellDisplayBox.setVerticalPadding({ paddingTop + intrinsicPaddingTop, paddingBottom + intrinsicPaddingBottom });
    }
}

void TableFormattingContext::setUsedGeometryForRows(LayoutUnit availableHorizontalSpace)
{
    auto& grid = formattingState().tableGrid();
    auto& rows = grid.rows().list();

    auto rowLogicalTop = grid.verticalSpacing();
    const ContainerBox* previousRow = nullptr;
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        auto& row = rows[rowIndex];
        auto& rowBox = row.box();
        auto& rowDisplayBox = formattingState().displayBox(rowBox);

        rowDisplayBox.setPadding(geometry().computedPadding(rowBox, availableHorizontalSpace));
        // Internal table elements do not have margins.
        rowDisplayBox.setHorizontalMargin({ });
        rowDisplayBox.setVerticalMargin({ });

        auto computedRowBorder = [&] {
            auto border = geometry().computedBorder(rowBox);
            if (!grid.collapsedBorder())
                return border;
            // Border collapsing delegates borders to table/cells.
            border.horizontal = { };
            if (!rowIndex)
                border.vertical.top = { };
            if (rowIndex == rows.size() - 1)
                border.vertical.bottom = { };
            return border;
        }();
        if (computedRowBorder.height() > row.logicalHeight()) {
            // FIXME: This is an odd quirk when the row border overflows the row.
            // We don't paint row borders so it does not matter too much, but if we don't
            // set this fake border value, than we either end up with a negative content box
            // or with a wide frame box.
            // If it happens to cause issues in the display tree, we could also consider
            // a special frame box override, where padding box + border != frame box.
            computedRowBorder.vertical.top = { };
            computedRowBorder.vertical.bottom = { };
        }
        rowDisplayBox.setContentBoxHeight(row.logicalHeight() - computedRowBorder.height());

        auto rowLogicalWidth = grid.columns().logicalWidth() + 2 * grid.horizontalSpacing();
        if (computedRowBorder.width() > rowLogicalWidth) {
            // See comment above.
            computedRowBorder.horizontal.left = { };
            computedRowBorder.horizontal.right = { };
        }
        rowDisplayBox.setContentBoxWidth(rowLogicalWidth - computedRowBorder.width());
        rowDisplayBox.setBorder(computedRowBorder);

        if (previousRow && &previousRow->parent() != &rowBox.parent()) {
            // This row is in a different section.
            rowLogicalTop = { };
        }
        rowDisplayBox.setTop(rowLogicalTop);
        rowDisplayBox.setLeft({ });

        rowLogicalTop += row.logicalHeight() + grid.verticalSpacing();
        previousRow = &rowBox;
    }

    auto& columns = grid.columns();
    Vector<InlineLayoutUnit> rowBaselines(rows.size(), 0);
    // Now that cells are laid out, let's compute the row baselines.
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
            auto& slot = *grid.slot({ columnIndex, rowIndex });
            if (slot.isRowSpanned())
                continue;
            if (slot.hasRowSpan())
                continue;
            auto& cell = slot.cell();
            rowBaselines[rowIndex] = std::max(rowBaselines[rowIndex], cell.baselineOffset());
        }
    }
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
        rows[rowIndex].setBaselineOffset(rowBaselines[rowIndex]);
}

void TableFormattingContext::setUsedGeometryForSections(const ConstraintsForInFlowContent& constraints)
{
    auto& grid = formattingState().tableGrid();
    auto& tableBox = root();
    auto sectionWidth = grid.columns().logicalWidth() + 2 * grid.horizontalSpacing();
    auto logicalTop = constraints.vertical.logicalTop;
    auto verticalSpacing = grid.verticalSpacing();
    auto paddingBefore = Optional<LayoutUnit> { verticalSpacing };
    auto paddingAfter = verticalSpacing;
    for (auto& sectionBox : childrenOfType<ContainerBox>(tableBox)) {
        auto& sectionDisplayBox = formattingState().displayBox(sectionBox);
        // Section borders are either collapsed or ignored.
        sectionDisplayBox.setBorder({ });
        // Use fake vertical padding to space out the sections.
        sectionDisplayBox.setPadding(Edges { { }, { paddingBefore.valueOr(0_lu), paddingAfter } });
        paddingBefore = WTF::nullopt;
        // Internal table elements do not have margins.
        sectionDisplayBox.setHorizontalMargin({ });
        sectionDisplayBox.setVerticalMargin({ });

        sectionDisplayBox.setContentBoxWidth(sectionWidth);
        auto sectionContentHeight = LayoutUnit { };
        size_t rowCount = 0;
        for (auto& rowBox : childrenOfType<ContainerBox>(sectionBox)) {
            sectionContentHeight += geometryForBox(rowBox).height();
            ++rowCount;
        }
        sectionContentHeight += verticalSpacing * (rowCount - 1);
        sectionDisplayBox.setContentBoxHeight(sectionContentHeight);
        sectionDisplayBox.setLeft(constraints.horizontal.logicalLeft);
        sectionDisplayBox.setTop(logicalTop);

        logicalTop += sectionDisplayBox.height();
    }
}

void TableFormattingContext::layoutCell(const TableGrid::Cell& cell, LayoutUnit availableHorizontalSpace, Optional<LayoutUnit> usedCellHeight)
{
    ASSERT(cell.box().establishesBlockFormattingContext());

    auto& grid = formattingState().tableGrid();
    auto& cellBox = cell.box();
    auto& cellDisplayBox = formattingState().displayBox(cellBox);

    cellDisplayBox.setBorder(geometry().computedCellBorder(cell));
    cellDisplayBox.setPadding(geometry().computedPadding(cellBox, availableHorizontalSpace));
    // Internal table elements do not have margins.
    cellDisplayBox.setHorizontalMargin({ });
    cellDisplayBox.setVerticalMargin({ });

    auto availableSpaceForContent = [&] {
        auto& columnList = grid.columns().list();
        auto logicalWidth = LayoutUnit { };
        for (auto columnIndex = cell.startColumn(); columnIndex < cell.endColumn(); ++columnIndex)
            logicalWidth += columnList.at(columnIndex).logicalWidth();
        // No column spacing when spanning.
        logicalWidth += (cell.columnSpan() - 1) * grid.horizontalSpacing();
        return logicalWidth - cellDisplayBox.horizontalMarginBorderAndPadding();
    }();
    cellDisplayBox.setContentBoxWidth(availableSpaceForContent);

    if (cellBox.hasInFlowOrFloatingChild()) {
        auto constraintsForCellContent = geometry().constraintsForInFlowContent(cellBox);
        constraintsForCellContent.vertical.logicalHeight = usedCellHeight;
        auto invalidationState = InvalidationState { };
        // FIXME: This should probably be part of the invalidation state to indicate when we re-layout the cell
        // multiple times as part of the multi-pass table algorithm.
        auto& floatingStateForCellContent = layoutState().ensureBlockFormattingState(cellBox).floatingState();
        floatingStateForCellContent.clear();
        LayoutContext::createFormattingContext(cellBox, layoutState())->layoutInFlowContent(invalidationState, constraintsForCellContent);
    }
    cellDisplayBox.setContentBoxHeight(geometry().cellHeigh(cellBox));
}

FormattingContext::IntrinsicWidthConstraints TableFormattingContext::computedIntrinsicWidthConstraints()
{
    // Tables have a slighty different concept of shrink to fit. It's really only different with non-auto "width" values, where
    // a generic shrink-to fit block level box like a float box would be just sized to the computed value of "width", tables
    // can actually be streched way over.
    auto& grid = formattingState().tableGrid();
    if (auto computedWidthConstraints = grid.widthConstraints())
        return *computedWidthConstraints;

    // Compute the minimum/maximum width of each column.
    auto computedWidthConstraints = computedPreferredWidthForColumns();
    grid.setWidthConstraints(computedWidthConstraints);
    return computedWidthConstraints;
}

UniqueRef<TableGrid> TableFormattingContext::ensureTableGrid(const ContainerBox& tableBox)
{
    auto tableGrid = makeUniqueRef<TableGrid>();
    auto& tableStyle = tableBox.style();
    auto shouldApplyBorderSpacing = tableStyle.borderCollapse() == BorderCollapse::Separate;
    tableGrid->setHorizontalSpacing(LayoutUnit { shouldApplyBorderSpacing ? tableStyle.horizontalBorderSpacing() : 0 });
    tableGrid->setVerticalSpacing(LayoutUnit { shouldApplyBorderSpacing ? tableStyle.verticalBorderSpacing() : 0 });

    auto* firstChild = tableBox.firstChild();
    if (!firstChild) {
        // The rare case of empty table.
        return tableGrid;
    }

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
        auto& columns = tableGrid->columns();
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
                tableGrid->appendCell(downcast<ContainerBox>(*cell));
            }
        }
    }
    return tableGrid;
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
            intrinsicWidth = geometry().intrinsicWidthConstraintsForCell(*cell);
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
    size_t numberOfActualColumns = 0;
    for (size_t columnIndex = 0; columnIndex < columnList.size(); ++columnIndex) {
        auto columnHasNonSpannedCell = false;
        for (size_t rowIndex = 0; rowIndex < grid.rows().size(); ++rowIndex) {
            auto& slot = *grid.slot({ columnIndex, rowIndex });
            if (slot.isColumnSpanned())
                continue;
            columnHasNonSpannedCell = true;
            if (slot.hasColumnSpan()) {
                spanningCellPositionList.append({ columnIndex, rowIndex });
                continue;
            }
            auto columnFixedWidth = fixedWidthColumns[columnIndex];
            auto widthConstraints = !columnFixedWidth ? slot.widthConstraints() : FormattingContext::IntrinsicWidthConstraints { *columnFixedWidth, *columnFixedWidth };
            columnIntrinsicWidths[columnIndex].minimum = std::max(widthConstraints.minimum, columnIntrinsicWidths[columnIndex].minimum);
            columnIntrinsicWidths[columnIndex].maximum = std::max(widthConstraints.maximum, columnIntrinsicWidths[columnIndex].maximum);
        }
        if (columnHasNonSpannedCell)
            ++numberOfActualColumns;
    }

    // 4. Distribute the spanning min/max widths.
    for (auto spanningCellPosition : spanningCellPositionList) {
        auto& slot = *grid.slot(spanningCellPosition);
        auto& cell = slot.cell();
        ASSERT(slot.hasColumnSpan());
        auto widthConstraintsToDistribute = slot.widthConstraints();
        for (size_t columnSpanIndex = cell.startColumn(); columnSpanIndex < cell.endColumn(); ++columnSpanIndex)
            widthConstraintsToDistribute -= columnIntrinsicWidths[columnSpanIndex];
        // <table style="border-spacing: 50px"><tr><td colspan=2>long long text</td></tr><tr><td>lo</td><td>xt</td><tr></table>
        // [long long text]
        // [lo]        [xt]
        // While it looks like the spanning cell has to distribute all its spanning width, the border-spacing takes most of the space and
        // no distribution is needed at all.
        widthConstraintsToDistribute -= (cell.columnSpan() - 1) * grid.horizontalSpacing();
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
    // Exapand the preferred width with leading and trailing cell spacing (note that column spanners count as one cell).
    tableWidthConstraints += (numberOfActualColumns + 1) * grid.horizontalSpacing();
    return tableWidthConstraints;
}

void TableFormattingContext::computeAndDistributeExtraSpace(LayoutUnit availableHorizontalSpace, Optional<LayoutUnit> availableVerticalSpace)
{
    // Compute and balance the column and row spaces.
    auto& grid = formattingState().tableGrid();
    auto& columns = grid.columns().list();
    auto tableLayout = this->tableLayout();

    // Columns first.
    auto distributedHorizontalSpaces = tableLayout.distributedHorizontalSpace(availableHorizontalSpace);
    ASSERT(distributedHorizontalSpaces.size() == columns.size());
    auto columnLogicalLeft = grid.horizontalSpacing();
    for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
        auto& column = columns[columnIndex];
        column.setLogicalLeft(columnLogicalLeft);
        column.setLogicalWidth(distributedHorizontalSpaces[columnIndex]);
        columnLogicalLeft += distributedHorizontalSpaces[columnIndex] + grid.horizontalSpacing();
    }

    // Rows second.
    auto& rows = grid.rows().list();
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
            auto& slot = *grid.slot({ columnIndex, rowIndex });
            if (slot.isRowSpanned())
                continue;
            layoutCell(slot.cell(), availableHorizontalSpace);
            if (slot.hasRowSpan())
                continue;
            // The minimum height of a row (without spanning-related height distribution) is defined as the height of an hypothetical
            // linebox containing the cells originating in the row.
            auto& cell = slot.cell();
            cell.setBaselineOffset(geometry().usedBaselineForCell(cell.box()));
        }
    }

    auto distributedVerticalSpaces = tableLayout.distributedVerticalSpace(availableVerticalSpace);
    ASSERT(distributedVerticalSpaces.size() == rows.size());
    auto rowLogicalTop = grid.verticalSpacing();
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        auto& row = rows[rowIndex];
        row.setLogicalHeight(distributedVerticalSpaces[rowIndex]);
        row.setLogicalTop(rowLogicalTop);
        rowLogicalTop += distributedVerticalSpaces[rowIndex] + grid.verticalSpacing();
    }
}

}
}

#endif
