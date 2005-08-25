/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Peter Kelly (pmk@post.com)
              (C) 2001 Dirk Mueller (mueller@kde.org)
              (C) 2003 Apple Computer, Inc.

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

#include "kdom/Helper.h"
#include "AttrImpl.h"
#include "NodeImpl.h"
#include "domattrs.h"
#include "ElementImpl.h"
#include "DocumentImpl.h"
#include "TypeInfoImpl.h"
#include "TagNodeListImpl.h"
#include "NamedAttrMapImpl.h"
#include "MutationEventImpl.h"
#include "EventListenerImpl.h"
#include "CSSStyleSheetImpl.h"
#include "DOMImplementationImpl.h"
#include "CSSStyleDeclarationImpl.h"
#include "RenderStyle.h"

using namespace KDOM;
	
ElementImpl::ElementImpl(DocumentPtr *doc) : NodeBaseImpl(doc), m_attributes(0)
{
	m_styleDeclarations = 0;
	m_prefix = 0;

	m_restyleLate = false;
	m_restyleSelfLate = false;
	m_restyleChildrenLate = false;
}

ElementImpl::ElementImpl(DocumentPtr *doc, DOMStringImpl *prefix, bool nullNSSpecified) : NodeBaseImpl(doc), m_attributes(0)
{
	m_dom2 = true;
	m_restyleLate = false;
	m_restyleSelfLate = false;
	m_restyleChildrenLate = false;
	
	m_styleDeclarations = 0;

	m_prefix = prefix;
	if(m_prefix)
		m_prefix->ref();
		
	m_nullNSSpecified = nullNSSpecified;
}

ElementImpl::~ElementImpl()
{
	if(m_attributes)
	{
		m_attributes->detachFromElement();
		m_attributes->deref();
	}

	if(m_styleDeclarations)
	{
		m_styleDeclarations->setNode(0);
		m_styleDeclarations->setParent(0);
		m_styleDeclarations->deref();
	}

	if(m_prefix)
		m_prefix->deref();
}

DOMStringImpl *ElementImpl::nodeName() const
{
	return tagName();
}

unsigned short ElementImpl::nodeType() const
{
	return ELEMENT_NODE;
}

DOMStringImpl *ElementImpl::prefix() const
{
	return m_prefix;
}

void ElementImpl::setPrefix(DOMStringImpl *prefix)
{
	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	if(isReadOnly())
		throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

	Helper::CheckPrefix(prefix, nodeName(), namespaceURI());
	KDOM_SAFE_SET(m_prefix, prefix);
}

bool ElementImpl::hasAttributes() const
{
	return attributes()->length() > 0;
}

bool ElementImpl::hasAttribute(DOMStringImpl *name) const
{
	if(!attributes())
		return false;

	NodeImpl::Id id = ownerDocument()->getId(NodeImpl::AttributeId, name, true);
	if(!id)
		return (attributes()->lookupAttribute(name) != 0);

	return attributes()->getValue(id, false, name) != 0;
}

bool ElementImpl::hasAttributeNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName) const
{
	if(!attributes())
		return false;

	NodeImpl::Id id = ownerDocument()->getId(NodeImpl::AttributeId, namespaceURI, 0, localName, true);
	if(!id)
		return (attributes()->lookupAttribute(localName) != 0);

	return attributes()->getValue(id, true) != 0;
}

NamedAttrMapImpl *ElementImpl::attributes(bool readonly) const
{
	if(!readonly && !m_attributes)
	{
		m_attributes = new NamedAttrMapImpl(const_cast<ElementImpl*>(this));
		m_attributes->ref();
	}

	return m_attributes;
}

bool ElementImpl::childAllowed(NodeImpl *newChild)
{
	if(!childTypeAllowed(newChild->nodeType()))
		return false;

	// ### check xml element allowedness according to DTD

	// If either this node or the other node is an XML element node,
	// allow regardless (we don't do DTD checks for XML yet)
	if(nodeType() == ELEMENT_NODE || newChild->nodeType() == ELEMENT_NODE)
		return true;

	return checkChild(id(), newChild->id());
}

