/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith <catfish.man@gmail.com>
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

#include "ChildNodeList.h"
#include "HTMLCollection.h"
#include "HTMLNames.h"
#include "LiveNodeList.h"
#include "MutationObserverRegistration.h"
#include "Page.h"
#include "QualifiedName.h"
#include "TagCollection.h"
#include <wtf/HashSet.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class LabelsNodeList;
class NameNodeList;
class RadioNodeList;
class TreeScope;

template <class ListType> struct NodeListTypeIdentifier;
template <> struct NodeListTypeIdentifier<NameNodeList> { static int value() { return 0; } };
template <> struct NodeListTypeIdentifier<RadioNodeList> { static int value() { return 1; } };
template <> struct NodeListTypeIdentifier<LabelsNodeList> { static int value() { return 2; } };

class NodeListsNodeData {
    WTF_MAKE_NONCOPYABLE(NodeListsNodeData); WTF_MAKE_FAST_ALLOCATED;
public:
    NodeListsNodeData()
        : m_childNodeList(nullptr)
        , m_emptyChildNodeList(nullptr)
    {
    }

    void clearChildNodeListCache()
    {
        if (m_childNodeList)
            m_childNodeList->invalidateCache();
    }

    Ref<ChildNodeList> ensureChildNodeList(ContainerNode& node)
    {
        ASSERT(!m_emptyChildNodeList);
        if (m_childNodeList)
            return *m_childNodeList;
        auto list = ChildNodeList::create(node);
        m_childNodeList = list.ptr();
        return list;
    }

    void removeChildNodeList(ChildNodeList* list)
    {
        ASSERT(m_childNodeList == list);
        if (deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(list->ownerNode()))
            return;
        m_childNodeList = nullptr;
    }

    Ref<EmptyNodeList> ensureEmptyChildNodeList(Node& node)
    {
        ASSERT(!m_childNodeList);
        if (m_emptyChildNodeList)
            return *m_emptyChildNodeList;
        auto list = EmptyNodeList::create(node);
        m_emptyChildNodeList = list.ptr();
        return list;
    }

    void removeEmptyChildNodeList(EmptyNodeList* list)
    {
        ASSERT(m_emptyChildNodeList == list);
        if (deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(list->ownerNode()))
            return;
        m_emptyChildNodeList = nullptr;
    }

    struct NodeListCacheMapEntryHash {
        static unsigned hash(const std::pair<unsigned char, AtomicString>& entry)
        {
            return DefaultHash<AtomicString>::Hash::hash(entry.second) + entry.first;
        }
        static bool equal(const std::pair<unsigned char, AtomicString>& a, const std::pair<unsigned char, AtomicString>& b) { return a.first == b.first && DefaultHash<AtomicString>::Hash::equal(a.second, b.second); }
        static const bool safeToCompareToEmptyOrDeleted = DefaultHash<AtomicString>::Hash::safeToCompareToEmptyOrDeleted;
    };

    typedef HashMap<std::pair<unsigned char, AtomicString>, LiveNodeList*, NodeListCacheMapEntryHash> NodeListAtomicNameCacheMap;
    typedef HashMap<std::pair<unsigned char, AtomicString>, HTMLCollection*, NodeListCacheMapEntryHash> CollectionCacheMap;
    typedef HashMap<QualifiedName, TagCollectionNS*> TagCollectionNSCache;

    template<typename T, typename ContainerType>
    ALWAYS_INLINE Ref<T> addCacheWithAtomicName(ContainerType& container, const AtomicString& name)
    {
        NodeListAtomicNameCacheMap::AddResult result = m_atomicNameCaches.fastAdd(namedNodeListKey<T>(name), nullptr);
        if (!result.isNewEntry)
            return static_cast<T&>(*result.iterator->value);

        auto list = T::create(container, name);
        result.iterator->value = &list.get();
        return list;
    }

    ALWAYS_INLINE Ref<TagCollectionNS> addCachedTagCollectionNS(ContainerNode& node, const AtomicString& namespaceURI, const AtomicString& localName)
    {
        QualifiedName name(nullAtom(), localName, namespaceURI);
        TagCollectionNSCache::AddResult result = m_tagCollectionNSCache.fastAdd(name, nullptr);
        if (!result.isNewEntry)
            return *result.iterator->value;

        auto list = TagCollectionNS::create(node, namespaceURI, localName);
        result.iterator->value = list.ptr();
        return list;
    }

    template<typename T, typename ContainerType>
    ALWAYS_INLINE Ref<T> addCachedCollection(ContainerType& container, CollectionType collectionType, const AtomicString& name)
    {
        CollectionCacheMap::AddResult result = m_cachedCollections.fastAdd(namedCollectionKey(collectionType, name), nullptr);
        if (!result.isNewEntry)
            return static_cast<T&>(*result.iterator->value);

        auto list = T::create(container, collectionType, name);
        result.iterator->value = &list.get();
        return list;
    }

    template<typename T, typename ContainerType>
    ALWAYS_INLINE Ref<T> addCachedCollection(ContainerType& container, CollectionType collectionType)
    {
        CollectionCacheMap::AddResult result = m_cachedCollections.fastAdd(namedCollectionKey(collectionType, starAtom()), nullptr);
        if (!result.isNewEntry)
            return static_cast<T&>(*result.iterator->value);

        auto list = T::create(container, collectionType);
        result.iterator->value = &list.get();
        return list;
    }

    template<typename T>
    T* cachedCollection(CollectionType collectionType)
    {
        return static_cast<T*>(m_cachedCollections.get(namedCollectionKey(collectionType, starAtom())));
    }

