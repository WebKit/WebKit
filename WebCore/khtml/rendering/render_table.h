/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
#ifndef RENDER_TABLE_H
#define RENDER_TABLE_H

#include <qcolor.h>

#include "render_block.h"
#include "render_style.h"
#include "misc/khtmllayout.h"

namespace DOM {
    class DOMString;
};

namespace khtml {

class RenderTable;
class RenderTableSection;
class RenderTableRow;
class RenderTableCell;
class RenderTableCol;
class TableLayout;

class RenderTable : public RenderBlock
{
public:
    enum Rules {
        None    = 0x00,
        RGroups = 0x01,
        CGroups = 0x02,
        Groups  = 0x03,
        Rows    = 0x05,
        Cols    = 0x0a,
        All     = 0x0f
    };
    enum Frame {
        Void   = 0x00,
        Above  = 0x01,
        Below  = 0x02,
        Lhs    = 0x04,
        Rhs    = 0x08,
        Hsides = 0x03,
        Vsides = 0x0c,
        Box    = 0x0f
    };

    RenderTable(DOM::NodeImpl* node);
    ~RenderTable();

    virtual const char *renderName() const { return "RenderTable"; }

    virtual void setStyle(RenderStyle *style);

    virtual bool isTable() const { return true; }

    int getColumnPos(int col) const
        { return columnPos[col]; }

    int hBorderSpacing() const { return hspacing; }
    int vBorderSpacing() const { return vspacing; }
    
    bool collapseBorders() const { return style()->borderCollapse(); }
    int borderLeft() const;
    int borderRight() const;
    int borderTop() const;
    int borderBottom() const;
    
    Rules getRules() const { return rules; }

    const QColor &bgColor() const { return style()->backgroundColor(); }

    uint cellPadding() const { return padding; }
    void setCellPadding(uint p) { padding = p; }

    // overrides
    virtual int overflowHeight(bool includeInterior = true) const { return height(); }
    virtual int overflowWidth(bool includeInterior = true) const { return width(); }
    virtual void addChild(RenderObject *child, RenderObject *beforeChild = 0);
    virtual void paint(PaintInfo& i, int tx, int ty);
    virtual void paintBoxDecorations(PaintInfo& i, int _tx, int _ty);
    virtual void layout();
    virtual void calcMinMaxWidth();

    virtual RenderBlock* firstLineBlock() const;
    virtual void updateFirstLetter();
    
    virtual void setCellWidths();

    virtual void calcWidth();

#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif
    struct ColumnStruct {
	enum {
	    WidthUndefined = 0xffff
	};
	ColumnStruct() {
	    span = 1;
	    width = WidthUndefined;
	}
	ushort span;
	uint width; // the calculated position of the column
    };

    Array<int> columnPos;
    Array<ColumnStruct> columns;

    void splitColumn(int pos, int firstSpan);
    void appendColumn(int span);
    int numEffCols() const { return columns.size(); }
    int spanOfEffCol(int effCol) const { return columns[effCol].span; }
    int colToEffCol(int col) const {
	int c = 0;
	int i = 0;
	while (c < col && i < (int)columns.size()) {
	    c += columns[i].span;
	    i++;
	}
	return i;
    }
    int effColToCol(int effCol) const {
	int c = 0;
	for (int i = 0; i < effCol; i++)
	    c += columns[i].span;
	return c;
    }

    int bordersPaddingAndSpacing() const {
	return borderLeft() + borderRight() + 
               (collapseBorders() ? 0 : (paddingLeft() + paddingRight() + (numEffCols() + 1) * hBorderSpacing()));
    }

    RenderTableCol *colElement(int col);

    void setNeedSectionRecalc() { needSectionRecalc = true; }

    virtual RenderObject* removeChildNode(RenderObject* child);

    RenderTableCell* cellAbove(const RenderTableCell* cell) const;
    RenderTableCell* cellBelow(const RenderTableCell* cell) const;
    RenderTableCell* cellLeft(const RenderTableCell* cell) const;
    RenderTableCell* cellRight(const RenderTableCell* cell) const;
 
    CollapsedBorderValue* currentBorderStyle() { return m_currentBorder; }
    
    bool hasSections() const { return head || foot || firstBody; }

protected:

    void recalcSections();

    friend class AutoTableLayout;
    friend class FixedTableLayout;

    RenderBlock *tCaption;
    RenderTableSection *head;
    RenderTableSection *foot;
    RenderTableSection *firstBody;

    TableLayout *tableLayout;

    CollapsedBorderValue* m_currentBorder;
    
    Frame frame                 : 4;
    Rules rules                 : 4;

    bool has_col_elems		: 1;
    uint padding		: 22;
    uint needSectionRecalc	: 1;
    
    short hspacing;
    short vspacing;
};

// -------------------------------------------------------------------------

class RenderTableSection : public RenderContainer
{
public:
    RenderTableSection(DOM::NodeImpl* node);
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
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

    void addCell(RenderTableCell *cell);

    void setCellWidths();
    void calcRowHeight();
    int layoutRows(int height);

    RenderTable *table() const { return static_cast<RenderTable *>(parent()); }

    struct CellStruct {
        RenderTableCell *cell;
        bool inColSpan; // true for columns after the first in a colspan
    };
    typedef Array<CellStruct> Row;
    struct RowStruct {
	Row *row;
	int baseLine;
	Length height;
    };

