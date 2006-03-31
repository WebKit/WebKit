/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "RenderTableSection.h"
#include "RenderTableCell.h"
#include "RenderTableRow.h"
#include "RenderTableCol.h"
#include "Document.h"
#include "HTMLNames.h"
#include <qtextstream.h>

namespace WebCore {

using namespace HTMLNames;

RenderTableSection::RenderTableSection(Node* node)
    : RenderContainer(node)
{
    // init RenderObject attributes
    setInline(false);   // our object is not Inline
    gridRows = 0;
    cCol = 0;
    cRow = -1;
    needCellRecalc = false;
}

RenderTableSection::~RenderTableSection()
{
    clearGrid();
}

void RenderTableSection::destroy()
{
    // recalc cell info because RenderTable has unguarded pointers
    // stored that point to this RenderTableSection.
    if (table())
        table()->setNeedSectionRecalc();

    RenderContainer::destroy();
}

void RenderTableSection::setStyle(RenderStyle* _style)
{
    // we don't allow changing this one
    if (style())
        _style->setDisplay(style()->display());
    else if (_style->display() != TABLE_FOOTER_GROUP && _style->display() != TABLE_HEADER_GROUP)
        _style->setDisplay(TABLE_ROW_GROUP);

    RenderContainer::setStyle(_style);
}

void RenderTableSection::addChild(RenderObject* child, RenderObject* beforeChild)
{
    bool isTableSection = element() && (element()->hasTagName(theadTag) || element()->hasTagName(tbodyTag) || element()->hasTagName(tfootTag));

    if (!child->isTableRow()) {
        if (isTableSection && child->element() && child->element()->hasTagName(formTag)) {
            RenderContainer::addChild(child, beforeChild);
            return;
        }

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

        RenderObject* row = new (renderArena()) RenderTableRow(document() /* anonymous table */);
        RenderStyle* newStyle = new (renderArena()) RenderStyle();
        newStyle->inheritFrom(style());
        newStyle->setDisplay(TABLE_ROW);
        row->setStyle(newStyle);
        addChild(row, beforeChild);
        row->addChild(child);
        return;
    }

    if (beforeChild)
        setNeedCellRecalc();

    ++cRow;
    cCol = 0;

    ensureRows(cRow + 1);

    if (!beforeChild) {
        grid[cRow].height = child->style()->height();
        if (grid[cRow].height.isRelative())
            grid[cRow].height = Length();
    }

    RenderContainer::addChild(child, beforeChild);
}

bool RenderTableSection::ensureRows(int numRows)
{
    int nRows = gridRows;
    if (numRows > nRows) {
        if (numRows > static_cast<int>(grid.size()))
            if (!grid.resize(numRows*2+1))
                return false;

        gridRows = numRows;
        int nCols = table()->numEffCols();
        CellStruct emptyCellStruct;
        emptyCellStruct.cell = 0;
        emptyCellStruct.inColSpan = false;
        for (int r = nRows; r < numRows; r++) {
            grid[r].row = new Row(nCols);
            grid[r].row->fill(emptyCellStruct);
            grid[r].rowRenderer = 0;
            grid[r].baseLine = 0;
            grid[r].height = Length();
        }
    }

    return true;
}

void RenderTableSection::addCell(RenderTableCell *cell, RenderObject* row)
{
    int rSpan = cell->rowSpan();
    int cSpan = cell->colSpan();
    DeprecatedArray<RenderTable::ColumnStruct> &columns = table()->columns;
    int nCols = columns.size();

    // ### mozilla still seems to do the old HTML way, even for strict DTD
    // (see the annotation on table cell layouting in the CSS specs and the testcase below:
    // <TABLE border>
    // <TR><TD>1 <TD rowspan="2">2 <TD>3 <TD>4
    // <TR><TD colspan="2">5
    // </TABLE>

    while (cCol < nCols && (cellAt(cRow, cCol).cell || cellAt(cRow, cCol).inColSpan))
        cCol++;

    if (rSpan == 1) {
        // we ignore height settings on rowspan cells
        Length height = cell->style()->height();
        if (height.value() > 0 || (height.isRelative() && height.value() >= 0)) {
            Length cRowHeight = grid[cRow].height;
            switch (height.type()) {
                case Percent:
                    if (!(cRowHeight.isPercent()) ||
                        (cRowHeight.isPercent() && cRowHeight.value() < height.value()))
                        grid[cRow].height = height;
                        break;
                case Fixed:
                    if (cRowHeight.type() < Percent ||
                        (cRowHeight.isFixed() && cRowHeight.value() < height.value()))
                        grid[cRow].height = height;
                    break;
                case Relative:
                default:
                    break;
            }
        }
    }

    // make sure we have enough rows
    if (!ensureRows(cRow + rSpan))
        return;

    grid[cRow].rowRenderer = row;

    int col = cCol;
    // tell the cell where it is
    CellStruct currentCell;
    currentCell.cell = cell;
    currentCell.inColSpan = false;
    while (cSpan) {
        int currentSpan;
        if (cCol >= nCols) {
            table()->appendColumn(cSpan);
            currentSpan = cSpan;
        } else {
            if (cSpan < columns[cCol].span)
                table()->splitColumn(cCol, cSpan);
            currentSpan = columns[cCol].span;
        }

        for (int r = 0; r < rSpan; r++) {
            CellStruct& c = cellAt(cRow + r, cCol);
            if (currentCell.cell && !c.cell)
                c.cell = currentCell.cell;
            if (currentCell.inColSpan)
                c.inColSpan = true;
        }
        cCol++;
        cSpan -= currentSpan;
        currentCell.cell = 0;
        currentCell.inColSpan = true;
    }
    if (cell) {
        cell->setRow(cRow);
        cell->setCol(table()->effColToCol(col));
    }
}



void RenderTableSection::setCellWidths()
{
    DeprecatedArray<int> &columnPos = table()->columnPos;

    int rows = gridRows;
    for (int i = 0; i < rows; i++) {
        Row &row = *grid[i].row;
        int cols = row.size();
        for (int j = 0; j < cols; j++) {
            CellStruct current = row[j];
            RenderTableCell *cell = current.cell;

            if (!cell)
                continue;
            int endCol = j;
            int cspan = cell->colSpan();
            while (cspan && endCol < cols) {
                cspan -= table()->columns[endCol].span;
                endCol++;
            }
            int w = columnPos[endCol] - columnPos[j] - table()->hBorderSpacing();
            int oldWidth = cell->width();
            if (w != oldWidth) {
                bool neededLayout = cell->selfNeedsLayout();
                cell->setNeedsLayout(true);
                if (!neededLayout && !selfNeedsLayout() && cell->checkForRepaintDuringLayout())
                    cell->repaintObjectsBeforeLayout();
                cell->setWidth(w);
            }
        }
    }
}


void RenderTableSection::calcRowHeight()
{
    int indx;
    RenderTableCell *cell;

    int totalRows = gridRows;
    int spacing = table()->vBorderSpacing();

    rowPos.resize(totalRows + 1);
    rowPos[0] = spacing;

    for (int r = 0; r < totalRows; r++) {
        rowPos[r + 1] = 0;

        int baseline = 0;
        int bdesc = 0;
        int ch = grid[r].height.calcMinValue(0);
        int pos = rowPos[r + 1] + ch + spacing;

        if (pos > rowPos[r + 1])
            rowPos[r + 1] = pos;

        Row *row = grid[r].row;
        int totalCols = row->size();
        int totalRows = gridRows;

        for (int c = 0; c < totalCols; c++) {
            CellStruct current = cellAt(r, c);
            cell = current.cell;
            if (!cell || current.inColSpan)
                continue;
            if (r < totalRows - 1 && cellAt(r + 1, c).cell == cell)
                continue;

            if ((indx = r - cell->rowSpan() + 1) < 0)
                indx = 0;

            if (cell->overrideSize() != -1) {
                cell->setOverrideSize(-1);
                cell->setChildNeedsLayout(true, false);
                cell->layoutIfNeeded();
            }
            
            // Explicit heights use the border box in quirks mode.  In strict mode do the right
            // thing and actually add in the border and padding.
            ch = cell->style()->height().calcValue(0) + 
                (cell->style()->htmlHacks() ? 0 : (cell->paddingTop() + cell->paddingBottom() +
                                                   cell->borderTop() + cell->borderBottom()));
            if (cell->height() > ch)
                ch = cell->height();

            pos = rowPos[ indx ] + ch + spacing;

            if (pos > rowPos[r + 1])
                rowPos[r + 1] = pos;

            // find out the baseline
            EVerticalAlign va = cell->style()->verticalAlign();
            if (va == BASELINE || va == TEXT_BOTTOM || va == TEXT_TOP
                || va == SUPER || va == SUB) {
                int b = cell->baselinePosition();
                if (b > cell->borderTop() + cell->paddingTop()) {
                    if (b > baseline)
                        baseline = b;

                    int td = rowPos[indx] + ch - b;
                    if (td > bdesc)
                        bdesc = td;
                }
            }
        }

        //do we have baseline aligned elements?
        if (baseline) {
            // increase rowheight if baseline requires
            int bRowPos = baseline + bdesc  + spacing ; // + 2*padding
            if (rowPos[r + 1] < bRowPos)
                rowPos[r + 1] = bRowPos;

            grid[r].baseLine = baseline;
        }

        if (rowPos[r + 1] < rowPos[r])
            rowPos[r + 1] = rowPos[r];
    }
}

int RenderTableSection::layoutRows(int toAdd)
{
    int rHeight;
    int rindx;
    int totalRows = gridRows;
    int hspacing = table()->hBorderSpacing();
    int vspacing = table()->vBorderSpacing();
    
    // Set the width of our section now.  The rows will also be this width.
    m_width = table()->contentWidth();
    
    if (toAdd && totalRows && (rowPos[totalRows] || !nextSibling())) {

        int totalHeight = rowPos[totalRows] + toAdd;

        int dh = toAdd;
        int totalPercent = 0;
        int numAuto = 0;
        for (int r = 0; r < totalRows; r++) {
            if (grid[r].height.isAuto())
                numAuto++;
            else if (grid[r].height.isPercent())
                totalPercent += grid[r].height.value();
        }
        if (totalPercent) {
            // try to satisfy percent
            int add = 0;
            if (totalPercent > 100)
                totalPercent = 100;
            int rh = rowPos[1] - rowPos[0];
            for (int r = 0; r < totalRows; r++) {
                if (totalPercent > 0 && grid[r].height.isPercent()) {
                    int toAdd = kMin(dh, (totalHeight * grid[r].height.value() / 100) - rh);
                    // If toAdd is negative, then we don't want to shrink the row (this bug
                    // affected Outlook Web Access).
                    toAdd = kMax(0, toAdd);
                    add += toAdd;
                    dh -= toAdd;
                    totalPercent -= grid[r].height.value();
                }
                if (r < totalRows - 1)
                    rh = rowPos[r + 2] - rowPos[r + 1];
                rowPos[r + 1] += add;
            }
        }
        if (numAuto) {
            // distribute over variable cols
            int add = 0;
            for (int r = 0; r < totalRows; r++) {
                if (numAuto > 0 && grid[r].height.isAuto()) {
                    int toAdd = dh/numAuto;
                    add += toAdd;
                    dh -= toAdd;
                    numAuto--;
                }
                rowPos[r + 1] += add;
            }
        }
        if (dh > 0 && rowPos[totalRows]) {
            // if some left overs, distribute equally.
            int tot = rowPos[totalRows];
            int add = 0;
            int prev = rowPos[0];
            for (int r = 0; r < totalRows; r++) {
                //weight with the original height
                add += dh * (rowPos[r + 1] - prev) / tot;
                prev = rowPos[r + 1];
                rowPos[r + 1] += add;
            }
        }
    }

    int leftOffset = hspacing;

    int nEffCols = table()->numEffCols();
    for (int r = 0; r < totalRows; r++) {
        Row *row = grid[r].row;
        int totalCols = row->size();
        
        // Set the row's x/y position and width/height.
        if (grid[r].rowRenderer) {
            grid[r].rowRenderer->setPos(0, rowPos[r]);
            grid[r].rowRenderer->setWidth(m_width);
            grid[r].rowRenderer->setHeight(rowPos[r+1] - rowPos[r] - vspacing);
        }

        for (int c = 0; c < nEffCols; c++) {
            CellStruct current = cellAt(r, c);
            RenderTableCell* cell = current.cell;
            
            if (!cell)
                continue;
            if (r < totalRows - 1 && cell == cellAt(r + 1, c).cell)
                continue;

            if ((rindx = r-cell->rowSpan() + 1) < 0)
                rindx = 0;

            rHeight = rowPos[r + 1] - rowPos[rindx] - vspacing;
            
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
            bool flexAllChildren = cell->style()->height().isFixed() || 
                (!table()->style()->height().isAuto() && rHeight != cell->height());

            for (RenderObject* o = cell->firstChild(); o; o = o->nextSibling()) {
                if (!o->isText() && o->style()->height().isPercent() && (o->isReplaced() || o->scrollsOverflow() || flexAllChildren)) {
                    // Tables with no sections do not flex.
                    if (!o->isTable() || static_cast<RenderTable*>(o)->hasSections()) {
                        o->setNeedsLayout(true, false);
                        cell->setChildNeedsLayout(true, false);
                        cellChildrenFlex = true;
                    }
                }
            }
            if (cellChildrenFlex) {
                cell->setOverrideSize(kMax(0, 
                                           rHeight - cell->borderTop() - cell->paddingTop() - 
                                                     cell->borderBottom() - cell->paddingBottom()));
                cell->layoutIfNeeded();

                // Alignment within a cell is based off the calculated
                // height, which becomes irrelevant once the cell has
                // been resized based off its percentage. -dwh
                cell->setCellTopExtra(0);
                cell->setCellBottomExtra(0);
            }
            else {
                EVerticalAlign va = cell->style()->verticalAlign();
                int te = 0;
                switch (va) {
                    case SUB:
                    case SUPER:
                    case TEXT_TOP:
                    case TEXT_BOTTOM:
                    case BASELINE:
                        te = getBaseline(r) - cell->baselinePosition() ;
                        break;
                    case TOP:
                        te = 0;
                        break;
                    case MIDDLE:
                        te = (rHeight - cell->height())/2;
                        break;
                    case BOTTOM:
                        te = rHeight - cell->height();
                        break;
                    default:
                        break;
                }
                
                cell->setCellTopExtra(te);
                cell->setCellBottomExtra(rHeight - cell->height() - te);
            }
            
            int oldCellX = cell->xPos();
            int oldCellY = cell->yPos();
        
            if (style()->direction() == RTL) {
                cell->setPos(
                    table()->columnPos[(int)totalCols] -
                    table()->columnPos[table()->colToEffCol(cell->col()+cell->colSpan())] +
                    leftOffset,
                    rowPos[rindx]);
            } else
                cell->setPos(table()->columnPos[c] + leftOffset, rowPos[rindx]);

            // If the cell moved, we have to repaint it as well as any floating/positioned
            // descendants.  An exception is if we need a layout.  In this case, we know we're going to
            // repaint ourselves (and the cell) anyway.
            if (!table()->selfNeedsLayout() && cell->checkForRepaintDuringLayout())
                cell->repaintDuringLayoutIfMoved(oldCellX, oldCellY);
        }
    }

    m_height = rowPos[totalRows];
    return m_height;
}

int RenderTableSection::lowestPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int bottom = RenderContainer::lowestPosition(includeOverflowInterior, includeSelf);
    if (!includeOverflowInterior && hasOverflowClip())
        return bottom;

