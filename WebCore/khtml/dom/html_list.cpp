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
 *
 */
// --------------------------------------------------------------------------

#include "dom/html_list.h"
#include "html/html_listimpl.h"

using namespace DOM;

HTMLDListElement::HTMLDListElement() : HTMLElement()
{
}

HTMLDListElement::HTMLDListElement(const HTMLDListElement &other) : HTMLElement(other)
{
}

HTMLDListElement::HTMLDListElement(HTMLDListElementImpl *impl) : HTMLElement(impl)
{
}

HTMLDListElement &HTMLDListElement::operator = (const Node &other)
{
    assignOther( other, HTMLNames::dl() );
    return *this;
}

HTMLDListElement &HTMLDListElement::operator = (const HTMLDListElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLDListElement::~HTMLDListElement()
{
}

bool HTMLDListElement::compact() const
{
    if(!impl) return 0;
    return !((ElementImpl *)impl)->getAttribute(ATTR_COMPACT).isNull();
}

void HTMLDListElement::setCompact( bool _compact )
{
   if(impl)
    {
	DOMString str;
	if( _compact )
	    str = "";
	((ElementImpl *)impl)->setAttribute(ATTR_COMPACT, str);
    }
}

// --------------------------------------------------------------------------

HTMLDirectoryElement::HTMLDirectoryElement() : HTMLElement()
{
}

HTMLDirectoryElement::HTMLDirectoryElement(const HTMLDirectoryElement &other) : HTMLElement(other)
{
}

HTMLDirectoryElement::HTMLDirectoryElement(HTMLDirectoryElementImpl *impl) : HTMLElement(impl)
{
}

HTMLDirectoryElement &HTMLDirectoryElement::operator = (const Node &other)
{
    assignOther( other, HTMLNames::dir() );
    return *this;
}

HTMLDirectoryElement &HTMLDirectoryElement::operator = (const HTMLDirectoryElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLDirectoryElement::~HTMLDirectoryElement()
{
}

bool HTMLDirectoryElement::compact() const
{
    if(!impl) return 0;
    return !((ElementImpl *)impl)->getAttribute(ATTR_COMPACT).isNull();
}

void HTMLDirectoryElement::setCompact( bool _compact )
{
   if(impl)
    {
	DOMString str;
	if( _compact )
	    str = "";
	((ElementImpl *)impl)->setAttribute(ATTR_COMPACT, str);
    }
}

// --------------------------------------------------------------------------

HTMLLIElement::HTMLLIElement() : HTMLElement()
{
}

HTMLLIElement::HTMLLIElement(const HTMLLIElement &other) : HTMLElement(other)
{
}

HTMLLIElement::HTMLLIElement(HTMLLIElementImpl *impl) : HTMLElement(impl)
{
}

HTMLLIElement &HTMLLIElement::operator = (const Node &other)
{
    assignOther( other, HTMLNames::li() );
    return *this;
}

HTMLLIElement &HTMLLIElement::operator = (const HTMLLIElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLLIElement::~HTMLLIElement()
{
}

DOMString HTMLLIElement::type() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_TYPE);
}

void HTMLLIElement::setType( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_TYPE, value);
}

long HTMLLIElement::value() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute(ATTR_VALUE).toInt();
}

void HTMLLIElement::setValue( long _value )
{
    if(impl) {
	DOMString value(QString::number(_value));
        ((ElementImpl *)impl)->setAttribute(ATTR_VALUE,value);
    }
}

// --------------------------------------------------------------------------

HTMLMenuElement::HTMLMenuElement() : HTMLElement()
{
}

HTMLMenuElement::HTMLMenuElement(const HTMLMenuElement &other) : HTMLElement(other)
{
}

HTMLMenuElement::HTMLMenuElement(HTMLMenuElementImpl *impl) : HTMLElement(impl)
{
}

HTMLMenuElement &HTMLMenuElement::operator = (const Node &other)
{
    assignOther( other, HTMLNames::menu() );
    return *this;
}

HTMLMenuElement &HTMLMenuElement::operator = (const HTMLMenuElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLMenuElement::~HTMLMenuElement()
{
}

bool HTMLMenuElement::compact() const
{
    if(!impl) return 0;
    return !((ElementImpl *)impl)->getAttribute(ATTR_COMPACT).isNull();
}

void HTMLMenuElement::setCompact( bool _compact )
{
   if(impl)
    {
	DOMString str;
	if( _compact )
	    str = "";
	((ElementImpl *)impl)->setAttribute(ATTR_COMPACT, str);
    }
}

// --------------------------------------------------------------------------

HTMLOListElement::HTMLOListElement() : HTMLElement()
{
}

HTMLOListElement::HTMLOListElement(const HTMLOListElement &other) : HTMLElement(other)
{
}

HTMLOListElement::HTMLOListElement(HTMLOListElementImpl *impl) : HTMLElement(impl)
{
}

HTMLOListElement &HTMLOListElement::operator = (const Node &other)
{
    assignOther( other, HTMLNames::ol() );
    return *this;
}

HTMLOListElement &HTMLOListElement::operator = (const HTMLOListElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLOListElement::~HTMLOListElement()
{
}

bool HTMLOListElement::compact() const
{
    if(!impl) return 0;
    return !((ElementImpl *)impl)->getAttribute(ATTR_COMPACT).isNull();
}

void HTMLOListElement::setCompact( bool _compact )
{
   if(impl)
    {
	DOMString str;
	if( _compact )
	    str = "";
	((ElementImpl *)impl)->setAttribute(ATTR_COMPACT, str);
    }
}

long HTMLOListElement::start() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute(ATTR_START).toInt();
}

void HTMLOListElement::setStart( long _start )
{

    if(impl) {
	DOMString value(QString::number(_start));
        ((ElementImpl *)impl)->setAttribute(ATTR_START,value);
    }
}

DOMString HTMLOListElement::type() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_TYPE);
}

void HTMLOListElement::setType( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_TYPE, value);
}

// --------------------------------------------------------------------------

HTMLUListElement::HTMLUListElement() : HTMLElement()
{
}

HTMLUListElement::HTMLUListElement(const HTMLUListElement &other) : HTMLElement(other)
{
}

HTMLUListElement::HTMLUListElement(HTMLUListElementImpl *impl) : HTMLElement(impl)
{
}

HTMLUListElement &HTMLUListElement::operator = (const Node &other)
{
    assignOther( other, HTMLNames::ul() );
    return *this;
}

HTMLUListElement &HTMLUListElement::operator = (const HTMLUListElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLUListElement::~HTMLUListElement()
{
}

bool HTMLUListElement::compact() const
{
    if(!impl) return 0;
    return !((ElementImpl *)impl)->getAttribute(ATTR_COMPACT).isNull();
}

void HTMLUListElement::setCompact( bool _compact )
{
   if(impl)
    {
	DOMString str;
	if( _compact )
	    str = "";
	((ElementImpl *)impl)->setAttribute(ATTR_COMPACT, str);
    }
}

DOMString HTMLUListElement::type() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_TYPE);
}

void HTMLUListElement::setType( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_TYPE, value);
}

