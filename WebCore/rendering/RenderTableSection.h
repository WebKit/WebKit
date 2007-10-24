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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef RenderTableSection_h
#define RenderTableSection_h

#include "RenderTable.h"
#include <wtf/Vector.h>

namespace WebCore {

class RenderTableCell;

class RenderTableSection : public RenderContainer {
public:
    RenderTableSection(Node*);
    ~RenderTableSection();

    virtual const char* renderName() const { return isAnonymous() ? "RenderTableSection (anonymous)" : "RenderTableSection"; }

    virtual bool isTableSection() const { return true; }

    virtual void destroy();

    virtual void setStyle(RenderStyle*);

    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0);

    virtual short lineHeight(bool firstLine, bool isRootLineBox = false) const { return 0; }
    virtual void position(InlineBox*) { }

    void addCell(RenderTableCell*, RenderObject* row);

    void setCellWidths();
    void calcRowHeight();
    int layoutRows(int height);

    RenderTable* table() const { return static_cast<RenderTable*>(parent()); }

    struct CellStruct {
        RenderTableCell* cell;
        bool inColSpan; // true for columns after the first in a colspan
    };

    typedef Vector<CellStruct> Row;

    struct RowStruct {
        Row* row;
        RenderObject* rowRenderer;
        int baseline;
        Length height;
    };

    CellStruct& cellAt(int row,  int col) { return (*m_grid[row].row)[col]; }
    const CellStruct& cellAt(int row, int col) const { return (*m_grid[row].row)[col]; }

    void appendColumn(int pos);
    void splitColumn(int pos, int newSize);

    virtual int overflowWidth(bool includeInterior = true) const { return (!includeInterior && hasOverflowClip()) ? m_width : m_overflowWidth; }
    virtual int overflowLeft(bool includeInterior = true) const { return (!includeInterior && hasOverflowClip()) ? 0 : m_overflowLeft; }
    virtual int overflowHeight(bool includeInterior = true) const { return (!includeInterior && hasOverflowClip()) ? m_height : m_overflowHeight; }
    virtual int overflowTop(bool includeInterior = true) const { return (!includeInterior && hasOverflowClip()) ? 0 : m_overflowTop; }

    virtual int lowestPosition(bool includeOverflowInterior, bool includeSelf) const;
    virtual int rightmostPosition(bool includeOverflowInterior, bool includeSelf) const;
    virtual int leftmostPosition(bool includeOverflowInterior, bool includeSelf) const;

    int calcOuterBorderTop() const;
    int calcOuterBorderBottom() const;
    int calcOuterBorderLeft(bool rtl) const;
    int calcOuterBorderRight(bool rtl) const;
    void recalcOuterBorder();

    int outerBorderTop() const { return m_outerBorderTop; }
    int outerBorderBottom() const { return m_outerBorderBottom; }
    int outerBorderLeft() const { return m_outerBorderLeft; }
    int outerBorderRight() const { return m_outerBorderRight; }

    virtual void paint(PaintInfo&, int tx, int ty);
    virtual void imageChanged(CachedImage*);

    int numRows() const { return m_gridRows; }
    int numColumns() const;
    void recalcCells();
    void recalcCellsIfNeeded()
    {
        if (m_needsCellRecalc)
            recalcCells();
    }

    bool needsCellRecalc() const { return m_needsCellRecalc; }
    void setNeedsCellRecalc()
    {
        m_needsCellRecalc = true;
        table()->setNeedsSectionRecalc();
    }

    int getBaseline(int row) { return m_grid[row].baseline; }

    virtual RenderObject* removeChildNode(RenderObject*, bool fullRemove = true);

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

#ifndef NDEBUG
    virtual void dump(TextStream*, DeprecatedString ind = "") const;
#endif

protected:
    bool ensureRows(int);
    void clearGrid();

    Vector<RowStruct> m_grid;
    int m_gridRows;
    Vector<int> m_rowPos;

    // the current insertion position
    int m_cCol;
    int m_cRow;
    bool m_needsCellRecalc;

    int m_outerBorderLeft;
    int m_outerBorderRight;
    int m_outerBorderTop;
    int m_outerBorderBottom;
    int m_overflowLeft;
    int m_overflowWidth;
    int m_overflowTop;
    int m_overflowHeight;
    bool m_hasOverflowingCell;
};

} // namespace WebCore

#endif // RenderTableSection_h
