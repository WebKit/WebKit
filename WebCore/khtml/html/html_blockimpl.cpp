/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
// -------------------------------------------------------------------------
//#define DEBUG
#include "html_blockimpl.h"
#include "html_documentimpl.h"
#include "css/cssstyleselector.h"

#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "misc/htmlhashes.h"

#include <kdebug.h>

using namespace khtml;
using namespace DOM;

HTMLBlockquoteElementImpl::HTMLBlockquoteElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLBlockquoteElementImpl::~HTMLBlockquoteElementImpl()
{
}

NodeImpl::Id HTMLBlockquoteElementImpl::id() const
{
    return ID_BLOCKQUOTE;
}

// -------------------------------------------------------------------------

HTMLDivElementImpl::HTMLDivElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLDivElementImpl::~HTMLDivElementImpl()
{
}

NodeImpl::Id HTMLDivElementImpl::id() const
{
    return ID_DIV;
}

void HTMLDivElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_ALIGN:
    {
        DOMString v = attr->value();
	if ( strcasecmp( attr->value(), "middle" ) == 0 || strcasecmp( attr->value(), "center" ) == 0 )
           addCSSProperty(CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_CENTER);
        else if (strcasecmp(attr->value(), "left") == 0)
            addCSSProperty(CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_LEFT);
        else if (strcasecmp(attr->value(), "right") == 0)
            addCSSProperty(CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_RIGHT);
        else
            addCSSProperty(CSS_PROP_TEXT_ALIGN, v);
        break;
    }
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

// -------------------------------------------------------------------------

HTMLHRElementImpl::HTMLHRElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLHRElementImpl::~HTMLHRElementImpl()
{
}

NodeImpl::Id HTMLHRElementImpl::id() const
{
    return ID_HR;
}

void HTMLHRElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch( attr->id() )
    {
    case ATTR_ALIGN: {
        if (strcasecmp(attr->value(), "left") == 0) {
            addCSSProperty(CSS_PROP_MARGIN_LEFT, "0");
	    addCSSProperty(CSS_PROP_MARGIN_RIGHT, CSS_VAL_AUTO);
	}
        else if (strcasecmp(attr->value(), "right") == 0) {
	    addCSSProperty(CSS_PROP_MARGIN_LEFT, CSS_VAL_AUTO);
	    addCSSProperty(CSS_PROP_MARGIN_RIGHT, "0");
	}
	else {
      	    addCSSProperty(CSS_PROP_MARGIN_LEFT, CSS_VAL_AUTO);
            addCSSProperty(CSS_PROP_MARGIN_RIGHT, CSS_VAL_AUTO);
	}
        break;
    }
    case ATTR_WIDTH:
    {
        if(!attr->val()) break;
        // cheap hack to cause linebreaks
        // khtmltests/html/strange_hr.html
        bool ok;
        int v = attr->val()->toInt(&ok);
        if(ok && !v)
            addCSSLength(CSS_PROP_WIDTH, "1");
        else
            addCSSLength(CSS_PROP_WIDTH, attr->value());
    }
    break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

// ### make sure we undo what we did during detach
void HTMLHRElementImpl::attach()
{
    if (attributes(true /* readonly */)) {
        // there are some attributes, lets check
        DOMString color = getAttribute(ATTR_COLOR);
        DOMStringImpl* si = getAttribute(ATTR_SIZE).implementation();
        int _s =  si ? si->toInt() : -1;
        DOMString n("1");
        if (!color.isNull()) {
            addCSSProperty(CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID);
            addCSSProperty(CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID);
            addCSSProperty(CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID);
            addCSSProperty(CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID);
            addCSSProperty(CSS_PROP_BORDER_TOP_WIDTH, DOMString("0"));
            addCSSLength(CSS_PROP_BORDER_BOTTOM_WIDTH, DOMString(si));
            addHTMLColor(CSS_PROP_BORDER_COLOR, color);
        }
        else {
            if (_s > 1 && getAttribute(ATTR_NOSHADE).isNull()) {
                addCSSProperty(CSS_PROP_BORDER_BOTTOM_WIDTH, n);
                addCSSProperty(CSS_PROP_BORDER_TOP_WIDTH, n);
                addCSSProperty(CSS_PROP_BORDER_LEFT_WIDTH, n);
                addCSSProperty(CSS_PROP_BORDER_RIGHT_WIDTH, n);
                addCSSLength(CSS_PROP_HEIGHT, DOMString(QString::number(_s-2)));
            }
            else if (_s >= 0) {
                addCSSProperty(CSS_PROP_BORDER_TOP_WIDTH, DOMString(QString::number(_s)));
                addCSSProperty(CSS_PROP_BORDER_BOTTOM_WIDTH, DOMString("0"));
            }
        }
        if (_s == 0)
            addCSSProperty(CSS_PROP_MARGIN_BOTTOM, n);
    }

    HTMLElementImpl::attach();
}

// -------------------------------------------------------------------------

HTMLHeadingElementImpl::HTMLHeadingElementImpl(DocumentPtr *doc, ushort _tagid)
    : HTMLGenericElementImpl(doc, _tagid)
{
}

// -------------------------------------------------------------------------

HTMLParagraphElementImpl::HTMLParagraphElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

NodeImpl::Id HTMLParagraphElementImpl::id() const
{
    return ID_P;
}

void HTMLParagraphElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
        case ATTR_ALIGN:
        {
            DOMString v = attr->value();
            if ( strcasecmp( attr->value(), "middle" ) == 0 || strcasecmp( attr->value(), "center" ) == 0 )
                addCSSProperty(CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_CENTER);
            else if (strcasecmp(attr->value(), "left") == 0)
                addCSSProperty(CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_LEFT);
            else if (strcasecmp(attr->value(), "right") == 0)
                addCSSProperty(CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_RIGHT);
            else
                addCSSProperty(CSS_PROP_TEXT_ALIGN, v);
            break;
        }
        default:
            HTMLElementImpl::parseAttribute(attr);
    }
}

