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
#include "Element.h"
#include "ExceptionCode.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

static inline bool shouldIgnoreAttributeCase(const Element* e)
{
    return e && e->document()->isHTMLDocument() && e->isHTMLElement();
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

PassRefPtr<Node> NamedNodeMap::getNamedItem(const String& name) const
{
    Attribute* a = m_attributeData.getAttributeItem(name, shouldIgnoreAttributeCase(m_element));
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
    ASSERT(m_element);

    size_t index = m_attributeData.getAttributeItemIndex(name, shouldIgnoreAttributeCase(m_element));
    if (index == notFound) {
        ec = NOT_FOUND_ERR;
        return 0;
    }
    
    return m_attributeData.takeAttribute(index, m_element);
}

PassRefPtr<Node> NamedNodeMap::removeNamedItemNS(const String& namespaceURI, const String& localName, ExceptionCode& ec)
{
    return removeNamedItem(QualifiedName(nullAtom, localName, namespaceURI), ec);
}

PassRefPtr<Node> NamedNodeMap::getNamedItem(const QualifiedName& name) const
{
    Attribute* a = m_attributeData.getAttributeItem(name);
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

    return m_element->setAttributeNode(static_cast<Attr*>(node), ec);
}

PassRefPtr<Node> NamedNodeMap::setNamedItemNS(Node* node, ExceptionCode& ec)
{
    return setNamedItem(node, ec);
}

PassRefPtr<Node> NamedNodeMap::removeNamedItem(const QualifiedName& name, ExceptionCode& ec)
{
    ASSERT(m_element);

    size_t index = m_attributeData.getAttributeItemIndex(name);
    if (index == notFound) {
        ec = NOT_FOUND_ERR;
        return 0;
    }

    return m_attributeData.takeAttribute(index, m_element);
}

PassRefPtr<Node> NamedNodeMap::item(unsigned index) const
{
    if (index >= length())
        return 0;

    return m_attributeData.m_attributes[index]->createAttrIfNeeded(m_element);
}

void NamedNodeMap::detachFromElement()
{
    // This can't happen if the holder of the map is JavaScript, because we mark the
    // element if the map is alive. So it has no impact on web page behavior. Because
    // of that, we can simply clear all the attributes to avoid accessing stale
    // pointers to do things like create Attr objects.
    m_element = 0;
    m_attributeData.clearAttributes();
}

bool NamedNodeMap::mapsEquivalent(const NamedNodeMap* otherMap) const
{
    if (!otherMap)
        return m_attributeData.isEmpty();
    return m_attributeData.isEquivalent(otherMap->attributeData());
}

} // namespace WebCore
