/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
#include "RenderTableSection.h"
#include "Document.h"
#include "HitTestResult.h"
#include "HTMLNames.h"
#include "PaintInfo.h"
#include "RenderChildIterator.h"
#include "RenderLayoutState.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableRow.h"
#include "RenderTextControl.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "StyleInheritedData.h"
#include <limits>
#include <wtf/IsoMallocInlines.h>
#include <wtf/HashSet.h>
#include <wtf/StackStats.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderTableSection);

// Those 2 variables are used to balance the memory consumption vs the repaint time on big tables.
static const unsigned gMinTableSizeToUseFastPaintPathWithOverflowingCell = 75 * 75;
static const float gMaxAllowedOverflowingCellRatioForFastPaintPath = 0.1f;

static inline void setRowLogicalHeightToRowStyleLogicalHeightIfNotRelative(RenderTableSection::RowStruct& row)
{
    ASSERT(row.rowRenderer);
    row.logicalHeight = row.rowRenderer->style().logicalHeight();
    if (row.logicalHeight.isRelative())
        row.logicalHeight = Length();
}

static inline void updateLogicalHeightForCell(RenderTableSection::RowStruct& row, const RenderTableCell* cell)
{
    // We ignore height settings on rowspan cells.
    if (cell->rowSpan() != 1)
        return;

    Length logicalHeight = cell->style().logicalHeight();
    if (logicalHeight.isPositive() || (logicalHeight.isRelative() && logicalHeight.value() >= 0)) {
        Length cRowLogicalHeight = row.logicalHeight;
        switch (logicalHeight.type()) {
        case Percent:
            if (!cRowLogicalHeight.isPercent() || cRowLogicalHeight.percent() < logicalHeight.percent())
                row.logicalHeight = logicalHeight;
            break;
        case Fixed:
            if (cRowLogicalHeight.isAuto() || cRowLogicalHeight.isRelative()
                || (cRowLogicalHeight.isFixed() && cRowLogicalHeight.value() < logicalHeight.value()))
                row.logicalHeight = logicalHeight;
            break;
        case Relative:
        default:
            break;
        }
    }
}

RenderTableSection::RenderTableSection(Element& element, RenderStyle&& style)
    : RenderBox(element, WTFMove(style), 0)
{
    setInline(false);
}

RenderTableSection::RenderTableSection(Document& document, RenderStyle&& style)
    : RenderBox(document, WTFMove(style), 0)
{
    setInline(false);
}

RenderTableSection::~RenderTableSection() = default;

void RenderTableSection::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBox::styleDidChange(diff, oldStyle);
    propagateStyleToAnonymousChildren(PropagateToAllChildren);

    // If border was changed, notify table.
    RenderTable* table = this->table();
    if (table && oldStyle && oldStyle->border() != style().border())
        table->invalidateCollapsedBorders();
}

void RenderTableSection::willBeRemovedFromTree()
{
    RenderBox::willBeRemovedFromTree();

    // Preventively invalidate our cells as we may be re-inserted into
    // a new table which would require us to rebuild our structure.
    setNeedsCellRecalc();
}

void RenderTableSection::willInsertTableRow(RenderTableRow& child, RenderObject* beforeChild)
{
    if (beforeChild)
        setNeedsCellRecalc();

    unsigned insertionRow = m_cRow;
    ++m_cRow;
    m_cCol = 0;

    ensureRows(m_cRow);

    m_grid[insertionRow].rowRenderer = &child;
    child.setRowIndex(insertionRow);

    if (!beforeChild)
        setRowLogicalHeightToRowStyleLogicalHeightIfNotRelative(m_grid[insertionRow]);
}

void RenderTableSection::ensureRows(unsigned numRows)
{
    if (numRows <= m_grid.size())
        return;

    unsigned oldSize = m_grid.size();
    m_grid.grow(numRows);

    unsigned effectiveColumnCount = std::max(1u, table()->numEffCols());
    for (unsigned row = oldSize; row < m_grid.size(); ++row)
        m_grid[row].row.resizeToFit(effectiveColumnCount);
}

void RenderTableSection::addCell(RenderTableCell* cell, RenderTableRow* row)
{
    // We don't insert the cell if we need cell recalc as our internal columns' representation
    // will have drifted from the table's representation. Also recalcCells will call addCell
    // at a later time after sync'ing our columns' with the table's.
    if (needsCellRecalc())
        return;

    unsigned rSpan = cell->rowSpan();
    unsigned cSpan = cell->colSpan();
    const Vector<RenderTable::ColumnStruct>& columns = table()->columns();
    unsigned nCols = columns.size();
    unsigned insertionRow = row->rowIndex();

    // ### mozilla still seems to do the old HTML way, even for strict DTD
    // (see the annotation on table cell layouting in the CSS specs and the testcase below:
    // <TABLE border>
    // <TR><TD>1 <TD rowspan="2">2 <TD>3 <TD>4
    // <TR><TD colspan="2">5
    // </TABLE>
    while (m_cCol < nCols && (cellAt(insertionRow, m_cCol).hasCells() || cellAt(insertionRow, m_cCol).inColSpan))
        m_cCol++;

    updateLogicalHeightForCell(m_grid[insertionRow], cell);

    ensureRows(insertionRow + rSpan);

    m_grid[insertionRow].rowRenderer = row;

    unsigned col = m_cCol;
    // tell the cell where it is
    bool inColSpan = false;
    while (cSpan) {
        unsigned currentSpan;
        if (m_cCol >= nCols) {
            table()->appendColumn(cSpan);
            currentSpan = cSpan;
        } else {
            if (cSpan < columns[m_cCol].span)
                table()->splitColumn(m_cCol, cSpan);
            currentSpan = columns[m_cCol].span;
        }
        for (unsigned r = 0; r < rSpan; r++) {
            CellStruct& c = cellAt(insertionRow + r, m_cCol);
            ASSERT(cell);
            c.cells.append(cell);
            // If cells overlap then we take the slow path for painting.
            if (c.cells.size() > 1)
                m_hasMultipleCellLevels = true;
            if (inColSpan)
                c.inColSpan = true;
        }
        m_cCol++;
        cSpan -= currentSpan;
        inColSpan = true;
    }
    cell->setCol(table()->effColToCol(col));
}

static LayoutUnit resolveLogicalHeightForRow(const Length& rowLogicalHeight)
{
    if (rowLogicalHeight.isFixed())
        return rowLogicalHeight.value();
    if (rowLogicalHeight.isCalculated())
        return rowLogicalHeight.nonNanCalculatedValue(0);
    return 0;
}

