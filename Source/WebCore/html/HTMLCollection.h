/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2011, 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#include "CollectionIndexCache.h"
#include "HTMLNames.h"
#include "LiveNodeList.h"
#include "ScriptWrappable.h"
#include <wtf/HashMap.h>

namespace WebCore {

class Element;

class CollectionNamedElementCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    const Vector<Element*>* findElementsWithId(const AtomicString& id) const;
    const Vector<Element*>* findElementsWithName(const AtomicString& name) const;
    const Vector<AtomicString>& propertyNames() const { return m_propertyNames; }

    void appendToIdCache(const AtomicString& id, Element&);
    void appendToNameCache(const AtomicString& name, Element&);
    void didPopulate();

    size_t memoryCost() const;

private:
    typedef HashMap<AtomicStringImpl*, Vector<Element*>> StringToElementsMap;

    const Vector<Element*>* find(const StringToElementsMap&, const AtomicString& key) const;
    void append(StringToElementsMap&, const AtomicString& key, Element&);

    StringToElementsMap m_idMap;
    StringToElementsMap m_nameMap;
    Vector<AtomicString> m_propertyNames;

#if !ASSERT_DISABLED
    bool m_didPopulate { false };
#endif
};

// HTMLCollection subclasses NodeList to maintain legacy ObjC API compatibility.
class HTMLCollection : public NodeList {
public:
    virtual ~HTMLCollection();

    // DOM API
    Element* item(unsigned index) const override = 0; // Tighten return type from NodeList::item().
    virtual Element* namedItem(const AtomicString& name) const = 0;
    const Vector<AtomicString>& supportedPropertyNames();

    // Non-DOM API
    Vector<Ref<Element>> namedItems(const AtomicString& name) const;
    size_t memoryCost() const override;

    bool isRootedAtDocument() const;
    NodeListInvalidationType invalidationType() const;
    CollectionType type() const;
    ContainerNode& ownerNode() const;
    ContainerNode& rootNode() const;
    void invalidateCacheForAttribute(const QualifiedName* attributeName);
    virtual void invalidateCache(Document&);

    bool hasNamedElementCache() const;

protected:
    HTMLCollection(ContainerNode& base, CollectionType);

    virtual void updateNamedElementCache() const;
    WEBCORE_EXPORT Element* namedItemSlow(const AtomicString& name) const;

    void setNamedItemCache(std::unique_ptr<CollectionNamedElementCache>) const;
    const CollectionNamedElementCache& namedItemCaches() const;

    Document& document() const;

    void invalidateNamedElementCache(Document&) const;

    enum RootType { IsRootedAtNode, IsRootedAtDocument };
    static RootType rootTypeFromCollectionType(CollectionType);

    Ref<ContainerNode> m_ownerNode;

    mutable std::unique_ptr<CollectionNamedElementCache> m_namedElementCache;

    const unsigned m_collectionType : 5;
    const unsigned m_invalidationType : 4;
    const unsigned m_rootType : 1;
};

inline ContainerNode& HTMLCollection::rootNode() const
{
    if (isRootedAtDocument() && ownerNode().isConnected())
        return ownerNode().document();

    return ownerNode();
}

inline const Vector<Element*>* CollectionNamedElementCache::findElementsWithId(const AtomicString& id) const
{
    return find(m_idMap, id);
}

inline const Vector<Element*>* CollectionNamedElementCache::findElementsWithName(const AtomicString& name) const
{
    return find(m_nameMap, name);
}

inline void CollectionNamedElementCache::appendToIdCache(const AtomicString& id, Element& element)
{
    append(m_idMap, id, element);
}

inline void CollectionNamedElementCache::appendToNameCache(const AtomicString& name, Element& element)
{
    append(m_nameMap, name, element);
}

inline size_t CollectionNamedElementCache::memoryCost() const
{
    return (m_idMap.size() + m_nameMap.size()) * sizeof(Element*) + m_propertyNames.size() * sizeof(AtomicString);
}

inline void CollectionNamedElementCache::didPopulate()
{
#if !ASSERT_DISABLED
    m_didPopulate = true;
#endif
    if (size_t cost = memoryCost())
        reportExtraMemoryAllocatedForCollectionIndexCache(cost);
}

inline const Vector<Element*>* CollectionNamedElementCache::find(const StringToElementsMap& map, const AtomicString& key) const
{
    ASSERT(m_didPopulate);
    auto it = map.find(key.impl());
    return it != map.end() ? &it->value : nullptr;
}

inline void CollectionNamedElementCache::append(StringToElementsMap& map, const AtomicString& key, Element& element)
{
    if (!m_idMap.contains(key.impl()) && !m_nameMap.contains(key.impl()))
        m_propertyNames.append(key);
    map.add(key.impl(), Vector<Element*>()).iterator->value.append(&element);
}

inline size_t HTMLCollection::memoryCost() const
{
    return m_namedElementCache ? m_namedElementCache->memoryCost() : 0;
}

inline bool HTMLCollection::isRootedAtDocument() const
{
    return m_rootType == IsRootedAtDocument;
}

inline NodeListInvalidationType HTMLCollection::invalidationType() const
{
    return static_cast<NodeListInvalidationType>(m_invalidationType);
}

inline CollectionType HTMLCollection::type() const
{
    return static_cast<CollectionType>(m_collectionType);
}

inline ContainerNode& HTMLCollection::ownerNode() const
{
    return m_ownerNode;
}

inline Document& HTMLCollection::document() const
{
    return m_ownerNode->document();
}

inline void HTMLCollection::invalidateCacheForAttribute(const QualifiedName* attributeName)
{
    if (!attributeName || shouldInvalidateTypeOnAttributeChange(invalidationType(), *attributeName))
        invalidateCache(document());
    else if (hasNamedElementCache() && (*attributeName == HTMLNames::idAttr || *attributeName == HTMLNames::nameAttr))
        invalidateNamedElementCache(document());
}

inline bool HTMLCollection::hasNamedElementCache() const
{
    return !!m_namedElementCache;
}

inline void HTMLCollection::setNamedItemCache(std::unique_ptr<CollectionNamedElementCache> cache) const
{
    ASSERT(cache);
    ASSERT(!m_namedElementCache);
    cache->didPopulate();
    m_namedElementCache = WTFMove(cache);
    document().collectionCachedIdNameMap(*this);
}

inline const CollectionNamedElementCache& HTMLCollection::namedItemCaches() const
{
    ASSERT(!!m_namedElementCache);
    return *m_namedElementCache;
}

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_HTMLCOLLECTION(ClassName, Type) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ClassName) \
    static bool isType(const WebCore::HTMLCollection& collection) { return collection.type() == WebCore::Type; } \
SPECIALIZE_TYPE_TRAITS_END()
