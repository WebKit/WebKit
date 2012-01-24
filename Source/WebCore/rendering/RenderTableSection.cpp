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
#include "CachedImage.h"
#include "Document.h"
#include "HitTestResult.h"
#include "HTMLNames.h"
#include "PaintInfo.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableRow.h"
#include "RenderView.h"
#include <limits>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

// Those 2 variables are used to balance the memory consumption vs the repaint time on big tables.
static unsigned gMinTableSizeToUseFastPaintPathWithOverflowingCell = 75 * 75;
static float gMaxAllowedOverflowingCellRatioForFastPaintPath = 0.1f;

static inline void setRowLogicalHeightToRowStyleLogicalHeightIfNotRelative(RenderTableSection::RowStruct& row)
{
    ASSERT(row.rowRenderer);
    row.logicalHeight = row.rowRenderer->style()->logicalHeight();
    if (row.logicalHeight.isRelative())
        row.logicalHeight = Length();
}

static inline void updateLogicalHeightForCell(RenderTableSection::RowStruct& row, const RenderTableCell* cell)
{
    // We ignore height settings on rowspan cells.
    if (cell->rowSpan() != 1)
        return;

    Length logicalHeight = cell->style()->logicalHeight();
    if (logicalHeight.isPositive() || (logicalHeight.isRelative() && logicalHeight.value() >= 0)) {
        Length cRowLogicalHeight = row.logicalHeight;
        switch (logicalHeight.type()) {
        case Percent:
            if (!(cRowLogicalHeight.isPercent())
                || (cRowLogicalHeight.isPercent() && cRowLogicalHeight.percent() < logicalHeight.percent()))
                row.logicalHeight = logicalHeight;
            break;
        case Fixed:
            if (cRowLogicalHeight.type() < Percent
                || (cRowLogicalHeight.isFixed() && cRowLogicalHeight.value() < logicalHeight.value()))
                row.logicalHeight = logicalHeight;
            break;
        case Relative:
        default:
            break;
        }
    }
}


RenderTableSection::RenderTableSection(Node* node)
    : RenderBox(node)
    , m_cCol(0)
    , m_cRow(0)
    , m_outerBorderStart(0)
    , m_outerBorderEnd(0)
    , m_outerBorderBefore(0)
    , m_outerBorderAfter(0)
    , m_needsCellRecalc(false)
    , m_hasMultipleCellLevels(false)
{
    // init RenderObject attributes
    setInline(false); // our object is not Inline
}

RenderTableSection::~RenderTableSection()
{
}

void RenderTableSection::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBox::styleDidChange(diff, oldStyle);
    propagateStyleToAnonymousChildren();

    // If border was changed, notify table.
    RenderTable* table = this->table();
    if (table && !table->selfNeedsLayout() && !table->normalChildNeedsLayout() && oldStyle && oldStyle->border() != style()->border())
        table->invalidateCollapsedBorders();
}

void RenderTableSection::willBeDestroyed()
{
    RenderTable* recalcTable = table();
    
    RenderBox::willBeDestroyed();
    
    // recalc cell info because RenderTable has unguarded pointers
    // stored that point to this RenderTableSection.
    if (recalcTable)
        recalcTable->setNeedsSectionRecalc();
}

void RenderTableSection::addChild(RenderObject* child, RenderObject* beforeChild)
{
    // Make sure we don't append things after :after-generated content if we have it.
    if (!beforeChild)
        beforeChild = afterPseudoElementRenderer();

    if (!child->isTableRow()) {
        RenderObject* last = beforeChild;
        if (!last)
            last = lastChild();
        if (last && last->isAnonymous() && !last->isBeforeOrAfterContent()) {
            if (beforeChild == last)
                beforeChild = last->firstChild();
            last->addChild(child, beforeChild);
            return;
        }

        if (beforeChild && !beforeChild->isAnonymous() && beforeChild->parent() == this) {
            RenderObject* row = beforeChild->previousSibling();
            if (row && row->isTableRow() && row->isAnonymous()) {
                row->addChild(child);
                return;
            }
        }

        // If beforeChild is inside an anonymous cell/row, insert into the cell or into
        // the anonymous row containing it, if there is one.
        RenderObject* lastBox = last;
        while (lastBox && lastBox->parent()->isAnonymous() && !lastBox->isTableRow())
            lastBox = lastBox->parent();
        if (lastBox && lastBox->isAnonymous() && !lastBox->isBeforeOrAfterContent()) {
            lastBox->addChild(child, beforeChild);
            return;
        }

        RenderObject* row = new (renderArena()) RenderTableRow(document() /* anonymous table row */);
        RefPtr<RenderStyle> newStyle = RenderStyle::create();
        newStyle->inheritFrom(style());
        newStyle->setDisplay(TABLE_ROW);
        row->setStyle(newStyle.release());
        addChild(row, beforeChild);
        row->addChild(child);
        return;
    }

    if (beforeChild)
        setNeedsCellRecalc();

    unsigned insertionRow = m_cRow;
    ++m_cRow;
    m_cCol = 0;

    ensureRows(m_cRow);

    m_grid[insertionRow].rowRenderer = toRenderTableRow(child);

    if (!beforeChild)
        setRowLogicalHeightToRowStyleLogicalHeightIfNotRelative(m_grid[insertionRow]);

    // If the next renderer is actually wrapped in an anonymous table row, we need to go up and find that.
    while (beforeChild && beforeChild->parent() != this)
        beforeChild = beforeChild->parent();

    ASSERT(!beforeChild || beforeChild->isTableRow());
    RenderBox::addChild(child, beforeChild);
    toRenderTableRow(child)->updateBeforeAndAfterContent();
}

void RenderTableSection::removeChild(RenderObject* oldChild)
{
    setNeedsCellRecalc();
    RenderBox::removeChild(oldChild);
}