LayoutUnit RenderTableSection::calcRowLogicalHeight()
{
#ifndef NDEBUG
    SetLayoutNeededForbiddenScope layoutForbiddenScope(this);
#endif

    ASSERT(!needsLayout());

    RenderTableCell* cell;

    // We ignore the border-spacing on any non-top section as it is already included in the previous section's last row position.
    LayoutUnit spacing;
    if (this == table()->topSection())
        spacing = table()->vBorderSpacing();

    LayoutStateMaintainer statePusher(*this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());

    m_rowPos.resize(m_grid.size() + 1);
    m_rowPos[0] = spacing;

    unsigned totalRows = m_grid.size();

    for (unsigned r = 0; r < totalRows; r++) {
        m_grid[r].baseline = 0;
        LayoutUnit baselineDescent;

        // Our base size is the biggest logical height from our cells' styles (excluding row spanning cells).
        m_rowPos[r + 1] = std::max(m_rowPos[r] + resolveLogicalHeightForRow(m_grid[r].logicalHeight), 0_lu);

        Row& row = m_grid[r].row;
        unsigned totalCols = row.size();

        for (unsigned c = 0; c < totalCols; c++) {
            CellStruct& current = cellAt(r, c);
            for (unsigned i = 0; i < current.cells.size(); i++) {
                cell = current.cells[i];
                if (current.inColSpan && cell->rowSpan() == 1)
                    continue;

                // FIXME: We are always adding the height of a rowspan to the last rows which doesn't match
                // other browsers. See webkit.org/b/52185 for example.
                if ((cell->rowIndex() + cell->rowSpan() - 1) != r) {
                    // We will apply the height of the rowspan to the current row if next row is not valid.
                    if ((r + 1) < totalRows) {
                        unsigned col = 0;
                        CellStruct nextRowCell = cellAt(r + 1, col);

                        // We are trying to find that next row is valid or not.
                        while (nextRowCell.cells.size() && nextRowCell.cells[0]->rowSpan() > 1 && nextRowCell.cells[0]->rowIndex() < (r + 1)) {
                            col++;
                            if (col < totalCols)
                                nextRowCell = cellAt(r + 1, col);
                            else
                                break;
                        }

                        // We are adding the height of the rowspan to the current row if next row is not valid.
                        if (col < totalCols && nextRowCell.cells.size())
                            continue;
                    }
                }

                // For row spanning cells, |r| is the last row in the span.
                unsigned cellStartRow = cell->rowIndex();

                if (cell->hasOverrideContentLogicalHeight()) {
                    cell->clearIntrinsicPadding();
                    cell->clearOverrideContentSize();
                    cell->setChildNeedsLayout(MarkOnlyThis);
                    cell->layoutIfNeeded();
                }

                LayoutUnit cellLogicalHeight = cell->logicalHeightForRowSizing();
                m_rowPos[r + 1] = std::max(m_rowPos[r + 1], m_rowPos[cellStartRow] + cellLogicalHeight);

                // Find out the baseline. The baseline is set on the first row in a rowspan.
                if (cell->isBaselineAligned()) {
                    LayoutUnit baselinePosition = cell->cellBaselinePosition() - cell->intrinsicPaddingBefore();
                    LayoutUnit borderAndComputedPaddingBefore = cell->borderAndPaddingBefore() - cell->intrinsicPaddingBefore();
                    if (baselinePosition > borderAndComputedPaddingBefore) {
                        m_grid[cellStartRow].baseline = std::max(m_grid[cellStartRow].baseline, baselinePosition);
                        // The descent of a cell that spans multiple rows does not affect the height of the first row it spans, so don't let it
                        // become the baseline descent applied to the rest of the row. Also we don't account for the baseline descent of
                        // non-spanning cells when computing a spanning cell's extent.
                        LayoutUnit cellStartRowBaselineDescent;
                        if (cell->rowSpan() == 1) {
                            baselineDescent = std::max(baselineDescent, cellLogicalHeight - baselinePosition);
                            cellStartRowBaselineDescent = baselineDescent;
                        }
                        m_rowPos[cellStartRow + 1] = std::max(m_rowPos[cellStartRow + 1], m_rowPos[cellStartRow] + m_grid[cellStartRow].baseline + cellStartRowBaselineDescent);
                    }
                }
            }
        }

        // Add the border-spacing to our final position.
        // Use table border-spacing even in non-top sections
        spacing = table()->vBorderSpacing();
        m_rowPos[r + 1] += m_grid[r].rowRenderer ? spacing : 0_lu;
        m_rowPos[r + 1] = std::max(m_rowPos[r + 1], m_rowPos[r]);
    }

    ASSERT(!needsLayout());

    return m_rowPos[m_grid.size()];
}

void RenderTableSection::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());
    ASSERT(!needsCellRecalc());
    ASSERT(!table()->needsSectionRecalc());

    m_forceSlowPaintPathWithOverflowingCell = false;
    // addChild may over-grow m_grid but we don't want to throw away the memory too early as addChild
    // can be called in a loop (e.g during parsing). Doing it now ensures we have a stable-enough structure.
    m_grid.shrinkToFit();

    LayoutStateMaintainer statePusher(*this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());
    bool paginated = view().frameView().layoutContext().layoutState()->isPaginated();
    
    const Vector<LayoutUnit>& columnPos = table()->columnPositions();
    
    for (unsigned r = 0; r < m_grid.size(); ++r) {
        Row& row = m_grid[r].row;
        unsigned cols = row.size();
        // First, propagate our table layout's information to the cells. This will mark the row as needing layout
        // if there was a column logical width change.
        for (unsigned startColumn = 0; startColumn < cols; ++startColumn) {
            CellStruct& current = row[startColumn];
            RenderTableCell* cell = current.primaryCell();
            if (!cell || current.inColSpan)
                continue;

            unsigned endCol = startColumn;
            unsigned cspan = cell->colSpan();
            while (cspan && endCol < cols) {
                ASSERT(endCol < table()->columns().size());
                cspan -= table()->columns()[endCol].span;
                endCol++;
            }
            LayoutUnit tableLayoutLogicalWidth = columnPos[endCol] - columnPos[startColumn] - table()->hBorderSpacing();
            cell->setCellLogicalWidth(tableLayoutLogicalWidth);
        }

        if (RenderTableRow* rowRenderer = m_grid[r].rowRenderer) {
            if (!rowRenderer->needsLayout() && paginated && view().frameView().layoutContext().layoutState()->pageLogicalHeightChanged())
                rowRenderer->setChildNeedsLayout(MarkOnlyThis);

            rowRenderer->layoutIfNeeded();
        }
    }
    clearNeedsLayout();
}

void RenderTableSection::distributeExtraLogicalHeightToPercentRows(LayoutUnit& extraLogicalHeight, int totalPercent)
{
    if (!totalPercent)
        return;

    unsigned totalRows = m_grid.size();
    LayoutUnit totalHeight = m_rowPos[totalRows] + extraLogicalHeight;
    LayoutUnit totalLogicalHeightAdded;
    totalPercent = std::min(totalPercent, 100);
    LayoutUnit rowHeight = m_rowPos[1] - m_rowPos[0];
    for (unsigned r = 0; r < totalRows; ++r) {
        if (totalPercent > 0 && m_grid[r].logicalHeight.isPercent()) {
            LayoutUnit toAdd = std::min<LayoutUnit>(extraLogicalHeight, (totalHeight * m_grid[r].logicalHeight.percent() / 100) - rowHeight);
            // If toAdd is negative, then we don't want to shrink the row (this bug
            // affected Outlook Web Access).
            toAdd = std::max(0_lu, toAdd);
            totalLogicalHeightAdded += toAdd;
            extraLogicalHeight -= toAdd;
            totalPercent -= m_grid[r].logicalHeight.percent();
        }
        ASSERT(totalRows >= 1);
        if (r < totalRows - 1)
            rowHeight = m_rowPos[r + 2] - m_rowPos[r + 1];
        m_rowPos[r + 1] += totalLogicalHeightAdded;
    }
}

void RenderTableSection::distributeExtraLogicalHeightToAutoRows(LayoutUnit& extraLogicalHeight, unsigned autoRowsCount)
{
    if (!autoRowsCount)
        return;

    LayoutUnit totalLogicalHeightAdded;
    for (unsigned r = 0; r < m_grid.size(); ++r) {
        if (autoRowsCount > 0 && m_grid[r].logicalHeight.isAuto()) {
            // Recomputing |extraLogicalHeightForRow| guarantees that we properly ditribute round |extraLogicalHeight|.
            LayoutUnit extraLogicalHeightForRow = extraLogicalHeight / autoRowsCount;
            totalLogicalHeightAdded += extraLogicalHeightForRow;
            extraLogicalHeight -= extraLogicalHeightForRow;
            --autoRowsCount;
        }
        m_rowPos[r + 1] += totalLogicalHeightAdded;
    }
}

void RenderTableSection::distributeRemainingExtraLogicalHeight(LayoutUnit& extraLogicalHeight)
{
    unsigned totalRows = m_grid.size();

    if (extraLogicalHeight <= 0 || !m_rowPos[totalRows])
        return;

    // FIXME: m_rowPos[totalRows] - m_rowPos[0] is the total rows' size.
    LayoutUnit totalRowSize = m_rowPos[totalRows];
    LayoutUnit totalLogicalHeightAdded;
    LayoutUnit previousRowPosition = m_rowPos[0];
    for (unsigned r = 0; r < totalRows; r++) {
        // weight with the original height
        totalLogicalHeightAdded += extraLogicalHeight * (m_rowPos[r + 1] - previousRowPosition) / totalRowSize;
        previousRowPosition = m_rowPos[r + 1];
        m_rowPos[r + 1] += totalLogicalHeightAdded;
    }

    extraLogicalHeight -= totalLogicalHeightAdded;
}

