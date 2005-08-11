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

#include <qstring.h>
#include "kdom/Helper.h"
#include "NodeImpl.h"
#include "AttrImpl.h"
#include "TextImpl.h"
#include "TypeInfoImpl.h"
#include "DOMString.h"
#include "ElementImpl.h"
#include "DocumentImpl.h"
#include "DOMStringImpl.h"
#include "MutationEventImpl.h"

using namespace KDOM;

AttrImpl::AttrImpl(DocumentImpl *doc, NodeImpl::Id id) : NodeBaseImpl(doc), m_ownerElement(0), m_prefix(0), m_value(0), m_id(id)
{
	m_isId = false;
	m_specified = true;
}

AttrImpl::AttrImpl(DocumentImpl *doc, NodeImpl::Id id, DOMStringImpl *prefix, bool nullNSSpecified) : NodeBaseImpl(doc), m_ownerElement(0), m_value(0), m_id(id)
{
	m_prefix = prefix;
	if(m_prefix)
		m_prefix->ref();

	m_dom2 = true;
	m_isId = false;
	m_specified = true;
	m_nullNSSpecified = nullNSSpecified;
}

AttrImpl::AttrImpl(DocumentImpl *doc, NodeImpl::Id id, DOMStringImpl *value, DOMStringImpl *prefix, bool nullNSSpecified) : NodeBaseImpl(doc), m_ownerElement(0), m_id(id)
{
	m_prefix = prefix;
	if(m_prefix)
		m_prefix->ref();

	m_value = value;
	if(m_value)
		m_value->ref();

	m_dom2 = true;
	m_isId = false;
	m_specified = true;
	m_nullNSSpecified = nullNSSpecified;

	if(value)
		appendChild(doc->createTextNode(DOMString(value)));
}

AttrImpl::~AttrImpl()
{
	if(m_value)
		m_value->deref();
	if(m_prefix)
		m_prefix->deref();
}

bool AttrImpl::specified() const
{
	return m_specified;
}

bool AttrImpl::isId() const
{
	return m_isId;
}

void AttrImpl::setIsId(bool isId)
{
	m_isId = isId;
}

DOMString AttrImpl::nodeName() const
{
	return name();
}
	
DOMString AttrImpl::localName() const
{
	if(!m_dom2)
		return DOMString();

	return ownerDocument()->getName(NodeImpl::AttributeId, m_id);
}

DOMString AttrImpl::nodeValue() const
{
	return value();
}

void AttrImpl::setNodeValue(const DOMString &nodeValue)
{
	setValue(nodeValue);
}

unsigned short AttrImpl::nodeType() const
{
	return ATTRIBUTE_NODE;
}

DOMString AttrImpl::value() const
{
	DOMString result("");
	
	for(NodeImpl *child = firstChild(); child != 0; child = child->nextSibling())
		result += child->textContent();

	return result;
}

DOMStringImpl *AttrImpl::val()
{
	if(!m_value)
	{
		DOMString result = value();
		
		m_value = new DOMStringImpl(result.unicode(), result.length());
		m_value->ref();
	}

	return m_value;
}

DOMString AttrImpl::name() const
{
	DOMString ret = ownerDocument()->getName(NodeImpl::AttributeId, m_id);

	if(m_prefix && m_prefix->length())
		return DOMString(m_prefix) + ":" + ret;

	return ret;
}

DOMString AttrImpl::namespaceURI() const
{
	if(m_nullNSSpecified || !m_dom2 || !ownerDocument())
		return DOMString();

	return ownerDocument()->getName(NodeImpl::NamespaceId, m_id >> 16);
}

DOMString AttrImpl::prefix() const
{
	return DOMString(m_prefix);
}

void AttrImpl::setPrefix(const DOMString &_prefix)
{
	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	if(isReadOnly())
		throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

	Helper::CheckPrefix(_prefix, localName(), namespaceURI());
	
	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is an attribute and the qualifiedName of this node is "xmlns"
	if(nodeName() == "xmlns")
		throw new DOMExceptionImpl(NAMESPACE_ERR);

	if(m_prefix == _prefix.implementation())
		return;

	if(m_prefix)
		m_prefix->deref();
	
	m_prefix = _prefix.implementation();
	
	if(m_prefix)
		m_prefix->ref();
}

bool AttrImpl::isReadOnly() const
{
	if(!m_ownerElement)
		return false;

	return m_ownerElement->isReadOnly();
}

bool AttrImpl::childAllowed(NodeImpl *newChild)
{
	if(!newChild)
		return false;

	return childTypeAllowed(newChild->nodeType());
}

bool AttrImpl::childTypeAllowed(unsigned short type) const
{
	switch(type)
	{
		case TEXT_NODE:
		case ENTITY_REFERENCE_NODE:
			return true;
		default:
			return false;
	}
}

NodeImpl *AttrImpl::cloneNode(bool, DocumentImpl *doc) const
{
	AttrImpl *clone = 0;
	if(m_dom2)
		clone = doc->createAttributeNS(namespaceURI(), nodeName());
	else
		clone = doc->createAttribute(nodeName());

	// Cloning an Attr always clones its children, since they
	// represent its value,
	cloneChildNodes(clone, doc);
	return clone;
}

void AttrImpl::setValue(const DOMString &_value)
{
	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	if(isReadOnly())
		throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

//	DOMString prevValue = value(); TODO: Mutation events?
	removeChildren();

	addChild(ownerDocument()->createTextNode(_value));

	if(m_value == _value.implementation())
		return;
	
	if(m_value)
		m_value->deref();
	
	m_value = _value.implementation();
	
	if(m_value)
		m_value->ref();

	// is attr owned?
	if(m_ownerElement)
		m_ownerElement->parseAttribute(m_id, m_value);
}

ElementImpl *AttrImpl::ownerElement() const
{
	return m_ownerElement;
}

void AttrImpl::setOwnerElement(ElementImpl *impl)
{
	m_ownerElement = impl;
}

void AttrImpl::setSpecified(bool specified)
{
	m_specified = specified;
}

TypeInfoImpl *AttrImpl::schemaTypeInfo() const
{
	return 0;
}

void AttributeImpl::setValue(DOMStringImpl *value, ElementImpl *element)
{
	Q_ASSERT(value != 0);
	if(m_attrId)
	{
		if(m_data.value == value)
			return;

		m_data.value->deref();
		m_data.value = value;
		m_data.value->ref();

		if(element)
			element->parseAttribute(this);
	}
	else
		m_data.attr->setValue(DOMString(value));
}

AttrImpl *AttributeImpl::createAttr(ElementImpl *element)
{
	if(m_attrId)
	{
		AttrImpl *attr = new AttrImpl(element->ownerDocument(), m_attrId);
		if(!attr)
			return 0;

		attr->setValue(DOMString(m_data.value));
		attr->setOwnerElement(element);	

		m_data.value->deref();
		m_data.attr = attr;
		m_data.attr->ref();
		m_attrId = 0; /* "has implementation" flag */
	}

	return m_data.attr;
}

void AttributeImpl::free()
{
	if(m_attrId)
		m_data.value->deref();
	else
	{
		m_data.attr->setOwnerElement(0);
		m_data.attr->deref();
	}
}

// vim:ts=4:noet
