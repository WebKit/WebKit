/*
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
#ifndef RenderTableSection_H
#define RenderTableSection_H

#include "RenderContainer.h"
#include "RenderTable.h"

namespace WebCore {

class RenderTableCell;

class RenderTableSection : public RenderContainer
{
public:
    RenderTableSection(WebCore::Node* node);
    ~RenderTableSection();
    virtual void destroy();

    virtual void setStyle(RenderStyle *style);

    virtual const char *renderName() const { return "RenderTableSection"; }

    // overrides
    virtual void addChild(RenderObject *child, RenderObject *beforeChild = 0);
    virtual bool isTableSection() const { return true; }

    virtual short lineHeight(bool) const { return 0; }
    virtual void position(int, int, int, int, int, bool, bool, int) {}

#ifndef NDEBUG
    virtual void dump(QTextStream *stream, DeprecatedString ind = "") const;
#endif

    void addCell(RenderTableCell *cell, RenderObject* row);

    void setCellWidths();
    void calcRowHeight();
    int layoutRows(int height);

    RenderTable *table() const { return static_cast<RenderTable *>(parent()); }

    struct CellStruct {
        RenderTableCell *cell;
        bool inColSpan; // true for columns after the first in a colspan
    };
    typedef DeprecatedArray<CellStruct> Row;
    struct RowStruct {
        Row* row;
        RenderObject* rowRenderer;
        int baseLine;
        Length height;
    };

    CellStruct& cellAt(int row,  int col) {
        return (*grid[row].row)[col];
    }
    const CellStruct& cellAt(int row, int col) const {
        return (*grid[row].row)[col];
    }

    virtual int lowestPosition(bool includeOverflowInterior, bool includeSelf) const;
    virtual int rightmostPosition(bool includeOverflowInterior, bool includeSelf) const;
    virtual int leftmostPosition(bool includeOverflowInterior, bool includeSelf) const;
    virtual void paint(PaintInfo& i, int tx, int ty);

    int numRows() const { return gridRows; }
    int numColumns() const;
    int getBaseline(int row) {return grid[row].baseLine;}

    void setNeedCellRecalc() {
        needCellRecalc = true;
        table()->setNeedSectionRecalc();
    }

    virtual RenderObject* removeChildNode(RenderObject* child);

    virtual bool nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty, HitTestAction action);

    // this gets a cell grid data structure. changing the number of
    // columns is done by the table
    DeprecatedArray<RowStruct> grid;
    int gridRows;
    DeprecatedArray<int> rowPos;

    // the current insertion position
    int cCol;
    int cRow;
    bool needCellRecalc;

    void recalcCells();
protected:
    bool ensureRows(int numRows);
    void clearGrid();
};

}
#endif
