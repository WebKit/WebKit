/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
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
// --------------------------------------------------------------------------

#include "dom/dom_doc.h"
#include "dom/html_base.h"
#include "html/html_baseimpl.h"
#include "misc/htmlhashes.h"

using namespace DOM;

HTMLBodyElement::HTMLBodyElement() : HTMLElement()
{
}

HTMLBodyElement::HTMLBodyElement(const HTMLBodyElement &other) : HTMLElement(other)
{
}

HTMLBodyElement::HTMLBodyElement(HTMLBodyElementImpl *impl) : HTMLElement(impl)
{
}

HTMLBodyElement &HTMLBodyElement::operator = (const Node &other)
{
    assignOther( other, ID_BODY );
    return *this;
}

HTMLBodyElement &HTMLBodyElement::operator = (const HTMLBodyElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLBodyElement::~HTMLBodyElement()
{
}

DOMString HTMLBodyElement::aLink() const
{
    return impl ? ((ElementImpl *)impl)->getAttribute(ATTR_ALINK) : DOMString();
}

void HTMLBodyElement::setALink( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ALINK, value);
}

DOMString HTMLBodyElement::background() const
{
    return impl ? ((ElementImpl *)impl)->getAttribute(ATTR_BACKGROUND) : DOMString();
}

void HTMLBodyElement::setBackground( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_BACKGROUND, value);
}

DOMString HTMLBodyElement::bgColor() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_BGCOLOR);
}

void HTMLBodyElement::setBgColor( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_BGCOLOR, value);
}

DOMString HTMLBodyElement::link() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_LINK);
}

void HTMLBodyElement::setLink( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_LINK, value);
}

DOMString HTMLBodyElement::text() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_TEXT);
}

void HTMLBodyElement::setText( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_TEXT, value);
}

DOMString HTMLBodyElement::vLink() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_VLINK);
}

void HTMLBodyElement::setVLink( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_VLINK, value);
}

// --------------------------------------------------------------------------

HTMLFrameElement::HTMLFrameElement() : HTMLElement()
{
}

HTMLFrameElement::HTMLFrameElement(const HTMLFrameElement &other) : HTMLElement(other)
{
}

HTMLFrameElement::HTMLFrameElement(HTMLFrameElementImpl *impl) : HTMLElement(impl)
{
}

HTMLFrameElement &HTMLFrameElement::operator = (const Node &other)
{
    assignOther( other, ID_FRAME );
    return *this;
}

HTMLFrameElement &HTMLFrameElement::operator = (const HTMLFrameElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLFrameElement::~HTMLFrameElement()
{
}

DOMString HTMLFrameElement::frameBorder() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_FRAMEBORDER);
}

void HTMLFrameElement::setFrameBorder( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_FRAMEBORDER, value);
}

DOMString HTMLFrameElement::longDesc() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_LONGDESC);
}

void HTMLFrameElement::setLongDesc( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_LONGDESC, value);
}

DOMString HTMLFrameElement::marginHeight() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_MARGINHEIGHT);
}

void HTMLFrameElement::setMarginHeight( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_MARGINHEIGHT, value);
}

DOMString HTMLFrameElement::marginWidth() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_MARGINWIDTH);
}

void HTMLFrameElement::setMarginWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_MARGINWIDTH, value);
}

DOMString HTMLFrameElement::name() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_NAME);
}

void HTMLFrameElement::setName( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_NAME, value);
}

bool HTMLFrameElement::noResize() const
{
    if(!impl) return false;
    return !((ElementImpl *)impl)->getAttribute(ATTR_NORESIZE).isNull();
}

void HTMLFrameElement::setNoResize( bool _noResize )
{
    if(impl)
    {
	DOMString str;
	if( _noResize )
	    str = "";
	((ElementImpl *)impl)->setAttribute(ATTR_NORESIZE, str);
    }
}

DOMString HTMLFrameElement::scrolling() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_SCROLLING);
}

void HTMLFrameElement::setScrolling( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_SCROLLING, value);
}

DOMString HTMLFrameElement::src() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_SRC);
}

void HTMLFrameElement::setSrc( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_SRC, value);
}

Document HTMLFrameElement::contentDocument() const
{
    if (impl) return static_cast<HTMLFrameElementImpl*>(impl)->contentDocument();
    return Document();
}

// --------------------------------------------------------------------------

HTMLIFrameElement::HTMLIFrameElement() : HTMLElement()
{
}

HTMLIFrameElement::HTMLIFrameElement(const HTMLIFrameElement &other) : HTMLElement(other)
{
}

HTMLIFrameElement::HTMLIFrameElement(HTMLIFrameElementImpl *impl) : HTMLElement(impl)
{
}