void RenderTableSection::ensureRows(unsigned numRows)
{
    if (numRows <= m_grid.size())
        return;

    unsigned oldSize = m_grid.size();
    m_grid.grow(numRows);

    unsigned effectiveColumnCount = max(1u, table()->numEffCols());
    for (unsigned row = oldSize; row < m_grid.size(); ++row)
        m_grid[row].row.grow(effectiveColumnCount);
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
    Vector<RenderTable::ColumnStruct>& columns = table()->columns();
    unsigned nCols = columns.size();
    // addCell should be called only after m_cRow has been incremented.
    ASSERT(m_cRow);
    unsigned insertionRow = m_cRow - 1;

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
    cell->setRow(insertionRow);
    cell->setCol(table()->effColToCol(col));
}

void RenderTableSection::setCellLogicalWidths()
{
    Vector<LayoutUnit>& columnPos = table()->columnPositions();

    LayoutStateMaintainer statePusher(view());

    for (unsigned i = 0; i < m_grid.size(); i++) {
        Row& row = m_grid[i].row;
        unsigned cols = row.size();
        for (unsigned j = 0; j < cols; j++) {
            CellStruct& current = row[j];
            RenderTableCell* cell = current.primaryCell();
            if (!cell || current.inColSpan)
              continue;
            unsigned endCol = j;
            unsigned cspan = cell->colSpan();
            while (cspan && endCol < cols) {
                ASSERT(endCol < table()->columns().size());
                cspan -= table()->columns()[endCol].span;
                endCol++;
            }
            LayoutUnit w = columnPos[endCol] - columnPos[j] - table()->hBorderSpacing();
            LayoutUnit oldLogicalWidth = cell->logicalWidth();
            if (w != oldLogicalWidth) {
                cell->setNeedsLayout(true);
                if (!table()->selfNeedsLayout() && cell->checkForRepaintDuringLayout()) {
                    if (!statePusher.didPush()) {
                        // Technically, we should also push state for the row, but since
                        // rows don't push a coordinate transform, that's not necessary.
                        statePusher.push(this, LayoutSize(x(), y()));
                    }
                    cell->repaint();
                }
                cell->updateLogicalWidth(w);
            }
        }
    }
    
    statePusher.pop(); // only pops if we pushed
}

LayoutUnit RenderTableSection::calcRowLogicalHeight()
{
#ifndef NDEBUG
    setNeedsLayoutIsForbidden(true);
#endif

    ASSERT(!needsLayout());

    RenderTableCell* cell;

    LayoutUnit spacing = table()->vBorderSpacing();

    LayoutStateMaintainer statePusher(view());

    m_rowPos.resize(m_grid.size() + 1);
    m_rowPos[0] = spacing;

    for (unsigned r = 0; r < m_grid.size(); r++) {
        m_rowPos[r + 1] = 0;
        m_grid[r].baseline = 0;
        LayoutUnit baseline = 0;
        LayoutUnit bdesc = 0;
        LayoutUnit ch = m_grid[r].logicalHeight.calcMinValue(0);
        LayoutUnit pos = m_rowPos[r] + ch + (m_grid[r].rowRenderer ? spacing : 0);

        m_rowPos[r + 1] = max(m_rowPos[r + 1], pos);

        Row& row = m_grid[r].row;
        unsigned totalCols = row.size();

        for (unsigned c = 0; c < totalCols; c++) {
            CellStruct& current = cellAt(r, c);
            cell = current.primaryCell();

            if (!cell || current.inColSpan)
                continue;

            if ((cell->row() + cell->rowSpan() - 1) > r)
                continue;

            unsigned indx = max(r - cell->rowSpan() + 1, 0u);

            if (cell->hasOverrideHeight()) {
                if (!statePusher.didPush()) {
                    // Technically, we should also push state for the row, but since
                    // rows don't push a coordinate transform, that's not necessary.
                    statePusher.push(this, locationOffset());
                }
                cell->clearIntrinsicPadding();
                cell->clearOverrideSize();
                cell->setChildNeedsLayout(true, false);
                cell->layoutIfNeeded();
            }

            LayoutUnit adjustedLogicalHeight = cell->logicalHeight() - (cell->intrinsicPaddingBefore() + cell->intrinsicPaddingAfter());

            ch = cell->style()->logicalHeight().calcValue(0);
            if (document()->inQuirksMode() || cell->style()->boxSizing() == BORDER_BOX) {
                // Explicit heights use the border box in quirks mode.
                // Don't adjust height.
            } else {
                // In strict mode, box-sizing: content-box do the right
                // thing and actually add in the border and padding.
                LayoutUnit adjustedPaddingBefore = cell->paddingBefore() - cell->intrinsicPaddingBefore();
                LayoutUnit adjustedPaddingAfter = cell->paddingAfter() - cell->intrinsicPaddingAfter();
                ch += adjustedPaddingBefore + adjustedPaddingAfter + cell->borderBefore() + cell->borderAfter();
            }
            ch = max(ch, adjustedLogicalHeight);

            pos = m_rowPos[indx] + ch + (m_grid[r].rowRenderer ? spacing : 0);

            m_rowPos[r + 1] = max(m_rowPos[r + 1], pos);

            // find out the baseline
            EVerticalAlign va = cell->style()->verticalAlign();
            if (va == BASELINE || va == TEXT_BOTTOM || va == TEXT_TOP || va == SUPER || va == SUB) {
                LayoutUnit b = cell->cellBaselinePosition();
                if (b > cell->borderBefore() + cell->paddingBefore()) {
                    baseline = max(baseline, b - cell->intrinsicPaddingBefore());
                    bdesc = max(bdesc, m_rowPos[indx] + ch - (b - cell->intrinsicPaddingBefore()));
                }
            }
        }

        // do we have baseline aligned elements?
        if (baseline) {
            // increase rowheight if baseline requires
            m_rowPos[r + 1] = max(m_rowPos[r + 1], baseline + bdesc + (m_grid[r].rowRenderer ? spacing : 0));
            m_grid[r].baseline = baseline;
        }

        m_rowPos[r + 1] = max(m_rowPos[r + 1], m_rowPos[r]);
    }

#ifndef NDEBUG
    setNeedsLayoutIsForbidden(false);
#endif

    ASSERT(!needsLayout());

    statePusher.pop();

    return m_rowPos[m_grid.size()];
}