LayoutUnit RenderTableSection::distributeExtraLogicalHeightToRows(LayoutUnit extraLogicalHeight)
{
    if (!extraLogicalHeight)
        return extraLogicalHeight;

    unsigned totalRows = m_grid.size();
    if (!totalRows)
        return extraLogicalHeight;

    if (!m_rowPos[totalRows] && nextSibling())
        return extraLogicalHeight;

    unsigned autoRowsCount = 0;
    int totalPercent = 0;
    for (unsigned r = 0; r < totalRows; r++) {
        if (m_grid[r].logicalHeight.isAuto())
            ++autoRowsCount;
        else if (m_grid[r].logicalHeight.isPercent())
            totalPercent += m_grid[r].logicalHeight.percent();
    }

    LayoutUnit remainingExtraLogicalHeight = extraLogicalHeight;
    distributeExtraLogicalHeightToPercentRows(remainingExtraLogicalHeight, totalPercent);
    distributeExtraLogicalHeightToAutoRows(remainingExtraLogicalHeight, autoRowsCount);
    distributeRemainingExtraLogicalHeight(remainingExtraLogicalHeight);
    return extraLogicalHeight - remainingExtraLogicalHeight;
}

static bool shouldFlexCellChild(const RenderTableCell& cell, const RenderBox& cellDescendant)
{
    if (!cell.style().logicalHeight().isSpecified())
        return false;
    if (cellDescendant.scrollsOverflowY())
        return true;
    return cellDescendant.shouldTreatChildAsReplacedInTableCells();
}

void RenderTableSection::relayoutCellIfFlexed(RenderTableCell& cell, int rowIndex, int rowHeight)
{
    // Force percent height children to lay themselves out again.
    // This will cause these children to grow to fill the cell.
    // FIXME: There is still more work to do here to fully match WinIE (should
    // it become necessary to do so). In quirks mode, WinIE behaves like we
    // do, but it will clip the cells that spill out of the table section. In
    // strict mode, Mozilla and WinIE both regrow the table to accommodate the
    // new height of the cell (thus letting the percentages cause growth one
    // time only). We may also not be handling row-spanning cells correctly.
    //
    // Note also the oddity where replaced elements always flex, and yet blocks/tables do
    // not necessarily flex. WinIE is crazy and inconsistent, and we can't hope to
    // match the behavior perfectly, but we'll continue to refine it as we discover new
    // bugs. :)
    bool cellChildrenFlex = false;
    bool flexAllChildren = cell.style().logicalHeight().isFixed() || (!table()->style().logicalHeight().isAuto() && rowHeight != cell.logicalHeight());
    
    for (auto& renderer : childrenOfType<RenderBox>(cell)) {
        if (renderer.style().logicalHeight().isPercentOrCalculated()
            && (flexAllChildren || shouldFlexCellChild(cell, renderer))
            && (!is<RenderTable>(renderer) || downcast<RenderTable>(renderer).hasSections())) {
            cellChildrenFlex = true;
            break;
        }
    }

    if (!cellChildrenFlex) {
        if (TrackedRendererListHashSet* percentHeightDescendants = cell.percentHeightDescendants()) {
            for (auto* descendant : *percentHeightDescendants) {
                if (flexAllChildren || shouldFlexCellChild(cell, *descendant)) {
                    cellChildrenFlex = true;
                    break;
                }
            }
        }
    }
    
    if (!cellChildrenFlex)
        return;

    cell.setChildNeedsLayout(MarkOnlyThis);
        // Alignment within a cell is based off the calculated
    // height, which becomes irrelevant once the cell has
    // been resized based off its percentage.
    cell.setOverrideContentLogicalHeightFromRowHeight(rowHeight);
    cell.layoutIfNeeded();
    
    if (!cell.isBaselineAligned())
        return;
    
    // If the baseline moved, we may have to update the data for our row. Find out the new baseline.
    LayoutUnit baseline = cell.cellBaselinePosition();
    if (baseline > cell.borderAndPaddingBefore())
        m_grid[rowIndex].baseline = std::max(m_grid[rowIndex].baseline, baseline);
}

void RenderTableSection::layoutRows()
{
#ifndef NDEBUG
    SetLayoutNeededForbiddenScope layoutForbiddenScope(this);
#endif

    ASSERT(!needsLayout());

    unsigned totalRows = m_grid.size();

    // Set the width of our section now.  The rows will also be this width.
    setLogicalWidth(table()->contentLogicalWidth());
    m_forceSlowPaintPathWithOverflowingCell = false;

    LayoutUnit vspacing = table()->vBorderSpacing();
    unsigned nEffCols = table()->numEffCols();

    LayoutStateMaintainer statePusher(*this, locationOffset(), hasTransform() || style().isFlippedBlocksWritingMode());

    for (unsigned r = 0; r < totalRows; r++) {
        // Set the row's x/y position and width/height.
        if (RenderTableRow* rowRenderer = m_grid[r].rowRenderer) {
            // FIXME: the x() position of the row should be table()->hBorderSpacing() so that it can 
            // report the correct offsetLeft. However, that will require a lot of rebaselining of test results.
            rowRenderer->setLocation(LayoutPoint(0_lu, m_rowPos[r]));
            rowRenderer->setLogicalWidth(logicalWidth());
            rowRenderer->setLogicalHeight(m_rowPos[r + 1] - m_rowPos[r] - vspacing);
            rowRenderer->updateLayerTransform();
            rowRenderer->clearOverflow();
            rowRenderer->addVisualEffectOverflow();
        }

        LayoutUnit rowHeightIncreaseForPagination;

        for (unsigned c = 0; c < nEffCols; c++) {
            CellStruct& cs = cellAt(r, c);
            RenderTableCell* cell = cs.primaryCell();

            if (!cell || cs.inColSpan)
                continue;

            int rowIndex = cell->rowIndex();
            LayoutUnit rHeight = m_rowPos[rowIndex + cell->rowSpan()] - m_rowPos[rowIndex] - vspacing;

            relayoutCellIfFlexed(*cell, r, rHeight);

            cell->computeIntrinsicPadding(rHeight);

            LayoutRect oldCellRect = cell->frameRect();

            setLogicalPositionForCell(cell, c);

            auto* layoutState = view().frameView().layoutContext().layoutState();
            if (!cell->needsLayout() && layoutState->pageLogicalHeight() && layoutState->pageLogicalOffset(cell, cell->logicalTop()) != cell->pageLogicalOffset())
                cell->setChildNeedsLayout(MarkOnlyThis);

            cell->layoutIfNeeded();

            // FIXME: Make pagination work with vertical tables.
            if (layoutState->pageLogicalHeight() && cell->logicalHeight() != rHeight) {
                // FIXME: Pagination might have made us change size. For now just shrink or grow the cell to fit without doing a relayout.
                // We'll also do a basic increase of the row height to accommodate the cell if it's bigger, but this isn't quite right
                // either. It's at least stable though and won't result in an infinite # of relayouts that may never stabilize.
                if (cell->logicalHeight() > rHeight)
                    rowHeightIncreaseForPagination = std::max(rowHeightIncreaseForPagination, cell->logicalHeight() - rHeight);
                cell->setLogicalHeight(rHeight);
            }

            LayoutSize childOffset(cell->location() - oldCellRect.location());
            if (childOffset.width() || childOffset.height()) {
                view().frameView().layoutContext().addLayoutDelta(childOffset);

                // If the child moved, we have to repaint it as well as any floating/positioned
                // descendants.  An exception is if we need a layout.  In this case, we know we're going to
                // repaint ourselves (and the child) anyway.
                if (!table()->selfNeedsLayout() && cell->checkForRepaintDuringLayout())
                    cell->repaintDuringLayoutIfMoved(oldCellRect);
            }
        }
        if (rowHeightIncreaseForPagination) {
            for (unsigned rowIndex = r + 1; rowIndex <= totalRows; rowIndex++)
                m_rowPos[rowIndex] += rowHeightIncreaseForPagination;
            for (unsigned c = 0; c < nEffCols; ++c) {
                Vector<RenderTableCell*, 1>& cells = cellAt(r, c).cells;
                for (size_t i = 0; i < cells.size(); ++i)
                    cells[i]->setLogicalHeight(cells[i]->logicalHeight() + rowHeightIncreaseForPagination);
            }
        }
    }

    ASSERT(!needsLayout());

    setLogicalHeight(m_rowPos[totalRows]);

    computeOverflowFromCells(totalRows, nEffCols);
}

void RenderTableSection::computeOverflowFromCells()
{
    unsigned totalRows = m_grid.size();
    unsigned nEffCols = table()->numEffCols();
    computeOverflowFromCells(totalRows, nEffCols);
}

