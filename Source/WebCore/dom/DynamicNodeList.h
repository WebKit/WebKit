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
#include <wtf/RefCounted.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Element;
class Node;

class DynamicNodeList : public NodeList {
public:
    struct Caches : RefCounted<Caches> {
        static PassRefPtr<Caches> create();
        void reset();
        
        unsigned cachedLength;
        Node* lastItem;
        unsigned lastItemOffset;
        bool isLengthCacheValid : 1;
        bool isItemCacheValid : 1;

    protected:
        Caches();
    };

    DynamicNodeList(PassRefPtr<Node> node)
        : m_node(node)
    { }
    virtual ~DynamicNodeList() { }

    // DOM methods & attributes for NodeList
    virtual unsigned length() const = 0;
    virtual Node* item(unsigned index) const = 0;
    virtual Node* itemWithName(const AtomicString&) const;

    // Other methods (not part of DOM)
    Node* node() const { return m_node.get(); }

protected:
    virtual bool nodeMatches(Element*) const = 0;
    RefPtr<Node> m_node;
};

class DynamicSubtreeNodeList : public DynamicNodeList {
public:
    virtual ~DynamicSubtreeNodeList();
    virtual unsigned length() const OVERRIDE;
    virtual Node* item(unsigned index) const OVERRIDE;
    void invalidateCache();
    Node* rootNode() const { return node(); }

protected:
    DynamicSubtreeNodeList(PassRefPtr<Node> rootNode);

private:

    class SubtreeCaches {
    public:
        SubtreeCaches();

        bool isLengthCacheValid() const { return m_isLengthCacheValid && domVersionIsConsistent(); }
        bool isItemCacheValid() const { return m_isItemCacheValid && domVersionIsConsistent(); }

        unsigned cachedLength() const { return m_cachedLength; }
        Node* cachedItem() const { return m_cachedItem; }
        unsigned cachedItemOffset() const { return m_cachedItemOffset; }

        void setLengthCache(Node* node, unsigned length);
        void setItemCache(Node* item, unsigned offset);
        void reset();

    private:
        Node* m_cachedItem;
        unsigned m_cachedItemOffset;
        unsigned m_cachedLength : 30;
        unsigned m_isLengthCacheValid : 1;
        unsigned m_isItemCacheValid : 1;

        bool domVersionIsConsistent() const
        {
            return m_cachedItem && m_cachedItem->document()->domTreeVersion() == m_DOMVersionAtTimeOfCaching;
        }
        uint64_t m_DOMVersionAtTimeOfCaching;
    };

    mutable SubtreeCaches m_caches;

    virtual bool isDynamicNodeList() const;
    Node* itemForwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const;
    Node* itemBackwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const;
};

} // namespace WebCore

#endif // DynamicNodeList_h
