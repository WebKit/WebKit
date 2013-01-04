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
#include "DOMSettableTokenList.h"
#include "LiveNodeList.h"
#include "MutationObserver.h"
#include "MutationObserverRegistration.h"
#include "QualifiedName.h"
#include "TagNodeList.h"
#if ENABLE(VIDEO_TRACK)
#include "TextTrack.h"
#endif
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/StringHash.h>

#if ENABLE(MICRODATA)
#include "HTMLPropertiesCollection.h"
#include "MicroDataItemList.h"
#endif

namespace WebCore {

class LabelsNodeList;
class RadioNodeList;
class TreeScope;

class NodeListsNodeData {
    WTF_MAKE_NONCOPYABLE(NodeListsNodeData); WTF_MAKE_FAST_ALLOCATED;
public:
    void clearChildNodeListCache()
    {
        if (m_childNodeList)
            m_childNodeList->invalidateCache();
    }

    PassRefPtr<ChildNodeList> ensureChildNodeList(Node* node)
    {
        if (m_childNodeList)
            return m_childNodeList;
        RefPtr<ChildNodeList> list = ChildNodeList::create(node);
        m_childNodeList = list.get();
        return list.release();
    }

    void removeChildNodeList(ChildNodeList* list)
    {
        ASSERT_UNUSED(list, m_childNodeList = list);
        m_childNodeList = 0;
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

    typedef HashMap<std::pair<unsigned char, AtomicString>, LiveNodeListBase*, NodeListCacheMapEntryHash<AtomicString> > NodeListAtomicNameCacheMap;
    typedef HashMap<std::pair<unsigned char, String>, LiveNodeListBase*, NodeListCacheMapEntryHash<String> > NodeListNameCacheMap;
    typedef HashMap<QualifiedName, TagNodeList*> TagNodeListCacheNS;

    template<typename T>
    PassRefPtr<T> addCacheWithAtomicName(Node* node, CollectionType collectionType, const AtomicString& name)
    {
        NodeListAtomicNameCacheMap::AddResult result = m_atomicNameCaches.add(namedNodeListKey(collectionType, name), 0);
        if (!result.isNewEntry)
            return static_cast<T*>(result.iterator->value);

        RefPtr<T> list = T::create(node, collectionType, name);
        result.iterator->value = list.get();
        return list.release();
    }

    // FIXME: This function should be renamed since it doesn't have an atomic name.
    template<typename T>
    PassRefPtr<T> addCacheWithAtomicName(Node* node, CollectionType collectionType)
    {
        NodeListAtomicNameCacheMap::AddResult result = m_atomicNameCaches.add(namedNodeListKey(collectionType, starAtom), 0);
        if (!result.isNewEntry)
            return static_cast<T*>(result.iterator->value);

        RefPtr<T> list = T::create(node, collectionType);
        result.iterator->value = list.get();
        return list.release();
    }

    template<typename T>
    T* cacheWithAtomicName(CollectionType collectionType)
    {
        return static_cast<T*>(m_atomicNameCaches.get(namedNodeListKey(collectionType, starAtom)));
    }

    template<typename T>
    PassRefPtr<T> addCacheWithName(Node* node, CollectionType collectionType, const String& name)
    {
        NodeListNameCacheMap::AddResult result = m_nameCaches.add(namedNodeListKey(collectionType, name), 0);
        if (!result.isNewEntry)
            return static_cast<T*>(result.iterator->value);

        RefPtr<T> list = T::create(node, name);
        result.iterator->value = list.get();
        return list.release();
    }

    PassRefPtr<TagNodeList> addCacheWithQualifiedName(Node* node, const AtomicString& namespaceURI, const AtomicString& localName)
    {
        QualifiedName name(nullAtom, localName, namespaceURI);
        TagNodeListCacheNS::AddResult result = m_tagNodeListCacheNS.add(name, 0);
        if (!result.isNewEntry)
            return result.iterator->value;

        RefPtr<TagNodeList> list = TagNodeList::create(node, namespaceURI, localName);
        result.iterator->value = list.get();
        return list.release();
    }

    void removeCacheWithAtomicName(LiveNodeListBase* list, CollectionType collectionType, const AtomicString& name = starAtom)
    {
        ASSERT_UNUSED(list, list == m_atomicNameCaches.get(namedNodeListKey(collectionType, name)));
        m_atomicNameCaches.remove(namedNodeListKey(collectionType, name));
    }

    void removeCacheWithName(LiveNodeListBase* list, CollectionType collectionType, const String& name)
    {
        ASSERT_UNUSED(list, list == m_nameCaches.get(namedNodeListKey(collectionType, name)));
        m_nameCaches.remove(namedNodeListKey(collectionType, name));
    }

    void removeCacheWithQualifiedName(LiveNodeList* list, const AtomicString& namespaceURI, const AtomicString& localName)
    {
        QualifiedName name(nullAtom, localName, namespaceURI);
        ASSERT_UNUSED(list, list == m_tagNodeListCacheNS.get(name));
        m_tagNodeListCacheNS.remove(name);
    }

    static PassOwnPtr<NodeListsNodeData> create()
    {
        return adoptPtr(new NodeListsNodeData);
    }

    void invalidateCaches(const QualifiedName* attrName = 0);
    bool isEmpty() const
    {
        return m_atomicNameCaches.isEmpty() && m_nameCaches.isEmpty() && m_tagNodeListCacheNS.isEmpty();
    }

    void adoptTreeScope(Document* oldDocument, Document* newDocument)
    {
        invalidateCaches();

        if (oldDocument != newDocument) {
            NodeListAtomicNameCacheMap::const_iterator atomicNameCacheEnd = m_atomicNameCaches.end();
            for (NodeListAtomicNameCacheMap::const_iterator it = m_atomicNameCaches.begin(); it != atomicNameCacheEnd; ++it) {
                LiveNodeListBase* list = it->value;
                oldDocument->unregisterNodeList(list);
                newDocument->registerNodeList(list);
            }

            NodeListNameCacheMap::const_iterator nameCacheEnd = m_nameCaches.end();
            for (NodeListNameCacheMap::const_iterator it = m_nameCaches.begin(); it != nameCacheEnd; ++it) {
                LiveNodeListBase* list = it->value;
                oldDocument->unregisterNodeList(list);
                newDocument->registerNodeList(list);
            }

            TagNodeListCacheNS::const_iterator tagEnd = m_tagNodeListCacheNS.end();
            for (TagNodeListCacheNS::const_iterator it = m_tagNodeListCacheNS.begin(); it != tagEnd; ++it) {
                LiveNodeListBase* list = it->value;
                ASSERT(!list->isRootedAtDocument());
                oldDocument->unregisterNodeList(list);
                newDocument->registerNodeList(list);
            }
        }
    }

    void reportMemoryUsage(MemoryObjectInfo*) const;

private:
    NodeListsNodeData()
        : m_childNodeList(0)
    { }

    std::pair<unsigned char, AtomicString> namedNodeListKey(CollectionType type, const AtomicString& name)
    {
        return std::pair<unsigned char, AtomicString>(type, name);
    }

    std::pair<unsigned char, String> namedNodeListKey(CollectionType type, const String& name)
    {
        return std::pair<unsigned char, String>(type, name);
    }

    // FIXME: m_childNodeList should be merged into m_atomicNameCaches or at least be shared with HTMLCollection returned by Element::children
    // but it's tricky because invalidateCaches shouldn't invalidate this cache and adoptTreeScope shouldn't call registerNodeList or unregisterNodeList.
    ChildNodeList* m_childNodeList;
    NodeListAtomicNameCacheMap m_atomicNameCaches;
    NodeListNameCacheMap m_nameCaches;
    TagNodeListCacheNS m_tagNodeListCacheNS;
};

class NodeRareData : public NodeRareDataBase {
    WTF_MAKE_NONCOPYABLE(NodeRareData); WTF_MAKE_FAST_ALLOCATED;

