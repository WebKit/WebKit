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
    computeAndDistributeExtraHorizontalSpace(availableHorizontalSpace);
    computeAndDistributeExtraVerticalSpace(availableHorizontalSpace, availableVerticalSpace);
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
    for (auto& cell : grid.cells()) {
        auto& cellBox = cell->box();
        auto& cellDisplayBox = formattingState().displayBox(cellBox);
        cellDisplayBox.setTop(rowList[cell->startRow()].logicalTop());
        cellDisplayBox.setLeft(columnList[cell->startColumn()].logicalLeft());
        auto availableVerticalSpace = rowList[cell->startRow()].logicalHeight();
        for (size_t rowIndex = cell->startRow() + 1; rowIndex < cell->endRow(); ++rowIndex)
            availableVerticalSpace += rowList[rowIndex].logicalHeight();
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
        cellDisplayBox.setVerticalPadding({ paddingTop + intrinsicPaddingTop, paddingBottom + intrinsicPaddingBottom });
    }
}

void TableFormattingContext::setUsedGeometryForRows(LayoutUnit availableHorizontalSpace)
{
    auto& grid = formattingState().tableGrid();
    auto rowWidth = grid.columns().logicalWidth() + 2 * grid.horizontalSpacing();
    auto rowLogicalTop = grid.verticalSpacing();
    for (auto& row : grid.rows().list()) {
        auto& rowBox = row.box();
        auto& rowDisplayBox = formattingState().displayBox(rowBox);
        computeBorderAndPadding(rowBox, HorizontalConstraints { { }, availableHorizontalSpace });
        // Internal table elements do not have margins.
        rowDisplayBox.setHorizontalMargin({ });
        rowDisplayBox.setHorizontalComputedMargin({ });
        rowDisplayBox.setVerticalMargin({ { }, { } });

        rowDisplayBox.setContentBoxHeight(row.logicalHeight());
        rowDisplayBox.setContentBoxWidth(rowWidth);
        rowDisplayBox.setTop(rowLogicalTop);
        rowDisplayBox.setLeft({ });

        row.setLogicalTop(rowLogicalTop);
        rowLogicalTop += row.logicalHeight() + grid.verticalSpacing();
    }
}

void TableFormattingContext::setUsedGeometryForSections(const ConstraintsForInFlowContent& constraints)
{
    auto& grid = formattingState().tableGrid();
    auto sectionWidth = grid.columns().logicalWidth() + 2 * grid.horizontalSpacing();
    auto logicalTop = constraints.vertical.logicalTop;
    for (auto& section : childrenOfType<ContainerBox>(root())) {
        auto& sectionDisplayBox = formattingState().displayBox(section);
        computeBorderAndPadding(section, HorizontalConstraints { { }, constraints.horizontal.logicalWidth });
        // Internal table elements do not have margins.
        sectionDisplayBox.setHorizontalMargin({ });
        sectionDisplayBox.setHorizontalComputedMargin({ });
        sectionDisplayBox.setVerticalMargin({ { }, { } });

        sectionDisplayBox.setContentBoxWidth(sectionWidth);
        sectionDisplayBox.setContentBoxHeight(grid.rows().list().last().logicalBottom() + grid.verticalSpacing());

        sectionDisplayBox.setLeft(constraints.horizontal.logicalLeft);
        sectionDisplayBox.setTop(logicalTop);

        logicalTop += sectionDisplayBox.height();
    }
}

