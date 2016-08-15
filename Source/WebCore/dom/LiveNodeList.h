/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006-2007, 2013-2014 Apple Inc. All rights reserved.
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

#ifndef LiveNodeList_h
#define LiveNodeList_h

#include "CollectionIndexCache.h"
#include "CollectionTraversal.h"
#include "CollectionType.h"
#include "Document.h"
#include "ElementDescendantIterator.h"
#include "HTMLNames.h"
#include "NodeList.h"
#include <wtf/Forward.h>

namespace WebCore {

class Element;

static bool shouldInvalidateTypeOnAttributeChange(NodeListInvalidationType, const QualifiedName&);

class LiveNodeList : public NodeList {
public:
    LiveNodeList(ContainerNode& ownerNode, NodeListInvalidationType);
    virtual ~LiveNodeList();

    virtual bool elementMatches(Element&) const = 0;
    virtual bool isRootedAtDocument() const = 0;

    ALWAYS_INLINE NodeListInvalidationType invalidationType() const { return static_cast<NodeListInvalidationType>(m_invalidationType); }
    ContainerNode& ownerNode() const { return const_cast<ContainerNode&>(m_ownerNode.get()); }
    ALWAYS_INLINE void invalidateCacheForAttribute(const QualifiedName* attrName) const
    {
        if (!attrName || shouldInvalidateTypeOnAttributeChange(invalidationType(), *attrName))
            invalidateCache(document());
    }
    virtual void invalidateCache(Document&) const = 0;

    bool isRegisteredForInvalidationAtDocument() const { return m_isRegisteredForInvalidationAtDocument; }
    void setRegisteredForInvalidationAtDocument(bool f) { m_isRegisteredForInvalidationAtDocument = f; }

protected:
    Document& document() const { return m_ownerNode->document(); }

private:
    bool isLiveNodeList() const final { return true; }

    ContainerNode& rootNode() const;

    Ref<ContainerNode> m_ownerNode;

    const unsigned m_invalidationType;
    bool m_isRegisteredForInvalidationAtDocument;
};

template <class NodeListType>
class CachedLiveNodeList : public LiveNodeList {
public:
    virtual ~CachedLiveNodeList();

    unsigned length() const final { return m_indexCache.nodeCount(nodeList()); }
    Element* item(unsigned offset) const override { return m_indexCache.nodeAt(nodeList(), offset); }

    // For CollectionIndexCache
    ElementDescendantIterator collectionBegin() const { return CollectionTraversal<CollectionTraversalType::Descendants>::begin(nodeList(), rootNode()); }
    ElementDescendantIterator collectionLast() const { return CollectionTraversal<CollectionTraversalType::Descendants>::last(nodeList(), rootNode()); }
    ElementDescendantIterator collectionEnd() const { return ElementDescendantIterator(); }
    void collectionTraverseForward(ElementDescendantIterator& current, unsigned count, unsigned& traversedCount) const { CollectionTraversal<CollectionTraversalType::Descendants>::traverseForward(nodeList(), current, count, traversedCount); }
    void collectionTraverseBackward(ElementDescendantIterator& current, unsigned count) const { CollectionTraversal<CollectionTraversalType::Descendants>::traverseBackward(nodeList(), current, count); }
    bool collectionCanTraverseBackward() const { return true; }
    void willValidateIndexCache() const { document().registerNodeListForInvalidation(const_cast<CachedLiveNodeList<NodeListType>&>(*this)); }

    void invalidateCache(Document&) const final;
    size_t memoryCost() const final { return m_indexCache.memoryCost(); }

protected:
    CachedLiveNodeList(ContainerNode& rootNode, NodeListInvalidationType);

private:
    NodeListType& nodeList() { return static_cast<NodeListType&>(*this); }
    const NodeListType& nodeList() const { return static_cast<const NodeListType&>(*this); }

    ContainerNode& rootNode() const;

    mutable CollectionIndexCache<NodeListType, ElementDescendantIterator> m_indexCache;
};

ALWAYS_INLINE bool shouldInvalidateTypeOnAttributeChange(NodeListInvalidationType type, const QualifiedName& attrName)
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
    case DoNotInvalidateOnAttributeChanges:
        return false;
    case InvalidateOnAnyAttrChange:
        return true;
    }
    return false;
}

template <class NodeListType>
CachedLiveNodeList<NodeListType>::CachedLiveNodeList(ContainerNode& ownerNode, NodeListInvalidationType invalidationType)
    : LiveNodeList(ownerNode, invalidationType)
    , m_indexCache(nodeList())
{
}

template <class NodeListType>
CachedLiveNodeList<NodeListType>::~CachedLiveNodeList()
{
    if (m_indexCache.hasValidCache(nodeList()))
        document().unregisterNodeListForInvalidation(*this);
}

template <class NodeListType>
inline ContainerNode& CachedLiveNodeList<NodeListType>::rootNode() const
{
    if (nodeList().isRootedAtDocument() && ownerNode().inDocument())
        return ownerNode().document();

    return ownerNode();
}

template <class NodeListType>
void CachedLiveNodeList<NodeListType>::invalidateCache(Document& document) const
{
    if (!m_indexCache.hasValidCache(nodeList()))
        return;
    document.unregisterNodeListForInvalidation(const_cast<NodeListType&>(nodeList()));
    m_indexCache.invalidate(nodeList());
}

} // namespace WebCore

#endif // LiveNodeList_h
