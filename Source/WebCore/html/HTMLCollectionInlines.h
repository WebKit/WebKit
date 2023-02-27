/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "HTMLCollection.h"
#include "LiveNodeListInlines.h"

namespace WebCore {

inline ContainerNode& HTMLCollection::rootNode() const
{
    if (isRootedAtTreeScope() && ownerNode().isInTreeScope())
        return ownerNode().treeScope().rootNode();
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

inline void CollectionNamedElementCache::didPopulate()
{
#if ASSERT_ENABLED
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

inline bool HTMLCollection::isRootedAtTreeScope() const
{
    return m_rootType == IsRootedAtTreeScope;
}

inline NodeListInvalidationType HTMLCollection::invalidationType() const
{
    return static_cast<NodeListInvalidationType>(m_invalidationType);
}

inline CollectionType HTMLCollection::type() const
{
    return static_cast<CollectionType>(m_collectionType);
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

inline void HTMLCollection::invalidateCache()
{
    invalidateCacheForDocument(document());
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
        Locker locker { m_namedElementCacheAssignmentLock };
        m_namedElementCache = WTFMove(cache);
    }
    document().collectionCachedIdNameMap(*this);
}

inline const CollectionNamedElementCache& HTMLCollection::namedItemCaches() const
{
    ASSERT(!!m_namedElementCache);
    return *m_namedElementCache;
}


}
