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

#include "dom/html_head.h"
#include "dom/dom_doc.h"
#include "html/html_headimpl.h"
#include "misc/htmlhashes.h"

using namespace DOM;

HTMLBaseElement::HTMLBaseElement() : HTMLElement()
{
}

HTMLBaseElement::HTMLBaseElement(const HTMLBaseElement &other) : HTMLElement(other)
{
}

HTMLBaseElement::HTMLBaseElement(HTMLBaseElementImpl *impl) : HTMLElement(impl)
{
}

HTMLBaseElement &HTMLBaseElement::operator = (const Node &other)
{
    assignOther( other, ID_BASE );
    return *this;
}

HTMLBaseElement &HTMLBaseElement::operator = (const HTMLBaseElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLBaseElement::~HTMLBaseElement()
{
}

DOMString HTMLBaseElement::href() const
{
    if(!impl) return DOMString();
    DOMString s = ((ElementImpl *)impl)->getAttribute(ATTR_HREF);
    if (!s.isNull())
	s = ownerDocument().completeURL( s );
    return s;
}

void HTMLBaseElement::setHref( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_HREF, value);
}

DOMString HTMLBaseElement::target() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_TARGET);
}

void HTMLBaseElement::setTarget( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_TARGET, value);
}

// --------------------------------------------------------------------------

HTMLLinkElement::HTMLLinkElement() : HTMLElement()
{
}

HTMLLinkElement::HTMLLinkElement(const HTMLLinkElement &other) : HTMLElement(other)
{
}

HTMLLinkElement::HTMLLinkElement(HTMLLinkElementImpl *impl) : HTMLElement(impl)
{
}

HTMLLinkElement &HTMLLinkElement::operator = (const Node &other)
{
    assignOther( other, ID_LINK );
    return *this;
}

HTMLLinkElement &HTMLLinkElement::operator = (const HTMLLinkElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLLinkElement::~HTMLLinkElement()
{
}

bool HTMLLinkElement::disabled() const
{
    if(!impl) return 0;
    return !((ElementImpl *)impl)->getAttribute(ATTR_DISABLED).isNull();
}

void HTMLLinkElement::setDisabled( bool _disabled )
{
    if(impl) {
        ((ElementImpl *)impl)->setAttribute(ATTR_DISABLED, _disabled ? "" : 0);
        ((HTMLLinkElementImpl*)impl)->setDisabledState(_disabled);
    }
}

DOMString HTMLLinkElement::charset() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CHARSET);
}

void HTMLLinkElement::setCharset( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CHARSET, value);
}

DOMString HTMLLinkElement::href() const
{
    if(!impl) return DOMString();
    DOMString s = ((ElementImpl *)impl)->getAttribute(ATTR_HREF);
    if (!s.isNull())
	s = ownerDocument().completeURL( s );
    return s;
}

void HTMLLinkElement::setHref( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_HREF, value);
}

DOMString HTMLLinkElement::hreflang() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_HREFLANG);
}

void HTMLLinkElement::setHreflang( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_HREFLANG, value);
}

DOMString HTMLLinkElement::media() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_MEDIA);
}

void HTMLLinkElement::setMedia( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_MEDIA, value);
}

DOMString HTMLLinkElement::rel() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_REL);
}

void HTMLLinkElement::setRel( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_REL, value);
}

DOMString HTMLLinkElement::rev() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_REV);
}

void HTMLLinkElement::setRev( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_REV, value);
}

DOMString HTMLLinkElement::target() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_TARGET);
}

void HTMLLinkElement::setTarget( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_TARGET, value);
}

DOMString HTMLLinkElement::type() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_TYPE);
}

void HTMLLinkElement::setType( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_TYPE, value);
}

StyleSheet HTMLLinkElement::sheet() const
{
    if(!impl) return 0;
    return ((HTMLLinkElementImpl *)impl)->sheet();
}

// --------------------------------------------------------------------------

HTMLMetaElement::HTMLMetaElement() : HTMLElement()
{
}

HTMLMetaElement::HTMLMetaElement(const HTMLMetaElement &other) : HTMLElement(other)
{
}

HTMLMetaElement::HTMLMetaElement(HTMLMetaElementImpl *impl) : HTMLElement(impl)
{
}

HTMLMetaElement &HTMLMetaElement::operator = (const Node &other)
{
    assignOther( other, ID_META );
    return *this;
}

HTMLMetaElement &HTMLMetaElement::operator = (const HTMLMetaElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLMetaElement::~HTMLMetaElement()
{
}

DOMString HTMLMetaElement::content() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CONTENT);
}

void HTMLMetaElement::setContent( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CONTENT, value);
}

DOMString HTMLMetaElement::httpEquiv() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_HTTP_EQUIV);
}

void HTMLMetaElement::setHttpEquiv( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_HTTP_EQUIV, value);
}

DOMString HTMLMetaElement::name() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_NAME);
}

void HTMLMetaElement::setName( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_NAME, value);
}

DOMString HTMLMetaElement::scheme() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_SCHEME);
}

void HTMLMetaElement::setScheme( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_SCHEME, value);
}

// --------------------------------------------------------------------------

