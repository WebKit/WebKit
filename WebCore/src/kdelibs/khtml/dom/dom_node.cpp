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

#include "dom_node.h"
#include "dom_doc.h"
#include "dom_exception.h"
#include "dom_string.h"
#include "dom_nodeimpl.h"
#include "dom_elementimpl.h"
#include "dom2_events.h"
#include <qstring.h>
#include <qrect.h>
using namespace DOM;

// ### eventually we should get these methods to throw NOT_FOUND_ERR
// when they are being used with a null node
//
// unfortunately this breaks a whole heap of stuff at the moment :(

NamedNodeMap::NamedNodeMap()
{
    impl = 0;
}

NamedNodeMap::NamedNodeMap(const NamedNodeMap &other)
{
    impl = other.impl;
    if (impl) impl->ref();
}

NamedNodeMap::NamedNodeMap(NamedNodeMapImpl *i)
{
    impl = i;
    if (impl) impl->ref();
}

NamedNodeMap &NamedNodeMap::operator = (const NamedNodeMap &other)
{
    if(impl) impl->deref();
    impl = other.impl;
    if(impl) impl->ref();

    return *this;
}

NamedNodeMap::~NamedNodeMap()
{
    if(impl) impl->deref();
}

Node NamedNodeMap::getNamedItem( const DOMString &name ) const
{
    if (!impl)
	return 0; // ### enable	throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    NodeImpl *r = impl->getNamedItem(name,exceptioncode);
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

Node NamedNodeMap::setNamedItem( const Node &arg )
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    Node r = impl->setNamedItem(arg,exceptioncode);
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

Node NamedNodeMap::removeNamedItem( const DOMString &name )
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    Node r = impl->removeNamedItem(name,exceptioncode);
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

Node NamedNodeMap::item( unsigned long index ) const
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    NodeImpl *r = impl->item(index,exceptioncode);
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

unsigned long NamedNodeMap::length() const
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    unsigned long r = impl->length(exceptioncode);
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

NamedNodeMapImpl *NamedNodeMap::handle() const
{
    return impl;
}

bool NamedNodeMap::isNull() const
{
    return (impl == 0);
}

// ---------------------------------------------------------------------------

Node::Node()
{
    impl = 0;
}

Node::Node(const Node &other)
{
    impl = other.impl;
    if(impl) impl->ref();
}

Node::Node( NodeImpl *i )
{
    impl = i;
    if(impl) impl->ref();
}

Node &Node::operator = (const Node &other)
{
    if(impl == other.impl) return *this;
    if(impl) impl->deref();
    impl = other.impl;
    if(impl) impl->ref();
    return *this;
}

bool Node::operator == (const Node &other)
{
    if(impl == other.impl)
        return true;
    else
        return false;
}

bool Node::operator != (const Node &other)
{
    if(impl != other.impl)
        return true;
    else
        return false;
}

Node::~Node()
{
    if(impl) impl->deref();
}

DOMString Node::nodeName() const
{
    if(impl) return impl->nodeName();
    return DOMString();
}

DOMString Node::nodeValue() const
{
    if(impl) return impl->nodeValue();
    return DOMString();
}

void Node::setNodeValue( const DOMString &_str )
{
    if (!impl)
	return; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    if(impl) impl->setNodeValue( _str,exceptioncode );
    if (exceptioncode)
	throw DOMException(exceptioncode);
}

unsigned short Node::nodeType() const
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    unsigned short r = impl->nodeType();
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

Node Node::parentNode() const
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    NodeImpl *r = impl->parentNode();
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

NodeList Node::childNodes() const
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    NodeListImpl *r = impl->childNodes();
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;

}

Node Node::firstChild() const
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    NodeImpl *r = impl->firstChild();
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

Node Node::lastChild() const
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    NodeImpl *r = impl->lastChild();
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

Node Node::previousSibling() const
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    NodeImpl *r = impl->previousSibling();
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

