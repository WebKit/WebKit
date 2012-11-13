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
#include "DynamicNodeList.h"
#include "MutationObserverRegistration.h"
#include "NameNodeList.h"
#include "QualifiedName.h"
#include "TagNodeList.h"
#include "WebKitMutationObserver.h"
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
class TreeScope;

struct NodeListsNodeData {
    WTF_MAKE_NONCOPYABLE(NodeListsNodeData); WTF_MAKE_FAST_ALLOCATED;
public:
    typedef HashSet<DynamicSubtreeNodeList*> NodeListSet;
    NodeListSet m_listsInvalidatedAtDocument;

    typedef HashMap<String, ClassNodeList*> ClassNodeListCache;
    ClassNodeListCache m_classNodeListCache;

    typedef HashMap<String, NameNodeList*> NameNodeListCache;
    NameNodeListCache m_nameNodeListCache;
 
    typedef HashMap<AtomicString, TagNodeList*> TagNodeListCache;
    TagNodeListCache m_tagNodeListCache;

    typedef HashMap<RefPtr<QualifiedName::QualifiedNameImpl>, TagNodeList*> TagNodeListCacheNS;
    TagNodeListCacheNS m_tagNodeListCacheNS;

#if ENABLE(MICRODATA)
    typedef HashMap<String, MicroDataItemList*> MicroDataItemListCache;
    MicroDataItemListCache m_microDataItemListCache;
#endif

    LabelsNodeList* m_labelsNodeListCache;

    static PassOwnPtr<NodeListsNodeData> create()
    {
        return adoptPtr(new NodeListsNodeData);
    }

    void invalidateCaches();
    void invalidateCachesThatDependOnAttributes();

    bool isEmpty() const;

private:
    NodeListsNodeData() : m_labelsNodeListCache(0) {}
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
#if ENABLE(STYLE_SCOPED)
        , m_numberOfScopedHTMLStyleChildren(0)
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
    NodeListsNodeData* ensureNodeLists(Node* node)
    {
        if (!m_nodeLists)
            createNodeLists(node);
        return m_nodeLists.get();
    }
    void clearChildNodeListCache();

    ChildNodeList* childNodeList() const { return m_childNodeList; }
    void setChildNodeList(ChildNodeList* list) { m_childNodeList = list; }

    short tabIndex() const { return m_tabIndex; }
    void setTabIndexExplicitly(short index) { m_tabIndex = index; m_tabIndexWasSetExplicitly = true; }
    bool tabIndexSetExplicitly() const { return m_tabIndexWasSetExplicitly; }
    void clearTabIndexExplicitly() { m_tabIndex = 0; m_tabIndexWasSetExplicitly = false; }

    EventTargetData* eventTargetData() { return m_eventTargetData.get(); }
    EventTargetData* ensureEventTargetData()
    {
        if (!m_eventTargetData)
            m_eventTargetData = adoptPtr(new EventTargetData);
        return m_eventTargetData.get();
    }

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

    HTMLPropertiesCollection* properties(Node* node)
    {
        if (!m_properties)
            m_properties = HTMLPropertiesCollection::create(node);

        return m_properties.get();
    }
#endif

#if ENABLE(STYLE_SCOPED)
    void registerScopedHTMLStyleChild()
    {
        ++m_numberOfScopedHTMLStyleChildren;
    }

    void unregisterScopedHTMLStyleChild()
    {
        ASSERT(m_numberOfScopedHTMLStyleChildren > 0);
        if (m_numberOfScopedHTMLStyleChildren > 0)
            --m_numberOfScopedHTMLStyleChildren;
    }

    bool hasScopedHTMLStyleChild() const
    {
        return m_numberOfScopedHTMLStyleChildren;
    }

    size_t numberOfScopedHTMLStyleChildren() const
    {
        return m_numberOfScopedHTMLStyleChildren;
    }
#endif

    bool isFocused() const { return m_isFocused; }
    void setFocused(bool focused) { m_isFocused = focused; }

protected:
    // for ElementRareData
    bool needsFocusAppearanceUpdateSoonAfterAttach() const { return m_needsFocusAppearanceUpdateSoonAfterAttach; }
    void setNeedsFocusAppearanceUpdateSoonAfterAttach(bool needs) { m_needsFocusAppearanceUpdateSoonAfterAttach = needs; }

private:
    void createNodeLists(Node*);

    TreeScope* m_treeScope;
    OwnPtr<NodeListsNodeData> m_nodeLists;
    ChildNodeList* m_childNodeList;
    OwnPtr<EventTargetData> m_eventTargetData;
    short m_tabIndex;
    bool m_tabIndexWasSetExplicitly : 1;
    bool m_isFocused : 1;
    bool m_needsFocusAppearanceUpdateSoonAfterAttach : 1;

#if ENABLE(MUTATION_OBSERVERS)
    OwnPtr<Vector<OwnPtr<MutationObserverRegistration> > > m_mutationObserverRegistry;
    OwnPtr<HashSet<MutationObserverRegistration*> > m_transientMutationObserverRegistry;
#endif

#if ENABLE(MICRODATA)
    mutable RefPtr<DOMSettableTokenList> m_itemProp;
    mutable RefPtr<DOMSettableTokenList> m_itemRef;
    mutable RefPtr<DOMSettableTokenList> m_itemType;
    mutable OwnPtr<HTMLPropertiesCollection> m_properties;
#endif

#if ENABLE(STYLE_SCOPED)
    size_t m_numberOfScopedHTMLStyleChildren;
#endif
};

} // namespace WebCore

#endif // NodeRareData_h
