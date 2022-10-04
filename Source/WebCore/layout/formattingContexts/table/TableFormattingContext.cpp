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

#include "BlockFormattingState.h"
#include "FloatingState.h"
#include "InlineFormattingState.h"
#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"
#include "LayoutContext.h"
#include "LayoutInitialContainingBlock.h"
#include "TableFormattingConstraints.h"
#include "TableFormattingState.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(TableFormattingContext);

// https://www.w3.org/TR/css-tables-3/#table-layout-algorithm
TableFormattingContext::TableFormattingContext(const ElementBox& formattingContextRoot, TableFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
    , m_tableFormattingGeometry(*this)
    , m_tableFormattingQuirks(*this)
{
}

void TableFormattingContext::layoutInFlowContent(const ConstraintsForInFlowContent& constraints)
{
    auto availableHorizontalSpace = constraints.horizontal().logicalWidth;
    auto availableVerticalSpace = downcast<ConstraintsForTableContent>(constraints).availableVerticalSpaceForContent();
    // 1. Compute width and height for the grid.
    computeAndDistributeExtraSpace(availableHorizontalSpace, availableVerticalSpace);
    // 2. Finalize cells.
    setUsedGeometryForCells(availableHorizontalSpace, availableVerticalSpace);
    // 3. Finalize rows.
    setUsedGeometryForRows(availableHorizontalSpace);
    // 4. Finalize sections.
    setUsedGeometryForSections(constraints);
}

LayoutUnit TableFormattingContext::usedContentHeight() const
{
    // Table has to have some section content, at least one <tbody>.
    auto top = BoxGeometry::marginBoxRect(geometryForBox(*root().firstInFlowChild())).top();
    auto bottom = BoxGeometry::marginBoxRect(geometryForBox(*root().lastInFlowChild())).bottom();
    return bottom - top;
}

void TableFormattingContext::setUsedGeometryForCells(LayoutUnit availableHorizontalSpace, std::optional<LayoutUnit> availableVerticalSpace)
{
    auto& grid = formattingState().tableGrid();
    auto& columnList = grid.columns().list();
    auto& rowList = grid.rows().list();
    auto& formattingGeometry = this->formattingGeometry();
    // Final table cell layout. At this point all percentage values can be resolved.
    auto sectionOffset = LayoutUnit { };
    auto* currentSection = &rowList.first().box().parent();
    for (auto& cell : grid.cells()) {
        auto& cellBox = cell->box();
        auto& cellBoxGeometry = formattingState().boxGeometry(cellBox);
        auto& section = rowList[cell->startRow()].box().parent();
        if (&section != currentSection) {
            currentSection = &section;
            // While the grid is a continuous flow of rows, in the display tree they are relative to their sections.
            sectionOffset = rowList[cell->startRow()].logicalTop();
        }
        // Internal table elements do not have margins.
        cellBoxGeometry.setHorizontalMargin({ });
        cellBoxGeometry.setVerticalMargin({ });

        cellBoxGeometry.setBorder(formattingGeometry.computedCellBorder(*cell));
        cellBoxGeometry.setPadding(formattingGeometry.computedPadding(cellBox, availableHorizontalSpace));
        cellBoxGeometry.setLogicalTop(rowList[cell->startRow()].logicalTop() - sectionOffset);
        cellBoxGeometry.setLogicalLeft(columnList[cell->startColumn()].usedLogicalLeft());
        cellBoxGeometry.setContentBoxWidth(formattingGeometry.horizontalSpaceForCellContent(*cell));

        if (cellBox.hasInFlowOrFloatingChild()) {
            // FIXME: This should probably be part of the invalidation state to indicate when we re-layout the cell multiple times as part of the multi-pass table algorithm.
            auto& floatingStateForCellContent = layoutState().ensureBlockFormattingState(cellBox).floatingState();
            floatingStateForCellContent.clear();
            LayoutContext::createFormattingContext(cellBox, layoutState())->layoutInFlowContent(formattingGeometry.constraintsForInFlowContent(cellBox));
        }
        cellBoxGeometry.setContentBoxHeight(formattingGeometry.verticalSpaceForCellContent(*cell, availableVerticalSpace));

        auto computeIntrinsicVerticalPaddingForCell = [&] {
            auto cellLogicalHeight = rowList[cell->startRow()].logicalHeight();
            for (size_t rowIndex = cell->startRow() + 1; rowIndex < cell->endRow(); ++rowIndex)
                cellLogicalHeight += rowList[rowIndex].logicalHeight();
            cellLogicalHeight += (cell->rowSpan() - 1) * grid.verticalSpacing();
            // Intrinsic padding is the extra padding for the cell box when it is shorter than the row. Cell boxes have to
            // fill the available vertical space
            // e.g <td height=100px></td><td height=1px></td>
            // the second <td> ends up being 100px tall too with the extra intrinsic padding.

            // FIXME: Find out if it is ok to use the regular padding here to align the content box inside a tall cell or we need to
            // use some kind of intrinsic padding similar to RenderTableCell.
            auto paddingTop = valueOrDefault(cellBoxGeometry.paddingBefore());
            auto paddingBottom = valueOrDefault(cellBoxGeometry.paddingAfter());
            auto intrinsicPaddingTop = LayoutUnit { };
            auto intrinsicPaddingBottom = LayoutUnit { };

            switch (cellBox.style().verticalAlign()) {
            case VerticalAlign::Middle: {
                auto intrinsicVerticalPadding = std::max(0_lu, cellLogicalHeight - cellBoxGeometry.verticalMarginBorderAndPadding() - cellBoxGeometry.contentBoxHeight());
                intrinsicPaddingTop = intrinsicVerticalPadding / 2;
                intrinsicPaddingBottom = intrinsicVerticalPadding / 2;
                break;
            }
            case VerticalAlign::Baseline: {
                auto rowBaseline = LayoutUnit { rowList[cell->startRow()].baseline() };
                auto cellBaseline = LayoutUnit { cell->baseline() };
                intrinsicPaddingTop = std::max(0_lu, rowBaseline - cellBaseline - cellBoxGeometry.borderBefore());
                intrinsicPaddingBottom = std::max(0_lu, cellLogicalHeight - cellBoxGeometry.verticalMarginBorderAndPadding() - intrinsicPaddingTop - cellBoxGeometry.contentBoxHeight());
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
                    // Normally by the time we start positioning the child content, we already have computed borders and padding for the containing block.
                    // This is different with table cells where the final padding offset depends on the content height as we use
                    // the padding box to vertically align the table cell content.
                    auto& formattingState = layoutState().formattingStateForFormattingContext(cellBox);
                    for (auto* child = cellBox.firstInFlowOrFloatingChild(); child; child = child->nextInFlowOrFloatingSibling()) {
                        if (child->isInlineTextBox())
                            continue;
                        formattingState.boxGeometry(*child).moveVertically(intrinsicPaddingTop);
                    }
                    if (cellBox.establishesInlineFormattingContext()) {
                        auto& inlineFormattingStatee = layoutState().formattingStateForInlineFormattingContext(cellBox);
                        for (auto& box : inlineFormattingStatee.boxes())
                            box.moveVertically(intrinsicPaddingTop);
                        for (auto& line : inlineFormattingStatee.lines())
                            line.moveVertically(intrinsicPaddingTop);
                    }
                };
                adjustCellContentWithInstrinsicPaddingBefore();
            }
            cellBoxGeometry.setVerticalPadding({ paddingTop + intrinsicPaddingTop, paddingBottom + intrinsicPaddingBottom });
        };
        computeIntrinsicVerticalPaddingForCell();
    }
}

void TableFormattingContext::setUsedGeometryForRows(LayoutUnit availableHorizontalSpace)
{
    auto& grid = formattingState().tableGrid();
    auto& rows = grid.rows().list();

    auto rowLogicalTop = grid.verticalSpacing();
    const ElementBox* previousRow = nullptr;
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        auto& row = rows[rowIndex];
        auto& rowBox = row.box();
        auto& rowBoxGeometry = formattingState().boxGeometry(rowBox);

        rowBoxGeometry.setPadding(formattingGeometry().computedPadding(rowBox, availableHorizontalSpace));
        // Internal table elements do not have margins.
        rowBoxGeometry.setHorizontalMargin({ });
        rowBoxGeometry.setVerticalMargin({ });

        auto computedRowBorder = [&] {
            auto border = formattingGeometry().computedBorder(rowBox);
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
        rowBoxGeometry.setContentBoxHeight(row.logicalHeight() - computedRowBorder.height());

        auto rowLogicalWidth = grid.columns().logicalWidth() + 2 * grid.horizontalSpacing();
        if (computedRowBorder.width() > rowLogicalWidth) {
            // See comment above.
            computedRowBorder.horizontal.left = { };
            computedRowBorder.horizontal.right = { };
        }
        rowBoxGeometry.setContentBoxWidth(rowLogicalWidth - computedRowBorder.width());
        rowBoxGeometry.setBorder(computedRowBorder);

        if (previousRow && &previousRow->parent() != &rowBox.parent()) {
            // This row is in a different section.
            rowLogicalTop = { };
        }
        rowBoxGeometry.setLogicalTop(rowLogicalTop);
        rowBoxGeometry.setLogicalLeft({ });

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
            rowBaselines[rowIndex] = std::max(rowBaselines[rowIndex], cell.baseline());
        }
    }
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
        rows[rowIndex].setBaseline(rowBaselines[rowIndex]);
}

