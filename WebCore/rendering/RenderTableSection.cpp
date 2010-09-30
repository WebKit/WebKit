/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

static inline void setRowHeightToRowStyleHeightIfNotRelative(RenderTableSection::RowStruct* row)
{
    ASSERT(row && row->rowRenderer);
    row->height = row->rowRenderer->style()->height();
    if (row->height.isRelative())
        row->height = Length();
}

RenderTableSection::RenderTableSection(Node* node)
    : RenderBox(node)
    , m_gridRows(0)
    , m_cCol(0)
    , m_cRow(-1)
    , m_outerBorderLeft(0)
    , m_outerBorderRight(0)
    , m_outerBorderTop(0)
    , m_outerBorderBottom(0)
    , m_needsCellRecalc(false)
    , m_hasOverflowingCell(false)
    , m_hasMultipleCellLevels(false)
{
    // init RenderObject attributes
    setInline(false); // our object is not Inline
}

RenderTableSection::~RenderTableSection()
{
    clearGrid();
}

void RenderTableSection::destroy()
{
    RenderTable* recalcTable = table();
    
    RenderBox::destroy();
    
    // recalc cell info because RenderTable has unguarded pointers
    // stored that point to this RenderTableSection.
    if (recalcTable)
        recalcTable->setNeedsSectionRecalc();
}