void RenderTableSection::computeOverflowFromCells(unsigned totalRows, unsigned nEffCols)
{
    clearOverflow();
    m_overflowingCells.clear();
    unsigned totalCellsCount = nEffCols * totalRows;
    unsigned maxAllowedOverflowingCellsCount = totalCellsCount < gMinTableSizeToUseFastPaintPathWithOverflowingCell ? 0 : gMaxAllowedOverflowingCellRatioForFastPaintPath * totalCellsCount;

#ifndef NDEBUG
    bool hasOverflowingCell = false;
#endif
    // Now that our height has been determined, add in overflow from cells.
    for (unsigned r = 0; r < totalRows; r++) {
        for (unsigned c = 0; c < nEffCols; c++) {
            CellStruct& cs = cellAt(r, c);
            RenderTableCell* cell = cs.primaryCell();
            if (!cell || cs.inColSpan)
                continue;
            if (r < totalRows - 1 && cell == primaryCellAt(r + 1, c))
                continue;
            addOverflowFromChild(cell);
#ifndef NDEBUG
            hasOverflowingCell |= cell->hasVisualOverflow();
#endif
            if (cell->hasVisualOverflow() && !m_forceSlowPaintPathWithOverflowingCell) {
                m_overflowingCells.add(cell);
                if (m_overflowingCells.size() > maxAllowedOverflowingCellsCount) {
                    // We need to set m_forcesSlowPaintPath only if there is a least one overflowing cells as the hit testing code rely on this information.
                    m_forceSlowPaintPathWithOverflowingCell = true;
                    // The slow path does not make any use of the overflowing cells info, don't hold on to the memory.
                    m_overflowingCells.clear();
                }
            }
        }
    }
    ASSERT(hasOverflowingCell == this->hasOverflowingCell());
}

LayoutUnit RenderTableSection::calcOuterBorderBefore() const
{
    unsigned totalCols = table()->numEffCols();
    if (!m_grid.size() || !totalCols)
        return 0;

    LayoutUnit borderWidth;

    const BorderValue& sb = style().borderBefore();
    if (sb.style() == BorderStyle::Hidden)
        return -1;
    if (sb.style() > BorderStyle::Hidden)
        borderWidth = sb.width();

    const BorderValue& rb = firstRow()->style().borderBefore();
    if (rb.style() == BorderStyle::Hidden)
        return -1;
    if (rb.style() > BorderStyle::Hidden && rb.width() > borderWidth)
        borderWidth = rb.width();

    bool allHidden = true;
    for (unsigned c = 0; c < totalCols; c++) {
        const CellStruct& current = cellAt(0, c);
        if (current.inColSpan || !current.hasCells())
            continue;
        const BorderValue& cb = current.primaryCell()->style().borderBefore(); // FIXME: Make this work with perpendicular and flipped cells.
        // FIXME: Don't repeat for the same col group
        RenderTableCol* colGroup = table()->colElement(c);
        if (colGroup) {
            const BorderValue& gb = colGroup->style().borderBefore();
            if (gb.style() == BorderStyle::Hidden || cb.style() == BorderStyle::Hidden)
                continue;
            allHidden = false;
            if (gb.style() > BorderStyle::Hidden && gb.width() > borderWidth)
                borderWidth = gb.width();
            if (cb.style() > BorderStyle::Hidden && cb.width() > borderWidth)
                borderWidth = cb.width();
        } else {
            if (cb.style() == BorderStyle::Hidden)
                continue;
            allHidden = false;
            if (cb.style() > BorderStyle::Hidden && cb.width() > borderWidth)
                borderWidth = cb.width();
        }
    }
    if (allHidden)
        return -1;
    return CollapsedBorderValue::adjustedCollapsedBorderWidth(borderWidth, document().deviceScaleFactor(), false);
}

LayoutUnit RenderTableSection::calcOuterBorderAfter() const
{
    unsigned totalCols = table()->numEffCols();
    if (!m_grid.size() || !totalCols)
        return 0;

    LayoutUnit borderWidth;

    const BorderValue& sb = style().borderAfter();
    if (sb.style() == BorderStyle::Hidden)
        return -1;
    if (sb.style() > BorderStyle::Hidden)
        borderWidth = sb.width();

    const BorderValue& rb = lastRow()->style().borderAfter();
    if (rb.style() == BorderStyle::Hidden)
        return -1;
    if (rb.style() > BorderStyle::Hidden && rb.width() > borderWidth)
        borderWidth = rb.width();

    bool allHidden = true;
    for (unsigned c = 0; c < totalCols; c++) {
        const CellStruct& current = cellAt(m_grid.size() - 1, c);
        if (current.inColSpan || !current.hasCells())
            continue;
        const BorderValue& cb = current.primaryCell()->style().borderAfter(); // FIXME: Make this work with perpendicular and flipped cells.
        // FIXME: Don't repeat for the same col group
        RenderTableCol* colGroup = table()->colElement(c);
        if (colGroup) {
            const BorderValue& gb = colGroup->style().borderAfter();
            if (gb.style() == BorderStyle::Hidden || cb.style() == BorderStyle::Hidden)
                continue;
            allHidden = false;
            if (gb.style() > BorderStyle::Hidden && gb.width() > borderWidth)
                borderWidth = gb.width();
            if (cb.style() > BorderStyle::Hidden && cb.width() > borderWidth)
                borderWidth = cb.width();
        } else {
            if (cb.style() == BorderStyle::Hidden)
                continue;
            allHidden = false;
            if (cb.style() > BorderStyle::Hidden && cb.width() > borderWidth)
                borderWidth = cb.width();
        }
    }
    if (allHidden)
        return -1;
    return CollapsedBorderValue::adjustedCollapsedBorderWidth(borderWidth, document().deviceScaleFactor(), true);
}

LayoutUnit RenderTableSection::calcOuterBorderStart() const
{
    unsigned totalCols = table()->numEffCols();
    if (!m_grid.size() || !totalCols)
        return 0;

    LayoutUnit borderWidth;

    const BorderValue& sb = style().borderStart();
    if (sb.style() == BorderStyle::Hidden)
        return -1;
    if (sb.style() > BorderStyle::Hidden)
        borderWidth = sb.width();

    if (RenderTableCol* colGroup = table()->colElement(0)) {
        const BorderValue& gb = colGroup->style().borderStart();
        if (gb.style() == BorderStyle::Hidden)
            return -1;
        if (gb.style() > BorderStyle::Hidden && gb.width() > borderWidth)
            borderWidth = gb.width();
    }

    bool allHidden = true;
    for (unsigned r = 0; r < m_grid.size(); r++) {
        const CellStruct& current = cellAt(r, 0);
        if (!current.hasCells())
            continue;
        // FIXME: Don't repeat for the same cell
        const BorderValue& cb = current.primaryCell()->style().borderStart(); // FIXME: Make this work with perpendicular and flipped cells.
        const BorderValue& rb = current.primaryCell()->parent()->style().borderStart();
        if (cb.style() == BorderStyle::Hidden || rb.style() == BorderStyle::Hidden)
            continue;
        allHidden = false;
        if (cb.style() > BorderStyle::Hidden && cb.width() > borderWidth)
            borderWidth = cb.width();
        if (rb.style() > BorderStyle::Hidden && rb.width() > borderWidth)
            borderWidth = rb.width();
    }
    if (allHidden)
        return -1;
    return CollapsedBorderValue::adjustedCollapsedBorderWidth(borderWidth, document().deviceScaleFactor(), !table()->style().isLeftToRightDirection());
}

LayoutUnit RenderTableSection::calcOuterBorderEnd() const
{
    unsigned totalCols = table()->numEffCols();
    if (!m_grid.size() || !totalCols)
        return 0;

    LayoutUnit borderWidth;

    const BorderValue& sb = style().borderEnd();
    if (sb.style() == BorderStyle::Hidden)
        return -1;
    if (sb.style() > BorderStyle::Hidden)
        borderWidth = sb.width();

    if (RenderTableCol* colGroup = table()->colElement(totalCols - 1)) {
        const BorderValue& gb = colGroup->style().borderEnd();
        if (gb.style() == BorderStyle::Hidden)
            return -1;
        if (gb.style() > BorderStyle::Hidden && gb.width() > borderWidth)
            borderWidth = gb.width();
    }

    bool allHidden = true;
    for (unsigned r = 0; r < m_grid.size(); r++) {
        const CellStruct& current = cellAt(r, totalCols - 1);
        if (!current.hasCells())
            continue;
        // FIXME: Don't repeat for the same cell
        const BorderValue& cb = current.primaryCell()->style().borderEnd(); // FIXME: Make this work with perpendicular and flipped cells.
        const BorderValue& rb = current.primaryCell()->parent()->style().borderEnd();
        if (cb.style() == BorderStyle::Hidden || rb.style() == BorderStyle::Hidden)
            continue;
        allHidden = false;
        if (cb.style() > BorderStyle::Hidden && cb.width() > borderWidth)
            borderWidth = cb.width();
        if (rb.style() > BorderStyle::Hidden && rb.width() > borderWidth)
            borderWidth = rb.width();
    }
    if (allHidden)
        return -1;
    return CollapsedBorderValue::adjustedCollapsedBorderWidth(borderWidth, document().deviceScaleFactor(), table()->style().isLeftToRightDirection());
}

