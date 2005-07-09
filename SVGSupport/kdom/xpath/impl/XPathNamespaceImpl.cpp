/*
    Copyright (C) 2005 Frans Englich <frans.englich@telia.com>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "XPathNamespace.h"
#include "kdom.h"

#include "DOMExceptionImpl.h"
#include "DocumentImpl.h"
#include "ElementImpl.h"
#include "XPathNamespaceImpl.h"

using namespace KDOM;
	
XPathNamespaceImpl::XPathNamespaceImpl(DocumentImpl *doc) : NodeImpl(doc)
{
}

XPathNamespaceImpl::~XPathNamespaceImpl()
{
}

DOMString XPathNamespaceImpl::prefix() const
{
	return DOMString(); // TODO
}

DocumentImpl *XPathNamespaceImpl::ownerDocument() const
{
	return 0; // TODO
}

ElementImpl *XPathNamespaceImpl::ownerElement() const
{
	return 0; // TODO
}

DOMString XPathNamespaceImpl::localName() const
{
	return prefix();
}

DOMString XPathNamespaceImpl::nodeValue() const
{
	return namespaceURI();
}

unsigned short XPathNamespaceImpl::nodeType() const
{
	return XPathNamespace::XPATH_NAMESPACE_NODE;
}

DOMString XPathNamespaceImpl::nodeName() const
{
	return "#namespace";
}

NodeImpl *XPathNamespaceImpl::cloneNode(bool, DocumentImpl *) const
{
	throw new DOMExceptionImpl(NOT_SUPPORTED_ERR);
}

NamedAttrMapImpl *XPathNamespaceImpl::attributes() const
{
	return 0;
}

NodeListImpl *XPathNamespaceImpl::childNodes() const
{
	return 0;
}

NodeImpl *XPathNamespaceImpl::parentNode() const
{
	return 0;
}

NodeImpl *XPathNamespaceImpl::previousSibling() const
{
	return 0;
}

NodeImpl *XPathNamespaceImpl::nextSibling() const
{
	return 0;
}

NodeImpl *XPathNamespaceImpl::firstChild() const
{
	return 0;
}

NodeImpl *XPathNamespaceImpl::lastChild() const
{
	return 0;
}

bool XPathNamespaceImpl::hasChildNodes() const
{
	return false;
}

bool XPathNamespaceImpl::hasAttributes() const
{
	return false;
}

bool XPathNamespaceImpl::isSupported(const DOMString &, const DOMString &) const
{
	return false;
}

unsigned short XPathNamespaceImpl::compareDocumentPosition(NodeImpl *) const
{
	return 0;
}

bool XPathNamespaceImpl::isDefaultNamespace(const DOMString &) const
{
	return false;
}

bool XPathNamespaceImpl::isSameNode(NodeImpl *) const
{
	return false;
}

bool XPathNamespaceImpl::isEqualNode(NodeImpl *) const
{
	return false;
}

DOMString XPathNamespaceImpl::textContent() const
{
	return DOMString();
}

DOMString XPathNamespaceImpl::lookupNamespaceURI(const DOMString &) const
{
	return DOMString();
}

DOMString XPathNamespaceImpl::lookupPrefix(const DOMString &) 
{
	return DOMString();
}

void XPathNamespaceImpl::setNodeValue(const DOMString &)
{
	throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
}

NodeImpl *XPathNamespaceImpl::insertBefore(NodeImpl *, NodeImpl *)
{
	throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
}

NodeImpl *XPathNamespaceImpl::replaceChild(NodeImpl *, NodeImpl *)
{
	throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
}

NodeImpl *XPathNamespaceImpl::removeChild(NodeImpl *)
{
	throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
}

NodeImpl *XPathNamespaceImpl::appendChild(NodeImpl *)
{
	throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
}

void XPathNamespaceImpl::setOwnerDocument(DocumentImpl *)
{
	throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
}

void XPathNamespaceImpl::normalize()
{
	throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
}

void XPathNamespaceImpl::setPrefix(const DOMString &)
{
	throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
}

void XPathNamespaceImpl::setTextContent(const DOMString &)
{
	throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
}

// vim:ts=4:noet
