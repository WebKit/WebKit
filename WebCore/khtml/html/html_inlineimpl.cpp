/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
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
// -------------------------------------------------------------------------
#include "html_inlineimpl.h"
#include "html_imageimpl.h"
#include "html_documentimpl.h"
using namespace DOM;

#include <kdebug.h>

#include "htmlhashes.h"
#include "khtmlview.h"
#include "khtml_part.h"
#include "css/csshelper.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "css/cssstyleselector.h"
#include "xml/dom2_eventsimpl.h"
#include "rendering/render_br.h"
#include "rendering/render_image.h"

using namespace khtml;

HTMLAnchorElementImpl::HTMLAnchorElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
    href = 0;
    target = 0;
}

HTMLAnchorElementImpl::~HTMLAnchorElementImpl()
{
    if( href ) href->deref();
    if( target ) target->deref();
}

const DOMString HTMLAnchorElementImpl::nodeName() const
{
    return "A";
}

ushort HTMLAnchorElementImpl::id() const
{
    return ID_A;
}

bool HTMLAnchorElementImpl::prepareMouseEvent( int _x, int _y,
                                               int _tx, int _ty,
                                               MouseEvent *ev)
{
    bool inside = HTMLElementImpl::prepareMouseEvent( _x, _y, _tx, _ty, ev);

    // ### ev->noHref obsolete now ? ( Dirk )
    if ( inside && ev->url==0 && !ev->noHref
         && m_render && m_render->style() && m_render->style()->visiblity() != HIDDEN )
    {
        //kdDebug() << "HTMLAnchorElementImpl::prepareMouseEvent" << _tx << "/" << _ty <<endl;
        // set the url
        if(target && href)
            ev->url = DOMString("target://") + DOMString(target) + DOMString("/#") + DOMString(href);
        else
            ev->url = href;
    }

    return inside;
}


void HTMLAnchorElementImpl::defaultEventHandler(EventImpl *evt)
{
    // ### KHTML_CLICK_EVENT??? why that?
    // port to DOMACTIVATE
    if (evt->id() == EventImpl::KHTML_CLICK_EVENT && evt->isMouseEvent() && href) {
        MouseEventImpl* e = static_cast<MouseEventImpl*>( evt );
        QString utarget;
        QString url;

        if ( e->button() == 2 ) return;

        url = QConstString( href->s, href->l ).string();

        if ( target )
            utarget = QConstString( target->s, target->l ).string();

        if ( e->button() == 1 )
            utarget = "_blank";

        if ( e->target()->id() == ID_IMG ) {
            HTMLImageElementImpl* img = static_cast<HTMLImageElementImpl*>( e->target() );
            if ( img && img->isServerMap() )
            {
                khtml::RenderImage *r = static_cast<khtml::RenderImage *>(img->renderer());
                if(r)
                {
                    int absx, absy;
                    r->absolutePosition(absx, absy);
                    int x(e->clientX() - absx), y(e->clientY() - absy);
                    url += QString("?%1,%2").arg( x ).arg( y );
                }
            }
        }
        if ( !evt->defaultPrevented() ) {
            int state = 0;

            if ( e->ctrlKey() )
                state |= Qt::ControlButton;
            if ( e->shiftKey() )
                state |= Qt::ShiftButton;
            if ( e->altKey() )
                state |= Qt::AltButton;

            int button = 0;

            if ( e->button() == 0 )
                button = Qt::LeftButton;
            else if ( e->button() == 1 )
                button = Qt::MidButton;
            else if ( e->button() == 2 )
                button = Qt::RightButton;

            ownerDocument()->view()->part()->
                urlSelected( url, button, state, utarget );
        }
    }
}


