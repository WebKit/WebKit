/**
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
 */

//#define TABLE_DEBUG
//#define TABLE_PRINT
//#define DEBUG_LAYOUT
//#define BOX_DEBUG

#include "rendering/render_table.h"
#include "html/html_tableimpl.h"
#include "misc/htmltags.h"

#include <kglobal.h>

#include <qapplication.h>
#include <qstyle.h>

#include <kdebug.h>
#include <assert.h>

using namespace khtml;

template class QMemArray<LengthType>;

#define FOR_EACH_CELL(r,c,cell) \
    for ( unsigned int r = 0; r < totalRows; r++ )                    \
    {                                                                 \
        for ( unsigned int c = 0; c < totalCols; c++ )                \
        {                                                             \
            RenderTableCell *cell = cells[r][c];             \
            if (!cell)                                                \
                continue;                                             \
            if ( (c < totalCols - 1) && (cell == cells[r][c+1]) )     \
                continue;                                             \
            if ( (r < totalRows - 1) && (cells[r+1][c] == cell) )     \
                continue;

#define END_FOR_EACH } }


RenderTable::RenderTable(DOM::NodeImpl* node)
    : RenderFlow(node)
{

    tCaption = 0;
    _oldColElem = 0;
    head = 0;
    foot = 0;
    firstBody = 0;

    incremental = false;
    m_maxWidth = 0;


    rules = None;
    frame = Void;

    row = 0;
    col = 0;

    maxColSpan = 0;

    colInfos.setAutoDelete(true);

    _currentCol=0;

    _lastParentWidth = 0;

    columnPos.resize( 2 );
    colMaxWidth.resize( 1 );
    colMinWidth.resize( 1 );
    colValue.resize(1);
    colType.resize(1);
    actColWidth.resize(1);
    columnPos.fill( 0 );
    colMaxWidth.fill( 0 );
    colMinWidth.fill( 0 );
    colValue.fill(0);
    colType.fill(Variable);
    actColWidth.fill(0);

    columnPos[0] = spacing;

    totalCols = 0;   // this should be expanded to the maximum number of cols
                     // by the first row parsed
    totalRows = 1;
    allocRows = 5;   // allocate five rows initially
    rowHeights.resize( totalRows+1 );
    rowHeights[0] = rowHeights[1] = 0;

    cells = new RenderTableCell ** [allocRows];

    for ( unsigned int r = 0; r < allocRows; r++ )
    {
        cells[r] = new RenderTableCell * [totalCols];
        memset( cells[r], 0, totalCols * sizeof( RenderTableCell * ));
    }
    needsCellsRecalc = false;
    colWidthKnown = false;
    hasPercent = false;
}

RenderTable::~RenderTable()
{
    for ( unsigned int r = 0; r < allocRows; r++ )
        delete [] cells[r];
    delete [] cells;
}

void RenderTable::setStyle(RenderStyle *_style)
{
    RenderFlow::setStyle(_style);

    // init RenderObject attributes
    setInline(style()->display()==INLINE_TABLE && !isPositioned());
    setReplaced(style()->display()==INLINE_TABLE);

    spacing = style()->borderSpacing();
    collapseBorders = style()->borderCollapse();
}

void RenderTable::position(int x, int y, int, int, int, bool, bool, int)
{
    //for inline tables only
    m_x = x + marginLeft();
    m_y = y + marginTop();
}

void RenderTable::addChild(RenderObject *child, RenderObject *beforeChild)
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(Table)::addChild( " << child->renderName() << ", " <<
                       (beforeChild ? beforeChild->renderName() : "0") << " )" << endl;
#endif
    RenderObject *o = child;

    switch(child->style()->display())
    {
    case TABLE_CAPTION:
        tCaption = static_cast<RenderTableCaption *>(child);
        break;
    case TABLE_COLUMN:
    case TABLE_COLUMN_GROUP:
        {
	    RenderContainer::addChild(child,beforeChild);
	    RenderTableCol* colel = static_cast<RenderTableCol *>(child);
	    if (_oldColElem && _oldColElem->style()->display() == TABLE_COLUMN_GROUP)
		_currentCol = _oldColElem->lastCol();
	    _oldColElem = colel;
	    colel->setStartCol(_currentCol);
	    if ( colel->span() != 0 ) {
		if (child->style()->display() == TABLE_COLUMN)
		    _currentCol++;
		else
		    _currentCol+=colel->span();
		addColInfo(colel);
	    }
	    incremental = true;
	    colel->setTable(this);
	}
	child->setLayouted( false );
	child->setMinMaxKnown( false );
        return;
    case TABLE_HEADER_GROUP:
        if(incremental && !columnPos[totalCols]);// calcColWidth();
//      setTHead(static_cast<RenderTableSection *>(child));
        break;
    case TABLE_FOOTER_GROUP:
        if(incremental && !columnPos[totalCols]);// calcColWidth();
//      setTFoot(static_cast<RenderTableSection *>(child));
        break;
    case TABLE_ROW_GROUP:
        if(incremental && !columnPos[totalCols]);// calcColWidth();
        if(!firstBody)
            firstBody = static_cast<RenderTableSection *>(child);
        break;
    default:
        if ( !beforeChild )
            beforeChild = lastChild();
        if ( beforeChild && beforeChild->isAnonymousBox() )
            o = beforeChild;
        else {
	    RenderObject *lastBox = beforeChild;
	    while ( lastBox && lastBox->parent()->isAnonymousBox() &&
		    !lastBox->isTableSection() && lastBox->style()->display() != TABLE_CAPTION )
		lastBox = lastBox->parent();
	    if ( lastBox && lastBox->isAnonymousBox() ) {
		lastBox->addChild( child, beforeChild );
		return;
	    } else {
//          kdDebug( 6040 ) << "creating anonymous table section" << endl;
		o = new RenderTableSection(0 /* anonymous */);
		RenderStyle *newStyle = new RenderStyle();
		newStyle->inheritFrom(style());
		newStyle->setDisplay(TABLE_ROW_GROUP);
		o->setStyle(newStyle);
		o->setIsAnonymousBox(true);
		addChild(o, beforeChild);
	    }
        }
        o->addChild(child);
	child->setLayouted( false );
	child->setMinMaxKnown( false );
        return;
    }
    RenderContainer::addChild(child,beforeChild);
    child->setTable(this);
}

void RenderTable::startRow()
{
    while ( col < totalCols && cells[row][col] != 0 )
        col++;
    if ( col )
        row++;
    col = 0;
    if(row > totalRows) totalRows = row;
}

void RenderTable::closeRow()
{
    while ( col < totalCols && cells[row][col] != 0L )
        col++;

    if (col<=0) return;

    RenderTableCell* cell = cells[row][col-1];

    if (!cell) return;
}

void RenderTable::addCell( RenderTableCell *cell )
{
    while ( col < totalCols && cells[row][col] != 0L )
        col++;
    setCells( row, col, cell );

    col++;
}


void RenderTable::setCells( unsigned int r, unsigned int c,
                                     RenderTableCell *cell )
{
#ifdef TABLE_DEBUG
    kdDebug( 6040 ) << "setCells: span = " << cell->rowSpan() << "/" << cell->colSpan() << " pos = " << r << "/" << c << endl;
#endif
    cell->setRow(r);
    cell->setCol(c);

    unsigned int endRow = r + cell->rowSpan();
    unsigned int endCol = c + cell->colSpan();

    if ( endCol > totalCols )
        addColumns( endCol - totalCols );

    if ( endRow >= allocRows )
        addRows( endRow - allocRows + 10 );

    if ( endRow > totalRows )
        totalRows = endRow;

    for ( ; r < endRow; r++ ) {
        for ( unsigned int tc = c; tc < endCol; tc++ ) {
            cells[r][tc] = cell;
        }
    }
}

void RenderTable::addRows( int num )
{
    RenderTableCell ***newRows =
        new RenderTableCell ** [allocRows + num];
    memcpy( newRows, cells, allocRows * sizeof(RenderTableCell **) );
    delete [] cells;
    cells = newRows;

    for ( unsigned int r = allocRows; r < allocRows + num; r++ )
    {
        cells[r] = new RenderTableCell * [totalCols];
        memset( cells[r], 0, totalCols * sizeof( RenderTableCell * ));
    }

    allocRows += num;
}

void RenderTable::addColumns( int num )
{
#ifdef TABLE_DEBUG
    kdDebug( 6040 ) << "addColumns() totalCols=" << totalCols << " new=" << num << endl;
#endif
    RenderTableCell **newCells;

    int newCols = totalCols + num;
    // resize the col structs to the number of columns
    columnPos.resize(newCols+1);
    memset( columnPos.data() + totalCols + 1, 0, num*sizeof(int));
    colMaxWidth.resize(newCols);
    memset( colMaxWidth.data() + totalCols , 0, num*sizeof(int));
    colMinWidth.resize(newCols);
    memset( colMinWidth.data() + totalCols , 0, num*sizeof(int));
    colValue.resize(newCols);
    memset( colValue.data() + totalCols , 0, num*sizeof(int));
    colType.resize(newCols);
    memset( colType.data() + totalCols , 0, num*sizeof(LengthType));
    actColWidth.resize(newCols);
    memset( actColWidth.data() + totalCols , 0, num*sizeof(LengthType));

    for ( unsigned int r = 0; r < allocRows; r++ )
    {
        newCells = new RenderTableCell * [newCols];
        memcpy( newCells, cells[r],
                totalCols * sizeof( RenderTableCell * ) );
        memset( newCells + totalCols, 0,
                num * sizeof( RenderTableCell * ) );
        delete [] cells[r];
        cells[r] = newCells;
    }

    int mSpan = newCols;

    colInfos.resize(mSpan);

    for ( unsigned int c =0 ; c < totalCols; c++ )
    {
        colInfos[c]->resize(newCols);
    }
    for ( unsigned int c = totalCols; (int)c < newCols; c++ )
    {
        colInfos.insert(c, new ColInfoLine(newCols-c+1));
    }

    totalCols = newCols;
}


void RenderTable::recalcColInfos()
{
//    kdDebug(0) << "RenderTable::recalcColInfos()" << endl;
    for (int s=0 ; s<maxColSpan; s++)
    {
        for (unsigned int c=0 ; c<totalCols; c++)
            if (c<colInfos[s]->size())
                colInfos[s]->remove(c);
    }

    maxColSpan = 0;

    FOR_EACH_CELL(r,c,cell)
        addColInfo(cell);
    END_FOR_EACH
}

void RenderTable::recalcColInfo( ColInfo *col )
{
    //qDebug("------------- recalcColinfo: line=%d, span=%d", col->start, col->span-1);

    KHTMLAssert( colInfos[col->span-1]->data()[col->start] == col );
    ColInfoLine *line = (colInfos[col->span-1]);
#ifdef APPLE_CHANGES
    // We can't mutate the buffer in place, so we use remove to clear out the item. 
    // We unset auto-delete so that the |col| is not deleted.  That happens at the
    // end of the function below. -dwh
    line->setAutoDelete(false);
    line->remove(col->start);
    line->setAutoDelete(true);
#else
    ColInfo **data = line->data() + col->start;
    *data = 0;
    KHTMLAssert( colInfos[col->span-1]->data()[col->start] == 0 );
#endif
    
    // add table-column if exists
    RenderObject *child = firstChild();
    while( child ) {
	if ( child->style()->display() == TABLE_COLUMN ||
	     child->style()->display() == TABLE_COLUMN_GROUP ) {
	    RenderTableCol *tc = static_cast<RenderTableCol *>(child);
	    if ( tc->span() == col->span && tc->col() == col->start ) {
		addColInfo( tc );
		break;
	    }
	} else {
	    break;
	}
	child = child->nextSibling();
    }

    // now the cells
    for ( unsigned int r = 0; r < totalRows; r++ ) {
	RenderTableCell *cell = cells[r][col->start];
	if ( cell && cell->colSpan() == col->span )
	    addColInfo(cell, false);
    }
    delete col;
    setMinMaxKnown( false );

    //qDebug("------------- end recalcColinfo");
}


void RenderTable::addColInfo(RenderTableCol *colel)
{

    int _startCol = colel->col();
    int span = colel->span();
    int _minSize=0;
    int _maxSize=0;
    Length _width = colel->style()->width();
    if (_width.type==Fixed) {
        _maxSize=_width.value;
	_minSize=_width.value;
    }

    for (int n=0; n<span; ++n) {
#ifdef TABLE_DEBUG
        kdDebug( 6040 ) << "COL Element" << endl;
        kdDebug( 6040 ) << "    startCol=" << _startCol << " span=" << span << endl;
        kdDebug( 6040 ) << "    min=" << _minSize << " max=" << _maxSize << " val=" << _width.value << endl;
#endif
        addColInfo(_startCol+n, 1 , _minSize, _maxSize, _width ,0);
    }

}

void RenderTable::addColInfo(RenderTableCell *cell, bool allowRecalc)
{

    int _startCol = cell->col();
    int _colSpan = cell->colSpan();
    int _minSize = cell->minWidth();
    int _maxSize = cell->maxWidth();

    if (collapseBorders)
    {
        int bw = cell->borderLeft() + cell->borderRight();
        _minSize -= bw;
        _maxSize -= bw;
    }

    Length _width = cell->style()->width();
    addColInfo(_startCol, _colSpan, _minSize, _maxSize, _width ,cell, allowRecalc);
}

void RenderTable::addColInfo(int _startCol, int _colSpan,
                                      int _minSize, int _maxSize,
                                      Length _width, RenderTableCell* _cell, bool allowRecalc )
{
    // Netscape ignores width values of "0" or "0%"
    if ( style()->htmlHacks() && _width.value == 0 && (_width.type == Percent || _width.type == Fixed) )
	    _width = Length();

    if (_startCol + _colSpan > (int) totalCols)
        addColumns(totalCols - _startCol + _colSpan);

    ColInfo* col = colInfos[_colSpan-1]->at(_startCol);

    bool changed = false;
    bool recalc = false;

    if (!col) {
        col = new ColInfo;
        col->span = _colSpan;
        col->start = _startCol;
        col->minCell = _cell;
        col->maxCell = _cell;
	col->min = _minSize;
	col->max = _maxSize;
        if (_colSpan>maxColSpan)
            maxColSpan=_colSpan;
        colInfos[_colSpan-1]->insert(_startCol,col);
	col->type = _width.type;
	col->value = _width.value;
	col->widthCell = _cell;

	changed = true;
    } else {
	if (_minSize > col->min)
	{
	    if ( allowRecalc && col->minCell == _cell ) {
		recalc = true;
	    } else {
		col->min = _minSize;
		col->minCell = _cell;
		changed = true;
	    }
	}
	if (_maxSize > col->max)
	{
	    if ( allowRecalc && col->maxCell == _cell ) {
		recalc = true;
	    } else {
		col->max = _maxSize;
		col->maxCell = _cell;
		changed = true;
	    }
	}

	// Fixed width is treated as variable

	if (_width.type == col->type && _width.value > col->value ) {
	    if ( allowRecalc && col->widthCell == _cell ) {
		recalc = true;
	    } else {
		col->value = _width.value;
		col->widthCell = _cell;
		changed = true;
	    }
	} else if ( (_width.type > int(col->type) && (!(_width.type==Fixed) || (int(col->type)<=Variable)))
		    || ( col->type == Fixed && !(_width.type==Variable)) ) {
	    if ( allowRecalc && col->widthCell == _cell ) {
		recalc = true;
	    } else {
		col->type = _width.type;
		col->value = _width.value;
		col->widthCell = _cell;
		changed = true;
	    }
	}
    }
    
    if ( recalc )
        recalcColInfo( col );
    
    if ( changed )
	setMinMaxKnown(false);

    if ( recalc || changed )
	colWidthKnown = false;

#ifdef TABLE_DEBUG
    kdDebug( 6040 ) << "(" << this << "):addColInfo():" << endl;
    kdDebug( 6040 ) << "    startCol=" << col->start << " span=" << col->span << endl;
    kdDebug( 6040 ) << "    min=" << col->min << " max=" << col->max << endl;
    kdDebug( 6040 ) << "    type=" << col->type << " width=" << col->value << endl;
#endif

}

void RenderTable::spreadSpanMinMax(int col, int span, int distmin,
    int distmax, LengthType type)
{
#ifdef TABLE_DEBUG
    kdDebug( 6040 ) << "RenderTable::spreadSpanMinMax() " << type << " " << distmin << " " << distmax << endl;
#endif

    if (distmin<1)
        distmin=0;
    if (distmax<1)
        distmax=0;
    if (distmin<1 && distmax<1)
        return;


    bool hasUsableCols=false;
    int tmax=distmax;
    int tmin=distmin;
    int c;

    for (c=col; c < col+span ; ++c)
    {

        if (colType[c]<=type || (type == Variable && distmax==0))
        {
            hasUsableCols=true;
            break;
        }
    }

    if (hasUsableCols) {
        // spread span maxWidth
        for (int i = LengthType(Variable); i <= int(Fixed) && i <= type && tmax; ++i)
            tmax = distributeMaxWidth(tmax,type,LengthType(i),col,span);


        // spread span minWidth
        for (int i = LengthType(Variable); i <= int(Fixed) && i <= type && tmin; ++i)
            tmin = distributeMinWidth(tmin,type,LengthType(i),col,span,true);

        // force spread rest of the minWidth
        for (int i = LengthType(Variable); i <= int(Fixed) && tmin; ++i)
            tmin = distributeMinWidth(tmin,type,LengthType(i),col,span,false);

        for (int c=col; c < col+span ; ++c)
            colMaxWidth[c]=KMAX(colMinWidth[c],colMaxWidth[c]);
    }
}


int RenderTable::distributeMinWidth(int distrib, LengthType distType,
            LengthType toType, int start, int span, bool mlim )
{
//    kdDebug( 6040 ) << "MINDIST, " << distrib << " pixels of type " << distType << " to type " << toType << " cols sp=" << span << " " << endl;

    int olddis=0;
    int c=start;
    int totper=0;

    int tdis = distrib;

    if (!mlim)
    {
        // first target unused columns
        for(; c<start+span; ++c)
        {
            if (colInfos[0]->at(c)==0)
            {
                colMinWidth[c]+=tdis;
                colType[c]=distType;
                tdis=0;
                break;
            }
        }
    }

    if (toType==Percent || toType==Relative)
        for (int n=start;n<start+span;n++)
            if (colType[n]==Percent || colType[n]==Relative) totper += colValue[n];

    c=start;
    while(tdis>0)
    {
//      kdDebug( 6040 ) << c << ": ct=" << colType[c] << " min=" << colMinWidth[c]
//                << " max=" << colMaxWidth[c] << endl;
        if (colType[c]==toType || (mlim && colMaxWidth[c]-colMinWidth[c]>0))
        {
            int delta = distrib/span;
            if (totper)
                delta=(distrib * colValue[c])/totper;
            if (mlim)
                delta = KMIN(delta,colMaxWidth[c]-colMinWidth[c]);

            delta = KMIN(tdis,delta);

            if (delta==0 && tdis && (!mlim || colMaxWidth[c]>colMinWidth[c]))
                delta=1;

//            kdDebug( 6040 ) << "delta=" << delta << endl;

            colMinWidth[c]+=delta;
            if (mlim)
                colType[c]=distType;
            tdis-=delta;
        }
        if (++c==start+span)
        {
            c=start;
            if (olddis==tdis)
                break;
            olddis=tdis;
        }
    }

    return tdis;
}



int RenderTable::distributeMaxWidth(int distrib, LengthType/* distType*/,
            LengthType toType, int start, int span)
{
//    kdDebug( 6040 ) << "MAXDIST, " << distrib << " pixels of type " << distType << " to type " << toType << " cols sp=" << span << " " << endl;
    int olddis=0;
    int c=start;

    int tdis = distrib;

    // spread span maxWidth evenly
    c=start;
    while(tdis>0)
    {
//      kdDebug( 6040 ) << c << ": ct=" << colType[c] << " min=" << colMinWidth[c]
//                << " max=" << colMaxWidth[c] << endl;

        if (colType[c]==toType)
        {
            colMaxWidth[c]+=distrib/span;
            tdis-=distrib/span;
            if (tdis<span)
            {
                colMaxWidth[c]+=tdis;
                tdis=0;
            }
        }
        if (++c==start+span)
        {
            c=start;
            if (olddis==tdis)
                break;
            olddis=tdis;
        }
    }
    return tdis;
}



void RenderTable::calcSingleColMinMax(int c, ColInfo* col)
{
#ifdef TABLE_DEBUG
    kdDebug( 6040 ) << "RenderTable::calcSingleColMinMax()" << endl;
#endif

    int span=col->span;
    int smin = col->min;
    int smax = col->max;

    if (span==1)
    {
      //kdDebug( 6040 ) << "col (s=1) c=" << c << ",m=" << smin << ",x=" << smax << endl;
        colMinWidth[c] = smin;
        colMaxWidth[c] = smax;
        colValue[c] = col->value;
        colType[c] = col->type;
    }
    else
    {
        int oldmin=0;
        int oldmax=0;
        for (int o=c; o<c+span; ++o)
        {
            oldmin+=colMinWidth[o];
            oldmax+=colMaxWidth[o];
        }
        int spreadmin = smin-oldmin-(span-1)*spacing;
//        kdDebug( 6040 ) << "colmin " << smin  << endl;
//        kdDebug( 6040 ) << "oldmin " << oldmin  << endl;
//        kdDebug( 6040 ) << "oldmax " << oldmax  << endl;
//      kdDebug( 6040 ) << "spreading span " << spreadmin  << endl;
        spreadSpanMinMax
            (c, span, spreadmin, 0 , col->type);
    }

}

void RenderTable::calcFinalColMax(int c, ColInfo* col)
{
#ifdef TABLE_DEBUG
    kdDebug( 6040 ) << "RenderTable::calcPercentRelativeMax()" << endl;
#endif
    int span=col->span;

    int oldmax=0;
    int oldmin=0;

    for (int o=c; o<c+span; ++o)
    {
        oldmax+=colMaxWidth[o];
        oldmin+=colMinWidth[o];
    }

    int smax = col->max;

    if (col->type == Percent)
    {
        smax = m_width * col->value / KMAX(100u,totalPercent);
    }
    else if (col->type == Relative && totalRelative)
    {
        smax = m_width * col->value / totalRelative;
    }

    smax = KMAX(smax,oldmin);

//    kdDebug( 6040 ) << "smin " << smin << " smax " << smax << " span " << span << endl;
    if (span==1)
    {
//       kdDebug( 6040 ) << "col (s=1) c=" << c << ",m=" << smin << ",x=" << smax << endl;
       colMaxWidth[c] = smax;
       colType[c] = col->type;
    }
    else
    {
        int spreadmax = smax-oldmax-(span-1)*spacing;
//      kdDebug( 6040 ) << "spreading span " << spreadmax << endl;
        spreadSpanMinMax
            (c, span, 0, spreadmax, col->type);
    }

}



void RenderTable::calcColMinMax()
{
// Calculate minmimum and maximum widths for all
// columns.
// Calculate min and max width for the table.

    // PHASE 1, prepare

    colMinWidth.fill(0);
    colMaxWidth.fill(0);

    int availableWidth = containingBlockWidth();

    int realMaxWidth=spacing;

    int* spanPercent = new int[maxColSpan];
    int* spanPercentMax = new int[maxColSpan];

    LengthType widthType = style()->width().type;

#ifdef TABLE_DEBUG
    kdDebug( 6040 ) << "RenderTable(" << this << ")::calcColMinMax(), maxCelSpan" << maxColSpan << " totalCols " << totalCols << " widthtype=" << widthType << " widthval=" << style()->width().value << endl;
#endif

    Length l;
    if ( ( l = style()->marginLeft() ).isFixed() )
        availableWidth -= QMAX( l.value, 0 );
    if ( ( l = style()->marginRight() ).isFixed() )
        availableWidth -= QMAX( l.value, 0 );

    availableWidth -= QMAX( style()->borderLeftWidth(), 0 );
    availableWidth -= QMAX( style()->borderRightWidth(), 0 );
    // PHASE 2, calculate simple minimums and maximums
    for ( unsigned int s=0;  (int)s<maxColSpan ; ++s)
    {
        ColInfoLine* spanCols = colInfos[s];

        int spanMax=0;
        spanPercentMax[s] = spacing;
        spanPercent[s] = 0;

        for ( unsigned int c=0; c<totalCols-s; ++c)
        {
            ColInfo* col;
            col = spanCols->at(c);

            if (!col || col->span==0)
                continue;
#ifdef TABLE_DEBUG
            kdDebug( 6040 ) << " s=" << s << " c=" << c << " min=" << col->min << " value=" << col->value  <<
                        " max="<<col->max<< endl;
#endif

            spanMax += col->max + spacing;

            if (col->type==Percent)
            {
                spanPercentMax[s] += col->max+spacing;
                spanPercent[s] += col->value;
            }

            calcSingleColMinMax(c, col);

            if ( col->span>1 && widthType != Percent
                && (col->type==Variable || col->type==Fixed))
            {
                calcFinalColMax(c, col);
            }

        }

        // this should be some sort of generic path algorithm
        // but we'll just hack it for now
        if (spanMax>realMaxWidth)
            realMaxWidth=spanMax;
    }

    // PHASE 3, calculate table width

    totalPercent=0;
    totalRelative=0;

    int minPercent=0;
    int maxPercent=0;

    int minRel=0;
    int minVar=0;
    int maxRel=0;
    int maxVar=0;
    hasPercent=false;
    bool hasRel=false;
    bool hasVar=false;

    int maxPercentColumn=0;
    int maxTentativePercentWidth=0;

    m_minWidth = spacing;
    m_maxWidth = spacing;

    for(int i = 0; i < (int)totalCols; i++)
    {
        m_minWidth += colMinWidth[i] + spacing;
        m_maxWidth += colMaxWidth[i] + spacing;

        switch(colType[i])
        {
        case Percent:
            if (!hasPercent){
                hasPercent=true;
                minPercent=maxPercent=spacing;
            }
            totalPercent += colValue[i];

            maxPercentColumn = KMAX(colValue[i],maxPercentColumn);

            minPercent += colMinWidth[i] + spacing;
            maxPercent += colMaxWidth[i] + spacing;

            maxTentativePercentWidth = KMAX(colValue[i]==0?0:colMaxWidth[i]*100/colValue[i],
                    maxTentativePercentWidth);
            break;
        case Relative:
            if (!hasRel){
                hasRel=true;
                minRel=maxRel=spacing;
            }
            totalRelative += colValue[i] ;
            minRel += colMinWidth[i] + spacing;
            maxRel += colMaxWidth[i] + spacing;
            break;
        case Variable:
        case Fixed:
        default:
            if (!hasVar){
                hasVar=true;
                minVar=maxVar=spacing;
            }
            minVar += colMinWidth[i] + spacing;
            maxVar += colMaxWidth[i] + spacing;
        }

    }

    for ( int s=0; s<maxColSpan ; ++s)
    {
        maxPercent = KMAX(spanPercentMax[s],maxPercent);
        totalPercent = KMAX(spanPercent[s],int(totalPercent));
    }
    delete[] spanPercentMax;
    delete[] spanPercent;

    if (widthType <= Relative && hasPercent) {
	int tot = KMIN(100u, totalPercent );

        if (tot>0)
    	    m_maxWidth = maxPercent*100/tot;

        if (tot<100)
            m_maxWidth = KMAX( short((maxVar+maxRel)*100/(100-tot)), m_maxWidth );
        else if (hasRel || hasVar || ((int)totalPercent>maxPercentColumn && maxPercentColumn>=100))
            m_maxWidth = 10000;
        else if (totalPercent>0)
            m_maxWidth = KMAX(short(maxTentativePercentWidth*100/totalPercent), m_maxWidth);

    }



    // PHASE 5, set table min and max to final values

    if(widthType == Fixed) {
	m_width = style()->width().value;
	if ( m_width < m_minWidth )
	    m_width = m_minWidth;
        m_minWidth = m_maxWidth = m_width;
    } else {
        if (realMaxWidth > m_maxWidth)
            m_maxWidth = realMaxWidth;
    }

    m_minWidth += borderLeft() + borderRight();
    m_maxWidth += borderLeft() + borderRight();

#ifdef TABLE_DEBUG
    kdDebug( 6040 ) << "TABLE width=" << m_width <<
                " m_minWidth=" << m_minWidth <<
                " m_maxWidth=" << m_maxWidth <<
                " realMaxWidth=" << realMaxWidth << endl;
#endif
}

void RenderTable::calcWidth()
{
    if ( isPositioned() ) {
        calcAbsoluteHorizontal();
    }

    int borderWidth = borderLeft() + borderRight();
    RenderObject *cb = containingBlock();
    int availableWidth = cb->contentWidth() - borderWidth;


    LengthType widthType = style()->width().type;
    if(widthType > Relative) {
	// Percent or fixed table
        m_width = style()->width().minWidth( availableWidth );
        if(m_minWidth > m_width) m_width = m_minWidth;
	//kdDebug( 6040 ) << "1 width=" << m_width << " minWidth=" << m_minWidth << " availableWidth=" << availableWidth << " " << endl;
    } else if (hasPercent) {
        m_width = KMIN(short( availableWidth ),m_maxWidth);
//        kdDebug( 6040 ) << "width=" << m_width << " maxPercent=" << maxPercent << " maxVar=" << maxVar << " " << endl;
    } else {
        m_width = KMIN(short( availableWidth ),m_maxWidth);
    }

    // restrict width to what we really have in case we flow around floats
    if ( style()->flowAroundFloats() && cb->isFlow() )
	m_width = QMIN( static_cast<RenderFlow *>(cb)->lineWidth( m_y ) - borderWidth, m_width );

    m_width = KMAX (m_width, m_minWidth);

    m_marginRight=0;
    m_marginLeft=0;

    calcHorizontalMargins(style()->marginLeft(),style()->marginRight(),availableWidth);

    // PHASE 4, calculate maximums for percent and relative columns. We can't do this in
    // the minMax calculations, as we do not have the correct table width there.

    for ( unsigned int s=0;  (int)s<maxColSpan ; ++s) {
        ColInfoLine* spanCols = colInfos[s];
        for ( unsigned int c=0; c<totalCols-s; ++c) {
            ColInfo* col;
            col = spanCols->at(c);

            if (!col || col->span==0)
                continue;
            if (col->type==Variable || col->type==Fixed)
                continue;

            calcFinalColMax(c, col);
        }
    }

    m_width += borderWidth;
}

void RenderTable::calcColWidth(void)
{

#ifdef TABLE_DEBUG
    kdDebug( 6040 ) << "START calcColWidth() this = " << this << endl;
    kdDebug( 6040 ) << "maxColSpan = " << maxColSpan << endl;
#endif

    colWidthKnown = true;

    if (totalCols==0)
        return;

    /*
     * Set actColWidth[] to column minimums, it will
     * grow from there.
     * Collect same statistics for future use.
     */

    int actWidth = spacing + borderLeft() + borderRight();

    int minFixed = 0;
    int minPercent = 0;
    int minRel = 0;
    int minVar = 0;

    int maxFixed = 0;
    int maxPercent = 0;
    int maxRel = 0;
    int maxVar = 0;

    int numFixed = 0;
    int numPercent = 0;
    int numRel = 0;
    int numVar = 0;

    actColWidth.fill(0);

    unsigned int i;
    for(i = 0; i < totalCols; i++)
    {
        actColWidth[i] = colMinWidth[i];
        actWidth += actColWidth[i] + spacing;

        switch(colType[i])
        {
        case Fixed:
            minFixed += colMinWidth[i];
            maxFixed += colMaxWidth[i];
            numFixed++;
            break;
        case Percent:
            minPercent += colMinWidth[i];
            maxPercent += colMaxWidth[i];
            numPercent++;
            break;
        case Relative:
            minRel += colMinWidth[i];
            maxRel += colMaxWidth[i];
            numRel++;
            break;
        case Variable:
        default:
            minVar += colMinWidth[i];
            maxVar += colMaxWidth[i];
            numVar++;
        }

    }

#ifdef TABLE_DEBUG
    for(int i = 0; i < (int)totalCols; i++)
    {
        kdDebug( 6040 ) << "Start->target " << i << ": " << actColWidth[i] << "->" << colMaxWidth[i] << " type=" << colType[i] << endl;
    }
#endif

    int toAdd = m_width - actWidth;      // what we can add

#ifdef TABLE_DEBUG
    kdDebug( 6040 ) << "toAdd = width - actwidth: " << toAdd << " = " << m_width << " - " << actWidth << endl;
#endif

    /*
     * distribute the free width among the columns so that
     * they reach their max width.
     * Order: percent->fixed->relative->variable
     */

    toAdd = distributeWidth(toAdd,Percent,numPercent);
    toAdd = distributeWidth(toAdd,Fixed,numFixed);
    toAdd = distributeWidth(toAdd,Relative,numRel);
    toAdd = distributeWidth(toAdd,Variable,numVar);

#ifdef TABLE_DEBUG
    for(int i = 0; i < (int)totalCols; i++)
    {
        kdDebug( 6040 ) << "distributeWidth->target " << i << ": " << actColWidth[i] << "->" << colMaxWidth[i] << " type=" << colType[i] << endl;
    }
#endif

    /*
     * Some width still left?
     * Reverse order, variable->relative->percent
     */

    if ( numVar ) toAdd = distributeRest(toAdd,Variable,maxVar);
    if ( numRel ) toAdd = distributeRest(toAdd,Relative,maxRel);
    if ( numPercent ) toAdd = distributeRest(toAdd,Percent,maxPercent);
    if ( numFixed ) toAdd = distributeRest(toAdd,Fixed,maxFixed);

#ifdef TABLE_DEBUG
    for(int i = 0; i < (int)totalCols; i++)
    {
        kdDebug( 6040 ) << "distributeRest->target " << i << ": " << actColWidth[i] << "->" << colMaxWidth[i] << " type=" << colType[i] << endl;
    }
#endif
    /*
     * If something remains, put it to the last column
     */
    actColWidth[totalCols-1] += toAdd;

    /*
     * Calculate the placement of colums
     */

    columnPos.fill(0);
    columnPos[0] = spacing;
    for(i = 1; i <= totalCols; i++)
    {
        columnPos[i] += columnPos[i-1] + actColWidth[i-1] + spacing;
#ifdef TABLE_DEBUG
        kdDebug( 6040 ) << "Actual width col " << i << ": " << actColWidth[i-1] << " pos = " << columnPos[i-1] << endl;
#endif
    }

#ifdef TABLE_DEBUG
    if(m_width - borderLeft() - borderLeft() != columnPos[totalCols] )
        kdDebug( 6040 ) << "========> table layout error!!! <===============================" << endl;
    kdDebug( 6040 ) << "total width = " << m_width << " colpos = " << columnPos[totalCols] << endl;
#endif

}


int RenderTable::distributeWidth(int distrib, LengthType type, int typeCols )
{
    int olddis=0;
    int c=0;

    int tdis = distrib;
#ifdef TABLE_DEBUG
    kdDebug( 6040 ) << "DISTRIBUTING " << distrib << " pixels to type " << type << " cols" << endl;
#endif

    while(tdis>0)
    {
        if (colType[c]==type)
        {
            int delta = KMIN(distrib/typeCols,colMaxWidth[c]-actColWidth[c]);
            delta = KMIN(tdis,delta);
            if (delta==0 && tdis && colMaxWidth[c]>actColWidth[c])
                delta=1;
            actColWidth[c]+=delta;
            tdis-=delta;
        }
        if (++c == (int)totalCols)
        {
            c=0;
            if (olddis==tdis)
                break;
            olddis=tdis;
        }
    }
    return tdis;
}


int RenderTable::distributeRest(int distrib, LengthType type, int divider )
{
    if ( !divider )
	return distrib;

#ifdef TABLE_DEBUG
    kdDebug( 6040 ) << "DISTRIBUTING rest, " << distrib << " pixels to type " << type << " cols" << endl;
#endif

    int olddis=0;
    int c=0;

    int tdis = distrib;

    while(tdis>0)
    {
        if (colType[c]==type)
        {
            int delta = colMaxWidth[c] * distrib / divider;
            delta=KMIN(delta,tdis);
            actColWidth[c] += delta;
            tdis -= delta;
        }
        if (++c == (int)totalCols)
        {
            c=0;
            if (olddis==tdis)
                break;
            olddis=tdis;
        }
    }
    return tdis;
}


void RenderTable::calcRowHeight(int r)
{
    unsigned int c;
    int indx;//, borderExtra = border ? 1 : 0;
    RenderTableCell *cell;

    rowHeights.resize( totalRows+1 );
    rowBaselines.resize( totalRows );
    rowHeights[0] =  spacing + borderTop();

  //int oldheight = rowHeights[r+1] - rowHeights[r];
    rowHeights[r+1] = 0;

    int baseline=0;
    int bdesc=0;
    int ch;

    for ( c = 0; c < totalCols; c++ )
    {
        if ( ( cell = cells[r][c] ) == 0 )
            continue;
        if ( c < totalCols - 1 && cell == cells[r][c+1] )
            continue;
        if ( r < (int)totalRows - 1 && cells[r+1][c] == cell )
            continue;

        if ( ( indx = r - cell->rowSpan() + 1 ) < 0 )
            indx = 0;

        ch = cell->style()->height().width(0);
        if ( cell->height() > ch)
            ch = cell->height();

        int rowPos = rowHeights[ indx ] + ch +
             spacing ; // + padding

        if ( rowPos > rowHeights[r+1] )
            rowHeights[r+1] = rowPos;

        // find out the baseline
        EVerticalAlign va = cell->style()->verticalAlign();
        if (va == BASELINE || va == TEXT_BOTTOM || va == TEXT_TOP
            || va == SUPER || va == SUB)
        {
            int b=cell->baselinePosition();

            if (b>baseline)
                baseline=b;

            int td = rowHeights[ indx ] + ch - b;
            if (td>bdesc)
                bdesc = td;
        }
    }

    //do we have baseline aligned elements?
    if (baseline)
    {
        // increase rowheight if baseline requires
        int bRowPos = baseline + bdesc  + spacing ; // + 2*padding
        if (rowHeights[r+1]<bRowPos)
            rowHeights[r+1]=bRowPos;

        rowBaselines[r]=baseline;
    }

    if ( rowHeights[r+1] < rowHeights[r] )
        rowHeights[r+1] = rowHeights[r];
}

void RenderTable::layout()
{
    KHTMLAssert( !layouted() );
    KHTMLAssert( minMaxKnown() );

    //kdDebug( 6040 ) << renderName() << "(Table)"<< this << " ::layout0() width=" << width() << ", layouted=" << layouted() << endl;

    _lastParentWidth = containingBlockWidth();

    m_height = 0;

    int oldWidth = m_width;
    calcWidth();
    if ( !colWidthKnown || oldWidth != m_width )
	calcColWidth();

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(Table)::layout1() width=" << width() << ", marginLeft=" << marginLeft() << " marginRight=" << marginRight() << endl;
#endif


    setCellWidths();

    // ### collapse caption margin, left, right
    if(tCaption && tCaption->style()->captionSide() != CAPBOTTOM)
    {
        tCaption->setPos(m_height, tCaption->marginLeft());
	if ( !tCaption->layouted() )
	    tCaption->layout();
        m_height += tCaption->height() + tCaption->marginTop() + tCaption->marginBottom();
    }

    // layout child objects
    RenderObject *child = firstChild();
    while( child ) {
	if ( child != tCaption && !child->layouted() )
	    child->layout();
	child = child->nextSibling();
    }

    // layout rows
    layoutRows(m_height);

    m_height += rowHeights[totalRows];
    m_height += borderBottom();

    if(tCaption && tCaption->style()->captionSide()==CAPBOTTOM)
    {
        tCaption->setPos(tCaption->marginLeft(), m_height);
	if ( !tCaption->layouted() )
	    tCaption->layout();
        m_height += tCaption->height() + tCaption->marginTop() + tCaption->marginBottom();
    }

    //kdDebug(0) << "table height: " << m_height << endl;

    calcHeight();

    //kdDebug(0) << "table height: " << m_height << endl;

    // table can be containing block of positioned elements.
    // ### only pass true if width or height changed.
    layoutSpecialObjects( true );

    setLayouted();

}


void RenderTable::layoutRows(int yoff)
{
    int rHeight;
    int indx, rindx;

    for ( unsigned int r = 0; r < totalRows; r++ ) {
        calcRowHeight(r);
    }


    // html tables with percent height are relative to view
    Length h = style()->height();
    int th=0;
    if (h.isFixed())
        th = h.value;
    else if (h.isPercent())
    {
        Length ch = containingBlock()->style()->height();
        if (ch.isFixed())
            th = h.width(ch.value);
        else
        {
            // check we or not inside a table
            RenderObject* ro = parent();
            for (; ro && !ro->isTableCell(); ro=ro->parent());
            if (!ro)
            {
		// we need to substract the bodys margins
		// ### fixme: use exact values here.
                th = h.width(viewRect().height() - 20 );
                // not really, but this way the view height change
                // gets propagated correctly
                setOverhangingContents();
            }
        }
    }
    if (th && totalRows && rowHeights[totalRows])
    {
        th-=(totalRows+1)*spacing;
        int dh = th-rowHeights[totalRows];
        if (dh>0)
        {
            int tot=rowHeights[totalRows];
            int add=0;
            int prev=rowHeights[0];
            for ( unsigned int r = 0; r < totalRows; r++ )
            {
                //weight with the original height
                add+=dh*(rowHeights[r+1]-prev)/tot;
                prev=rowHeights[r+1];
                rowHeights[r+1]+=add;
            }
            rowHeights[totalRows]=th;
        }
    }


    for ( unsigned int r = 0; r < totalRows; r++ )
    {
        for ( unsigned int c = 0; c < totalCols; c++ )
        {
            RenderTableCell *cell = cells[r][c];
            if (!cell)
                continue;
            if ( c < totalCols - 1 && cell == cells[r][c+1] )
                continue;
            if ( r < totalRows - 1 && cell == cells[r+1][c] )
                continue;

            if ( ( indx = c-cell->colSpan()+1 ) < 0 )
                indx = 0;

            if ( ( rindx = r-cell->rowSpan()+1 ) < 0 )
                rindx = 0;

            //kdDebug( 6040 ) << "setting position " << r << "/" << indx << "-" << c << ": " << //columnPos[indx] + padding << "/" << rowHeights[rindx] << " " << endl;
            rHeight = rowHeights[r+1] - rowHeights[rindx] -
                spacing;

            EVerticalAlign va = cell->style()->verticalAlign();
            int te=0;
            switch (va)
            {
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
#ifdef DEBUG_LAYOUT
            kdDebug( 6040 ) << "CELL te=" << te << ", be=" << rHeight - cell->height() - te << ", rHeight=" << rHeight << ", valign=" << va << endl;
#endif
            cell->setCellTopExtra( te );
            cell->setCellBottomExtra( rHeight - cell->height() - te);

            if (style()->direction()==RTL)
            {
                cell->setPos( columnPos[(int)totalCols]
                    - columnPos[(int)(indx+cell->colSpan())] + borderLeft(),
                    rowHeights[rindx]+yoff );
            }
            else
                cell->setPos( columnPos[indx] + borderLeft(), rowHeights[rindx]+yoff );

            cell->setRowHeight(rHeight);
            // ###
            // cell->setHeight(cellHeight);
        }
    }
}


void RenderTable::setCellWidths()
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(Table, this=0x" << this << ")::setCellWidths()" << endl;
#endif


    int indx;
    FOR_EACH_CELL( r, c, cell)
        {
            if ( ( indx = c-cell->colSpan()+1) < 0 )
                indx = 0;
            int w = columnPos[c+1] - columnPos[ indx ] - spacing ; //- padding*2;

#ifdef TABLE_DEBUG
            kdDebug( 6040 ) << "0x" << this << ": setting width " << r << "/" << indx << "-" << c << " (0x" << cell << "): " << w << " " << endl;
#endif
	    int oldWidth = cell->width();
            cell->setWidth( w );
            if ( w != oldWidth )
                cell->setLayouted(false);
        }
    END_FOR_EACH

}

void RenderTable::print( QPainter *p, int _x, int _y,
                                  int _w, int _h, int _tx, int _ty)
{

//     if(!layouted()) return;

    _tx += xPos();
    _ty += yPos();

#ifdef TABLE_PRINT
    kdDebug( 6040 ) << "RenderTable::print() w/h = (" << width() << "/" << height() << ")" << endl;
#endif
    if (!overhangingContents() && !isRelPositioned() && !isPositioned())
    {
        if((_ty > _y + _h) || (_ty + height() < _y)) return;
        if((_tx > _x + _w) || (_tx + width() < _x)) return;
    }

#ifdef TABLE_PRINT
     kdDebug( 6040 ) << "RenderTable::print(2) " << _tx << "/" << _ty << " (" << _x << "/" << _y << ")" << endl;
#endif
    // the case below happens during parsing
    // when we have a new table that never got layouted. Don't print it.
    if ( totalRows == 1 && rowHeights[1] == 0 )
	return;

    if(style()->visibility() == VISIBLE)
         printBoxDecorations(p, _x, _y, _w, _h, _tx, _ty);

    int topextra = 0;

    if ( tCaption ) {
        tCaption->print( p, _x, _y, _w, _h, _tx, _ty );
        if (tCaption->style()->captionSide() != CAPBOTTOM)
            topextra = - borderTopExtra();
    }

    // check which rows and cols are visible and only print these
    // ### fixme: could use a binary search here
    unsigned int startrow = 0;
    unsigned int endrow = totalRows;
    for ( ; startrow < totalRows; startrow++ ) {
	if ( _ty + topextra + rowHeights[startrow+1] > _y )
	    break;
    }
    for ( ; endrow > 0; endrow-- ) {
	if ( _ty + topextra + rowHeights[endrow-1] < _y + _h )
	    break;
    }
    unsigned int startcol = 0;
    unsigned int endcol = totalCols;
    if ( style()->direction() == LTR ) {
    for ( ; startcol < totalCols; startcol++ ) {
	if ( _tx + columnPos[startcol+1] > _x )
	    break;
    }
    for ( ; endcol > 0; endcol-- ) {
	if ( _tx + columnPos[endcol-1] < _x + _w )
	    break;
    }
    }

    // draw the cells
    for ( unsigned int r = startrow; r < endrow; r++ ) {
        for ( unsigned int c = startcol; c < endcol; c++ ) {
            RenderTableCell *cell = cells[r][c];
            if (!cell)
                continue;
            if ( (c < endcol - 1) && (cell == cells[r][c+1]) )
                continue;
            if ( (r < endrow - 1) && (cells[r+1][c] == cell) )
                continue;
#ifdef DEBUG_LAYOUT
	    kdDebug( 6040 ) << "printing cell " << r << "/" << c << endl;
#endif
	    cell->print( p, _x, _y, _w, _h, _tx, _ty);
	}
    }

    if ( specialObjects )
	printSpecialObjects( p, _x, _y, _w, _h, _tx, _ty);

#ifdef BOX_DEBUG
    outlineBox(p, _tx, _ty, "blue");
#endif
}

void RenderTable::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    if ( needsCellsRecalc )
	recalcCells();
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(Table " << this << ")::calcMinMaxWidth()" <<  endl;
#endif

    /*
     * Calculate min and max width for every column,
     * Max width for percent cols are still not accurate, but as they don't
     * influence the total max width of the table we don't care.
     */
     calcColMinMax();

    setMinMaxKnown();
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "END: (Table " << this << ")::calcMinMaxWidth() min = " << m_minWidth << " max = " << m_maxWidth <<  endl;
#endif
}

void RenderTable::close()
{
//    kdDebug( 6040 ) << "RenderTable::close()" << endl;
    setLayouted(false);
    setMinMaxKnown(false);
}

int RenderTable::borderTopExtra()
{
    if (tCaption && tCaption->style()->captionSide()!=CAPBOTTOM)
        return -(tCaption->height() + tCaption->marginBottom() +  tCaption->marginTop());
    else
        return 0;

}

int RenderTable::borderBottomExtra()
{
    if (tCaption && tCaption->style()->captionSide()==CAPBOTTOM)
        return -(tCaption->height() + tCaption->marginBottom() +  tCaption->marginTop());
    else
        return 0;
}

void RenderTable::setNeedsCellsRecalc()
{
    needsCellsRecalc = true;
    setMinMaxKnown(false);
    setLayouted(false);
}

void RenderTable::recalcCells()
{
    needsCellsRecalc = false;

    _oldColElem = 0;
    m_maxWidth = 0;

    row = 0;
    col = 0;

    maxColSpan = 0;

    _currentCol=0;

    _lastParentWidth = 0;

    columnPos.resize( 0 );
    columnPos.resize( 1 );
    colMaxWidth.resize( 0 );
    colMaxWidth.resize( 1 );
    colMinWidth.resize( 0 );
    colMinWidth.resize( 1 );
    colValue.resize(0);
    colValue.resize(1);
    colType.resize(0);
    colType.resize(1);
    actColWidth.resize(0);
    actColWidth.resize(1);
    columnPos.fill( 0 );
    colMaxWidth.fill( 0 );
    colMinWidth.fill( 0 );
    colValue.fill(0);
    colType.fill(Variable);
    actColWidth.fill(0);

    columnPos[0] = spacing;

    for (unsigned int r = 0; r < allocRows; r++)
    {
	delete[] cells[r];
    }
    delete[] cells;

    totalCols = 0;   // this should be expanded to the maximum number of cols
                     // by the first row parsed
    totalRows = 1;
    allocRows = 5;   // allocate five rows initially

    cells = new RenderTableCell ** [allocRows];

    for ( unsigned int r = 0; r < allocRows; r++ )
    {
        cells[r] = new RenderTableCell * [totalCols];
        memset( cells[r], 0, totalCols * sizeof( RenderTableCell * ));
    }

    for (RenderObject *s = m_first; s; s = s->nextSibling()) {
	if (s->isTableSection()) {
	    for (RenderObject *r = static_cast<RenderTableSection*>(s)->firstChild(); r; r = r->nextSibling()) {
		if (r->isTableRow()) {
		    startRow();
		    for (RenderObject *c = static_cast<RenderTableRow*>(r)->firstChild(); c; c = c->nextSibling()) {
			if (c->isTableCell())
			    addCell(static_cast<RenderTableCell*>(c));
		    }
		    closeRow();
		}
	    }
	}
    }

    recalcColInfos();
}

#ifndef NDEBUG
void RenderTable::dump(QTextStream *stream, QString ind) const
{
    *stream << " totalCols=" << totalCols;
    *stream << " totalRows=" << totalRows;

    if (tCaption)
	*stream << " tCaption";
    if (head)
	*stream << " head";
    if (foot)
	*stream << " foot";

    if (collapseBorders)
	*stream << " collapseBorders";

// ###    RenderTableCell ***cells;
// ###    QPtrVector<ColInfoLine> colInfos;
// ###    Frame frame;
// ###    Rules rules;
// ###    RenderTableCol *_oldColElem;

    RenderFlow::dump(stream,ind);
}
#endif

// --------------------------------------------------------------------------

RenderTableSection::RenderTableSection(DOM::NodeImpl* node)
    : RenderContainer(node)
{
    // init RenderObject attributes
    setInline(false);   // our object is not Inline
    nrows = 0;
}

RenderTableSection::~RenderTableSection()
{
    // recalc cell info because RenderTable has unguarded pointers
    // stored that point to this RenderTableSection.
    if (table)
        table->setNeedsCellsRecalc();
}

void RenderTableSection::addChild(RenderObject *child, RenderObject *beforeChild)
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(TableSection)::addChild( " << child->renderName()  << ", beforeChild=" <<
                       (beforeChild ? beforeChild->renderName() : "0") << " )" << endl;
#endif
    RenderObject *row = child;

    if ( !child->isTableRow() ) {

        if( !beforeChild )
            beforeChild = lastChild();

        if( beforeChild && beforeChild->isAnonymousBox() )
            row = beforeChild;
        else {
	    RenderObject *lastBox = beforeChild;
	    while ( lastBox && lastBox->parent()->isAnonymousBox() && !lastBox->isTableRow() )
		lastBox = lastBox->parent();
	    if ( lastBox && lastBox->isAnonymousBox() ) {
		lastBox->addChild( child, beforeChild );
		return;
	    } else {
		kdDebug( 6040 ) << "creating anonymous table row" << endl;
		row = new RenderTableRow(0 /* anonymous table */);
		RenderStyle *newStyle = new RenderStyle();
		newStyle->inheritFrom(style());
		newStyle->setDisplay(TABLE_ROW);
		row->setStyle(newStyle);
		row->setIsAnonymousBox(true);
		addChild(row, beforeChild);
	    }
        }
        row->addChild(child);
	child->setLayouted( false );
	child->setMinMaxKnown( false );
        return;
    }

    if (beforeChild)
	table->setNeedsCellsRecalc();

    table->startRow();
    child->setTable(table);
    RenderContainer::addChild(child,beforeChild);
}

#ifndef NDEBUG
void RenderTableSection::dump(QTextStream *stream, QString ind) const
{
    *stream << " nrows=" << nrows;

    RenderContainer::dump(stream,ind);
}
#endif

// -------------------------------------------------------------------------

RenderTableRow::RenderTableRow(DOM::NodeImpl* node)
    : RenderContainer(node)
{
    // init RenderObject attributes
    setInline(false);   // our object is not Inline

    rIndex = -1;
    ncols = 0;
}

RenderTableRow::~RenderTableRow()
{
    if (table)
        table->setNeedsCellsRecalc();
}


long RenderTableRow::rowIndex() const
{
    // ###
    return 0;
}

void RenderTableRow::setRowIndex( long  )
{
    // ###
}

void RenderTableRow::close()
{
    table->closeRow();
    RenderContainer::close();
}

void RenderTableRow::addChild(RenderObject *child, RenderObject *beforeChild)
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(TableRow)::addChild( " << child->renderName() << " )"  << ", " <<
                       (beforeChild ? beforeChild->renderName() : "0") << " )" << endl;
#endif
    RenderTableCell *cell;

    if ( !child->isTableCell() ) {
        if ( !beforeChild )
            beforeChild = lastChild();
        RenderTableCell *cell;
        if( beforeChild && beforeChild->isAnonymousBox() && beforeChild->isTableCell() )
            cell = static_cast<RenderTableCell *>(beforeChild);
        else {
	    RenderObject *lastBox = beforeChild;
	    while ( lastBox && lastBox->parent()->isAnonymousBox() && !lastBox->isTableCell() )
		lastBox = lastBox->parent();
	    if ( lastBox && lastBox->isAnonymousBox() ) {
		lastBox->addChild( child, beforeChild );
		return;
	    } else {
//          kdDebug( 6040 ) << "creating anonymous table cell" << endl;
		cell = new RenderTableCell(0 /* anonymous object */);
		RenderStyle *newStyle = new RenderStyle();
		newStyle->inheritFrom(style());
		newStyle->setDisplay(TABLE_CELL);
		cell->setStyle(newStyle);
		cell->setIsAnonymousBox(true);
		addChild(cell, beforeChild);
	    }
        }
        cell->addChild(child);
	child->setLayouted( false );
	child->setMinMaxKnown( false );
        return;
    } else
        cell = static_cast<RenderTableCell *>(child);

    cell->setTable(table);
    cell->setRowImpl(this);
    table->addCell(cell);  // ### may not work for beforeChild != 0

    RenderContainer::addChild(cell,beforeChild);

    if (beforeChild || nextSibling())
	table->setNeedsCellsRecalc();

}