void RenderTableSection::layout()
{
    ASSERT(needsLayout());

    LayoutStateMaintainer statePusher(view(), this, locationOffset(), style()->isFlippedBlocksWritingMode());
    for (RenderObject* child = children()->firstChild(); child; child = child->nextSibling()) {
        if (child->isTableRow()) {
            child->layoutIfNeeded();
            ASSERT(!child->needsLayout());
        }
    }
    statePusher.pop();
    setNeedsLayout(false);
}

LayoutUnit RenderTableSection::layoutRows(LayoutUnit toAdd)
{
#ifndef NDEBUG
    setNeedsLayoutIsForbidden(true);
#endif

    ASSERT(!needsLayout());

    LayoutUnit rHeight;
    unsigned rindx;
    unsigned totalRows = m_grid.size();

    // Set the width of our section now.  The rows will also be this width.
    setLogicalWidth(table()->contentLogicalWidth());
    m_overflow.clear();
    m_overflowingCells.clear();
    m_forceSlowPaintPathWithOverflowingCell = false;

    if (toAdd && totalRows && (m_rowPos[totalRows] || !nextSibling())) {
        LayoutUnit totalHeight = m_rowPos[totalRows] + toAdd;

        LayoutUnit dh = toAdd;
        int totalPercent = 0;
        int numAuto = 0;
        for (unsigned r = 0; r < totalRows; r++) {
            if (m_grid[r].logicalHeight.isAuto())
                numAuto++;
            else if (m_grid[r].logicalHeight.isPercent())
                totalPercent += m_grid[r].logicalHeight.percent();
        }
        if (totalPercent) {
            // try to satisfy percent
            LayoutUnit add = 0;
            totalPercent = min(totalPercent, 100);
            LayoutUnit rh = m_rowPos[1] - m_rowPos[0];
            for (unsigned r = 0; r < totalRows; r++) {
                if (totalPercent > 0 && m_grid[r].logicalHeight.isPercent()) {
                    LayoutUnit toAdd = min<LayoutUnit>(dh, (totalHeight * m_grid[r].logicalHeight.percent() / 100) - rh);
                    // If toAdd is negative, then we don't want to shrink the row (this bug
                    // affected Outlook Web Access).
                    toAdd = max<LayoutUnit>(0, toAdd);
                    add += toAdd;
                    dh -= toAdd;
                    totalPercent -= m_grid[r].logicalHeight.percent();
                }
                ASSERT(totalRows >= 1);
                if (r < totalRows - 1)
                    rh = m_rowPos[r + 2] - m_rowPos[r + 1];
                m_rowPos[r + 1] += add;
            }
        }
        if (numAuto) {
            // distribute over variable cols
            LayoutUnit add = 0;
            for (unsigned r = 0; r < totalRows; r++) {
                if (numAuto > 0 && m_grid[r].logicalHeight.isAuto()) {
                    LayoutUnit toAdd = dh / numAuto;
                    add += toAdd;
                    dh -= toAdd;
                    numAuto--;
                }
                m_rowPos[r + 1] += add;
            }
        }
        if (dh > 0 && m_rowPos[totalRows]) {
            // if some left overs, distribute equally.
            LayoutUnit tot = m_rowPos[totalRows];
            LayoutUnit add = 0;
            LayoutUnit prev = m_rowPos[0];
            for (unsigned r = 0; r < totalRows; r++) {
                // weight with the original height
                add += dh * (m_rowPos[r + 1] - prev) / tot;
                prev = m_rowPos[r + 1];
                m_rowPos[r + 1] += add;
            }
        }
    }

    LayoutUnit hspacing = table()->hBorderSpacing();
    LayoutUnit vspacing = table()->vBorderSpacing();
    unsigned nEffCols = table()->numEffCols();

    LayoutStateMaintainer statePusher(view(), this, LayoutSize(x(), y()), style()->isFlippedBlocksWritingMode());

    for (unsigned r = 0; r < totalRows; r++) {
        // Set the row's x/y position and width/height.
        if (RenderTableRow* rowRenderer = m_grid[r].rowRenderer) {
            rowRenderer->setLocation(LayoutPoint(0, m_rowPos[r]));
            rowRenderer->setLogicalWidth(logicalWidth());
            rowRenderer->setLogicalHeight(m_rowPos[r + 1] - m_rowPos[r] - vspacing);
            rowRenderer->updateLayerTransform();
        }

        for (unsigned c = 0; c < nEffCols; c++) {
            CellStruct& cs = cellAt(r, c);
            RenderTableCell* cell = cs.primaryCell();

            if (!cell || cs.inColSpan)
                continue;

            rindx = cell->row();
            rHeight = m_rowPos[rindx + cell->rowSpan()] - m_rowPos[rindx] - vspacing;
            
            // Force percent height children to lay themselves out again.
            // This will cause these children to grow to fill the cell.
            // FIXME: There is still more work to do here to fully match WinIE (should
            // it become necessary to do so).  In quirks mode, WinIE behaves like we
            // do, but it will clip the cells that spill out of the table section.  In
            // strict mode, Mozilla and WinIE both regrow the table to accommodate the
            // new height of the cell (thus letting the percentages cause growth one
            // time only).  We may also not be handling row-spanning cells correctly.
            //
            // Note also the oddity where replaced elements always flex, and yet blocks/tables do
            // not necessarily flex.  WinIE is crazy and inconsistent, and we can't hope to
            // match the behavior perfectly, but we'll continue to refine it as we discover new
            // bugs. :)
            bool cellChildrenFlex = false;
            bool flexAllChildren = cell->style()->logicalHeight().isFixed()
                || (!table()->style()->logicalHeight().isAuto() && rHeight != cell->logicalHeight());

            for (RenderObject* o = cell->firstChild(); o; o = o->nextSibling()) {
                if (!o->isText() && o->style()->logicalHeight().isPercent() && (flexAllChildren || o->isReplaced() || (o->isBox() && toRenderBox(o)->scrollsOverflow()))) {
                    // Tables with no sections do not flex.
                    if (!o->isTable() || toRenderTable(o)->hasSections()) {
                        o->setNeedsLayout(true, false);
                        cellChildrenFlex = true;
                    }
                }
            }

            if (HashSet<RenderBox*>* percentHeightDescendants = cell->percentHeightDescendants()) {
                HashSet<RenderBox*>::iterator end = percentHeightDescendants->end();
                for (HashSet<RenderBox*>::iterator it = percentHeightDescendants->begin(); it != end; ++it) {
                    RenderBox* box = *it;
                    if (!box->isReplaced() && !box->scrollsOverflow() && !flexAllChildren)
                        continue;

                    while (box != cell) {
                        if (box->normalChildNeedsLayout())
                            break;
                        box->setChildNeedsLayout(true, false);
                        box = box->containingBlock();
                        ASSERT(box);
                        if (!box)
                            break;
                    }
                    cellChildrenFlex = true;
                }
            }

            if (cellChildrenFlex) {
                cell->setChildNeedsLayout(true, false);
                // Alignment within a cell is based off the calculated
                // height, which becomes irrelevant once the cell has
                // been resized based off its percentage.
                cell->setOverrideHeightFromRowHeight(rHeight);
                cell->layoutIfNeeded();

                // If the baseline moved, we may have to update the data for our row. Find out the new baseline.
                EVerticalAlign va = cell->style()->verticalAlign();
                if (va == BASELINE || va == TEXT_BOTTOM || va == TEXT_TOP || va == SUPER || va == SUB) {
                    LayoutUnit baseline = cell->cellBaselinePosition();
                    if (baseline > cell->borderBefore() + cell->paddingBefore())
                        m_grid[r].baseline = max(m_grid[r].baseline, baseline);
                }
            }

            LayoutUnit oldIntrinsicPaddingBefore = cell->intrinsicPaddingBefore();
            LayoutUnit oldIntrinsicPaddingAfter = cell->intrinsicPaddingAfter();
            LayoutUnit logicalHeightWithoutIntrinsicPadding = cell->logicalHeight() - oldIntrinsicPaddingBefore - oldIntrinsicPaddingAfter;

            LayoutUnit intrinsicPaddingBefore = 0;
            switch (cell->style()->verticalAlign()) {
                case SUB:
                case SUPER:
                case TEXT_TOP:
                case TEXT_BOTTOM:
                case BASELINE: {
                    LayoutUnit b = cell->cellBaselinePosition();
                    if (b > cell->borderBefore() + cell->paddingBefore())
                        intrinsicPaddingBefore = getBaseline(r) - (b - oldIntrinsicPaddingBefore);
                    break;
                }
                case TOP:
                    break;
                case MIDDLE:
                    intrinsicPaddingBefore = (rHeight - logicalHeightWithoutIntrinsicPadding) / 2;
                    break;
                case BOTTOM:
                    intrinsicPaddingBefore = rHeight - logicalHeightWithoutIntrinsicPadding;
                    break;
                default:
                    break;
            }
            
            LayoutUnit intrinsicPaddingAfter = rHeight - logicalHeightWithoutIntrinsicPadding - intrinsicPaddingBefore;
            cell->setIntrinsicPaddingBefore(intrinsicPaddingBefore);
            cell->setIntrinsicPaddingAfter(intrinsicPaddingAfter);

            LayoutRect oldCellRect(cell->x(), cell->y() , cell->width(), cell->height());

            LayoutPoint cellLocation(0, m_rowPos[rindx]);
            if (!style()->isLeftToRightDirection())
                cellLocation.setX(table()->columnPositions()[nEffCols] - table()->columnPositions()[table()->colToEffCol(cell->col() + cell->colSpan())] + hspacing);
            else
                cellLocation.setX(table()->columnPositions()[c] + hspacing);
            cell->setLogicalLocation(cellLocation);
            view()->addLayoutDelta(oldCellRect.location() - cell->location());

            if (intrinsicPaddingBefore != oldIntrinsicPaddingBefore || intrinsicPaddingAfter != oldIntrinsicPaddingAfter)
                cell->setNeedsLayout(true, false);

            if (!cell->needsLayout() && view()->layoutState()->pageLogicalHeight() && view()->layoutState()->pageLogicalOffset(cell->logicalTop()) != cell->pageLogicalOffset())
                cell->setChildNeedsLayout(true, false);

            cell->layoutIfNeeded();

            // FIXME: Make pagination work with vertical tables.
            if (style()->isHorizontalWritingMode() && view()->layoutState()->pageLogicalHeight() && cell->height() != rHeight)
                cell->setHeight(rHeight); // FIXME: Pagination might have made us change size.  For now just shrink or grow the cell to fit without doing a relayout.

            LayoutSize childOffset(cell->location() - oldCellRect.location());
            if (childOffset.width() || childOffset.height()) {
                view()->addLayoutDelta(childOffset);

                // If the child moved, we have to repaint it as well as any floating/positioned
                // descendants.  An exception is if we need a layout.  In this case, we know we're going to
                // repaint ourselves (and the child) anyway.
                if (!table()->selfNeedsLayout() && cell->checkForRepaintDuringLayout())
                    cell->repaintDuringLayoutIfMoved(oldCellRect);
            }
        }
    }

#ifndef NDEBUG
    setNeedsLayoutIsForbidden(false);
#endif

    ASSERT(!needsLayout());

    setLogicalHeight(m_rowPos[totalRows]);

    unsigned totalCellsCount = nEffCols * totalRows;
    int maxAllowedOverflowingCellsCount = totalCellsCount < gMinTableSizeToUseFastPaintPathWithOverflowingCell ? 0 : gMaxAllowedOverflowingCellRatioForFastPaintPath * totalCellsCount;

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

    statePusher.pop();
    return height();
}