void RenderTableSection::recalcOuterBorder()
{
    m_outerBorderBefore = calcOuterBorderBefore();
    m_outerBorderAfter = calcOuterBorderAfter();
    m_outerBorderStart = calcOuterBorderStart();
    m_outerBorderEnd = calcOuterBorderEnd();
}

Optional<int> RenderTableSection::firstLineBaseline() const
{
    if (!m_grid.size())
        return Optional<int>();

    int firstLineBaseline = m_grid[0].baseline;
    if (firstLineBaseline)
        return firstLineBaseline + roundToInt(m_rowPos[0]);

    Optional<int> result;
    const Row& firstRow = m_grid[0].row;
    for (size_t i = 0; i < firstRow.size(); ++i) {
        const CellStruct& cs = firstRow.at(i);
        const RenderTableCell* cell = cs.primaryCell();
        // Only cells with content have a baseline
        if (cell && cell->contentLogicalHeight()) {
            int candidate = roundToInt(cell->logicalTop() + cell->borderAndPaddingBefore() + cell->contentLogicalHeight());
            result = std::max(result.valueOr(candidate), candidate);
        }
    }

    return result;
}

void RenderTableSection::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(!needsLayout());
    // avoid crashing on bugs that cause us to paint with dirty layout
    if (needsLayout())
        return;
    
    unsigned totalRows = m_grid.size();
    unsigned totalCols = table()->columns().size();

    if (!totalRows || !totalCols)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + location();

    PaintPhase phase = paintInfo.phase;
    bool pushedClip = pushContentsClip(paintInfo, adjustedPaintOffset);
    paintObject(paintInfo, adjustedPaintOffset);
    if (pushedClip)
        popContentsClip(paintInfo, phase, adjustedPaintOffset);

    if ((phase == PaintPhase::Outline || phase == PaintPhase::SelfOutline) && style().visibility() == Visibility::Visible)
        paintOutline(paintInfo, LayoutRect(adjustedPaintOffset, size()));
}

static inline bool compareCellPositions(RenderTableCell* elem1, RenderTableCell* elem2)
{
    return elem1->rowIndex() < elem2->rowIndex();
}

// This comparison is used only when we have overflowing cells as we have an unsorted array to sort. We thus need
// to sort both on rows and columns to properly repaint.
static inline bool compareCellPositionsWithOverflowingCells(RenderTableCell* elem1, RenderTableCell* elem2)
{
    if (elem1->rowIndex() != elem2->rowIndex())
        return elem1->rowIndex() < elem2->rowIndex();

    return elem1->col() < elem2->col();
}

void RenderTableSection::paintCell(RenderTableCell* cell, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint cellPoint = flipForWritingModeForChild(cell, paintOffset);
    PaintPhase paintPhase = paintInfo.phase;
    RenderTableRow& row = downcast<RenderTableRow>(*cell->parent());

    if (paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground) {
        // We need to handle painting a stack of backgrounds.  This stack (from bottom to top) consists of
        // the column group, column, row group, row, and then the cell.
        RenderTableCol* column = table()->colElement(cell->col());
        RenderTableCol* columnGroup = column ? column->enclosingColumnGroup() : nullptr;

        // Column groups and columns first.
        // FIXME: Columns and column groups do not currently support opacity, and they are being painted "too late" in
        // the stack, since we have already opened a transparency layer (potentially) for the table row group.
        // Note that we deliberately ignore whether or not the cell has a layer, since these backgrounds paint "behind" the
        // cell.
        cell->paintBackgroundsBehindCell(paintInfo, cellPoint, columnGroup);
        cell->paintBackgroundsBehindCell(paintInfo, cellPoint, column);

        // Paint the row group next.
        cell->paintBackgroundsBehindCell(paintInfo, cellPoint, this);

        // Paint the row next, but only if it doesn't have a layer.  If a row has a layer, it will be responsible for
        // painting the row background for the cell.
        if (!row.hasSelfPaintingLayer())
            cell->paintBackgroundsBehindCell(paintInfo, cellPoint, &row);
    }
    if ((!cell->hasSelfPaintingLayer() && !row.hasSelfPaintingLayer()))
        cell->paint(paintInfo, cellPoint);
}

LayoutRect RenderTableSection::logicalRectForWritingModeAndDirection(const LayoutRect& rect) const
{
    LayoutRect tableAlignedRect(rect);

    flipForWritingMode(tableAlignedRect);

    if (!style().isHorizontalWritingMode())
        tableAlignedRect = tableAlignedRect.transposedRect();

    const Vector<LayoutUnit>& columnPos = table()->columnPositions();
    // FIXME: The table's direction should determine our row's direction, not the section's (see bug 96691).
    if (!style().isLeftToRightDirection())
        tableAlignedRect.setX(columnPos[columnPos.size() - 1] - tableAlignedRect.maxX());

    return tableAlignedRect;
}

CellSpan RenderTableSection::dirtiedRows(const LayoutRect& damageRect) const
{
    if (m_forceSlowPaintPathWithOverflowingCell) 
        return fullTableRowSpan();

    CellSpan coveredRows = spannedRows(damageRect, IncludeAllIntersectingCells);

    // To repaint the border we might need to repaint first or last row even if they are not spanned themselves.
    if (coveredRows.start >= m_rowPos.size() - 1 && m_rowPos[m_rowPos.size() - 1] + table()->outerBorderAfter() >= damageRect.y())
        --coveredRows.start;

    if (!coveredRows.end && m_rowPos[0] - table()->outerBorderBefore() <= damageRect.maxY())
        ++coveredRows.end;

    return coveredRows;
}

CellSpan RenderTableSection::dirtiedColumns(const LayoutRect& damageRect) const
{
    if (m_forceSlowPaintPathWithOverflowingCell) 
        return fullTableColumnSpan();

    CellSpan coveredColumns = spannedColumns(damageRect, IncludeAllIntersectingCells);

    const Vector<LayoutUnit>& columnPos = table()->columnPositions();
    // To repaint the border we might need to repaint first or last column even if they are not spanned themselves.
    if (coveredColumns.start >= columnPos.size() - 1 && columnPos[columnPos.size() - 1] + table()->outerBorderEnd() >= damageRect.x())
        --coveredColumns.start;

    if (!coveredColumns.end && columnPos[0] - table()->outerBorderStart() <= damageRect.maxX())
        ++coveredColumns.end;

    return coveredColumns;
}

CellSpan RenderTableSection::spannedRows(const LayoutRect& flippedRect, ShouldIncludeAllIntersectingCells shouldIncludeAllIntersectionCells) const
{
    // Find the first row that starts after rect top.
    unsigned nextRow = std::upper_bound(m_rowPos.begin(), m_rowPos.end(), flippedRect.y()) - m_rowPos.begin();
    if (shouldIncludeAllIntersectionCells == IncludeAllIntersectingCells && nextRow && m_rowPos[nextRow - 1] == flippedRect.y())
        --nextRow;

    if (nextRow == m_rowPos.size())
        return CellSpan(m_rowPos.size() - 1, m_rowPos.size() - 1); // After all rows.

    unsigned startRow = nextRow > 0 ? nextRow - 1 : 0;

    // Find the first row that starts after rect bottom.
    unsigned endRow;
    if (m_rowPos[nextRow] >= flippedRect.maxY())
        endRow = nextRow;
    else {
        endRow = std::upper_bound(m_rowPos.begin() + static_cast<int32_t>(nextRow), m_rowPos.end(), flippedRect.maxY()) - m_rowPos.begin();
        if (endRow == m_rowPos.size())
            endRow = m_rowPos.size() - 1;
    }

    return CellSpan(startRow, endRow);
}

