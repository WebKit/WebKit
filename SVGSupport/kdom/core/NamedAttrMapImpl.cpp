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

#include "config.h"
#include <stdlib.h>

#include <kdebug.h>

#include <kdom/Helper.h>
#include "AttrImpl.h"
#include "NodeImpl.h"
#include "kdomevents.h"
#include "ElementImpl.h"
#include "DocumentImpl.h"
#include "NamedAttrMapImpl.h"
#include "MutationEventImpl.h"

using namespace KDOM;

NamedAttrMapImpl::NamedAttrMapImpl(ElementImpl *element) : NamedNodeMapImpl(), m_ownerElement(element), m_attrs(0), m_attrCount(0)
{
}

NamedAttrMapImpl::~NamedAttrMapImpl()
{
    for(unsigned long i = 0; i < m_attrCount; i++)
        m_attrs[i].free();

    free(m_attrs);
}

NodeImpl *NamedAttrMapImpl::getNamedItem(NodeImpl::Id id, bool nsAware, DOMStringImpl *qNameImpl)
{
    DOMString qName(qNameImpl);

    if(!m_ownerElement)
        return 0;

    unsigned int mask = nsAware ? ~0L : NodeImpl_IdLocalMask;
    id = (id & mask);

    for(unsigned long i = 0; i < m_attrCount; i++)
    {
        if((m_attrs[i].id() & mask) == id)
        {                      
            // if we are called with a qualified name, filter
            // out NS-aware elements with non-matching name.
            if(qNameImpl && (m_attrs[i].id() & NodeImpl_IdNSMask) &&
                strcasecmp(DOMString(m_attrs[i].name()), qName))
                continue;

            return m_attrs[i].createAttr(m_ownerElement);
        }
    }

    return 0;
}

NodeImpl *NamedAttrMapImpl::removeNamedItem(NodeImpl::Id id, bool nsAware, DOMStringImpl *qName)
{
    if(!m_ownerElement)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
    if(isReadOnly())
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

    unsigned int mask = nsAware ? ~0L : NodeImpl_IdLocalMask;
    id = (id & mask);
    
    for(unsigned long i = 0; i < m_attrCount; i++)
    {
        if((m_attrs[i].id() & mask) == id)
        {
            // if we are called with a qualified name, filter out NS-aware
            // elements with non-matching name.
            if(qName && (m_attrs[i].id() & NodeImpl_IdNSMask) &&
                strcasecmp(DOMString(m_attrs[i].name()), DOMString(qName)))
                continue;

            id = m_attrs[i].id();

            NodeImpl *removed = m_attrs[i].createAttr(m_ownerElement);
            removed->ref();

            m_attrs[i].free();
            memmove(m_attrs + i, m_attrs + i + 1,(m_attrCount - i - 1) * sizeof(AttributeImpl));

            m_attrCount--;
            m_attrs = (AttributeImpl *) realloc(m_attrs, m_attrCount * sizeof(AttributeImpl));

            m_ownerElement->parseAttribute(id, 0);

            dispatchAttrMutationEvent(m_ownerElement, removed->nodeValue(),
                                      0, removed->nodeName(), REMOVAL);

            dispatchSubtreeModifiedEvent();

            return removed;
        }
    }

    // NOT_FOUND_ERR: Raised if there is no node with the specified namespaceURI
    // and localName in this map.
    throw new DOMExceptionImpl(NOT_FOUND_ERR);
    return 0;
}

