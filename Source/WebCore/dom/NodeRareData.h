/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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
#include "MutationObserverRegistration.h"
#include "QualifiedName.h"
#include "TagCollection.h"
#include <wtf/HashSet.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class LabelsNodeList;
class LiveNodeList;
class NameNodeList;
class RadioNodeList;

template<typename ListType> struct NodeListTypeIdentifier;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(NodeListsNodeData);
class NodeListsNodeData {
    WTF_MAKE_NONCOPYABLE(NodeListsNodeData);
    WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(NodeListsNodeData);
public:
    NodeListsNodeData() = default;

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
        static unsigned hash(const std::pair<unsigned char, AtomString>& entry)
        {
            return DefaultHash<AtomString>::hash(entry.second) + entry.first;
        }
        static bool equal(const std::pair<unsigned char, AtomString>& a, const std::pair<unsigned char, AtomString>& b) { return a.first == b.first && DefaultHash<AtomString>::equal(a.second, b.second); }
        static const bool safeToCompareToEmptyOrDeleted = DefaultHash<AtomString>::safeToCompareToEmptyOrDeleted;
    };

    using NodeListCacheMap = HashMap<std::pair<unsigned char, AtomString>, LiveNodeList*, NodeListCacheMapEntryHash>;
    using CollectionCacheMap = HashMap<std::pair<unsigned char, AtomString>, HTMLCollection*, NodeListCacheMapEntryHash>;
    using TagCollectionNSCache = HashMap<QualifiedName, TagCollectionNS*>;

    template<typename T, typename ContainerType>
    ALWAYS_INLINE Ref<T> addCacheWithAtomName(ContainerType& container, const AtomString& name)
    {
        auto result = m_atomNameCaches.fastAdd(namedNodeListKey<T>(name), nullptr);
        if (!result.isNewEntry)
            return static_cast<T&>(*result.iterator->value);

        auto list = T::create(container, name);
        result.iterator->value = &list.get();
        return list;
    }

    ALWAYS_INLINE Ref<TagCollectionNS> addCachedTagCollectionNS(ContainerNode& node, const AtomString& namespaceURI, const AtomString& localName)
    {
        auto result = m_tagCollectionNSCache.fastAdd(QualifiedName { nullAtom(), localName, namespaceURI }, nullptr);
        if (!result.isNewEntry)
            return *result.iterator->value;

        auto list = TagCollectionNS::create(node, namespaceURI, localName);
        result.iterator->value = list.ptr();
        return list;
    }

    template<typename T, typename ContainerType>
    ALWAYS_INLINE Ref<T> addCachedCollection(ContainerType& container, CollectionType collectionType, const AtomString& name)
    {
        auto result = m_cachedCollections.fastAdd(namedCollectionKey(collectionType, name), nullptr);
        if (!result.isNewEntry)
            return static_cast<T&>(*result.iterator->value);

        auto list = T::create(container, collectionType, name);
        result.iterator->value = &list.get();
        return list;
    }

    template<typename T, typename ContainerType>
    ALWAYS_INLINE Ref<T> addCachedCollection(ContainerType& container, CollectionType collectionType)
    {
        auto result = m_cachedCollections.fastAdd(namedCollectionKey(collectionType, starAtom()), nullptr);
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

    template<typename NodeListType>
    void removeCacheWithAtomName(NodeListType& list, const AtomString& name)
    {
        ASSERT(&list == m_atomNameCaches.get(namedNodeListKey<NodeListType>(name)));
        if (deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(list.ownerNode()))
            return;
        m_atomNameCaches.remove(namedNodeListKey<NodeListType>(name));
    }

    void removeCachedTagCollectionNS(HTMLCollection& collection, const AtomString& namespaceURI, const AtomString& localName)
    {
        QualifiedName name(nullAtom(), localName, namespaceURI);
        ASSERT(&collection == m_tagCollectionNSCache.get(name));
        if (deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(collection.ownerNode()))
            return;
        m_tagCollectionNSCache.remove(name);
    }

    void removeCachedCollection(HTMLCollection* collection, const AtomString& name = starAtom())
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

        for (auto& cache : m_atomNameCaches.values())
            cache->invalidateCacheForDocument(oldDocument);

        for (auto& list : m_tagCollectionNSCache.values()) {
            ASSERT(!list->isRootedAtDocument());
            list->invalidateCacheForDocument(oldDocument);
        }

        for (auto& collection : m_cachedCollections.values())
            collection->invalidateCacheForDocument(oldDocument);
    }

