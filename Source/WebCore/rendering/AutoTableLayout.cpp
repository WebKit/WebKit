/*
 * Copyright (C) 2002 Lars Knoll (knoll@kde.org)
 *           (C) 2002 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2008, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "AutoTableLayout.h"

#include "RenderChildIterator.h"
#include "RenderFlexibleBox.h"
#include "RenderGrid.h"
#include "RenderTable.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableSection.h"
#include "RenderView.h"

namespace WebCore {

AutoTableLayout::AutoTableLayout(RenderTable* table)
    : TableLayout(table)
    , m_hasPercent(false)
    , m_effectiveLogicalWidthDirty(true)
{
}

AutoTableLayout::~AutoTableLayout() = default;

void AutoTableLayout::recalcColumn(unsigned effCol)
{
    Layout& columnLayout = m_layoutStruct[effCol];

    RenderTableCell* fixedContributor = nullptr;
    RenderTableCell* maxContributor = nullptr;

    for (auto& child : childrenOfType<RenderObject>(*m_table)) {
        if (is<RenderTableCol>(child)) {
            // RenderTableCols don't have the concept of preferred logical width, but we need to clear their dirty bits
            // so that if we call setPreferredWidthsDirty(true) on a col or one of its descendants, we'll mark it's
            // ancestors as dirty.
            downcast<RenderTableCol>(child).clearPreferredLogicalWidthsDirtyBits();
        } else if (is<RenderTableSection>(child)) {
            auto& section = downcast<RenderTableSection>(child);
            unsigned numRows = section.numRows();
            for (unsigned i = 0; i < numRows; ++i) {
                RenderTableSection::CellStruct current = section.cellAt(i, effCol);
                RenderTableCell* cell = current.primaryCell();
                
                if (current.inColSpan || !cell)
                    continue;

                bool cellHasContent = cell->firstChild() || cell->style().hasBorder() || cell->style().hasPadding() || cell->style().hasBackground();
                if (cellHasContent)
                    columnLayout.emptyCellsOnly = false;

                // A cell originates in this column. Ensure we have
                // a min/max width of at least 1px for this column now.
                columnLayout.minLogicalWidth = std::max(columnLayout.minLogicalWidth, 0.f);
                columnLayout.maxLogicalWidth = std::max(columnLayout.maxLogicalWidth, 0.f);

                if (cell->colSpan() == 1) {
                    columnLayout.minLogicalWidth = std::max(cell->minPreferredLogicalWidth().ceilToFloat(), columnLayout.minLogicalWidth);
                    float maxPreferredWidth = cell->maxPreferredLogicalWidth().ceilToFloat();
                    if (maxPreferredWidth > columnLayout.maxLogicalWidth) {
                        columnLayout.maxLogicalWidth = maxPreferredWidth;
                        maxContributor = cell;
                    }

                    // All browsers implement a size limit on the cell's max width. 
                    // Our limit is based on KHTML's representation that used 16 bits widths.
                    // FIXME: Other browsers have a lower limit for the cell's max width. 
                    const float cCellMaxWidth = 32760;
                    Length cellLogicalWidth = cell->styleOrColLogicalWidth();
                    if (cellLogicalWidth.value() > cCellMaxWidth)
                        cellLogicalWidth.setValue(Fixed, cCellMaxWidth);
                    if (cellLogicalWidth.isNegative())
                        cellLogicalWidth.setValue(Fixed, 0);
                    switch (cellLogicalWidth.type()) {
                    case Fixed:
                        // ignore width=0
                        if (cellLogicalWidth.isPositive() && !columnLayout.logicalWidth.isPercentOrCalculated()) {
                            float logicalWidth = cell->adjustBorderBoxLogicalWidthForBoxSizing(cellLogicalWidth.value());
                            if (columnLayout.logicalWidth.isFixed()) {
                                // Nav/IE weirdness
                                if ((logicalWidth > columnLayout.logicalWidth.value()) 
                                    || ((columnLayout.logicalWidth.value() == logicalWidth) && (maxContributor == cell))) {
                                    columnLayout.logicalWidth.setValue(Fixed, logicalWidth);
                                    fixedContributor = cell;
                                }
                            } else {
                                columnLayout.logicalWidth.setValue(Fixed, logicalWidth);
                                fixedContributor = cell;
                            }
                        }
                        break;
                    case Percent:
                        m_hasPercent = true;
                        if (cellLogicalWidth.isPositive() && (!columnLayout.logicalWidth.isPercent() || cellLogicalWidth.percent() > columnLayout.logicalWidth.percent()))
                            columnLayout.logicalWidth = cellLogicalWidth;
                        break;
                    case Relative:
                        // FIXME: Need to understand this case and whether it makes sense to compare values
                        // which are not necessarily of the same type.
                        if (cellLogicalWidth.value() > columnLayout.logicalWidth.value())
                            columnLayout.logicalWidth = cellLogicalWidth;
                        break;
                    default:
                        break;
                    }
                } else if (!effCol || section.primaryCellAt(i, effCol - 1) != cell) {
                    // This spanning cell originates in this column. Insert the cell into spanning cells list.
                    insertSpanCell(cell);
                }
            }
        }
    }

    // Nav/IE weirdness
    if (columnLayout.logicalWidth.isFixed()) {
        if (m_table->document().inQuirksMode() && columnLayout.maxLogicalWidth > columnLayout.logicalWidth.value() && fixedContributor != maxContributor) {
            columnLayout.logicalWidth = Length();
            fixedContributor = nullptr;
        }
    }

    columnLayout.maxLogicalWidth = std::max(columnLayout.maxLogicalWidth, columnLayout.minLogicalWidth);
}

void AutoTableLayout::fullRecalc()
{
    m_hasPercent = false;
    m_effectiveLogicalWidthDirty = true;

    unsigned nEffCols = m_table->numEffCols();
    m_layoutStruct.resizeToFit(nEffCols);
    m_layoutStruct.fill(Layout());
    m_spanCells.fill(0);

    Length groupLogicalWidth;
    unsigned currentColumn = 0;
    for (RenderTableCol* column = m_table->firstColumn(); column; column = column->nextColumn()) {
        if (column->isTableColumnGroupWithColumnChildren())
            groupLogicalWidth = column->style().logicalWidth();
        else {
            Length colLogicalWidth = column->style().logicalWidth();
            if (colLogicalWidth.isAuto())
                colLogicalWidth = groupLogicalWidth;
            if ((colLogicalWidth.isFixed() || colLogicalWidth.isPercentOrCalculated()) && colLogicalWidth.isZero())
                colLogicalWidth = Length();
            unsigned effCol = m_table->colToEffCol(currentColumn);
            unsigned span = column->span();
            if (!colLogicalWidth.isAuto() && span == 1 && effCol < nEffCols && m_table->spanOfEffCol(effCol) == 1) {
                m_layoutStruct[effCol].logicalWidth = colLogicalWidth;
                if (colLogicalWidth.isFixed() && m_layoutStruct[effCol].maxLogicalWidth < colLogicalWidth.value())
                    m_layoutStruct[effCol].maxLogicalWidth = colLogicalWidth.value();
            }
            currentColumn += span;
        }

        // For the last column in a column-group, we invalidate our group logical width.
        if (column->isTableColumn() && !column->nextSibling())
            groupLogicalWidth = Length();
    }

    for (unsigned i = 0; i < nEffCols; i++)
        recalcColumn(i);
}

static bool shouldScaleColumnsForParent(const RenderTable& table)
{
    RenderBlock* containingBlock = table.containingBlock();
    while (containingBlock && !is<RenderView>(containingBlock)) {
        // It doesn't matter if our table is auto or fixed: auto means we don't
        // scale. Fixed doesn't care if we do or not because it doesn't depend
        // on the cell contents' preferred widths.
        if (is<RenderTableCell>(containingBlock))
            return false;
        // The max logical width of a table may be "infinity" (or tableMaxWidth, to be more exact) if the sum if the
        // columns' percentages is 100% or more, AND there is at least one column that has a non-percentage-based positive
        // logical width. In such situations no table logical width will be large enough to satisfy the constraint
        // set by the contents. So the idea is to use ~infinity to make sure we use all available size in the containing
        // block. However, this just doesn't work if this is a flex or grid item, so disallow scaling in that case.
        if (is<RenderFlexibleBox>(containingBlock) || is<RenderGrid>(containingBlock))
            return false;
        containingBlock = containingBlock->containingBlock();
    }
    return true;
}

// FIXME: This needs to be adapted for vertical writing modes.
static bool shouldScaleColumnsForSelf(RenderTable* table)
{
    // Normally, scale all columns to satisfy this from CSS2.2:
    // "A percentage value for a column width is relative to the table width.
    // If the table has 'width: auto', a percentage represents a constraint on the column's width"

    // A special case.  If this table is not fixed width and contained inside
    // a cell, then don't bloat the maxwidth by examining percentage growth.
    bool scale = true;
    while (table) {
        Length tableWidth = table->style().width();
        if ((tableWidth.isAuto() || tableWidth.isPercentOrCalculated()) && !table->isOutOfFlowPositioned()) {
            RenderBlock* containingBlock = table->containingBlock();
            while (containingBlock && !is<RenderView>(*containingBlock) && !is<RenderTableCell>(*containingBlock)
                && containingBlock->style().width().isAuto() && !containingBlock->isOutOfFlowPositioned())
                containingBlock = containingBlock->containingBlock();

            table = nullptr;
            if (is<RenderTableCell>(containingBlock)
                && (containingBlock->style().width().isAuto() || containingBlock->style().width().isPercentOrCalculated())) {
                RenderTableCell& cell = downcast<RenderTableCell>(*containingBlock);
                if (cell.colSpan() > 1 || cell.table()->style().width().isAuto())
                    scale = false;
                else
                    table = cell.table();
            }
        }
        else
            table = nullptr;
    }
    return scale;
}

void AutoTableLayout::computeIntrinsicLogicalWidths(LayoutUnit& minWidth, LayoutUnit& maxWidth)
{
    fullRecalc();

    float spanMaxLogicalWidth = calcEffectiveLogicalWidth();
    minWidth = 0;
    maxWidth = 0;
    float maxPercent = 0;
    float maxNonPercent = 0;
    bool scaleColumnsForSelf = shouldScaleColumnsForSelf(m_table);

    // We substitute 0 percent by (epsilon / percentScaleFactor) percent in two places below to avoid division by zero.
    // FIXME: Handle the 0% cases properly.
    const float epsilon = 1 / 128.0f;

    float remainingPercent = 100;
    for (size_t i = 0; i < m_layoutStruct.size(); ++i) {
        minWidth += m_layoutStruct[i].effectiveMinLogicalWidth;
        maxWidth += m_layoutStruct[i].effectiveMaxLogicalWidth;
        if (scaleColumnsForSelf) {
            if (m_layoutStruct[i].effectiveLogicalWidth.isPercent()) {
                float percent = std::min(m_layoutStruct[i].effectiveLogicalWidth.percent(), remainingPercent);
                float logicalWidth = m_layoutStruct[i].effectiveMaxLogicalWidth * 100 / std::max(percent, epsilon);
                maxPercent = std::max(logicalWidth,  maxPercent);
                remainingPercent -= percent;
            } else
                maxNonPercent += m_layoutStruct[i].effectiveMaxLogicalWidth;
        }
    }

    if (scaleColumnsForSelf) {
        maxNonPercent = maxNonPercent * 100 / std::max(remainingPercent, epsilon);
        m_scaledWidthFromPercentColumns = LayoutUnit(std::min<float>(maxNonPercent, tableMaxWidth));
        m_scaledWidthFromPercentColumns = std::max(m_scaledWidthFromPercentColumns, LayoutUnit(std::min<float>(maxPercent, tableMaxWidth)));
        if (m_scaledWidthFromPercentColumns > maxWidth && shouldScaleColumnsForParent(*m_table))
            maxWidth = m_scaledWidthFromPercentColumns;
    }

    maxWidth = std::max(maxWidth, LayoutUnit(spanMaxLogicalWidth));
}

void AutoTableLayout::applyPreferredLogicalWidthQuirks(LayoutUnit& minWidth, LayoutUnit& maxWidth) const
{
    Length tableLogicalWidth = m_table->style().logicalWidth();
    if (tableLogicalWidth.isFixed() && tableLogicalWidth.isPositive())
        minWidth = maxWidth = std::max(minWidth, LayoutUnit(tableLogicalWidth.value()));
}

/*
  This method takes care of colspans.
  effWidth is the same as width for cells without colspans. If we have colspans, they get modified.
 */