void TableFormattingContext::setUsedGeometryForSections(const ConstraintsForInFlowContent& constraints)
{
    auto& grid = formattingState().tableGrid();
    auto& tableBox = root();
    auto sectionWidth = grid.columns().logicalWidth() + 2 * grid.horizontalSpacing();
    auto logicalTop = constraints.logicalTop();
    auto verticalSpacing = grid.verticalSpacing();
    auto paddingBefore = std::optional<LayoutUnit> { verticalSpacing };
    auto paddingAfter = verticalSpacing;
    for (auto& sectionBox : childrenOfType<ElementBox>(tableBox)) {
        auto& sectionBoxGeometry = formattingState().boxGeometry(sectionBox);
        // Section borders are either collapsed or ignored.
        sectionBoxGeometry.setBorder({ });
        // Use fake vertical padding to space out the sections.
        sectionBoxGeometry.setPadding(Edges { { }, { paddingBefore.value_or(0_lu), paddingAfter } });
        paddingBefore = std::nullopt;
        // Internal table elements do not have margins.
        sectionBoxGeometry.setHorizontalMargin({ });
        sectionBoxGeometry.setVerticalMargin({ });

        sectionBoxGeometry.setContentBoxWidth(sectionWidth);
        auto sectionContentHeight = LayoutUnit { };
        size_t rowCount = 0;
        for (auto& rowBox : childrenOfType<ElementBox>(sectionBox)) {
            sectionContentHeight += geometryForBox(rowBox).borderBoxHeight();
            ++rowCount;
        }
        sectionContentHeight += verticalSpacing * (rowCount - 1);
        sectionBoxGeometry.setContentBoxHeight(sectionContentHeight);
        sectionBoxGeometry.setLogicalLeft(constraints.horizontal().logicalLeft);
        sectionBoxGeometry.setLogicalTop(logicalTop);

        logicalTop += sectionBoxGeometry.borderBoxHeight();
    }
}

