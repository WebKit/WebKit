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
class RenderTableCaption;

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

    int getColumnPos(int col)
        { return columnPos[col]; }
    int getColumnWidth(int col)
        { if(!actColWidth.size() < col) return 0; return actColWidth[col]; }

    int cellSpacing() { return spacing; }

    Rules getRules() { return rules; }

    QColor bgColor() { return style()->backgroundColor(); }


    void startRow();
    void addCell( RenderTableCell *cell );
    void endTable();
    void  addColInfo(RenderTableCell *cell, bool recalc = true);
    void  addColInfo(RenderTableCol *colel);

    void addColInfo(int _startCol, int _colSpan,
                    int _minSize, int _maxSize, khtml::Length _width,
                    RenderTableCell* _cell, bool recalc = true);

    void recalcColInfos();

    // overrides
    virtual void addChild(RenderObject *child, RenderObject *beforeChild = 0);
    virtual void print( QPainter *, int x, int y, int w, int h,
                        int tx, int ty);
    virtual void layout();
    virtual void calcMinMaxWidth();
    virtual void close();

    virtual void setCellWidths( );

    int getBaseline(int row) {return rowBaselines[row];}

    virtual void position(int x, int y, int from, int len, int width, bool reverse, bool firstLine, int);

    virtual void calcWidth();

    virtual int borderTopExtra();
    virtual int borderBottomExtra();

    void closeRow();
    void setNeedsCellsRecalc();
    void recalcCells();

#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

public:
    /*
     * For each table element with a different width a ColInfo struct is
     * maintained. Consider for example the following table:
     * +---+---+---+
     * | A | B | C |
     * +---+---+---+
     * |   D   | E |
     * +-------+---+
     *
     * This table would result in 4 ColInfo structs being allocated.
     * 1 for A, 1 for B, 1 for C & E, and 1 for D.
     *
     * Note that C and E share the same ColInfo.
     *
     * Note that D has a seperate ColInfo entry.
     *
     * There is always 1 default ColInfo entry which stretches across the
     * entire table.
     */
    struct ColInfo
    {
        ColInfo()
        {
	    needsRecalc();
        }

        void needsRecalc() {
            span = start = min = max = value = 0;
            minCell = maxCell = widthCell = 0;
            type=khtml::Variable;
            needRecalc = true;
        }
            
        int     span;
        int     start;
        int     min;
        int     max;
        RenderTableCell* minCell;
        RenderTableCell* maxCell;
        khtml::LengthType       type;
        int     value;
	RenderTableCell* widthCell;
        bool    needRecalc;
    };

protected:

    void recalcColInfo( ColInfo *col );

    // This function calculates the actual widths of the columns
    void calcColWidth();

    // calculates the height of each row
    void calcRowHeight(int r);

    void layoutRows(int yoff);

    void setCells( unsigned int r, unsigned int c, RenderTableCell *cell );
    void addRows( int num );
    void addColumns( int num );

    RenderTableCell ***cells;

    class ColInfoLine : public QPtrVector<ColInfo>
    {
    public:
        ColInfoLine() : QPtrVector<ColInfo>()
        { setAutoDelete(true); }
        ColInfoLine(int i) : QPtrVector<ColInfo>(i)
        { setAutoDelete(true); }
        ColInfoLine(const QPtrVector<ColInfo> &v) : QPtrVector<ColInfo>(v)
        { setAutoDelete(true); }
    };

    QPtrVector<ColInfoLine> colInfos;

    void calcColMinMax();
    void calcSingleColMinMax(int c, ColInfo* col);
    void calcFinalColMax(int c, ColInfo* col);
    void spreadSpanMinMax(int col, int span, int min, int max, khtml::LengthType type);
    int distributeWidth(int distrib, khtml::LengthType type, int typeCols );
    int distributeMinWidth(int distrib, khtml::LengthType distType,
            khtml::LengthType toType, int start, int span, bool minlimit );
    int distributeMaxWidth(int distrib, LengthType distType,
            LengthType toType, int start, int span);
    int distributeRest(int distrib, khtml::LengthType type, int divider );

    int maxColSpan;

    QMemArray<int> columnPos;
    QMemArray<int> colMaxWidth;
    QMemArray<int> colMinWidth;
    QMemArray<khtml::LengthType> colType;
    QMemArray<int> colValue;
    QMemArray<int> rowHeights;
    QMemArray<int> rowBaselines;
    QMemArray<int> actColWidth;
    unsigned int col;
    unsigned int totalCols;
    unsigned int row;
    unsigned int totalRows;
    unsigned int allocRows;

    unsigned int totalPercent ;
    unsigned int totalRelative ;

    RenderTableCaption *tCaption;
    RenderTableSection *head;
    RenderTableSection *foot;
    RenderTableSection *firstBody;

    Frame frame;
    Rules rules;

    RenderTableCol *_oldColElem;
    int _currentCol; // keeps track of current col for col/colgroup stuff
    int spacing;
    short _lastParentWidth 	: 16;
    bool incremental 		: 1;
    bool collapseBorders 	: 1;
    bool colWidthKnown 		: 1;
    bool needsCellsRecalc 	: 1;
    bool hasPercent 		: 1;
};

// -------------------------------------------------------------------------

class RenderTableSection : public RenderContainer
{
public:
    RenderTableSection(DOM::NodeImpl* node);
    ~RenderTableSection();

    virtual const char *renderName() const { return "RenderTableSection"; }