CellSpan RenderTableSection::spannedColumns(const LayoutRect& flippedRect, ShouldIncludeAllIntersectingCells shouldIncludeAllIntersectionCells) const
{
    const Vector<LayoutUnit>& columnPos = table()->columnPositions();

    // Find the first column that starts after rect left.
    // lower_bound doesn't handle the edge between two cells properly as it would wrongly return the
    // cell on the logical top/left.
    // upper_bound on the other hand properly returns the cell on the logical bottom/right, which also
    // matches the behavior of other browsers.
    unsigned nextColumn = std::upper_bound(columnPos.begin(), columnPos.end(), flippedRect.x()) - columnPos.begin();
    if (shouldIncludeAllIntersectionCells == IncludeAllIntersectingCells && nextColumn && columnPos[nextColumn - 1] == flippedRect.x())
        --nextColumn;

    if (nextColumn == columnPos.size())
        return CellSpan(columnPos.size() - 1, columnPos.size() - 1); // After all columns.

    unsigned startColumn = nextColumn > 0 ? nextColumn - 1 : 0;

    // Find the first column that starts after rect right.
    unsigned endColumn;
    if (columnPos[nextColumn] >= flippedRect.maxX())
        endColumn = nextColumn;
    else {
        endColumn = std::upper_bound(columnPos.begin() + static_cast<int32_t>(nextColumn), columnPos.end(), flippedRect.maxX()) - columnPos.begin();
        if (endColumn == columnPos.size())
            endColumn = columnPos.size() - 1;
    }

    return CellSpan(startColumn, endColumn);
}

void RenderTableSection::paintRowGroupBorder(const PaintInfo& paintInfo, bool antialias, LayoutRect rect, BoxSide side, CSSPropertyID borderColor, BorderStyle borderStyle, BorderStyle tableBorderStyle)
{
    if (tableBorderStyle == BorderStyle::Hidden)
        return;
    rect.intersect(paintInfo.rect);
    if (rect.isEmpty())
        return;
    drawLineForBoxSide(paintInfo.context(), rect, side, style().visitedDependentColorWithColorFilter(borderColor), borderStyle, 0, 0, antialias);
}

LayoutUnit RenderTableSection::offsetLeftForRowGroupBorder(RenderTableCell* cell, const LayoutRect& rowGroupRect, unsigned row)
{
    if (style().isHorizontalWritingMode()) {
        if (style().isLeftToRightDirection())
            return cell ? cell->x() + cell->width() : 0_lu;
        return -outerBorderLeft(&style());
    }
    bool isLastRow = row + 1 == m_grid.size();
    return rowGroupRect.width() - m_rowPos[row + 1] + (isLastRow ? -outerBorderLeft(&style()) : 0_lu);
}

LayoutUnit RenderTableSection::offsetTopForRowGroupBorder(RenderTableCell* cell, BoxSide borderSide, unsigned row)
{
    bool isLastRow = row + 1 == m_grid.size();
    if (style().isHorizontalWritingMode())
        return m_rowPos[row] + (!row && borderSide == BSRight ? -outerBorderTop(&style()) : isLastRow && borderSide == BSLeft ? outerBorderTop(&style()) : 0_lu);
    if (style().isLeftToRightDirection())
        return (cell ? cell->y() + cell->height() : 0_lu) + (borderSide == BSLeft ? outerBorderTop(&style()) : 0_lu);
    return borderSide == BSRight ? -outerBorderTop(&style()) : 0_lu;
}

LayoutUnit RenderTableSection::verticalRowGroupBorderHeight(RenderTableCell* cell, const LayoutRect& rowGroupRect, unsigned row)
{
    bool isLastRow = row + 1 == m_grid.size();
    if (style().isHorizontalWritingMode())
        return m_rowPos[row + 1] - m_rowPos[row] + (!row ? outerBorderTop(&style()) : isLastRow ? outerBorderBottom(&style()) : 0_lu);
    if (style().isLeftToRightDirection())
        return rowGroupRect.height() - (cell ? cell->y() + cell->height() : 0_lu) + outerBorderBottom(&style());
    return cell ? rowGroupRect.height() - (cell->y() - cell->height()) : 0_lu;
}

LayoutUnit RenderTableSection::horizontalRowGroupBorderWidth(RenderTableCell* cell, const LayoutRect& rowGroupRect, unsigned row, unsigned column)
{
    if (style().isHorizontalWritingMode()) {
        if (style().isLeftToRightDirection())
            return rowGroupRect.width() - (cell ? cell->x() + cell->width() : 0_lu) + (!column ? outerBorderLeft(&style()) : column == table()->numEffCols() ? outerBorderRight(&style()) : 0_lu);
        return cell ? rowGroupRect.width() - (cell->x() - cell->width()) : 0_lu;
    }
    bool isLastRow = row + 1 == m_grid.size();
    return m_rowPos[row + 1] - m_rowPos[row] + (isLastRow ? outerBorderLeft(&style()) : !row ? outerBorderRight(&style()) : 0_lu);
}

void RenderTableSection::paintRowGroupBorderIfRequired(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, unsigned row, unsigned column, BoxSide borderSide, RenderTableCell* cell)
{
    if (table()->currentBorderValue()->precedence() > BorderPrecedence::RowGroup)
        return;
    if (paintInfo.context().paintingDisabled())
        return;

    const RenderStyle& style = this->style();
    bool antialias = shouldAntialiasLines(paintInfo.context());
    LayoutRect rowGroupRect = LayoutRect(paintOffset, size());
    rowGroupRect.moveBy(-LayoutPoint(outerBorderLeft(&style), (borderSide == BSRight) ? 0_lu : outerBorderTop(&style)));

    switch (borderSide) {
    case BSTop:
        paintRowGroupBorder(paintInfo, antialias, LayoutRect(paintOffset.x() + offsetLeftForRowGroupBorder(cell, rowGroupRect, row), rowGroupRect.y(), 
            horizontalRowGroupBorderWidth(cell, rowGroupRect, row, column), style.borderTop().width()), BSTop, CSSPropertyBorderTopColor, style.borderTopStyle(), table()->style().borderTopStyle());
        break;
    case BSBottom:
        paintRowGroupBorder(paintInfo, antialias, LayoutRect(paintOffset.x() + offsetLeftForRowGroupBorder(cell, rowGroupRect, row), rowGroupRect.y() + rowGroupRect.height(), 
            horizontalRowGroupBorderWidth(cell, rowGroupRect, row, column), style.borderBottom().width()), BSBottom, CSSPropertyBorderBottomColor, style.borderBottomStyle(), table()->style().borderBottomStyle());
        break;
    case BSLeft:
        paintRowGroupBorder(paintInfo, antialias, LayoutRect(rowGroupRect.x(), rowGroupRect.y() + offsetTopForRowGroupBorder(cell, borderSide, row), style.borderLeft().width(),
            verticalRowGroupBorderHeight(cell, rowGroupRect, row)), BSLeft, CSSPropertyBorderLeftColor, style.borderLeftStyle(), table()->style().borderLeftStyle());
        break;
    case BSRight:
        paintRowGroupBorder(paintInfo, antialias, LayoutRect(rowGroupRect.x() + rowGroupRect.width(), rowGroupRect.y() + offsetTopForRowGroupBorder(cell, borderSide, row), style.borderRight().width(),
            verticalRowGroupBorderHeight(cell, rowGroupRect, row)), BSRight, CSSPropertyBorderRightColor, style.borderRightStyle(), table()->style().borderRightStyle());
        break;
    default:
        break;
    }

}

static BoxSide physicalBorderForDirection(const RenderStyle* styleForCellFlow, CollapsedBorderSide side)
{

    switch (side) {
    case CBSStart:
        if (styleForCellFlow->isHorizontalWritingMode())
            return styleForCellFlow->isLeftToRightDirection() ? BSLeft : BSRight;
        return styleForCellFlow->isLeftToRightDirection() ? BSTop : BSBottom;
    case CBSEnd:
        if (styleForCellFlow->isHorizontalWritingMode())
            return styleForCellFlow->isLeftToRightDirection() ? BSRight : BSLeft;
        return styleForCellFlow->isLeftToRightDirection() ? BSBottom : BSTop;
    case CBSBefore:
        if (styleForCellFlow->isHorizontalWritingMode())
            return BSTop;
        return styleForCellFlow->isLeftToRightDirection() ? BSRight : BSLeft;
    case CBSAfter:
        if (styleForCellFlow->isHorizontalWritingMode())
            return BSBottom;
        return styleForCellFlow->isLeftToRightDirection() ? BSLeft : BSRight;
    default:
        ASSERT_NOT_REACHED();
        return BSLeft;
    }
}