LayoutUnit RenderTableSection::calcOuterBorderBefore() const
{
    unsigned totalCols = table()->numEffCols();
    if (!m_grid.size() || !totalCols)
        return 0;

    unsigned borderWidth = 0;

    const BorderValue& sb = style()->borderBefore();
    if (sb.style() == BHIDDEN)
        return -1;
    if (sb.style() > BHIDDEN)
        borderWidth = sb.width();

    const BorderValue& rb = firstChild()->style()->borderBefore();
    if (rb.style() == BHIDDEN)
        return -1;
    if (rb.style() > BHIDDEN && rb.width() > borderWidth)
        borderWidth = rb.width();

    bool allHidden = true;
    for (unsigned c = 0; c < totalCols; c++) {
        const CellStruct& current = cellAt(0, c);
        if (current.inColSpan || !current.hasCells())
            continue;
        const BorderValue& cb = current.primaryCell()->style()->borderBefore(); // FIXME: Make this work with perpendicular and flipped cells.
        // FIXME: Don't repeat for the same col group
        RenderTableCol* colGroup = table()->colElement(c);
        if (colGroup) {
            const BorderValue& gb = colGroup->style()->borderBefore();
            if (gb.style() == BHIDDEN || cb.style() == BHIDDEN)
                continue;
            allHidden = false;
            if (gb.style() > BHIDDEN && gb.width() > borderWidth)
                borderWidth = gb.width();
            if (cb.style() > BHIDDEN && cb.width() > borderWidth)
                borderWidth = cb.width();
        } else {
            if (cb.style() == BHIDDEN)
                continue;
            allHidden = false;
            if (cb.style() > BHIDDEN && cb.width() > borderWidth)
                borderWidth = cb.width();
        }
    }
    if (allHidden)
        return -1;

    return borderWidth / 2;
}

