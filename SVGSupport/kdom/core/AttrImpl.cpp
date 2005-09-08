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

#include <qstring.h>
#include "kdom/Helper.h"
#include "NodeImpl.h"
#include "AttrImpl.h"
#include "TextImpl.h"
#include "ElementImpl.h"
#include "DocumentImpl.h"
#include "TypeInfoImpl.h"
#include "DOMStringImpl.h"
#include "MutationEventImpl.h"

using namespace KDOM;

AttrImpl::AttrImpl(DocumentPtr *doc, NodeImpl::Id id) : NodeBaseImpl(doc), m_ownerElement(0), m_prefix(0), m_value(0), m_id(id)
{
    m_isId = false;
    m_specified = true;
}

AttrImpl::AttrImpl(DocumentPtr *doc, NodeImpl::Id id, DOMStringImpl *prefix, bool nullNSSpecified) : NodeBaseImpl(doc), m_ownerElement(0), m_value(0), m_id(id)
{
    m_prefix = prefix;
    if(m_prefix)
        m_prefix->ref();

    m_dom2 = true;
    m_isId = false;
    m_specified = true;
    m_nullNSSpecified = nullNSSpecified;
}

AttrImpl::AttrImpl(DocumentPtr *doc, NodeImpl::Id id, DOMStringImpl *value, DOMStringImpl *prefix, bool nullNSSpecified) : NodeBaseImpl(doc), m_ownerElement(0), m_id(id)
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
        appendChild(ownerDocument()->createTextNode(value));
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

DOMStringImpl *AttrImpl::nodeName() const
{
    return name();
}
    
DOMStringImpl *AttrImpl::localName() const
{
    if(!m_dom2)
        return 0;

    return ownerDocument()->getName(NodeImpl::AttributeId, m_id);
}

DOMStringImpl *AttrImpl::nodeValue() const
{
    return value();
}

void AttrImpl::setNodeValue(DOMStringImpl *nodeValue)
{
    setValue(nodeValue);
}

unsigned short AttrImpl::nodeType() const
{
    return ATTRIBUTE_NODE;
}

DOMStringImpl *AttrImpl::value() const
{
    DOMStringImpl *ret = new DOMStringImpl("");

    for(NodeImpl *child = firstChild(); child != 0; child = child->nextSibling())
        ret->append(DOMString(child->textContent()).string());

    return ret;
}

DOMStringImpl *AttrImpl::val()
{
    if(!m_value)
    {
        DOMString result(value());

        m_value = new DOMStringImpl(result.unicode(), result.length());
        m_value->ref();
    }

    return m_value;
}

DOMStringImpl *AttrImpl::name() const
{
    DOMStringImpl *ret = ownerDocument()->getName(NodeImpl::AttributeId, m_id);
    if(!ret)
        return 0;

    if(m_prefix && m_prefix->length())
        return new DOMStringImpl(m_prefix->string() + QString::fromLatin1(":") + DOMString(ret).string());

    return ret;
}

DOMStringImpl *AttrImpl::namespaceURI() const
{
    if(m_nullNSSpecified || !m_dom2 || !ownerDocument())
        return 0;

    return ownerDocument()->getName(NodeImpl::NamespaceId, m_id >> 16);
}

DOMStringImpl *AttrImpl::prefix() const
{
    return m_prefix;
}

void AttrImpl::setPrefix(DOMStringImpl *_prefix)
{
    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
    if(isReadOnly())
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

    Helper::CheckPrefix(_prefix, localName(), namespaceURI());

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is an attribute and the qualifiedName of this node is "xmlns"
    if(DOMString(nodeName()) == "xmlns")
        throw new DOMExceptionImpl(NAMESPACE_ERR);

    KDOM_SAFE_SET(m_prefix, _prefix);
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

NodeImpl *AttrImpl::cloneNode(bool, DocumentPtr *doc) const
{
    AttrImpl *clone = 0;
    if(m_dom2)
        clone = doc->document()->createAttributeNS(namespaceURI(), nodeName());
    else
        clone = doc->document()->createAttribute(nodeName());

    // Cloning an Attr always clones its children, since they
    // represent its value,
    cloneChildNodes(clone, doc);
    return clone;
}

void AttrImpl::setValue(DOMStringImpl *_value)
{
    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
    if(isReadOnly())
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

//    DOMStringImpl *prevValue = value(); TODO: Mutation events?
    removeChildren();

    addChild(ownerDocument()->createTextNode(_value));

    KDOM_SAFE_SET(m_value, _value);

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
        m_data.attr->setValue(value);
}

AttrImpl *AttributeImpl::createAttr(ElementImpl *element)
{
    if(m_attrId)
    {
        AttrImpl *attr = new AttrImpl(element->docPtr(), m_attrId);
        if(!attr)
            return 0;

        attr->setValue(m_data.value);
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
