/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
 *
 * $Id$
 */
#ifndef RENDER_TABLE_H
#define RENDER_TABLE_H

#include <qcolor.h>
#include <qptrvector.h>

#include "render_box.h"
#include "render_flow.h"
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

class RenderTable : public RenderFlow
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

    virtual bool isRendered() const { return true; }
    virtual bool isTable() const { return true; }

    int getColumnPos(int col) const
        { return columnPos[col]; }

    int cellSpacing() const { return spacing; }

    Rules getRules() const { return rules; }

    const QColor &bgColor() const { return style()->backgroundColor(); }

    uint cellPadding() const { return padding; }
    void setCellPadding( uint p ) { padding = p; }

    // overrides
    virtual int overflowHeight() const { return height(); }
    virtual int overflowWidth() const { return width(); }
    virtual void addChild(RenderObject *child, RenderObject *beforeChild = 0);
    virtual void paint( QPainter *, int x, int y, int w, int h,
                        int tx, int ty, int paintPhase);
    virtual void layout();
    virtual void calcMinMaxWidth();
    virtual void close();

    virtual short lineHeight(bool b) const;
    virtual short baselinePosition(bool b) const;

    virtual void setCellWidths( );

    virtual void position(int x, int y, int from, int len, int width, bool reverse, bool firstLine, int);

    virtual void calcWidth();

    virtual int borderTopExtra();
    virtual int borderBottomExtra();

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
	ushort width; // the calculated position of the column
    };

    QMemArray<int> columnPos;
    QMemArray<ColumnStruct> columns;

    void splitColumn( int pos, int firstSpan );
    void appendColumn( int span );
    int numEffCols() const { return columns.size(); }
    int spanOfEffCol( int effCol ) const { return columns[effCol].span; }
    int colToEffCol( int col ) const {
	int c = 0;
	int i = 0;
	while ( c < col && i < (int)columns.size() ) {
	    c += columns[i].span;
	    i++;
	}
	return i;
    }
    int effColToCol( int effCol ) const {
	int c = 0;
	for ( int i = 0; i < effCol; i++ )
	    c += columns[i].span;
	return c;
    }

    int bordersAndSpacing() const {
	return borderLeft() + borderRight() + (numEffCols()+1) * cellSpacing();
    }

    RenderTableCol *colElement( int col );

    void setNeedSectionRecalc() { needSectionRecalc = true; }

    virtual RenderObject* removeChildNode(RenderObject* child);

protected:

    void recalcSections();

    friend class AutoTableLayout;
    friend class FixedTableLayout;

    RenderFlow         *tCaption;
    RenderTableSection *head;
    RenderTableSection *foot;
    RenderTableSection *firstBody;

    TableLayout *tableLayout;

    Frame frame                 : 4;
    Rules rules                 : 4;

    bool has_col_elems		: 1;
    uint spacing                : 11;
    uint padding		: 11;
    uint needSectionRecalc	: 1;
};

// -------------------------------------------------------------------------

class RenderTableSection : public RenderBox
{
public:
    RenderTableSection(DOM::NodeImpl* node);
    ~RenderTableSection();
    virtual void detach(RenderArena* arena);

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

    void addCell( RenderTableCell *cell );

    void setCellWidths();
    void calcRowHeight();
    int layoutRows( int height );

    RenderTable *table() const { return static_cast<RenderTable *>(parent()); }

    typedef QMemArray<RenderTableCell *> Row;
    struct RowStruct {
	Row *row;
	int baseLine;
	Length height;
    };

    RenderTableCell *&cellAt( int row,  int col ) {
	return (*(grid[row].row))[col];
    }
    RenderTableCell *cellAt( int row,  int col ) const {
	return (*(grid[row].row))[col];
    }

    virtual void paint( QPainter *, int x, int y, int w, int h,
                        int tx, int ty, int paintPhase);

    int numRows() const { return grid.size(); }
    int getBaseline(int row) {return grid[row].baseLine;}

    void setNeedCellRecalc() {
        needCellRecalc = true;
        table()->setNeedSectionRecalc();
    }

