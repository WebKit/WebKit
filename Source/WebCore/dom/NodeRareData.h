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

#ifndef NodeRareData_h
#define NodeRareData_h

#include "ChildNodeList.h"
#include "ClassNodeList.h"
#include "DOMSettableTokenList.h"
#include "HTMLCollection.h"
#include "HTMLNames.h"
#include "LiveNodeList.h"
#include "MutationObserver.h"
#include "MutationObserverRegistration.h"
#include "Page.h"
#include "QualifiedName.h"
#include "TagNodeList.h"
#if ENABLE(VIDEO_TRACK)
#include "TextTrack.h"
#endif
#include <memory>
#include <wtf/HashSet.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class LabelsNodeList;
class RadioNodeList;
class TreeScope;

class NodeListsNodeData {
    WTF_MAKE_NONCOPYABLE(NodeListsNodeData); WTF_MAKE_FAST_ALLOCATED;
public:
    NodeListsNodeData()
        : m_childNodeList(nullptr)
        , m_emptyChildNodeList(nullptr)
    { }

    void clearChildNodeListCache()
    {
        if (m_childNodeList)
            m_childNodeList->invalidateCache();
    }

    PassRefPtr<ChildNodeList> ensureChildNodeList(ContainerNode& node)
    {
        ASSERT(!m_emptyChildNodeList);
        if (m_childNodeList)
            return m_childNodeList;
        RefPtr<ChildNodeList> list = ChildNodeList::create(node);
        m_childNodeList = list.get();
        return list.release();
    }

    void removeChildNodeList(ChildNodeList* list)
    {
        ASSERT(m_childNodeList == list);
        if (deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(list->ownerNode()))
            return;
        m_childNodeList = nullptr;
    }

    PassRefPtr<EmptyNodeList> ensureEmptyChildNodeList(Node& node)
    {
        ASSERT(!m_childNodeList);
        if (m_emptyChildNodeList)
            return m_emptyChildNodeList;
        RefPtr<EmptyNodeList> list = EmptyNodeList::create(node);
        m_emptyChildNodeList = list.get();
        return list.release();
    }

    void removeEmptyChildNodeList(EmptyNodeList* list)
    {
        ASSERT(m_emptyChildNodeList == list);
        if (deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(list->ownerNode()))
            return;
        m_emptyChildNodeList = nullptr;
    }

    template <typename StringType>
    struct NodeListCacheMapEntryHash {
        static unsigned hash(const std::pair<unsigned char, StringType>& entry)
        {
            return DefaultHash<StringType>::Hash::hash(entry.second) + entry.first;
        }
        static bool equal(const std::pair<unsigned char, StringType>& a, const std::pair<unsigned char, StringType>& b) { return a == b; }
        static const bool safeToCompareToEmptyOrDeleted = DefaultHash<StringType>::Hash::safeToCompareToEmptyOrDeleted;
    };

    typedef HashMap<std::pair<unsigned char, AtomicString>, LiveNodeList*, NodeListCacheMapEntryHash<AtomicString>> NodeListAtomicNameCacheMap;
    typedef HashMap<std::pair<unsigned char, String>, LiveNodeList*, NodeListCacheMapEntryHash<String>> NodeListNameCacheMap;
    typedef HashMap<std::pair<unsigned char, AtomicString>, HTMLCollection*, NodeListCacheMapEntryHash<AtomicString>> CollectionCacheMap;
    typedef HashMap<QualifiedName, TagNodeList*> TagNodeListCacheNS;

    template<typename T, typename ContainerType>
    PassRefPtr<T> addCacheWithAtomicName(ContainerType& container, LiveNodeList::Type type, const AtomicString& name)
    {
        NodeListAtomicNameCacheMap::AddResult result = m_atomicNameCaches.add(namedNodeListKey(type, name), nullptr);
        if (!result.isNewEntry)
            return static_cast<T*>(result.iterator->value);

        RefPtr<T> list = T::create(container, type, name);
        result.iterator->value = list.get();
        return list.release();
    }

    template<typename T>
    PassRefPtr<T> addCacheWithName(ContainerNode& node, LiveNodeList::Type type, const String& name)
    {
        NodeListNameCacheMap::AddResult result = m_nameCaches.add(namedNodeListKey(type, name), nullptr);
        if (!result.isNewEntry)
            return static_cast<T*>(result.iterator->value);

        RefPtr<T> list = T::create(node, name);
        result.iterator->value = list.get();
        return list.release();
    }

    PassRefPtr<TagNodeList> addCacheWithQualifiedName(ContainerNode& node, const AtomicString& namespaceURI, const AtomicString& localName)
    {
        QualifiedName name(nullAtom, localName, namespaceURI);
        TagNodeListCacheNS::AddResult result = m_tagNodeListCacheNS.add(name, nullptr);
        if (!result.isNewEntry)
            return result.iterator->value;

        RefPtr<TagNodeList> list = TagNodeList::create(node, namespaceURI, localName);
        result.iterator->value = list.get();
        return list.release();
    }

    template<typename T, typename ContainerType>
    PassRefPtr<T> addCachedCollection(ContainerType& container, CollectionType collectionType, const AtomicString& name)
    {
        CollectionCacheMap::AddResult result = m_cachedCollections.add(namedCollectionKey(collectionType, name), nullptr);
        if (!result.isNewEntry)
            return static_cast<T*>(result.iterator->value);

        RefPtr<T> list = T::create(container, collectionType, name);
        result.iterator->value = list.get();
        return list.release();
    }