Node Node::nextSibling() const
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    NodeImpl *r = impl->nextSibling();
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

NamedNodeMap Node::attributes() const
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    NamedNodeMapImpl *r = impl->attributes();
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

Document Node::ownerDocument() const
{
    if (!impl)
	return Document(); // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    DocumentImpl *r = impl->ownerDocument();
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

Node Node::insertBefore( const Node &newChild, const Node &refChild )
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    NodeImpl *r = impl->insertBefore( newChild.impl, refChild.impl, exceptioncode );
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

Node Node::replaceChild( const Node &newChild, const Node &oldChild )
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    NodeImpl *r = impl->replaceChild( newChild.impl, oldChild.impl, exceptioncode );
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

Node Node::removeChild( const Node &oldChild )
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    NodeImpl *r = impl->removeChild( oldChild.impl, exceptioncode );
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

Node Node::appendChild( const Node &newChild )
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    NodeImpl *r = impl->appendChild( newChild.impl, exceptioncode );
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

bool Node::hasChildNodes(  )
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    bool r = impl->hasChildNodes();
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

Node Node::cloneNode( bool deep )
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    NodeImpl *r = impl->cloneNode( deep, exceptioncode );
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

void Node::addEventListener(const DOMString &type,
			  EventListener *listener,
			  const bool useCapture)
{
    if (!impl)
	throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    impl->addEventListener(type,listener,useCapture,exceptioncode);
    if (exceptioncode)
	throw DOMException(exceptioncode);
}

void Node::removeEventListener(const DOMString &type,
			     EventListener *listener,
			     bool useCapture)
{
    if (!impl)
	throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    impl->removeEventListener(type,listener,useCapture,exceptioncode);
    if (exceptioncode)
	throw DOMException(exceptioncode);
}

bool Node::dispatchEvent(const Event &evt)
{
    if (!impl)
	throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    bool r = impl->dispatchEvent(evt.handle(),exceptioncode);
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}


unsigned short Node::elementId() const
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    int r = impl->id();
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

unsigned long Node::index() const
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    return impl->nodeIndex();
}

QString Node::toHTML()
{
    if (!impl)
	return 0; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    QString r = impl->toHTML();
    if (exceptioncode)
	throw DOMException(exceptioncode);
    return r;
}

void Node::applyChanges()
{
    if (!impl)
	return; // ### enable	throw throw DOMException(DOMException::NOT_FOUND_ERR);

    int exceptioncode = 0;
    impl->applyChanges();
    if (exceptioncode)
	throw DOMException(exceptioncode);
}

void Node::getCursor(int offset, int &_x, int &_y, int &height)
{
    if(!impl) return;
    impl->getCursor(offset, _x, _y, height);
}

QRect Node::getRect()
{
    if(!impl) return QRect();
    return impl->getRect();
}

bool Node::isNull() const
{
    return (impl == 0);
}

NodeImpl *Node::handle() const
{
    return impl;
}

//-----------------------------------------------------------------------------

NodeList::NodeList()
{
    impl = 0;
}

NodeList::NodeList(const NodeList &other)
{
    impl = other.impl;
    if(impl) impl->ref();
}

NodeList::NodeList(const NodeListImpl *i)
{
    impl = const_cast<NodeListImpl *>(i);
    if(impl) impl->ref();
}

NodeList &NodeList::operator = (const NodeList &other)
{
    if(impl) impl->deref();
    impl = other.impl;
    if(impl) impl->ref();
    return *this;
}

NodeList::~NodeList()
{
    if(impl) impl->deref();
}

Node NodeList::item( unsigned long index ) const
{
    if(!impl) return 0;
    return impl->item(index);
}

unsigned long NodeList::length() const
{
    if(!impl) return 0;
    return impl->length();
}

NodeListImpl *NodeList::handle() const
{
    return impl;
}

bool NodeList::isNull() const
{
    return (impl == 0);
}