void TableFormattingContext::layoutCell(const TableGrid::Cell& cell, LayoutUnit availableHorizontalSpace, Optional<LayoutUnit> usedCellHeight)
{
    ASSERT(cell.box().establishesBlockFormattingContext());

    auto& cellBox = cell.box();
    auto& cellDisplayBox = formattingState().displayBox(cellBox);

    computeBorderAndPadding(cellBox, HorizontalConstraints { { }, availableHorizontalSpace });
    // Internal table elements do not have margins.
    cellDisplayBox.setHorizontalMargin({ });
    cellDisplayBox.setHorizontalComputedMargin({ });
    cellDisplayBox.setVerticalMargin({ { }, { } });

    auto availableSpaceForContent = [&] {
        auto& grid = formattingState().tableGrid();
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
        auto invalidationState = InvalidationState { };
        auto constraintsForCellContent = geometry().constraintsForInFlowContent(cellBox);
        constraintsForCellContent.vertical.logicalHeight = usedCellHeight;
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

    // 1. Ensure each cell slot is occupied by at least one cell.
    ensureTableGrid();
    // 2. Compute the minimum/maximum width of each column.
    auto computedWidthConstraints = computedPreferredWidthForColumns();
    grid.setWidthConstraints(computedWidthConstraints);
    return computedWidthConstraints;
}

void TableFormattingContext::ensureTableGrid()
{
    auto& tableBox = root();
    auto& tableGrid = formattingState().tableGrid();
    auto& tableStyle = tableBox.style();
    auto shouldApplyBorderSpacing = tableStyle.borderCollapse() == BorderCollapse::Separate;
    tableGrid.setHorizontalSpacing(LayoutUnit { shouldApplyBorderSpacing ? tableStyle.horizontalBorderSpacing() : 0 });
    tableGrid.setVerticalSpacing(LayoutUnit { shouldApplyBorderSpacing ? tableStyle.verticalBorderSpacing() : 0 });

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

struct ColumnSpan {
    static size_t hasSpan(const TableGrid::Slot& slot) { return slot.hasColumnSpan(); }
    static size_t isSpanned(const TableGrid::Slot& slot) { return slot.isColumnSpanned(); }

    static size_t spanCount(const TableGrid::Cell& cell) { return cell.columnSpan(); }
    static size_t startSpan(const TableGrid::Cell& cell) { return cell.startColumn(); }
    static size_t endSpan(const TableGrid::Cell& cell) { return cell.endColumn(); }

    static size_t index(size_t columnIndex, size_t /*rowIndex*/) { return columnIndex; }
    static size_t size(const TableGrid& grid) { return grid.columns().size(); }

    static LayoutUnit spacing(const TableGrid& grid, const TableGrid::Cell& cell) { return (cell.columnSpan() - 1) * grid.horizontalSpacing(); }
};

struct RowSpan {
    static size_t hasSpan(const TableGrid::Slot& slot) { return slot.hasRowSpan(); }
    static size_t isSpanned(const TableGrid::Slot& slot) { return slot.isRowSpanned(); }

    static size_t spanCount(const TableGrid::Cell& cell) { return cell.rowSpan(); }
    static size_t startSpan(const TableGrid::Cell& cell) { return cell.startRow(); }
    static size_t endSpan(const TableGrid::Cell& cell) { return cell.endRow(); }

    static size_t index(size_t /*columnIndex*/, size_t rowIndex) { return rowIndex; }
    static size_t size(const TableGrid& grid) { return grid.rows().size(); }

    static LayoutUnit spacing(const TableGrid& grid, const TableGrid::Cell& cell) { return (cell.rowSpan() - 1) * grid.verticalSpacing(); }
};

using DistributedSpaces = Vector<float>;
template <typename SpanType>
static DistributedSpaces distributeAvailableSpace(const TableGrid& grid, float spaceToDistribute, const WTF::Function<LayoutUnit(const TableGrid::Slot&, size_t)>& slotSpace)
{
    struct ResolvedSpace {
        float value { 0 };
        bool isFixed { false };
    };

    auto& columns = grid.columns();
    auto& rows = grid.rows();
    // 1. Collect the non-spanning spaces first. They are used for the final distribution as well as for distributing the spanning space.
    Vector<Optional<ResolvedSpace>> resolvedSpaces(SpanType::size(grid));
    for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
        for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
            auto& slot = *grid.slot({ columnIndex, rowIndex });
            if (SpanType::hasSpan(slot) || SpanType::isSpanned(slot))
                continue;
            auto index = SpanType::index(columnIndex, rowIndex);
            if (!resolvedSpaces[index])
                resolvedSpaces[index] = ResolvedSpace { };
            resolvedSpaces[index]->value = std::max<float>(resolvedSpaces[index]->value, slotSpace(slot, index));
        }
    }

    // 2. Collect the spanning cells.
    struct SpanningCell {
        SlotPosition position;
        LayoutUnit unresolvedSpace;
    };
    Vector<SpanningCell> spanningCells;
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
            auto& slot = *grid.slot({ columnIndex, rowIndex });
            if (SpanType::hasSpan(slot))
                spanningCells.append({ { columnIndex, rowIndex }, slotSpace(slot, SpanType::index(columnIndex, rowIndex)) });
        }
    }
    // We need these spanning cells in the order of the number of columns/rows they span so that
    // we can resolve overlapping spans starting with the shorter ones e.g.
    // <td colspan=4>#a</td><td>#b</td>
    // <td colspan=2>#c</td><td colspan=3>#d</td>
    std::sort(spanningCells.begin(), spanningCells.end(), [&] (auto& a, auto& b) {
        return SpanType::spanCount(grid.slot(a.position)->cell()) < SpanType::spanCount(grid.slot(b.position)->cell());
    });

    // 3. Distribute the spanning cells' mimimum space across the columns/rows using the non-spanning spaces.
    // e.g. [ 1 ][ 5 ][ 1 ]
    //      [    9   ][ 1 ]
    // The initial widths are: [ 2 ][ 7 ][ 1 ]
    for (auto spanningCell : spanningCells) {
        auto& cell = grid.slot(spanningCell.position)->cell();
        float unresolvedSpanningSpace = spanningCell.unresolvedSpace;
        if (!resolvedSpaces[SpanType::startSpan(cell)] || !resolvedSpaces[SpanType::endSpan(cell) - 1]) {
            // <td colspan=4>#a</td><td>#b</td>
            // <td colspan=2>#c</td><td colspan=3>#d</td>
            // Unresolved columns are: 1 2 3 4
            // 1. Take colspan=2 (shortest span) and resolve column 1 and 2
            // 2. Take colspan=3 and resolve column 3 and 4 (5 is resolved because it already has a non-spanning cell).
            // 3. colspan=4 needs no resolving because all the spanned columns (1 2 3 4) have already been resolved.
            auto unresolvedColumnCount = cell.columnSpan();
            for (auto spanIndex = SpanType::startSpan(cell); spanIndex < SpanType::endSpan(cell); ++spanIndex) {
                if (!resolvedSpaces[spanIndex])
                    continue;
                ASSERT(unresolvedColumnCount);
                --unresolvedColumnCount;
                unresolvedSpanningSpace = std::max(0.0f, unresolvedSpanningSpace - resolvedSpaces[spanIndex]->value);
            }
            ASSERT(unresolvedColumnCount);
            for (auto spanIndex = SpanType::startSpan(cell); spanIndex < SpanType::endSpan(cell); ++spanIndex) {
                if (resolvedSpaces[spanIndex])
                    continue;
                resolvedSpaces[spanIndex] = ResolvedSpace { unresolvedSpanningSpace / unresolvedColumnCount, false };
            }
        } else {
            // 1. Collect the non-spaning resolved spaces.
            // 2. Distribute the extra space among the spanned columns/rows based on the resolved space values.
            // e.g. spanning width: [   9   ]. Resolved widths for the spanned columns: [ 1 ] [ 2 ]
            // New resolved widths: [ 3 ] [ 6 ].
            float resolvedSpace = 0;
            for (auto spanIndex = SpanType::startSpan(cell); spanIndex < SpanType::endSpan(cell); ++spanIndex)
                resolvedSpace += resolvedSpaces[spanIndex]->value;
            if (resolvedSpace >= unresolvedSpanningSpace) {
                // The spanning cell fits the spanned columns/rows just fine. Nothing to distribute.
                continue;
            }
            auto spanningSpaceToDistribute = std::max(0.0f, unresolvedSpanningSpace - SpanType::spacing(grid, cell) - resolvedSpace);
            if (spanningSpaceToDistribute) {
                for (auto spanIndex = SpanType::startSpan(cell); spanIndex < SpanType::endSpan(cell); ++spanIndex)
                    resolvedSpaces[spanIndex]->value += spanningSpaceToDistribute / resolvedSpace * resolvedSpaces[spanIndex]->value;
            }
        }
    }
    // 4. Distribute the extra space using the final resolved widths.