    template <class NodeListType>
    void removeCacheWithAtomicName(NodeListType* list, const AtomicString& name = starAtom())
    {
        ASSERT(list == m_atomicNameCaches.get(namedNodeListKey<NodeListType>(name)));
        if (deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(list->ownerNode()))
            return;
        m_atomicNameCaches.remove(namedNodeListKey<NodeListType>(name));
    }

    void removeCachedTagCollectionNS(HTMLCollection& collection, const AtomicString& namespaceURI, const AtomicString& localName)
    {
        QualifiedName name(nullAtom(), localName, namespaceURI);
        ASSERT(&collection == m_tagCollectionNSCache.get(name));
        if (deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(collection.ownerNode()))
            return;
        m_tagCollectionNSCache.remove(name);
    }

    void removeCachedCollection(HTMLCollection* collection, const AtomicString& name = starAtom())
    {
        ASSERT(collection == m_cachedCollections.get(namedCollectionKey(collection->type(), name)));
        if (deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(collection->ownerNode()))
            return;
        m_cachedCollections.remove(namedCollectionKey(collection->type(), name));
    }

    void invalidateCaches();
    void invalidateCachesForAttribute(const QualifiedName& attrName);

    void adoptTreeScope()
    {
        invalidateCaches();
    }

    void adoptDocument(Document& oldDocument, Document& newDocument)
    {
        if (&oldDocument == &newDocument) {
            invalidateCaches();
            return;
        }

        for (auto& cache : m_atomicNameCaches.values())
            cache->invalidateCacheForDocument(oldDocument);

        for (auto& list : m_tagCollectionNSCache.values()) {
            ASSERT(!list->isRootedAtDocument());
            list->invalidateCacheForDocument(oldDocument);
        }

        for (auto& collection : m_cachedCollections.values())
            collection->invalidateCacheForDocument(oldDocument);
    }

private:
    std::pair<unsigned char, AtomicString> namedCollectionKey(CollectionType type, const AtomicString& name)
    {
        return std::pair<unsigned char, AtomicString>(type, name);
    }

    template <class NodeListType>
    std::pair<unsigned char, AtomicString> namedNodeListKey(const AtomicString& name)
    {
        return std::pair<unsigned char, AtomicString>(NodeListTypeIdentifier<NodeListType>::value(), name);
    }

    bool deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(Node&);

    // These two are currently mutually exclusive and could be unioned. Not very important as this class is large anyway.
    ChildNodeList* m_childNodeList;
    EmptyNodeList* m_emptyChildNodeList;

    NodeListAtomicNameCacheMap m_atomicNameCaches;
    TagCollectionNSCache m_tagCollectionNSCache;
    CollectionCacheMap m_cachedCollections;
};

class NodeMutationObserverData {
    WTF_MAKE_NONCOPYABLE(NodeMutationObserverData); WTF_MAKE_FAST_ALLOCATED;
public:
    Vector<std::unique_ptr<MutationObserverRegistration>> registry;
    HashSet<MutationObserverRegistration*> transientRegistry;

    NodeMutationObserverData() { }
};

class NodeRareData : public NodeRareDataBase {
    WTF_MAKE_NONCOPYABLE(NodeRareData); WTF_MAKE_FAST_ALLOCATED;
public:
    NodeRareData(RenderObject* renderer)
        : NodeRareDataBase(renderer)
        , m_connectedFrameCount(0)
    { }

    void clearNodeLists() { m_nodeLists = nullptr; }
    NodeListsNodeData* nodeLists() const { return m_nodeLists.get(); }
    NodeListsNodeData& ensureNodeLists()
    {
        if (!m_nodeLists)
            m_nodeLists = std::make_unique<NodeListsNodeData>();
        return *m_nodeLists;
    }

    NodeMutationObserverData* mutationObserverData() { return m_mutationObserverData.get(); }
    NodeMutationObserverData& ensureMutationObserverData()
    {
        if (!m_mutationObserverData)
            m_mutationObserverData = std::make_unique<NodeMutationObserverData>();
        return *m_mutationObserverData;
    }

    unsigned connectedSubframeCount() const { return m_connectedFrameCount; }
    void incrementConnectedSubframeCount(unsigned amount)
    {
        m_connectedFrameCount += amount;
    }
    void decrementConnectedSubframeCount(unsigned amount)
    {
        ASSERT(m_connectedFrameCount);
        ASSERT(amount <= m_connectedFrameCount);
        m_connectedFrameCount -= amount;
    }

private:
    unsigned m_connectedFrameCount : 10; // Must fit Page::maxNumberOfFrames.

    std::unique_ptr<NodeListsNodeData> m_nodeLists;
    std::unique_ptr<NodeMutationObserverData> m_mutationObserverData;
};

inline bool NodeListsNodeData::deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(Node& ownerNode)
{
    ASSERT(ownerNode.nodeLists() == this);
    if ((m_childNodeList ? 1 : 0) + (m_emptyChildNodeList ? 1 : 0) + m_atomicNameCaches.size()
        + m_tagCollectionNSCache.size() + m_cachedCollections.size() != 1)
        return false;
    ownerNode.clearNodeLists();
    return true;
}

inline NodeRareData* Node::rareData() const
{
    ASSERT_WITH_SECURITY_IMPLICATION(hasRareData());
    return static_cast<NodeRareData*>(m_data.m_rareData);
}

inline NodeRareData& Node::ensureRareData()
{
    if (!hasRareData())
        materializeRareData();
    return *rareData();
}

// Ensure the 10 bits reserved for the m_connectedFrameCount cannot overflow
static_assert(Page::maxNumberOfFrames < 1024, "Frame limit should fit in rare data count");

} // namespace WebCore
