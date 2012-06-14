/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef DynamicNodeList_h
#define DynamicNodeList_h

#include "Document.h"
#include "NodeList.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Element;
class Node;

class DynamicNodeList : public NodeList {
public:
    enum RootType {
        RootedAtNode,
        RootedAtDocument,
    };

    DynamicNodeList(PassRefPtr<Node> node, RootType rootType = RootedAtNode)
        : m_node(node)
        , m_caches(rootType)
    { }
    virtual ~DynamicNodeList() { }

    // DOM methods & attributes for NodeList
    virtual unsigned length() const = 0;
    virtual Node* item(unsigned index) const = 0;
    virtual Node* itemWithName(const AtomicString&) const;

    // Other methods (not part of DOM)
    Node* node() const
    {
        if (m_caches.rootedAtDocument && m_node->inDocument())
            return m_node->document();
        return m_node.get();
    }

    void invalidateCache() { m_caches.reset(); }

protected:
    virtual bool nodeMatches(Element*) const = 0;

    struct Caches {
        Caches(RootType rootType)
            : rootedAtDocument(rootType == RootedAtDocument)
        {
            reset();
        }

        void reset()
        {
            lastItem = 0;
            isLengthCacheValid = false;
            isItemCacheValid = false;
        }

        Node* lastItem;
        unsigned cachedLength;
        unsigned lastItemOffset : 29; // Borrow 3-bits for bit fields
        unsigned isLengthCacheValid : 1;
        unsigned isItemCacheValid : 1;
        unsigned rootedAtDocument : 1;
    };

    RefPtr<Node> m_node;
    mutable Caches m_caches;

private:
    virtual bool isDynamicNodeList() const OVERRIDE { return true; }
};

class DynamicSubtreeNodeList : public DynamicNodeList {
public:
    virtual ~DynamicSubtreeNodeList();
    virtual unsigned length() const OVERRIDE;
    virtual Node* item(unsigned index) const OVERRIDE;

protected:
    DynamicSubtreeNodeList(PassRefPtr<Node>, RootType = RootedAtNode);

private:
    using DynamicNodeList::invalidateCache;
    friend struct NodeListsNodeData;

    Node* itemForwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const;
    Node* itemBackwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const;
};

} // namespace WebCore

#endif // DynamicNodeList_h
