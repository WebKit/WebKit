/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
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
//#define DEBUG_LAYOUT

#include "render_hr.h"

#include <qpixmap.h>
#include <qdrawutil.h>
#include <qpalette.h>

#include <kdebug.h>

#include "rendering/render_style.h"
#include "rendering/render_flow.h"

using namespace DOM;
using namespace khtml;

// -------------------------------------------------------------------------

RenderHR::RenderHR()
    : RenderBox()
{
    // init RenderObject attributes
    setInline( false );
    setSpecialObjects( false );
    setParsing( false );

    hr_shade = false;
    m_length = 0;
}

RenderHR::~RenderHR()
{
}

void RenderHR::print(QPainter *p, int _x, int _y, int _w, int _h, int _tx, int _ty)
{
    // add offset for relative positioning
    if(isRelPositioned())
        relativePositionOffset(_tx, _ty);

    int xp = _tx;

    RenderObject *prev = previousSibling();
    while(prev && !prev->isFlow())
        prev = prev->previousSibling();
    if(prev && static_cast<RenderFlow *>(prev)->floatBottom() > prev->height() )
        xp += static_cast<RenderFlow *>(prev)->leftOffset( prev->height() );

    int yp = _ty ;



    switch(style()->textAlign()) {
    case LEFT:
        break;
    case RIGHT:
        xp += m_width - m_length;
        break;
    case JUSTIFY:
    case CENTER:
    case KONQ_CENTER:
        //kdDebug(6040) << "centered" << endl;
        xp += (m_width - m_length)/2;
        break;
    }

    kdDebug(6040) << "tx = " << xp << " ty=" << _ty << " m_width = " << m_width << " length = " << m_length << " m_height: " << m_height <<  endl;

    printBorder(p, xp, _ty, m_length, m_height, style());

#if 0
    if ( shade )
    {
        if(size < 2) size = 2, lw = 1;
        qDrawShadePanel( p, xp, yp, length, size,
                colorGrp, true, lw, 0 );
    }
    else
    {
        if(size < 1) size = 1;
        p->fillRect( xp, yp, length, size, Qt::black );
    }
#endif
}

void RenderHR::layout()
{
    if ( layouted() ) return;

    kdDebug(6040) << "RenderHR::layout!" << endl;

    calcWidth();
    calcHeight();

    kdDebug(6040) << "minWidth: " << m_minWidth <<
        " maxWidth: " << m_maxWidth << " width: " << m_width << 
        " height: " << m_height << endl;

    //  setLayouted();
}

void RenderHR::calcMinMaxWidth()
{
    if ( minMaxKnown() ) return;

    // contentWidth
    Length w = style()->width();

    int width = containingBlockWidth();

    switch(w.type)
    {
    case Fixed:
        m_minWidth = m_maxWidth = width;
        m_length   = width;
        break;
    case Percent:
        m_minWidth = 1;
        m_maxWidth = width;
        m_length   = w.minWidth( width );
        break;
    default:
        m_minWidth = m_maxWidth = 0;
        m_length   = width;
    }

//    setMinMaxKnown();
}