LayoutUnit RenderTableSection::calcOuterBorderAfter() const
{
    unsigned totalCols = table()->numEffCols();
    if (!m_grid.size() || !totalCols)
        return 0;

    unsigned borderWidth = 0;

    const BorderValue& sb = style()->borderAfter();
    if (sb.style() == BHIDDEN)
        return -1;
    if (sb.style() > BHIDDEN)
        borderWidth = sb.width();

    const BorderValue& rb = lastChild()->style()->borderAfter();
    if (rb.style() == BHIDDEN)
        return -1;
    if (rb.style() > BHIDDEN && rb.width() > borderWidth)
        borderWidth = rb.width();

    bool allHidden = true;
    for (unsigned c = 0; c < totalCols; c++) {
        const CellStruct& current = cellAt(m_grid.size() - 1, c);
        if (current.inColSpan || !current.hasCells())
            continue;
        const BorderValue& cb = current.primaryCell()->style()->borderAfter(); // FIXME: Make this work with perpendicular and flipped cells.
        // FIXME: Don't repeat for the same col group
        RenderTableCol* colGroup = table()->colElement(c);
        if (colGroup) {
            const BorderValue& gb = colGroup->style()->borderAfter();
            if (gb.style() == BHIDDEN || cb.style() == BHIDDEN)
                continue;
            allHidden = false;
            if (gb.style() > BHIDDEN && gb.width() > borderWidth)
                borderWidth = gb.width();
            if (cb.style() > BHIDDEN && cb.width() > borderWidth)
                borderWidth = cb.width();
        } else {
            if (cb.style() == BHIDDEN)
                continue;
            allHidden = false;
            if (cb.style() > BHIDDEN && cb.width() > borderWidth)
                borderWidth = cb.width();
        }
    }
    if (allHidden)
        return -1;

    return (borderWidth + 1) / 2;
}

LayoutUnit RenderTableSection::calcOuterBorderStart() const
{
    unsigned totalCols = table()->numEffCols();
    if (!m_grid.size() || !totalCols)
        return 0;

    unsigned borderWidth = 0;

    const BorderValue& sb = style()->borderStart();
    if (sb.style() == BHIDDEN)
        return -1;
    if (sb.style() > BHIDDEN)
        borderWidth = sb.width();

    if (RenderTableCol* colGroup = table()->colElement(0)) {
        const BorderValue& gb = colGroup->style()->borderStart();
        if (gb.style() == BHIDDEN)
            return -1;
        if (gb.style() > BHIDDEN && gb.width() > borderWidth)
            borderWidth = gb.width();
    }

    bool allHidden = true;
    for (unsigned r = 0; r < m_grid.size(); r++) {
        const CellStruct& current = cellAt(r, 0);
        if (!current.hasCells())
            continue;
        // FIXME: Don't repeat for the same cell
        const BorderValue& cb = current.primaryCell()->style()->borderStart(); // FIXME: Make this work with perpendicular and flipped cells.
        const BorderValue& rb = current.primaryCell()->parent()->style()->borderStart();
        if (cb.style() == BHIDDEN || rb.style() == BHIDDEN)
            continue;
        allHidden = false;
        if (cb.style() > BHIDDEN && cb.width() > borderWidth)
            borderWidth = cb.width();
        if (rb.style() > BHIDDEN && rb.width() > borderWidth)
            borderWidth = rb.width();
    }
    if (allHidden)
        return -1;

    return (borderWidth + (table()->style()->isLeftToRightDirection() ? 0 : 1)) / 2;
}

