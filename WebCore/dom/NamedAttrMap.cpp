/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2007 Eric Seidel (eric@webkit.org)
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "NamedAttrMap.h"

#include "Document.h"
#include "Element.h"
#include "ExceptionCode.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

static inline bool shouldIgnoreAttributeCase(const Element* e)
{
    return e && e->document()->isHTMLDocument() && e->isHTMLElement();
}

NamedAttrMap::NamedAttrMap(Element *e)
    : element(e)
{
}

NamedAttrMap::~NamedAttrMap()
{
    NamedAttrMap::clearAttributes(); // virtual method, so qualify just to be explicit
}

bool NamedAttrMap::isMappedAttributeMap() const
{
    return false;
}

PassRefPtr<Node> NamedAttrMap::getNamedItem(const String& name) const
{
    String localName = shouldIgnoreAttributeCase(element) ? name.lower() : name;
    Attribute* a = getAttributeItem(localName);
    if (!a)
        return 0;
    
    return a->createAttrIfNeeded(element);
}

PassRefPtr<Node> NamedAttrMap::getNamedItemNS(const String& namespaceURI, const String& localName) const
{
    return getNamedItem(QualifiedName(nullAtom, localName, namespaceURI));
}

PassRefPtr<Node> NamedAttrMap::removeNamedItem(const String& name, ExceptionCode& ec)
{
    String localName = shouldIgnoreAttributeCase(element) ? name.lower() : name;
    Attribute* a = getAttributeItem(localName);
    if (!a) {
        ec = NOT_FOUND_ERR;
        return 0;
    }
    
    return removeNamedItem(a->name(), ec);
}

PassRefPtr<Node> NamedAttrMap::removeNamedItemNS(const String& namespaceURI, const String& localName, ExceptionCode& ec)
{
    return removeNamedItem(QualifiedName(nullAtom, localName, namespaceURI), ec);
}

PassRefPtr<Node> NamedAttrMap::getNamedItem(const QualifiedName& name) const
{
    Attribute* a = getAttributeItem(name);
    if (!a)
        return 0;

    return a->createAttrIfNeeded(element);
}