NodeImpl *NamedAttrMapImpl::setNamedItem(NodeImpl *arg, bool nsAware, DOMStringImpl *qName)
{
    if(!m_ownerElement)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
    if(isReadOnly())
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

    // Raised if an attempt is made to add a node doesn't belong in this NamedNodeMap.
    if(arg->nodeType() != ATTRIBUTE_NODE)
        throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);

    AttrImpl *attr = static_cast<AttrImpl *>(arg);
    Helper::CheckInUse(m_ownerElement, attr);
    Helper::CheckWrongDocument(m_ownerElement->ownerDocument(), attr);

    // Already have this attribute.
    // DOMTS core-1 test "hc_elementreplaceattributewithself" says we should return it.
    if(attr->ownerElement() == m_ownerElement)
        return attr;
 
    unsigned int mask = nsAware ? ~0L : NodeImpl_IdLocalMask;
    NodeImpl::Id id = (attr->id() & mask);

    for(unsigned long i = 0; i < m_attrCount; i++)
    {
        if((m_attrs[i].id() & mask) == id)
        {
            // if we are called with a qualified name, filter out NS-aware
            // elements with non-matching name.
            if(qName && (m_attrs[i].id() & NodeImpl_IdNSMask) &&
                strcasecmp(DOMString(m_attrs[i].name()), DOMString(qName)))
                continue;

            // Attribute exists; replace it
            NodeImpl *replaced = m_attrs[i].createAttr(m_ownerElement);
            replaced->ref();

            m_attrs[i].free();

            m_attrs[i].m_attrId = 0; /* "has implementation" flag */
            m_attrs[i].m_data.attr = attr;
            m_attrs[i].m_data.attr->ref();

            attr->setOwnerElement(m_ownerElement);
            m_ownerElement->parseAttribute(&m_attrs[i]);
    
            dispatchAttrMutationEvent(m_ownerElement, replaced->nodeValue(),
                                      attr->nodeValue(), attr->name(), MODIFICATION);

            dispatchSubtreeModifiedEvent();

            return replaced;
        }
    }

    // No existing attribute; add to list
    m_attrCount++;
    m_attrs = (AttributeImpl *) realloc(m_attrs, m_attrCount * sizeof(AttributeImpl));
    m_attrs[m_attrCount - 1].m_attrId = 0; /* "has implementation" flag */
    m_attrs[m_attrCount - 1].m_data.attr = attr;
    m_attrs[m_attrCount - 1].m_data.attr->ref();
    attr->setOwnerElement(m_ownerElement);

    m_ownerElement->parseAttribute(&m_attrs[m_attrCount - 1]);
        
    dispatchAttrMutationEvent(m_ownerElement, 0, attr->nodeValue(), attr->name(), MODIFICATION);
    dispatchSubtreeModifiedEvent();

    return 0;
}

NodeImpl *NamedAttrMapImpl::item(unsigned long index) const
{
    if(!m_ownerElement)
        return 0;

    if(index >= m_attrCount)
        return 0;

    return m_attrs[index].createAttr(m_ownerElement);
}

unsigned long NamedAttrMapImpl::length() const
{
    if(!m_ownerElement)
        return 0;

    return m_attrCount;
}

bool NamedAttrMapImpl::isReadOnly() const
{
    if(m_ownerElement)
        return m_ownerElement->isReadOnly();

    return false;
}

NodeImpl::Id NamedAttrMapImpl::mapId(DOMStringImpl *namespaceURI, DOMStringImpl *localName, bool readonly)
{
    if(!m_ownerElement)
        return 0;

    return m_ownerElement->ownerDocument()->getId(NodeImpl::AttributeId, namespaceURI, 0, localName, readonly);
}

DOMStringImpl *NamedAttrMapImpl::getValue(NodeImpl::Id id, bool nsAware, DOMStringImpl *qName) const
{
    unsigned int mask = nsAware ? ~0L : NodeImpl_IdLocalMask;
    id = (id & mask);

    for(unsigned long i = 0; i < m_attrCount; i++)
    {
        if((m_attrs[i].id() & mask) == id)
        {
            // if we are called with a qualified name, filter out
            // NS-aware elements with non-matching name.
            if(qName && (m_attrs[i].id() & NodeImpl_IdNSMask) &&
                strcasecmp(DOMString(m_attrs[i].name()), DOMString(qName)))
                continue;

            return m_attrs[i].value();
        }
    }

    return 0;
}

