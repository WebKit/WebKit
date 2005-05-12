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

#include "dom_element.h"

#include "dom/dom_exception.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_elementimpl.h"

using namespace DOM;

Attr::Attr() : Node()
{
}

Attr::Attr(const Attr &other) : Node(other)
{
}

Attr::Attr( AttrImpl *_impl )
{
    impl= _impl;
    if (impl) impl->ref();
}

Attr &Attr::operator = (const Node &other)
{
    NodeImpl* ohandle = other.handle();
    if ( impl != ohandle ) {
    if (!ohandle || !ohandle->isAttributeNode()) {
	impl = 0;
	} else {
    Node::operator =(other);
	}
    }
    return *this;
}

Attr &Attr::operator = (const Attr &other)
{
    Node::operator =(other);
    return *this;
}

Attr::~Attr()
{
}

DOMString Attr::name() const
{
    if (!impl) throw DOMException(DOMException::NOT_FOUND_ERR);
    return static_cast<AttrImpl *>(impl)->name();
}


bool Attr::specified() const
{
  if (impl) return ((AttrImpl *)impl)->specified();
  return 0;
}

Element Attr::ownerElement() const
{
  if (!impl) return 0;
  return static_cast<AttrImpl*>(impl)->ownerElement();
}

DOMString Attr::value() const
{
    if (!impl) throw DOMException(DOMException::NOT_FOUND_ERR);
    return static_cast<AttrImpl *>(impl)->value();
}

void Attr::setValue( const DOMString &newValue )
{
  if (!impl)
    return;

  int exceptioncode = 0;
  ((AttrImpl *)impl)->setValue(newValue,exceptioncode);
  if (exceptioncode)
    throw DOMException(exceptioncode);
}

// ---------------------------------------------------------------------------

Element::Element() : Node()
{
}

Element::Element(const Element &other) : Node(other)
{
}

Element::Element(ElementImpl *impl) : Node(impl)
{
}

Element &Element::operator = (const Node &other)
{
    NodeImpl* ohandle = other.handle();
    if ( impl != ohandle ) {
    if (!ohandle || !ohandle->isElementNode()) {
	impl = 0;
	} else {
    Node::operator =(other);
	}
    }
    return *this;
}

Element &Element::operator = (const Element &other)
{
    Node::operator =(other);
    return *this;
}

Element::~Element()
{
}

DOMString Element::tagName() const
{
    if (!impl) throw DOMException(DOMException::NOT_FOUND_ERR);
    return static_cast<ElementImpl*>(impl)->tagName();
}

DOMString Element::getAttribute( const DOMString &name )
{
    if (!impl) throw DOMException(DOMException::NOT_FOUND_ERR);
    return static_cast<ElementImpl*>(impl)->getAttribute(name);
}

void Element::setAttribute( const DOMString &name, const DOMString &value )
{
    if (!impl) throw DOMException(DOMException::NOT_FOUND_ERR);
    int exception = 0;
    static_cast<ElementImpl*>(impl)->setAttribute(name, value, exception);
    if (exception)
        throw DOMException(exception);
}

void Element::removeAttribute( const DOMString &name )
{
    if (!impl) throw DOMException(DOMException::NOT_FOUND_ERR);
    int exception = 0;
    static_cast<ElementImpl*>(impl)->removeAttribute(name, exception);
    if (exception)
        throw DOMException(exception);
}

Attr Element::getAttributeNode( const DOMString &name )
{
    if (!impl) throw DOMException(DOMException::NOT_FOUND_ERR);
    return static_cast<ElementImpl*>(impl)->getAttributeNode(name);
}

Attr Element::setAttributeNode( const Attr &newAttr )
{
    if (!impl) throw DOMException(DOMException::NOT_FOUND_ERR);
    int exception = 0;
    Attr result = static_cast<ElementImpl*>(impl)->setAttributeNode(static_cast<AttrImpl *>(newAttr.handle()), exception).get();
    if (exception)
        throw DOMException(exception);
    return result;
}

