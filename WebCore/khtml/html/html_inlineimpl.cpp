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
 */
// -------------------------------------------------------------------------

#include "html/html_inlineimpl.h"
#include "html/html_imageimpl.h"
#include "html/html_documentimpl.h"

#include "misc/htmlhashes.h"
#include "khtmlview.h"
#include "khtml_part.h"
#include "css/csshelper.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "css/cssstyleselector.h"
#include "xml/dom2_eventsimpl.h"
#include "rendering/render_br.h"
#include "rendering/render_image.h"

#include <kdebug.h>

using namespace khtml;
using namespace DOM;

HTMLAnchorElementImpl::HTMLAnchorElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
    m_hasTarget = false;
}

HTMLAnchorElementImpl::~HTMLAnchorElementImpl()
{
}

NodeImpl::Id HTMLAnchorElementImpl::id() const
{
    return ID_A;
}

void HTMLAnchorElementImpl::defaultEventHandler(EventImpl *evt)
{
    // React on clicks and on keypresses.
    // Don't make this KEYUP_EVENT again, it makes khtml follow links it shouldn't,
    // when pressing Enter in the combo.
    if ( ( evt->id() == EventImpl::KHTML_CLICK_EVENT ||
         ( evt->id() == EventImpl::KHTML_KEYDOWN_EVENT && m_focused)) && m_hasAnchor) {
        MouseEventImpl *e = 0;
        if ( evt->id() == EventImpl::KHTML_CLICK_EVENT )
            e = static_cast<MouseEventImpl*>( evt );

        KeyEventImpl *k = 0;
        if (evt->id() == EventImpl::KHTML_KEYDOWN_EVENT)
            k = static_cast<KeyEventImpl *>( evt );

        QString utarget;
        QString url;

        if ( e && e->button() == 2 ) {
	    HTMLElementImpl::defaultEventHandler(evt);
	    return;
        }

        if ( k ) {
            if (k->virtKeyVal() != KeyEventImpl::DOM_VK_ENTER) {
                HTMLElementImpl::defaultEventHandler(evt);
                return;
            }
            if (k->qKeyEvent) k->qKeyEvent->accept();
        }

        url = khtml::parseURL(getAttribute(ATTR_HREF)).string();

        utarget = getAttribute(ATTR_TARGET).string();

        if ( e && e->button() == 1 )
            utarget = "_blank";

        if ( evt->target()->id() == ID_IMG ) {
            HTMLImageElementImpl* img = static_cast<HTMLImageElementImpl*>( evt->target() );
            if ( img && img->isServerMap() )
            {
                khtml::RenderImage *r = static_cast<khtml::RenderImage *>(img->renderer());
                if(r && e)
                {
                    int absx, absy;
                    r->absolutePosition(absx, absy);
                    int x(e->clientX() - absx), y(e->clientY() - absy);
                    url += QString("?%1,%2").arg( x ).arg( y );
                }
                else {
                    evt->setDefaultHandled();
		    HTMLElementImpl::defaultEventHandler(evt);
		    return;
                }
            }
        }
        if ( !evt->defaultPrevented() ) {
            int state = 0;
            int button = 0;

            if ( e ) {
                if ( e->ctrlKey() )
                    state |= Qt::ControlButton;
                if ( e->shiftKey() )
                    state |= Qt::ShiftButton;
                if ( e->altKey() )
                    state |= Qt::AltButton;
                if ( e->metaKey() )
                    state |= Qt::MetaButton;

                if ( e->button() == 0 )
                    button = Qt::LeftButton;
                else if ( e->button() == 1 )
                    button = Qt::MidButton;
                else if ( e->button() == 2 )
                    button = Qt::RightButton;
            }
	    else if ( k )
	    {
	      if ( k->checkModifier(Qt::ShiftButton) )
                state |= Qt::ShiftButton;
	      if ( k->checkModifier(Qt::AltButton) )
                state |= Qt::AltButton;
	      if ( k->checkModifier(Qt::ControlButton) )
                state |= Qt::ControlButton;
	    }

            getDocument()->view()->resetCursor();
            getDocument()->view()->part()->
                urlSelected( url, button, state, utarget );
        }
        evt->setDefaultHandled();
    }
    HTMLElementImpl::defaultEventHandler(evt);
}


void HTMLAnchorElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_HREF:
        m_hasAnchor = attr->val() != 0;
        break;
    case ATTR_TARGET:
        m_hasTarget = attr->val() != 0;
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

NodeImpl::Id HTMLBRElementImpl::id() const
{
    return ID_BR;
}

void HTMLBRElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
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
    assert(!attached());
    assert(!m_render);
    assert(parentNode());

    if (parentNode()->renderer()) {
        m_render = new RenderBR(this);
        m_render->setStyle(getDocument()->styleSelector()->styleForElement(this));
        parentNode()->renderer()->addChild(m_render, nextRenderer());
    }
    NodeImpl::attach();
}

// -------------------------------------------------------------------------

HTMLFontElementImpl::HTMLFontElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLFontElementImpl::~HTMLFontElementImpl()
{
}

NodeImpl::Id HTMLFontElementImpl::id() const
{
    return ID_FONT;
}

void HTMLFontElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_SIZE:
    {
        DOMString s = attr->value();
        if(!s.isNull()) {
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