void NamedAttrMapImpl::setValue(NodeImpl::Id id, DOMStringImpl *value, DOMStringImpl *qNameImpl, DOMStringImpl *prefix, bool nsAware, bool hasNS)
{
    DOMString qName(qNameImpl);

    Q_ASSERT(!(qNameImpl && nsAware));
    if(!id)
        return;

    // Passing in a null value here causes the attribute to be removed. This is a khtml extension
    // (the spec does not specify what to do in this situation).
    if(!value)
    {
        removeNamedItem(id, nsAware, qNameImpl);
        return;
    }

    unsigned int mask = nsAware ? ~0L : NodeImpl_IdLocalMask;
    NodeImpl::Id mid = (id & mask);

    // Check for an existing attribute.
    for(unsigned long i = 0; i < m_attrCount; i++)
    {
        if((m_attrs[i].id() & mask) == mid)
        {
            // if we are called with a qualified name, filter out
            // NS-aware elements with non-matching name.
            if(qNameImpl && (m_attrs[i].id() & NodeImpl_IdNSMask) &&
                strcasecmp(DOMString(m_attrs[i].name()), qName))
                continue;

            if(prefix)
                m_attrs[i].attr()->setPrefix(prefix);

            DOMStringImpl *prevValue = m_attrs[i].value(); 
            m_attrs[i].setValue(value->copy(), m_ownerElement);
 
            dispatchAttrMutationEvent(m_ownerElement, prevValue, value, m_attrs[i].name(), MODIFICATION);
            dispatchSubtreeModifiedEvent();

            return;
        }
    }

    // No existing matching attribute; add a new one
    m_attrCount++;
    m_attrs = (AttributeImpl *) realloc(m_attrs, m_attrCount * sizeof(AttributeImpl));
    if(!nsAware)
    {
        // Called from setAttribute()... we only have a name
        m_attrs[m_attrCount - 1].m_attrId = id;
        m_attrs[m_attrCount - 1].m_data.value = value;
        m_attrs[m_attrCount - 1].m_data.value->ref();
    }
    else
    {
        // Called from setAttributeNS()... need to create a full AttrImpl here
        if(!m_ownerElement)
            return;

        m_attrs[m_attrCount - 1].m_data.attr = new AttrImpl(m_ownerElement->docPtr(), id, value, prefix, !hasNS);
        m_attrs[m_attrCount - 1].m_attrId = 0; /* "has implementation" flag */
        m_attrs[m_attrCount - 1].m_data.attr->ref();
        m_attrs[m_attrCount - 1].m_data.attr->setOwnerElement(m_ownerElement);
    }

    if(m_ownerElement)
    {
        m_ownerElement->parseAttribute(&m_attrs[m_attrCount - 1]);
 
        dispatchAttrMutationEvent(m_ownerElement, 0, value, m_attrs[m_attrCount - 1].name(), ADDITION);
        dispatchSubtreeModifiedEvent();
    }
}

AttrImpl *NamedAttrMapImpl::removeAttr(AttrImpl *attr)
{
    for(unsigned long i = 0; i < m_attrCount; i++)
    {
        if(m_attrs[i].attr() == attr)
        {
            NodeImpl::Id id = m_attrs[i].id();

            AttrImpl *removed = m_attrs[i].createAttr(m_ownerElement);
            removed->ref();

            m_attrs[i].free();

            memmove(m_attrs + i,m_attrs + i + 1, (m_attrCount - i - 1) * sizeof(AttributeImpl));
            m_attrCount--;

            m_attrs = (AttributeImpl *) realloc(m_attrs, m_attrCount * sizeof(AttributeImpl));
            m_ownerElement->parseAttribute(id, 0);

            dispatchAttrMutationEvent(m_ownerElement, removed->nodeValue(), 0, removed->nodeName(), REMOVAL);
            dispatchSubtreeModifiedEvent();
 
            return removed;
        }
    }

    return 0;
}

void NamedAttrMapImpl::clone(NamedNodeMapImpl *_other)
{
    NamedAttrMapImpl *other = static_cast<NamedAttrMapImpl *>(_other);
    if(!other)
        return;

    unsigned long i;
    for(i = 0; i < m_attrCount; i++)
        m_attrs[i].free();

    m_attrCount = other->m_attrCount;
    m_attrs = (AttributeImpl *) realloc(m_attrs, m_attrCount * sizeof(AttributeImpl));

    for(i = 0; i < m_attrCount; i++)
    {
        m_attrs[i].m_attrId = other->m_attrs[i].m_attrId;

        if(m_attrs[i].m_attrId)
        {
            m_attrs[i].m_data.value = other->m_attrs[i].m_data.value;
            m_attrs[i].m_data.value->ref();
        }
        else
        {
            m_attrs[i].m_data.attr = static_cast<AttrImpl *>(other->m_attrs[i].m_data.attr->cloneNode(true, m_ownerElement->docPtr()));
            m_attrs[i].m_data.attr->ref();
            m_attrs[i].m_data.attr->setOwnerElement(m_ownerElement);
        }

        m_ownerElement->parseAttribute(&m_attrs[i]);
    }
}

NodeImpl *NamedAttrMapImpl::getNamedItem(DOMStringImpl *name)
{
    NodeImpl::Id nid = mapId(0, name, true);
    if(!nid)
        return lookupAttribute(name);

    return getNamedItem(nid, false, name);
}

