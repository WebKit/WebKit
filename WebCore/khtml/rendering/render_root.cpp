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
 */
#include "rendering/render_root.h"


#include "khtmlview.h"
#include <kdebug.h>

using namespace khtml;

//#define BOX_DEBUG
//#define SPEED_DEBUG

RenderRoot::RenderRoot(DOM::NodeImpl* node, KHTMLView *view)
    : RenderFlow(node)
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
    m_printImages = true;

    m_selectionStart = 0;
    m_selectionEnd = 0;
    m_selectionStartPos = -1;
    m_selectionEndPos = -1;
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
    KHTMLAssert( !minMaxKnown() );

    RenderFlow::calcMinMaxWidth();

    m_maxWidth = m_minWidth;

    setMinMaxKnown();
}

//#define SPEED_DEBUG

void RenderRoot::layout()
{
    //kdDebug(6040) << "RenderRoot::layout()" << endl;
    if (m_printingMode)
       m_minWidth = m_width;

    if(firstChild()) {
        firstChild()->setLayouted(false);
    }

#ifdef SPEED_DEBUG
    QTime qt;
    qt.start();
#endif
    if ( recalcMinMax() )
	recalcMinMaxWidths();
#ifdef SPEED_DEBUG
    kdDebug() << "RenderRoot::calcMinMax time used=" << qt.elapsed() << endl;
    // this fixes frameset resizing
    qt.start();
#endif

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

    // ### we could maybe do the call below better and only pass true if the docsize changed.
    layoutSpecialObjects( true );
#ifdef SPEED_DEBUG
    kdDebug() << "RenderRoot::end time used=" << qt.elapsed() << endl;
#endif

    setLayouted();
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
        printSpecialObjects(p, _x, _y, _w, _h, _tx, _ty);

#ifdef BOX_DEBUG
    outlineBox(p, _tx, _ty);
#endif

}


void RenderRoot::repaintRectangle(int x, int y, int w, int h, bool f)
{
    if (m_printingMode) return;
//    kdDebug( 6040 ) << "updating views contents (" << x << "/" << y << ") (" << w << "/" << h << ")" << endl;

    if ( f && m_view ) {
        x += m_view->contentsX();
        y += m_view->contentsY();
    }

    QRect vr = viewRect();
    QRect ur(x, y, w, h);

    if (ur.intersects(vr))
        if (m_view) m_view->scheduleRepaint(x, y, w, h);
}

void RenderRoot::repaint()
{
    if (m_view && !m_printingMode)
        m_view->scheduleRepaint(m_view->contentsX(), m_view->contentsY(),
                                m_view->visibleWidth(), m_view->visibleHeight());
}

void RenderRoot::close()
{
    setLayouted( false );
    if (m_view) {
        m_view->layout();
    }
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

    // set selection start
    if (m_selectionStart)
        m_selectionStart->setIsSelectionBorder(false);
    m_selectionStart = s;
    if (m_selectionStart)
        m_selectionStart->setIsSelectionBorder(true);
    m_selectionStartPos = sp;

    // set selection end
    if (m_selectionEnd)
        m_selectionEnd->setIsSelectionBorder(false);
    m_selectionEnd = e;
    if (m_selectionEnd)
        m_selectionEnd->setIsSelectionBorder(true);
    m_selectionEndPos = ep;

    // update selection status of all objects between m_selectionStart and m_selectionEnd
    RenderObject* o = s;
    while (o && o!=e)
    {
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
    repaint();
}


void RenderRoot::clearSelection()
{
    // update selection status of all objects between m_selectionStart and m_selectionEnd
    RenderObject* o = m_selectionStart;
    while (o && o!=m_selectionEnd)
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
    if (m_selectionEnd)
    {
        m_selectionEnd->setSelectionState(SelectionNone);
        m_selectionEnd->repaint();
    }

    // set selection start & end to 0
    if (m_selectionStart)
        m_selectionStart->setIsSelectionBorder(false);
    m_selectionStart = 0;
    m_selectionStartPos = -1;

    if (m_selectionEnd)
        m_selectionEnd->setIsSelectionBorder(false);
    m_selectionEnd = 0;
    m_selectionEndPos = -1;
}

void RenderRoot::selectionStartEnd(int& spos, int& epos)
{
    spos = m_selectionStartPos;
    epos = m_selectionEndPos;
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
        int rightmostPos = fc->rightmostPosition();
        if( rightmostPos > dw )
            dw = rightmostPos;
        if( dw > w )
            w = dw;
    }
    return w;
}