    virtual RenderObject* removeChildNode(RenderObject* child);

    // this gets a cell grid data structure. changing the number of
    // columns is done by the table
    QMemArray<RowStruct> grid;
    QMemArray<int> rowPos;

    ushort cCol : 15;
    short cRow : 16;
    bool needCellRecalc : 1;

    void recalcCells();
protected:
    void ensureRows( int numRows );
    void clearGrid();
};

// -------------------------------------------------------------------------

class RenderTableRow : public RenderContainer
{
public:
    RenderTableRow(DOM::NodeImpl* node);

    virtual void detach(RenderArena* arena);

    virtual void setStyle( RenderStyle* );
    virtual const char *renderName() const { return "RenderTableRow"; }

    virtual bool isTableRow() const { return true; }

    // overrides
    virtual void addChild(RenderObject *child, RenderObject *beforeChild = 0);
    virtual RenderObject* removeChildNode(RenderObject* child);

    virtual short lineHeight( bool ) const { return 0; }
    virtual void position(int, int, int, int, int, bool, bool, int) {}

    virtual void layout();

    RenderTable *table() const { return static_cast<RenderTable *>(parent()->parent()); }
    RenderTableSection *section() const { return static_cast<RenderTableSection *>(parent()); }

#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif
};

// -------------------------------------------------------------------------

class RenderTableCell : public RenderFlow
{
public:
    RenderTableCell(DOM::NodeImpl* node);

    virtual void detach(RenderArena* arena);

    virtual const char *renderName() const { return "RenderTableCell"; }
    virtual bool isTableCell() const { return true; }

    // ### FIX these two...
    long cellIndex() const { return 0; }
    void setCellIndex( long ) { }

    unsigned short colSpan() const { return cSpan; }
    void setColSpan( unsigned short c ) { cSpan = c; }

    unsigned short rowSpan() const { return rSpan; }
    void setRowSpan( unsigned short r ) { rSpan = r; }

    int col() const { return _col; }
    void setCol(int col) { _col = col; }
    int row() const { return _row; }
    void setRow(int r) { _row = r; }

    // overrides
    virtual void calcMinMaxWidth();
    virtual void calcWidth();
    virtual void setWidth( int width );
    virtual void setStyle( RenderStyle *style );

    virtual void updateFromElement();

    void setCellTopExtra(int p) { _topExtra = p; }
    void setCellBottomExtra(int p) { _bottomExtra = p; }

    int getCellPercentageHeight() const;
    void setCellPercentageHeight(int h);
    
    virtual void paint( QPainter* p, int x, int y,
                        int w, int h, int tx, int ty, int paintPhase);

    virtual void close();

    // lie position to outside observers
    virtual int yPos() const { return m_y + _topExtra; }

    virtual void repaintRectangle(int x, int y, int w, int h, bool immediate = false, bool f=false);
    virtual bool absolutePosition(int &xPos, int &yPos, bool f = false);

    virtual short baselinePosition( bool = false ) const;

    virtual int borderTopExtra() { return _topExtra; }
    virtual int borderBottomExtra() { return _bottomExtra; }

    RenderTable *table() const { return static_cast<RenderTable *>(parent()->parent()->parent()); }
    RenderTableSection *section() const { return static_cast<RenderTableSection *>(parent()->parent()); }

#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

    bool widthChanged() {
	bool retval = m_widthChanged;
	m_widthChanged = false;
	return retval;
    }

protected:
    virtual void paintBoxDecorations(QPainter *p,int _x, int _y,
                                     int _w, int _h, int _tx, int _ty);
    
    short _row;
    short _col;
    ushort rSpan;
    ushort cSpan;
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

    long span() const { return _span; }
    void setSpan( long s ) { _span = s; }

    virtual void addChild(RenderObject *child, RenderObject *beforeChild = 0);

    virtual bool isTableCol() const { return true; }

    virtual short lineHeight( bool ) const { return 0; }
    virtual void position(int, int, int, int, int, bool, bool, int) {}
    virtual void layout() {}

    virtual void updateFromElement();

#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

protected:
    short _span;
};

};
#endif

