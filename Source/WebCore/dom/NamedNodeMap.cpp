/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "NamedNodeMap.h"

#include "Attr.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "HTMLNames.h"
#include "StyledElement.h"

namespace WebCore {

using namespace HTMLNames;

static inline bool shouldIgnoreAttributeCase(const Element* e)
{
    return e && e->document()->isHTMLDocument() && e->isHTMLElement();
}

inline void NamedNodeMap::detachAttributesFromElement()
{
    size_t size = m_attributes.size();
    for (size_t i = 0; i < size; i++) {
        if (Attr* attr = m_attributes[i]->attr())
            attr->m_element = 0;
    }
}

void NamedNodeMap::ref()
{
    ASSERT(m_element);
    m_element->ref();
}

void NamedNodeMap::deref()
{
    ASSERT(m_element);
    m_element->deref();
}

NamedNodeMap::~NamedNodeMap()
{
    detachAttributesFromElement();
}

PassRefPtr<Node> NamedNodeMap::getNamedItem(const String& name) const
{
    Attribute* a = getAttributeItem(name, shouldIgnoreAttributeCase(m_element));
    if (!a)
        return 0;
    
    return a->createAttrIfNeeded(m_element);
}

PassRefPtr<Node> NamedNodeMap::getNamedItemNS(const String& namespaceURI, const String& localName) const
{
    return getNamedItem(QualifiedName(nullAtom, localName, namespaceURI));
}

PassRefPtr<Node> NamedNodeMap::removeNamedItem(const String& name, ExceptionCode& ec)
{
    Attribute* a = getAttributeItem(name, shouldIgnoreAttributeCase(m_element));
    if (!a) {
        ec = NOT_FOUND_ERR;
        return 0;
    }
    
    return removeNamedItem(a->name(), ec);
}

PassRefPtr<Node> NamedNodeMap::removeNamedItemNS(const String& namespaceURI, const String& localName, ExceptionCode& ec)
{
    return removeNamedItem(QualifiedName(nullAtom, localName, namespaceURI), ec);
}

PassRefPtr<Node> NamedNodeMap::getNamedItem(const QualifiedName& name) const
{
    Attribute* a = getAttributeItem(name);
    if (!a)
        return 0;

    return a->createAttrIfNeeded(m_element);
}

PassRefPtr<Node> NamedNodeMap::setNamedItem(Node* node, ExceptionCode& ec)
{
    if (!m_element || !node) {
        ec = NOT_FOUND_ERR;
        return 0;
    }

    // Not mentioned in spec: throw a HIERARCHY_REQUEST_ERROR if the user passes in a non-attribute node
    if (!node->isAttributeNode()) {
        ec = HIERARCHY_REQUEST_ERR;
        return 0;
    }
    Attr* attr = static_cast<Attr*>(node);

    Attribute* attribute = attr->attr();
    size_t index = getAttributeItemIndex(attribute->name());
    Attribute* oldAttribute = index != notFound ? attributeItem(index) : 0;
    if (oldAttribute == attribute)
        return node; // we know about it already

    // INUSE_ATTRIBUTE_ERR: Raised if node is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attr->ownerElement()) {
        ec = INUSE_ATTRIBUTE_ERR;
        return 0;
    }

    RefPtr<Attr> oldAttr;
    if (oldAttribute) {
        oldAttr = oldAttribute->createAttrIfNeeded(m_element);
        replaceAttribute(index, attribute);
    } else
        addAttribute(attribute);

    return oldAttr.release();
}

PassRefPtr<Node> NamedNodeMap::setNamedItemNS(Node* node, ExceptionCode& ec)
{
    return setNamedItem(node, ec);
}

PassRefPtr<Node> NamedNodeMap::removeNamedItem(const QualifiedName& name, ExceptionCode& ec)
{
    ASSERT(m_element);

    size_t index = getAttributeItemIndex(name);
    if (index == notFound) {
        ec = NOT_FOUND_ERR;
        return 0;
    }

    RefPtr<Attr> attr = m_attributes[index]->createAttrIfNeeded(m_element);

    removeAttribute(index);

    return attr.release();
}

PassRefPtr<Node> NamedNodeMap::item(unsigned index) const
{
    if (index >= length())
        return 0;

    return m_attributes[index]->createAttrIfNeeded(m_element);
}

void NamedNodeMap::copyAttributesToVector(Vector<RefPtr<Attribute> >& copy)
{
    copy = m_attributes;
}

size_t NamedNodeMap::getAttributeItemIndexSlowCase(const String& name, bool shouldIgnoreAttributeCase) const
{
    unsigned len = length();

    // Continue to checking case-insensitively and/or full namespaced names if necessary:
    for (unsigned i = 0; i < len; ++i) {
        const QualifiedName& attrName = m_attributes[i]->name();
        if (!attrName.hasPrefix()) {
            if (shouldIgnoreAttributeCase && equalIgnoringCase(name, attrName.localName()))
                return i;
        } else {
            // FIXME: Would be faster to do this comparison without calling toString, which
            // generates a temporary string by concatenation. But this branch is only reached
            // if the attribute name has a prefix, which is rare in HTML.
            if (equalPossiblyIgnoringCase(name, attrName.toString(), shouldIgnoreAttributeCase))
                return i;
        }
    }
    return notFound;
}

