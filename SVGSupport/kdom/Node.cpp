/*
    Copyright (C) 2004 Nikolas Zimmermann   <wildfox@kde.org>
                  2004 Rob Buis             <buis@kde.org>

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

#include <kurl.h>

#include "Ecma.h"
#include "Node.h"
#include "NodeImpl.h"
#include "NodeList.h"
#include "DOMString.h"
#include "DOMObject.h"
#include "DOMException.h"
#include "NamedNodeMap.h"
#include "DocumentImpl.h"
#include "NodeListImpl.h"
#include "NamedAttrMapImpl.h"
#include "DOMImplementationImpl.h"

#include "DOMConstants.h"
#include "Node.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin Node::s_hashTable 19
 nodeName				NodeConstants::NodeName				DontDelete|ReadOnly
 nodeValue				NodeConstants::NodeValue			DontDelete
 nodeType				NodeConstants::NodeType				DontDelete|ReadOnly
 parentNode				NodeConstants::ParentNode			DontDelete|ReadOnly
 childNodes				NodeConstants::ChildNodes			DontDelete|ReadOnly
 firstChild				NodeConstants::FirstChild			DontDelete|ReadOnly
 lastChild				NodeConstants::LastChild			DontDelete|ReadOnly
 previousSibling		NodeConstants::PreviousSibling		DontDelete|ReadOnly
 nextSibling			NodeConstants::NextSibling			DontDelete|ReadOnly
 attributes				NodeConstants::Attributes			DontDelete|ReadOnly
 ownerDocument			NodeConstants::OwnerDocument		DontDelete|ReadOnly
 namespaceURI			NodeConstants::NamespaceURI			DontDelete|ReadOnly
 prefix					NodeConstants::Prefix				DontDelete
 localName				NodeConstants::LocalName			DontDelete|ReadOnly
 textContent			NodeConstants::TextContent			DontDelete
 baseURI				NodeConstants::BaseURI				DontDelete|ReadOnly
@end
@begin NodeProto::s_hashTable 19
 insertBefore				NodeConstants::InsertBefore				DontDelete|Function 2
 replaceChild				NodeConstants::ReplaceChild				DontDelete|Function 2
 removeChild				NodeConstants::RemoveChild				DontDelete|Function 1
 appendChild				NodeConstants::AppendChild				DontDelete|Function 1
 hasChildNodes				NodeConstants::HasChildNodes			DontDelete|Function 0
 cloneNode					NodeConstants::CloneNode				DontDelete|Function 1
 normalize					NodeConstants::Normalize				DontDelete|Function 0
 isSupported				NodeConstants::IsSupported				DontDelete|Function 2
 hasAttributes				NodeConstants::HasAttributes			DontDelete|Function 0
 compareDocumentPosition 	NodeConstants::CompareDocumentPosition	DontDelete|Function 1
 isDefaultNamespace 		NodeConstants::IsDefaultNamespace		DontDelete|Function 1
 lookupNamespaceURI 		NodeConstants::LookupNamespaceURI		DontDelete|Function 1
 lookupPrefix 				NodeConstants::LookupPrefix				DontDelete|Function 1
 isEqualNode	 			NodeConstants::IsEqualNode				DontDelete|Function 1
 isSameNode		 			NodeConstants::IsSameNode				DontDelete|Function 1
 getFeature					NodeConstants::GetFeature				DontDelete|Function 2
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("Node", NodeProto, NodeProtoFunc)

ValueImp *Node::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case NodeConstants::NodeName:
			return getDOMString(nodeName());
		case NodeConstants::NodeValue:
			return getDOMString(nodeValue());
		case NodeConstants::NodeType:
			return Number(nodeType());
		case NodeConstants::ParentNode:
			return getDOMNode(exec, parentNode());
		case NodeConstants::ChildNodes:
			return safe_cache<NodeList>(exec, childNodes());
		case NodeConstants::FirstChild:
			return getDOMNode(exec, firstChild());
		case NodeConstants::LastChild:
			return getDOMNode(exec, lastChild());
		case NodeConstants::PreviousSibling:
			return getDOMNode(exec, previousSibling());
		case NodeConstants::NextSibling:
			return getDOMNode(exec, nextSibling());
		case NodeConstants::Attributes:
			return safe_cache<NamedNodeMap>(exec, attributes());
		case NodeConstants::OwnerDocument:
			return getDOMNode(exec, ownerDocument());
		case NodeConstants::NamespaceURI:
			return getDOMString(namespaceURI());
		case NodeConstants::Prefix:
			return getDOMString(prefix());
		case NodeConstants::LocalName:
			return getDOMString(localName());
		case NodeConstants::TextContent:
			return getDOMString(textContent());
		case NodeConstants::BaseURI:
			return getDOMString(baseURI());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
};

void Node::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case NodeConstants::NodeValue:
			setNodeValue(toDOMString(exec, value));
			break;
		case NodeConstants::Prefix:
			setPrefix(toDOMString(exec, value));
			break;
		case NodeConstants::TextContent:
			setTextContent(toDOMString(exec, value));
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
}

ValueImp *NodeProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(Node)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case NodeConstants::InsertBefore:
		{
			Node newChild = ecma_cast<Node>(exec, args[0], &toNode);
			Node beforeChild = ecma_cast<Node>(exec, args[1], &toNode);
			return getDOMNode(exec, obj.insertBefore(newChild, beforeChild));
		}
		case NodeConstants::ReplaceChild:
		{
			Node newChild = ecma_cast<Node>(exec, args[0], &toNode);
			Node oldChild = ecma_cast<Node>(exec, args[1], &toNode);
			return getDOMNode(exec, obj.replaceChild(newChild, oldChild));
		}
		case NodeConstants::RemoveChild:
		{
			Node remove = ecma_cast<Node>(exec, args[0], &toNode);
			return getDOMNode(exec, obj.removeChild(remove));
		}
		case NodeConstants::AppendChild:
		{
			Node newChild = ecma_cast<Node>(exec, args[0], &toNode);
			return getDOMNode(exec, obj.appendChild(newChild));
		}
		case NodeConstants::HasChildNodes:
			return KJS::Boolean(obj.hasChildNodes());
		case NodeConstants::CloneNode:
		{
			bool deep = args[0]->toBoolean(exec);
			return getDOMNode(exec, obj.cloneNode(deep));
		}
		case NodeConstants::Normalize:
		{
			obj.normalize();
			return NULL;
		}
		case NodeConstants::IsSupported:
		{
			DOMString feature = toDOMString(exec, args[0]);
			DOMString version = toDOMString(exec, args[1]);
			return KJS::Boolean(obj.isSupported(feature, version));
		}
		case NodeConstants::HasAttributes:
			return KJS::Boolean(obj.hasAttributes());
		case NodeConstants::CompareDocumentPosition:
		{
			Node other = ecma_cast<Node>(exec, args[0], &toNode);
			return Number(obj.compareDocumentPosition(other));
		}
		case NodeConstants::IsSameNode:
		{
			Node other = ecma_cast<Node>(exec, args[0], &toNode);
			return KJS::Boolean(obj.isSameNode(other));
		}
		case NodeConstants::IsDefaultNamespace:
		{
			DOMString namespaceURI = toDOMString(exec, args[0]);
			return KJS::Boolean(obj.isDefaultNamespace(namespaceURI));
		}
		case NodeConstants::LookupNamespaceURI:
		{
			DOMString prefix = toDOMString(exec, args[0]);
			return getDOMString(obj.lookupNamespaceURI(prefix));
		}
		case NodeConstants::LookupPrefix:
		{
			DOMString namespaceURI = toDOMString(exec, args[0]);
			return getDOMString(obj.lookupPrefix(namespaceURI));
		}
		case NodeConstants::IsEqualNode:
		{
			Node other = ecma_cast<Node>(exec, args[0], &toNode);
			return KJS::Boolean(obj.isEqualNode(other));
		}
		case NodeConstants::GetFeature:
		{
			DOMString feature = toDOMString(exec, args[0]);
			DOMString version = toDOMString(exec, args[1]);
			DOMObject object = obj.getFeature(feature, version);
			NodeImpl *imp = static_cast<NodeImpl *>(object.handle());
			if(imp)
				return Node(imp).cache(exec);

			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<NodeImpl *>(d))

Node Node::null;

Node::Node() : EventTarget()
{
}

Node::Node(NodeImpl *i) : EventTarget(i)
{
}

Node::Node(const Node &other) : EventTarget()
{
	(*this) = other;
}

Node::~Node()
{
}

Node &Node::operator=(const Node &other)
{
	EventTarget::operator=(other);
	return *this;
}

DOMString Node::nodeName() const
{
	if(!d)
		return DOMString();

	return impl->nodeName();
}

DOMString Node::nodeValue() const
{
	if(!d)
		return DOMString();

	return impl->nodeValue();
}

void Node::setNodeValue(const DOMString &nodeValue)
{
	if(d)
		impl->setNodeValue(nodeValue);
}

unsigned short Node::nodeType() const
{
	if(!d)
		return 0;

	return impl->nodeType();
}

Node Node::parentNode() const
{
	if(!d)
		return Node::null;

	return Node(impl->parentNode());
}

NodeList Node::childNodes() const
{
	if(!d)
		return NodeList::null;

	return NodeList(impl->childNodes());
}

Node Node::firstChild() const
{
	if(!d)
		return Node::null;

	return Node(impl->firstChild());
}

Node Node::lastChild() const
{
	if(!d)
		return Node::null;

	return Node(impl->lastChild());
}

Node Node::previousSibling() const
{
	if(!d)
		return Node::null;

	return Node(impl->previousSibling());
}

Node Node::nextSibling() const
{
	if(!d)
		return Node::null;

	return Node(impl->nextSibling());
}

NamedNodeMap Node::attributes() const
{
	if(!d)
		return NamedNodeMap::null;

	return NamedNodeMap(impl->attributes());
}

KDOM::Document Node::ownerDocument() const
{
	if(!d || nodeType() == DOCUMENT_NODE)
		return Document::null;

	return Document(impl->ownerDocument());
}

Node Node::insertBefore(const Node &newChild, const Node &refChild)
{
	if(!d)
		return Node::null;

	return Node(impl->insertBefore(static_cast<NodeImpl *>(newChild.d),
								   static_cast<NodeImpl *>(refChild.d)));
}

Node Node::replaceChild(const Node &newChild, const Node &oldChild)
{
	if(!d)
		return Node::null;

	return Node(impl->replaceChild(static_cast<NodeImpl *>(newChild.d),
								   static_cast<NodeImpl *>(oldChild.d)));
}

Node Node::removeChild(const Node &oldChild)
{
	if(!d)
		return Node::null;

	return Node(impl->removeChild(static_cast<NodeImpl *>(oldChild.d)));
}

Node Node::appendChild(const Node &newChild)
{
	if(!d)
		return Node::null;

	return Node(impl->appendChild(static_cast<NodeImpl *>(newChild.d)));
}

bool Node::hasChildNodes() const
{
	if(!d)
		return false;

	return impl->hasChildNodes();
}

Node Node::cloneNode(bool deep) const
{
	if(!d)
		return Node::null;

	return Node(impl->cloneNode(deep, impl->ownerDocument()));
}

void Node::normalize()
{
	if(d)
		impl->normalize();
}

bool Node::isSupported(const DOMString &feature, const DOMString &version) const
{
	if(!d)
		return false;

	return impl->isSupported(feature, version);
}

DOMString Node::namespaceURI() const
{
	if(!d)
		return DOMString();

	return impl->namespaceURI();
}

DOMString Node::prefix() const
{
	if(!d)
		return DOMString();

	return impl->prefix();
}

void Node::setPrefix(const DOMString &prefix)
{
	if(d)
		impl->setPrefix(prefix);
}

DOMString Node::localName() const
{
	if(!d)
		return DOMString();

	return impl->localName();
}

bool Node::hasAttributes() const
{
	if(!d)
		return false;

	return impl->hasAttributes();
}

DOMString Node::textContent() const
{
	if(!d)
		return DOMString();

	return impl->textContent();
}

void Node::setTextContent(const DOMString &text)
{
	if(d)
		impl->setTextContent(text);
}

bool Node::isDefaultNamespace(const DOMString &namespaceURI) const
{
	if(!d)
		return false;

	return impl->isDefaultNamespace(namespaceURI);
}

bool Node::isSameNode(const Node &other) const
{
	if(!d)
		return false;

	return impl->isSameNode(static_cast<NodeImpl *>(other.d));
}

bool Node::isEqualNode(const Node &arg) const
{
	if(!d)
		return false;

	return impl->isEqualNode(static_cast<NodeImpl *>(arg.d));
}

DOMString Node::lookupNamespaceURI(const DOMString &prefix) const
{
	if(!d)
		return DOMString();

	return impl->lookupNamespaceURI(prefix);
}

DOMString Node::lookupPrefix(const DOMString &namespaceURI) const
{
	if(!d)
		return DOMString();

	return impl->lookupPrefix(namespaceURI);
}

DOMString Node::baseURI() const
{
	if(!d)
		return DOMString();

	return impl->baseURI();
}

KURL Node::baseKURI() const
{
	if(!d)
		return KURL();

	return impl->baseKURI();
}

unsigned short Node::compareDocumentPosition(const Node &other) const
{
	if(!d)
		return 0;

	return impl->compareDocumentPosition(static_cast<NodeImpl *>(other.handle()));
}

DOMObject Node::getFeature(const DOMString &feature, const DOMString &version) const
{
	if(d)
	{
		DocumentImpl *doc = impl->ownerDocument();
		if(!doc && nodeType() == DOCUMENT_NODE)
			doc = static_cast<DocumentImpl *>(impl);
		if(doc && doc->implementation() &&
		   doc->implementation()->hasFeature(feature, version))
			return DOMObject(impl);
	}

	return DOMObject::null;
}

// vim:ts=4:noet
