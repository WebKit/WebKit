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
#include "dom/dom_exception.h"
#include "dom/html_misc.h"
#include "css/css_base.h"
#include "html/html_miscimpl.h" // HTMLCollectionImpl

#include "misc/htmlhashes.h"

using namespace DOM;

HTMLElement::HTMLElement() : Element()
{
}

HTMLElement::HTMLElement(const HTMLElement &other) : Element(other)
{
}

HTMLElement::HTMLElement(HTMLElementImpl *impl) : Element(impl)
{
}

HTMLElement &HTMLElement::operator = (const HTMLElement &other)
{
    Element::operator = (other);
    return *this;
}

HTMLElement &HTMLElement::operator = (const Node &other)
{
    NodeImpl* ohandle = other.handle();
    if (!ohandle || !ohandle->isHTMLElement()) {
	impl = 0;
	return *this;
    }
    Node::operator = (other);
    return *this;
}

HTMLElement::~HTMLElement()
{
}

DOMString HTMLElement::id() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_ID);
}

void HTMLElement::setId( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_ID, value);
}

DOMString HTMLElement::title() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_TITLE);
}

void HTMLElement::setTitle( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_TITLE, value);
}

DOMString HTMLElement::lang() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_LANG);
}

void HTMLElement::setLang( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_LANG, value);
}

DOMString HTMLElement::dir() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_DIR);
}

void HTMLElement::setDir( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_DIR, value);
}

DOMString HTMLElement::className() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CLASS);
}

void HTMLElement::setClassName( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CLASS, value);
}

void HTMLElement::removeCSSProperty( const DOMString &property )
{
    int id = getPropertyID(property.string().lower().ascii(), property.length());
    if(id && impl)
        static_cast<HTMLElementImpl*>(impl)->removeCSSProperty(id);
}

void HTMLElement::addCSSProperty( const DOMString &property, const DOMString &value )
{
    int id = getPropertyID(property.string().lower().ascii(), property.length());
    if(id && impl)
        static_cast<HTMLElementImpl*>(impl)->addCSSProperty(id, value);
}

DOMString HTMLElement::innerHTML() const
{
    if ( !impl ) return DOMString();
    return ((HTMLElementImpl *)impl)->innerHTML();
}

void HTMLElement::setInnerHTML( const DOMString &html )
{
    bool ok = false;
    if( impl )
	ok = ((HTMLElementImpl *)impl)->setInnerHTML( html );
    if ( !ok )
	throw DOMException(DOMException::NO_MODIFICATION_ALLOWED_ERR);
}

DOMString HTMLElement::innerText() const
{
    if ( !impl ) return DOMString();
    return ((HTMLElementImpl *)impl)->innerText();
}

void HTMLElement::setInnerText( const DOMString &text )
{
    bool ok = false;
    if( impl )
	ok = ((HTMLElementImpl *)impl)->setInnerText( text );
    if ( !ok )
	throw DOMException(DOMException::NO_MODIFICATION_ALLOWED_ERR);
}

HTMLCollection HTMLElement::children() const
{
    if(!impl) return HTMLCollection();
    return HTMLCollection(impl, HTMLCollectionImpl::NODE_CHILDREN);
}

void HTMLElement::assignOther( const Node &other, int elementId )
{
    if((int)other.elementId() != elementId) {
	if ( impl ) impl->deref();
	impl = 0;
    } else {
	Node::operator = (other);
    }   
}

bool HTMLElement::isContentEditable() const
{
    if(!impl) return false;
    return static_cast<HTMLElementImpl *>(impl)->isContentEditable();
}

DOMString HTMLElement::contentEditable() const {
    if(!impl) return "inherit";
    return static_cast<HTMLElementImpl *>(impl)->contentEditable();
}

void HTMLElement::setContentEditable(const DOMString &enabled) {
    if(!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);
    if (enabled == "inherit") {
        int exceptionCode;
        static_cast<HTMLElementImpl *>(impl)->removeAttribute(ATTR_CONTENTEDITABLE, exceptionCode);
    }
    else
        static_cast<HTMLElementImpl *>(impl)->setAttribute(ATTR_CONTENTEDITABLE, enabled.isEmpty() ? "true" : enabled);
}