void RenderTableRow::repaint()
{
    if ( table ) table->repaint();
}

#ifndef NDEBUG
void RenderTableRow::dump(QTextStream *stream, QString ind) const
{
    *stream << " rIndex = " << rIndex;
    *stream << " ncols = " << ncols;

    RenderContainer::dump(stream,ind);
}
#endif

void RenderTableRow::layout()
{
    KHTMLAssert( !layouted() );
    KHTMLAssert( minMaxKnown() );

    RenderObject *child = firstChild();
    while( child ) {
	KHTMLAssert( child->isTableCell() );
	if ( !child->layouted() ) {
	    RenderTableCell *cell = static_cast<RenderTableCell *>(child);
	    cell->calcVerticalMargins();
	    cell->layout();
	    cell->setCellTopExtra(0);
	    cell->setCellBottomExtra(0);
	}
	child = child->nextSibling();
    }
    setLayouted();
}

// -------------------------------------------------------------------------

RenderTableCell::RenderTableCell(DOM::NodeImpl* _node)
  : RenderFlow(_node)
{
  _col = -1;
  _row = -1;
  updateFromElement();
  _id = 0;
  rowHeight = 0;
  m_table = 0;
  rowimpl = 0;
  setSpecialObjects(true);
  _topExtra = 0;
  _bottomExtra = 0;
}

