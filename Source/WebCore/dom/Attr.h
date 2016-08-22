/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#pragma once

#include "ContainerNode.h"
#include "QualifiedName.h"

namespace WebCore {

class Attribute;
class CSSStyleDeclaration;
class MutableStyleProperties;

// Attr can have Text children
// therefore it has to be a fullblown Node. The plan
// is to dynamically allocate a textchild and store the
// resulting nodevalue in the attribute upon
// destruction. however, this is not yet implemented.

class Attr final : public ContainerNode {
public:
    static Ref<Attr> create(Element*, const QualifiedName&);
    static Ref<Attr> create(Document&, const QualifiedName&, const AtomicString& value);
    virtual ~Attr();

    String name() const { return qualifiedName().toString(); }
    bool specified() const { return true; }
    Element* ownerElement() const { return m_element; }

    WEBCORE_EXPORT const AtomicString& value() const;
    void setValue(const AtomicString&);
    const AtomicString& valueForBindings() const { return value(); }
    WEBCORE_EXPORT void setValueForBindings(const AtomicString&);

    const QualifiedName& qualifiedName() const { return m_name; }

    WEBCORE_EXPORT bool isId() const;

    WEBCORE_EXPORT CSSStyleDeclaration* style();

    void attachToElement(Element*);
    void detachFromElementWithValue(const AtomicString&);

    const AtomicString& namespaceURI() const final { return m_name.namespaceURI(); }
    const AtomicString& localName() const final { return m_name.localName(); }
    const AtomicString& prefix() const final { return m_name.prefix(); }

    void setPrefix(const AtomicString&, ExceptionCode&) final;

private:
    Attr(Element*, const QualifiedName&);
    Attr(Document&, const QualifiedName&, const AtomicString& value);

    void createTextChild();

    String nodeName() const override { return name(); }
    NodeType nodeType() const override { return ATTRIBUTE_NODE; }

    String nodeValue() const override { return value(); }
    void setNodeValue(const String&, ExceptionCode&) override;
    Ref<Node> cloneNodeInternal(Document&, CloningOperation) override;

    bool isAttributeNode() const override { return true; }
    bool childTypeAllowed(NodeType) const override;

    void childrenChanged(const ChildChange&) override;

    Attribute& elementAttribute();

    // Attr wraps either an element/name, or a name/value pair (when it's a standalone Node.)
    // Note that m_name is always set, but m_element/m_standaloneValue may be null.
    Element* m_element { nullptr };
    QualifiedName m_name;
    AtomicString m_standaloneValue;

    RefPtr<MutableStyleProperties> m_style;
    unsigned m_ignoreChildrenChanged { 0 };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Attr)
    static bool isType(const WebCore::Node& node) { return node.isAttributeNode(); }
SPECIALIZE_TYPE_TRAITS_END()
