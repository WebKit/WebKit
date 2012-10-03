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
#include "DynamicNodeList.h"
#include "MutationObserver.h"
#include "MutationObserverRegistration.h"
#include "QualifiedName.h"
#include "TagNodeList.h"
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
    template <typename StringType>
    struct NodeListCacheMapEntryHash {
        static unsigned hash(const std::pair<unsigned char, StringType>& entry)
        {
            return DefaultHash<StringType>::Hash::hash(entry.second) + entry.first;
        }
        static bool equal(const std::pair<unsigned char, StringType>& a, const std::pair<unsigned char, StringType>& b) { return a == b; }
        static const bool safeToCompareToEmptyOrDeleted = DefaultHash<StringType>::Hash::safeToCompareToEmptyOrDeleted;
    };

    typedef HashMap<std::pair<unsigned char, AtomicString>, DynamicSubtreeNodeList*, NodeListCacheMapEntryHash<AtomicString> > NodeListAtomicNameCacheMap;
    typedef HashMap<std::pair<unsigned char, String>, DynamicSubtreeNodeList*, NodeListCacheMapEntryHash<String> > NodeListNameCacheMap;
    typedef HashMap<QualifiedName, TagNodeList*> TagNodeListCacheNS;

    template<typename T>
    PassRefPtr<T> addCacheWithAtomicName(Node* node, DynamicNodeList::NodeListType listType, const AtomicString& name)
    {
        NodeListAtomicNameCacheMap::AddResult result = m_atomicNameCaches.add(namedNodeListKey(listType, name), 0);
        if (!result.isNewEntry)
            return static_cast<T*>(result.iterator->second);

        RefPtr<T> list = T::create(node, name);
        result.iterator->second = list.get();
        return list.release();
    }

    template<typename T>
    PassRefPtr<T> addCacheWithName(Node* node, DynamicNodeList::NodeListType listType, const String& name)
    {
        NodeListNameCacheMap::AddResult result = m_nameCaches.add(namedNodeListKey(listType, name), 0);
        if (!result.isNewEntry)
            return static_cast<T*>(result.iterator->second);

        RefPtr<T> list = T::create(node, name);
        result.iterator->second = list.get();
        return list.release();
    }

    PassRefPtr<TagNodeList> addCacheWithQualifiedName(Node* node, const AtomicString& namespaceURI, const AtomicString& localName)
    {
        QualifiedName name(nullAtom, localName, namespaceURI);
        TagNodeListCacheNS::AddResult result = m_tagNodeListCacheNS.add(name, 0);
        if (!result.isNewEntry)
            return result.iterator->second;

        RefPtr<TagNodeList> list = TagNodeList::create(node, namespaceURI, localName);
        result.iterator->second = list.get();
        return list.release();
    }

    void removeCacheWithAtomicName(DynamicSubtreeNodeList* list, DynamicNodeList::NodeListType listType, const AtomicString& name)
    {
        ASSERT_UNUSED(list, list == m_atomicNameCaches.get(namedNodeListKey(listType, name)));
        m_atomicNameCaches.remove(namedNodeListKey(listType, name));
    }

    void removeCacheWithName(DynamicSubtreeNodeList* list, DynamicNodeList::NodeListType listType, const String& name)
    {
        ASSERT_UNUSED(list, list == m_nameCaches.get(namedNodeListKey(listType, name)));
        m_nameCaches.remove(namedNodeListKey(listType, name));
    }

    void removeCacheWithQualifiedName(DynamicSubtreeNodeList* list, const AtomicString& namespaceURI, const AtomicString& localName)
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
                DynamicSubtreeNodeList* list = it->second;
                oldDocument->unregisterNodeListCache(list);
                newDocument->registerNodeListCache(list);
            }

            NodeListNameCacheMap::const_iterator nameCacheEnd = m_nameCaches.end();
            for (NodeListNameCacheMap::const_iterator it = m_nameCaches.begin(); it != nameCacheEnd; ++it) {
                DynamicSubtreeNodeList* list = it->second;
                oldDocument->unregisterNodeListCache(list);
                newDocument->registerNodeListCache(list);
            }

            TagNodeListCacheNS::const_iterator tagEnd = m_tagNodeListCacheNS.end();
            for (TagNodeListCacheNS::const_iterator it = m_tagNodeListCacheNS.begin(); it != tagEnd; ++it) {
                DynamicSubtreeNodeList* list = it->second;
                ASSERT(!list->isRootedAtDocument());
                oldDocument->unregisterNodeListCache(list);
                newDocument->registerNodeListCache(list);
            }
        }
    }

