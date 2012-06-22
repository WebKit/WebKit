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
    enum NodeListType {
        ChildNodeListType,
        ClassNodeListType,
        NameNodeListType,
        TagNodeListType,
        RadioNodeListType,
        LabelsNodeListType,
        MicroDataItemListType,
    };
    enum RootType {
        RootedAtNode,
        RootedAtDocument,
    };
    enum InvalidationType {
        AlwaysInvalidate,
        DoNotInvalidateOnAttributeChange,
    };
    DynamicNodeList(PassRefPtr<Node> ownerNode, RootType rootType, InvalidationType invalidationType)
        : m_ownerNode(ownerNode)
        , m_caches(rootType, invalidationType)
    { }
    virtual ~DynamicNodeList() { }

    // DOM methods & attributes for NodeList
    virtual unsigned length() const = 0;
    virtual Node* item(unsigned index) const = 0;
    virtual Node* itemWithName(const AtomicString&) const;

    // Other methods (not part of DOM)
    Node* ownerNode() const { return m_ownerNode.get(); }
    bool isRootedAtDocument() const { return m_caches.rootedAtDocument; }
    bool shouldInvalidateOnAttributeChange() const { return m_caches.shouldInvalidateOnAttributeChange; }
    void invalidateCache() { m_caches.reset(); }

protected:
    Node* rootNode() const
    {
        if (m_caches.rootedAtDocument && m_ownerNode->inDocument())
            return m_ownerNode->document();
        return m_ownerNode.get();
    }
    Document* document() const { return m_ownerNode->document(); }
    virtual bool nodeMatches(Element*) const = 0;

    struct Caches {
        Caches(RootType rootType, InvalidationType invalidationType)
            : rootedAtDocument(rootType == RootedAtDocument)
            , shouldInvalidateOnAttributeChange(invalidationType == AlwaysInvalidate)
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
        unsigned lastItemOffset;
        unsigned isLengthCacheValid : 1;
        unsigned isItemCacheValid : 1;

        // Following flags should belong in DynamicSubtreeNode but are here for bit-packing.
        unsigned type : 4;
        unsigned rootedAtDocument : 1;
        unsigned shouldInvalidateOnAttributeChange : 1;
    };

    RefPtr<Node> m_ownerNode;
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
    DynamicSubtreeNodeList(PassRefPtr<Node> node, RootType rootType = RootedAtNode, InvalidationType invalidationType = AlwaysInvalidate)
        : DynamicNodeList(node, rootType, invalidationType)
    { }

private:
    Node* itemForwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const;
    Node* itemBackwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const;
};

} // namespace WebCore

#endif // DynamicNodeList_h
