/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2001 Dirk mueller (mueller@kde.org)
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
// --------------------------------------------------------------------------

#include "dom/dom_doc.h"
#include "dom/html_inline.h"
#include "html/html_inlineimpl.h"
#include "html/html_baseimpl.h"
#include "xml/dom_docimpl.h"
#include "misc/htmlhashes.h"

using namespace DOM;

HTMLAnchorElement::HTMLAnchorElement() : HTMLElement()
{
}

HTMLAnchorElement::HTMLAnchorElement(const HTMLAnchorElement &other) : HTMLElement(other)
{
}

HTMLAnchorElement::HTMLAnchorElement(HTMLAnchorElementImpl *impl) : HTMLElement(impl)
{
}

HTMLAnchorElement &HTMLAnchorElement::operator = (const Node &other)
{
    assignOther( other, ID_A );
    return *this;
}

HTMLAnchorElement &HTMLAnchorElement::operator = (const HTMLAnchorElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLAnchorElement::~HTMLAnchorElement()
{
}

DOMString HTMLAnchorElement::accessKey() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLAnchorElementImpl *>(impl)->accessKey();
}

void HTMLAnchorElement::setAccessKey( const DOMString &value )
{
    if(impl) static_cast<HTMLAnchorElementImpl *>(impl)->setAccessKey(value);
}

DOMString HTMLAnchorElement::charset() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLAnchorElementImpl *>(impl)->charset();
}

void HTMLAnchorElement::setCharset( const DOMString &value )
{
    if(impl) static_cast<HTMLAnchorElementImpl *>(impl)->setCharset(value);
}

DOMString HTMLAnchorElement::coords() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLAnchorElementImpl *>(impl)->coords();
}

void HTMLAnchorElement::setCoords( const DOMString &value )
{
    if(impl) static_cast<HTMLAnchorElementImpl *>(impl)->setCoords(value);
}

DOMString HTMLAnchorElement::href() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLAnchorElementImpl *>(impl)->href();
}

void HTMLAnchorElement::setHref( const DOMString &value )
{
    if(impl) static_cast<HTMLAnchorElementImpl *>(impl)->setHref(value);
}

DOMString HTMLAnchorElement::hreflang() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLAnchorElementImpl *>(impl)->hreflang();
}

void HTMLAnchorElement::setHreflang( const DOMString &value )
{
    if(impl) static_cast<HTMLAnchorElementImpl *>(impl)->setHreflang(value);
}

DOMString HTMLAnchorElement::name() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLAnchorElementImpl *>(impl)->name();
}

void HTMLAnchorElement::setName( const DOMString &value )
{
    if(impl) static_cast<HTMLAnchorElementImpl *>(impl)->setName(value);
}

DOMString HTMLAnchorElement::rel() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLAnchorElementImpl *>(impl)->rel();
}

void HTMLAnchorElement::setRel( const DOMString &value )
{
    if(impl) static_cast<HTMLAnchorElementImpl *>(impl)->setRel(value);
}

DOMString HTMLAnchorElement::rev() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLAnchorElementImpl *>(impl)->rev();
}

void HTMLAnchorElement::setRev( const DOMString &value )
{
    if(impl) static_cast<HTMLAnchorElementImpl *>(impl)->setRev(value);
}

DOMString HTMLAnchorElement::shape() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLAnchorElementImpl *>(impl)->shape();
}

void HTMLAnchorElement::setShape( const DOMString &value )
{
    if(impl) static_cast<HTMLAnchorElementImpl *>(impl)->setShape(value);
}

long HTMLAnchorElement::tabIndex() const
{
    if(!impl) return 0;
    return static_cast<HTMLAnchorElementImpl *>(impl)->tabIndex();
}

void HTMLAnchorElement::setTabIndex( long _tabIndex )
{
    if(impl) static_cast<HTMLAnchorElementImpl *>(impl)->setTabIndex(_tabIndex);
}

DOMString HTMLAnchorElement::target() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLAnchorElementImpl *>(impl)->target();
}

void HTMLAnchorElement::setTarget( const DOMString &value )
{
    if(impl) static_cast<HTMLAnchorElementImpl *>(impl)->setTarget(value);
}

DOMString HTMLAnchorElement::type() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLAnchorElementImpl *>(impl)->type();
}

void HTMLAnchorElement::setType( const DOMString &value )
{
    if(impl) static_cast<HTMLAnchorElementImpl *>(impl)->setType(value);
}

void HTMLAnchorElement::blur(  )
{
    if (impl) static_cast<HTMLAnchorElementImpl *>(impl)->blur();
}

void HTMLAnchorElement::focus(  )
{
    if (impl) static_cast<HTMLAnchorElementImpl *>(impl)->focus();
}

// --------------------------------------------------------------------------