float AutoTableLayout::calcEffectiveLogicalWidth()
{
    float maxLogicalWidth = 0;

    size_t nEffCols = m_layoutStruct.size();
    float spacingInRowDirection = m_table->hBorderSpacing();

    for (size_t i = 0; i < nEffCols; ++i) {
        m_layoutStruct[i].effectiveLogicalWidth = m_layoutStruct[i].logicalWidth;
        m_layoutStruct[i].effectiveMinLogicalWidth = m_layoutStruct[i].minLogicalWidth;
        m_layoutStruct[i].effectiveMaxLogicalWidth = m_layoutStruct[i].maxLogicalWidth;
    }

    for (size_t i = 0; i < m_spanCells.size(); ++i) {
        RenderTableCell* cell = m_spanCells[i];
        if (!cell)
            break;

        unsigned span = cell->colSpan();

        Length cellLogicalWidth = cell->styleOrColLogicalWidth();
        if (!cellLogicalWidth.isRelative() && cellLogicalWidth.isZero())
            cellLogicalWidth = Length(); // make it Auto

        unsigned effCol = m_table->colToEffCol(cell->col());
        size_t lastCol = effCol;
        float cellMinLogicalWidth = cell->minPreferredLogicalWidth() + spacingInRowDirection;
        float cellMaxLogicalWidth = cell->maxPreferredLogicalWidth() + spacingInRowDirection;
        float totalPercent = 0;
        float spanMinLogicalWidth = 0;
        float spanMaxLogicalWidth = 0;
        bool allColsArePercent = true;
        bool allColsAreFixed = true;
        bool haveAuto = false;
        bool spanHasEmptyCellsOnly = true;
        float fixedWidth = 0;
        while (lastCol < nEffCols && span > 0) {
            Layout& columnLayout = m_layoutStruct[lastCol];
            switch (columnLayout.logicalWidth.type()) {
            case Percent:
                totalPercent += columnLayout.logicalWidth.percent();
                allColsAreFixed = false;
                break;
            case Fixed:
                if (columnLayout.logicalWidth.value() > 0) {
                    fixedWidth += columnLayout.logicalWidth.value();
                    allColsArePercent = false;
                    // IE resets effWidth to Auto here, but this breaks the konqueror about page and seems to be some bad
                    // legacy behaviour anyway. mozilla doesn't do this so I decided we don't neither.
                    break;
                }
                FALLTHROUGH;
            case Auto:
                haveAuto = true;
                FALLTHROUGH;
            default:
                // If the column is a percentage width, do not let the spanning cell overwrite the
                // width value.  This caused a mis-rendering on amazon.com.
                // Sample snippet:
                // <table border=2 width=100%><
                //   <tr><td>1</td><td colspan=2>2-3</tr>
                //   <tr><td>1</td><td colspan=2 width=100%>2-3</td></tr>
                // </table>
                if (!columnLayout.effectiveLogicalWidth.isPercent()) {
                    columnLayout.effectiveLogicalWidth = Length();
                    allColsArePercent = false;
                } else
                    totalPercent += columnLayout.effectiveLogicalWidth.percent();
                allColsAreFixed = false;
            }
            if (!columnLayout.emptyCellsOnly)
                spanHasEmptyCellsOnly = false;
            span -= m_table->spanOfEffCol(lastCol);
            spanMinLogicalWidth += columnLayout.effectiveMinLogicalWidth;
            spanMaxLogicalWidth += columnLayout.effectiveMaxLogicalWidth;
            lastCol++;
            cellMinLogicalWidth -= spacingInRowDirection;
            cellMaxLogicalWidth -= spacingInRowDirection;
        }

        // adjust table max width if needed
        if (cellLogicalWidth.isPercent()) {
            if (totalPercent > cellLogicalWidth.percent() || allColsArePercent) {
                // can't satify this condition, treat as variable
                cellLogicalWidth = Length();
            } else {
                maxLogicalWidth = std::max(maxLogicalWidth, std::max(spanMaxLogicalWidth, cellMaxLogicalWidth) * 100  / cellLogicalWidth.percent());

                // all non percent columns in the span get percent values to sum up correctly.
                float percentMissing = cellLogicalWidth.percent() - totalPercent;
                float totalWidth = 0;
                for (unsigned pos = effCol; pos < lastCol; ++pos) {
                    if (!m_layoutStruct[pos].effectiveLogicalWidth.isPercentOrCalculated())
                        totalWidth += m_layoutStruct[pos].effectiveMaxLogicalWidth;
                }

                for (unsigned pos = effCol; pos < lastCol; ++pos) {
                    if (!m_layoutStruct[pos].effectiveLogicalWidth.isPercentOrCalculated()) {
                        // Handle the case when there's only one cell with 'width: percent' and it's empty.
                        auto percent = percentMissing * (totalWidth ? m_layoutStruct[pos].effectiveMaxLogicalWidth / totalWidth : 1);
                        totalWidth -= m_layoutStruct[pos].effectiveMaxLogicalWidth;
                        percentMissing -= percent;
                        if (percent > 0)
                            m_layoutStruct[pos].effectiveLogicalWidth.setValue(Percent, percent);
                        else
                            m_layoutStruct[pos].effectiveLogicalWidth = Length();
                    }
                    if (totalWidth <= 0)
                        break;
                }
            }
        }

        // make sure minWidth and maxWidth of the spanning cell are honoured
        if (cellMinLogicalWidth > spanMinLogicalWidth) {
            if (allColsAreFixed) {
                for (unsigned pos = effCol; fixedWidth > 0 && pos < lastCol; ++pos) {
                    float cellLogicalWidth = std::max(m_layoutStruct[pos].effectiveMinLogicalWidth, cellMinLogicalWidth * m_layoutStruct[pos].logicalWidth.value() / fixedWidth);
                    fixedWidth -= m_layoutStruct[pos].logicalWidth.value();
                    cellMinLogicalWidth -= cellLogicalWidth;
                    m_layoutStruct[pos].effectiveMinLogicalWidth = cellLogicalWidth;
                }
            } else if (allColsArePercent) {
                // In this case, we just split the colspan's min amd max widths following the percentage.
                float allocatedMinLogicalWidth = 0;
                float allocatedMaxLogicalWidth = 0;
                for (unsigned pos = effCol; pos < lastCol; ++pos) {
                    ASSERT(m_layoutStruct[pos].logicalWidth.isPercent() || m_layoutStruct[pos].effectiveLogicalWidth.isPercent());
                    // |allColsArePercent| means that either the logicalWidth *or* the effectiveLogicalWidth are percents, handle both of them here.
                    float percent = m_layoutStruct[pos].logicalWidth.isPercent() ? m_layoutStruct[pos].logicalWidth.percent() : m_layoutStruct[pos].effectiveLogicalWidth.percent();
                    float columnMinLogicalWidth = percent * cellMinLogicalWidth / totalPercent;
                    float columnMaxLogicalWidth = percent * cellMaxLogicalWidth / totalPercent;
                    m_layoutStruct[pos].effectiveMinLogicalWidth = std::max(m_layoutStruct[pos].effectiveMinLogicalWidth, columnMinLogicalWidth);
                    m_layoutStruct[pos].effectiveMaxLogicalWidth = columnMaxLogicalWidth;
                    allocatedMinLogicalWidth += columnMinLogicalWidth;
                    allocatedMaxLogicalWidth += columnMaxLogicalWidth;
                }
                ASSERT(allocatedMinLogicalWidth < cellMinLogicalWidth || WTF::areEssentiallyEqual(allocatedMinLogicalWidth, cellMinLogicalWidth));
                ASSERT(allocatedMaxLogicalWidth < cellMaxLogicalWidth || WTF::areEssentiallyEqual(allocatedMaxLogicalWidth, cellMaxLogicalWidth));
                cellMinLogicalWidth -= allocatedMinLogicalWidth;
                cellMaxLogicalWidth -= allocatedMaxLogicalWidth;
            } else {
                float remainingMaxLogicalWidth = spanMaxLogicalWidth;
                float remainingMinLogicalWidth = spanMinLogicalWidth;
                
                // Give min to variable first, to fixed second, and to others third.
                for (unsigned pos = effCol; remainingMaxLogicalWidth >= 0 && pos < lastCol; ++pos) {
                    if (m_layoutStruct[pos].logicalWidth.isFixed() && haveAuto && fixedWidth <= cellMinLogicalWidth) {
                        float colMinLogicalWidth = std::max(m_layoutStruct[pos].effectiveMinLogicalWidth, m_layoutStruct[pos].logicalWidth.value());
                        fixedWidth -= m_layoutStruct[pos].logicalWidth.value();
                        remainingMinLogicalWidth -= m_layoutStruct[pos].effectiveMinLogicalWidth;
                        remainingMaxLogicalWidth -= m_layoutStruct[pos].effectiveMaxLogicalWidth;
                        cellMinLogicalWidth -= colMinLogicalWidth;
                        m_layoutStruct[pos].effectiveMinLogicalWidth = colMinLogicalWidth;
                    }
                }

                for (unsigned pos = effCol; remainingMaxLogicalWidth >= 0 && pos < lastCol && remainingMinLogicalWidth < cellMinLogicalWidth; ++pos) {
                    if (!(m_layoutStruct[pos].logicalWidth.isFixed() && haveAuto && fixedWidth <= cellMinLogicalWidth)) {
                        float colMinLogicalWidth = std::max(m_layoutStruct[pos].effectiveMinLogicalWidth, remainingMaxLogicalWidth ? cellMinLogicalWidth * m_layoutStruct[pos].effectiveMaxLogicalWidth / remainingMaxLogicalWidth : cellMinLogicalWidth);
                        colMinLogicalWidth = std::min(m_layoutStruct[pos].effectiveMinLogicalWidth + (cellMinLogicalWidth - remainingMinLogicalWidth), colMinLogicalWidth);
                        remainingMaxLogicalWidth -= m_layoutStruct[pos].effectiveMaxLogicalWidth;
                        remainingMinLogicalWidth -= m_layoutStruct[pos].effectiveMinLogicalWidth;
                        cellMinLogicalWidth -= colMinLogicalWidth;
                        m_layoutStruct[pos].effectiveMinLogicalWidth = colMinLogicalWidth;
                    }
                }
            }
        }
        if (!cellLogicalWidth.isPercentOrCalculated()) {
            if (cellMaxLogicalWidth > spanMaxLogicalWidth) {
                for (unsigned pos = effCol; spanMaxLogicalWidth >= 0 && pos < lastCol; ++pos) {
                    float colMaxLogicalWidth = std::max(m_layoutStruct[pos].effectiveMaxLogicalWidth, spanMaxLogicalWidth ? cellMaxLogicalWidth * m_layoutStruct[pos].effectiveMaxLogicalWidth / spanMaxLogicalWidth : cellMaxLogicalWidth);
                    spanMaxLogicalWidth -= m_layoutStruct[pos].effectiveMaxLogicalWidth;
                    cellMaxLogicalWidth -= colMaxLogicalWidth;
                    m_layoutStruct[pos].effectiveMaxLogicalWidth = colMaxLogicalWidth;
                }
            }
        } else {
            for (unsigned pos = effCol; pos < lastCol; ++pos)
                m_layoutStruct[pos].maxLogicalWidth = std::max(m_layoutStruct[pos].maxLogicalWidth, m_layoutStruct[pos].minLogicalWidth);
        }
        // treat span ranges consisting of empty cells only as if they had content
        if (spanHasEmptyCellsOnly) {
            for (unsigned pos = effCol; pos < lastCol; ++pos)
                m_layoutStruct[pos].emptyCellsOnly = false;
        }
    }
    m_effectiveLogicalWidthDirty = false;

    return std::min<float>(maxLogicalWidth, tableMaxWidth);
}