#if ASSERT_ENABLED
    // We have to have all the spaces resolved at this point.
    for (auto& resolvedSpace : resolvedSpaces)
        ASSERT(resolvedSpace);
#endif
    // Fixed size cells don't participate in available space distribution.
    float adjustabledSpace = 0;
    for (auto& resolvedSpace : resolvedSpaces) {
        if (resolvedSpace->isFixed)
            continue;
        adjustabledSpace += resolvedSpace->value;
    }

    DistributedSpaces distributedSpaces(resolvedSpaces.size());
    // Distribute the extra space based on the resolved spaces.
    for (size_t index = 0; index < resolvedSpaces.size(); ++index) {
        auto& resolvedSpace = resolvedSpaces[index];
        auto hasExtraSpaceToDistribute = spaceToDistribute && !resolvedSpace->isFixed;
        auto resolvedValue = resolvedSpace->value;
        distributedSpaces[index] = hasExtraSpaceToDistribute ? resolvedValue + (spaceToDistribute / adjustabledSpace * resolvedValue) : resolvedValue;
    }
    return distributedSpaces;
}

void TableFormattingContext::computeAndDistributeExtraHorizontalSpace(LayoutUnit availableHorizontalSpace)
{
    auto& grid = formattingState().tableGrid();
    auto& columns = grid.columns();
    auto tableWidthConstraints = *grid.widthConstraints();

    enum class ColumnWidthBalancingBase { MinimumWidth, MaximumWidth };
    auto computeColumnWidths = [&] (auto columnWidthBalancingBase, auto extraHorizontalSpace) {
        auto distributedSpaces = distributeAvailableSpace<ColumnSpan>(grid, extraHorizontalSpace, [&] (const TableGrid::Slot& slot, size_t columnIndex) {
            auto& column = columns.list()[columnIndex];
            auto columnFixedWidth = column.box() ? column.box()->columnWidth() : WTF::nullopt;
            auto slotWidth = columnWidthBalancingBase == ColumnWidthBalancingBase::MinimumWidth ? slot.widthConstraints().minimum : slot.widthConstraints().maximum;
            return std::max(slotWidth, columnFixedWidth.valueOr(0_lu));
        });
        // Set finial horizontal position and width.
        auto columnLogicalLeft = grid.horizontalSpacing();
        for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
            auto& column = columns.list()[columnIndex];
            auto columnWidth = LayoutUnit { distributedSpaces[columnIndex] };

            column.setLogicalLeft(columnLogicalLeft);
            column.setLogicalWidth(columnWidth);
            columnLogicalLeft += columnWidth + grid.horizontalSpacing();
        }
    };

    auto needsExtraSpaceDistribution = availableHorizontalSpace != tableWidthConstraints.minimum && availableHorizontalSpace != tableWidthConstraints.maximum;
    if (!needsExtraSpaceDistribution) {
        auto columnWidthBalancingBase = availableHorizontalSpace == tableWidthConstraints.maximum ? ColumnWidthBalancingBase::MaximumWidth : ColumnWidthBalancingBase::MinimumWidth;
        computeColumnWidths(columnWidthBalancingBase, LayoutUnit { });
        return;
    }
    auto horizontalSpaceToDistribute = availableHorizontalSpace - tableWidthConstraints.minimum;
    ASSERT(horizontalSpaceToDistribute > 0);
    computeColumnWidths(ColumnWidthBalancingBase::MinimumWidth, horizontalSpaceToDistribute);
}