    struct NodeMutationObserverData {
        Vector<OwnPtr<MutationObserverRegistration> > m_registry;
        HashSet<MutationObserverRegistration*> m_transientRegistry;

        static PassOwnPtr<NodeMutationObserverData> create() { return adoptPtr(new NodeMutationObserverData); }
    };

#if ENABLE(MICRODATA)
    struct NodeMicroDataTokenLists {
        RefPtr<DOMSettableTokenList> m_itemProp;
        RefPtr<DOMSettableTokenList> m_itemRef;
        RefPtr<DOMSettableTokenList> m_itemType;

        static PassOwnPtr<NodeMicroDataTokenLists> create() { return adoptPtr(new NodeMicroDataTokenLists); }
    };
#endif

public:    
    NodeRareData()
        : m_tabIndex(0)
        , m_childIndex(0)
        , m_tabIndexWasSetExplicitly(false)
        , m_needsFocusAppearanceUpdateSoonAfterAttach(false)
        , m_styleAffectedByEmpty(false)
        , m_isInCanvasSubtree(false)
#if ENABLE(FULLSCREEN_API)
        , m_containsFullScreenElement(false)
#endif
#if ENABLE(DIALOG_ELEMENT)
        , m_isInTopLayer(false)
#endif
        , m_childrenAffectedByHover(false)
        , m_childrenAffectedByActive(false)
        , m_childrenAffectedByDrag(false)
        , m_childrenAffectedByFirstChildRules(false)
        , m_childrenAffectedByLastChildRules(false)
        , m_childrenAffectedByDirectAdjacentRules(false)
        , m_childrenAffectedByForwardPositionalRules(false)
        , m_childrenAffectedByBackwardPositionalRules(false)
#if ENABLE(VIDEO_TRACK)
        , m_WebVTTNodeType(TextTrack::WebVTTNodeTypeNone)
#endif
    {
    }