bool ElementImpl::childTypeAllowed(unsigned short type) const
{
	switch(type)
	{
		case ELEMENT_NODE:
		case TEXT_NODE:
		case COMMENT_NODE:
		case PROCESSING_INSTRUCTION_NODE:
		case CDATA_SECTION_NODE:
		case ENTITY_REFERENCE_NODE:
			return true;
		default:
			return false;
	}
}

DOMStringImpl *ElementImpl::getAttribute(NodeImpl::Id id, bool nsAware, DOMStringImpl *qName) const
{
	if(m_attributes)
	{
		DOMStringImpl *value = m_attributes->getValue(id, nsAware, qName);
		if(value)
			return value;
	}

	return new DOMStringImpl(""); // Spec says empty, not null string
}

DOMStringImpl *ElementImpl::getAttribute(DOMStringImpl *name) const
{
	if(!name)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	NodeImpl::Id id = ownerDocument()->getId(NodeImpl::AttributeId, name, true);
	if(!id)
	{
		AttrImpl *attr = attributes()->lookupAttribute(name);
		if(attr)
			return attr->value();

		return 0;
	}

	return getAttribute(id, false, name);
}

DOMStringImpl *ElementImpl::getAttributeNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName) const
{
	if(!localName)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	NodeImpl::Id id = ownerDocument()->getId(NodeImpl::AttributeId, namespaceURI, 0, localName, true);
	if(!id)
	{
		AttrImpl *attr = attributes()->lookupAttribute(localName);
		if(attr)
			return attr->value();

		return 0;
	}

	return getAttribute(id, true);
}

void ElementImpl::setAttribute(NodeImpl::Id id, DOMStringImpl *value, DOMStringImpl *qName)
{
	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	if(isReadOnly())
		throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

	// INVALID_CHARACTER_ERR: Raised if the specified name contains an illegal character.
	if(!Helper::ValidateAttributeName(qName))
		throw new DOMExceptionImpl(INVALID_CHARACTER_ERR);

	attributes()->setValue(id, value, qName);
}

void ElementImpl::setAttribute(DOMStringImpl *name, DOMStringImpl *value)
{
	NodeImpl::Id id = ownerDocument()->getId(NodeImpl::AttributeId, name, false /* allocate */);
	setAttribute(id, value, name);
}

void ElementImpl::removeAttribute(DOMStringImpl *name)
{
	NodeImpl *ret =attributes()->removeNamedItem(name);
	if(ret)
		ret->deref();
}

void ElementImpl::removeAttributeNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName)
{
	NodeImpl *ret = attributes()->removeNamedItemNS(namespaceURI, localName);
	if(ret)
		ret->deref();
}

NodeListImpl *ElementImpl::getElementsByTagName(DOMStringImpl *name) const
{
	return new TagNodeListImpl(const_cast<ElementImpl *>(this), name);
}

void ElementImpl::setAttributeNS(DOMStringImpl *namespaceURI, DOMStringImpl *qualifiedName, DOMStringImpl *value) 
{
	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	if(isReadOnly())
		throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
 
 	if(namespaceURI)
		namespaceURI->ref();
	if(qualifiedName)
		qualifiedName->ref();

	int colonPos = 0;
	Helper::CheckQualifiedName(qualifiedName, namespaceURI, colonPos,
							   false /* nameCanBeNull */, false /* nameCanBeEmpty */);

	DOMStringImpl *prefix = 0, *localName = 0;
	Helper::SplitPrefixLocalName(qualifiedName, prefix, localName, colonPos);

	if(prefix)
		prefix->ref();
	if(localName)
		localName->ref();

	// INVALID_CHARACTER_ERR: Raised if the specified name contains an illegal character.
	if(!Helper::ValidateAttributeName(localName))
		throw new DOMExceptionImpl(INVALID_CHARACTER_ERR);

	NodeImpl::Id id = ownerDocument()->getId(AttributeId, namespaceURI, prefix, localName, false);
	attributes()->setValue(id, value, 0, prefix, true /* nsAware */, (namespaceURI && !namespaceURI->isEmpty()) /* hasNS */);

	if(prefix)
		prefix->deref();
	if(localName)
		localName->deref();
	if(namespaceURI)
		namespaceURI->deref();
	if(qualifiedName)
		qualifiedName->deref();
}

