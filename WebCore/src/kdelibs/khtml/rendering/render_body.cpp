/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
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
#include "render_body.h"
#include "render_root.h"
#include "html/html_baseimpl.h"
#include "xml/dom_docimpl.h"
#include "khtmlview.h"

#include <qpainter.h>
#include <qscrollview.h>

#include <kdebug.h>

using namespace khtml;
using namespace DOM;

RenderBody::RenderBody(HTMLBodyElementImpl* _element)
    : RenderFlow()
{
    scrollbarsStyled = false;
    m_element = _element;
}

RenderBody::~RenderBody()
{
}

void RenderBody::setStyle(RenderStyle* style)
{
    RenderFlow::setStyle(style);
    m_element->ownerDocument()->setTextColor( DOMString( style->color().name() ) );
    scrollbarsStyled = false;
}

void RenderBody::printBoxDecorations(QPainter *p,int, int _y,
				       int, int _h, int _tx, int _ty)
{
    //kdDebug( 6040 ) << renderName() << "::printDecorations()" << endl;
    QColor c;
    if( parent()->style()->backgroundColor().isValid() )
	c =  style()->backgroundColor();
    CachedImage *bg = 0;
    if( parent()->style()->backgroundImage() )
	bg = style()->backgroundImage();

    int w = width();
    int h = height() + borderTopExtra() + borderBottomExtra();
    _ty -= borderTopExtra();

    int my = QMAX(_ty,_y);
    int mh;
    if (_ty<_y)
    	mh=QMAX(0,h-(_y-_ty));
    else
    	mh = QMIN(_h,h);

    printBackground(p, c, bg, my, mh, _tx, _ty, w, h);

    if(style()->hasBorder())
	printBorder( p, _tx, _ty, w, h, style() );

}

void RenderBody::repaint()
{
    RenderObject *cb = containingBlock();
    if(cb != this)
	cb->repaint();
}

void RenderBody::layout()
{
    RenderFlow::layout();

    if (!scrollbarsStyled)
    {
        if (root()->view())
        {
            root()->view()->horizontalScrollBar()->setPalette(style()->palette());
            root()->view()->verticalScrollBar()->setPalette(style()->palette());
        }
        scrollbarsStyled=true;
    }
}