void TableFormattingContext::computeAndDistributeExtraVerticalSpace(LayoutUnit availableHorizontalSpace, Optional<LayoutUnit> availableVerticalSpace)
{
    auto& grid = formattingState().tableGrid();
    auto& columns = grid.columns().list();
    auto& rows = grid.rows();

    struct RowHeight {
        InlineLayoutUnit height() const { return ascent + descent; }

        InlineLayoutUnit ascent { 0 };
        InlineLayoutUnit descent { 0 };
    };
    Vector<RowHeight> rowHeight(rows.size());
    Vector<SlotPosition> spanningRowPositionList;
    float tableUsedHeight = 0;
    // 1. Collect initial, basline aligned row heights.
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        auto maximumColumnAscent = InlineLayoutUnit { };
        auto maximumColumnDescent = InlineLayoutUnit { };
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
            auto& cellBox = cell.box();
            cell.setBaselineOffset(geometry().usedBaselineForCell(cellBox));
            maximumColumnAscent = std::max(maximumColumnAscent, cell.baselineOffset());
            maximumColumnDescent = std::max(maximumColumnDescent, geometryForBox(cellBox).height() - cell.baselineOffset());
        }
        // <tr style="height: 10px"> is considered as min height.
        rowHeight[rowIndex] = { maximumColumnAscent, maximumColumnDescent };
        tableUsedHeight += maximumColumnAscent + maximumColumnDescent;
    }
    // FIXME: Collect spanning row maximum heights.

    // Distribute extra space if the table is supposed to be taller than the sum of the row heights.
    float spaceToDistribute = 0;
    if (availableVerticalSpace)
        spaceToDistribute = std::max(0.0f, *availableVerticalSpace - ((rows.size() + 1) * grid.verticalSpacing()) - tableUsedHeight);
    auto distributedSpaces = distributeAvailableSpace<RowSpan>(grid, spaceToDistribute, [&] (const TableGrid::Slot& slot, size_t rowIndex) {
        if (slot.hasRowSpan())
            return geometryForBox(slot.cell().box()).height();
        auto computedRowHeight = geometry().computedHeight(rows.list()[rowIndex].box(), { });
        return std::max(LayoutUnit { rowHeight[rowIndex].height() }, computedRowHeight.valueOr(0_lu));
    });

    auto rowLogicalTop = grid.verticalSpacing();
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        auto& row = grid.rows().list()[rowIndex];
        auto rowUsedHeight = LayoutUnit { distributedSpaces[rowIndex] };

        row.setLogicalHeight(rowUsedHeight);
        row.setBaselineOffset(rowHeight[rowIndex].ascent);
        row.setLogicalTop(rowLogicalTop);
        rowLogicalTop += rowUsedHeight + grid.verticalSpacing();
    }
}

}
}

#endif