    for (RenderObject *row = firstChild(); row; row = row->nextSibling()) {
        for (RenderObject *cell = row->firstChild(); cell; cell = cell->nextSibling())
            if (cell->isTableCell()) {
                int bp = cell->yPos() + cell->lowestPosition(false);
                bottom = kMax(bottom, bp);
        }
    }
    
    return bottom;
}

int RenderTableSection::rightmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int right = RenderContainer::rightmostPosition(includeOverflowInterior, includeSelf);
    if (!includeOverflowInterior && hasOverflowClip())
        return right;

    for (RenderObject *row = firstChild(); row; row = row->nextSibling()) {
        for (RenderObject *cell = row->firstChild(); cell; cell = cell->nextSibling())
            if (cell->isTableCell()) {
                int rp = cell->xPos() + cell->rightmostPosition(false);
                right = kMax(right, rp);
        }
    }
    
    return right;
}

int RenderTableSection::leftmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int left = RenderContainer::leftmostPosition(includeOverflowInterior, includeSelf);
    if (!includeOverflowInterior && hasOverflowClip())
        return left;
    
    for (RenderObject *row = firstChild(); row; row = row->nextSibling()) {
        for (RenderObject *cell = row->firstChild(); cell; cell = cell->nextSibling())
            if (cell->isTableCell()) {
                int lp = cell->xPos() + cell->leftmostPosition(false);
                left = kMin(left, lp);
        }
    }
    
    return left;
}

