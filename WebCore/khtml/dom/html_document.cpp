/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
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
// --------------------------------------------------------------------------

#include "html_document.h"

#include "dom/html_misc.h"
#include "xml/dom_textimpl.h"
#include "html/html_documentimpl.h"
#include "html/html_miscimpl.h"
#include "dom/html_image.h"
#include "dom/html_form.h"
#include "html/html_imageimpl.h"
#include "html/html_formimpl.h"
#include "dom/dom_exception.h"

using namespace DOM;

HTMLDocument::HTMLDocument() : Document(false) // create the impl here
{
    impl = DOMImplementationImpl::instance()->createHTMLDocument();
    impl->ref();

}

HTMLDocument::HTMLDocument(KHTMLView *parent)
    : Document(false) // create the impl here
{
    impl = DOMImplementationImpl::instance()->createHTMLDocument(parent);
    impl->ref();
}

HTMLDocument::HTMLDocument(const HTMLDocument &other) : Document(other)
{
}

HTMLDocument::HTMLDocument(HTMLDocumentImpl *impl) : Document(impl)
{
}

HTMLDocument &HTMLDocument::operator = (const Node &other)
{
    if(other.nodeType() != DOCUMENT_NODE) {
	if ( impl ) impl->deref();
	impl = 0;
    } else {
	DocumentImpl *d = static_cast<DocumentImpl *>(other.handle());
	if(!d->isHTMLDocument()) {
	    if ( impl ) impl->deref();
	impl = 0;
	} else {
	Node::operator =(other);
	}
    }
    return *this;
}

HTMLDocument &HTMLDocument::operator = (const HTMLDocument &other)
{
    Document::operator =(other);
    return *this;
}

HTMLDocument::~HTMLDocument()
{
}

DOMString HTMLDocument::title() const
{
    if(!impl) return DOMString();
    return static_cast<HTMLDocumentImpl *>(impl)->title();
}

void HTMLDocument::setTitle( const DOMString &v )
{
    if (!impl) return;
    ((HTMLDocumentImpl *)impl)->setTitle(v);
}

DOMString HTMLDocument::referrer() const
{
    if(!impl) return DOMString();
    return ((HTMLDocumentImpl *)impl)->referrer();
}

DOMString HTMLDocument::completeURL(const DOMString& str) const
{
    if(!impl) return str;
    return ((HTMLDocumentImpl *)impl)->completeURL(str);
}

DOMString HTMLDocument::domain() const
{
    if(!impl) return DOMString();
    return ((HTMLDocumentImpl *)impl)->domain();
}

DOMString HTMLDocument::lastModified() const
{
    if(!impl) return DOMString();
    return ((HTMLDocumentImpl *)impl)->lastModified();
}

DOMString HTMLDocument::URL() const
{
    if(!impl) return DOMString();
    return ((HTMLDocumentImpl *)impl)->URL();
}

HTMLElement HTMLDocument::body() const
{
    if(!impl) return 0;
    return ((HTMLDocumentImpl *)impl)->body();
}

void HTMLDocument::setBody(const HTMLElement &_body)
{
    if (!impl) return;
    int exceptioncode = 0;
    ((HTMLDocumentImpl *)impl)->setBody(static_cast<HTMLElementImpl *>(_body.handle()), exceptioncode);
    if ( exceptioncode )
        throw DOMException( exceptioncode );
    return;
}

HTMLCollection HTMLDocument::images() const
{
    if(!impl) return HTMLCollection();
    return HTMLCollection(impl, HTMLCollectionImpl::DOC_IMAGES);
}

HTMLCollection HTMLDocument::applets() const
{
    if(!impl) return HTMLCollection();
    return HTMLCollection(impl, HTMLCollectionImpl::DOC_APPLETS);
}

HTMLCollection HTMLDocument::embeds() const
{
    if(!impl) return HTMLCollection();
    return HTMLCollection(impl, HTMLCollectionImpl::DOC_EMBEDS);
}

HTMLCollection HTMLDocument::objects() const
{
    if(!impl) return HTMLCollection();
    return HTMLCollection(impl, HTMLCollectionImpl::DOC_OBJECTS);
}

HTMLCollection HTMLDocument::links() const
{
    if(!impl) return HTMLCollection();
    return HTMLCollection(impl, HTMLCollectionImpl::DOC_LINKS);
}

HTMLCollection HTMLDocument::forms() const
{
    if(!impl) return HTMLCollection();
    return HTMLCollection(impl, HTMLCollectionImpl::DOC_FORMS);
}

HTMLCollection HTMLDocument::anchors() const
{
    if(!impl) return HTMLCollection();
    return HTMLCollection(impl, HTMLCollectionImpl::DOC_ANCHORS);
}

HTMLCollection HTMLDocument::all() const
{
    if(!impl) return HTMLCollection();
    return HTMLCollection(impl, HTMLCollectionImpl::DOC_ALL);
}

HTMLCollection HTMLDocument::nameableItems() const
{
    if(!impl) return HTMLCollection();
    return HTMLCollection(impl, HTMLCollectionImpl::DOC_NAMEABLE_ITEMS);
}

DOMString HTMLDocument::cookie() const
{
   if (!impl) return DOMString();
   return ((HTMLDocumentImpl *)impl)->cookie();
}

void HTMLDocument::setCookie( const DOMString & value )
{
   if (impl)
        ((HTMLDocumentImpl *)impl)->setCookie(value);

}

void HTMLDocument::setPolicyBaseURL( const DOMString &s )
{
   if (impl)
        ((HTMLDocumentImpl *)impl)->setPolicyBaseURL(s);
}

void HTMLDocument::open(  )
{
    if(impl)
        ((HTMLDocumentImpl *)impl)->open(  );
}

void HTMLDocument::close(  )
{
    if(impl)
        ((HTMLDocumentImpl *)impl)->close(  );
}

void HTMLDocument::write( const DOMString &text )
{
    if(impl)
        ((HTMLDocumentImpl *)impl)->write( text );
}

void HTMLDocument::writeln( const DOMString &text )
{
    if(impl)
        ((HTMLDocumentImpl *)impl)->writeln( text );
}

NodeList HTMLDocument::getElementsByName( const DOMString &elementName )
{
    if(!impl) return 0;
    return static_cast<HTMLDocumentImpl *>(impl)->getElementsByName(elementName).get();
}
