/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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
#include <wtf/HashMap.h>

namespace WebCore {

class Element;

class CollectionNamedElementCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    const Vector<Element*>* findElementsWithId(const AtomString& id) const;
    const Vector<Element*>* findElementsWithName(const AtomString& name) const;
    const Vector<AtomString>& propertyNames() const { return m_propertyNames; }
    
    void appendToIdCache(const AtomString& id, Element&);
    void appendToNameCache(const AtomString& name, Element&);
    void didPopulate();

    size_t memoryCost() const;

private:
    typedef HashMap<AtomStringImpl*, Vector<Element*>> StringToElementsMap;

    const Vector<Element*>* find(const StringToElementsMap&, const AtomString& key) const;
    void append(StringToElementsMap&, const AtomString& key, Element&);

    StringToElementsMap m_idMap;
    StringToElementsMap m_nameMap;
    Vector<AtomString> m_propertyNames;

#if !ASSERT_DISABLED
    bool m_didPopulate { false };
#endif
};

// HTMLCollection subclasses NodeList to maintain legacy ObjC API compatibility.
class HTMLCollection : public NodeList {
    WTF_MAKE_ISO_ALLOCATED(HTMLCollection);
public:
    virtual ~HTMLCollection();

    // DOM API
    Element* item(unsigned index) const override = 0; // Tighten return type from NodeList::item().
    virtual Element* namedItem(const AtomString& name) const = 0;
    const Vector<AtomString>& supportedPropertyNames();
    bool isSupportedPropertyName(const String& name);

    // Non-DOM API
    Vector<Ref<Element>> namedItems(const AtomString& name) const;
    size_t memoryCost() const override;

    bool isRootedAtDocument() const;
    NodeListInvalidationType invalidationType() const;
    CollectionType type() const;
    ContainerNode& ownerNode() const;
    ContainerNode& rootNode() const;
    void invalidateCacheForAttribute(const QualifiedName& attributeName);
    virtual void invalidateCacheForDocument(Document&);
    void invalidateCache() { invalidateCacheForDocument(document()); }

    bool hasNamedElementCache() const;

protected:
    HTMLCollection(ContainerNode& base, CollectionType);

    virtual void updateNamedElementCache() const;
    WEBCORE_EXPORT Element* namedItemSlow(const AtomString& name) const;

    void setNamedItemCache(std::unique_ptr<CollectionNamedElementCache>) const;
    const CollectionNamedElementCache& namedItemCaches() const;

    Document& document() const;

    void invalidateNamedElementCache(Document&) const;

    enum RootType { IsRootedAtNode, IsRootedAtDocument };
    static RootType rootTypeFromCollectionType(CollectionType);

    mutable Lock m_namedElementCacheAssignmentLock;

    const unsigned m_collectionType : 5; // CollectionType
    const unsigned m_invalidationType : 4; // NodeListInvalidationType
    const unsigned m_rootType : 1; // RootType

    Ref<ContainerNode> m_ownerNode;

    mutable std::unique_ptr<CollectionNamedElementCache> m_namedElementCache;
};

inline ContainerNode& HTMLCollection::rootNode() const
{
    if (isRootedAtDocument() && ownerNode().isConnected())
        return ownerNode().document();

    return ownerNode();
}

inline const Vector<Element*>* CollectionNamedElementCache::findElementsWithId(const AtomString& id) const
{
    return find(m_idMap, id);
}

inline const Vector<Element*>* CollectionNamedElementCache::findElementsWithName(const AtomString& name) const
{
    return find(m_nameMap, name);
}

inline void CollectionNamedElementCache::appendToIdCache(const AtomString& id, Element& element)
{
    append(m_idMap, id, element);
}

inline void CollectionNamedElementCache::appendToNameCache(const AtomString& name, Element& element)
{
    append(m_nameMap, name, element);
}

inline size_t CollectionNamedElementCache::memoryCost() const
{
    // memoryCost() may be invoked concurrently from a GC thread, and we need to be careful about what data we access here and how.
    // It is safe to access m_idMap.size(), m_nameMap.size(), and m_propertyNames.size() because they don't chase pointers.
    return (m_idMap.size() + m_nameMap.size()) * sizeof(Element*) + m_propertyNames.size() * sizeof(AtomString);
}

inline void CollectionNamedElementCache::didPopulate()
{
#if !ASSERT_DISABLED
    m_didPopulate = true;
#endif
    if (size_t cost = memoryCost())
        reportExtraMemoryAllocatedForCollectionIndexCache(cost);
}

inline const Vector<Element*>* CollectionNamedElementCache::find(const StringToElementsMap& map, const AtomString& key) const
{
    ASSERT(m_didPopulate);
    auto it = map.find(key.impl());
    return it != map.end() ? &it->value : nullptr;
}

inline void CollectionNamedElementCache::append(StringToElementsMap& map, const AtomString& key, Element& element)
{
    if (!m_idMap.contains(key.impl()) && !m_nameMap.contains(key.impl()))
        m_propertyNames.append(key);
    map.add(key.impl(), Vector<Element*>()).iterator->value.append(&element);
}

inline size_t HTMLCollection::memoryCost() const
{
    // memoryCost() may be invoked concurrently from a GC thread, and we need to be careful about what data we access here and how.
    // Hence, we need to guard m_namedElementCache from being replaced while accessing it.
    auto locker = holdLock(m_namedElementCacheAssignmentLock);
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

inline void HTMLCollection::invalidateCacheForAttribute(const QualifiedName& attributeName)
{
    if (shouldInvalidateTypeOnAttributeChange(invalidationType(), attributeName))
        invalidateCache();
    else if (hasNamedElementCache() && (attributeName == HTMLNames::idAttr || attributeName == HTMLNames::nameAttr))
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
    {
        auto locker = holdLock(m_namedElementCacheAssignmentLock);
        m_namedElementCache = WTFMove(cache);
    }
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