AttrImpl *ElementImpl::getAttributeNode(DOMStringImpl *name) const
{
	if(!name)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	if(!attributes())
		return 0;

	NodeImpl::Id id = ownerDocument()->getId(NodeImpl::AttributeId, name, true);
	if(!id)
		return attributes()->lookupAttribute(name);

	return static_cast<AttrImpl *>(attributes()->getNamedItem(id, false, name));
}

AttrImpl *ElementImpl::setAttributeNode(AttrImpl *newAttr)
{
	if(!newAttr)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	AttrImpl *attr = static_cast<AttrImpl *>(attributes()->setNamedItem(newAttr));
	newAttr->setOwnerElement(this);
	return attr;
}

AttrImpl *ElementImpl::removeAttributeNode(AttrImpl *oldAttr)
{
	if(!oldAttr || oldAttr->ownerElement() != this)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	if(isReadOnly())
		throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

	if(!attributes())
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return attributes()->removeAttr(oldAttr);
}

AttrImpl *ElementImpl::getAttributeNodeNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName) const
{
	if(!localName)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	if(!attributes())
		return 0;

	NodeImpl::Id id = ownerDocument()->getId(NodeImpl::AttributeId, namespaceURI, 0, localName, true);
	if(!id)
		return attributes()->lookupAttribute(localName);

	return static_cast<AttrImpl *>(attributes()->getNamedItem(id, true));
}

AttrImpl *ElementImpl::setAttributeNodeNS(AttrImpl *newAttr)
{
	if(!newAttr)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	AttrImpl *attr = static_cast<AttrImpl *>(attributes()->setNamedItemNS(newAttr));
	newAttr->setOwnerElement(this);
	return attr;
}

NodeListImpl *ElementImpl::getElementsByTagNameNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName) const
{
	return new TagNodeListImpl(const_cast<ElementImpl *>(this), localName, namespaceURI);
}

NodeImpl *ElementImpl::cloneNode(bool deep, DocumentPtr *doc) const
{
	ElementImpl *clone = 0;
	if(m_dom2)
		clone = doc->document()->createElementNS(namespaceURI(), nodeName());
	else
		clone = doc->document()->createElement(nodeName());

	clone->attributes()->clone(attributes());

	if(m_styleDeclarations)
		*(clone->styleRules()) = *m_styleDeclarations;

	if(deep)
		cloneChildNodes(clone, doc);

	return clone;
}

AttrImpl *ElementImpl::getIdAttribute(DOMStringImpl *name) const
{
	DOMString _name(name);

	unsigned long len = attributes()->length();
	for(unsigned long i = 0; i < len; i++)
	{
		AttrImpl *attr = static_cast<AttrImpl *>(attributes()->item(i));
		DOMStringImpl *val = attr->value();
		if(val)
			val->ref();

		if(attr && attr->isId() && DOMString(val) == _name)
		{
			if(val)
				val->deref();

			return attr;
		}

		if(val)
			val->deref();
	}

	return 0;
}

DOMStringImpl *ElementImpl::namespaceURI() const
{
	if(m_nullNSSpecified || !m_dom2 || !ownerDocument())
		return 0;

	return ownerDocument()->getName(NodeImpl::NamespaceId, id() >> 16);
}

void ElementImpl::setIdAttribute(DOMStringImpl *name, bool isId)
{
	NodeImpl::Id id = ownerDocument()->getId(NodeImpl::AttributeId, name, true);
	
	// NOT_FOUND_ERR: Raised if there is no node named name in this map.
	if(!id)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	AttrImpl *attr = static_cast<AttrImpl *>(attributes()->getNamedItem(id, false, name));

	// NOT_FOUND_ERR: Raised if there is no node named name in this map.
	if(!attr)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	attr->setIsId(isId);
}

void ElementImpl::setIdAttributeNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName, bool isId)
{
	// NOT_FOUND_ERR: Raised if there is no node named name in this map.
	NodeImpl::Id id = ownerDocument()->getId(AttributeId, namespaceURI, 0, localName, false);
	if(!id)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	AttrImpl *attr = static_cast<AttrImpl *>(attributes()->getNamedItem(id, true));

	// NOT_FOUND_ERR: Raised if there is no node named name in this map.
	if(!attr)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	attr->setIsId(isId);
}