    int numRows() { return nrows; }

    // overrides
    virtual void addChild(RenderObject *child, RenderObject *beforeChild = 0);
    virtual bool isTableSection() const { return true; }

    virtual short lineHeight(bool) const { return 0; }
    virtual void position(int, int, int, int, int, bool, bool, int) {}

    virtual void setTable(RenderTable *t) { table = t; }

#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

protected:
    RenderTable *table;
    int nrows;
};

// -------------------------------------------------------------------------

class RenderTableRow : public RenderContainer
{
public:
    RenderTableRow(DOM::NodeImpl* node);
    ~RenderTableRow();

    virtual const char *renderName() const { return "RenderTableRow"; }

    long rowIndex() const;
    void setRowIndex( long );

    long sectionRowIndex() const { return rIndex; }
    void setSectionRowIndex( long i ) { rIndex = i; }

    virtual bool isTableRow() const { return true; }

    // overrides
    virtual void addChild(RenderObject *child, RenderObject *beforeChild = 0);

    virtual short lineHeight( bool ) const { return 0; }
    virtual void position(int, int, int, int, int, bool, bool, int) {}

    virtual void close();

    virtual void repaint();

    virtual void layout();

    virtual void setTable(RenderTable *t) { table = t; }

#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

protected:
    RenderTable *table;

    // relative to the current section!
    int rIndex;
    int ncols;
};

// -------------------------------------------------------------------------

class RenderTableCell : public RenderFlow
{
public:
    RenderTableCell(DOM::NodeImpl* node);
    ~RenderTableCell();

    virtual const char *renderName() const { return "RenderTableCell"; }
    virtual bool isTableCell() const { return true; }

    // ### FIX these two...
    long cellIndex() const { return 0; }
    void setCellIndex( long ) { }

    long colSpan() const { return cSpan; }
    void setColSpan( long c ) { cSpan = c; }

    long rowSpan() const { return rSpan; }
    void setRowSpan( long r ) { rSpan = r; }

    bool noWrap() const { return nWrap; }
    void setNoWrap(bool nw) { nWrap = nw; }

    int col() const { return _col; }
    void setCol(int col) { _col = col; }
    int row() const { return _row; }
    void setRow(int r) { _row = r; }

    khtml::LengthType colType();

    // overrides
    virtual void calcMinMaxWidth();
    virtual void calcWidth();
    virtual void setWidth( int width );
    virtual void setStyle( RenderStyle *style );
    virtual void repaint();

    virtual void updateFromElement();

    void setRowHeight(int h) { rowHeight = h; }

    void setRowImpl(RenderTableRow *r) { rowimpl = r; }

    void setCellTopExtra(int p) { _topExtra = p; }
    void setCellBottomExtra(int p) { _bottomExtra = p; }

    virtual void setTable(RenderTable *t) { m_table = t; }
    RenderTable *table() const { return m_table; }

    virtual void print( QPainter* p, int x, int y,
                        int w, int h, int tx, int ty);

    virtual void close();

    // lie position to outside observers
    virtual int yPos() const { return m_y + _topExtra; }

    virtual void repaintRectangle(int x, int y, int w, int h, bool f=false);
    virtual bool absolutePosition(int &xPos, int &yPos, bool f = false);

    virtual short baselinePosition( bool = false ) const;

#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

    bool widthChanged() {
	bool retval = m_widthChanged;
	m_widthChanged = false;
	return retval;
    }

protected:
    RenderTable *m_table;

    virtual void printBoxDecorations(QPainter *p,int _x, int _y,
                                     int _w, int _h, int _tx, int _ty);

    short _row;
    short _col;
    short rSpan;
    short cSpan;
    int _id;
    int rowHeight;
    int _topExtra;
    int _bottomExtra;
    bool nWrap : 1;
    bool m_widthChanged : 1;

    virtual int borderTopExtra() { return _topExtra; }
    virtual int borderBottomExtra() { return _bottomExtra; }


    RenderTableRow *rowimpl;
};

// -------------------------------------------------------------------------

class RenderTableCol : public RenderContainer
{
public:
    RenderTableCol(DOM::NodeImpl* node);
    ~RenderTableCol();

    virtual const char *renderName() const { return "RenderTableCol"; }

    void setStartCol( int c ) {_startCol = _currentCol = c; }
    int col() { return _startCol; }
    int lastCol() { return _currentCol; }

    long span() const { return _span; }
    void setSpan( long s ) { _span = s; }

    virtual void addChild(RenderObject *child, RenderObject *beforeChild = 0);

    virtual short lineHeight( bool ) const { return 0; }
    virtual void position(int, int, int, int, int, bool, bool, int) {}
    virtual void layout() {}

    virtual void setTable(RenderTable *t) { table = t; }

    virtual void updateFromElement();

#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

protected:
    RenderTable *table;
    int _span;
    int _currentCol;
    int _startCol;

    // could be ID_COL or ID_COLGROUP ... The DOM is not quite clear on
    // this, but since both elements work quite similar, we use one
    // DOMElement for them...
    ushort _id;
};

// -------------------------------------------------------------------------

class RenderTableCaption : public RenderFlow
{
public:
    RenderTableCaption(DOM::NodeImpl*);
    ~RenderTableCaption();

    virtual const char *renderName() const { return "RenderTableCaption"; }

    virtual void setTable(RenderTable *t) { table = t; }
protected:
    RenderTable *table;
};

};
#endif