/* gets all cells that originate in a column and have a cellspan > 1
   Sorts them by increasing cellspan
*/
void AutoTableLayout::insertSpanCell(RenderTableCell *cell)
{
    ASSERT_ARG(cell, cell && cell->colSpan() != 1);
    if (!cell || cell->colSpan() == 1)
        return;

    unsigned size = m_spanCells.size();
    if (!size || m_spanCells[size-1] != 0) {
        m_spanCells.grow(size + 10);
        for (unsigned i = 0; i < 10; i++)
            m_spanCells[size + i] = 0;
        size += 10;
    }

    // add them in sort. This is a slow algorithm, and a binary search or a fast sorting after collection would be better
    unsigned pos = 0;
    unsigned span = cell->colSpan();
    while (pos < m_spanCells.size() && m_spanCells[pos] && span > m_spanCells[pos]->colSpan())
        pos++;
    memmove(m_spanCells.data()+pos+1, m_spanCells.data()+pos, (size-pos-1)*sizeof(RenderTableCell *));
    m_spanCells[pos] = cell;
}


void AutoTableLayout::layout()
{
    // table layout based on the values collected in the layout structure.
    float tableLogicalWidth = m_table->logicalWidth() - m_table->bordersPaddingAndSpacingInRowDirection();
    float available = tableLogicalWidth;
    size_t nEffCols = m_table->numEffCols();

    // FIXME: It is possible to be called without having properly updated our internal representation.
    // This means that our preferred logical widths were not recomputed as expected.
    if (nEffCols != m_layoutStruct.size()) {
        fullRecalc();
        // FIXME: Table layout shouldn't modify our table structure (but does due to columns and column-groups).
        nEffCols = m_table->numEffCols();
    }

    if (m_effectiveLogicalWidthDirty)
        calcEffectiveLogicalWidth();

    bool havePercent = false;
    float totalRelative = 0;
    int numFixed = 0;
    size_t numberOfNonEmptyAuto = 0;
    Optional<float> totalAuto;
    float totalFixed = 0;
    float totalPercent = 0;
    float allocAuto = 0;
    unsigned numAutoEmptyCellsOnly = 0;

    // fill up every cell with its minWidth
    for (size_t i = 0; i < nEffCols; ++i) {
        float cellLogicalWidth = m_layoutStruct[i].effectiveMinLogicalWidth;
        m_layoutStruct[i].computedLogicalWidth = cellLogicalWidth;
        available -= cellLogicalWidth;
        Length& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
        switch (logicalWidth.type()) {
        case Percent:
            havePercent = true;
            totalPercent += logicalWidth.percent();
            break;
        case Relative:
            totalRelative += logicalWidth.value();
            break;
        case Fixed:
            numFixed++;
            totalFixed += m_layoutStruct[i].effectiveMaxLogicalWidth;
            break;
        case Auto:
            if (m_layoutStruct[i].emptyCellsOnly)
                numAutoEmptyCellsOnly++;
            else {
                ++numberOfNonEmptyAuto;
                totalAuto = totalAuto.valueOr(0.f) + m_layoutStruct[i].effectiveMaxLogicalWidth;
                allocAuto += cellLogicalWidth;
            }
            break;
        default:
            break;
        }
    }

    // allocate width to percent cols
    if (available > 0 && havePercent) {
        for (size_t i = 0; i < nEffCols; ++i) {
            Length& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
            if (logicalWidth.isPercentOrCalculated()) {
                float cellLogicalWidth = std::max<float>(m_layoutStruct[i].effectiveMinLogicalWidth, minimumValueForLength(logicalWidth, tableLogicalWidth));
                available += m_layoutStruct[i].computedLogicalWidth - cellLogicalWidth;
                m_layoutStruct[i].computedLogicalWidth = cellLogicalWidth;
            }
        }
        if (totalPercent > 100) {
            // remove overallocated space from the last columns
            float excess = tableLogicalWidth * (totalPercent - 100) / 100;
            for (unsigned i = nEffCols; i; ) {
                --i;
                if (m_layoutStruct[i].effectiveLogicalWidth.isPercentOrCalculated()) {
                    float cellLogicalWidth = m_layoutStruct[i].computedLogicalWidth;
                    float reduction = std::min(cellLogicalWidth,  excess);
                    // the lines below might look inconsistent, but that's the way it's handled in mozilla
                    excess -= reduction;
                    float newLogicalWidth = std::max(m_layoutStruct[i].effectiveMinLogicalWidth, cellLogicalWidth - reduction);
                    available += cellLogicalWidth - newLogicalWidth;
                    m_layoutStruct[i].computedLogicalWidth = newLogicalWidth;
                }
            }
        }
    }
    
    // then allocate width to fixed cols
    if (available > 0) {
        for (size_t i = 0; i < nEffCols; ++i) {
            Length& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
            if (logicalWidth.isFixed() && logicalWidth.value() > m_layoutStruct[i].computedLogicalWidth) {
                available += m_layoutStruct[i].computedLogicalWidth - logicalWidth.value();
                m_layoutStruct[i].computedLogicalWidth = logicalWidth.value();
            }
        }
    }

    // now satisfy relative
    if (available > 0) {
        for (size_t i = 0; i < nEffCols; ++i) {
            Length& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
            if (logicalWidth.isRelative() && logicalWidth.value() != 0) {
                // width=0* gets effMinWidth.
                float cellLogicalWidth = logicalWidth.value() * tableLogicalWidth / totalRelative;
                available += m_layoutStruct[i].computedLogicalWidth - cellLogicalWidth;
                m_layoutStruct[i].computedLogicalWidth = cellLogicalWidth;
            }
        }
    }

    // now satisfy variable
    if (available > 0 && numberOfNonEmptyAuto) {
        ASSERT(totalAuto);
        available += allocAuto; // this gets redistributed.
        auto equalWidthForZeroLengthColumns = Optional<float> { };
        if (!*totalAuto) {
            // All columns in this table are (non-empty)zero length with 'width: auto'.
            equalWidthForZeroLengthColumns = available / numberOfNonEmptyAuto;
        }
        for (size_t i = 0; i < nEffCols; ++i) {
            auto& column = m_layoutStruct[i];
            if (!column.effectiveLogicalWidth.isAuto() || column.emptyCellsOnly)
                continue;
            auto columnWidthCandidate = equalWidthForZeroLengthColumns ? *equalWidthForZeroLengthColumns : available * column.effectiveMaxLogicalWidth / *totalAuto;
            column.computedLogicalWidth = std::max(column.computedLogicalWidth, columnWidthCandidate);
            available -= column.computedLogicalWidth;
            if (!equalWidthForZeroLengthColumns) {
                *totalAuto -= column.effectiveMaxLogicalWidth;
                if (*totalAuto <= 0)
                    break;
            }
        }
    }

    // spread over fixed columns
    if (available > 0 && numFixed) {
        for (size_t i = 0; i < nEffCols; ++i) {
            Length& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
            if (logicalWidth.isFixed()) {
                float cellLogicalWidth = available * m_layoutStruct[i].effectiveMaxLogicalWidth / totalFixed;
                available -= cellLogicalWidth;
                totalFixed -= m_layoutStruct[i].effectiveMaxLogicalWidth;
                m_layoutStruct[i].computedLogicalWidth += cellLogicalWidth;
            }
        }
    }

    // spread over percent colums
    if (available > 0 && m_hasPercent && totalPercent < 100) {
        for (size_t i = 0; i < nEffCols; ++i) {
            Length& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
            if (logicalWidth.isPercent()) {
                float cellLogicalWidth = available * logicalWidth.percent() / totalPercent;
                available -= cellLogicalWidth;
                totalPercent -= logicalWidth.percent();
                m_layoutStruct[i].computedLogicalWidth += cellLogicalWidth;
                if (!available || !totalPercent)
                    break;
            }
        }
    }

    // spread over the rest
    if (available > 0 && nEffCols > numAutoEmptyCellsOnly) {
        unsigned total = nEffCols - numAutoEmptyCellsOnly;
        // still have some width to spread
        for (unsigned i = nEffCols; i; ) {
            --i;
            // variable columns with empty cells only don't get any width
            if (m_layoutStruct[i].effectiveLogicalWidth.isAuto() && m_layoutStruct[i].emptyCellsOnly)
                continue;
            float cellLogicalWidth = available / total;
            available -= cellLogicalWidth;
            total--;
            m_layoutStruct[i].computedLogicalWidth += cellLogicalWidth;
        }
    }

    // If we have overallocated, reduce every cell according to the difference between desired width and minwidth
    // this seems to produce to the pixel exact results with IE. Wonder if some of this also holds for width distributing.
    if (available < 0) {
        // Need to reduce cells with the following prioritization:
        // (1) Auto
        // (2) Relative
        // (3) Fixed
        // (4) Percent
        // This is basically the reverse of how we grew the cells.
        if (available < 0) {
            float logicalWidthBeyondMin = 0;
            for (unsigned i = nEffCols; i; ) {
                --i;
                Length& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
                if (logicalWidth.isAuto())
                    logicalWidthBeyondMin += m_layoutStruct[i].computedLogicalWidth - m_layoutStruct[i].effectiveMinLogicalWidth;
            }
            
            for (unsigned i = nEffCols; i && logicalWidthBeyondMin > 0; ) {
                --i;
                Length& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
                if (logicalWidth.isAuto()) {
                    float minMaxDiff = m_layoutStruct[i].computedLogicalWidth - m_layoutStruct[i].effectiveMinLogicalWidth;
                    float reduce = available * minMaxDiff / logicalWidthBeyondMin;
                    m_layoutStruct[i].computedLogicalWidth += reduce;
                    available -= reduce;
                    logicalWidthBeyondMin -= minMaxDiff;
                    if (available >= 0)
                        break;
                }
            }
        }

        if (available < 0) {
            float logicalWidthBeyondMin = 0;
            for (unsigned i = nEffCols; i; ) {
                --i;
                Length& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
                if (logicalWidth.isRelative())
                    logicalWidthBeyondMin += m_layoutStruct[i].computedLogicalWidth - m_layoutStruct[i].effectiveMinLogicalWidth;
            }
            
            for (unsigned i = nEffCols; i && logicalWidthBeyondMin > 0; ) {
                --i;
                Length& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
                if (logicalWidth.isRelative()) {
                    float minMaxDiff = m_layoutStruct[i].computedLogicalWidth - m_layoutStruct[i].effectiveMinLogicalWidth;
                    float reduce = available * minMaxDiff / logicalWidthBeyondMin;
                    m_layoutStruct[i].computedLogicalWidth += reduce;
                    available -= reduce;
                    logicalWidthBeyondMin -= minMaxDiff;
                    if (available >= 0)
                        break;
                }
            }
        }

        if (available < 0) {
            float logicalWidthBeyondMin = 0;
            for (unsigned i = nEffCols; i; ) {
                --i;
                Length& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
                if (logicalWidth.isFixed())
                    logicalWidthBeyondMin += m_layoutStruct[i].computedLogicalWidth - m_layoutStruct[i].effectiveMinLogicalWidth;
            }
            
            for (unsigned i = nEffCols; i && logicalWidthBeyondMin > 0; ) {
                --i;
                Length& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
                if (logicalWidth.isFixed()) {
                    float minMaxDiff = m_layoutStruct[i].computedLogicalWidth - m_layoutStruct[i].effectiveMinLogicalWidth;
                    float reduce = available * minMaxDiff / logicalWidthBeyondMin;
                    m_layoutStruct[i].computedLogicalWidth += reduce;
                    available -= reduce;
                    logicalWidthBeyondMin -= minMaxDiff;
                    if (available >= 0)
                        break;
                }
            }
        }

        if (available < 0) {
            float logicalWidthBeyondMin = 0;
            for (unsigned i = nEffCols; i; ) {
                --i;
                Length& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
                if (logicalWidth.isPercentOrCalculated())
                    logicalWidthBeyondMin += m_layoutStruct[i].computedLogicalWidth - m_layoutStruct[i].effectiveMinLogicalWidth;
            }
            
            for (unsigned i = nEffCols; i && logicalWidthBeyondMin > 0; ) {
                --i;
                Length& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
                if (logicalWidth.isPercentOrCalculated()) {
                    float minMaxDiff = m_layoutStruct[i].computedLogicalWidth - m_layoutStruct[i].effectiveMinLogicalWidth;
                    float reduce = available * minMaxDiff / logicalWidthBeyondMin;
                    m_layoutStruct[i].computedLogicalWidth += reduce;
                    available -= reduce;
                    logicalWidthBeyondMin -= minMaxDiff;
                    if (available >= 0)
                        break;
                }
            }
        }
    }

    LayoutUnit pos;
    for (size_t i = 0; i < nEffCols; ++i) {
        m_table->setColumnPosition(i, pos);
        pos += LayoutUnit::fromFloatCeil(m_layoutStruct[i].computedLogicalWidth) + m_table->hBorderSpacing();
    }
    m_table->setColumnPosition(m_table->columnPositions().size() - 1, pos);
}

}