PassRefPtr<Node> NamedAttrMap::setNamedItem(Node* arg, ExceptionCode& ec)
{
    if (!element) {
        ec = NOT_FOUND_ERR;
        return 0;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this map is readonly.
    if (isReadOnlyNode()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return 0;
    }

    // WRONG_DOCUMENT_ERR: Raised if arg was created from a different document than the one that created this map.
    if (arg->document() != element->document()) {
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    // Not mentioned in spec: throw a HIERARCHY_REQUEST_ERROR if the user passes in a non-attribute node
    if (!arg->isAttributeNode()) {
        ec = HIERARCHY_REQUEST_ERR;
        return 0;
    }
    Attr *attr = static_cast<Attr*>(arg);

    Attribute* a = attr->attr();
    Attribute* old = getAttributeItem(a->name());
    if (old == a)
        return RefPtr<Node>(arg); // we know about it already

    // INUSE_ATTRIBUTE_ERR: Raised if arg is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attr->ownerElement()) {
        ec = INUSE_ATTRIBUTE_ERR;
        return 0;
    }

    if (a->name() == idAttr)
        element->updateId(old ? old->value() : nullAtom, a->value());

    // ### slightly inefficient - resizes attribute array twice.
    RefPtr<Node> r;
    if (old) {
        r = old->createAttrIfNeeded(element);
        removeAttribute(a->name());
    }

    addAttribute(a);
    return r.release();
}

// The DOM2 spec doesn't say that removeAttribute[NS] throws NOT_FOUND_ERR
// if the attribute is not found, but at this level we have to throw NOT_FOUND_ERR
// because of removeNamedItem, removeNamedItemNS, and removeAttributeNode.
PassRefPtr<Node> NamedAttrMap::removeNamedItem(const QualifiedName& name, ExceptionCode& ec)
{
    // ### should this really be raised when the attribute to remove isn't there at all?
    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (isReadOnlyNode()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return 0;
    }

    Attribute* a = getAttributeItem(name);
    if (!a) {
        ec = NOT_FOUND_ERR;
        return 0;
    }

    RefPtr<Node> r = a->createAttrIfNeeded(element);

    if (name == idAttr)
        element->updateId(a->value(), nullAtom);

    removeAttribute(name);
    return r.release();
}

PassRefPtr<Node> NamedAttrMap::item (unsigned index) const
{
    if (index >= length())
        return 0;

    return m_attributes[index]->createAttrIfNeeded(element);
}

Attribute* NamedAttrMap::getAttributeItem(const String& name) const
{
    unsigned len = length();
    for (unsigned i = 0; i < len; ++i) {
        if (!m_attributes[i]->name().hasPrefix() && 
            m_attributes[i]->name().localName() == name)
                return m_attributes[i].get();
        
        if (m_attributes[i]->name().toString() == name)
            return m_attributes[i].get();
    }
    return 0;
}

Attribute* NamedAttrMap::getAttributeItem(const QualifiedName& name) const
{
    unsigned len = length();
    for (unsigned i = 0; i < len; ++i) {
        if (m_attributes[i]->name().matches(name))
            return m_attributes[i].get();
    }
    return 0;
}

void NamedAttrMap::clearAttributes()
{
    unsigned len = length();
    for (unsigned i = 0; i < len; i++)
        if (Attr* attr = m_attributes[i]->attr())
            attr->m_element = 0;

    m_attributes.clear();
}

void NamedAttrMap::detachFromElement()
{
    // we allow a NamedAttrMap w/o an element in case someone still has a reference
    // to if after the element gets deleted - but the map is now invalid
    element = 0;
    clearAttributes();
}

NamedAttrMap& NamedAttrMap::operator=(const NamedAttrMap& other)
{
    // clone all attributes in the other map, but attach to our element
    if (!element)
        return *this;

    // If assigning the map changes the id attribute, we need to call
    // updateId.
    Attribute *oldId = getAttributeItem(idAttr);
    Attribute *newId = other.getAttributeItem(idAttr);

    if (oldId || newId)
        element->updateId(oldId ? oldId->value() : nullAtom, newId ? newId->value() : nullAtom);

    clearAttributes();
    unsigned newLength = other.length();
    m_attributes.resize(newLength);
    for (unsigned i = 0; i < newLength; i++)
        m_attributes[i] = other.m_attributes[i]->clone();

    // FIXME: This is wasteful.  The class list could be preserved on a copy, and we
    // wouldn't have to waste time reparsing the attribute.
    // The derived class, HTMLNamedAttrMap, which manages a parsed class list for the CLASS attribute,
    // will update its member variable when parse attribute is called.
    for(unsigned i = 0; i < newLength; i++)
        element->attributeChanged(m_attributes[i].get(), true);

    return *this;
}

void NamedAttrMap::addAttribute(PassRefPtr<Attribute> prpAttribute)
{
    RefPtr<Attribute> attribute = prpAttribute;
    
    // Add the attribute to the list
    m_attributes.append(attribute);

    if (Attr* attr = attribute->attr())
        attr->m_element = element;

    // Notify the element that the attribute has been added, and dispatch appropriate mutation events
    // Note that element may be null here if we are called from insertAttr() during parsing
    if (element) {
        element->attributeChanged(attribute.get());
        // Because of our updateStyleAttributeIfNeeded() style modification events are never sent at the right time, so don't bother sending them.
        if (attribute->name() != styleAttr) {
            element->dispatchAttrAdditionEvent(attribute.get());
            element->dispatchSubtreeModifiedEvent();
        }
    }
}

void NamedAttrMap::removeAttribute(const QualifiedName& name)
{
    unsigned len = length();
    unsigned index = len + 1;
    for (unsigned i = 0; i < len; ++i)
        if (m_attributes[i]->name().matches(name)) {
            index = i;
            break;
        }

    if (index >= len)
        return;

    // Remove the attribute from the list
    RefPtr<Attribute> attr = m_attributes[index].get();
    if (Attr* a = m_attributes[index]->attr())
        a->m_element = 0;

    m_attributes.remove(index);

    // Notify the element that the attribute has been removed
    // dispatch appropriate mutation events
    if (element && !attr->m_value.isNull()) {
        AtomicString value = attr->m_value;
        attr->m_value = nullAtom;
        element->attributeChanged(attr.get());
        attr->m_value = value;
    }
    if (element) {
        element->dispatchAttrRemovalEvent(attr.get());
        element->dispatchSubtreeModifiedEvent();
    }
}

bool NamedAttrMap::mapsEquivalent(const NamedAttrMap* otherMap) const
{
    if (!otherMap)
        return false;
    
    unsigned len = length();
    if (len != otherMap->length())
        return false;
    
    for (unsigned i = 0; i < len; i++) {
        Attribute *attr = attributeItem(i);
        Attribute *otherAttr = otherMap->getAttributeItem(attr->name());
            
        if (!otherAttr || attr->value() != otherAttr->value())
            return false;
    }
    
    return true;
}

bool NamedAttrMap::isReadOnlyNode()
{
    return element && element->isReadOnlyNode();
}

}
