/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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

#ifndef NodeList_h
#define NodeList_h

#include "Shared.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class AtomicString;
class Node;

class NodeList : public Shared<NodeList> {
public:

    struct Caches {
        Caches();
        void reset();
        
        unsigned cachedLength;
        Node* lastItem;
        unsigned lastItemOffset;
        bool isLengthCacheValid : 1;
        bool isItemCacheValid : 1;
    };

    NodeList(PassRefPtr<Node> rootNode);
    NodeList(PassRefPtr<Node> rootNode, Caches*);
    virtual ~NodeList();

    // DOM methods & attributes for NodeList
    virtual unsigned length() const = 0;
    virtual Node* item(unsigned index) const = 0;
    Node* itemWithName(const AtomicString&) const;

    // Other methods (not part of DOM)
    virtual void rootNodeChildrenChanged();
    virtual void rootNodeAttributeChanged() {}

protected:
    // helper functions for searching all ElementImpls in a tree
    unsigned recursiveLength(Node* start = 0) const;
    Node* recursiveItem (unsigned offset, Node* start = 0) const;
    virtual bool nodeMatches(Node* testNode) const = 0;

    RefPtr<Node> m_rootNode;
    mutable Caches* m_caches;
    bool m_ownsCaches;

 private:
    Node* itemForwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const;
    Node* itemBackwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const;
};

} //namespace

#endif