private:
    NodeListsNodeData() { }

    std::pair<unsigned char, AtomicString> namedNodeListKey(DynamicNodeList::NodeListType listType, const AtomicString& name)
    {
        return std::pair<unsigned char, AtomicString>(listType, name);
    }

    std::pair<unsigned char, String> namedNodeListKey(DynamicNodeList::NodeListType listType, const String& name)
    {
        return std::pair<unsigned char, String>(listType, name);
    }

    NodeListAtomicNameCacheMap m_atomicNameCaches;
    NodeListNameCacheMap m_nameCaches;
    TagNodeListCacheNS m_tagNodeListCacheNS;
};

class NodeRareData {
    WTF_MAKE_NONCOPYABLE(NodeRareData); WTF_MAKE_FAST_ALLOCATED;
public:    
    NodeRareData()
        : m_treeScope(0)
        , m_childNodeList(0)
        , m_tabIndex(0)
        , m_tabIndexWasSetExplicitly(false)
        , m_isFocused(false)
        , m_needsFocusAppearanceUpdateSoonAfterAttach(false)
        , m_styleAffectedByEmpty(false)
        , m_isInCanvasSubtree(false)
#if ENABLE(FULLSCREEN_API)
        , m_containsFullScreenElement(false)
#endif
    {
    }

    virtual ~NodeRareData()
    {
    }

    typedef HashMap<const Node*, NodeRareData*> NodeRareDataMap;
    
    static NodeRareDataMap& rareDataMap()
    {
        static NodeRareDataMap* dataMap = new NodeRareDataMap;
        return *dataMap;
    }
    
    static NodeRareData* rareDataFromMap(const Node* node)
    {
        return rareDataMap().get(node);
    }

    TreeScope* treeScope() const { return m_treeScope; }
    void setTreeScope(TreeScope* treeScope) { m_treeScope = treeScope; }
    
    void clearNodeLists() { m_nodeLists.clear(); }
    void setNodeLists(PassOwnPtr<NodeListsNodeData> lists) { m_nodeLists = lists; }
    NodeListsNodeData* nodeLists() const { return m_nodeLists.get(); }
    NodeListsNodeData* ensureNodeLists()
    {
        if (!m_nodeLists)
            setNodeLists(NodeListsNodeData::create());
        return m_nodeLists.get();
    }
    void clearChildNodeListCache()
    {
        if (m_childNodeList)
            m_childNodeList->invalidateCache();
    }

    ChildNodeList* childNodeList() const { return m_childNodeList; }
    void setChildNodeList(ChildNodeList* list) { m_childNodeList = list; }

    short tabIndex() const { return m_tabIndex; }
    void setTabIndexExplicitly(short index) { m_tabIndex = index; m_tabIndexWasSetExplicitly = true; }
    bool tabIndexSetExplicitly() const { return m_tabIndexWasSetExplicitly; }
    void clearTabIndexExplicitly() { m_tabIndex = 0; m_tabIndexWasSetExplicitly = false; }

#if ENABLE(MUTATION_OBSERVERS)
    Vector<OwnPtr<MutationObserverRegistration> >* mutationObserverRegistry() { return m_mutationObserverRegistry.get(); }
    Vector<OwnPtr<MutationObserverRegistration> >* ensureMutationObserverRegistry()
    {
        if (!m_mutationObserverRegistry)
            m_mutationObserverRegistry = adoptPtr(new Vector<OwnPtr<MutationObserverRegistration> >);
        return m_mutationObserverRegistry.get();
    }

