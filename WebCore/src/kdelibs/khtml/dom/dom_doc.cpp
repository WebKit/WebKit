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
 * $Id$
 */

#include "dom_string.h"
#include "dom_doc.h"
#include "dom_docimpl.h"
#include "dom_node.h"
#include "dom_nodeimpl.h"
#include "dom_exception.h"
#include "dom_element.h"
#include "dom_elementimpl.h"
#include "dom_text.h"
#include "dom_xml.h"
#include "dom2_range.h"
#include "dom2_traversal.h"
#include "dom2_events.h"
#include "dom2_views.h"
#include "css_value.h"
#include <kdebug.h>

using namespace DOM;


DOMImplementation::DOMImplementation()
{
    impl = 0;
}

DOMImplementation::DOMImplementation(const DOMImplementation &other)
{
    impl = other.impl;
    if (impl) impl->ref();
}

DOMImplementation::DOMImplementation(DOMImplementationImpl *i)
{
    impl = i;
    if (impl) impl->ref();
}

DOMImplementation &DOMImplementation::operator = (const DOMImplementation &other)
{
    if (impl) impl->deref();
    impl = other.impl;
    if (impl) impl->ref();

    return *this;
}

DOMImplementation::~DOMImplementation()
{
    if (impl) impl->deref();
}

bool DOMImplementation::hasFeature( const DOMString &feature, const DOMString &version )
{
    if (impl) return impl->hasFeature(feature,version);
    return false;
}

CSSStyleSheet DOMImplementation::createCSSStyleSheet(const DOMString &title, const DOMString &media)
{
    if (impl) return impl->createCSSStyleSheet(title.implementation(),media.implementation());
    return 0;
}


DOMImplementationImpl *DOMImplementation::handle() const
{
    return impl;
}

bool DOMImplementation::isNull() const
{
    return (impl == 0);
}

// ----------------------------------------------------------------------------

Document::Document() : Node()
{
    // we always wan't an implementation
    impl = new DocumentImpl();
    impl->ref();
//    kdDebug(6090) << "Document::Document()" << endl;
}

Document::Document(bool create) : Node()
{
    if(create)
    {
	impl = new DocumentImpl();
	impl->ref();
    }
    else
	impl = 0;
//    kdDebug(6090) << "Document::Document(bool)" << endl;
}

Document::Document(const Document &other) : Node(other)
{
//    kdDebug(6090) << "Document::Document(Document &)" << endl;
}

Document::Document(DocumentImpl *i) : Node(i)
{
//    kdDebug(6090) << "Document::Document(DocumentImpl)" << endl;
}

Document &Document::operator = (const Node &other)
{
    if(other.nodeType() != DOCUMENT_NODE)
    {
	impl = 0;
	return *this;
    }
    Node::operator =(other);
    return *this;
}

Document &Document::operator = (const Document &other)
{
    Node::operator =(other);
    return *this;
}

Document::~Document()
{
//    kdDebug(6090) << "Document::~Document\n" << endl;
}

DocumentType Document::doctype() const
{
    if (impl) return ((DocumentImpl *)impl)->doctype();
    return 0;
}

DOMImplementation Document::implementation() const
{
    if (impl) return ((DocumentImpl *)impl)->implementation();
    return 0;
}

Element Document::documentElement() const
{
    if (impl) return ((DocumentImpl *)impl)->documentElement();
    return 0;
}

Element Document::createElement( const DOMString &tagName )
{
    if (impl) return ((DocumentImpl *)impl)->createElement(tagName);
    return 0;
}

Element Document::createElementNS( const DOMString &namespaceURI, const DOMString &qualifiedName )
{
    if (impl) return ((DocumentImpl *)impl)->createElementNS(namespaceURI,qualifiedName);
    return 0;
}

DocumentFragment Document::createDocumentFragment(  )
{
    if (impl) return ((DocumentImpl *)impl)->createDocumentFragment();
    return 0;
}

Text Document::createTextNode( const DOMString &data )
{
    if (impl) return ((DocumentImpl *)impl)->createTextNode( data );
    return 0;
}

Comment Document::createComment( const DOMString &data )
{
    if (impl) return ((DocumentImpl *)impl)->createComment( data );
    return 0;
}

CDATASection Document::createCDATASection( const DOMString &data )
{
    // ### DOM1 spec says raise exception if html documents - what about XHTML documents?
    if (impl) return ((DocumentImpl *)impl)->createCDATASection( data );
    return 0;
}

ProcessingInstruction Document::createProcessingInstruction( const DOMString &target, const DOMString &data )
{
    if (impl) return ((DocumentImpl *)impl)->createProcessingInstruction( target, data );
    return 0;
}

Attr Document::createAttribute( const DOMString &name )
{
    if (impl) return ((DocumentImpl *)impl)->createAttribute( name );
    return 0;
}

Attr Document::createAttributeNS( const DOMString &namespaceURI, const DOMString &qualifiedName )
{
    if (impl) return ((DocumentImpl *)impl)->createAttributeNS( namespaceURI, qualifiedName );
    return 0;
}

