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

#ifndef HTMLCollection_h
#define HTMLCollection_h

#include "CollectionIndexCache.h"
#include "HTMLNames.h"
#include "LiveNodeList.h"
#include "ScriptWrappable.h"
#include <wtf/HashMap.h>

namespace WebCore {

class Element;

class CollectionNamedElementCache {
public:
    const Vector<Element*>* findElementsWithId(const AtomicString& id) const;
    const Vector<Element*>* findElementsWithName(const AtomicString& name) const;

    void appendToIdCache(const AtomicString& id, Element&);
    void appendToNameCache(const AtomicString& name, Element&);
    void didPopulate();

    size_t memoryCost() const;

private:
    typedef HashMap<AtomicStringImpl*, Vector<Element*>> StringToElementsMap;

    const Vector<Element*>* find(const StringToElementsMap&, const AtomicString& key) const;
    static void append(StringToElementsMap&, const AtomicString& key, Element&);

    StringToElementsMap m_idMap;
    StringToElementsMap m_nameMap;

#if !ASSERT_DISABLED
    bool m_didPopulate { false };
#endif
};

class HTMLCollection : public ScriptWrappable, public RefCounted<HTMLCollection> {
public:
    static Ref<HTMLCollection> create(ContainerNode& base, CollectionType);
    virtual ~HTMLCollection();

    // DOM API
    unsigned length() const;
    Element* item(unsigned offset) const;
    virtual Element* namedItem(const AtomicString& name) const;
    PassRefPtr<NodeList> tags(const String&);

    // Non-DOM API
    bool hasNamedItem(const AtomicString& name) const;
    Vector<Ref<Element>> namedItems(const AtomicString& name) const;
    size_t memoryCost() const;

    bool isRootedAtDocument() const;
    NodeListInvalidationType invalidationType() const;
    CollectionType type() const;
    ContainerNode& ownerNode() const;
    void invalidateCache(const QualifiedName* attributeName) const;
    virtual void invalidateCache(Document&) const;

    // For CollectionIndexCache; do not use elsewhere.
    Element* collectionBegin() const;
    Element* collectionLast() const;
    Element* collectionEnd() const;
    void collectionTraverseForward(Element*&, unsigned count, unsigned& traversedCount) const;
    void collectionTraverseBackward(Element*&, unsigned count) const;
    bool collectionCanTraverseBackward() const;
    void willValidateIndexCache() const;

    bool hasNamedElementCache() const;

protected:
    enum ElementTraversalType { NormalTraversal, CustomForwardOnlyTraversal };
    HTMLCollection(ContainerNode& base, CollectionType, ElementTraversalType = NormalTraversal);

    virtual void updateNamedElementCache() const;

    void setNamedItemCache(std::unique_ptr<CollectionNamedElementCache>) const;
    const CollectionNamedElementCache& namedItemCaches() const;

private:
    Document& document() const;
    ContainerNode& rootNode() const;
    bool usesCustomForwardOnlyTraversal() const;

    Element* iterateForPreviousElement(Element*) const;
    Element* firstElement(ContainerNode& root) const;
    Element* traverseForward(Element&, unsigned count, unsigned& traversedCount, ContainerNode& root) const;

    virtual Element* customElementAfter(Element*) const;

    void invalidateNamedElementCache(Document&) const;

    enum RootType { IsRootedAtNode, IsRootedAtDocument };
    static RootType rootTypeFromCollectionType(CollectionType);

    Ref<ContainerNode> m_ownerNode;

    mutable CollectionIndexCache<HTMLCollection, Element*> m_indexCache;
    mutable std::unique_ptr<CollectionNamedElementCache> m_namedElementCache;

    const unsigned m_collectionType : 5;
    const unsigned m_invalidationType : 4;
    const unsigned m_rootType : 1;
    const unsigned m_shouldOnlyIncludeDirectChildren : 1;
    const unsigned m_usesCustomForwardOnlyTraversal : 1;
};

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
    return append(m_idMap, id, element);
}

inline void CollectionNamedElementCache::appendToNameCache(const AtomicString& name, Element& element)
{
    return append(m_nameMap, name, element);
}

inline size_t CollectionNamedElementCache::memoryCost() const
{
    return (m_idMap.size() + m_nameMap.size()) * sizeof(Element*);
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
    map.add(key.impl(), Vector<Element*>()).iterator->value.append(&element);
}

inline size_t HTMLCollection::memoryCost() const
{
    return m_indexCache.memoryCost() + (m_namedElementCache ? m_namedElementCache->memoryCost() : 0);
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
    return const_cast<ContainerNode&>(m_ownerNode.get());
}

inline Document& HTMLCollection::document() const
{
    return m_ownerNode->document();
}

inline void HTMLCollection::invalidateCache(const QualifiedName* attributeName) const
{
    if (!attributeName || shouldInvalidateTypeOnAttributeChange(invalidationType(), *attributeName))
        invalidateCache(document());
    else if (hasNamedElementCache() && (*attributeName == HTMLNames::idAttr || *attributeName == HTMLNames::nameAttr))
        invalidateNamedElementCache(document());
}

inline Element* HTMLCollection::collectionEnd() const
{
    return nullptr;
}

inline bool HTMLCollection::collectionCanTraverseBackward() const
{
    return !m_usesCustomForwardOnlyTraversal;
}

inline void HTMLCollection::willValidateIndexCache() const
{
    document().registerCollection(const_cast<HTMLCollection&>(*this));
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
    m_namedElementCache = WTF::move(cache);
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

#endif // HTMLCollection_h