    CellStruct& cellAt(int row,  int col) {
	return (*grid[row].row)[col];
    }
    const CellStruct& cellAt(int row, int col) const {
	return (*grid[row].row)[col];
    }

    virtual void paint(PaintInfo& i, int tx, int ty);

    int numRows() const { return gridRows; }
    int getBaseline(int row) {return grid[row].baseLine;}

    void setNeedCellRecalc() {
        needCellRecalc = true;
        table()->setNeedSectionRecalc();
    }

    virtual RenderObject* removeChildNode(RenderObject* child);

    // this gets a cell grid data structure. changing the number of
    // columns is done by the table
    Array<RowStruct> grid;
    int gridRows;
    Array<int> rowPos;

    int cCol;
    int cRow;
    bool needCellRecalc;

    void recalcCells();
protected:
    bool ensureRows(int numRows);
    void clearGrid();
};

// -------------------------------------------------------------------------

class RenderTableRow : public RenderContainer
{
public:
    RenderTableRow(DOM::NodeImpl* node);

    virtual void destroy();

    virtual void setStyle(RenderStyle*);
    virtual const char *renderName() const { return "RenderTableRow"; }

    virtual bool isTableRow() const { return true; }

    // overrides
    virtual void addChild(RenderObject *child, RenderObject *beforeChild = 0);
    virtual RenderObject* removeChildNode(RenderObject* child);

    virtual short lineHeight(bool) const { return 0; }
    virtual void position(int, int, int, int, int, bool, bool, int) {}

    virtual void layout();
    virtual IntRect getAbsoluteRepaintRect();
    
    RenderTable *table() const { return static_cast<RenderTable *>(parent()->parent()); }
    RenderTableSection *section() const { return static_cast<RenderTableSection *>(parent()); }

#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif
};

// -------------------------------------------------------------------------

class RenderTableCell : public RenderBlock
{
public:
    RenderTableCell(DOM::NodeImpl* node);

    virtual void destroy();

    virtual const char *renderName() const { return "RenderTableCell"; }
    virtual bool isTableCell() const { return true; }

    // overrides RenderObject
    virtual bool requiresLayer();

    // ### FIX these two...
    int cellIndex() const { return 0; }
    void setCellIndex(int) { }

    int colSpan() const { return cSpan; }
    void setColSpan(int c) { cSpan = c; }

    int rowSpan() const { return rSpan; }
    void setRowSpan(int r) { rSpan = r; }

    int col() const { return _col; }
    void setCol(int col) { _col = col; }
    int row() const { return _row; }
    void setRow(int r) { _row = r; }

    Length styleOrColWidth();

    // overrides
    virtual void calcMinMaxWidth();
    virtual void calcWidth();
    virtual void setWidth(int width);
    virtual void setStyle(RenderStyle *style);

    int borderLeft() const;
    int borderRight() const;
    int borderTop() const;
    int borderBottom() const;

    CollapsedBorderValue collapsedLeftBorder() const;
    CollapsedBorderValue collapsedRightBorder() const;
    CollapsedBorderValue collapsedTopBorder() const;
    CollapsedBorderValue collapsedBottomBorder() const;
    virtual void collectBorders(QValueList<CollapsedBorderValue>& borderStyles);

    virtual void updateFromElement();

    virtual void layout();
    
    void setCellTopExtra(int p) { _topExtra = p; }
    void setCellBottomExtra(int p) { _bottomExtra = p; }

    virtual void paint(PaintInfo& i, int tx, int ty);

    void paintCollapsedBorder(QPainter* p, int x, int y, int w, int h);
    
    // lie position to outside observers
    virtual int yPos() const { return m_y + _topExtra; }

    virtual void computeAbsoluteRepaintRect(IntRect& r, bool f=false);
    virtual bool absolutePosition(int &xPos, int &yPos, bool f = false);

    virtual short baselinePosition(bool = false) const;

    virtual int borderTopExtra() const { return _topExtra; }
    virtual int borderBottomExtra() const { return _bottomExtra; }

    RenderTable *table() const { return static_cast<RenderTable *>(parent()->parent()->parent()); }
    RenderTableSection *section() const { return static_cast<RenderTableSection *>(parent()->parent()); }

#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

    virtual IntRect getAbsoluteRepaintRect();
    
protected:
    virtual void paintBoxDecorations(PaintInfo& i, int _tx, int _ty);
    
    int _row;
    int _col;
    int rSpan;
    int cSpan;
    int _topExtra : 31;
    bool nWrap : 1;
    int _bottomExtra : 31;
    bool m_widthChanged : 1;
    
    int m_percentageHeight;
};


// -------------------------------------------------------------------------

class RenderTableCol : public RenderContainer
{
public:
    RenderTableCol(DOM::NodeImpl* node);

    virtual const char *renderName() const { return "RenderTableCol"; }

    int span() const { return _span; }
    void setSpan(int s) { _span = s; }

    virtual void addChild(RenderObject *child, RenderObject *beforeChild = 0);

    virtual bool isTableCol() const { return true; }

    virtual short lineHeight(bool) const { return 0; }

    virtual void updateFromElement();

    virtual bool canHaveChildren() const;
    
#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

protected:
    int _span;
};

};
#endif