HTMLScriptElement::HTMLScriptElement() : HTMLElement()
{
}

HTMLScriptElement::HTMLScriptElement(const HTMLScriptElement &other) : HTMLElement(other)
{
}

HTMLScriptElement::HTMLScriptElement(HTMLScriptElementImpl *impl) : HTMLElement(impl)
{
}

HTMLScriptElement &HTMLScriptElement::operator = (const Node &other)
{
    assignOther( other, ID_SCRIPT );
    return *this;
}

HTMLScriptElement &HTMLScriptElement::operator = (const HTMLScriptElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLScriptElement::~HTMLScriptElement()
{
}

DOMString HTMLScriptElement::text() const
{
    if(!impl) return DOMString();
    return ((HTMLScriptElementImpl *)impl)->text();
}

void HTMLScriptElement::setText( const DOMString &value )
{
    if(impl) ((HTMLScriptElementImpl *)impl)->setText(value);
}

DOMString HTMLScriptElement::htmlFor() const
{
    // DOM Level 1 says: reserved for future use...
    return DOMString();
}

void HTMLScriptElement::setHtmlFor( const DOMString &/*value*/ )
{
    // DOM Level 1 says: reserved for future use...
}

DOMString HTMLScriptElement::event() const
{
    // DOM Level 1 says: reserved for future use...
    return DOMString();
}

void HTMLScriptElement::setEvent( const DOMString &/*value*/ )
{
    // DOM Level 1 says: reserved for future use...
}

DOMString HTMLScriptElement::charset() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_CHARSET);
}

void HTMLScriptElement::setCharset( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_CHARSET, value);
}

bool HTMLScriptElement::defer() const
{
    if(!impl) return 0;
    return !((ElementImpl *)impl)->getAttribute(ATTR_DEFER).isNull();
}

void HTMLScriptElement::setDefer( bool _defer )
{

    if(impl)
        ((ElementImpl *)impl)->setAttribute(ATTR_DEFER,_defer ? "" : 0);
}

DOMString HTMLScriptElement::src() const
{
    if(!impl) return DOMString();
    DOMString s = ((ElementImpl *)impl)->getAttribute(ATTR_SRC);
    if (!s.isNull())
	s = ownerDocument().completeURL( s );
    return s;
}

void HTMLScriptElement::setSrc( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_SRC, value);
}

DOMString HTMLScriptElement::type() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_TYPE);
}

void HTMLScriptElement::setType( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_TYPE, value);
}

// --------------------------------------------------------------------------

HTMLStyleElement::HTMLStyleElement() : HTMLElement()
{
}

HTMLStyleElement::HTMLStyleElement(const HTMLStyleElement &other) : HTMLElement(other)
{
}

HTMLStyleElement::HTMLStyleElement(HTMLStyleElementImpl *impl) : HTMLElement(impl)
{
}

HTMLStyleElement &HTMLStyleElement::operator = (const Node &other)
{
    assignOther( other, ID_STYLE );
    return *this;
}

HTMLStyleElement &HTMLStyleElement::operator = (const HTMLStyleElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLStyleElement::~HTMLStyleElement()
{
}

bool HTMLStyleElement::disabled() const
{
    if(!impl) return 0;
    return !((HTMLStyleElementImpl *)impl)->getAttribute(ATTR_DISABLED).isNull();
}

void HTMLStyleElement::setDisabled( bool _disabled )
{

    if(impl)
        ((ElementImpl *)impl)->setAttribute(ATTR_DISABLED,_disabled ? "" : 0);
}

DOMString HTMLStyleElement::media() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_MEDIA);
}

void HTMLStyleElement::setMedia( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_MEDIA, value);
}

DOMString HTMLStyleElement::type() const
{
    if(!impl) return DOMString();
    return ((ElementImpl *)impl)->getAttribute(ATTR_TYPE);
}

void HTMLStyleElement::setType( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute(ATTR_TYPE, value);
}

StyleSheet HTMLStyleElement::sheet() const
{
    if(!impl) return 0;
    return ((HTMLStyleElementImpl *)impl)->sheet();
}


// --------------------------------------------------------------------------

HTMLTitleElement::HTMLTitleElement() : HTMLElement()
{
}

HTMLTitleElement::HTMLTitleElement(const HTMLTitleElement &other) : HTMLElement(other)
{
}

HTMLTitleElement::HTMLTitleElement(HTMLTitleElementImpl *impl) : HTMLElement(impl)
{
}

HTMLTitleElement &HTMLTitleElement::operator = (const Node &other)
{
    assignOther( other, ID_TITLE );
    return *this;
}

HTMLTitleElement &HTMLTitleElement::operator = (const HTMLTitleElement &other)
{
    HTMLElement::operator = (other);
    return *this;
}

HTMLTitleElement::~HTMLTitleElement()
{
}

DOMString HTMLTitleElement::text() const
{
    if(!impl) return DOMString();
    return ((HTMLTitleElementImpl *)impl)->text();
}

void HTMLTitleElement::setText( const DOMString &value )
{
    if(impl) ((HTMLTitleElementImpl *)impl)->setText(value);
}

