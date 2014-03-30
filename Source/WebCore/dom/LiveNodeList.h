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
#include "CollectionType.h"
#include "Document.h"
#include "ElementDescendantIterator.h"
#include "HTMLNames.h"
#include "NodeList.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Element;

static bool shouldInvalidateTypeOnAttributeChange(NodeListInvalidationType, const QualifiedName&);

class LiveNodeList : public NodeList {
public:
    LiveNodeList(ContainerNode& ownerNode, NodeListInvalidationType);
    virtual ~LiveNodeList();

    virtual Node* namedItem(const AtomicString&) const override final;

    virtual bool nodeMatches(Element*) const = 0;
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
    virtual bool isLiveNodeList() const override final { return true; }

    ContainerNode& rootNode() const;

    Ref<ContainerNode> m_ownerNode;

    const unsigned m_invalidationType;
    bool m_isRegisteredForInvalidationAtDocument;
};

template <class NodeListType>
class CachedLiveNodeList : public LiveNodeList {
public:
    virtual ~CachedLiveNodeList();

    virtual unsigned length() const override final;
    virtual Node* item(unsigned offset) const override final;

    // For CollectionIndexCache
    ElementDescendantIterator collectionBegin() const;
    ElementDescendantIterator collectionLast() const;
    ElementDescendantIterator collectionEnd() const { return ElementDescendantIterator(); }
    void collectionTraverseForward(ElementDescendantIterator&, unsigned count, unsigned& traversedCount) const;
    void collectionTraverseBackward(ElementDescendantIterator&, unsigned count) const;
    bool collectionCanTraverseBackward() const { return true; }
    void willValidateIndexCache() const;

    virtual void invalidateCache(Document&) const;
    virtual size_t memoryCost() const override;

protected:
    CachedLiveNodeList(ContainerNode& rootNode, NodeListInvalidationType);

private:
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
    , m_indexCache(static_cast<NodeListType&>(*this))
{
}

template <class NodeListType>
CachedLiveNodeList<NodeListType>::~CachedLiveNodeList()
{
    auto& nodeList = static_cast<const NodeListType&>(*this);
    if (m_indexCache.hasValidCache(nodeList))
        document().unregisterNodeListForInvalidation(*this);
}

template <class NodeListType>
inline ContainerNode& CachedLiveNodeList<NodeListType>::rootNode() const
{
    if (static_cast<const NodeListType&>(*this).isRootedAtDocument() && ownerNode().inDocument())
        return ownerNode().document();

    return ownerNode();
}

template <class NodeListType>
ElementDescendantIterator CachedLiveNodeList<NodeListType>::collectionBegin() const
{
    auto& nodeList = static_cast<const NodeListType&>(*this);
    auto descendants = elementDescendants(rootNode());
    auto end = descendants.end();
    for (auto it = descendants.begin(); it != end; ++it) {
        if (nodeList.nodeMatches(&*it))
            return it;
    }
    return end;
}

template <class NodeListType>
ElementDescendantIterator CachedLiveNodeList<NodeListType>::collectionLast() const
{
    auto& nodeList = static_cast<const NodeListType&>(*this);
    auto descendants = elementDescendants(rootNode());
    auto end = descendants.end();
    for (auto it = descendants.last(); it != end; --it) {
        if (nodeList.nodeMatches(&*it))
            return it;
    }
    return end;
}

template <class NodeListType>
void CachedLiveNodeList<NodeListType>::collectionTraverseForward(ElementDescendantIterator& current, unsigned count, unsigned& traversedCount) const
{
    auto& nodeList = static_cast<const NodeListType&>(*this);
    ASSERT(nodeList.nodeMatches(&*current));
    auto end = collectionEnd();
    for (traversedCount = 0; traversedCount < count; ++traversedCount) {
        do {
            ++current;
            if (current == end)
                return;
        } while (!nodeList.nodeMatches(&*current));
    }
}

template <class NodeListType>
void CachedLiveNodeList<NodeListType>::collectionTraverseBackward(ElementDescendantIterator& current, unsigned count) const
{
    auto& nodeList = static_cast<const NodeListType&>(*this);
    ASSERT(nodeList.nodeMatches(&*current));
    auto end = collectionEnd();
    for (; count; --count) {
        do {
            --current;
            if (current == end)
                return;
        } while (!nodeList.nodeMatches(&*current));
    }
}

template <class NodeListType>
void CachedLiveNodeList<NodeListType>::willValidateIndexCache() const
{
    document().registerNodeListForInvalidation(const_cast<NodeListType&>(static_cast<const NodeListType&>(*this)));
}

template <class NodeListType>
void CachedLiveNodeList<NodeListType>::invalidateCache(Document& document) const
{
    auto& nodeList = static_cast<const NodeListType&>(*this);
    if (!m_indexCache.hasValidCache(nodeList))
        return;
    document.unregisterNodeListForInvalidation(const_cast<NodeListType&>(nodeList));
    m_indexCache.invalidate(nodeList);
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