void RenderTableSection::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutRect localRepaintRect = paintInfo.rect;
    localRepaintRect.moveBy(-paintOffset);

    LayoutRect tableAlignedRect = logicalRectForWritingModeAndDirection(localRepaintRect);

    CellSpan dirtiedRows = this->dirtiedRows(tableAlignedRect);
    CellSpan dirtiedColumns = this->dirtiedColumns(tableAlignedRect);

    if (dirtiedColumns.start < dirtiedColumns.end) {
        if (!m_hasMultipleCellLevels && !m_overflowingCells.size()) {
            if (paintInfo.phase == PaintPhase::CollapsedTableBorders) {
                // Collapsed borders are painted from the bottom right to the top left so that precedence
                // due to cell position is respected. We need to paint one row beyond the topmost dirtied
                // row to calculate its collapsed border value.
                unsigned startRow = dirtiedRows.start ? dirtiedRows.start - 1 : 0;
                for (unsigned r = dirtiedRows.end; r > startRow; r--) {
                    unsigned row = r - 1;
                    bool shouldPaintRowGroupBorder = false;
                    for (unsigned c = dirtiedColumns.end; c > dirtiedColumns.start; c--) {
                        unsigned col = c - 1;
                        CellStruct& current = cellAt(row, col);
                        RenderTableCell* cell = current.primaryCell();
                        if (!cell) {
                            if (!c)
                                paintRowGroupBorderIfRequired(paintInfo, paintOffset, row, col, physicalBorderForDirection(&style(), CBSStart));
                            else if (c == table()->numEffCols())
                                paintRowGroupBorderIfRequired(paintInfo, paintOffset, row, col, physicalBorderForDirection(&style(), CBSEnd));
                            shouldPaintRowGroupBorder = true;
                            continue;
                        }
                        if ((row > dirtiedRows.start && primaryCellAt(row - 1, col) == cell) || (col > dirtiedColumns.start && primaryCellAt(row, col - 1) == cell))
                            continue;
                        
                        // If we had a run of null cells paint their corresponding section of the row group's border if necessary. Note that
                        // this will only happen once within a row as the null cells will always be clustered together on one end of the row.
                        if (shouldPaintRowGroupBorder) {
                            if (r == m_grid.size())
                                paintRowGroupBorderIfRequired(paintInfo, paintOffset, row, col, physicalBorderForDirection(&style(), CBSAfter), cell);
                            else if (!row && !table()->sectionAbove(this))
                                paintRowGroupBorderIfRequired(paintInfo, paintOffset, row, col, physicalBorderForDirection(&style(), CBSBefore), cell);
                            shouldPaintRowGroupBorder = false;
                        }

                        LayoutPoint cellPoint = flipForWritingModeForChild(cell, paintOffset);
                        cell->paintCollapsedBorders(paintInfo, cellPoint);
                    }
                }
            } else {
                // Draw the dirty cells in the order that they appear.
                for (unsigned r = dirtiedRows.start; r < dirtiedRows.end; r++) {
                    RenderTableRow* row = m_grid[r].rowRenderer;
                    if (row && !row->hasSelfPaintingLayer())
                        row->paintOutlineForRowIfNeeded(paintInfo, paintOffset);
                    for (unsigned c = dirtiedColumns.start; c < dirtiedColumns.end; c++) {
                        CellStruct& current = cellAt(r, c);
                        RenderTableCell* cell = current.primaryCell();
                        if (!cell || (r > dirtiedRows.start && primaryCellAt(r - 1, c) == cell) || (c > dirtiedColumns.start && primaryCellAt(r, c - 1) == cell))
                            continue;
                        paintCell(cell, paintInfo, paintOffset);
                    }
                }
            }
        } else {
            // The overflowing cells should be scarce to avoid adding a lot of cells to the HashSet.
#ifndef NDEBUG
            unsigned totalRows = m_grid.size();
            unsigned totalCols = table()->columns().size();
            ASSERT(m_overflowingCells.size() < totalRows * totalCols * gMaxAllowedOverflowingCellRatioForFastPaintPath);
#endif

            // To make sure we properly repaint the section, we repaint all the overflowing cells that we collected.
            auto cells = copyToVector(m_overflowingCells);

            HashSet<RenderTableCell*> spanningCells;

            for (unsigned r = dirtiedRows.start; r < dirtiedRows.end; r++) {
                RenderTableRow* row = m_grid[r].rowRenderer;
                if (row && !row->hasSelfPaintingLayer())
                    row->paintOutlineForRowIfNeeded(paintInfo, paintOffset);
                for (unsigned c = dirtiedColumns.start; c < dirtiedColumns.end; c++) {
                    CellStruct& current = cellAt(r, c);
                    if (!current.hasCells())
                        continue;
                    for (unsigned i = 0; i < current.cells.size(); ++i) {
                        if (m_overflowingCells.contains(current.cells[i]))
                            continue;

                        if (current.cells[i]->rowSpan() > 1 || current.cells[i]->colSpan() > 1) {
                            if (!spanningCells.add(current.cells[i]).isNewEntry)
                                continue;
                        }

                        cells.append(current.cells[i]);
                    }
                }
            }

            // Sort the dirty cells by paint order.
            if (!m_overflowingCells.size())
                std::stable_sort(cells.begin(), cells.end(), compareCellPositions);
            else
                std::sort(cells.begin(), cells.end(), compareCellPositionsWithOverflowingCells);

            if (paintInfo.phase == PaintPhase::CollapsedTableBorders) {
                for (unsigned i = cells.size(); i > 0; --i) {
                    LayoutPoint cellPoint = flipForWritingModeForChild(cells[i - 1], paintOffset);
                    cells[i - 1]->paintCollapsedBorders(paintInfo, cellPoint);
                }
            } else {
                for (unsigned i = 0; i < cells.size(); ++i)
                    paintCell(cells[i], paintInfo, paintOffset);
            }
        }
    }
}

void RenderTableSection::imageChanged(WrappedImagePtr, const IntRect*)
{
    // FIXME: Examine cells and repaint only the rect the image paints in.
    repaint();
}

void RenderTableSection::recalcCells()
{
    ASSERT(m_needsCellRecalc);
    // We reset the flag here to ensure that addCell() works. This is safe to do because we clear the grid
    // and update its dimensions to be consistent with the table's column representation before we rebuild
    // the grid using addCell().
    m_needsCellRecalc = false;

    m_cCol = 0;
    m_cRow = 0;
    m_grid.clear();

    for (RenderTableRow* row = firstRow(); row; row = row->nextRow()) {
        unsigned insertionRow = m_cRow;
        m_cRow++;
        m_cCol = 0;
        ensureRows(m_cRow);

        m_grid[insertionRow].rowRenderer = row;
        row->setRowIndex(insertionRow);
        setRowLogicalHeightToRowStyleLogicalHeightIfNotRelative(m_grid[insertionRow]);

        for (RenderTableCell* cell = row->firstCell(); cell; cell = cell->nextCell())
            addCell(cell, row);
    }

    m_grid.shrinkToFit();
    setNeedsLayout();
}

void RenderTableSection::removeRedundantColumns()
{
    auto maximumNumberOfColumns = table()->numEffCols();
    for (auto& rowItem : m_grid) {
        if (rowItem.row.size() <= maximumNumberOfColumns)
            continue;
        rowItem.row.resize(maximumNumberOfColumns);
    }
}

// FIXME: This function could be made O(1) in certain cases (like for the non-most-constrainive cells' case).
void RenderTableSection::rowLogicalHeightChanged(unsigned rowIndex)
{
    if (needsCellRecalc())
        return;

    setRowLogicalHeightToRowStyleLogicalHeightIfNotRelative(m_grid[rowIndex]);

    for (RenderTableCell* cell = m_grid[rowIndex].rowRenderer->firstCell(); cell; cell = cell->nextCell())
        updateLogicalHeightForCell(m_grid[rowIndex], cell);
}

void RenderTableSection::setNeedsCellRecalc()
{
    m_needsCellRecalc = true;

    // Clear the grid now to ensure that we don't hold onto any stale pointers (e.g. a cell renderer that is being removed).
    m_grid.clear();

    if (RenderTable* t = table())
        t->setNeedsSectionRecalc();
}

