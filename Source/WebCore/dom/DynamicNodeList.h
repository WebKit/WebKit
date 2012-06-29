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

class DynamicNodeListCacheBase {
public:
    enum RootType {
        RootedAtNode,
        RootedAtDocument,
    };

    enum InvalidationType {
        AlwaysInvalidate,
        DoNotInvalidateOnAttributeChange,
    };

    DynamicNodeListCacheBase(RootType rootType, InvalidationType invalidationType)
        : m_rootedAtDocument(rootType == RootedAtDocument)
        , m_shouldInvalidateOnAttributeChange(invalidationType == AlwaysInvalidate)
    {
        clearCache();
    }

public:
    ALWAYS_INLINE bool isRootedAtDocument() const { return m_rootedAtDocument; }
    ALWAYS_INLINE bool shouldInvalidateOnAttributeChange() const { return m_shouldInvalidateOnAttributeChange; }

protected:
    ALWAYS_INLINE bool isItemCacheValid() const { return m_isItemCacheValid; }
    ALWAYS_INLINE Node* cachedItem() const { return m_cachedItem; }
    ALWAYS_INLINE unsigned cachedItemOffset() const { return m_cachedItemOffset; }

    ALWAYS_INLINE bool isLengthCacheValid() const { return m_isLengthCacheValid; }
    ALWAYS_INLINE unsigned cachedLength() const { return m_cachedLength; }
    ALWAYS_INLINE void setLengthCache(unsigned length) const
    {
        m_cachedLength = length;
        m_isLengthCacheValid = true;
    }
    ALWAYS_INLINE void setItemCache(Node* item, unsigned offset) const
    {
        m_cachedItem = item;
        m_cachedItemOffset = offset;
        m_isItemCacheValid = true;
    }

    void clearCache() const
    {
        m_cachedItem = 0;
        m_isLengthCacheValid = false;
        m_isItemCacheValid = false;
    }

private:
    mutable Node* m_cachedItem;
    mutable unsigned m_cachedLength;
    mutable unsigned m_cachedItemOffset;
    mutable unsigned m_isLengthCacheValid : 1;
    mutable unsigned m_isItemCacheValid : 1;

    // From DynamicNodeList
    const unsigned m_rootedAtDocument : 1;
    const unsigned m_shouldInvalidateOnAttributeChange : 1;
};

class DynamicNodeList : public NodeList, public DynamicNodeListCacheBase {
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
    DynamicNodeList(PassRefPtr<Node> ownerNode, RootType rootType, InvalidationType invalidationType)
        : DynamicNodeListCacheBase(rootType, invalidationType)
        , m_ownerNode(ownerNode)
    { }
    virtual ~DynamicNodeList() { }

    // DOM methods & attributes for NodeList
    virtual unsigned length() const = 0;
    virtual Node* item(unsigned index) const = 0;
    virtual Node* itemWithName(const AtomicString&) const;

    // Other methods (not part of DOM)
    Node* ownerNode() const { return m_ownerNode.get(); }
    void invalidateCache() { clearCache(); }

protected:
    Node* rootNode() const
    {
        if (isRootedAtDocument() && m_ownerNode->inDocument())
            return m_ownerNode->document();
        return m_ownerNode.get();
    }
    Document* document() const { return m_ownerNode->document(); }
    virtual bool nodeMatches(Element*) const = 0;

private:
    virtual bool isDynamicNodeList() const OVERRIDE { return true; }
    RefPtr<Node> m_ownerNode;
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
