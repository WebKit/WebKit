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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef NodeList_h
#define NodeList_h

#include "Shared.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class AtomicString;
class Element;
class Node;

class NodeList : public Shared<NodeList> {
public:
    virtual ~NodeList();

    virtual unsigned length() const = 0;
    virtual Node* item(unsigned index) const = 0;
    virtual Node* itemWithName(const AtomicString&) const;
};

// FIXME: Move this to its own source file.
class TreeNodeList : public NodeList {
public:
    TreeNodeList(PassRefPtr<Node> rootNode);
    virtual ~TreeNodeList();

    virtual Node* itemWithName(const AtomicString&) const;

    virtual void rootNodeChildrenChanged();
    virtual void rootNodeAttributeChanged();

protected:
    // helper functions for searching all elements in a tree
    unsigned recursiveLength(Node* start = 0) const;
    Node* recursiveItem(unsigned offset, Node* start = 0) const;
    virtual bool elementMatches(Element*) const = 0;

    RefPtr<Node> rootNode;
    mutable int cachedLength;
    mutable Node* lastItem;
    mutable unsigned lastItemOffset;
    mutable bool isLengthCacheValid : 1;
    mutable bool isItemCacheValid : 1;

 private:
    Node* itemForwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const;
    Node* itemBackwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const;
};

} //namespace

#endif