IntrinsicWidthConstraints TableFormattingContext::computedIntrinsicWidthConstraints()
{
    ASSERT(!root().isSizeContainmentBox());
    // Tables have a slightly different concept of shrink to fit. It's really only different with non-auto "width" values, where
    // a generic shrink-to fit block level box like a float box would be just sized to the computed value of "width", tables
    // can actually be stretched way over.
    auto& grid = formattingState().tableGrid();
    if (auto computedWidthConstraints = grid.widthConstraints())
        return *computedWidthConstraints;

    // Compute the minimum/maximum width of each column.
    auto computedWidthConstraints = computedPreferredWidthForColumns();
    grid.setWidthConstraints(computedWidthConstraints);
    return computedWidthConstraints;
}

IntrinsicWidthConstraints TableFormattingContext::computedPreferredWidthForColumns()
{
    auto& formattingState = this->formattingState();
    auto& grid = formattingState.tableGrid();
    ASSERT(!grid.widthConstraints());

    // Column preferred width computation as follows:
    // 1. Collect fixed column widths set by <colgroup>'s and <col>s
    // 2. Collect each cells' width constraints and adjust fixed width column values.
    // 3. Find the min/max width for each columns using the cell constraints and the <col> fixed widths but ignore column spans.
    // 4. Distribute column spanning cells min/max widths.
    // 5. Add them all up and return the computed min/max widths.
    // 2. Collect the fixed width <col>s.
    auto& columnList = grid.columns().list();
    auto& formattingGeometry = this->formattingGeometry();
    auto collectColsFixedWidth = [&] {
        for (auto& column : columnList) {
            auto fixedWidth = [&] () -> std::optional<LayoutUnit> {
                auto* columnBox = column.box();
                if (!columnBox) {
                    // Anonymous columns don't have associated layout boxes and can't have fixed col size.
                    return { };
                }
                if (auto width = columnBox->columnWidth())
                    return width;
                return formattingGeometry.computedColumnWidth(*columnBox);
            }();
        if (fixedWidth)
            column.setComputedLogicalWidth({ *fixedWidth, LengthType::Fixed });
        }
    };
    collectColsFixedWidth();

    auto hasColumnWithPercentWidth = false;
    auto hasColumnWithFixedWidth = false;
    Vector<std::optional<LayoutUnit>> maximumFixedColumnWidths(columnList.size());
    Vector<std::optional<float>> maximumPercentColumnWidths(columnList.size());

    auto collectCellsIntrinsicWidthConstraints = [&] {
        for (auto& cell : grid.cells()) {
            auto& cellBox = cell->box();
            ASSERT(cellBox.establishesBlockFormattingContext());

            auto intrinsicWidth = formattingState.intrinsicWidthConstraintsForBox(cellBox);
            if (!intrinsicWidth) {
                intrinsicWidth = formattingGeometry.intrinsicWidthConstraintsForCellContent(*cell);
                formattingState.setIntrinsicWidthConstraintsForBox(cellBox, *intrinsicWidth);
            }
            auto cellPosition = cell->position();
            auto& cellStyle = cellBox.style();
            // Expand it with border and padding.
            auto horizontalBorderAndPaddingWidth = formattingGeometry.computedCellBorder(*cell).width()
                + formattingGeometry.fixedValue(cellStyle.paddingLeft()).value_or(0)
                + formattingGeometry.fixedValue(cellStyle.paddingRight()).value_or(0);
            intrinsicWidth->expand(horizontalBorderAndPaddingWidth);
            // Spanner cells put their intrinsic widths on the initial slots.
            grid.slot(cellPosition)->setWidthConstraints(*intrinsicWidth);

            auto cellLogicalWidth = cellStyle.logicalWidth();
            auto columnIndex = cellPosition.column;
            switch (cellLogicalWidth.type()) {
            case LengthType::Fixed: {
                auto fixedWidth = LayoutUnit { cellLogicalWidth.value() } + horizontalBorderAndPaddingWidth;
                maximumFixedColumnWidths[columnIndex] = std::max(maximumFixedColumnWidths[columnIndex].value_or(0_lu), fixedWidth);
                hasColumnWithFixedWidth = true;
                break;
            }
            case LengthType::Percent: {
                maximumPercentColumnWidths[columnIndex] = std::max(maximumPercentColumnWidths[columnIndex].value_or(0.f), cellLogicalWidth.percent());
                hasColumnWithPercentWidth = true;
                break;
            }
            case LengthType::Relative:
                ASSERT_NOT_IMPLEMENTED_YET();
                break;
            default:
                break;
            }
        }
    };
    collectCellsIntrinsicWidthConstraints();

    Vector<IntrinsicWidthConstraints> columnIntrinsicWidths(columnList.size());
    Vector<SlotPosition> spanningCellPositionList;
    size_t numberOfActualColumns = 0;
    auto computeColumnsIntrinsicWidthConstraints = [&] {
        // 3. Collect he min/max width for each column but ignore column spans for now.
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
                auto widthConstraints = slot.widthConstraints();
                if (auto fixedColumnWidth = maximumFixedColumnWidths[columnIndex])
                    widthConstraints.maximum = std::max(*fixedColumnWidth, widthConstraints.minimum);

                columnIntrinsicWidths[columnIndex].minimum = std::max(widthConstraints.minimum, columnIntrinsicWidths[columnIndex].minimum);
                columnIntrinsicWidths[columnIndex].maximum = std::max(widthConstraints.maximum, columnIntrinsicWidths[columnIndex].maximum);
            }
            if (columnHasNonSpannedCell)
                ++numberOfActualColumns;
        }
    };
    computeColumnsIntrinsicWidthConstraints();

    auto resolveSpanningCells = [&] {
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
    };
    resolveSpanningCells();

    // 5. The table min/max widths is just the accumulated column constraints with the percent adjustment.
    auto tableWidthConstraints = IntrinsicWidthConstraints { };
    for (auto& columnIntrinsicWidth : columnIntrinsicWidths)
        tableWidthConstraints += columnIntrinsicWidth;

    auto adjustColumnsWithPercentAndFixedWidthValues = [&] {
        // 6. Adjust the table max width with the percent column values if applicable.
        if (!hasColumnWithFixedWidth && !hasColumnWithPercentWidth)
            return;

        if (hasColumnWithFixedWidth && !hasColumnWithPercentWidth) {
            for (size_t columnIndex = 0; columnIndex < columnList.size(); ++columnIndex) {
                if (auto fixedWidth = maximumFixedColumnWidths[columnIndex])
                    columnList[columnIndex].setComputedLogicalWidth({ *fixedWidth, LengthType::Fixed });
            }
            return;
        } 

        auto remainingPercent = 100.0f;
        auto percentMaximumWidth = LayoutUnit { };
        auto nonPercentColumnsWidth = LayoutUnit { };
        // Resolve the percent values as follows
        // - the percent value is resolved against the column maximum width (fixed or content based) as if the max value represented the percentage value
        //   e.g 50% with the maximum width of 100px produces a resolved width of 200px for the column.
        // - find the largest resolved value across the columns and used that as the maximum width for the percent based columns.
        // - Compute the non-percent based columns width by using the remaining percent value (e.g 50% and 10% columns would leave 40% for the rest of the columns)
        for (size_t columnIndex = 0; columnIndex < columnList.size(); ++columnIndex) {
            auto nonPercentColumnWidth = columnIntrinsicWidths[columnIndex].maximum;
            if (auto fixedWidth = maximumFixedColumnWidths[columnIndex]) {
                columnList[columnIndex].setComputedLogicalWidth({ *fixedWidth, LengthType::Fixed });
                nonPercentColumnWidth = std::max(nonPercentColumnWidth, *fixedWidth);
            }
            if (!maximumPercentColumnWidths[columnIndex]) {
                nonPercentColumnsWidth += nonPercentColumnWidth;
                continue;
            }
            auto percent = std::min(*maximumPercentColumnWidths[columnIndex], remainingPercent);
            columnList[columnIndex].setComputedLogicalWidth({ percent, LengthType::Percent });
            percentMaximumWidth = std::max(percentMaximumWidth, LayoutUnit { nonPercentColumnWidth * 100.0f / percent });
            remainingPercent -= percent;
        }
        ASSERT(remainingPercent >= 0.f);
        auto adjustedMaximumWidth = percentMaximumWidth;
        if (remainingPercent)
            adjustedMaximumWidth = std::max(adjustedMaximumWidth, LayoutUnit { nonPercentColumnsWidth * 100.0f / remainingPercent });
        else {
            // When the table has percent width column(s) and they add up to (or over) 100%, the maximum width is computed to
            // only constrained by the available horizontal width.
            // This is a very odd transition of going from 99.9% to 100%, where 99.9% computes normally (see above)
            // but as soon as we hit the 100% mark, the table suddenly stretches all the way to the horizontal available space.
            // It may very well be an ancient bug we need to support (it maps to the epsilon value in AutoTableLayout::computeIntrinsicLogicalWidths which is to avoid division by zero).
            adjustedMaximumWidth = LayoutUnit::max();
        }
        tableWidthConstraints.maximum = std::max(tableWidthConstraints.maximum, adjustedMaximumWidth);
    };
    adjustColumnsWithPercentAndFixedWidthValues();

    // Expand the preferred width with leading and trailing cell spacing (note that column spanners count as one cell).
    tableWidthConstraints += (numberOfActualColumns + 1) * grid.horizontalSpacing();
    return tableWidthConstraints;
}

