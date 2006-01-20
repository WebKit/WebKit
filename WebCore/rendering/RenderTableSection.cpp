/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
#include "DocumentImpl.h"
#include "htmlnames.h"
#include <qtextstream.h>

namespace WebCore {

using namespace HTMLNames;

RenderTableSection::RenderTableSection(NodeImpl* node)
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

void RenderTableSection::addChild(RenderObject *child, RenderObject *beforeChild)
{
    RenderObject *row = child;

    if (child->element() && child->element()->hasTagName(formTag)) {
        RenderContainer::addChild(child,beforeChild);
        return;
    }
    
    if (!child->isTableRow()) {

        if (!beforeChild)
            beforeChild = lastChild();

        if (beforeChild && beforeChild->isAnonymous())
            row = beforeChild;
        else {
	    RenderObject *lastBox = beforeChild;
	    while (lastBox && lastBox->parent()->isAnonymous() && !lastBox->isTableRow())
		lastBox = lastBox->parent();
	    if (lastBox && lastBox->isAnonymous()) {
		lastBox->addChild( child, beforeChild );
		return;
	    } else {
		row = new (renderArena()) RenderTableRow(document() /* anonymous table */);
		RenderStyle *newStyle = new (renderArena()) RenderStyle();
		newStyle->inheritFrom(style());
		newStyle->setDisplay(TABLE_ROW);
		row->setStyle(newStyle);
		addChild(row, beforeChild);
	    }
        }
        row->addChild(child);
        child->setNeedsLayoutAndMinMaxRecalc();
        return;
    }

    if (beforeChild)
	setNeedCellRecalc();

    cRow++;
    cCol = 0;

    ensureRows(cRow+1);

    if (!beforeChild) {
        grid[cRow].height = child->style()->height();
        if (grid[cRow].height.type == Relative)
            grid[cRow].height = Length();
    }


    RenderContainer::addChild(child,beforeChild);
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
	    grid[r].baseLine = 0;
	    grid[r].height = Length();
	}
    }

    return true;
}

void RenderTableSection::addCell(RenderTableCell *cell)
{
    int rSpan = cell->rowSpan();
    int cSpan = cell->colSpan();
    Array<RenderTable::ColumnStruct> &columns = table()->columns;
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
	if (height.value > 0 || (height.type == Relative && height.value >= 0)) {
	    Length cRowHeight = grid[cRow].height;
	    switch (height.type) {
                case Percent:
                    if (!(cRowHeight.type == Percent) ||
                        (cRowHeight.type == Percent && cRowHeight.value < height.value))
                        grid[cRow].height = height;
                        break;
                case Fixed:
                    if (cRowHeight.type < Percent ||
                        (cRowHeight.type == Fixed && cRowHeight.value < height.value))
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
    Array<int> &columnPos = table()->columnPos;

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
		cell->setNeedsLayout(true);
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
	int ch = grid[r].height.minWidth(0);
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
	    ch = cell->style()->height().width(0) + 
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
    
    if (toAdd && totalRows && (rowPos[totalRows] || !nextSibling())) {

	int totalHeight = rowPos[totalRows] + toAdd;

        int dh = toAdd;
	int totalPercent = 0;
	int numAuto = 0;
	for (int r = 0; r < totalRows; r++) {
	    if (grid[r].height.type == Auto)
		numAuto++;
	    else if (grid[r].height.type == Percent)
		totalPercent += grid[r].height.value;
	}
	if (totalPercent) {
	    // try to satisfy percent
	    int add = 0;
	    if (totalPercent > 100)
		totalPercent = 100;
	    int rh = rowPos[1] - rowPos[0];
	    for (int r = 0; r < totalRows; r++) {
		if (totalPercent > 0 && grid[r].height.type == Percent) {
		    int toAdd = kMin(dh, (totalHeight * grid[r].height.value / 100) - rh);
                    // If toAdd is negative, then we don't want to shrink the row (this bug
                    // affected Outlook Web Access).
                    toAdd = kMax(0, toAdd);
		    add += toAdd;
		    dh -= toAdd;
		    totalPercent -= grid[r].height.value;
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
		if (numAuto > 0 && grid[r].height.type == Auto) {
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


void RenderTableSection::paint(PaintInfo& i, int tx, int ty)
{
    unsigned int totalRows = gridRows;
    unsigned int totalCols = table()->columns.size();

    tx += m_x;
    ty += m_y;

    // check which rows and cols are visible and only paint these
    // ### fixme: could use a binary search here
    PaintAction paintAction = i.phase;
    int x = i.r.x();
    int y = i.r.y();
    int w = i.r.width();
    int h = i.r.height();

    int os = 2 * maximalOutlineSize(paintAction);
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
		
                if (!cell || (cell->layer() && i.phase != PaintActionCollapsedTableBorders)) 
		    continue;
                
                // Cells must always paint in the order in which they appear taking into account
                // their upper left originating row/column.  For cells with rowspans, avoid repainting
                // if we've already seen the cell.
		if (r > startrow && (cellAt(r-1, c).cell == cell))
		    continue;

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
	cRow++;
	cCol = 0;
	ensureRows(cRow + 1);
	for (RenderObject *cell = row->firstChild(); cell; cell = cell->nextSibling())
	    if (cell->isTableCell())
		addCell(static_cast<RenderTableCell *>(cell));
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

RenderObject* RenderTableSection::removeChildNode(RenderObject* child)
{
    setNeedCellRecalc();
    return RenderContainer::removeChildNode(child);
}

#ifndef NDEBUG
void RenderTableSection::dump(QTextStream *stream, QString ind) const
{
    *stream << endl << ind << "grid=(" << grid.size() << "," << table()->numEffCols() << ")" << endl << ind;
    for (unsigned int r = 0; r < grid.size(); r++) {
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