HTMLBRElement::HTMLBRElement() : HTMLElement()
{
}

HTMLBRElement::HTMLBRElement(const HTMLBRElement &other) : HTMLElement(other)
{
}

HTMLBRElement::HTMLBRElement(HTMLBRElementImpl *impl) : HTMLElement(impl)
{
}

HTMLBRElement &HTMLBRElement::operator = (const Node &other)
{
    assignOther( other, ID_BR );
    return *this;
}

HTMLBRElement &HTMLBRElement::operator = (const HTMLBRElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLBRElement::~HTMLBRElement()
{
}

DOMString HTMLBRElement::clear() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLBRElementImpl *>(impl)->clear();
}

void HTMLBRElement::setClear( const DOMString &value )
{
    if(impl) static_cast<HTMLBRElementImpl *>(impl)->setClear(value);
}

// --------------------------------------------------------------------------

HTMLFontElement::HTMLFontElement() : HTMLElement()
{
}

HTMLFontElement::HTMLFontElement(const HTMLFontElement &other) : HTMLElement(other)
{
}

HTMLFontElement::HTMLFontElement(HTMLFontElementImpl *impl) : HTMLElement(impl)
{
}

HTMLFontElement &HTMLFontElement::operator = (const Node &other)
{
    assignOther( other, ID_FONT );
    return *this;
}

HTMLFontElement &HTMLFontElement::operator = (const HTMLFontElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLFontElement::~HTMLFontElement()
{
}

DOMString HTMLFontElement::color() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLFontElementImpl *>(impl)->color();
}

void HTMLFontElement::setColor( const DOMString &value )
{
    if(impl) static_cast<HTMLFontElementImpl *>(impl)->setColor(value);
}

DOMString HTMLFontElement::face() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLFontElementImpl *>(impl)->face();
}

void HTMLFontElement::setFace( const DOMString &value )
{
    if(impl) static_cast<HTMLFontElementImpl *>(impl)->setFace(value);
}

DOMString HTMLFontElement::size() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLFontElementImpl *>(impl)->size();
}

void HTMLFontElement::setSize( const DOMString &value )
{
    if(impl) static_cast<HTMLFontElementImpl *>(impl)->setSize(value);
}


// --------------------------------------------------------------------------

HTMLModElement::HTMLModElement() : HTMLElement()
{
}

HTMLModElement::HTMLModElement(const HTMLModElement &other) : HTMLElement(other)
{
}

HTMLModElement::HTMLModElement(HTMLElementImpl *_impl)
    : HTMLElement()
{
    if (_impl && (_impl->id() == ID_INS || _impl->id() == ID_DEL))
        impl = _impl;
    else
        impl = 0;
    if ( impl ) impl->ref();
}

HTMLModElement &HTMLModElement::operator = (const Node &other)
{
    if( other.elementId() != ID_INS &&
	other.elementId() != ID_DEL )
    {
	if ( impl ) impl->deref();
	impl = 0;
    } else {
    Node::operator = (other);
    }
    return *this;
}

HTMLModElement &HTMLModElement::operator = (const HTMLModElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLModElement::~HTMLModElement()
{
}

DOMString HTMLModElement::cite() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLModElementImpl *>(impl)->cite();
}

void HTMLModElement::setCite( const DOMString &value )
{
    if(impl) static_cast<HTMLModElementImpl *>(impl)->setCite(value);
}

DOMString HTMLModElement::dateTime() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLModElementImpl *>(impl)->dateTime();
}

void HTMLModElement::setDateTime( const DOMString &value )
{
    if(impl) static_cast<HTMLModElementImpl *>(impl)->setDateTime(value);
}

// --------------------------------------------------------------------------

HTMLQuoteElement::HTMLQuoteElement() : HTMLElement()
{
}

HTMLQuoteElement::HTMLQuoteElement(const HTMLQuoteElement &other) : HTMLElement(other)
{
}

HTMLQuoteElement::HTMLQuoteElement(HTMLGenericElementImpl *_impl)
    : HTMLElement()
{
    if (_impl && _impl->id() == ID_Q)
        impl = _impl;
    else
        impl = 0;
    if ( impl ) impl->ref();
}

HTMLQuoteElement &HTMLQuoteElement::operator = (const Node &other)
{
    assignOther( other, ID_Q );
    return *this;
}

HTMLQuoteElement &HTMLQuoteElement::operator = (const HTMLQuoteElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLQuoteElement::~HTMLQuoteElement()
{
}

DOMString HTMLQuoteElement::cite() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLQuoteElementImpl *>(impl)->cite();
}

void HTMLQuoteElement::setCite( const DOMString &value )
{
    if(impl) static_cast<HTMLQuoteElementImpl *>(impl)->setCite(value);
}