void TableFormattingContext::computeAndDistributeExtraSpace(LayoutUnit availableHorizontalSpace, std::optional<LayoutUnit> availableVerticalSpace)
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
        column.setUsedLogicalLeft(columnLogicalLeft);
        column.setUsedLogicalWidth(distributedHorizontalSpaces[columnIndex]);
        columnLogicalLeft += distributedHorizontalSpaces[columnIndex] + grid.horizontalSpacing();
    }

    auto& formattingGeometry = this->formattingGeometry();
    // Rows second.
    auto& rows = grid.rows().list();
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
            auto& slot = *grid.slot({ columnIndex, rowIndex });
            if (slot.isRowSpanned())
                continue;
            auto layoutCellContent = [&](auto& cell) {
                auto& cellBox = cell.box();
                auto& cellBoxGeometry = formattingState().boxGeometry(cellBox);
                cellBoxGeometry.setBorder(formattingGeometry.computedCellBorder(cell));
                cellBoxGeometry.setPadding(formattingGeometry.computedPadding(cellBox, availableHorizontalSpace));
                cellBoxGeometry.setContentBoxWidth(formattingGeometry.horizontalSpaceForCellContent(cell));

                if (cellBox.hasInFlowOrFloatingChild())
                    LayoutContext::createFormattingContext(cellBox, layoutState())->layoutInFlowContent(formattingGeometry.constraintsForInFlowContent(cellBox));
                cellBoxGeometry.setContentBoxHeight(formattingGeometry.verticalSpaceForCellContent(cell, availableVerticalSpace));
            };
            layoutCellContent(slot.cell());
            if (slot.hasRowSpan())
                continue;
            // The minimum height of a row (without spanning-related height distribution) is defined as the height of an hypothetical
            // linebox containing the cells originating in the row.
            auto& cell = slot.cell();
            cell.setBaseline(formattingGeometry.usedBaselineForCell(cell.box()));
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