unsigned RenderTableSection::numColumns() const
{
    ASSERT(!m_needsCellRecalc);
    unsigned result = 0;
    
    for (unsigned r = 0; r < m_grid.size(); ++r) {
        for (unsigned c = result; c < table()->numEffCols(); ++c) {
            const CellStruct& cell = cellAt(r, c);
            if (cell.hasCells() || cell.inColSpan)
                result = c;
        }
    }
    
    return result + 1;
}

const BorderValue& RenderTableSection::borderAdjoiningStartCell(const RenderTableCell& cell) const
{
    ASSERT(cell.isFirstOrLastCellInRow());
    return isDirectionSame(this, &cell) ? style().borderStart() : style().borderEnd();
}

const BorderValue& RenderTableSection::borderAdjoiningEndCell(const RenderTableCell& cell) const
{
    ASSERT(cell.isFirstOrLastCellInRow());
    return isDirectionSame(this, &cell) ? style().borderEnd() : style().borderStart();
}

const RenderTableCell* RenderTableSection::firstRowCellAdjoiningTableStart() const
{
    unsigned adjoiningStartCellColumnIndex = isDirectionSame(this, table()) ? 0 : table()->lastColumnIndex();
    return cellAt(0, adjoiningStartCellColumnIndex).primaryCell();
}

const RenderTableCell* RenderTableSection::firstRowCellAdjoiningTableEnd() const
{
    unsigned adjoiningEndCellColumnIndex = isDirectionSame(this, table()) ? table()->lastColumnIndex() : 0;
    return cellAt(0, adjoiningEndCellColumnIndex).primaryCell();
}

void RenderTableSection::appendColumn(unsigned pos)
{
    ASSERT(!m_needsCellRecalc);

    for (unsigned row = 0; row < m_grid.size(); ++row)
        m_grid[row].row.resize(pos + 1);
}

void RenderTableSection::splitColumn(unsigned pos, unsigned first)
{
    ASSERT(!m_needsCellRecalc);

    if (m_cCol > pos)
        m_cCol++;
    for (unsigned row = 0; row < m_grid.size(); ++row) {
        Row& r = m_grid[row].row;
        r.insert(pos + 1, CellStruct());
        if (r[pos].hasCells()) {
            r[pos + 1].cells.appendVector(r[pos].cells);
            RenderTableCell* cell = r[pos].primaryCell();
            ASSERT(cell);
            ASSERT(cell->colSpan() >= (r[pos].inColSpan ? 1u : 0));
            unsigned colleft = cell->colSpan() - r[pos].inColSpan;
            if (first > colleft)
              r[pos + 1].inColSpan = 0;
            else
              r[pos + 1].inColSpan = first + r[pos].inColSpan;
        } else {
            r[pos + 1].inColSpan = 0;
        }
    }
}

// Hit Testing
bool RenderTableSection::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    // If we have no children then we have nothing to do.
    if (!firstRow())
        return false;

    // Table sections cannot ever be hit tested.  Effectively they do not exist.
    // Just forward to our children always.
    LayoutPoint adjustedLocation = accumulatedOffset + location();

    if (hasOverflowClip() && !locationInContainer.intersects(overflowClipRect(adjustedLocation, nullptr)))
        return false;

    if (hasOverflowingCell()) {
        for (RenderTableRow* row = lastRow(); row; row = row->previousRow()) {
            // FIXME: We have to skip over inline flows, since they can show up inside table rows
            // at the moment (a demoted inline <form> for example). If we ever implement a
            // table-specific hit-test method (which we should do for performance reasons anyway),
            // then we can remove this check.
            if (!row->hasSelfPaintingLayer()) {
                LayoutPoint childPoint = flipForWritingModeForChild(row, adjustedLocation);
                if (row->nodeAtPoint(request, result, locationInContainer, childPoint, action)) {
                    updateHitTestResult(result, toLayoutPoint(locationInContainer.point() - childPoint));
                    return true;
                }
            }
        }
        return false;
    }

    recalcCellsIfNeeded();

    LayoutRect hitTestRect = locationInContainer.boundingBox();
    hitTestRect.moveBy(-adjustedLocation);

    LayoutRect tableAlignedRect = logicalRectForWritingModeAndDirection(hitTestRect);
    CellSpan rowSpan = spannedRows(tableAlignedRect, DoNotIncludeAllIntersectingCells);
    CellSpan columnSpan = spannedColumns(tableAlignedRect, DoNotIncludeAllIntersectingCells);

    // Now iterate over the spanned rows and columns.
    for (unsigned hitRow = rowSpan.start; hitRow < rowSpan.end; ++hitRow) {
        for (unsigned hitColumn = columnSpan.start; hitColumn < columnSpan.end; ++hitColumn) {
            CellStruct& current = cellAt(hitRow, hitColumn);

            // If the cell is empty, there's nothing to do
            if (!current.hasCells())
                continue;

            for (unsigned i = current.cells.size() ; i; ) {
                --i;
                RenderTableCell* cell = current.cells[i];
                LayoutPoint cellPoint = flipForWritingModeForChild(cell, adjustedLocation);
                if (static_cast<RenderObject*>(cell)->nodeAtPoint(request, result, locationInContainer, cellPoint, action)) {
                    updateHitTestResult(result, locationInContainer.point() - toLayoutSize(cellPoint));
                    return true;
                }
            }
            if (!request.resultIsElementList())
                break;
        }
        if (!request.resultIsElementList())
            break;
    }

    return false;
}

void RenderTableSection::clearCachedCollapsedBorders()
{
    if (!table()->collapseBorders())
        return;
    m_cellsCollapsedBorders.clear();
}

void RenderTableSection::removeCachedCollapsedBorders(const RenderTableCell& cell)
{
    if (!table()->collapseBorders())
        return;
    
    for (int side = CBSBefore; side <= CBSEnd; ++side)
        m_cellsCollapsedBorders.remove(std::make_pair(&cell, side));
}

void RenderTableSection::setCachedCollapsedBorder(const RenderTableCell& cell, CollapsedBorderSide side, CollapsedBorderValue border)
{
    ASSERT(table()->collapseBorders());
    ASSERT(border.width());
    m_cellsCollapsedBorders.set(std::make_pair(&cell, side), border);
}

CollapsedBorderValue RenderTableSection::cachedCollapsedBorder(const RenderTableCell& cell, CollapsedBorderSide side)
{
    ASSERT(table()->collapseBorders() && table()->collapsedBordersAreValid());
    auto it = m_cellsCollapsedBorders.find(std::make_pair(&cell, side));
    // Only non-empty collapsed borders are in the hashmap.
    if (it == m_cellsCollapsedBorders.end())
        return CollapsedBorderValue(BorderValue(), Color(), BorderPrecedence::Cell);
    return it->value;
}

RenderPtr<RenderTableSection> RenderTableSection::createTableSectionWithStyle(Document& document, const RenderStyle& style)
{
    auto section = createRenderer<RenderTableSection>(document, RenderStyle::createAnonymousStyleWithDisplay(style, DisplayType::TableRowGroup));
    section->initializeStyle();
    return section;
}

RenderPtr<RenderTableSection> RenderTableSection::createAnonymousWithParentRenderer(const RenderTable& parent)
{
    return RenderTableSection::createTableSectionWithStyle(parent.document(), parent.style());
}

void RenderTableSection::setLogicalPositionForCell(RenderTableCell* cell, unsigned effectiveColumn) const
{
    LayoutPoint oldCellLocation = cell->location();

    LayoutPoint cellLocation(0_lu, m_rowPos[cell->rowIndex()]);
    LayoutUnit horizontalBorderSpacing = table()->hBorderSpacing();

    // FIXME: The table's direction should determine our row's direction, not the section's (see bug 96691).
    if (!style().isLeftToRightDirection())
        cellLocation.setX(table()->columnPositions()[table()->numEffCols()] - table()->columnPositions()[table()->colToEffCol(cell->col() + cell->colSpan())] + horizontalBorderSpacing);
    else
        cellLocation.setX(table()->columnPositions()[effectiveColumn] + horizontalBorderSpacing);

    cell->setLogicalLocation(cellLocation);
    view().frameView().layoutContext().addLayoutDelta(oldCellLocation - cell->location());
}

} // namespace WebCore