EntityReference Document::createEntityReference( const DOMString &name )
{
    if (impl) return ((DocumentImpl *)impl)->createEntityReference( name );
    return 0;
}

NodeList Document::getElementsByTagName( const DOMString &tagName )
{
    if (impl) return ((DocumentImpl *)impl)->getElementsByTagName( tagName );
    return 0;
}

bool Document::isHTMLDocument() const
{
    if (impl) return ((DocumentImpl *)impl)->isHTMLDocument();
    return 0;
}

Range Document::createRange()
{
    if (impl) return ((DocumentImpl *)impl)->createRange();
    return 0;
}

/*Range Document::createRange(const Node &sc, const long so, const Node &ec, const long eo)
{
// ### not part of the DOM
    Range r;
    r.setStart( sc, so );
    r.setEnd( ec, eo );
    return r;
}*/

/*NodeIterator Document::createNodeIterator()
{
// ### not part of the DOM
//  return NodeIterator( *this );
    return NodeIterator();
}*/

/*NodeIterator Document::createNodeIterator(long _whatToShow, NodeFilter *filter)
{
// ### not part of the DOM
//  return NodeIterator( *this, _whatToShow, filter );
    return NodeIterator();
}*/

/*TreeWalker Document::createTreeWalker()
{
// ### not part of the DOM
//  return TreeWalker( *this );
  return TreeWalker();
}*/

/*TreeWalker Document::createTreeWalker(long _whatToShow, NodeFilter *filter )
{
// ### not part of the DOM
//  return TreeWalker( *this, _whatToShow, filter);
  return TreeWalker();
}*/

NodeIterator Document::createNodeIterator(Node root, unsigned long whatToShow,
                                    NodeFilter filter, bool entityReferenceExpansion)
{
    if (!impl)
	throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    NodeIteratorImpl *r = static_cast<DocumentImpl*>(impl)->createNodeIterator(root.handle(),
			  whatToShow,filter,entityReferenceExpansion,exceptioncode);
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

TreeWalker Document::createTreeWalker(Node root, unsigned long whatToShow, NodeFilter filter,
                                bool entityReferenceExpansion)
{
    if (impl) return ((DocumentImpl *)impl)->createTreeWalker(root,whatToShow,filter,entityReferenceExpansion);
    return 0;
}

Event Document::createEvent(const DOMString &eventType)
{
    if (!impl)
	throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    EventImpl *r = ((DocumentImpl *)impl)->createEvent(eventType,exceptioncode);
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

AbstractView Document::defaultView() const
{
    if (!impl)
	throw DOMException(DOMException::INVALID_STATE_ERR);

    return ((DocumentImpl *)impl)->defaultView();
}

StyleSheetList Document::styleSheets() const
{
    if (!impl)
	throw DOMException(DOMException::INVALID_STATE_ERR);

    return ((DocumentImpl *)impl)->styleSheets();
}


KHTMLView *Document::view() const
{
    return ((DocumentImpl*)impl)->view();
}

CSSStyleDeclaration Document::getOverrideStyle(const Element &elt, const DOMString &pseudoElt)
{
    if (!impl)
	throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    CSSStyleDeclarationImpl *r = ((DocumentImpl *)impl)->getOverrideStyle(static_cast<ElementImpl*>(elt.handle()),pseudoElt.implementation());
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

// ----------------------------------------------------------------------------

DocumentFragment::DocumentFragment() : Node()
{
}

DocumentFragment::DocumentFragment(const DocumentFragment &other) : Node(other)
{
}

DocumentFragment &DocumentFragment::operator = (const Node &other)
{
    if(other.nodeType() != DOCUMENT_FRAGMENT_NODE)
    {
	impl = 0;
	return *this;
    }
    Node::operator =(other);
    return *this;
}

DocumentFragment &DocumentFragment::operator = (const DocumentFragment &other)
{
    Node::operator =(other);
    return *this;
}

DocumentFragment::~DocumentFragment()
{
}

DocumentFragment::DocumentFragment(DocumentFragmentImpl *i) : Node(i)
{
}

// ----------------------------------------------------------------------------

DocumentType::DocumentType() : Node()
{
}

DocumentType::DocumentType(const DocumentType &other) : Node(other)
{
}

DocumentType &DocumentType::operator = (const Node &other)
{
    if(other.nodeType() != DOCUMENT_TYPE_NODE)
    {
	impl = 0;
	return *this;
    }

    Node::operator =(other);
    return *this;
}

DocumentType &DocumentType::operator = (const DocumentType &other)
{
    Node::operator =(other);
    return *this;
}

DocumentType::~DocumentType()
{
}

DOMString DocumentType::name() const
{
    if (impl)
	return static_cast<DocumentTypeImpl*>(impl)->name();
    return DOMString();
}

NamedNodeMap DocumentType::entities() const
{
    if (impl)
	return static_cast<DocumentTypeImpl*>(impl)->entities();
    return 0;
}

NamedNodeMap DocumentType::notations() const
{
    if (impl)
	return static_cast<DocumentTypeImpl*>(impl)->notations();
    return 0;
}

DocumentType::DocumentType(DocumentTypeImpl *impl) : Node(impl)
{
}