LayoutUnit RenderTableSection::calcOuterBorderEnd() const
{
    unsigned totalCols = table()->numEffCols();
    if (!m_grid.size() || !totalCols)
        return 0;

    unsigned borderWidth = 0;

    const BorderValue& sb = style()->borderEnd();
    if (sb.style() == BHIDDEN)
        return -1;
    if (sb.style() > BHIDDEN)
        borderWidth = sb.width();

    if (RenderTableCol* colGroup = table()->colElement(totalCols - 1)) {
        const BorderValue& gb = colGroup->style()->borderEnd();
        if (gb.style() == BHIDDEN)
            return -1;
        if (gb.style() > BHIDDEN && gb.width() > borderWidth)
            borderWidth = gb.width();
    }

    bool allHidden = true;
    for (unsigned r = 0; r < m_grid.size(); r++) {
        const CellStruct& current = cellAt(r, totalCols - 1);
        if (!current.hasCells())
            continue;
        // FIXME: Don't repeat for the same cell
        const BorderValue& cb = current.primaryCell()->style()->borderEnd(); // FIXME: Make this work with perpendicular and flipped cells.
        const BorderValue& rb = current.primaryCell()->parent()->style()->borderEnd();
        if (cb.style() == BHIDDEN || rb.style() == BHIDDEN)
            continue;
        allHidden = false;
        if (cb.style() > BHIDDEN && cb.width() > borderWidth)
            borderWidth = cb.width();
        if (rb.style() > BHIDDEN && rb.width() > borderWidth)
            borderWidth = rb.width();
    }
    if (allHidden)
        return -1;

    return (borderWidth + (table()->style()->isLeftToRightDirection() ? 1 : 0)) / 2;
}

void RenderTableSection::recalcOuterBorder()
{
    m_outerBorderBefore = calcOuterBorderBefore();
    m_outerBorderAfter = calcOuterBorderAfter();
    m_outerBorderStart = calcOuterBorderStart();
    m_outerBorderEnd = calcOuterBorderEnd();
}

LayoutUnit RenderTableSection::firstLineBoxBaseline() const
{
    if (!m_grid.size())
        return -1;

    LayoutUnit firstLineBaseline = m_grid[0].baseline;
    if (firstLineBaseline)
        return firstLineBaseline + m_rowPos[0];

    firstLineBaseline = -1;
    const Row& firstRow = m_grid[0].row;
    for (size_t i = 0; i < firstRow.size(); ++i) {
        const CellStruct& cs = firstRow.at(i);
        const RenderTableCell* cell = cs.primaryCell();
        if (cell)
            firstLineBaseline = max(firstLineBaseline, cell->logicalTop() + cell->paddingBefore() + cell->borderBefore() + cell->contentLogicalHeight());
    }

    return firstLineBaseline;
}

void RenderTableSection::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // put this back in when all layout tests can handle it
    // ASSERT(!needsLayout());
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

    if ((phase == PaintPhaseOutline || phase == PaintPhaseSelfOutline) && style()->visibility() == VISIBLE)
        paintOutline(paintInfo.context, LayoutRect(adjustedPaintOffset, size()));
}

static inline bool compareCellPositions(RenderTableCell* elem1, RenderTableCell* elem2)
{
    return elem1->row() < elem2->row();
}

// This comparison is used only when we have overflowing cells as we have an unsorted array to sort. We thus need
// to sort both on rows and columns to properly repaint.
static inline bool compareCellPositionsWithOverflowingCells(RenderTableCell* elem1, RenderTableCell* elem2)
{
    if (elem1->row() != elem2->row())
        return elem1->row() < elem2->row();

    return elem1->col() < elem2->col();
}

void RenderTableSection::paintCell(RenderTableCell* cell, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint cellPoint = flipForWritingModeForChild(cell, paintOffset);
    PaintPhase paintPhase = paintInfo.phase;
    RenderTableRow* row = toRenderTableRow(cell->parent());

    if (paintPhase == PaintPhaseBlockBackground || paintPhase == PaintPhaseChildBlockBackground) {
        // We need to handle painting a stack of backgrounds.  This stack (from bottom to top) consists of
        // the column group, column, row group, row, and then the cell.
        RenderObject* col = table()->colElement(cell->col());
        RenderObject* colGroup = 0;
        if (col && col->parent()->style()->display() == TABLE_COLUMN_GROUP)
            colGroup = col->parent();

        // Column groups and columns first.
        // FIXME: Columns and column groups do not currently support opacity, and they are being painted "too late" in
        // the stack, since we have already opened a transparency layer (potentially) for the table row group.
        // Note that we deliberately ignore whether or not the cell has a layer, since these backgrounds paint "behind" the
        // cell.
        cell->paintBackgroundsBehindCell(paintInfo, cellPoint, colGroup);
        cell->paintBackgroundsBehindCell(paintInfo, cellPoint, col);

        // Paint the row group next.
        cell->paintBackgroundsBehindCell(paintInfo, cellPoint, this);

        // Paint the row next, but only if it doesn't have a layer.  If a row has a layer, it will be responsible for
        // painting the row background for the cell.
        if (!row->hasSelfPaintingLayer())
            cell->paintBackgroundsBehindCell(paintInfo, cellPoint, row);
    }
    if ((!cell->hasSelfPaintingLayer() && !row->hasSelfPaintingLayer()))
        cell->paint(paintInfo, cellPoint);
}