RenderTableCell::~RenderTableCell()
{
    if (m_table)
        m_table->setNeedsCellsRecalc();
}

void RenderTableCell::updateFromElement()
{
  DOM::NodeImpl *node = element();
  if ( node && (node->id() == ID_TD || node->id() == ID_TH) ) {
      DOM::HTMLTableCellElementImpl *tc = static_cast<DOM::HTMLTableCellElementImpl *>(node);
      cSpan = tc->colSpan();
      rSpan = tc->rowSpan();
      nWrap = tc->noWrap();
  } else {
      cSpan = rSpan = 1;
      nWrap = false;
  }
}

void RenderTableCell::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(TableCell)::calcMinMaxWidth() known=" << minMaxKnown() << endl;
#endif

    //if(minMaxKnown()) return;

    int oldMin = m_minWidth;
    int oldMax = m_maxWidth;

    RenderFlow::calcMinMaxWidth();

    // NOWRAP Should apply even in cases where the width is fixed.  Consider the following 
    // test case.
    // <table border="1">
    // <tr>
    // <td width="175">Check this text out. It shouldn't shrink below 300px.</td>
    // <td width="100%"></td>
    // </tr>
    // <tr><td width="300" nowrap></tr>
    // </table>
    // The width of the cell should not be allowed to fall below the max width of the column when
    // nowrap is enabled on any cell in the column.  - dwh
    if(nWrap)
        m_minWidth = m_maxWidth;
    
    if (m_minWidth!=oldMin || m_maxWidth!=oldMax) {
        m_table->addColInfo(this);
    }
    setMinMaxKnown();
}

