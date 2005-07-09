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

#ifndef KDOM_XPathNamespaceImpl_H
#define KDOM_XPathNamespaceImpl_H

#include <kdom/impl/NodeImpl.h>

namespace KDOM
{
	class DocumentImpl;
	class DOMString;
	class ElementImpl;

	class XPathNamespaceImpl : public NodeImpl
	{
	public:
		XPathNamespaceImpl(DocumentImpl *doc);
		XPathNamespaceImpl(DocumentImpl *doc, const DOMString &prefix, bool nullNSSpecified = false);
		virtual ~XPathNamespaceImpl();

	virtual ElementImpl *ownerElement() const;
	virtual DocumentImpl *ownerDocument() const;
	virtual DOMString nodeName() const;
	virtual DOMString prefix() const;
	virtual DOMString localName() const;
	virtual unsigned short nodeType() const;
	virtual DOMString nodeValue() const;
	virtual NodeImpl *cloneNode(bool deep, DocumentImpl *doc) const;


	virtual NamedAttrMapImpl *attributes() const;

	virtual NodeListImpl *childNodes() const;

	NodeImpl *parentNode() const;
	NodeImpl *previousSibling() const;
	NodeImpl *nextSibling() const;

	virtual NodeImpl *firstChild() const;
	virtual NodeImpl *lastChild() const;


	virtual void setNodeValue(const DOMString &nodeValue);
	void setOwnerDocument(DocumentImpl *doc);

	virtual NodeImpl *insertBefore(NodeImpl *newChild, NodeImpl *refChild);
	virtual NodeImpl *replaceChild(NodeImpl *newChild, NodeImpl *oldChild);
	virtual NodeImpl *removeChild(NodeImpl *oldChild);
	virtual NodeImpl *appendChild(NodeImpl *newChild);

	virtual bool hasChildNodes() const;

	virtual void normalize();
	virtual void setPrefix(const DOMString &prefix);

	virtual bool hasAttributes() const;

	virtual bool isSupported(const DOMString &feature, const DOMString &version) const;

	unsigned short compareDocumentPosition(NodeImpl *other) const;

	virtual DOMString textContent() const;
	void setTextContent(const DOMString &text);

	bool isDefaultNamespace(const DOMString &namespaceURI) const;
	bool isSameNode(NodeImpl *other) const;
	bool isEqualNode(NodeImpl *arg) const;

	DOMString lookupNamespaceURI(const DOMString &prefix) const;
	DOMString lookupPrefix(const DOMString &namespaceURI);

};
};

#endif

// vim:ts=4:noet