void RenderTableSection::addChild(RenderObject* child, RenderObject* beforeChild)
{
    // Make sure we don't append things after :after-generated content if we have it.
    if (!beforeChild && isAfterContent(lastChild()))
        beforeChild = lastChild();

    if (!child->isTableRow()) {
        RenderObject* last = beforeChild;
        if (!last)
            last = lastChild();
        if (last && last->isAnonymous()) {
            last->addChild(child);
            return;
        }

        // If beforeChild is inside an anonymous cell/row, insert into the cell or into
        // the anonymous row containing it, if there is one.
        RenderObject* lastBox = last;
        while (lastBox && lastBox->parent()->isAnonymous() && !lastBox->isTableRow())
            lastBox = lastBox->parent();
        if (lastBox && lastBox->isAnonymous()) {
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

    ++m_cRow;
    m_cCol = 0;

    // make sure we have enough rows
    if (!ensureRows(m_cRow + 1))
        return;

    m_grid[m_cRow].rowRenderer = toRenderTableRow(child);

    if (!beforeChild)
        setRowHeightToRowStyleHeightIfNotRelative(&m_grid[m_cRow]);

    // If the next renderer is actually wrapped in an anonymous table row, we need to go up and find that.
    while (beforeChild && beforeChild->parent() != this)
        beforeChild = beforeChild->parent();

    ASSERT(!beforeChild || beforeChild->isTableRow());
    RenderBox::addChild(child, beforeChild);
}

void RenderTableSection::removeChild(RenderObject* oldChild)
{
    setNeedsCellRecalc();
    RenderBox::removeChild(oldChild);
}

bool RenderTableSection::ensureRows(int numRows)
{
    int nRows = m_gridRows;
    if (numRows > nRows) {
        if (numRows > static_cast<int>(m_grid.size())) {
            size_t maxSize = numeric_limits<size_t>::max() / sizeof(RowStruct);
            if (static_cast<size_t>(numRows) > maxSize)
                return false;
            m_grid.grow(numRows);
        }
        m_gridRows = numRows;
        int nCols = max(1, table()->numEffCols());
        for (int r = nRows; r < numRows; r++) {
            m_grid[r].row = new Row(nCols);
            m_grid[r].rowRenderer = 0;
            m_grid[r].baseline = 0;
            m_grid[r].height = Length();
        }
    }

    return true;
}

void RenderTableSection::addCell(RenderTableCell* cell, RenderTableRow* row)
{
    int rSpan = cell->rowSpan();
    int cSpan = cell->colSpan();
    Vector<RenderTable::ColumnStruct>& columns = table()->columns();
    int nCols = columns.size();

    // ### mozilla still seems to do the old HTML way, even for strict DTD
    // (see the annotation on table cell layouting in the CSS specs and the testcase below:
    // <TABLE border>
    // <TR><TD>1 <TD rowspan="2">2 <TD>3 <TD>4
    // <TR><TD colspan="2">5
    // </TABLE>
    while (m_cCol < nCols && (cellAt(m_cRow, m_cCol).hasCells() || cellAt(m_cRow, m_cCol).inColSpan))
        m_cCol++;

    if (rSpan == 1) {
        // we ignore height settings on rowspan cells
        Length height = cell->style()->height();
        if (height.isPositive() || (height.isRelative() && height.value() >= 0)) {
            Length cRowHeight = m_grid[m_cRow].height;
            switch (height.type()) {
                case Percent:
                    if (!(cRowHeight.isPercent()) ||
                        (cRowHeight.isPercent() && cRowHeight.rawValue() < height.rawValue()))
                        m_grid[m_cRow].height = height;
                        break;
                case Fixed:
                    if (cRowHeight.type() < Percent ||
                        (cRowHeight.isFixed() && cRowHeight.value() < height.value()))
                        m_grid[m_cRow].height = height;
                    break;
                case Relative:
                default:
                    break;
            }
        }
    }

    // make sure we have enough rows
    if (!ensureRows(m_cRow + rSpan))
        return;

    m_grid[m_cRow].rowRenderer = row;

    int col = m_cCol;
    // tell the cell where it is
    bool inColSpan = false;
    while (cSpan) {
        int currentSpan;
        if (m_cCol >= nCols) {
            table()->appendColumn(cSpan);
            currentSpan = cSpan;
        } else {
            if (cSpan < (int)columns[m_cCol].span)
                table()->splitColumn(m_cCol, cSpan);
            currentSpan = columns[m_cCol].span;
        }
        for (int r = 0; r < rSpan; r++) {
            CellStruct& c = cellAt(m_cRow + r, m_cCol);
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
    cell->setRow(m_cRow);
    cell->setCol(table()->effColToCol(col));
}

void RenderTableSection::setCellWidths()
{
    Vector<int>& columnPos = table()->columnPositions();

    LayoutStateMaintainer statePusher(view());
    
    for (int i = 0; i < m_gridRows; i++) {
        Row& row = *m_grid[i].row;
        int cols = row.size();
        for (int j = 0; j < cols; j++) {
            CellStruct& current = row[j];
            RenderTableCell* cell = current.primaryCell();
            if (!cell || current.inColSpan)
              continue;
            int endCol = j;
            int cspan = cell->colSpan();
            while (cspan && endCol < cols) {
                ASSERT(endCol < (int)table()->columns().size());
                cspan -= table()->columns()[endCol].span;
                endCol++;
            }
            int w = columnPos[endCol] - columnPos[j] - table()->hBorderSpacing();
            int oldWidth = cell->width();
            if (w != oldWidth) {
                cell->setNeedsLayout(true);
                if (!table()->selfNeedsLayout() && cell->checkForRepaintDuringLayout()) {
                    if (!statePusher.didPush()) {
                        // Technically, we should also push state for the row, but since
                        // rows don't push a coordinate transform, that's not necessary.
                        statePusher.push(this, IntSize(x(), y()));
                    }
                    cell->repaint();
                }
                cell->updateWidth(w);
            }
        }
    }
    
    statePusher.pop(); // only pops if we pushed
}

int RenderTableSection::calcRowHeight()
{
#ifndef NDEBUG
    setNeedsLayoutIsForbidden(true);
#endif

    ASSERT(!needsLayout());

    RenderTableCell* cell;

    int spacing = table()->vBorderSpacing();

    LayoutStateMaintainer statePusher(view());

    m_rowPos.resize(m_gridRows + 1);
    m_rowPos[0] = spacing;

    for (int r = 0; r < m_gridRows; r++) {
        m_rowPos[r + 1] = 0;
        m_grid[r].baseline = 0;
        int baseline = 0;
        int bdesc = 0;
        int ch = m_grid[r].height.calcMinValue(0);
        int pos = m_rowPos[r] + ch + (m_grid[r].rowRenderer ? spacing : 0);

        m_rowPos[r + 1] = max(m_rowPos[r + 1], pos);

        Row* row = m_grid[r].row;
        int totalCols = row->size();

        for (int c = 0; c < totalCols; c++) {
            CellStruct& current = cellAt(r, c);
            cell = current.primaryCell();

            if (!cell || current.inColSpan)
                continue;

            if ((cell->row() + cell->rowSpan() - 1) > r)
                continue;

            int indx = max(r - cell->rowSpan() + 1, 0);

            if (cell->overrideSize() != -1) {
                if (!statePusher.didPush()) {
                    // Technically, we should also push state for the row, but since
                    // rows don't push a coordinate transform, that's not necessary.
                    statePusher.push(this, IntSize(x(), y()));
                }
                cell->setOverrideSize(-1);
                cell->setChildNeedsLayout(true, false);
                cell->layoutIfNeeded();
            }
            
            int adjustedPaddingTop = cell->paddingTop() - cell->intrinsicPaddingBefore();
            int adjustedPaddingBottom = cell->paddingBottom() - cell->intrinsicPaddingAfter();
            int adjustedHeight = cell->height() - (cell->intrinsicPaddingBefore() + cell->intrinsicPaddingAfter());
        
            // Explicit heights use the border box in quirks mode.  In strict mode do the right
            // thing and actually add in the border and padding.
            ch = cell->style()->height().calcValue(0) + 
                (document()->inQuirksMode() ? 0 : (adjustedPaddingTop + adjustedPaddingBottom +
                                                   cell->borderTop() + cell->borderBottom()));
            ch = max(ch, adjustedHeight);

            pos = m_rowPos[indx] + ch + (m_grid[r].rowRenderer ? spacing : 0);

            m_rowPos[r + 1] = max(m_rowPos[r + 1], pos);

            // find out the baseline
            EVerticalAlign va = cell->style()->verticalAlign();
            if (va == BASELINE || va == TEXT_BOTTOM || va == TEXT_TOP || va == SUPER || va == SUB) {
                int b = cell->baselinePosition();
                if (b > cell->borderTop() + cell->paddingTop()) {
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

    return m_rowPos[m_gridRows];
}

void RenderTableSection::layout()
{
    ASSERT(needsLayout());

    LayoutStateMaintainer statePusher(view(), this, IntSize(x(), y()));
    for (RenderObject* child = children()->firstChild(); child; child = child->nextSibling()) {
        if (child->isTableRow()) {
            child->layoutIfNeeded();
            ASSERT(!child->needsLayout());
        }
    }
    statePusher.pop();
    setNeedsLayout(false);
}

int RenderTableSection::layoutRows(int toAdd)
{
#ifndef NDEBUG
    setNeedsLayoutIsForbidden(true);
#endif

    ASSERT(!needsLayout());

    int rHeight;
    int rindx;
    int totalRows = m_gridRows;
    
    // Set the width of our section now.  The rows will also be this width.
    setWidth(table()->contentWidth());
    m_overflow.clear();
    m_hasOverflowingCell = false;

    if (toAdd && totalRows && (m_rowPos[totalRows] || !nextSibling())) {
        int totalHeight = m_rowPos[totalRows] + toAdd;

        int dh = toAdd;
        int totalPercent = 0;
        int numAuto = 0;
        for (int r = 0; r < totalRows; r++) {
            if (m_grid[r].height.isAuto())
                numAuto++;
            else if (m_grid[r].height.isPercent())
                totalPercent += m_grid[r].height.rawValue();
        }
        if (totalPercent) {
            // try to satisfy percent
            int add = 0;
            totalPercent = min(totalPercent, 100 * percentScaleFactor);
            int rh = m_rowPos[1] - m_rowPos[0];
            for (int r = 0; r < totalRows; r++) {
                if (totalPercent > 0 && m_grid[r].height.isPercent()) {
                    int toAdd = min(dh, (totalHeight * m_grid[r].height.rawValue() / (100 * percentScaleFactor)) - rh);
                    // If toAdd is negative, then we don't want to shrink the row (this bug
                    // affected Outlook Web Access).
                    toAdd = max(0, toAdd);
                    add += toAdd;
                    dh -= toAdd;
                    totalPercent -= m_grid[r].height.rawValue();
                }
                if (r < totalRows - 1)
                    rh = m_rowPos[r + 2] - m_rowPos[r + 1];
                m_rowPos[r + 1] += add;
            }
        }
        if (numAuto) {
            // distribute over variable cols
            int add = 0;
            for (int r = 0; r < totalRows; r++) {
                if (numAuto > 0 && m_grid[r].height.isAuto()) {
                    int toAdd = dh / numAuto;
                    add += toAdd;
                    dh -= toAdd;
                    numAuto--;
                }
                m_rowPos[r + 1] += add;
            }
        }
        if (dh > 0 && m_rowPos[totalRows]) {
            // if some left overs, distribute equally.
            int tot = m_rowPos[totalRows];
            int add = 0;
            int prev = m_rowPos[0];
            for (int r = 0; r < totalRows; r++) {
                // weight with the original height
                add += dh * (m_rowPos[r + 1] - prev) / tot;
                prev = m_rowPos[r + 1];
                m_rowPos[r + 1] += add;
            }
        }
    }

    int hspacing = table()->hBorderSpacing();
    int vspacing = table()->vBorderSpacing();
    int nEffCols = table()->numEffCols();

    LayoutStateMaintainer statePusher(view(), this, IntSize(x(), y()));

    for (int r = 0; r < totalRows; r++) {
        // Set the row's x/y position and width/height.
        if (RenderTableRow* rowRenderer = m_grid[r].rowRenderer) {
            rowRenderer->setLocation(0, m_rowPos[r]);
            rowRenderer->setWidth(width());
            rowRenderer->setHeight(m_rowPos[r + 1] - m_rowPos[r] - vspacing);
        }

        for (int c = 0; c < nEffCols; c++) {
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
            bool flexAllChildren = cell->style()->height().isFixed()
                || (!table()->style()->height().isAuto() && rHeight != cell->height());

            for (RenderObject* o = cell->firstChild(); o; o = o->nextSibling()) {
                if (!o->isText() && o->style()->height().isPercent() && (flexAllChildren || o->isReplaced() || (o->isBox() && toRenderBox(o)->scrollsOverflow()))) {
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
                cell->setOverrideSize(max(0, 
                                           rHeight - cell->borderTop() - cell->paddingTop() - 
                                                     cell->borderBottom() - cell->paddingBottom()));
                cell->layoutIfNeeded();
                
                // If the baseline moved, we may have to update the data for our row. Find out the new baseline.
                EVerticalAlign va = cell->style()->verticalAlign();
                if (va == BASELINE || va == TEXT_BOTTOM || va == TEXT_TOP || va == SUPER || va == SUB) {
                    int b = cell->baselinePosition();
                    if (b > cell->borderTop() + cell->paddingTop())
                        m_grid[r].baseline = max(m_grid[r].baseline, b);
                }
            }
            
            int oldTe = cell->intrinsicPaddingBefore();
            int oldBe = cell->intrinsicPaddingAfter();
            int heightWithoutIntrinsicPadding = cell->height() - oldTe - oldBe;
            
            int te = 0;
            switch (cell->style()->verticalAlign()) {
                case SUB:
                case SUPER:
                case TEXT_TOP:
                case TEXT_BOTTOM:
                case BASELINE: {
                    int b = cell->baselinePosition();
                    if (b > cell->borderTop() + cell->paddingTop())
                        te = getBaseline(r) - (b - oldTe);
                    break;
                }
                case TOP:
                    te = 0;
                    break;
                case MIDDLE:
                    te = (rHeight - heightWithoutIntrinsicPadding) / 2;
                    break;
                case BOTTOM:
                    te = rHeight - heightWithoutIntrinsicPadding;
                    break;
                default:
                    break;
            }
            
            int be = rHeight - heightWithoutIntrinsicPadding - te;
            cell->setIntrinsicPaddingBefore(te);
            cell->setIntrinsicPaddingAfter(be);
            
            IntRect oldCellRect(cell->x(), cell->y() , cell->width(), cell->height());
            
            if (!style()->isLeftToRightDirection())
                cell->setLocation(table()->columnPositions()[nEffCols] - table()->columnPositions()[table()->colToEffCol(cell->col() + cell->colSpan())] + hspacing, m_rowPos[rindx]);
            else
                cell->setLocation(table()->columnPositions()[c] + hspacing, m_rowPos[rindx]);
            view()->addLayoutDelta(IntSize(oldCellRect.x() - cell->x(), oldCellRect.y() - cell->y()));

            if (te != oldTe || be != oldBe)
                cell->setNeedsLayout(true, false);
            
            if (!cell->needsLayout() && view()->layoutState()->m_pageHeight && view()->layoutState()->pageY(cell->y()) != cell->pageY())
                cell->setChildNeedsLayout(true, false);
            
            cell->layoutIfNeeded();
            
            if (view()->layoutState()->m_pageHeight && cell->height() != rHeight)
                cell->setHeight(rHeight); // FIXME: Pagination might have made us change size.  For now just shrink or grow the cell to fit without doing a relayout.
        
            IntSize childOffset(cell->x() - oldCellRect.x(), cell->y() - oldCellRect.y());
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

    setHeight(m_rowPos[totalRows]);

    // Now that our height has been determined, add in overflow from cells.
    for (int r = 0; r < totalRows; r++) {
        for (int c = 0; c < nEffCols; c++) {
            CellStruct& cs = cellAt(r, c);
            RenderTableCell* cell = cs.primaryCell();
            if (!cell || cs.inColSpan)
                continue;
            if (r < totalRows - 1 && cell == primaryCellAt(r + 1, c))
                continue;
            addOverflowFromChild(cell);
            m_hasOverflowingCell |= cell->hasVisibleOverflow();
        }
    }

    statePusher.pop();
    return height();
}

int RenderTableSection::lowestPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int bottom = RenderBox::lowestPosition(includeOverflowInterior, includeSelf);
    if (!includeOverflowInterior && hasOverflowClip())
        return bottom;

    for (RenderObject* row = firstChild(); row; row = row->nextSibling()) {
        for (RenderObject* curr = row->firstChild(); curr; curr = curr->nextSibling()) {
            if (curr->isTableCell()) {
                RenderTableCell* cell = toRenderTableCell(curr);
                bottom = max(bottom, cell->y() + cell->lowestPosition(false));
            }
        }
    }
    
    return bottom;
}

int RenderTableSection::rightmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int right = RenderBox::rightmostPosition(includeOverflowInterior, includeSelf);
    if (!includeOverflowInterior && hasOverflowClip())
        return right;

    for (RenderObject* row = firstChild(); row; row = row->nextSibling()) {
        for (RenderObject* curr = row->firstChild(); curr; curr = curr->nextSibling()) {
            if (curr->isTableCell()) {
                RenderTableCell* cell = toRenderTableCell(curr);
                right = max(right, cell->x() + cell->rightmostPosition(false));
            }
        }
    }
    
    return right;
}

int RenderTableSection::leftmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int left = RenderBox::leftmostPosition(includeOverflowInterior, includeSelf);
    if (!includeOverflowInterior && hasOverflowClip())
        return left;
    
    for (RenderObject* row = firstChild(); row; row = row->nextSibling()) {
        for (RenderObject* curr = row->firstChild(); curr; curr = curr->nextSibling()) {
            if (curr->isTableCell()) {
                RenderTableCell* cell = toRenderTableCell(curr);
                left = min(left, cell->x() + cell->leftmostPosition(false));
            }
        }
    }
    
    return left;
}

int RenderTableSection::calcOuterBorderTop() const
{
    int totalCols = table()->numEffCols();
    if (!m_gridRows || !totalCols)
        return 0;

    unsigned borderWidth = 0;

    const BorderValue& sb = style()->borderTop();
    if (sb.style() == BHIDDEN)
        return -1;
    if (sb.style() > BHIDDEN)
        borderWidth = sb.width();

    const BorderValue& rb = firstChild()->style()->borderTop();
    if (rb.style() == BHIDDEN)
        return -1;
    if (rb.style() > BHIDDEN && rb.width() > borderWidth)
        borderWidth = rb.width();

    bool allHidden = true;
    for (int c = 0; c < totalCols; c++) {
        const CellStruct& current = cellAt(0, c);
        if (current.inColSpan || !current.hasCells())
            continue;
        const BorderValue& cb = current.primaryCell()->style()->borderTop();
        // FIXME: Don't repeat for the same col group
        RenderTableCol* colGroup = table()->colElement(c);
        if (colGroup) {
            const BorderValue& gb = colGroup->style()->borderTop();
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

int RenderTableSection::calcOuterBorderBottom() const
{
    int totalCols = table()->numEffCols();
    if (!m_gridRows || !totalCols)
        return 0;

    unsigned borderWidth = 0;

    const BorderValue& sb = style()->borderBottom();
    if (sb.style() == BHIDDEN)
        return -1;
    if (sb.style() > BHIDDEN)
        borderWidth = sb.width();

    const BorderValue& rb = lastChild()->style()->borderBottom();
    if (rb.style() == BHIDDEN)
        return -1;
    if (rb.style() > BHIDDEN && rb.width() > borderWidth)
        borderWidth = rb.width();

    bool allHidden = true;
    for (int c = 0; c < totalCols; c++) {
        const CellStruct& current = cellAt(m_gridRows - 1, c);
        if (current.inColSpan || !current.hasCells())
            continue;
        const BorderValue& cb = current.primaryCell()->style()->borderBottom();
        // FIXME: Don't repeat for the same col group
        RenderTableCol* colGroup = table()->colElement(c);
        if (colGroup) {
            const BorderValue& gb = colGroup->style()->borderBottom();
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

int RenderTableSection::calcOuterBorderLeft(bool rtl) const
{
    int totalCols = table()->numEffCols();
    if (!m_gridRows || !totalCols)
        return 0;

    unsigned borderWidth = 0;

    const BorderValue& sb = style()->borderLeft();
    if (sb.style() == BHIDDEN)
        return -1;
    if (sb.style() > BHIDDEN)
        borderWidth = sb.width();

    int leftmostColumn = rtl ? totalCols - 1 : 0;
    RenderTableCol* colGroup = table()->colElement(leftmostColumn);
    if (colGroup) {
        const BorderValue& gb = colGroup->style()->borderLeft();
        if (gb.style() == BHIDDEN)
            return -1;
        if (gb.style() > BHIDDEN && gb.width() > borderWidth)
            borderWidth = gb.width();
    }

    bool allHidden = true;
    for (int r = 0; r < m_gridRows; r++) {
        const CellStruct& current = cellAt(r, leftmostColumn);
        if (!current.hasCells())
            continue;
        // FIXME: Don't repeat for the same cell
        const BorderValue& cb = current.primaryCell()->style()->borderLeft();
        const BorderValue& rb = current.primaryCell()->parent()->style()->borderLeft();
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

    return borderWidth / 2;
}

int RenderTableSection::calcOuterBorderRight(bool rtl) const
{
    int totalCols = table()->numEffCols();
    if (!m_gridRows || !totalCols)
        return 0;

    unsigned borderWidth = 0;

    const BorderValue& sb = style()->borderRight();
    if (sb.style() == BHIDDEN)
        return -1;
    if (sb.style() > BHIDDEN)
        borderWidth = sb.width();

    int rightmostColumn = rtl ? 0 : totalCols - 1;
    RenderTableCol* colGroup = table()->colElement(rightmostColumn);
    if (colGroup) {
        const BorderValue& gb = colGroup->style()->borderRight();
        if (gb.style() == BHIDDEN)
            return -1;
        if (gb.style() > BHIDDEN && gb.width() > borderWidth)
            borderWidth = gb.width();
    }

    bool allHidden = true;
    for (int r = 0; r < m_gridRows; r++) {
        const CellStruct& current = cellAt(r, rightmostColumn);
        if (!current.hasCells())
            continue;
        // FIXME: Don't repeat for the same cell
        const BorderValue& cb = current.primaryCell()->style()->borderRight();
        const BorderValue& rb = current.primaryCell()->parent()->style()->borderRight();
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

    return (borderWidth + 1) / 2;
}

void RenderTableSection::recalcOuterBorder()
{
    bool rtl = !table()->style()->isLeftToRightDirection();
    m_outerBorderTop = calcOuterBorderTop();
    m_outerBorderBottom = calcOuterBorderBottom();
    m_outerBorderLeft = calcOuterBorderLeft(rtl);
    m_outerBorderRight = calcOuterBorderRight(rtl);
}

int RenderTableSection::firstLineBoxBaseline() const
{
    if (!m_gridRows)
        return -1;

    int firstLineBaseline = m_grid[0].baseline;
    if (firstLineBaseline)
        return firstLineBaseline + m_rowPos[0];

    firstLineBaseline = -1;
    Row* firstRow = m_grid[0].row;
    for (size_t i = 0; i < firstRow->size(); ++i) {
        CellStruct& cs = firstRow->at(i);
        RenderTableCell* cell = cs.primaryCell();
        if (cell)
            firstLineBaseline = max(firstLineBaseline, cell->y() + cell->paddingTop() + cell->borderTop() + cell->contentHeight());
    }

    return firstLineBaseline;
}

void RenderTableSection::paint(PaintInfo& paintInfo, int tx, int ty)
{
    // put this back in when all layout tests can handle it
    // ASSERT(!needsLayout());
    // avoid crashing on bugs that cause us to paint with dirty layout
    if (needsLayout())
        return;
    
    unsigned totalRows = m_gridRows;
    unsigned totalCols = table()->columns().size();

    if (!totalRows || !totalCols)
        return;

    tx += x();
    ty += y();

    PaintPhase phase = paintInfo.phase;
    bool pushedClip = pushContentsClip(paintInfo, tx, ty);
    paintObject(paintInfo, tx, ty);
    if (pushedClip)
        popContentsClip(paintInfo, phase, tx, ty);
}

static inline bool compareCellPositions(RenderTableCell* elem1, RenderTableCell* elem2)
{
    return elem1->row() < elem2->row();
}

void RenderTableSection::paintCell(RenderTableCell* cell, PaintInfo& paintInfo, int tx, int ty)
{
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
        cell->paintBackgroundsBehindCell(paintInfo, tx, ty, colGroup);
        cell->paintBackgroundsBehindCell(paintInfo, tx, ty, col);

        // Paint the row group next.
        cell->paintBackgroundsBehindCell(paintInfo, tx, ty, this);

        // Paint the row next, but only if it doesn't have a layer.  If a row has a layer, it will be responsible for
        // painting the row background for the cell.
        if (!row->hasSelfPaintingLayer())
            cell->paintBackgroundsBehindCell(paintInfo, tx, ty, row);
    }
    if ((!cell->hasSelfPaintingLayer() && !row->hasSelfPaintingLayer()) || paintInfo.phase == PaintPhaseCollapsedTableBorders)
        cell->paint(paintInfo, tx, ty);

}

void RenderTableSection::paintObject(PaintInfo& paintInfo, int tx, int ty)
{
    // Check which rows and cols are visible and only paint these.
    // FIXME: Could use a binary search here.
    unsigned totalRows = m_gridRows;
    unsigned totalCols = table()->columns().size();

    PaintPhase paintPhase = paintInfo.phase;
    int x = paintInfo.rect.x();
    int y = paintInfo.rect.y();
    int w = paintInfo.rect.width();
    int h = paintInfo.rect.height();

    int os = 2 * maximalOutlineSize(paintPhase);
    unsigned startrow = 0;
    unsigned endrow = totalRows;
    
    // If some cell overflows, just paint all of them.
    if (!m_hasOverflowingCell) {
        int relativeY = y - ty;
        int top = relativeY - os;
        // binary search to find a row
        startrow = std::lower_bound(m_rowPos.begin(), m_rowPos.end(), top) - m_rowPos.begin();

        // The binary search above gives us the first row with
        // a y position >= the top of the paint rect. Thus, the previous
        // may need to be repainted as well.
        if (startrow == m_rowPos.size() || (startrow > 0 && (m_rowPos[startrow] >  top)))
          --startrow;

        int bottom = relativeY + h + os - 1;
        endrow = std::lower_bound(m_rowPos.begin(), m_rowPos.end(), bottom) - m_rowPos.begin();
        if ((endrow == m_rowPos.size()) || (endrow > 0 && m_rowPos[endrow - 1] == bottom))
          --endrow;

        if (!endrow && ty + m_rowPos[0] - table()->outerBorderTop() <= y + h + os)
            ++endrow;
    }
    unsigned startcol = 0;
    unsigned endcol = totalCols;
    // FIXME: Implement RTL.
    if (!m_hasOverflowingCell && style()->isLeftToRightDirection()) {
        int relativeX = x - tx;
        int left = relativeX - os;
        Vector<int>& columnPos = table()->columnPositions();
        startcol = std::lower_bound(columnPos.begin(), columnPos.end(), left) - columnPos.begin();
        if ((startcol == columnPos.size()) || (startcol > 0 && (columnPos[startcol] > left)))
            --startcol;

        int right = relativeX + w + os - 1;
        endcol = std::lower_bound(columnPos.begin(), columnPos.end(), right) - columnPos.begin();
        if (endcol == columnPos.size() || (endcol > 0 && (columnPos[endcol - 1] == right)))
            --endcol;

        if (!endcol && tx + table()->columnPositions()[0] - table()->outerBorderLeft() <= y + w + os)
            ++endcol;
    }
    if (startcol < endcol) {
        if (!m_hasMultipleCellLevels) {
            // Draw the dirty cells in the order that they appear.
            for (unsigned r = startrow; r < endrow; r++) {
                for (unsigned c = startcol; c < endcol; c++) {
                    CellStruct& current = cellAt(r, c);
                    RenderTableCell* cell = current.primaryCell();
                    if (!cell || (r > startrow && primaryCellAt(r - 1, c) == cell) || (c > startcol && primaryCellAt(r, c - 1) == cell))
                        continue;
                    paintCell(cell, paintInfo, tx, ty);
                }
            }
        } else {
            // Draw the cells in the correct paint order.
            Vector<RenderTableCell*> cells;
            HashSet<RenderTableCell*> spanningCells;
            for (unsigned r = startrow; r < endrow; r++) {
                for (unsigned c = startcol; c < endcol; c++) {
                    CellStruct& current = cellAt(r, c);
                    if (!current.hasCells())
                        continue;
                    for (unsigned i = 0; i < current.cells.size(); ++i) {
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
            std::stable_sort(cells.begin(), cells.end(), compareCellPositions);
            int size = cells.size();
            // Paint the cells.
            for (int i = 0; i < size; ++i)
                paintCell(cells[i], paintInfo, tx, ty);
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
    m_cCol = 0;
    m_cRow = -1;
    clearGrid();
    m_gridRows = 0;

    for (RenderObject* row = firstChild(); row; row = row->nextSibling()) {
        if (row->isTableRow()) {
            m_cRow++;
            m_cCol = 0;
            if (!ensureRows(m_cRow + 1))
                break;
            
            RenderTableRow* tableRow = toRenderTableRow(row);
            m_grid[m_cRow].rowRenderer = tableRow;
            setRowHeightToRowStyleHeightIfNotRelative(&m_grid[m_cRow]);

            for (RenderObject* cell = row->firstChild(); cell; cell = cell->nextSibling()) {
                if (cell->isTableCell())
                    addCell(toRenderTableCell(cell), tableRow);
            }
        }
    }
    m_needsCellRecalc = false;
    setNeedsLayout(true);
}

void RenderTableSection::clearGrid()
{
    int rows = m_gridRows;
    while (rows--)
        delete m_grid[rows].row;
}

int RenderTableSection::numColumns() const
{
    int result = 0;
    
    for (int r = 0; r < m_gridRows; ++r) {
        for (int c = result; c < table()->numEffCols(); ++c) {
            const CellStruct& cell = cellAt(r, c);
            if (cell.hasCells() || cell.inColSpan)
                result = c;
        }
    }
    
    return result + 1;
}

void RenderTableSection::appendColumn(int pos)
{
    for (int row = 0; row < m_gridRows; ++row)
        m_grid[row].row->resize(pos + 1);
}

void RenderTableSection::splitColumn(int pos, int first)
{
    if (m_cCol > pos)
        m_cCol++;
    for (int row = 0; row < m_gridRows; ++row) {
        Row& r = *m_grid[row].row;
        r.insert(pos + 1, CellStruct());
        if (r[pos].hasCells()) {
            r[pos + 1].cells.append(r[pos].cells);
            RenderTableCell* cell = r[pos].primaryCell();
            ASSERT(cell);
            int colleft = cell->colSpan() - r[pos].inColSpan;
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
bool RenderTableSection::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int xPos, int yPos, int tx, int ty, HitTestAction action)
{
    // If we have no children then we have nothing to do.
    if (!firstChild())
        return false;

    // Table sections cannot ever be hit tested.  Effectively they do not exist.
    // Just forward to our children always.
    tx += x();
    ty += y();

    if (hasOverflowClip() && !overflowClipRect(tx, ty).intersects(result.rectFromPoint(xPos, yPos)))
        return false;

    if (m_hasOverflowingCell) {
        for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
            // FIXME: We have to skip over inline flows, since they can show up inside table rows
            // at the moment (a demoted inline <form> for example). If we ever implement a
            // table-specific hit-test method (which we should do for performance reasons anyway),
            // then we can remove this check.
            if (child->isBox() && !toRenderBox(child)->hasSelfPaintingLayer() && child->nodeAtPoint(request, result, xPos, yPos, tx, ty, action)) {
                updateHitTestResult(result, IntPoint(xPos - tx, yPos - ty));
                return true;
            }
        }
        return false;
    }

    int relativeY = yPos - ty;
    // leftrow corresponds to the first row that starts after the y mouse position
    unsigned leftrow = std::upper_bound(m_rowPos.begin(), m_rowPos.end(), relativeY) - m_rowPos.begin();
    if (leftrow == m_rowPos.size())
        return false;
    // Grab the last row that starts before the y mouse position.
    if (leftrow > 0)
        --leftrow;

    Vector<int>& columnPos = table()->columnPositions();
    bool rtl = !style()->isLeftToRightDirection();
    int relativeX = xPos - tx;
    if (rtl)
        relativeX = columnPos[columnPos.size() - 1] - relativeX;

    unsigned leftcol = std::lower_bound(columnPos.begin(), columnPos.end(), relativeX) - columnPos.begin();
    if (leftcol == columnPos.size())
        return false;
    if (leftcol > 0)
        --leftcol;

    CellStruct& current = cellAt(leftrow, leftcol);

    // If the cell is empty, there's nothing to do
    if (!current.hasCells())
        return false;

    for (int i = current.cells.size() - 1; i >= 0; --i) {
        RenderTableCell* cell = current.cells[i];
        if (static_cast<RenderObject*>(cell)->nodeAtPoint(request, result, xPos, yPos, tx, ty, action)) {
            updateHitTestResult(result, IntPoint(xPos - tx, yPos - ty));
            return true;
        }
    }
    return false;

}

} // namespace WebCore