void RenderTableCell::calcWidth()
{
}

void RenderTableCell::setWidth( int width )
{
    if ( width != m_width ) {
	m_width = width;
	m_widthChanged = true;
    }
}

void RenderTableCell::close()
{
    RenderFlow::close();

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderTableCell)::close() total height =" << m_height << endl;
#endif
}


void RenderTableCell::repaintRectangle(int x, int y, int w, int h, bool f)
{
    y += _topExtra;
    RenderFlow::repaintRectangle(x, y, w, h, f);
}

bool RenderTableCell::absolutePosition(int &xPos, int &yPos, bool f)
{
    bool ret = RenderFlow::absolutePosition(xPos, yPos, f);
    if (ret)
      yPos += _topExtra;
    return ret;
}

short RenderTableCell::baselinePosition( bool ) const
{
    RenderObject *o = firstChild();
    int offset = paddingTop();
    if ( !o ) return offset;
    while ( o->firstChild() ) {
	offset += paddingTop() + borderTop();
	o = o->firstChild();
    }
    offset += o->baselinePosition( true );
    return offset;
}


void RenderTableCell::setStyle( RenderStyle *style )
{
    RenderFlow::setStyle( style );
    setSpecialObjects(true);
}

#ifdef BOX_DEBUG
#include <qpainter.h>

