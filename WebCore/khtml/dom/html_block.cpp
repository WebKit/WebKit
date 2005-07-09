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


#include "dom/html_block.h"
#include "html/html_blockimpl.h"

using namespace DOM;

#include "misc/htmlhashes.h"

HTMLBlockquoteElement::HTMLBlockquoteElement() : HTMLElement()
{
}

HTMLBlockquoteElement::HTMLBlockquoteElement(const HTMLBlockquoteElement &other) : HTMLElement(other)
{
}

HTMLBlockquoteElement::HTMLBlockquoteElement(HTMLBlockquoteElementImpl *impl) : HTMLElement(impl)
{
}

HTMLBlockquoteElement &HTMLBlockquoteElement::operator = (const Node &other)
{
    assignOther( other, HTMLNames::blockquote() );
    return *this;
}

HTMLBlockquoteElement &HTMLBlockquoteElement::operator = (const HTMLBlockquoteElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLBlockquoteElement::~HTMLBlockquoteElement()
{
}

DOMString HTMLBlockquoteElement::cite() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CITE);
}

void HTMLBlockquoteElement::setCite( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CITE, value);
}

// --------------------------------------------------------------------------

HTMLDivElement::HTMLDivElement() : HTMLElement()
{
}

HTMLDivElement::HTMLDivElement(const HTMLDivElement &other) : HTMLElement(other)
{
}

HTMLDivElement::HTMLDivElement(HTMLDivElementImpl *impl) : HTMLElement(impl)
{
}

HTMLDivElement &HTMLDivElement::operator = (const Node &other)
{
    assignOther( other, HTMLNames::div() );
    return *this;
}

HTMLDivElement &HTMLDivElement::operator = (const HTMLDivElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLDivElement::~HTMLDivElement()
{
}

DOMString HTMLDivElement::align() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ALIGN);
}

void HTMLDivElement::setAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ALIGN, value);
}

// --------------------------------------------------------------------------

HTMLHRElement::HTMLHRElement() : HTMLElement()
{
}

HTMLHRElement::HTMLHRElement(const HTMLHRElement &other) : HTMLElement(other)
{
}

HTMLHRElement::HTMLHRElement(HTMLHRElementImpl *impl) : HTMLElement(impl)
{
}

HTMLHRElement &HTMLHRElement::operator = (const Node &other)
{
    assignOther( other, HTMLNames::hr() );
    return *this;
}

HTMLHRElement &HTMLHRElement::operator = (const HTMLHRElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLHRElement::~HTMLHRElement()
{
}

DOMString HTMLHRElement::align() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ALIGN);
}

void HTMLHRElement::setAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ALIGN, value);
}

bool HTMLHRElement::noShade() const
{
    if(!impl) return false;
    return !((ElementImpl *)impl)->getAttribute(ATTR_NOSHADE).isNull();
}

void HTMLHRElement::setNoShade( bool _noShade )
{
    if(impl)
    {
	DOMString str;
	if( _noShade )
	    str = "";
	((ElementImpl *)impl)->setAttribute(ATTR_NOSHADE, str);
    }
}

DOMString HTMLHRElement::size() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_SIZE);
}

void HTMLHRElement::setSize( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_SIZE, value);
}

DOMString HTMLHRElement::width() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_WIDTH);
}

void HTMLHRElement::setWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_WIDTH, value);
}

// --------------------------------------------------------------------------

HTMLHeadingElement::HTMLHeadingElement() : HTMLElement()
{
}

HTMLHeadingElement::HTMLHeadingElement(const HTMLHeadingElement &other) : HTMLElement(other)
{
}

HTMLHeadingElement::HTMLHeadingElement(HTMLHeadingElementImpl *impl) : HTMLElement(impl)
{
}

HTMLHeadingElement &HTMLHeadingElement::operator = (const Node &other)
{
    if (!other.handle()->hasTagName(HTMLNames::h1()) &&
        !other.handle()->hasTagName(HTMLNames::h2()) &&
        !other.handle()->hasTagName(HTMLNames::h3()) &&
        !other.handle()->hasTagName(HTMLNames::h4()) &&
        !other.handle()->hasTagName(HTMLNames::h5()) &&
        !other.handle()->hasTagName(HTMLNames::h6()))
    {
	if ( impl ) impl->deref();
	impl = 0;
    } else {

    Node::operator = (other);
    }
    return *this;
}

HTMLHeadingElement &HTMLHeadingElement::operator = (const HTMLHeadingElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLHeadingElement::~HTMLHeadingElement()
{
}

DOMString HTMLHeadingElement::align() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ALIGN);
}

void HTMLHeadingElement::setAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ALIGN, value);
}

// --------------------------------------------------------------------------

HTMLParagraphElement::HTMLParagraphElement() : HTMLElement()
{
}

HTMLParagraphElement::HTMLParagraphElement(const HTMLParagraphElement &other) : HTMLElement(other)
{
}

HTMLParagraphElement::HTMLParagraphElement(HTMLParagraphElementImpl *impl) : HTMLElement(impl)
{
}

HTMLParagraphElement &HTMLParagraphElement::operator = (const Node &other)
{
    assignOther( other, HTMLNames::p() );
    return *this;
}

HTMLParagraphElement &HTMLParagraphElement::operator = (const HTMLParagraphElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLParagraphElement::~HTMLParagraphElement()
{
}

DOMString HTMLParagraphElement::align() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ALIGN);
}

void HTMLParagraphElement::setAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ALIGN, value);
}

// --------------------------------------------------------------------------

HTMLPreElement::HTMLPreElement() : HTMLElement()
{
}

HTMLPreElement::HTMLPreElement(const HTMLPreElement &other) : HTMLElement(other)
{
}

HTMLPreElement::HTMLPreElement(HTMLPreElementImpl *impl) : HTMLElement(impl)
{
}

HTMLPreElement &HTMLPreElement::operator = (const Node &other)
{
    assignOther( other, HTMLNames::pre() );
    return *this;
}

HTMLPreElement &HTMLPreElement::operator = (const HTMLPreElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLPreElement::~HTMLPreElement()
{
}

long HTMLPreElement::width() const
{
    if(!impl) return 0;
    DOMString w = ((ElementImpl *)impl)->getAttribute(ATTR_WIDTH);
    return w.toInt();
}

void HTMLPreElement::setWidth( long _width )
{
    if(!impl) return;

    QString aStr;
    aStr.sprintf("%ld", _width);
    DOMString value(aStr);
    ((ElementImpl *)impl)->setAttribute(ATTR_WIDTH, value);
}