// -------------------------------------------------------------------------

HTMLPreElementImpl::HTMLPreElementImpl(DocumentPtr *doc, unsigned short _tagid)
    : HTMLGenericElementImpl(doc, _tagid)
{
}

long HTMLPreElementImpl::width() const
{
    // ###
    return 0;
}

void HTMLPreElementImpl::setWidth( long /*w*/ )
{
    // ###
}

// -------------------------------------------------------------------------

 // WinIE uses 60ms as the minimum delay by default.
const int defaultMinimumDelay = 60;

HTMLMarqueeElementImpl::HTMLMarqueeElementImpl(DocumentPtr *doc)
: HTMLElementImpl(doc),
  m_minimumDelay(defaultMinimumDelay)
{
}

NodeImpl::Id HTMLMarqueeElementImpl::id() const
{
    return ID_MARQUEE;
}

void HTMLMarqueeElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch(attr->id())
    {
        case ATTR_WIDTH:
            if (!attr->value().isEmpty())
                addCSSLength(CSS_PROP_WIDTH, attr->value());
            else
                removeCSSProperty(CSS_PROP_WIDTH);
            break;
        case ATTR_HEIGHT:
            if (!attr->value().isEmpty())
                addCSSLength(CSS_PROP_HEIGHT, attr->value());
            else
                removeCSSProperty(CSS_PROP_HEIGHT);
            break;
        case ATTR_BGCOLOR:
            if (!attr->value().isEmpty())
                addHTMLColor(CSS_PROP_BACKGROUND_COLOR, attr->value());
            else
                removeCSSProperty(CSS_PROP_BACKGROUND_COLOR);
            break;
        case ATTR_VSPACE:
            if (!attr->value().isEmpty()) {
                addCSSLength(CSS_PROP_MARGIN_TOP, attr->value());
                addCSSLength(CSS_PROP_MARGIN_BOTTOM, attr->value());
            }
            else {
                removeCSSProperty(CSS_PROP_MARGIN_TOP);
                removeCSSProperty(CSS_PROP_MARGIN_BOTTOM);
            }
            break;
        case ATTR_HSPACE:
            if (!attr->value().isEmpty()) {
                addCSSLength(CSS_PROP_MARGIN_LEFT, attr->value());
                addCSSLength(CSS_PROP_MARGIN_RIGHT, attr->value());
            }
            else {
                removeCSSProperty(CSS_PROP_MARGIN_LEFT);
                removeCSSProperty(CSS_PROP_MARGIN_RIGHT);
            }
            break;
        case ATTR_SCROLLAMOUNT:
            if (!attr->value().isEmpty())
                addCSSLength(CSS_PROP__KHTML_MARQUEE_INCREMENT, attr->value());
            else
                removeCSSProperty(CSS_PROP__KHTML_MARQUEE_INCREMENT);
            break;
        case ATTR_SCROLLDELAY:
            if (!attr->value().isEmpty())
                addCSSLength(CSS_PROP__KHTML_MARQUEE_SPEED, attr->value());
            else
                removeCSSProperty(CSS_PROP__KHTML_MARQUEE_SPEED);
            break;
        case ATTR_LOOP:
            if (!attr->value().isEmpty()) {
                if (attr->value() == "-1" || strcasecmp(attr->value(), "infinite") == 0)
                    addCSSProperty(CSS_PROP__KHTML_MARQUEE_REPETITION, CSS_VAL_INFINITE);
                else
                    addCSSLength(CSS_PROP__KHTML_MARQUEE_REPETITION, attr->value());
            }
            else
                removeCSSProperty(CSS_PROP__KHTML_MARQUEE_REPETITION);
            break;
        case ATTR_BEHAVIOR:
            if (!attr->value().isEmpty())
                addCSSProperty(CSS_PROP__KHTML_MARQUEE_STYLE, attr->value());
            else
                removeCSSProperty(CSS_PROP__KHTML_MARQUEE_STYLE);
            break;
        case ATTR_DIRECTION:
            if (!attr->value().isEmpty())
                addCSSProperty(CSS_PROP__KHTML_MARQUEE_DIRECTION, attr->value());
            else
                removeCSSProperty(CSS_PROP__KHTML_MARQUEE_DIRECTION);
            break;
        case ATTR_TRUESPEED:
            m_minimumDelay = attr->val() ? 0 : defaultMinimumDelay;
            break;
        default:
            HTMLElementImpl::parseAttribute(attr);
    }
}