static void outlineBox(QPainter *p, int _tx, int _ty, int w, int h)
{
    p->setPen(QPen(QColor("yellow"), 3, Qt::DotLine));
    p->setBrush( Qt::NoBrush );
    p->drawRect(_tx, _ty, w, h );
}
#endif

void RenderTableCell::print(QPainter *p, int _x, int _y,
                                       int _w, int _h, int _tx, int _ty)
{

#ifdef TABLE_PRINT
    kdDebug( 6040 ) << renderName() << "(RenderTableCell)::print() w/h = (" << width() << "/" << height() << ")" << endl;
#endif

//    if (!layouted())
//      return;

    _tx += m_x;
    _ty += m_y + _topExtra;

    // check if we need to do anything at all...
    if(!overhangingContents() && ((_ty-_topExtra > _y + _h)
        || (_ty + m_height+_topExtra+_bottomExtra < _y))) return;

    printObject(p, _x, _y, _w, _h, _tx, _ty);

#ifdef BOX_DEBUG
    ::outlineBox( p, _tx, _ty - _topExtra, width(), height() + borderTopExtra() + borderBottomExtra());
#endif
}


void RenderTableCell::printBoxDecorations(QPainter *p,int, int _y,
                                       int, int _h, int _tx, int _ty)
{
    //kdDebug( 6040 ) << renderName() << "::printDecorations()" << endl;

    int w = width();
    int h = height() + borderTopExtra() + borderBottomExtra();
    _ty -= borderTopExtra();

    int my = KMAX(_ty,_y);
    int mh;
    if (_ty<_y)
        mh= KMAX(0,h-(_y-_ty));
    else
        mh = KMIN(_h,h);

    QColor c = style()->backgroundColor();
    if ( !c.isValid() && parent() ) // take from row
        c = parent()->style()->backgroundColor();
    if ( !c.isValid() && parent() && parent()->parent() ) // take from rowgroup
        c = parent()->parent()->style()->backgroundColor();
    // ### col is missing...

    // ### get offsets right in case the bgimage is inherited.
    CachedImage *bg = style()->backgroundImage();
    if ( !bg && parent() )
        bg = parent()->style()->backgroundImage();
    if ( !bg && parent() && parent()->parent() )
        bg = parent()->parent()->style()->backgroundImage();

    if ( bg || c.isValid() )
	printBackground(p, c, bg, my, mh, _tx, _ty, w, h);

    if(style()->hasBorder())
        printBorder(p, _tx, _ty, w, h, style());
}

