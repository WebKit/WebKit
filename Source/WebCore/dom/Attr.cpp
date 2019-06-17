/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2012 Apple Inc. All rights reserved.
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
#include "Attr.h"

#include "AttributeChangeInvalidation.h"
#include "Event.h"
#include "ScopedEventQueue.h"
#include "StyleProperties.h"
#include "StyledElement.h"
#include "TextNodeTraversal.h"
#include "XMLNSNames.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Attr);

using namespace HTMLNames;

Attr::Attr(Element& element, const QualifiedName& name)
    : Node(element.document(), CreateOther)
    , m_element(&element)
    , m_name(name)
{
}

Attr::Attr(Document& document, const QualifiedName& name, const AtomString& standaloneValue)
    : Node(document, CreateOther)
    , m_name(name)
    , m_standaloneValue(standaloneValue)
{
}

Ref<Attr> Attr::create(Element& element, const QualifiedName& name)
{
    return adoptRef(*new Attr(element, name));
}

Ref<Attr> Attr::create(Document& document, const QualifiedName& name, const AtomString& value)
{
    return adoptRef(*new Attr(document, name, value));
}

Attr::~Attr()
{
    ASSERT_WITH_SECURITY_IMPLICATION(!isInShadowTree());
    ASSERT_WITH_SECURITY_IMPLICATION(treeScope().rootNode().isDocumentNode());
}

ExceptionOr<void> Attr::setPrefix(const AtomString& prefix)
{
    auto result = checkSetPrefix(prefix);
    if (result.hasException())
        return result.releaseException();

    if ((prefix == xmlnsAtom() && namespaceURI() != XMLNSNames::xmlnsNamespaceURI) || qualifiedName() == xmlnsAtom())
        return Exception { NamespaceError };

    const AtomString& newPrefix = prefix.isEmpty() ? nullAtom() : prefix;
    if (m_element)
        elementAttribute().setPrefix(newPrefix);
    m_name.setPrefix(newPrefix);

    return { };
}

void Attr::setValue(const AtomString& value)
{
    if (m_element)
        m_element->setAttribute(qualifiedName(), value);
    else
        m_standaloneValue = value;
}

ExceptionOr<void> Attr::setNodeValue(const String& value)
{
    setValue(value);
    return { };
}

Ref<Node> Attr::cloneNodeInternal(Document& targetDocument, CloningOperation)
{
    return adoptRef(*new Attr(targetDocument, qualifiedName(), value()));
}

CSSStyleDeclaration* Attr::style()
{
    // This is not part of the DOM API, and therefore not available to webpages. However, WebKit SPI
    // lets clients use this via the Objective-C and JavaScript bindings.
    if (!is<StyledElement>(m_element))
        return nullptr;
    m_style = MutableStyleProperties::create();
    downcast<StyledElement>(*m_element).collectStyleForPresentationAttribute(qualifiedName(), value(), *m_style);
    return &m_style->ensureCSSStyleDeclaration();
}

const AtomString& Attr::value() const
{
    if (m_element)
        return m_element->getAttribute(qualifiedName());
    return m_standaloneValue;
}

Attribute& Attr::elementAttribute()
{
    ASSERT(m_element);
    ASSERT(m_element->elementData());
    return *m_element->ensureUniqueElementData().findAttributeByName(qualifiedName());
}

void Attr::detachFromElementWithValue(const AtomString& value)
{
    ASSERT(m_element);
    ASSERT(m_standaloneValue.isNull());
    m_standaloneValue = value;
    m_element = nullptr;
    setTreeScopeRecursively(document());
}

void Attr::attachToElement(Element& element)
{
    ASSERT(!m_element);
    m_element = &element;
    m_standaloneValue = nullAtom();
    setTreeScopeRecursively(element.treeScope());
}

}
