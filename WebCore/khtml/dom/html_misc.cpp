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

#include "dom/html_misc.h"
#include "html/html_miscimpl.h"

using namespace DOM;

HTMLBaseFontElement::HTMLBaseFontElement() : HTMLElement()
{
}

HTMLBaseFontElement::HTMLBaseFontElement(const HTMLBaseFontElement &other) : HTMLElement(other)
{
}

HTMLBaseFontElement::HTMLBaseFontElement(HTMLBaseFontElementImpl *impl) : HTMLElement(impl)
{
}

HTMLBaseFontElement &HTMLBaseFontElement::operator = (const Node &other)
{
    assignOther( other, HTMLNames::basefont() );
    return *this;
}

HTMLBaseFontElement &HTMLBaseFontElement::operator = (const HTMLBaseFontElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLBaseFontElement::~HTMLBaseFontElement()
{
}

DOMString HTMLBaseFontElement::color() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_COLOR);
}

void HTMLBaseFontElement::setColor( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_COLOR, value);
}

DOMString HTMLBaseFontElement::face() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_FACE);
}

void HTMLBaseFontElement::setFace( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_FACE, value);
}

DOMString HTMLBaseFontElement::size() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_SIZE);
}

void HTMLBaseFontElement::setSize( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_SIZE, value);
}

// --------------------------------------------------------------------------

HTMLCollection::HTMLCollection()
  : impl(0)
{
}

HTMLCollection::HTMLCollection(const HTMLCollection &other)
{
    impl = other.impl;
    if(impl) impl->ref();
}

HTMLCollection::HTMLCollection(NodeImpl *base, int type)
{
    impl = new HTMLCollectionImpl(base, type);
    impl->ref();
}

HTMLCollection::HTMLCollection(HTMLCollectionImpl *other)
{
    impl = other;
    if (other)
        impl->ref();
}

HTMLCollection &HTMLCollection::operator = (const HTMLCollection &other)
{
    if(impl != other.impl) {
    if(impl) impl->deref();
    impl = other.impl;
    if(impl) impl->ref();
    }
    return *this;
}

HTMLCollection::~HTMLCollection()
{
    if(impl) impl->deref();
}

unsigned long HTMLCollection::length() const
{
    if(!impl) return 0;
    return ((HTMLCollectionImpl *)impl)->length();
}

Node HTMLCollection::item( unsigned long index ) const
{
    if(!impl) return 0;
    return ((HTMLCollectionImpl *)impl)->item( index );
}

Node HTMLCollection::namedItem( const DOMString &name ) const
{
    if(!impl) return 0;
    return ((HTMLCollectionImpl *)impl)->namedItem( name );
}

Node HTMLCollection::base() const
{
    if ( !impl )
        return 0;

    return static_cast<HTMLCollectionImpl*>( impl )->base();
}

Node HTMLCollection::firstItem() const
{
    if ( !impl )
        return 0;
    return static_cast<HTMLCollectionImpl*>( impl )->firstItem();
}

Node HTMLCollection::nextItem() const
{
    if ( !impl )
        return 0;
    return static_cast<HTMLCollectionImpl*>( impl )->nextItem();
}

Node HTMLCollection::nextNamedItem( const DOMString &name ) const
{
    if ( !impl )
        return 0;
    return static_cast<HTMLCollectionImpl*>( impl )->nextNamedItem( name );
}

QValueList<Node> HTMLCollection::namedItems( const DOMString & name ) const
{
    if ( !impl )
        return QValueList<Node>();

    QValueList< SharedPtr<NodeImpl> > list = static_cast<HTMLCollectionImpl*>( impl )->namedItems( name );

    QValueList<Node> copiedList;

    QValueListConstIterator< SharedPtr<NodeImpl> > end = list.end();
    for (QValueListConstIterator< SharedPtr<NodeImpl> > it = list.begin(); it != end; ++it)
        copiedList.append((*it).get());

    return copiedList;
}


HTMLCollectionImpl *HTMLCollection::handle() const
{
    return impl;
}

bool HTMLCollection::isNull() const
{
    return (impl == 0);
}


// -----------------------------------------------------------------------------

HTMLFormCollection::HTMLFormCollection(NodeImpl *base)
    : HTMLCollection()
{
    impl = new HTMLFormCollectionImpl(base);
    impl->ref();
}