void RenderTableCell::repaint()
{
    if ( m_table ) m_table->repaint();
}

#ifndef NDEBUG
void RenderTableCell::dump(QTextStream *stream, QString ind) const
{
    *stream << " _row=" << _row;
    *stream << " _col=" << _col;
    *stream << " rSpan=" << rSpan;
    *stream << " cSpan=" << cSpan;
    *stream << " _id=" << _id;
    *stream << " nWrap=" << nWrap;

    RenderFlow::dump(stream,ind);
}
#endif

// -------------------------------------------------------------------------

RenderTableCol::RenderTableCol(DOM::NodeImpl* node)
    : RenderContainer(node)
{
    // init RenderObject attributes
    setInline(true);   // our object is not Inline

    _span = 1;
    updateFromElement();
    _currentCol = 0;
    _startCol = 0;
    _id = 0;
}

RenderTableCol::~RenderTableCol()
{
}

void RenderTableCol::updateFromElement()
{
  DOM::NodeImpl *node = element();
  if ( node && (node->id() == ID_COL || node->id() == ID_COLGROUP) ) {
      DOM::HTMLTableColElementImpl *tc = static_cast<DOM::HTMLTableColElementImpl *>(node);
      _span = tc->span();
  } else {
      if ( style()->display() == TABLE_COLUMN_GROUP )
	  _span = 0;
      else
	  _span = 1;
  }
}

