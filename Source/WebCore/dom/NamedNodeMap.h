/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2010 Apple Inc. All rights reserved.
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
 *
 */

#ifndef NamedNodeMap_h
#define NamedNodeMap_h

#include "ElementAttributeData.h"
#include "SpaceSplitString.h"

namespace WebCore {

class Node;

typedef int ExceptionCode;

class NamedNodeMap {
    friend class Element;
public:
    static PassOwnPtr<NamedNodeMap> create(Element* element = 0)
    {
        return adoptPtr(new NamedNodeMap(element));
    }

    void ref();
    void deref();

    // Public DOM interface.

    PassRefPtr<Node> getNamedItem(const String& name) const;
    PassRefPtr<Node> removeNamedItem(const String& name, ExceptionCode&);

    PassRefPtr<Node> getNamedItemNS(const String& namespaceURI, const String& localName) const;
    PassRefPtr<Node> removeNamedItemNS(const String& namespaceURI, const String& localName, ExceptionCode&);

    PassRefPtr<Node> removeNamedItem(const QualifiedName& name, ExceptionCode&);
    PassRefPtr<Node> setNamedItem(Node*, ExceptionCode&);
    PassRefPtr<Node> setNamedItemNS(Node*, ExceptionCode&);

    PassRefPtr<Node> item(unsigned index) const;
    size_t length() const { return m_attributeData.length(); }

    bool mapsEquivalent(const NamedNodeMap* otherMap) const;

    // These functions do no error checking.
    void addAttribute(PassRefPtr<Attribute> attribute) { m_attributeData.addAttribute(attribute, m_element); }

    Element* element() const { return m_element; }

    ElementAttributeData* attributeData() { return &m_attributeData; }
    const ElementAttributeData* attributeData() const { return &m_attributeData; }

private:
    NamedNodeMap(Element* element)
        : m_element(element)
    {
    }

    void detachFromElement();
    Attribute* getAttributeItem(const String& name, bool shouldIgnoreAttributeCase) const { return m_attributeData.getAttributeItem(name, shouldIgnoreAttributeCase); }

    // FIXME: NamedNodeMap is being broken up into two classes, one containing data
    //        for elements with attributes, and one for exposure to the DOM.
    //        See <http://webkit.org/b/75069> for more information.
    ElementAttributeData m_attributeData;

    Element* m_element;
};

} // namespace WebCore

#endif // NamedNodeMap_h