    virtual ~NodeRareData()
    {
    }

    void clearNodeLists() { m_nodeLists.clear(); }
    NodeListsNodeData* nodeLists() const { return m_nodeLists.get(); }
    NodeListsNodeData* ensureNodeLists()
    {
        if (!m_nodeLists)
            m_nodeLists = NodeListsNodeData::create();
        return m_nodeLists.get();
    }

    short tabIndex() const { return m_tabIndex; }
    void setTabIndexExplicitly(short index) { m_tabIndex = index; m_tabIndexWasSetExplicitly = true; }
    bool tabIndexSetExplicitly() const { return m_tabIndexWasSetExplicitly; }
    void clearTabIndexExplicitly() { m_tabIndex = 0; m_tabIndexWasSetExplicitly = false; }

    Vector<OwnPtr<MutationObserverRegistration> >* mutationObserverRegistry() { return m_mutationObserverData ? &m_mutationObserverData->m_registry : 0; }
    Vector<OwnPtr<MutationObserverRegistration> >* ensureMutationObserverRegistry()
    {
        if (!m_mutationObserverData)
            m_mutationObserverData = NodeMutationObserverData::create();
        return &m_mutationObserverData->m_registry;
    }

    HashSet<MutationObserverRegistration*>* transientMutationObserverRegistry() { return m_mutationObserverData ? &m_mutationObserverData->m_transientRegistry : 0; }
    HashSet<MutationObserverRegistration*>* ensureTransientMutationObserverRegistry()
    {
        if (!m_mutationObserverData)
            m_mutationObserverData = NodeMutationObserverData::create();
        return &m_mutationObserverData->m_transientRegistry;
    }

#if ENABLE(MICRODATA)
    NodeMicroDataTokenLists* ensureMicroDataTokenLists() const
    {
        if (!m_microDataTokenLists)
            m_microDataTokenLists = NodeMicroDataTokenLists::create();
        return m_microDataTokenLists.get();
    }

