/**
 * This file is part of the HTML widget for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
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
#include "render_replaced.h"
#include "render_root.h"

#include <assert.h>
#include <qscrollview.h>

#include "misc/helper.h"
#include "khtmlview.h"

using namespace khtml;


RenderReplaced::RenderReplaced()
    : RenderBox()
{
    // init RenderObject attributes
    setReplaced(true);

    m_intrinsicWidth = 200;
    m_intrinsicHeight = 150;
}

void RenderReplaced::print( QPainter *p, int _x, int _y, int _w, int _h,
                            int _tx, int _ty)
{
    if ( !isVisible() )
        return;

    _tx += m_x;
    _ty += m_y;

    if((_ty > _y + _h) || (_ty + m_height < _y)) return;

    if(hasSpecialObjects()) printBoxDecorations(p, _x, _y, _w, _h, _tx, _ty);    
        
    // overflow: hidden    
    // save old clip region, set a new one
    QRegion oldClip;
    if (style()->overflow()==OHIDDEN)
    {
        if (p->hasClipping())
            oldClip = p->clipRegion();        
        calcClip(p, _tx, _ty, oldClip);   
    }    
    
    printObject(p, _x, _y, _w, _h, _tx, _ty);
    
    // overflow: hidden
    // restore clip region
    if (style()->overflow()==OHIDDEN)
    {
        if (oldClip.isNull())
            p->setClipping(false);
        else
            p->setClipRegion(oldClip);        
    }     
}

short RenderReplaced::calcReplacedWidth(bool* ieHack) const
{
    Length w = style()->width();
    short width;
    if ( ieHack )
        *ieHack = false;

    switch( w.type ) {
    case Variable:
    {
        Length h = style()->height();
        int ih = intrinsicHeight();
        if ( ih > 0 && ( h.isPercent() || h.isFixed() ) )
            width = ( ( h.isPercent() ? calcReplacedHeight() : h.value )*intrinsicWidth() ) / ih;
        else
            width = intrinsicWidth();
        break;
    }
    case Percent:
    {
        int cw = containingBlockWidth();
        if ( cw )
            width = w.minWidth( cw );
        else
            width = intrinsicWidth();

        if ( ieHack )
            *ieHack = cw;
        break;
    }
    case Fixed:
        width = w.value;
        break;
    default:
        width = intrinsicWidth();
        break;
    };

    return width;
}

int RenderReplaced::calcReplacedHeight() const
{
    Length h = style()->height();
    short height;
    switch( h.type ) {
    case Variable:
    {
        Length w = style()->width();
        int iw = intrinsicWidth();
        if( iw > 0 && ( w.isFixed() || w.isPercent() ))
            height = (( w.isPercent() ? calcReplacedWidth() : w.value ) * intrinsicHeight()) / iw;
        else
            height = intrinsicHeight();
    }
    break;
    case Percent:
    {
        RenderObject* cb = containingBlock();
        if ( cb->isBody() )
            height = h.minWidth( cb->root()->view()->visibleHeight() );
        else {
            if ( cb->isTableCell() )
                cb = cb->containingBlock();

            if ( cb->style()->height().isFixed() )
                height = h.minWidth( cb->style()->height().value );
            else
                height = intrinsicHeight();
        }
    }
    break;
    case Fixed:
        height = h.value;
        break;
    default:
        height = intrinsicHeight();
    };

    return height;
}

void RenderReplaced::calcMinMaxWidth()
{
    if(minMaxKnown()) return;

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << "RenderReplaced::calcMinMaxWidth() known=" << minMaxKnown() << endl;
#endif

    bool isPercent = false;
    int width = calcReplacedWidth(&isPercent);

    if ( isPercent ) {
        m_minWidth = 0;
        m_maxWidth = width;
    }
    else {
        m_minWidth = m_maxWidth = width;
        setMinMaxKnown();
    }
}

int RenderReplaced::lineHeight( bool ) const
{
    return height()+marginTop()+marginBottom();
}

short RenderReplaced::baselinePosition( bool ) const
{
    return height()+marginTop()+marginBottom();
}

void RenderReplaced::position(int x, int y, int, int, int, bool, bool)
{
    m_x = x + marginLeft();
    m_y = y + marginTop();
}

// -----------------------------------------------------------------------------

RenderWidget::RenderWidget(QScrollView *view)
        : RenderReplaced()
{
    m_widget = 0;
    m_view = view;

    // this is no real reference counting, its just there
    // to make sure that we're not deleted while we're recursed
    // in an eventFilter of the widget
    ref();
}

void RenderWidget::detach()
{
    remove();
    if ( m_widget ) {
        if ( m_view )
            m_view->removeChild( m_widget );

        m_widget->removeEventFilter( this );
        m_widget->setMouseTracking( false );
    }
    deref();
}

RenderWidget::~RenderWidget()
{
    assert( refCount() <= 0 );

    delete m_widget;
}

void RenderWidget::setQWidget(QWidget *widget)
{
    if (widget != m_widget)
    {
        if(m_widget) {
            disconnect( m_widget, SIGNAL( destroyed()),
                        this, SLOT( slotWidgetDestructed()));
            delete m_widget;
            m_widget = 0;
        }
	widget->setFocusPolicy(QWidget::ClickFocus);
        m_widget = widget;
        connect( m_widget, SIGNAL( destroyed()),
                 this, SLOT( slotWidgetDestructed()));
    }

    setContainsWidget(widget);
}

void RenderWidget::slotWidgetDestructed()
{
    m_widget = 0;
}

void RenderWidget::setStyle(RenderStyle *_style)
{
    RenderReplaced::setStyle(_style);
    if(m_widget)
    {
        m_widget->setFont(style()->font());
        if(!isVisible()) m_widget->hide();
    }
}

void RenderWidget::printObject(QPainter *, int, int, int, int, int _tx, int _ty)
{
    // ### this does not get called if a form element moves of the screen, so
    // the widget stays in it's old place!
    if(!(m_widget && m_view) || !isVisible()) return;

    // add offset for relative positioning
    if(isRelPositioned())
        relativePositionOffset(_tx, _ty);

    bool oldMouseTracking = m_widget->hasMouseTracking();
    m_view->addChild(m_widget, _tx+borderLeft()+paddingLeft(), _ty+borderTop()+paddingTop());
    m_widget->setMouseTracking(oldMouseTracking);

    m_widget->show();
}

void RenderWidget::placeWidget(int xPos, int yPos)
{
    // add offset for relative positioning
    if(isRelPositioned())
        relativePositionOffset(xPos, yPos);

    if(!(m_widget && m_view)) return;
    bool oldMouseTracking = m_widget->hasMouseTracking();
    m_view->addChild(m_widget,  xPos+borderLeft()+paddingLeft(), yPos+borderTop()+paddingTop());
    m_widget->setMouseTracking(oldMouseTracking);
}

void RenderWidget::focus()
{
    if (m_widget)
	m_widget->setFocus();
}

void RenderWidget::blur()
{
    if (m_widget)
	m_widget->clearFocus();
}

#include "render_replaced.moc"