void HTMLAnchorElementImpl::parseAttribute(AttrImpl *attr)
{
    switch(attr->attrId)
    {
    case ATTR_HREF:
    {
        DOMString s = khtml::parseURL(attr->val());
        href = s.implementation();
        if(href) href->ref();
        break;
    }
    case ATTR_TARGET:
        target = attr->val();
        target->ref();
        break;
    case ATTR_NAME:
    case ATTR_TITLE:
    case ATTR_REL:
	break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

// -------------------------------------------------------------------------

HTMLBRElementImpl::HTMLBRElementImpl(DocumentPtr *doc) : HTMLElementImpl(doc)
{
}

HTMLBRElementImpl::~HTMLBRElementImpl()
{
}

const DOMString HTMLBRElementImpl::nodeName() const
{
    return "BR";
}

ushort HTMLBRElementImpl::id() const
{
    return ID_BR;
}

void HTMLBRElementImpl::parseAttribute(AttrImpl *attr)
{
    switch(attr->attrId)
    {
    case ATTR_CLEAR:
    {
        DOMString str = attr->value();
        if( strcasecmp (str,"all")==0 || str.isEmpty() ) str = "both";
        addCSSProperty(CSS_PROP_CLEAR, str);
        break;
    }
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

void HTMLBRElementImpl::attach()
{
    //kdDebug( 6030 ) << "HTMLBRElementImpl::attach" << endl;
    setStyle(ownerDocument()->styleSelector()->styleForElement(this));
    khtml::RenderObject *r = _parent->renderer();
    if(r)
    {
        m_render = new RenderBR();
        m_render->setStyle(m_style);
        r->addChild(m_render, nextRenderer());
    }
    HTMLElementImpl::attach();
}

// -------------------------------------------------------------------------

HTMLFontElementImpl::HTMLFontElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLFontElementImpl::~HTMLFontElementImpl()
{
}

const DOMString HTMLFontElementImpl::nodeName() const
{
    return "FONT";
}

ushort HTMLFontElementImpl::id() const
{
    return ID_FONT;
}

void HTMLFontElementImpl::parseAttribute(AttrImpl *attr)
{
    switch(attr->attrId)
    {
    case ATTR_SIZE:
    {
        DOMString s = attr->value();
        if(s != 0) {
            int num = s.toInt();
            if ( *s.unicode() == '+' || *s.unicode() == '-' ) {
                num += 3;
            }
            int size = 0;
            switch (num)
            {
            case 1: size = CSS_VAL_X_SMALL; break;
            case 2: size = CSS_VAL_SMALL;   break;
            case 3: size = CSS_VAL_MEDIUM;  break;
            case 4: size = CSS_VAL_LARGE;   break;
            case 5: size = CSS_VAL_X_LARGE; break;
            case 6: size = CSS_VAL_XX_LARGE;break;
            default:
                if (num >= 6)
                    size = CSS_VAL__KONQ_XXX_LARGE;
                else if (num < 1)
                    size = CSS_VAL_XX_SMALL;
            }
            if ( size )
                addCSSProperty(CSS_PROP_FONT_SIZE, size);
        }
        break;
    }
    case ATTR_COLOR:
        addCSSProperty(CSS_PROP_COLOR, attr->value());
        // HTML4 compatibility hack
        addCSSProperty(CSS_PROP_TEXT_DECORATION_COLOR, attr->value());
        break;
    case ATTR_FACE:
        addCSSProperty(CSS_PROP_FONT_FAMILY, attr->value());
        break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}


void HTMLFontElementImpl::attach()
{
    HTMLElementImpl::attach();
#if 0
    // the font element needs special handling because it has to behave like
    // an inline or block level element depending on context.

    setStyle(document->styleSelector()->styleForElement(this));
    if(_parent && _parent->renderer())
    {
        if(_parent->style()->display() != INLINE)
            m_style->setDisplay(BLOCK);
        m_render = khtml::RenderObject::createObject(this);

        if(m_render)
        {
            _parent->renderer()->addChild(m_render, nextRenderer());
        }
    }

    HTMLElementImpl::attach();
#endif
}


// -------------------------------------------------------------------------

HTMLModElementImpl::HTMLModElementImpl(DocumentPtr *doc, ushort tagid) : HTMLElementImpl(doc)
{
    _id = tagid;
}

HTMLModElementImpl::~HTMLModElementImpl()
{
}

const DOMString HTMLModElementImpl::nodeName() const
{
    return getTagName(_id);
}

ushort HTMLModElementImpl::id() const
{
    return _id;
}

// -------------------------------------------------------------------------

HTMLQuoteElementImpl::HTMLQuoteElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLQuoteElementImpl::~HTMLQuoteElementImpl()
{
}

const DOMString HTMLQuoteElementImpl::nodeName() const
{
    return "Q";
}

ushort HTMLQuoteElementImpl::id() const
{
    return ID_Q;
}