    template<typename T, typename ContainerType>
    PassRefPtr<T> addCachedCollection(ContainerType& container, CollectionType collectionType)
    {
        CollectionCacheMap::AddResult result = m_cachedCollections.add(namedCollectionKey(collectionType, starAtom), nullptr);
        if (!result.isNewEntry)
            return static_cast<T*>(result.iterator->value);

        RefPtr<T> list = T::create(container, collectionType);
        result.iterator->value = list.get();
        return list.release();
    }

    template<typename T>
    T* cachedCollection(CollectionType collectionType)
    {
        return static_cast<T*>(m_cachedCollections.get(namedCollectionKey(collectionType, starAtom)));
    }

    void removeCacheWithAtomicName(LiveNodeList* list, const AtomicString& name = starAtom)
    {
        ASSERT(list == m_atomicNameCaches.get(namedNodeListKey(list->type(), name)));
        if (deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(list->ownerNode()))
            return;
        m_atomicNameCaches.remove(namedNodeListKey(list->type(), name));
    }

    void removeCacheWithName(LiveNodeList* list, const String& name)
    {
        ASSERT(list == m_nameCaches.get(namedNodeListKey(list->type(), name)));
        if (deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(list->ownerNode()))
            return;
        m_nameCaches.remove(namedNodeListKey(list->type(), name));
    }

    void removeCacheWithQualifiedName(LiveNodeList* list, const AtomicString& namespaceURI, const AtomicString& localName)
    {
        QualifiedName name(nullAtom, localName, namespaceURI);
        ASSERT(list == m_tagNodeListCacheNS.get(name));
        if (deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(list->ownerNode()))
            return;
        m_tagNodeListCacheNS.remove(name);
    }

    void removeCachedCollection(HTMLCollection* collection, const AtomicString& name = starAtom)
    {
        ASSERT(collection == m_cachedCollections.get(namedCollectionKey(collection->type(), name)));
        if (deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(collection->ownerNode()))
            return;
        m_cachedCollections.remove(namedCollectionKey(collection->type(), name));
    }

    void invalidateCaches(const QualifiedName* attrName = 0);
    bool isEmpty() const
    {
        return m_atomicNameCaches.isEmpty() && m_nameCaches.isEmpty() && m_cachedCollections.isEmpty() && m_tagNodeListCacheNS.isEmpty();
    }

    void adoptTreeScope()
    {
        invalidateCaches();
    }

    void adoptDocument(Document* oldDocument, Document* newDocument)
    {
        invalidateCaches();

        if (oldDocument != newDocument) {
            for (auto it = m_atomicNameCaches.begin(), end = m_atomicNameCaches.end(); it != end; ++it) {
                LiveNodeList& list = *it->value;
                oldDocument->unregisterNodeList(list);
                newDocument->registerNodeList(list);
            }

            for (auto it = m_nameCaches.begin(), end = m_nameCaches.end(); it != end; ++it) {
                LiveNodeList& list = *it->value;
                oldDocument->unregisterNodeList(list);
                newDocument->registerNodeList(list);
            }

            for (auto it = m_tagNodeListCacheNS.begin(), end = m_tagNodeListCacheNS.end(); it != end; ++it) {
                LiveNodeList& list = *it->value;
                ASSERT(!list.isRootedAtDocument());
                oldDocument->unregisterNodeList(list);
                newDocument->registerNodeList(list);
            }

            for (auto it = m_cachedCollections.begin(), end = m_cachedCollections.end(); it != end; ++it) {
                HTMLCollection& collection = *it->value;
                oldDocument->unregisterCollection(collection);
                newDocument->registerCollection(collection);
            }
        }
    }

private:
    std::pair<unsigned char, AtomicString> namedCollectionKey(CollectionType type, const AtomicString& name)
    {
        return std::pair<unsigned char, AtomicString>(type, name);
    }

    std::pair<unsigned char, String> namedNodeListKey(LiveNodeList::Type type, const String& name)
    {
        return std::pair<unsigned char, String>(type, name);
    }

    bool deleteThisAndUpdateNodeRareDataIfAboutToRemoveLastList(Node&);

    // These two are currently mutually exclusive and could be unioned. Not very important as this class is large anyway.
    ChildNodeList* m_childNodeList;
    EmptyNodeList* m_emptyChildNodeList;

    NodeListAtomicNameCacheMap m_atomicNameCaches;
    NodeListNameCacheMap m_nameCaches;
    TagNodeListCacheNS m_tagNodeListCacheNS;
    CollectionCacheMap m_cachedCollections;
};

class NodeMutationObserverData {
    WTF_MAKE_NONCOPYABLE(NodeMutationObserverData); WTF_MAKE_FAST_ALLOCATED;
public:
    NodeMutationObserverData() = default;
    Vector<std::unique_ptr<MutationObserverRegistration>> registry;
    HashSet<MutationObserverRegistration*> transientRegistry;
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
    if ((m_childNodeList ? 1 : 0) + (m_emptyChildNodeList ? 1 : 0) + m_atomicNameCaches.size() + m_nameCaches.size()
        + m_tagNodeListCacheNS.size() + m_cachedCollections.size() != 1)
        return false;
    ownerNode.clearNodeLists();
    return true;
}

// Ensure the 10 bits reserved for the m_connectedFrameCount cannot overflow
COMPILE_ASSERT(Page::maxNumberOfFrames < 1024, Frame_limit_should_fit_in_rare_data_count);

} // namespace WebCore

#endif // NodeRareData_h