// ------------------------------------------------------------------------

HTMLLayerElementImpl::HTMLLayerElementImpl(DocumentPtr *doc)
    : HTMLDivElementImpl( doc )
{
//    addCSSProperty(CSS_PROP_POSITION, CSS_VAL_ABSOLUTE);
//    fixed = false;
}

HTMLLayerElementImpl::~HTMLLayerElementImpl()
{
}

NodeImpl::Id HTMLLayerElementImpl::id() const
{
    return ID_LAYER;
}


void HTMLLayerElementImpl::parseAttribute(AttributeImpl *attr)
{
    HTMLElementImpl::parseAttribute(attr);

    // layers are evil
/*    int cssprop;
    bool page = false;
    switch(attr->id()) {
        case ATTR_PAGEX:
            page = true;
        case ATTR_LEFT:
            cssprop = CSS_PROP_LEFT;
            break;
        case ATTR_PAGEY:
            page = true;
        case ATTR_TOP:
            cssprop = CSS_PROP_TOP;
            break;
        case ATTR_WIDTH:
            cssprop = CSS_PROP_WIDTH;
            break;
        case ATTR_HEIGHT:
            cssprop = CSS_PROP_HEIGHT;
            break;
        case ATTR_Z_INDEX:
            cssprop = CSS_PROP_Z_INDEX;
            break;
        case ATTR_VISIBILITY:
            cssprop = CSS_PROP_VISIBILITY;
            break;
        default:
            HTMLDivElementImpl::parseAttribute(attr);
            return;
    }
    addCSSProperty(cssprop, attr->value());
    if ( !fixed && page ) {
        addCSSProperty(CSS_PROP_POSITION, "fixed");
        fixed = true;
    }*/
}