void RenderTableSection::paint(PaintInfo& i, int tx, int ty)
{
    unsigned int totalRows = gridRows;
    unsigned int totalCols = table()->columns.size();

    tx += m_x;
    ty += m_y;

    // check which rows and cols are visible and only paint these
    // ### fixme: could use a binary search here
    PaintPhase paintPhase = i.phase;
    int x = i.r.x();
    int y = i.r.y();
    int w = i.r.width();
    int h = i.r.height();

    int os = 2 * maximalOutlineSize(paintPhase);
    unsigned int startrow = 0;
    unsigned int endrow = totalRows;
    for (; startrow < totalRows; startrow++)
        if (ty + rowPos[startrow+1] >= y - os)
            break;

    for (; endrow > 0; endrow--)
        if ( ty + rowPos[endrow-1] <= y + h + os)
            break;

    unsigned int startcol = 0;
    unsigned int endcol = totalCols;
    if (style()->direction() == LTR) {
        for (; startcol < totalCols; startcol++) {
            if (tx + table()->columnPos[startcol + 1] >= x - os)
                break;
        }
        for (; endcol > 0; endcol--) {
            if (tx + table()->columnPos[endcol - 1] <= x + w + os)
                break;
        }
    }

    if (startcol < endcol) {
        // draw the cells
        for (unsigned int r = startrow; r < endrow; r++) {
            unsigned int c = startcol;
            // since a cell can be -1 (indicating a colspan) we might have to search backwards to include it
            while (c && cellAt(r, c).inColSpan)
                c--;
            for (; c < endcol; c++) {
                CellStruct current = cellAt(r, c);
                RenderTableCell *cell = current.cell;
                    
                // Cells must always paint in the order in which they appear taking into account
                // their upper left originating row/column.  For cells with rowspans, avoid repainting
                // if we've already seen the cell.
                if (!cell || (r > startrow && (cellAt(r-1, c).cell == cell)))
                    continue;

                if (paintPhase == PaintPhaseBlockBackground || paintPhase == PaintPhaseChildBlockBackground) {
                    // We need to handle painting a stack of backgrounds.  This stack (from bottom to top) consists of
                    // the column group, column, row group, row, and then the cell.
                    RenderObject* col = table()->colElement(c);
                    RenderObject* colGroup = 0;
                    if (col) {
                        RenderStyle *style = col->parent()->style();
                        if (style->display() == TABLE_COLUMN_GROUP)
                            colGroup = col->parent();
                    }
                    RenderObject* row = cell->parent();
                    
                    // Column groups and columns first.
                    // FIXME: Columns and column groups do not currently support opacity, and they are being painted "too late" in
                    // the stack, since we have already opened a transparency layer (potentially) for the table row group.
                    // Note that we deliberately ignore whether or not the cell has a layer, since these backgrounds paint "behind" the
                    // cell.
                    cell->paintBackgroundsBehindCell(i, tx, ty, colGroup);
                    cell->paintBackgroundsBehindCell(i, tx, ty, col);
                    
                    // Paint the row group next.
                    cell->paintBackgroundsBehindCell(i, tx, ty, this);
                    
                    // Paint the row next, but only if it doesn't have a layer.  If a row has a layer, it will be responsible for
                    // painting the row background for the cell.
                    if (!row->layer())
                        cell->paintBackgroundsBehindCell(i, tx, ty, row);
                }

                if ((!cell->layer() && !cell->parent()->layer()) || i.phase == PaintPhaseCollapsedTableBorders)
                    cell->paint(i, tx, ty);
            }
        }
    }
}

