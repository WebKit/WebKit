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

#include "CollectionType.h"
#include "Document.h"
#include "HTMLNames.h"
#include "NodeList.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Element;
class Node;

enum NodeListRootType {
    NodeListIsRootedAtNode,
    NodeListIsRootedAtDocument,
};

class DynamicNodeListCacheBase {
public:
    enum ItemBeforeSupportType {
        DoNotSupportItemBefore,
        SupportItemBefore,
    };

    DynamicNodeListCacheBase(NodeListRootType rootType, NodeListInvalidationType invalidationType,
        CollectionType collectionType = InvalidCollectionType, ItemBeforeSupportType itemBeforeSupportType = DoNotSupportItemBefore)
        : m_cachedItem(0)
        , m_isLengthCacheValid(false)
        , m_isItemCacheValid(false)
        , m_rootedAtDocument(rootType == NodeListIsRootedAtDocument)
        , m_invalidationType(invalidationType)
        , m_isNameCacheValid(false)
        , m_collectionType(collectionType)
        , m_supportsItemBefore(itemBeforeSupportType == SupportItemBefore)
    {
        ASSERT(m_invalidationType == static_cast<unsigned>(invalidationType));
        ASSERT(m_collectionType == static_cast<unsigned>(collectionType));
    }

public:
    ALWAYS_INLINE bool isRootedAtDocument() const { return m_rootedAtDocument; }
    ALWAYS_INLINE NodeListInvalidationType invalidationType() const { return static_cast<NodeListInvalidationType>(m_invalidationType); }
    ALWAYS_INLINE CollectionType type() const { return static_cast<CollectionType>(m_collectionType); }

    ALWAYS_INLINE void invalidateCache(const QualifiedName* attrName) const
    {
        if (!attrName || shouldInvalidateTypeOnAttributeChange(invalidationType(), *attrName))
            invalidateCache();
    }
    void invalidateCache() const;

    static bool shouldInvalidateTypeOnAttributeChange(NodeListInvalidationType, const QualifiedName&);

protected:
    bool supportsItemBefore() const { return m_supportsItemBefore; }

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
        ASSERT(item);
        m_cachedItem = item;
        m_cachedItemOffset = offset;
        m_isItemCacheValid = true;
    }

    bool hasNameCache() const { return m_isNameCacheValid; }
    void setHasNameCache() const { m_isNameCacheValid = true; }

private:
    mutable Node* m_cachedItem;
    mutable unsigned m_cachedLength;
    mutable unsigned m_cachedItemOffset;
    mutable unsigned m_isLengthCacheValid : 1;
    mutable unsigned m_isItemCacheValid : 1;
    const unsigned m_rootedAtDocument : 1;
    const unsigned m_invalidationType : 4;

    // From HTMLCollection
    mutable unsigned m_isNameCacheValid : 1;
    const unsigned m_collectionType : 5;
    const unsigned m_supportsItemBefore : 1;
};

ALWAYS_INLINE bool DynamicNodeListCacheBase::shouldInvalidateTypeOnAttributeChange(NodeListInvalidationType type, const QualifiedName& attrName)
{
    switch (type) {
    case InvalidateOnClassAttrChange:
        return attrName == HTMLNames::classAttr;
    case InvalidateOnNameAttrChange:
        return attrName == HTMLNames::nameAttr;
    case InvalidateOnIdNameAttrChange:
        return attrName == HTMLNames::idAttr || attrName == HTMLNames::nameAttr;
    case InvalidateOnForAttrChange:
        return attrName == HTMLNames::forAttr;
    case InvalidateForFormControls:
        return attrName == HTMLNames::nameAttr || attrName == HTMLNames::idAttr || attrName == HTMLNames::forAttr
            || attrName == HTMLNames::formAttr || attrName == HTMLNames::typeAttr;
    case InvalidateOnHRefAttrChange:
        return attrName == HTMLNames::hrefAttr;
    case InvalidateOnItemAttrChange:
#if ENABLE(MICRODATA)
        return attrName == HTMLNames::itemscopeAttr || attrName == HTMLNames::itempropAttr || attrName == HTMLNames::itemtypeAttr;
#endif // Intentionally fall through
    case DoNotInvalidateOnAttributeChanges:
        return false;
    case InvalidateOnAnyAttrChange:
        return true;
    }
    return false;
}

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
    DynamicNodeList(PassRefPtr<Node> ownerNode, NodeListRootType rootType, NodeListInvalidationType invalidationType)
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
    virtual ~DynamicSubtreeNodeList()
    {
        document()->unregisterNodeListCache(this);
    }
    virtual unsigned length() const OVERRIDE;
    virtual Node* item(unsigned index) const OVERRIDE;

protected:
    DynamicSubtreeNodeList(PassRefPtr<Node> node, NodeListInvalidationType invalidationType, NodeListRootType rootType = NodeListIsRootedAtNode)
        : DynamicNodeList(node, rootType, invalidationType)
    {
        document()->registerNodeListCache(this);
    }

private:
    Node* itemForwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const;
    Node* itemBackwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const;
};

} // namespace WebCore

#endif // DynamicNodeList_h
