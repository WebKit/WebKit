/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc. All rights reserved.
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
#include "HTMLElement.h"

namespace WebCore {

using namespace HTMLNames;

void NamedNodeMap::ref()
{
    m_element.ref();
}

void NamedNodeMap::deref()
{
    m_element.deref();
}

RefPtr<Attr> NamedNodeMap::getNamedItem(const AtomicString& name) const
{
    return m_element.getAttributeNode(name);
}

RefPtr<Attr> NamedNodeMap::getNamedItemNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    return m_element.getAttributeNodeNS(namespaceURI, localName);
}

ExceptionOr<Ref<Attr>> NamedNodeMap::removeNamedItem(const AtomicString& name)
{
    if (!m_element.hasAttributes())
        return Exception { NotFoundError };
    auto index = m_element.findAttributeIndexByName(name, shouldIgnoreAttributeCase(m_element));
    if (index == ElementData::attributeNotFound)
        return Exception { NotFoundError };
    return m_element.detachAttribute(index);
}

Vector<String> NamedNodeMap::supportedPropertyNames() const
{
    Vector<String> names = m_element.getAttributeNames();
    if (is<HTMLElement>(m_element) && m_element.document().isHTMLDocument()) {
        names.removeAllMatching([](String& name) {
            for (auto character : StringView { name }.codeUnits()) {
                if (isASCIIUpper(character))
                    return true;
            }
            return false;
        });
    }
    return names;
}

ExceptionOr<Ref<Attr>> NamedNodeMap::removeNamedItemNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    if (!m_element.hasAttributes())
        return Exception { NotFoundError };
    auto index = m_element.findAttributeIndexByName(QualifiedName { nullAtom(), localName, namespaceURI });
    if (index == ElementData::attributeNotFound)
        return Exception { NotFoundError };
    return m_element.detachAttribute(index);
}

ExceptionOr<RefPtr<Attr>> NamedNodeMap::setNamedItem(Attr& attr)
{
    return m_element.setAttributeNode(attr);
}

RefPtr<Attr> NamedNodeMap::item(unsigned index) const
{
    if (index >= length())
        return nullptr;
    return m_element.ensureAttr(m_element.attributeAt(index).name());
}

unsigned NamedNodeMap::length() const
{
    if (!m_element.hasAttributes())
        return 0;
    return m_element.attributeCount();
}

} // namespace WebCore
