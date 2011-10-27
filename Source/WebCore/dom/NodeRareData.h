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

#include "ClassNodeList.h"
#include "DynamicNodeList.h"

#if ENABLE(MICRODATA)
#include "MicroDataItemList.h"
#endif

#include "NameNodeList.h"
#include "QualifiedName.h"
#include "TagNodeList.h"
#include "WebKitMutationObserver.h"
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class LabelsNodeList;
class TreeScope;

struct NodeListsNodeData {
    WTF_MAKE_NONCOPYABLE(NodeListsNodeData); WTF_MAKE_FAST_ALLOCATED;
public:
    typedef HashSet<DynamicNodeList*> NodeListSet;
    NodeListSet m_listsWithCaches;
    
    RefPtr<DynamicNodeList::Caches> m_childNodeListCaches;
    
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

#if ENABLE(MICRODATA)
    void invalidateMicrodataItemListCaches();
#endif

    bool isEmpty() const;

private:
    NodeListsNodeData() : m_labelsNodeListCache(0) {}
};

#if ENABLE(MUTATION_OBSERVERS)
struct MutationObserverRegistration {
    MutationObserverRegistration(PassRefPtr<WebKitMutationObserver> observer, MutationObserverOptions options, Node* node)
        : observer(observer)
        , options(options)
        , registrationNode(node)
    {
    }

    bool operator==(const MutationObserverRegistration& other) const
    {
        return observer == other.observer && registrationNode == other.registrationNode;
    }

    bool hasAllOptions(MutationObserverOptions options) const
    {
        return (this->options & options) == options;
    }

    RefPtr<WebKitMutationObserver> observer;
    MutationObserverOptions options;

    // registrationNode will be 0 if the registration is non-transient. I.e. The registrationNode is the Node in whose
    // registry it exists.
    // Note that this doesn't need to be a RefPtr because the observer will be holding a RefPtr to the same node at
    // least for the lifetime of this Registration in its m_transientObservedNodes map.
    Node* registrationNode;
};
#endif // ENABLE(MUTATION_OBSERVERS)

class NodeRareData {
    WTF_MAKE_NONCOPYABLE(NodeRareData); WTF_MAKE_FAST_ALLOCATED;
public:    
    NodeRareData()
        : m_treeScope(0)
        , m_tabIndex(0)
        , m_tabIndexWasSetExplicitly(false)
        , m_isFocused(false)
        , m_needsFocusAppearanceUpdateSoonAfterAttach(false)
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
    Vector<MutationObserverRegistration>* mutationObserverRegistry() { return m_mutationObserverRegistry.get(); }
    Vector<MutationObserverRegistration>* ensureMutationObserverRegistry()
    {
        if (!m_mutationObserverRegistry)
            m_mutationObserverRegistry = adoptPtr(new Vector<MutationObserverRegistration>);
        return m_mutationObserverRegistry.get();
    }
#endif

    bool isFocused() const { return m_isFocused; }
    void setFocused(bool focused) { m_isFocused = focused; }

protected:
    // for ElementRareData
    bool needsFocusAppearanceUpdateSoonAfterAttach() const { return m_needsFocusAppearanceUpdateSoonAfterAttach; }
    void setNeedsFocusAppearanceUpdateSoonAfterAttach(bool needs) { m_needsFocusAppearanceUpdateSoonAfterAttach = needs; }

private:
    TreeScope* m_treeScope;
    OwnPtr<NodeListsNodeData> m_nodeLists;
    OwnPtr<EventTargetData> m_eventTargetData;
    short m_tabIndex;
    bool m_tabIndexWasSetExplicitly : 1;
    bool m_isFocused : 1;
    bool m_needsFocusAppearanceUpdateSoonAfterAttach : 1;

#if ENABLE(MUTATION_OBSERVERS)
    OwnPtr<Vector<MutationObserverRegistration> > m_mutationObserverRegistry;
#endif
};

} // namespace WebCore

#endif // NodeRareData_h
