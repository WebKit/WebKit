/**
 * This file is part of the HTML widget for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
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
#include "render_root.h"

#include "khtmlview.h"
#include <kdebug.h>
using namespace khtml;

RenderRoot::RenderRoot(KHTMLView *view)
    : RenderFlow()
{
    // init RenderObject attributes
    setInline(false);

    m_view = view;
    // try to contrain the width to the views width

    m_minWidth = 0;
    m_height = 0;

    m_width = m_minWidth;
    m_maxWidth = m_minWidth;

    m_rootWidth=0;
    m_rootHeight=0;

    setPositioned(true); // to 0,0 :)

    m_printingMode = false;

    selectionStart = 0;
    selectionEnd = 0;
    selectionStartPos = -1;
    selectionEndPos = -1;
    oldLayoutTime = 0;
    timeout = 800;
    setParsing();
}

RenderRoot::~RenderRoot()
{
}


void RenderRoot::calcWidth()
{
    // the width gets set by KHTMLView::print when printing to a printer.
    if(m_printingMode || !m_view)
    {
        m_width = m_rootWidth;
        return;
    }

    m_width = m_view ?
                m_view->frameWidth() + paddingLeft() + paddingRight() + borderLeft() + borderRight()
                : m_minWidth;

    if (style()->marginLeft().type==Fixed)
        m_marginLeft = style()->marginLeft().value;
    else
        m_marginLeft = 0;

    if (style()->marginRight().type==Fixed)
        m_marginRight = style()->marginRight().value;
    else
        m_marginRight = 0;
}

void RenderRoot::calcMinMaxWidth()
{
    RenderFlow::calcMinMaxWidth();

    m_maxWidth = m_minWidth;
}

//#define SPEED_DEBUG

void RenderRoot::layout()
{
    //kdDebug(6040) << "RenderRoot::layout()" << endl;
    if (m_printingMode)
       m_minWidth = m_width;

#ifdef SPEED_DEBUG
    QTime qt;
    qt.start();
#endif
    calcMinMaxWidth();
#ifdef SPEED_DEBUG
    kdDebug() << "RenderRoot::calcMinMax time used=" << qt.elapsed() << endl;
    // this fixes frameset resizing
    qt.start();
#endif

    if(firstChild()) {
        firstChild()->setLayouted(false);
    }
    RenderFlow::layout();
#ifdef SPEED_DEBUG
    kdDebug() << "RenderRoot::layout time used=" << qt.elapsed() << endl;
    qt.start();
#endif
    // have to do that before layoutSpecialObjects() to get fixed positioned objects at the right place
    if (m_view) m_view->resizeContents(docWidth(), docHeight());

    if (!m_printingMode && m_view)
    {
       m_height = m_view->visibleHeight();
       m_width = m_view->visibleWidth();
    }
    else if (!m_view)
    {
        m_height = m_rootHeight;
        m_width = m_rootWidth;
    }

    layoutSpecialObjects();
#ifdef SPEED_DEBUG
    kdDebug() << "RenderRoot::end time used=" << qt.elapsed() << endl;
#endif

    //kdDebug(0) << "root: height = " << m_height << endl;
}

bool RenderRoot::absolutePosition(int &xPos, int &yPos, bool f)
{
    if ( f && m_view) {
	xPos = m_view->contentsX();
	yPos = m_view->contentsY();
    }
    else {
        xPos = yPos = 0;
    }
    return true;
}

void RenderRoot::print(QPainter *p, int _x, int _y, int _w, int _h, int _tx, int _ty)
{
    printObject(p, _x, _y, _w, _h, _tx, _ty);
}

void RenderRoot::printObject(QPainter *p, int _x, int _y,
                                       int _w, int _h, int _tx, int _ty)
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderFlow) " << this << " ::printObject() w/h = (" << width() << "/" << height() << ")" << endl;
#endif
    // add offset for relative positioning
    if(isRelPositioned())
        relativePositionOffset(_tx, _ty);

    // 1. print background, borders etc
    if(hasSpecialObjects() && !isInline())
        printBoxDecorations(p, _x, _y, _w, _h, _tx, _ty);

    // 2. print contents
    RenderObject *child = firstChild();
    while(child != 0) {
        if(!child->isFloating() && !child->isPositioned()) {
            child->print(p, _x, _y, _w, _h, _tx, _ty);
        }
        child = child->nextSibling();
    }

    // 3. print floats and other non-flow objects.
    // we have to do that after the contents otherwise they would get obscured by background settings.
    // it is anyway undefined if regular text is above fixed objects or the other way round.
    if (m_view)
    {
        _tx += m_view->contentsX();
        _ty += m_view->contentsY();
    }

    if(specialObjects)
    {
        SpecialObject* r;
        QListIterator<SpecialObject> it(*specialObjects);
        for ( ; (r = it.current()); ++it )
        {
            if (r->node->containingBlock()==this)
            {
                RenderObject *o = r->node;
                //kdDebug(0) << "printing positioned at " << _tx + o->xPos() << "/" << _ty + o->yPos()<< endl;
                o->print(p, _x, _y, _w, _h, _tx , _ty);
            }
        }
    }

#ifdef BOX_DEBUG
    outlineBox(p, _tx, _ty);
#endif

}


void RenderRoot::repaintRectangle(int x, int y, int w, int h, bool f)
{
//    kdDebug( 6040 ) << "updating views contents (" << x << "/" << y << ") (" << w << "/" << h << ")" << endl;

    if ( f && m_view )
    {
	    x += m_view->contentsX();
	    y += m_view->contentsY();
    }

    QRect vr = viewRect();
    QRect ur(x, y, w, h);

    if (ur.intersects(vr))
        if (m_view) m_view->updateContents(x, y, w, h);
}

void RenderRoot::repaint()
{
    if (m_view) m_view->updateContents(0, 0, docWidth(), docHeight());
}

void RenderRoot::updateSize()
{
    //kdDebug( 6040 ) << renderName() << "(RenderRoot)::updateSize()" << endl;
    setMinMaxKnown(false);
    calcMinMaxWidth();

    updateHeight();
}


void RenderRoot::updateHeight()
{

    if (!m_view) return;

    //kdDebug( 6040 ) << renderName() << "(RenderRoot)::updateHeight() timer=" << updateTimer.elapsed() << endl;
    //int oldMin = m_minWidth;
    setLayouted(false);

    if (parsing())
    {
        if (!updateTimer.isNull() && updateTimer.elapsed() < timeout)
            return;
        else
            updateTimer.start();
    }

    int oldHeight = docHeight();

    m_view->layout(true);

    //kdDebug(0) << "root layout, time used=" << updateTimer.elapsed() << endl;

    int h = docHeight();
    int w = docWidth();
    if(h != oldHeight || h < m_view->visibleHeight())
    {
        if( h < m_view->visibleHeight() )
            h = m_view->visibleHeight();
        m_view->resizeContents(w, h);
    }
    m_view->repaintContents( 0, 0, w, h, FALSE );       //sync repaint!

    if(parsing()) {
        // This gets slower over time. Yes, its intended.
        oldLayoutTime = updateTimer.elapsed();
        timeout += oldLayoutTime / 2;
        if ( 2*docHeight() > 3*m_view->visibleHeight() )
            timeout = QMAX( 2000, timeout );

        timeout = QMIN( 2000+8*oldLayoutTime, timeout );

        updateTimer.start();
    }
    else
        oldLayoutTime = 0;
}

void RenderRoot::close()
{
    RenderObject* o = this;
    while (o)
    {
        if (!o->parsing())
            break;
        o->setParsing(false);
        RenderObject* no;
        if ( !(no = o->firstChild()) )
            if ( !(no = o->nextSibling()) )
            {
                no = o->parent();
                while (no && !no->nextSibling())
                    no = no->parent();
                if (no)
                    no = no->nextSibling();
            }
        o=no;
    }
    updateSize();
    if (m_view) m_view->layout(true);
    //printTree();
}

void RenderRoot::setSelection(RenderObject *s, int sp, RenderObject *e, int ep)
{
    // Check we got valid renderobjects. www.msnbc.com and clicking around, to find the case where this happened.
    if ( !s || !e )
    {
        kdWarning(6040) << "RenderRoot::setSelection() called with start=" << s << " end=" << e << endl;
        return;
    }
    //kdDebug( 6040 ) << "RenderRoot::setSelection(" << s << "," << sp << "," << e << "," << ep << ")" << endl;

    clearSelection();

    while (s->firstChild())
        s = s->firstChild();
    while (e->lastChild())
        e = e->lastChild();

    selectionStart = s;
    selectionEnd = e;
    selectionStartPos = sp;
    selectionEndPos = ep;

    RenderObject* o = s;
    while (o && o!=e)
    {
        if (o->selectionState()!=SelectionInside)
            o->repaint();
        o->setSelectionState(SelectionInside);
//      kdDebug( 6040 ) << "setting selected " << o << ", " << o->isText() << endl;
        RenderObject* no;
        if ( !(no = o->firstChild()) )
            if ( !(no = o->nextSibling()) )
            {
                no = o->parent();
                while (no && !no->nextSibling())
                    no = no->parent();
                if (no)
                    no = no->nextSibling();
            }
        o=no;
    }
    s->setSelectionState(SelectionStart);
    e->setSelectionState(SelectionEnd);
    if(s == e) s->setSelectionState(SelectionBoth);
    e->repaint();

}


void RenderRoot::clearSelection()
{
    RenderObject* o = selectionStart;
    while (o && o!=selectionEnd)
    {
        if (o->selectionState()!=SelectionNone)
            o->repaint();
        o->setSelectionState(SelectionNone);
        RenderObject* no;
        if ( !(no = o->firstChild()) )
            if ( !(no = o->nextSibling()) )
            {
                no = o->parent();
                while (no && !no->nextSibling())
                    no = no->parent();
                if (no)
                    no = no->nextSibling();
            }
        o=no;
    }
    if (selectionEnd)
    {
        selectionEnd->setSelectionState(SelectionNone);
        selectionEnd->repaint();
    }

}

void RenderRoot::selectionStartEnd(int& spos, int& epos)
{
    spos = selectionStartPos;
    epos = selectionEndPos;
}

QRect RenderRoot::viewRect() const
{
    if (m_printingMode)
        return QRect(0,0, m_width, m_height);
    else if (m_view)
        return QRect(m_view->contentsX(),
            m_view->contentsY(),
            m_view->visibleWidth(),
            m_view->visibleHeight());
    else return QRect(0,0,m_rootWidth,m_rootHeight);
}

int RenderRoot::docHeight() const
{
    int h;
    if (m_printingMode || !m_view)
        h = m_height;
    else
        h = m_view->visibleHeight();

    RenderObject *fc = firstChild();
    if(fc) {
        int dh = fc->height() + fc->marginTop() + fc->marginBottom();
        int lowestPos = firstChild()->lowestPosition();
        if( lowestPos > dh )
            dh = lowestPos;
        if( dh > h )
            h = dh;
    }
    return h;
}

int RenderRoot::docWidth() const
{
    int w;
    if (m_printingMode || !m_view)
        w = m_width;
    else
        w = m_view->visibleWidth();

    RenderObject *fc = firstChild();
    if(fc) {
        int dw = fc->width() + fc->marginLeft() + fc->marginRight();
        int rightmostPos = firstChild()->rightmostPosition();
        if( rightmostPos > dw )
            dw = rightmostPos;
        if( dw > w )
            w = dw;
    }
    return w;
}
