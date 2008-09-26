/**
 *
 * Copyright (C) 2008 Apple Computer, Inc.
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

#include "DynamicNodeList.h"
#include "StringHash.h"
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

struct NodeListsNodeData {
    typedef HashSet<DynamicNodeList*> NodeListSet;
    NodeListSet m_listsWithCaches;
    
    DynamicNodeList::Caches m_childNodeListCaches;
    
    typedef HashMap<String, DynamicNodeList::Caches*> CacheMap;
    CacheMap m_classNodeListCaches;
    CacheMap m_nameNodeListCaches;
    
    ~NodeListsNodeData()
    {
        deleteAllValues(m_classNodeListCaches);
        deleteAllValues(m_nameNodeListCaches);
    }
    
    void invalidateCaches();
    void invalidateCachesThatDependOnAttributes();
    bool isEmpty() const;
};
    
class NodeRareData {
public:    
    NodeRareData()
        : m_focused(false)
        , m_tabIndex(0)
        , m_tabIndexSetExplicitly(false)
        , m_needsFocusAppearanceUpdateSoonAfterAttach(false)
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
    
    void clearNodeLists() { m_nodeLists.clear(); }
    void setNodeLists(std::auto_ptr<NodeListsNodeData> lists) { m_nodeLists.set(lists.release()); }
    NodeListsNodeData* nodeLists() const { return m_nodeLists.get(); }
    
    short tabIndex() const { return m_tabIndex; }
    void setTabIndexExplicitly(short index) { m_tabIndex = index; m_tabIndexSetExplicitly = true; }
    bool tabIndexSetExplicitly() const { return m_tabIndexSetExplicitly; }
        
    bool m_focused : 1;

private:
    OwnPtr<NodeListsNodeData> m_nodeLists;
    short m_tabIndex;
    bool m_tabIndexSetExplicitly : 1;
public:
    bool m_needsFocusAppearanceUpdateSoonAfterAttach : 1; //for ElementRareData
};
} //namespace

#endif