NodeImpl *NamedAttrMapImpl::setNamedItem(NodeImpl *arg)
{
    if(!arg)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    return setNamedItem(arg, false, arg->nodeName());
}

NodeImpl *NamedAttrMapImpl::removeNamedItem(DOMStringImpl *name)
{
    return removeNamedItem(mapId(0, name, false), false, name);
}

NodeImpl *NamedAttrMapImpl::getNamedItemNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName)
{
    NodeImpl::Id nid = mapId(namespaceURI, localName, true);
    return getNamedItem(nid, true);
}

NodeImpl *NamedAttrMapImpl::setNamedItemNS(NodeImpl *arg)
{
    if(!arg)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    return setNamedItem(arg, true, 0);
}

NodeImpl *NamedAttrMapImpl::removeNamedItemNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName)
{
    NodeImpl::Id nid = mapId(namespaceURI, localName, false);
    return removeNamedItem(nid, true, 0);
}

void NamedAttrMapImpl::dispatchAttrMutationEvent(NodeImpl *node, DOMStringImpl *prevValue, DOMStringImpl *newValue, DOMStringImpl *name, unsigned short attrChange)
{
    DocumentImpl *document = (m_ownerElement ? m_ownerElement->ownerDocument() : 0);
    if(!document || !document->hasListenerType(DOMATTRMODIFIED_EVENT))
    {
        if(name) name->deref();
        return;
    }

    DOMStringImpl *eventType = new DOMStringImpl("MutationEvents");
    eventType->ref();

    MutationEventImpl *event = static_cast<MutationEventImpl *>(document->createEvent(eventType));
    event->ref();

    event->initMutationEvent(new DOMStringImpl("DOMAttrModified"), true, false,
                             node, prevValue, newValue, name, attrChange);

    m_ownerElement->dispatchEvent(event);

    event->deref();
    eventType->deref();
}

void NamedAttrMapImpl::dispatchSubtreeModifiedEvent()
{
    DocumentImpl *document = (m_ownerElement ? m_ownerElement->ownerDocument() : 0);
    if(!document || !document->hasListenerType(DOMSUBTREEMODIFIED_EVENT))
        return;

    DOMStringImpl *eventType = new DOMStringImpl("MutationEvents");
    eventType->ref();

    MutationEventImpl *event = static_cast<MutationEventImpl *>(document->createEvent(eventType));
    event->ref();

    event->initMutationEvent(new DOMStringImpl("DOMSubtreeModified"), true, false, 0, 0, 0, 0, 0);
    m_ownerElement->dispatchEvent(event);

    event->deref();
    eventType->deref();
}

NodeImpl::Id NamedAttrMapImpl::idAt(unsigned long index) const
{
    Q_ASSERT(index <= m_attrCount);
    return m_attrs[index].id();
}

DOMStringImpl *NamedAttrMapImpl::valueAt(unsigned long index) const
{
    Q_ASSERT(index <= m_attrCount);
    return m_attrs[index].value();
}

AttrImpl *NamedAttrMapImpl::lookupAttribute(DOMStringImpl *name) const
{
    DOMString _name(name);

    // reverting to simpler and slower algorithm
    for(unsigned long i = 0; i < length(); ++i)
    {
        AttrImpl *attr = static_cast<AttrImpl *>(item(i));
        if(attr && DOMString(attr->nodeName()) == _name)
            return attr;
    }

    return 0;
}

int NamedAttrMapImpl::index(NodeImpl *_item) const
{
    int len = length();
    for(int i = 0;i < len;++i)
    {
        AttrImpl *attr = static_cast<AttrImpl *>(item(i));
        if(attr == _item)
            return i;
    }

    return -1;
}

void NamedAttrMapImpl::detachFromElement()
{
    // This makes the map invalid; nothing can really be done with it now since it's not
    // associated with an element. But we have to keep it around in memory in case there
    // are still references to it.
    m_ownerElement = 0;

    for(unsigned long i = 0; i < m_attrCount; i++)
        m_attrs[i].free();
    
    free(m_attrs);
    
    m_attrs = 0;
    m_attrCount = 0;
}

void NamedAttrMapImpl::setElement(ElementImpl *element)
{
    Q_ASSERT(!m_ownerElement);
    m_ownerElement = element;

    for(unsigned long i = 0; i < m_attrCount; i++)
    {
        if(m_attrs[i].attr())
            m_attrs[i].attr()->setOwnerElement(element);
    }
}

// vim:ts=4:noet
