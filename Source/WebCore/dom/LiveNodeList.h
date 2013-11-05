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

#ifndef LiveNodeList_h
#define LiveNodeList_h

#include "CollectionType.h"
#include "Document.h"
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
    enum Type {
        ClassNodeListType,
        NameNodeListType,
        TagNodeListType,
        HTMLTagNodeListType,
        RadioNodeListType,
        LabelsNodeListType,
    };

    LiveNodeList(ContainerNode& ownerNode, Type type, NodeListInvalidationType invalidationType, NodeListRootType rootType = NodeListIsRootedAtNode)
        : m_ownerNode(ownerNode)
        , m_cachedElement(nullptr)
        , m_isLengthCacheValid(false)
        , m_isElementCacheValid(false)
        , m_rootType(rootType)
        , m_invalidationType(invalidationType)
        , m_type(type)
    {
        ASSERT(m_rootType == static_cast<unsigned>(rootType));
        ASSERT(m_invalidationType == static_cast<unsigned>(invalidationType));
        ASSERT(m_type == static_cast<unsigned>(type));

        document().registerNodeList(this);
    }
    virtual Node* namedItem(const AtomicString&) const OVERRIDE;
    virtual bool nodeMatches(Element*) const = 0;

    virtual ~LiveNodeList()
    {
        document().unregisterNodeList(this);
    }

    // DOM API
    virtual unsigned length() const OVERRIDE;
    virtual Node* item(unsigned offset) const OVERRIDE;

    ALWAYS_INLINE bool isRootedAtDocument() const { return m_rootType == NodeListIsRootedAtDocument; }
    ALWAYS_INLINE NodeListInvalidationType invalidationType() const { return static_cast<NodeListInvalidationType>(m_invalidationType); }
    ALWAYS_INLINE Type type() const { return static_cast<Type>(m_type); }
    ContainerNode& ownerNode() const { return const_cast<ContainerNode&>(m_ownerNode.get()); }
    ALWAYS_INLINE void invalidateCache(const QualifiedName* attrName) const
    {
        if (!attrName || shouldInvalidateTypeOnAttributeChange(invalidationType(), *attrName))
            invalidateCache();
    }
    void invalidateCache() const;

protected:
    Document& document() const { return m_ownerNode->document(); }
    ContainerNode& rootNode() const;

    ALWAYS_INLINE bool isElementCacheValid() const { return m_isElementCacheValid; }
    ALWAYS_INLINE Element* cachedElement() const { return m_cachedElement; }
    ALWAYS_INLINE unsigned cachedElementOffset() const { return m_cachedElementOffset; }

    ALWAYS_INLINE bool isLengthCacheValid() const { return m_isLengthCacheValid; }
    ALWAYS_INLINE unsigned cachedLength() const { return m_cachedLength; }
    ALWAYS_INLINE void setLengthCache(unsigned length) const
    {
        m_cachedLength = length;
        m_isLengthCacheValid = true;
    }
    ALWAYS_INLINE void setCachedElement(Element& element, unsigned offset) const
    {
        m_cachedElement = &element;
        m_cachedElementOffset = offset;
        m_isElementCacheValid = true;
    }

    ALWAYS_INLINE NodeListRootType rootType() const { return static_cast<NodeListRootType>(m_rootType); }

private:
    virtual bool isLiveNodeList() const OVERRIDE { return true; }

    Element* elementBeforeOrAfterCachedElement(unsigned offset, ContainerNode* root) const;
    Element* traverseLiveNodeListFirstElement(ContainerNode* root) const;
    Element* traverseLiveNodeListForwardToOffset(unsigned offset, Element* currentElement, unsigned& currentOffset, ContainerNode* root) const;
    bool isLastItemCloserThanLastOrCachedItem(unsigned offset) const;
    bool isFirstItemCloserThanCachedItem(unsigned offset) const;
    Element* iterateForPreviousElement(Element* current) const;
    Element* itemBefore(Element* previousItem) const;

    Ref<ContainerNode> m_ownerNode;
    mutable Element* m_cachedElement;
    mutable unsigned m_cachedLength;
    mutable unsigned m_cachedElementOffset;
    mutable unsigned m_isLengthCacheValid : 1;
    mutable unsigned m_isElementCacheValid : 1;
    const unsigned m_rootType : 2;
    const unsigned m_invalidationType : 4;
    const unsigned m_type : 5;
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

} // namespace WebCore

#endif // LiveNodeList_h