HTMLIFrameElement &HTMLIFrameElement::operator = (const Node &other)
{
    assignOther( other, ID_IFRAME );
    return *this;
}

HTMLIFrameElement &HTMLIFrameElement::operator = (const HTMLIFrameElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLIFrameElement::~HTMLIFrameElement()
{
}

DOMString HTMLIFrameElement::align() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ALIGN);
}

void HTMLIFrameElement::setAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ALIGN, value);
}

DOMString HTMLIFrameElement::frameBorder() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_FRAMEBORDER);
}

void HTMLIFrameElement::setFrameBorder( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_FRAMEBORDER, value);
}

DOMString HTMLIFrameElement::height() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_HEIGHT);
}

void HTMLIFrameElement::setHeight( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_HEIGHT, value);
}

DOMString HTMLIFrameElement::longDesc() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_LONGDESC);
}

void HTMLIFrameElement::setLongDesc( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_LONGDESC, value);
}

DOMString HTMLIFrameElement::marginHeight() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_MARGINHEIGHT);
}

void HTMLIFrameElement::setMarginHeight( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_MARGINHEIGHT, value);
}

DOMString HTMLIFrameElement::marginWidth() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_MARGINWIDTH);
}

void HTMLIFrameElement::setMarginWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_MARGINWIDTH, value);
}

DOMString HTMLIFrameElement::name() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_NAME);
}

void HTMLIFrameElement::setName( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_NAME, value);
}

DOMString HTMLIFrameElement::scrolling() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_SCROLLING);
}

void HTMLIFrameElement::setScrolling( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_SCROLLING, value);
}

DOMString HTMLIFrameElement::src() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_SRC);
}

void HTMLIFrameElement::setSrc( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_SRC, value);
}

DOMString HTMLIFrameElement::width() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_WIDTH);
}

void HTMLIFrameElement::setWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_WIDTH, value);
}

Document HTMLIFrameElement::contentDocument() const
{
    if (impl) return static_cast<HTMLIFrameElementImpl*>(impl)->contentDocument();
    return Document();
}

// --------------------------------------------------------------------------

HTMLFrameSetElement::HTMLFrameSetElement() : HTMLElement()
{
}

HTMLFrameSetElement::HTMLFrameSetElement(const HTMLFrameSetElement &other) : HTMLElement(other)
{
}

HTMLFrameSetElement::HTMLFrameSetElement(HTMLFrameSetElementImpl *impl) : HTMLElement(impl)
{
}

HTMLFrameSetElement &HTMLFrameSetElement::operator = (const Node &other)
{
    assignOther( other, ID_FRAMESET );
    return *this;
}

HTMLFrameSetElement &HTMLFrameSetElement::operator = (const HTMLFrameSetElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLFrameSetElement::~HTMLFrameSetElement()
{
}

DOMString HTMLFrameSetElement::cols() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_COLS);
}

void HTMLFrameSetElement::setCols( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_COLS, value);
}

DOMString HTMLFrameSetElement::rows() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ROWS);
}

void HTMLFrameSetElement::setRows( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ROWS, value);
}

// --------------------------------------------------------------------------

HTMLHeadElement::HTMLHeadElement() : HTMLElement()
{
}

HTMLHeadElement::HTMLHeadElement(const HTMLHeadElement &other) : HTMLElement(other)
{
}

HTMLHeadElement::HTMLHeadElement(HTMLHeadElementImpl *impl) : HTMLElement(impl)
{
}

HTMLHeadElement &HTMLHeadElement::operator = (const Node &other)
{
    assignOther( other, ID_HEAD );
    return *this;
}

HTMLHeadElement &HTMLHeadElement::operator = (const HTMLHeadElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLHeadElement::~HTMLHeadElement()
{
}

DOMString HTMLHeadElement::profile() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_PROFILE);
}

void HTMLHeadElement::setProfile( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_PROFILE, value);
}

// --------------------------------------------------------------------------

HTMLHtmlElement::HTMLHtmlElement() : HTMLElement()
{
}

HTMLHtmlElement::HTMLHtmlElement(const HTMLHtmlElement &other) : HTMLElement(other)
{
}

HTMLHtmlElement::HTMLHtmlElement(HTMLHtmlElementImpl *impl) : HTMLElement(impl)
{
}

HTMLHtmlElement &HTMLHtmlElement::operator = (const Node &other)
{
    assignOther( other, ID_HTML );
    return *this;
}

HTMLHtmlElement &HTMLHtmlElement::operator = (const HTMLHtmlElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLHtmlElement::~HTMLHtmlElement()
{
}

DOMString HTMLHtmlElement::version() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_VERSION);
}

void HTMLHtmlElement::setVersion( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_VERSION, value);
}

