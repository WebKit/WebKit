/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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

#include "CachedHTMLCollection.h"
#include "NodeRareData.h"
#include <wtf/IsoMalloc.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class Document;

template <typename HTMLCollectionClass, CollectionTraversalType traversalType>
class HTMLNameCollection : public CachedHTMLCollection<HTMLCollectionClass, traversalType> {
    WTF_MAKE_ISO_NONALLOCATABLE(HTMLNameCollection);
public:
    virtual ~HTMLNameCollection();

    Document& document() { return downcast<Document>(this->ownerNode()); }

protected:
    HTMLNameCollection(Document&, CollectionType, const AtomString& name);

    AtomString m_name;
};

template <typename HTMLCollectionClass, CollectionTraversalType traversalType>
HTMLNameCollection<HTMLCollectionClass, traversalType>::HTMLNameCollection(Document& document, CollectionType type, const AtomString& name)
    : CachedHTMLCollection<HTMLCollectionClass, traversalType>(document, type)
    , m_name(name)
{
}

class WindowNameCollection final : public HTMLNameCollection<WindowNameCollection, CollectionTraversalType::Descendants> {
    WTF_MAKE_ISO_ALLOCATED(WindowNameCollection);
public:
    static Ref<WindowNameCollection> create(Document& document, CollectionType type, const AtomString& name)
    {
        return adoptRef(*new WindowNameCollection(document, type, name));
    }

    // For CachedHTMLCollection.
    bool elementMatches(const Element& element) const { return elementMatches(element, m_name.impl()); }

    static bool elementMatchesIfIdAttributeMatch(const Element&) { return true; }
    static bool elementMatchesIfNameAttributeMatch(const Element&);
    static bool elementMatches(const Element&, const AtomString&);

private:
    WindowNameCollection(Document& document, CollectionType type, const AtomString& name)
        : HTMLNameCollection<WindowNameCollection, CollectionTraversalType::Descendants>(document, type, name)
    {
        ASSERT(type == CollectionType::WindowNamedItems);
    }
};

class DocumentNameCollection final : public HTMLNameCollection<DocumentNameCollection, CollectionTraversalType::Descendants> {
    WTF_MAKE_ISO_ALLOCATED(DocumentNameCollection);
public:
    static Ref<DocumentNameCollection> create(Document& document, CollectionType type, const AtomString& name)
    {
        return adoptRef(*new DocumentNameCollection(document, type, name));
    }

    static bool elementMatchesIfIdAttributeMatch(const Element&);
    static bool elementMatchesIfNameAttributeMatch(const Element&);

    // For CachedHTMLCollection.
    bool elementMatches(const Element& element) const { return elementMatches(element, m_name.impl()); }

    static bool elementMatches(const Element&, const AtomString&);

private:
    DocumentNameCollection(Document& document, CollectionType type, const AtomString& name)
        : HTMLNameCollection<DocumentNameCollection, CollectionTraversalType::Descendants>(document, type, name)
    {
        ASSERT(type == CollectionType::DocumentNamedItems);
    }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_HTMLCOLLECTION(WindowNameCollection, CollectionType::WindowNamedItems)
SPECIALIZE_TYPE_TRAITS_HTMLCOLLECTION(DocumentNameCollection, CollectionType::DocumentNamedItems)