void NamedNodeMap::clearAttributes()
{
    m_classNames.clear();
    detachAttributesFromElement();
    m_attributes.clear();
}

void NamedNodeMap::detachFromElement()
{
    // This can't happen if the holder of the map is JavaScript, because we mark the
    // element if the map is alive. So it has no impact on web page behavior. Because
    // of that, we can simply clear all the attributes to avoid accessing stale
    // pointers to do things like create Attr objects.
    m_element = 0;
    clearAttributes();
}

void NamedNodeMap::setAttributes(const NamedNodeMap& other)
{
    // clone all attributes in the other map, but attach to our element
    if (!m_element)
        return;

    // If assigning the map changes the id attribute, we need to call
    // updateId.
    Attribute* oldId = getAttributeItem(m_element->document()->idAttributeName());
    Attribute* newId = other.getAttributeItem(m_element->document()->idAttributeName());

    if (oldId || newId)
        m_element->updateId(oldId ? oldId->value() : nullAtom, newId ? newId->value() : nullAtom);

    Attribute* oldName = getAttributeItem(HTMLNames::nameAttr);
    Attribute* newName = other.getAttributeItem(HTMLNames::nameAttr);

    if (oldName || newName)
        m_element->updateName(oldName ? oldName->value() : nullAtom, newName ? newName->value() : nullAtom);

    clearAttributes();
    unsigned newLength = other.length();
    m_attributes.resize(newLength);
    for (unsigned i = 0; i < newLength; i++)
        m_attributes[i] = other.m_attributes[i]->clone();

    // FIXME: This is wasteful.  The class list could be preserved on a copy, and we
    // wouldn't have to waste time reparsing the attribute.
    // The derived class, HTMLNamedNodeMap, which manages a parsed class list for the CLASS attribute,
    // will update its member variable when parse attribute is called.
    for (unsigned i = 0; i < newLength; i++)
        m_element->attributeChanged(m_attributes[i].get(), true);
}

void NamedNodeMap::addAttribute(PassRefPtr<Attribute> prpAttribute)
{
    RefPtr<Attribute> attribute = prpAttribute;

    if (m_element)
        m_element->willModifyAttribute(attribute->name(), nullAtom, attribute->value());

    m_attributes.append(attribute);
    if (Attr* attr = attribute->attr())
        attr->m_element = m_element;

    if (m_element)
        m_element->didModifyAttribute(attribute.get());
}

void NamedNodeMap::removeAttribute(size_t index)
{
    ASSERT(index < length());

    RefPtr<Attribute> attribute = m_attributes[index];

    if (m_element)
        m_element->willRemoveAttribute(attribute->name(), attribute->value());

    if (Attr* attr = attribute->attr())
        attr->m_element = 0;
    m_attributes.remove(index);

    if (m_element)
        m_element->didRemoveAttribute(attribute.get());
}

void NamedNodeMap::replaceAttribute(size_t index, PassRefPtr<Attribute> prpAttribute)
{
    ASSERT(index < length());

    RefPtr<Attribute> attribute = prpAttribute;
    Attribute* old = m_attributes[index].get();

    if (m_element)
        m_element->willModifyAttribute(attribute->name(), old->value(), attribute->value());

    if (Attr* attr = old->attr())
        attr->m_element = 0;
    m_attributes[index] = attribute;
    if (Attr* attr = attribute->attr())
        attr->m_element = m_element;

    if (m_element)
        m_element->didModifyAttribute(attribute.get());
}

void NamedNodeMap::setClass(const String& classStr) 
{ 
    if (!element()->hasClass()) { 
        m_classNames.clear(); 
        return;
    }

    m_classNames.set(classStr, element()->document()->inQuirksMode()); 
}

bool NamedNodeMap::mapsEquivalent(const NamedNodeMap* otherMap) const
{
    if (!otherMap)
        return false;
    
    unsigned len = length();
    if (len != otherMap->length())
        return false;
    
    for (unsigned i = 0; i < len; i++) {
        Attribute* attr = attributeItem(i);
        Attribute* otherAttr = otherMap->getAttributeItem(attr->name());
        if (!otherAttr || attr->value() != otherAttr->value())
            return false;
    }
    
    return true;
}

CSSMutableStyleDeclaration* NamedNodeMap::ensureInlineStyleDecl()
{
    if (!m_inlineStyleDecl)
        createInlineStyleDecl();
    return m_inlineStyleDecl.get();
}

void NamedNodeMap::destroyInlineStyleDecl()
{
    if (!m_inlineStyleDecl)
        return;
    m_inlineStyleDecl->clearParentElement();
    m_inlineStyleDecl = 0;
}

void NamedNodeMap::createInlineStyleDecl()
{
    ASSERT(!m_inlineStyleDecl);
    ASSERT(m_element->isStyledElement());
    m_inlineStyleDecl = CSSMutableStyleDeclaration::createInline(static_cast<StyledElement*>(m_element));
    m_inlineStyleDecl->setStrictParsing(m_element->isHTMLElement() && !m_element->document()->inQuirksMode());
}

} // namespace WebCore
