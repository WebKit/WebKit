/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2013 Apple Inc. All rights reserved.
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
#include "CollectionType.h"
#include "Document.h"
#include "ElementTraversal.h"
#include "HTMLNames.h"
#include "NodeList.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Element;

enum NodeListRootType {
    NodeListIsRootedAtNode,
    NodeListIsRootedAtDocument
};

static bool shouldInvalidateTypeOnAttributeChange(NodeListInvalidationType, const QualifiedName&);

class LiveNodeList : public NodeList {
public:
    LiveNodeList(ContainerNode& ownerNode, NodeListInvalidationType, NodeListRootType);
    virtual Node* namedItem(const AtomicString&) const override final;
    virtual bool nodeMatches(Element*) const = 0;

    virtual ~LiveNodeList();

    ALWAYS_INLINE bool isRootedAtDocument() const { return m_rootType == NodeListIsRootedAtDocument; }
    ALWAYS_INLINE NodeListInvalidationType invalidationType() const { return static_cast<NodeListInvalidationType>(m_invalidationType); }
    ContainerNode& ownerNode() const { return const_cast<ContainerNode&>(m_ownerNode.get()); }
    ALWAYS_INLINE void invalidateCacheForAttribute(const QualifiedName* attrName) const
    {
        if (!attrName || shouldInvalidateTypeOnAttributeChange(invalidationType(), *attrName))
            invalidateCache(document());
    }
    virtual void invalidateCache(Document&) const = 0;

protected:
    Document& document() const { return m_ownerNode->document(); }
    ContainerNode& rootNode() const;

    ALWAYS_INLINE NodeListRootType rootType() const { return static_cast<NodeListRootType>(m_rootType); }

private:
    virtual bool isLiveNodeList() const override { return true; }

    Element* iterateForPreviousElement(Element* current) const;

    Ref<ContainerNode> m_ownerNode;

    const unsigned m_rootType : 1;
    const unsigned m_invalidationType : 4;
};

template <class NodeListType>
class CachedLiveNodeList : public LiveNodeList {
public:
    virtual ~CachedLiveNodeList();

    virtual unsigned length() const override final;
    virtual Node* item(unsigned offset) const override final;

    // For CollectionIndexCache
    Element* collectionFirst() const;
    Element* collectionLast() const;
    Element* collectionTraverseForward(Element&, unsigned count, unsigned& traversedCount) const;
    Element* collectionTraverseBackward(Element&, unsigned count) const;
    bool collectionCanTraverseBackward() const { return true; }
    void willValidateIndexCache() const;

    virtual void invalidateCache(Document&) const;
    virtual size_t memoryCost() const override;

protected:
    CachedLiveNodeList(ContainerNode& rootNode, NodeListInvalidationType, NodeListRootType = NodeListIsRootedAtNode);

private:
    mutable CollectionIndexCache<NodeListType, Element> m_indexCache;
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

inline ContainerNode& LiveNodeList::rootNode() const
{
    if (isRootedAtDocument() && ownerNode().inDocument())
        return ownerNode().document();

    return ownerNode();
}

template <class NodeListType>
CachedLiveNodeList<NodeListType>::CachedLiveNodeList(ContainerNode& ownerNode, NodeListInvalidationType invalidationType, NodeListRootType rootType)
    : LiveNodeList(ownerNode, invalidationType, rootType)
{
}

template <class NodeListType>
CachedLiveNodeList<NodeListType>::~CachedLiveNodeList()
{
    if (m_indexCache.hasValidCache())
        document().unregisterNodeList(*this);
}

template <class NodeListType>
Element* CachedLiveNodeList<NodeListType>::collectionFirst() const
{
    auto& root = rootNode();
    Element* element = ElementTraversal::firstWithin(&root);
    while (element && !static_cast<const NodeListType*>(this)->nodeMatches(element))
        element = ElementTraversal::next(element, &root);
    return element;
}

template <class NodeListType>
Element* CachedLiveNodeList<NodeListType>::collectionLast() const
{
    auto& root = rootNode();
    Element* element = ElementTraversal::lastWithin(&root);
    while (element && !static_cast<const NodeListType*>(this)->nodeMatches(element))
        element = ElementTraversal::previous(element, &root);
    return element;
}

template <class NodeListType>
inline Element* nextMatchingElement(const NodeListType* nodeList, Element* current, ContainerNode& root)
{
    do {
        current = ElementTraversal::next(current, &root);
    } while (current && !static_cast<const NodeListType*>(nodeList)->nodeMatches(current));
    return current;
}

template <class NodeListType>
Element* CachedLiveNodeList<NodeListType>::collectionTraverseForward(Element& current, unsigned count, unsigned& traversedCount) const
{
    auto& root = rootNode();
    Element* element = &current;
    for (traversedCount = 0; traversedCount < count; ++traversedCount) {
        element = nextMatchingElement(static_cast<const NodeListType*>(this), element, root);
        if (!element)
            return nullptr;
    }
    return element;
}

template <class NodeListType>
inline Element* previousMatchingElement(const NodeListType* nodeList, Element* current, ContainerNode& root)
{
    do {
        current = ElementTraversal::previous(current, &root);
    } while (current && !nodeList->nodeMatches(current));
    return current;
}

template <class NodeListType>
Element* CachedLiveNodeList<NodeListType>::collectionTraverseBackward(Element& current, unsigned count) const
{
    auto& root = rootNode();
    Element* element = &current;
    for (; count; --count) {
        element = previousMatchingElement(static_cast<const NodeListType*>(this), element, root);
        if (!element)
            return nullptr;
    }
    return element;
}
template <class NodeListType>
void CachedLiveNodeList<NodeListType>::willValidateIndexCache() const
{
    document().registerNodeList(const_cast<NodeListType&>(static_cast<const NodeListType&>(*this)));
}

template <class NodeListType>
void CachedLiveNodeList<NodeListType>::invalidateCache(Document& document) const
{
    if (!m_indexCache.hasValidCache())
        return;
    document.unregisterNodeList(const_cast<NodeListType&>(static_cast<const NodeListType&>(*this)));
    m_indexCache.invalidate();
}

template <class NodeListType>
unsigned CachedLiveNodeList<NodeListType>::length() const
{
    return m_indexCache.nodeCount(static_cast<const NodeListType&>(*this));
}

template <class NodeListType>
Node* CachedLiveNodeList<NodeListType>::item(unsigned offset) const
{
    return m_indexCache.nodeAt(static_cast<const NodeListType&>(*this), offset);
}

template <class NodeListType>
size_t CachedLiveNodeList<NodeListType>::memoryCost() const
{
    return m_indexCache.memoryCost();
}

} // namespace WebCore

#endif // LiveNodeList_h