void ElementImpl::setIdAttributeNode(AttrImpl *idAttr, bool isId)
{
	if(!idAttr)
		return;

	if((!idAttr->localName() || idAttr->localName()->isEmpty()))
	{
		DOMStringImpl *_name = (idAttr->name() ? idAttr->name()->copy() : 0);
		setIdAttribute(_name, isId);
	}
	else
	{
		DOMStringImpl *_localName = (idAttr->localName() ? idAttr->localName()->copy() : 0);
		DOMStringImpl *_namespaceURI = (idAttr->namespaceURI() ? idAttr->namespaceURI()->copy() : 0);

		setIdAttributeNS(_namespaceURI, _localName, isId);
	}
}

void ElementImpl::parseAttribute(AttributeImpl *attr)
{
	int id = (attr->id() & NodeImpl_IdLocalMask);
	DOMString value(attr->value());
	switch(id)
	{
		case ATTR_ID:
		{
			AttrImpl *attrId = attr->createAttr(this);
			attrId->setIsId(true);
			break;
		}
		case ATTR_ONFOCUSIN:
		case ATTR_ONFOCUSOUT:
		case ATTR_ONACTIVATE:
		case ATTR_ONCLICK:
		case ATTR_ONMOUSEDOWN:
		case ATTR_ONMOUSEMOVE:
		case ATTR_ONMOUSEOUT:
		case ATTR_ONMOUSEOVER:
		case ATTR_ONMOUSEUP:
		case ATTR_ONKEYDOWN:
		case ATTR_ONKEYUP:
		{
//			Ecma *ecmaEngine = (ownerDocument() ? ownerDocument()->ecmaEngine() : 0);
//			addDOMEventListener(ecmaEngine, DOMImplementationImpl::self()->idToType(id - 2), value);
			break;
		}
		default:
			break;
	}
}

void ElementImpl::parseAttribute(Id attrId, DOMStringImpl *value)
{
	AttributeImpl aimpl;
	aimpl.m_attrId = attrId;
	aimpl.m_data.value = value;
	parseAttribute(&aimpl);
}

CSSStyleDeclarationImpl *ElementImpl::styleRules() const
{
	if(!m_styleDeclarations)
		createStyleDeclaration();

	return m_styleDeclarations;
}

void ElementImpl::createStyleDeclaration() const
{
	m_styleDeclarations = new CSSStyleDeclarationImpl(ownerDocument()->implementation()->cdfInterface(), 0);
	m_styleDeclarations->ref();

	m_styleDeclarations->setNode(const_cast<ElementImpl *>(this));
	m_styleDeclarations->setParent(ownerDocument()->elementSheet());
}
/*
void ElementImpl::addDOMEventListener(Ecma *ecmaEngine, DOMStringImpl *type, DOMStringImpl *value)
{
	if(!ecmaEngine)
		return;

	EventListenerImpl *listener = ecmaEngine->createEventListener(type, value);
	if(listener)
	{
		listener->ref();
		addEventListener(type, listener, false);
	}
}
*/
bool ElementImpl::hasListenerType(int eventId) const
{
	if(EventTargetImpl::hasListenerType(eventId))
		return true;

	// Ask parent node...
	ElementImpl *parent = static_cast<ElementImpl *>(parentNode());
	if(parent)
	{
		if(parent->hasListenerType(eventId))
			return true;
	}
	
	return false;
}

TypeInfoImpl *ElementImpl::schemaTypeInfo() const
{
	return 0;
}

void ElementImpl::setAttributeMap(NamedAttrMapImpl *list)
{
	if(m_attributes)
	{
		m_attributes->detachFromElement();
		m_attributes->deref();
	}

	m_attributes = list;

	if(m_attributes)
	{
		m_attributes->ref();
		
		Q_ASSERT(m_attributes->m_ownerElement == 0);
		m_attributes->setElement(this);
		
		unsigned long len = m_attributes->length();
		for(unsigned long i = 0; i < len; i++)
			parseAttribute(&m_attributes->m_attrs[i]);
	}
}

// vim:ts=4:noet
