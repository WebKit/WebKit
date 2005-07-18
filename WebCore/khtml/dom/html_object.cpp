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

#include "dom/dom_doc.h"
#include "dom/html_object.h"
#include "html/html_objectimpl.h"

namespace DOM {

HTMLAppletElement::HTMLAppletElement() : HTMLElement()
{
}

HTMLAppletElement::HTMLAppletElement(const HTMLAppletElement &other)
    : HTMLElement(other)
{
}

HTMLAppletElement::HTMLAppletElement(HTMLAppletElementImpl *impl)
    : HTMLElement(impl)
{
}

HTMLAppletElement &HTMLAppletElement::operator = (const Node &other)
{
    assignOther( other, HTMLTags::applet() );
    return *this;
}

HTMLAppletElement &HTMLAppletElement::operator = (const HTMLAppletElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLAppletElement::~HTMLAppletElement()
{
}

DOMString HTMLAppletElement::align() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ALIGN);
}

void HTMLAppletElement::setAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ALIGN, value);
}

DOMString HTMLAppletElement::alt() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ALT);
}

void HTMLAppletElement::setAlt( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ALT, value);
}

DOMString HTMLAppletElement::archive() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ARCHIVE);
}

void HTMLAppletElement::setArchive( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ARCHIVE, value);
}

DOMString HTMLAppletElement::code() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CODE);
}

void HTMLAppletElement::setCode( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CODE, value);
}

DOMString HTMLAppletElement::codeBase() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CODEBASE);
}

void HTMLAppletElement::setCodeBase( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CODEBASE, value);
}

DOMString HTMLAppletElement::height() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_HEIGHT);
}

void HTMLAppletElement::setHeight( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_HEIGHT, value);
}

DOMString HTMLAppletElement::hspace() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_HSPACE);
}

void HTMLAppletElement::setHspace( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_HSPACE, value);
}

DOMString HTMLAppletElement::name() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_NAME);
}

void HTMLAppletElement::setName( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_NAME, value);
}

DOMString HTMLAppletElement::object() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_OBJECT);
}

void HTMLAppletElement::setObject( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_OBJECT, value);
}

DOMString HTMLAppletElement::vspace() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_VSPACE);
}

void HTMLAppletElement::setVspace( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_VSPACE, value);
}

DOMString HTMLAppletElement::width() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_WIDTH);
}

void HTMLAppletElement::setWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_WIDTH, value);
}

// --------------------------------------------------------------------------

HTMLObjectElement::HTMLObjectElement() : HTMLElement()
{
}

HTMLObjectElement::HTMLObjectElement(const HTMLObjectElement &other) : HTMLElement(other)
{
}

HTMLObjectElement::HTMLObjectElement(HTMLObjectElementImpl *impl) : HTMLElement(impl)
{
}

HTMLObjectElement &HTMLObjectElement::operator = (const Node &other)
{
    assignOther( other, HTMLTags::object() );
    return *this;
}

HTMLObjectElement &HTMLObjectElement::operator = (const HTMLObjectElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLObjectElement::~HTMLObjectElement()
{
}

HTMLFormElement HTMLObjectElement::form() const
{
    if(!impl) return 0;
    return ((HTMLObjectElementImpl *)impl)->form();
}

DOMString HTMLObjectElement::code() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CODE);
}

void HTMLObjectElement::setCode( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CODE, value);
}

DOMString HTMLObjectElement::align() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ALIGN);
}

void HTMLObjectElement::setAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ALIGN, value);
}

DOMString HTMLObjectElement::archive() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ARCHIVE);
}

void HTMLObjectElement::setArchive( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ARCHIVE, value);
}

DOMString HTMLObjectElement::border() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_BORDER);
}

void HTMLObjectElement::setBorder( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_BORDER, value);
}

DOMString HTMLObjectElement::codeBase() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CODEBASE);
}

void HTMLObjectElement::setCodeBase( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CODEBASE, value);
}

DOMString HTMLObjectElement::codeType() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CODETYPE);
}

void HTMLObjectElement::setCodeType( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CODETYPE, value);
}

DOMString HTMLObjectElement::data() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_DATA);
}

void HTMLObjectElement::setData( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_DATA, value);
}

bool HTMLObjectElement::declare() const
{
    if(!impl) return 0;
    return !((ElementImpl *)impl)->getAttribute(ATTR_DECLARE).isNull();
}

void HTMLObjectElement::setDeclare( bool _declare )
{
   if(impl)
    {
	DOMString str;
	if( _declare )
	    str = "";
	((ElementImpl *)impl)->setAttribute(ATTR_DECLARE, str);
    }
}

DOMString HTMLObjectElement::height() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_HEIGHT);
}

void HTMLObjectElement::setHeight( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_HEIGHT, value);
}

DOMString HTMLObjectElement::hspace() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_HSPACE);
}

void HTMLObjectElement::setHspace( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_HSPACE, value);
}

DOMString HTMLObjectElement::name() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_NAME);
}

void HTMLObjectElement::setName( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_NAME, value);
}

DOMString HTMLObjectElement::standby() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_STANDBY);
}

void HTMLObjectElement::setStandby( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_STANDBY, value);
}

long HTMLObjectElement::tabIndex() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute(ATTR_TABINDEX).toInt();
}

void HTMLObjectElement::setTabIndex( long _tabIndex )
{
    if(impl) {
	DOMString value(QString::number(_tabIndex));
        ((ElementImpl *)impl)->setAttribute(ATTR_TABINDEX,value);
    }
}

DOMString HTMLObjectElement::type() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_TYPE);
}

void HTMLObjectElement::setType( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_TYPE, value);
}

DOMString HTMLObjectElement::useMap() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_USEMAP);
}

void HTMLObjectElement::setUseMap( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_USEMAP, value);
}

DOMString HTMLObjectElement::vspace() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_VSPACE);
}

void HTMLObjectElement::setVspace( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_VSPACE, value);
}

DOMString HTMLObjectElement::width() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_WIDTH);
}

void HTMLObjectElement::setWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_WIDTH, value);
}

Document HTMLObjectElement::contentDocument() const
{
    if (impl) return static_cast<HTMLObjectElementImpl*>(impl)->contentDocument();
    return Document();
}

// --------------------------------------------------------------------------

HTMLParamElement::HTMLParamElement() : HTMLElement()
{
}

HTMLParamElement::HTMLParamElement(const HTMLParamElement &other) : HTMLElement(other)
{
}

HTMLParamElement::HTMLParamElement(HTMLParamElementImpl *impl) : HTMLElement(impl)
{
}

HTMLParamElement &HTMLParamElement::operator = (const Node &other)
{
    assignOther( other, HTMLTags::param() );
    return *this;
}

HTMLParamElement &HTMLParamElement::operator = (const HTMLParamElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLParamElement::~HTMLParamElement()
{
}

DOMString HTMLParamElement::name() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_NAME);
}

void HTMLParamElement::setName( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_NAME, value);
}

DOMString HTMLParamElement::type() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_TYPE);
}

void HTMLParamElement::setType( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_TYPE, value);
}

DOMString HTMLParamElement::value() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_VALUE);
}

void HTMLParamElement::setValue( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_VALUE, value);
}

DOMString HTMLParamElement::valueType() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_VALUETYPE);
}

void HTMLParamElement::setValueType( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_VALUETYPE, value);
}

} // namespace DOM