void RenderTableCol::addChild(RenderObject *child, RenderObject *beforeChild)
{
#ifdef DEBUG_LAYOUT
    //kdDebug( 6040 ) << renderName() << "(Table)::addChild( " << child->renderName() << " )"  << ", " <<
    //                   (beforeChild ? beforeChild->renderName() : 0) << " )" << endl;
#endif

    if (child->style()->display() == TABLE_COLUMN)
    {
        // these have to come before the table definition!
        RenderContainer::addChild(child,beforeChild);
        RenderTableCol* colel = static_cast<RenderTableCol *>(child);
        colel->setStartCol(_currentCol);
//      kdDebug( 6040 ) << "_currentCol=" << _currentCol << endl;
        table->addColInfo(colel);
        _currentCol++;
    }
}

#ifndef NDEBUG
void RenderTableCol::dump(QTextStream *stream, QString ind) const
{
    *stream << " _span=" << _span;
    *stream << " _startCol=" << _startCol;
    *stream << " _id=" << _id;

    RenderContainer::dump(stream,ind);
}
#endif

// -------------------------------------------------------------------------

RenderTableCaption::RenderTableCaption(DOM::NodeImpl* node)
  : RenderFlow(node)
{
}

RenderTableCaption::~RenderTableCaption()
{
}

#undef TABLE_DEBUG
#undef DEBUG_LAYOUT
#undef BOX_DEBUG