void RenderTableSection::recalcCells()
{
    cCol = 0;
    cRow = -1;
    clearGrid();
    gridRows = 0;

    for (RenderObject *row = firstChild(); row; row = row->nextSibling()) {
        if (row->isTableRow()) {
            cRow++;
            cCol = 0;
            ensureRows(cRow + 1);
            for (RenderObject *cell = row->firstChild(); cell; cell = cell->nextSibling())
                if (cell->isTableCell())
                    addCell(static_cast<RenderTableCell *>(cell), row);
        }
    }
    needCellRecalc = false;
    setNeedsLayout(true);
}

void RenderTableSection::clearGrid()
{
    int rows = gridRows;
    while (rows--)
        delete grid[rows].row;
}

int RenderTableSection::numColumns() const
{
    int result = 0;
    
    for (int r = 0; r < gridRows; ++r) {
        for (int c = result; c < table()->numEffCols(); ++c) {
            const CellStruct& cell = cellAt(r, c);
            if (cell.cell || cell.inColSpan)
                result = c;
        }
    }
    
    return result + 1;
}

RenderObject* RenderTableSection::removeChildNode(RenderObject* child)
{
    setNeedCellRecalc();
    return RenderContainer::removeChildNode(child);
}

// Hit Testing
bool RenderTableSection::nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty, HitTestAction action)
{
    // Table sections cannot ever be hit tested.  Effectively they do not exist.
    // Just forward to our children always.
    tx += m_x;
    ty += m_y;

    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        // FIXME: We have to skip over inline flows, since they can show up inside table rows
        // at the moment (a demoted inline <form> for example). If we ever implement a
        // table-specific hit-test method (which we should do for performance reasons anyway),
        // then we can remove this check.
        if (!child->layer() && !child->isInlineFlow() && child->nodeAtPoint(info, x, y, tx, ty, action)) {
            setInnerNode(info);
            return true;
        }
    }
    
    return false;
}

#ifndef NDEBUG
void RenderTableSection::dump(QTextStream *stream, DeprecatedString ind) const
{
    *stream << endl << ind << "grid=(" << gridRows << "," << table()->numEffCols() << ")" << endl << ind;
    for (int r = 0; r < gridRows; r++) {
        for (int c = 0; c < table()->numEffCols(); c++) {
            if (cellAt( r, c).cell && !cellAt(r, c).inColSpan)
                *stream << "(" << cellAt(r, c).cell->row() << "," << cellAt(r, c).cell->col() << ","
                        << cellAt(r, c).cell->rowSpan() << "," << cellAt(r, c).cell->colSpan() << ") ";
            else
                *stream << cellAt(r, c).cell << "null cell ";
        }
        *stream << endl << ind;
    }
    RenderContainer::dump(stream,ind);
}
#endif

}