private:
    std::pair<unsigned char, AtomString> namedCollectionKey(CollectionType type, const AtomString& name)
    {
        return std::pair<unsigned char, AtomString>(type, name);
    }

    template<typename NodeListType>
    std::pair<unsigned char, AtomString> namedNodeListKey(const AtomString& name)
    {
        return std::pair<unsigned char, AtomString>(NodeListTypeIdentifier<NodeListType>::value(), name);
    }

    bool deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(Node&);

    // These two are currently mutually exclusive and could be unioned. Not very important as this class is large anyway.
    ChildNodeList* m_childNodeList { nullptr };
    EmptyNodeList* m_emptyChildNodeList { nullptr };

    NodeListCacheMap m_atomNameCaches;
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

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(NodeRareData);
class NodeRareData {
    WTF_MAKE_NONCOPYABLE(NodeRareData);
    WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(NodeRareData);
public:
#if defined(DUMP_NODE_STATISTICS) && DUMP_NODE_STATISTICS
    enum class UseType : uint16_t {
        ConnectedFrameCount = 1 << 0,
        NodeList = 1 << 1,
        MutationObserver = 1 << 2,
        TabIndex = 1 << 3,
        MinimumSize = 1 << 4,
        ScrollingPosition = 1 << 5,
        ComputedStyle = 1 << 6,
        Dataset = 1 << 7,
        ClassList = 1 << 8,
        ShadowRoot = 1 << 9,
        CustomElementQueue = 1 << 10,
        AttributeMap = 1 << 11,
        InteractionObserver = 1 << 12,
        PseudoElements = 1 << 13,
        Animations = 1 << 14,
    };
#endif

    enum class Type { Element, Node };

    NodeRareData(Type type = Type::Node)
        : m_connectedFrameCount(0)
        , m_isElementRareData(type == Type::Element)
    {
    }

    bool isElementRareData() { return m_isElementRareData; }

    void clearNodeLists() { m_nodeLists = nullptr; }
    NodeListsNodeData* nodeLists() const { return m_nodeLists.get(); }
    NodeListsNodeData& ensureNodeLists()
    {
        if (!m_nodeLists)
            m_nodeLists = makeUnique<NodeListsNodeData>();
        return *m_nodeLists;
    }

    NodeMutationObserverData* mutationObserverData() { return m_mutationObserverData.get(); }
    NodeMutationObserverData& ensureMutationObserverData()
    {
        if (!m_mutationObserverData)
            m_mutationObserverData = makeUnique<NodeMutationObserverData>();
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

#if DUMP_NODE_STATISTICS
    OptionSet<UseType> useTypes() const
    {
        OptionSet<UseType> result;
        if (m_connectedFrameCount)
            result.add(UseType::ConnectedFrameCount);
        if (m_nodeLists)
            result.add(UseType::NodeList);
        if (m_mutationObserverData)
            result.add(UseType::MutationObserver);
        return result;
    }
#endif

private:
    unsigned m_connectedFrameCount : 31; // Must fit Page::maxNumberOfFrames.
    unsigned m_isElementRareData : 1;

    std::unique_ptr<NodeListsNodeData> m_nodeLists;
    std::unique_ptr<NodeMutationObserverData> m_mutationObserverData;
};

template<> struct NodeListTypeIdentifier<NameNodeList> {
    static constexpr unsigned char value() { return 0; }
};

template<> struct NodeListTypeIdentifier<RadioNodeList> {
    static constexpr unsigned char value() { return 1; }
};

template<> struct NodeListTypeIdentifier<LabelsNodeList> {
    static constexpr unsigned char value() { return 2; }
};

inline bool NodeListsNodeData::deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(Node& ownerNode)
{
    ASSERT(ownerNode.nodeLists() == this);
    size_t listsCount = (m_childNodeList ? 1 : 0)
        + (m_emptyChildNodeList ? 1 : 0)
        + m_atomNameCaches.size()
        + m_tagCollectionNSCache.size()
        + m_cachedCollections.size();
    if (listsCount != 1)
        return false;
    ownerNode.clearNodeLists();
    return true;
}

inline NodeRareData& Node::ensureRareData()
{
    if (!hasRareData())
        materializeRareData();
    return *rareData();
}

} // namespace WebCore
