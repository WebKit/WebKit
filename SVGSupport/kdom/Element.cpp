/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

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

#include "Attr.h"
#include "Ecma.h"
#include "Element.h"
#include "TypeInfo.h"
#include "DOMException.h"
#include "NodeList.h"
#include "NamedNodeMap.h"

#include "DOMConstants.h"
#include "Element.lut.h"
#include "ElementImpl.h"
#include "DocumentImpl.h"
#include "NamedAttrMapImpl.h"
#include "TypeInfoImpl.h"
#include "AttrImpl.h"
using namespace KDOM;
using namespace KJS;

/*
@begin Element::s_hashTable 2
 tagName				ElementConstants::TagName					DontDelete|ReadOnly
 schemaTypeInfo			ElementConstants::SchemaTypeInfo			DontDelete|ReadOnly
@end
@begin ElementProto::s_hashTable 19
 getAttribute			ElementConstants::GetAttribute				DontDelete|Function 1
 setAttribute			ElementConstants::SetAttribute				DontDelete|Function 2
 removeAttribute		ElementConstants::RemoveAttribute			DontDelete|Function 1
 getAttributeNode		ElementConstants::GetAttributeNode			DontDelete|Function 1
 setAttributeNode		ElementConstants::SetAttributeNode			DontDelete|Function 1
 removeAttributeNode	ElementConstants::RemoveAttributeNode		DontDelete|Function 1
 getElementsByTagName	ElementConstants::GetElementsByTagName		DontDelete|Function 1
 getAttributeNS			ElementConstants::GetAttributeNS			DontDelete|Function 2
 setAttributeNS			ElementConstants::SetAttributeNS			DontDelete|Function 3
 removeAttributeNS		ElementConstants::RemoveAttributeNS			DontDelete|Function 2
 getAttributeNodeNS		ElementConstants::GetAttributeNodeNS		DontDelete|Function 2
 setAttributeNodeNS		ElementConstants::SetAttributeNodeNS		DontDelete|Function 1
 getElementsByTagNameNS	ElementConstants::GetElementsByTagNameNS	DontDelete|Function 2
 hasAttribute			ElementConstants::HasAttribute				DontDelete|Function 1
 hasAttributeNS			ElementConstants::HasAttributeNS			DontDelete|Function 2
 setIdAttribute			ElementConstants::SetIdAttribute			DontDelete|Function 2
 setIdAttributeNS		ElementConstants::SetIdAttributeNS			DontDelete|Function 3
 setIdAttributeNode		ElementConstants::SetIdAttributeNode		DontDelete|Function 2
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("Element", ElementProto, ElementProtoFunc)

ValueImp *Element::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case ElementConstants::TagName:
			return getDOMString(tagName());
		case ElementConstants::SchemaTypeInfo:
			return safe_cache<TypeInfo>(exec, schemaTypeInfo());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

ValueImp *ElementProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(Element)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case ElementConstants::GetAttribute:
		{
			DOMString name = toDOMString(exec, args[0]);
			return getDOMString(obj.getAttribute(name));
		}
		case ElementConstants::SetAttribute:
		{
			DOMString name = toDOMString(exec, args[0]);
			DOMString value = toDOMString(exec, args[1]);
			obj.setAttribute(name, value);
			return Undefined();
		}
		case ElementConstants::RemoveAttribute:
		{
			DOMString name = toDOMString(exec, args[0]);
			obj.removeAttribute(name);
			return Undefined();
		}
		case ElementConstants::GetAttributeNode:
		{
			DOMString name = toDOMString(exec, args[0]);
			return getDOMNode(exec, obj.getAttributeNode(name));
		}
		case ElementConstants::SetAttributeNode:
		{
			Attr newAttr = ecma_cast<Attr>(exec, args[0], &toAttr);
			return getDOMNode(exec, obj.setAttributeNode(newAttr));
		}
		case ElementConstants::RemoveAttributeNode:
		{
			Attr oldAttr = ecma_cast<Attr>(exec, args[0], &toAttr);
			return getDOMNode(exec, obj.removeAttributeNode(oldAttr));
		}
		case ElementConstants::GetElementsByTagName:
		{
			DOMString name = toDOMString(exec, args[0]);
			return safe_cache<NodeList>(exec, obj.getElementsByTagName(name));
		}
		case ElementConstants::GetAttributeNS:
		{
			DOMString namespaceURI = toDOMString(exec, args[0]);
			DOMString localName = toDOMString(exec, args[1]);
			return getDOMString(obj.getAttributeNS(namespaceURI, localName));
		}
		case ElementConstants::SetAttributeNS:
		{
			DOMString namespaceURI = toDOMString(exec, args[0]);
			DOMString qualifiedName = toDOMString(exec, args[1]);
			DOMString value = toDOMString(exec, args[2]);
			obj.setAttributeNS(namespaceURI, qualifiedName, value);
			return Undefined();
		}
		case ElementConstants::RemoveAttributeNS:
		{
			DOMString namespaceURI = toDOMString(exec, args[0]);
			DOMString localName = toDOMString(exec, args[1]);
			obj.removeAttributeNS(namespaceURI, localName);
			return Undefined();
		}
		case ElementConstants::GetAttributeNodeNS:
		{
			DOMString namespaceURI = toDOMString(exec, args[0]);
			DOMString localName = toDOMString(exec, args[1]);
			return getDOMNode(exec, obj.getAttributeNodeNS(namespaceURI, localName));
		}
		case ElementConstants::SetAttributeNodeNS:
		{
			Attr newAttr = ecma_cast<Attr>(exec, args[0], &toAttr);
			return getDOMNode(exec, obj.setAttributeNodeNS(newAttr));
		}
		case ElementConstants::GetElementsByTagNameNS:
		{
			DOMString namespaceURI = toDOMString(exec, args[0]);
			DOMString localName = toDOMString(exec, args[1]);
			return safe_cache<NodeList>(exec, obj.getElementsByTagNameNS(namespaceURI, localName));
		}
		case ElementConstants::HasAttribute:
		{
			DOMString name = toDOMString(exec, args[0]);
			return KJS::Boolean(obj.hasAttribute(name));
		}
		case ElementConstants::HasAttributeNS:
		{
			DOMString namespaceURI = toDOMString(exec, args[0]);
			DOMString localName = toDOMString(exec, args[1]);
			return KJS::Boolean(obj.hasAttributeNS(namespaceURI, localName));
		}
		case ElementConstants::SetIdAttribute:
		{
			DOMString name = toDOMString(exec, args[0]);
			bool isId = args[1]->toBoolean(exec);
			obj.setIdAttribute(name, isId);
			return Undefined();
		}
		case ElementConstants::SetIdAttributeNS:
		{
			DOMString namespaceURI = toDOMString(exec, args[0]);
			DOMString localName = toDOMString(exec, args[1]);
			bool isId = args[2]->toBoolean(exec);
			obj.setIdAttributeNS(namespaceURI, localName, isId);
			return Undefined();
		}
		case ElementConstants::SetIdAttributeNode:
		{
			Attr idAttr = ecma_cast<Attr>(exec, args[0], &toAttr);
			bool isId = args[1]->toBoolean(exec);
			obj.setIdAttributeNode(idAttr, isId);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<ElementImpl *>(d))

Element Element::null;

Element::Element() : Node()
{
}

Element::Element(ElementImpl *i) : Node(i)
{
}

Element::Element(const Element &other) : Node()
{
	(*this) = other;
}

Element::Element(const Node &other) : Node()
{
	(*this)=other;
}

Element::~Element()
{
}

Element &Element::operator=(const Element &other)
{
	Node::operator=(other);
	return *this;
}

KDOM_NODE_DERIVED_ASSIGN_OP(Element, ELEMENT_NODE)

DOMString Element::tagName() const
{
	if(!d)
		return DOMString();

	return impl->tagName();
}

DOMString Element::getAttribute(const DOMString &name) const
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return impl->getAttribute(name);
}

void Element::setAttribute(const DOMString &name, const DOMString &value)
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	impl->setAttribute(name.implementation(), value.implementation());
}

void Element::removeAttribute(const DOMString &name)
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return impl->removeAttribute(name.implementation());
}

Attr Element::getAttributeNode(const DOMString &name) const
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return Attr(impl->getAttributeNode(name.implementation()));
}

Attr Element::setAttributeNode(const Attr &newAttr)
{
	if(!d || newAttr == Attr::null)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	AttrImpl *attr = static_cast<AttrImpl *>(newAttr.handle());
	return Attr(impl->setAttributeNode(attr));
}

Attr Element::removeAttributeNode(const Attr &oldAttr)
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	AttrImpl *attr = static_cast<AttrImpl *>(oldAttr.handle());
	return Attr(impl->removeAttributeNode(attr));
}

NodeList Element::getElementsByTagName(const DOMString &name) const
{
	if(!d)
		return NodeList::null;

	return NodeList(impl->getElementsByTagName(name));
}

DOMString Element::getAttributeNS(const DOMString &namespaceURI, const DOMString &localName) const
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return impl->getAttributeNS(namespaceURI, localName);
}

void Element::setAttributeNS(const DOMString &namespaceURI, const DOMString &qualifiedName, const DOMString &value)
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	impl->setAttributeNS(namespaceURI, qualifiedName, value);
}

void Element::removeAttributeNS(const DOMString &namespaceURI, const DOMString &localName)
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	impl->removeAttributeNS(namespaceURI.implementation(), localName.implementation());
}

Attr Element::getAttributeNodeNS(const DOMString &namespaceURI, const DOMString &localName) const
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return Attr(static_cast<AttrImpl *>(impl->getAttributeNodeNS(namespaceURI.implementation(), localName.implementation())));
}

Attr Element::setAttributeNodeNS(const Attr &newAttr)
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	AttrImpl *n = static_cast<AttrImpl *>(newAttr.handle());
	return Attr(impl->setAttributeNodeNS(n));
}

NodeList Element::getElementsByTagNameNS(const DOMString &namespaceURI, const DOMString &localName) const
{
	if(!d)
		return NodeList::null;

	return NodeList(impl->getElementsByTagNameNS(namespaceURI, localName));
}

bool Element::hasAttribute(const DOMString &name) const
{
	if(!d || !impl->attributes())
		return false;
		
	return impl->hasAttribute(name);
}

bool Element::hasAttributeNS(const DOMString &namespaceURI, const DOMString &localName) const
{
	if(!d || !impl->attributes())
		return false;
		
	return impl->hasAttributeNS(namespaceURI, localName);
}

void Element::setIdAttribute(const DOMString &name, bool isId)
{
	if(!d)
		return;

	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	if(impl->isReadOnly())
		throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

	impl->setIdAttribute(name, isId);
}

void Element::setIdAttributeNS(const DOMString &namespaceURI, const DOMString &localName, bool isId)
{
	if(!d)
		return;
		
	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	if(impl->isReadOnly())
		throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

	impl->setIdAttributeNS(namespaceURI, localName, isId);
}

void Element::setIdAttributeNode(const Attr &idAttr, bool isId)
{
	if(!d)
		return;
	
	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	if(impl->isReadOnly())
		throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

	impl->setIdAttributeNode(static_cast<AttrImpl *>(idAttr.handle()), isId);
}

TypeInfo Element::schemaTypeInfo() const
{
	if(!d)
		return TypeInfo();

	return TypeInfo(impl->schemaTypeInfo());
}

// vim:ts=4:noet