Attr Element::removeAttributeNode( const Attr &oldAttr )
{
    if (!impl) throw DOMException(DOMException::NOT_FOUND_ERR);
    int exception = 0;
    Attr result = static_cast<ElementImpl*>(impl)->removeAttributeNode(static_cast<AttrImpl *>(oldAttr.handle()), exception).get();
    if (exception)
        throw DOMException(exception);
    return result;
}

NodeList Element::getElementsByTagName( const DOMString &name )
{
    if (!impl) return 0;
    return impl->getElementsByTagName(name).get();
}

NodeList Element::getElementsByTagNameNS( const DOMString &namespaceURI,
                                          const DOMString &localName )
{
    if (!impl) return 0;
    return impl->getElementsByTagNameNS(namespaceURI, localName).get();
}

DOMString Element::getAttributeNS( const DOMString &namespaceURI,
                                   const DOMString &localName)
{
    if (!impl) throw DOMException(DOMException::NOT_FOUND_ERR);
    return static_cast<ElementImpl*>(impl)->getAttributeNS(namespaceURI, localName);
}

void Element::setAttributeNS( const DOMString &namespaceURI,
                              const DOMString &qualifiedName,
                              const DOMString &value)
{
    if (!impl) throw DOMException(DOMException::NOT_FOUND_ERR);
    int exceptioncode = 0;
    static_cast<ElementImpl*>(impl)->setAttributeNS(namespaceURI, qualifiedName, value, exceptioncode);
    if (exceptioncode)
        throw DOMException(exceptioncode);
}

void Element::removeAttributeNS( const DOMString &namespaceURI,
                                 const DOMString &localName )
{
    if (!impl) throw DOMException(DOMException::NOT_FOUND_ERR);
    int exceptioncode = 0;
    ((ElementImpl *)impl)->removeAttributeNS(namespaceURI, localName, exceptioncode);
    if ( exceptioncode )
        throw DOMException( exceptioncode );
}

Attr Element::getAttributeNodeNS( const DOMString &namespaceURI,
                                  const DOMString &localName )
{
    if (!impl) throw DOMException(DOMException::NOT_FOUND_ERR);
    return static_cast<ElementImpl *>(impl)->getAttributeNodeNS(namespaceURI, localName);
}

Attr Element::setAttributeNodeNS( const Attr &newAttr )
{
    if (!impl) throw DOMException(DOMException::NOT_FOUND_ERR);
    int exception = 0;
    Attr result = static_cast<ElementImpl*>(impl)->setAttributeNodeNS(static_cast<AttrImpl *>(newAttr.handle()), exception).get();
    if (exception)
        throw DOMException(exception);
    return result;
}

bool Element::hasAttribute( const DOMString& name )
{
    if (!impl) return false; // ### throw ?
    return static_cast<ElementImpl*>(impl)->hasAttribute(name);
}

bool Element::hasAttributeNS( const DOMString &namespaceURI,
                              const DOMString &localName )
{
    if (!impl) return false; // ### throw ?
    return static_cast<ElementImpl*>(impl)->hasAttributeNS(namespaceURI, localName);
}

bool Element::isHTMLElement() const
{
    if(!impl) return false;
    return ((ElementImpl *)impl)->isHTMLElement();
}

CSSStyleDeclaration Element::style()
{
    if (impl) return ((ElementImpl *)impl)->style();
    return 0;
}

bool Element::khtmlValidAttrName(const DOMString &/*name*/)
{
    // ###
    return true;
}

bool Element::khtmlValidPrefix(const DOMString &/*name*/)
{
    // ###
    return true;
}

bool Element::khtmlValidQualifiedName(const DOMString &/*name*/)
{
    // ###
    return true;
}

bool Element::khtmlMalformedQualifiedName(const DOMString &/*name*/)
{
    // ###
    return false;
}

bool Element::khtmlMalformedPrefix(const DOMString &/*name*/)
{
    // ###
    return false;
}