    DOMSettableTokenList* itemProp() const
    {
        if (!ensureMicroDataTokenLists()->m_itemProp)
            m_microDataTokenLists->m_itemProp = DOMSettableTokenList::create();

        return m_microDataTokenLists->m_itemProp.get();
    }

    void setItemProp(const String& value)
    {
        if (!ensureMicroDataTokenLists()->m_itemProp)
            m_microDataTokenLists->m_itemProp = DOMSettableTokenList::create();

        m_microDataTokenLists->m_itemProp->setValue(value);
    }

    DOMSettableTokenList* itemRef() const
    {
        if (!ensureMicroDataTokenLists()->m_itemRef)
            m_microDataTokenLists->m_itemRef = DOMSettableTokenList::create();

        return m_microDataTokenLists->m_itemRef.get();
    }

    void setItemRef(const String& value)
    {
        if (!ensureMicroDataTokenLists()->m_itemRef)
            m_microDataTokenLists->m_itemRef = DOMSettableTokenList::create();

        m_microDataTokenLists->m_itemRef->setValue(value);
    }

    DOMSettableTokenList* itemType() const
    {
        if (!ensureMicroDataTokenLists()->m_itemType)
            m_microDataTokenLists->m_itemType = DOMSettableTokenList::create();

        return m_microDataTokenLists->m_itemType.get();
    }

    void setItemType(const String& value)
    {
        if (!ensureMicroDataTokenLists()->m_itemType)
            m_microDataTokenLists->m_itemType = DOMSettableTokenList::create();

        m_microDataTokenLists->m_itemType->setValue(value);
    }
#endif

    virtual void reportMemoryUsage(MemoryObjectInfo*) const;

protected:
#if ENABLE(VIDEO_TRACK)
    bool isWebVTTNode() { return m_WebVTTNodeType != TextTrack::WebVTTNodeTypeNone; }
    void setIsWebVTTNode() { m_WebVTTNodeType = TextTrack::WebVTTNodeTypePast; }
    bool isWebVTTFutureNode() { return m_WebVTTNodeType == TextTrack::WebVTTNodeTypeFuture; }
    void setIsWebVTTFutureNode() { m_WebVTTNodeType = TextTrack::WebVTTNodeTypeFuture; }
#endif
    short m_tabIndex;
    unsigned short m_childIndex;
    bool m_tabIndexWasSetExplicitly : 1;
    bool m_needsFocusAppearanceUpdateSoonAfterAttach : 1;
    bool m_styleAffectedByEmpty : 1;
    bool m_isInCanvasSubtree : 1;
#if ENABLE(FULLSCREEN_API)
    bool m_containsFullScreenElement : 1;
#endif
#if ENABLE(DIALOG_ELEMENT)
    bool m_isInTopLayer : 1;
#endif
    bool m_childrenAffectedByHover : 1;
    bool m_childrenAffectedByActive : 1;
    bool m_childrenAffectedByDrag : 1;
    // Bits for dynamic child matching.
    // We optimize for :first-child and :last-child. The other positional child selectors like nth-child or
    // *-child-of-type, we will just give up and re-evaluate whenever children change at all.
    bool m_childrenAffectedByFirstChildRules : 1;
    bool m_childrenAffectedByLastChildRules : 1;
    bool m_childrenAffectedByDirectAdjacentRules : 1;
    bool m_childrenAffectedByForwardPositionalRules : 1;
    bool m_childrenAffectedByBackwardPositionalRules : 1;
#if ENABLE(VIDEO_TRACK)
    TextTrack::WebVTTNodeType m_WebVTTNodeType : 2;
#endif
private:
    OwnPtr<NodeListsNodeData> m_nodeLists;
    OwnPtr<NodeMutationObserverData> m_mutationObserverData;

#if ENABLE(MICRODATA)
    mutable OwnPtr<NodeMicroDataTokenLists> m_microDataTokenLists;
#endif
};

} // namespace WebCore

#endif // NodeRareData_h