    HashSet<MutationObserverRegistration*>* transientMutationObserverRegistry() { return m_transientMutationObserverRegistry.get(); }
    HashSet<MutationObserverRegistration*>* ensureTransientMutationObserverRegistry()
    {
        if (!m_transientMutationObserverRegistry)
            m_transientMutationObserverRegistry = adoptPtr(new HashSet<MutationObserverRegistration*>);
        return m_transientMutationObserverRegistry.get();
    }
#endif

#if ENABLE(MICRODATA)
    DOMSettableTokenList* itemProp() const
    {
        if (!m_itemProp)
            m_itemProp = DOMSettableTokenList::create();

        return m_itemProp.get();
    }

    void setItemProp(const String& value)
    {
        if (!m_itemProp)
            m_itemProp = DOMSettableTokenList::create();

        m_itemProp->setValue(value);
    }

    DOMSettableTokenList* itemRef() const
    {
        if (!m_itemRef)
            m_itemRef = DOMSettableTokenList::create();

        return m_itemRef.get();
    }

    void setItemRef(const String& value)
    {
        if (!m_itemRef)
            m_itemRef = DOMSettableTokenList::create();

        m_itemRef->setValue(value);
    }

    DOMSettableTokenList* itemType() const
    {
        if (!m_itemType)
            m_itemType = DOMSettableTokenList::create();

        return m_itemType.get();
    }

    void setItemType(const String& value)
    {
        if (!m_itemType)
            m_itemType = DOMSettableTokenList::create();

        m_itemType->setValue(value);
    }
#endif

    bool isFocused() const { return m_isFocused; }
    void setFocused(bool focused) { m_isFocused = focused; }

protected:
    // for ElementRareData
    bool needsFocusAppearanceUpdateSoonAfterAttach() const { return m_needsFocusAppearanceUpdateSoonAfterAttach; }
    void setNeedsFocusAppearanceUpdateSoonAfterAttach(bool needs) { m_needsFocusAppearanceUpdateSoonAfterAttach = needs; }
    bool styleAffectedByEmpty() const { return m_styleAffectedByEmpty; }
    void setStyleAffectedByEmpty(bool value) { m_styleAffectedByEmpty = value; }
    bool isInCanvasSubtree() const { return m_isInCanvasSubtree; }
    void setIsInCanvasSubtree(bool value) { m_isInCanvasSubtree = value; }
#if ENABLE(FULLSCREEN_API)
    bool containsFullScreenElement() { return m_containsFullScreenElement; }
    void setContainsFullScreenElement(bool value) { m_containsFullScreenElement = value; }
#endif

private:
    TreeScope* m_treeScope;
    OwnPtr<NodeListsNodeData> m_nodeLists;
    ChildNodeList* m_childNodeList;
    short m_tabIndex;
    bool m_tabIndexWasSetExplicitly : 1;
    bool m_isFocused : 1;
    bool m_needsFocusAppearanceUpdateSoonAfterAttach : 1;
    bool m_styleAffectedByEmpty : 1;
    bool m_isInCanvasSubtree : 1;
#if ENABLE(FULLSCREEN_API)
    bool m_containsFullScreenElement : 1;
#endif

#if ENABLE(MUTATION_OBSERVERS)
    OwnPtr<Vector<OwnPtr<MutationObserverRegistration> > > m_mutationObserverRegistry;
    OwnPtr<HashSet<MutationObserverRegistration*> > m_transientMutationObserverRegistry;
#endif

#if ENABLE(MICRODATA)
    mutable RefPtr<DOMSettableTokenList> m_itemProp;
    mutable RefPtr<DOMSettableTokenList> m_itemRef;
    mutable RefPtr<DOMSettableTokenList> m_itemType;
#endif
};

} // namespace WebCore

#endif // NodeRareData_h