void RenderTableSection::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // Check which rows and cols are visible and only paint these.
    unsigned totalRows = m_grid.size();
    unsigned totalCols = table()->columns().size();

    PaintPhase paintPhase = paintInfo.phase;

    LayoutUnit os = 2 * maximalOutlineSize(paintPhase);
    unsigned startrow = 0;
    unsigned endrow = totalRows;

    LayoutRect localRepaintRect = paintInfo.rect;
    localRepaintRect.moveBy(-paintOffset);
    if (style()->isFlippedBlocksWritingMode()) {
        if (style()->isHorizontalWritingMode())
            localRepaintRect.setY(height() - localRepaintRect.maxY());
        else
            localRepaintRect.setX(width() - localRepaintRect.maxX());
    }

    if (!m_forceSlowPaintPathWithOverflowingCell) {
        LayoutUnit before = (style()->isHorizontalWritingMode() ? localRepaintRect.y() : localRepaintRect.x()) - os;
        // binary search to find a row
        startrow = std::lower_bound(m_rowPos.begin(), m_rowPos.end(), before) - m_rowPos.begin();

        // The binary search above gives us the first row with
        // a y position >= the top of the paint rect. Thus, the previous
        // may need to be repainted as well.
        if (startrow == m_rowPos.size() || (startrow > 0 && (m_rowPos[startrow] >  before)))
          --startrow;

        LayoutUnit after = (style()->isHorizontalWritingMode() ? localRepaintRect.maxY() : localRepaintRect.maxX()) + os;
        endrow = std::lower_bound(m_rowPos.begin(), m_rowPos.end(), after) - m_rowPos.begin();
        if (endrow == m_rowPos.size())
          --endrow;

        if (!endrow && m_rowPos[0] - table()->outerBorderBefore() <= after)
            ++endrow;
    }

    unsigned startcol = 0;
    unsigned endcol = totalCols;
    // FIXME: Implement RTL.
    if (!m_forceSlowPaintPathWithOverflowingCell && style()->isLeftToRightDirection()) {
        LayoutUnit start = (style()->isHorizontalWritingMode() ? localRepaintRect.x() : localRepaintRect.y()) - os;
        Vector<LayoutUnit>& columnPos = table()->columnPositions();
        startcol = std::lower_bound(columnPos.begin(), columnPos.end(), start) - columnPos.begin();
        if ((startcol == columnPos.size()) || (startcol > 0 && (columnPos[startcol] > start)))
            --startcol;

        LayoutUnit end = (style()->isHorizontalWritingMode() ? localRepaintRect.maxX() : localRepaintRect.maxY()) + os;
        endcol = std::lower_bound(columnPos.begin(), columnPos.end(), end) - columnPos.begin();
        if (endcol == columnPos.size())
            --endcol;

        if (!endcol && columnPos[0] - table()->outerBorderStart() <= end)
            ++endcol;
    }
    if (startcol < endcol) {
        if (!m_hasMultipleCellLevels && !m_overflowingCells.size()) {
            if (paintInfo.phase == PaintPhaseCollapsedTableBorders) {
                // Collapsed borders are painted from the bottom right to the top left so that precedence
                // due to cell position is respected.
                for (unsigned r = endrow; r > startrow; r--) {
                    unsigned row = r - 1;
                    for (unsigned c = endcol; c > startcol; c--) {
                        unsigned col = c - 1;
                        CellStruct& current = cellAt(row, col);
                        RenderTableCell* cell = current.primaryCell();
                        if (!cell || (row > startrow && primaryCellAt(row - 1, col) == cell) || (col > startcol && primaryCellAt(row, col - 1) == cell))
                            continue;
                        LayoutPoint cellPoint = flipForWritingModeForChild(cell, paintOffset);
                        cell->paintCollapsedBorders(paintInfo, cellPoint);
                    }
                }
            } else {
                // Draw the dirty cells in the order that they appear.
                for (unsigned r = startrow; r < endrow; r++) {
                    RenderTableRow* row = m_grid[r].rowRenderer;
                    if (row && !row->hasSelfPaintingLayer())
                        row->paintOutlineForRowIfNeeded(paintInfo, paintOffset);
                    for (unsigned c = startcol; c < endcol; c++) {
                        CellStruct& current = cellAt(r, c);
                        RenderTableCell* cell = current.primaryCell();
                        if (!cell || (r > startrow && primaryCellAt(r - 1, c) == cell) || (c > startcol && primaryCellAt(r, c - 1) == cell))
                            continue;
                        paintCell(cell, paintInfo, paintOffset);
                    }
                }
            }
        } else {
            // The overflowing cells should be scarce to avoid adding a lot of cells to the HashSet.
            ASSERT(m_overflowingCells.size() < totalRows * totalCols * gMaxAllowedOverflowingCellRatioForFastPaintPath);

            // To make sure we properly repaint the section, we repaint all the overflowing cells that we collected.
            Vector<RenderTableCell*> cells;
            copyToVector(m_overflowingCells, cells);

            HashSet<RenderTableCell*> spanningCells;

            for (unsigned r = startrow; r < endrow; r++) {
                RenderTableRow* row = m_grid[r].rowRenderer;
                if (row && !row->hasSelfPaintingLayer())
                    row->paintOutlineForRowIfNeeded(paintInfo, paintOffset);
                for (unsigned c = startcol; c < endcol; c++) {
                    CellStruct& current = cellAt(r, c);
                    if (!current.hasCells())
                        continue;
                    for (unsigned i = 0; i < current.cells.size(); ++i) {
                        if (m_overflowingCells.contains(current.cells[i]))
                            continue;

                        if (current.cells[i]->rowSpan() > 1 || current.cells[i]->colSpan() > 1) {
                            if (spanningCells.contains(current.cells[i]))
                                continue;
                            spanningCells.add(current.cells[i]);
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

            if (paintInfo.phase == PaintPhaseCollapsedTableBorders) {
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
    // We reset the flag here to ensure that |addCell| works. This is safe to do as
    // fillRowsWithDefaultStartingAtPosition makes sure we match the table's columns
    // representation.
    m_needsCellRecalc = false;

    m_cCol = 0;
    m_cRow = 0;
    m_grid.clear();

    for (RenderObject* row = firstChild(); row; row = row->nextSibling()) {
        if (row->isTableRow()) {
            unsigned insertionRow = m_cRow;
            m_cRow++;
            m_cCol = 0;
            ensureRows(m_cRow);

            RenderTableRow* tableRow = toRenderTableRow(row);
            m_grid[insertionRow].rowRenderer = tableRow;
            setRowLogicalHeightToRowStyleLogicalHeightIfNotRelative(m_grid[insertionRow]);

            for (RenderObject* cell = row->firstChild(); cell; cell = cell->nextSibling()) {
                if (!cell->isTableCell())
                    continue;

                RenderTableCell* tableCell = toRenderTableCell(cell);
                addCell(tableCell, tableRow);
            }
        }
    }

    m_grid.shrinkToFit();
    setNeedsLayout(true);
}

// FIXME: This function could be made O(1) in certain cases (like for the non-most-constrainive cells' case).
void RenderTableSection::rowLogicalHeightChanged(unsigned rowIndex)
{
    if (needsCellRecalc())
        return;

    setRowLogicalHeightToRowStyleLogicalHeightIfNotRelative(m_grid[rowIndex]);

    for (RenderObject* cell = m_grid[rowIndex].rowRenderer->firstChild(); cell; cell = cell->nextSibling()) {
        if (!cell->isTableCell())
            continue;

        updateLogicalHeightForCell(m_grid[rowIndex], toRenderTableCell(cell));
    }
}

void RenderTableSection::setNeedsCellRecalc()
{
    m_needsCellRecalc = true;
    if (RenderTable* t = table())
        t->setNeedsSectionRecalc();
}

unsigned RenderTableSection::numColumns() const
{
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
            r[pos + 1].cells.append(r[pos].cells);
            RenderTableCell* cell = r[pos].primaryCell();
            ASSERT(cell);
            ASSERT(cell->colSpan() >= (r[pos].inColSpan ? 1 : 0));
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
bool RenderTableSection::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    // If we have no children then we have nothing to do.
    if (!firstChild())
        return false;

    // Table sections cannot ever be hit tested.  Effectively they do not exist.
    // Just forward to our children always.
    LayoutPoint adjustedLocation = accumulatedOffset + location();

    if (hasOverflowClip() && !overflowClipRect(adjustedLocation, result.region()).intersects(result.rectForPoint(pointInContainer)))
        return false;

    if (hasOverflowingCell()) {
        for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
            // FIXME: We have to skip over inline flows, since they can show up inside table rows
            // at the moment (a demoted inline <form> for example). If we ever implement a
            // table-specific hit-test method (which we should do for performance reasons anyway),
            // then we can remove this check.
            if (child->isBox() && !toRenderBox(child)->hasSelfPaintingLayer()) {
                LayoutPoint childPoint = flipForWritingModeForChild(toRenderBox(child), adjustedLocation);
                if (child->nodeAtPoint(request, result, pointInContainer, childPoint, action)) {
                    updateHitTestResult(result, toLayoutPoint(pointInContainer - childPoint));
                    return true;
                }
            }
        }
        return false;
    }

    LayoutPoint location = pointInContainer - toLayoutSize(adjustedLocation);
    if (style()->isFlippedBlocksWritingMode()) {
        if (style()->isHorizontalWritingMode())
            location.setY(height() - location.y());
        else
            location.setX(width() - location.x());
    }

    LayoutUnit offsetInColumnDirection = style()->isHorizontalWritingMode() ? location.y() : location.x();
    // Find the first row that starts after offsetInColumnDirection.
    unsigned nextRow = std::upper_bound(m_rowPos.begin(), m_rowPos.end(), offsetInColumnDirection) - m_rowPos.begin();
    if (nextRow == m_rowPos.size())
        return false;
    // Now set hitRow to the index of the hit row, or 0.
    unsigned hitRow = nextRow > 0 ? nextRow - 1 : 0;

    Vector<LayoutUnit>& columnPos = table()->columnPositions();
    LayoutUnit offsetInRowDirection = style()->isHorizontalWritingMode() ? location.x() : location.y();
    if (!style()->isLeftToRightDirection())
        offsetInRowDirection = columnPos[columnPos.size() - 1] - offsetInRowDirection;

    unsigned nextColumn = std::lower_bound(columnPos.begin(), columnPos.end(), offsetInRowDirection) - columnPos.begin();
    if (nextColumn == columnPos.size())
        return false;
    unsigned hitColumn = nextColumn > 0 ? nextColumn - 1 : 0;

    CellStruct& current = cellAt(hitRow, hitColumn);

    // If the cell is empty, there's nothing to do
    if (!current.hasCells())
        return false;

    for (unsigned i = current.cells.size() ; i; ) {
        --i;
        RenderTableCell* cell = current.cells[i];
        LayoutPoint cellPoint = flipForWritingModeForChild(cell, adjustedLocation);
        if (static_cast<RenderObject*>(cell)->nodeAtPoint(request, result, pointInContainer, cellPoint, action)) {
            updateHitTestResult(result, toLayoutPoint(pointInContainer - cellPoint));
            return true;
        }
    }
    return false;

}

unsigned RenderTableSection::rowIndexForRenderer(const RenderTableRow* row) const
{
    for (size_t i = 0; i < m_grid.size(); ++i) {
        if (m_grid[i].rowRenderer == row)
            return i;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void RenderTableSection::removeCachedCollapsedBorders(const RenderTableCell* cell)
{
    if (!table()->collapseBorders())
        return;
    
    for (int side = CBSBefore; side <= CBSEnd; ++side)
        m_cellsCollapsedBorders.remove(make_pair(cell, side));
}

void RenderTableSection::setCachedCollapsedBorder(const RenderTableCell* cell, CollapsedBorderSide side, CollapsedBorderValue border)
{
    ASSERT(table()->collapseBorders());
    m_cellsCollapsedBorders.set(make_pair(cell, side), border);
}

CollapsedBorderValue& RenderTableSection::cachedCollapsedBorder(const RenderTableCell* cell, CollapsedBorderSide side)
{
    ASSERT(table()->collapseBorders());
    HashMap<pair<const RenderTableCell*, int>, CollapsedBorderValue>::iterator it = m_cellsCollapsedBorders.find(make_pair(cell, side));
    ASSERT(it != m_cellsCollapsedBorders.end());
    return it->second;
}

} // namespace WebCore
