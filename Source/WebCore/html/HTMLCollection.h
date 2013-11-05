/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef HTMLCollection_h
#define HTMLCollection_h

#include "CollectionType.h"
#include "ContainerNode.h"
#include "Document.h"
#include "HTMLNames.h"
#include "LiveNodeList.h"
#include "ScriptWrappable.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

class Element;

class HTMLCollection : public ScriptWrappable, public RefCounted<HTMLCollection> {
public:
    enum ItemAfterOverrideType {
        OverridesItemAfter,
        DoesNotOverrideItemAfter,
    };

    static PassRefPtr<HTMLCollection> create(ContainerNode& base, CollectionType);
    virtual ~HTMLCollection();

    // DOM API
    unsigned length() const;
    Node* item(unsigned offset) const;
    virtual Node* namedItem(const AtomicString& name) const;
    PassRefPtr<NodeList> tags(const String&);

    // Non-DOM API
    virtual bool hasNamedItem(const AtomicString& name) const;
    void namedItems(const AtomicString& name, Vector<Ref<Element>>&) const;
    bool isEmpty() const
    {
        if (isLengthCacheValid())
            return !cachedLength();
        if (isElementCacheValid())
            return !cachedElement();
        return !item(0);
    }
    bool hasExactlyOneItem() const
    {
        if (isLengthCacheValid())
            return cachedLength() == 1;
        if (isElementCacheValid())
            return cachedElement() && !cachedElementOffset() && !item(1);
        return item(0) && !item(1);
    }

    virtual Element* virtualItemAfter(unsigned& offsetInArray, Element*) const;

    Element* traverseFirstElement(unsigned& offsetInArray, ContainerNode* root) const;
    Element* traverseForwardToOffset(unsigned offset, Element* currentElement, unsigned& currentOffset, unsigned& offsetInArray, ContainerNode* root) const;

    ALWAYS_INLINE bool isRootedAtDocument() const { return m_rootType == NodeListIsRootedAtDocument; }
    ALWAYS_INLINE NodeListInvalidationType invalidationType() const { return static_cast<NodeListInvalidationType>(m_invalidationType); }
    ALWAYS_INLINE CollectionType type() const { return static_cast<CollectionType>(m_collectionType); }
    ContainerNode& ownerNode() const { return const_cast<ContainerNode&>(m_ownerNode.get()); }
    ALWAYS_INLINE void invalidateCache(const QualifiedName* attrName) const
    {
        if (!attrName || shouldInvalidateTypeOnAttributeChange(invalidationType(), *attrName))
            invalidateCache();
        else if (*attrName == HTMLNames::idAttr || *attrName == HTMLNames::nameAttr)
            invalidateIdNameCacheMaps();
    }
    void invalidateCache() const;
    void invalidateIdNameCacheMaps() const;

protected:
    HTMLCollection(ContainerNode& base, CollectionType, ItemAfterOverrideType);

    virtual void updateNameCache() const;

    typedef HashMap<AtomicStringImpl*, OwnPtr<Vector<Element*>>> NodeCacheMap;
    Vector<Element*>* idCache(const AtomicString& name) const { return m_idCache.get(name.impl()); }
    Vector<Element*>* nameCache(const AtomicString& name) const { return m_nameCache.get(name.impl()); }
    void appendIdCache(const AtomicString& name, Element* element) const { append(m_idCache, name, element); }
    void appendNameCache(const AtomicString& name, Element* element) const { append(m_nameCache, name, element); }

    Document& document() const { return m_ownerNode->document(); }
    ContainerNode& rootNode() const;
    bool overridesItemAfter() const { return m_overridesItemAfter; }

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
    void setCachedElement(Element&, unsigned offset, unsigned elementsArrayOffset) const;

    ALWAYS_INLINE bool isItemRefElementsCacheValid() const { return m_isItemRefElementsCacheValid; }
    ALWAYS_INLINE void setItemRefElementsCacheValid() const { m_isItemRefElementsCacheValid = true; }

    ALWAYS_INLINE NodeListRootType rootType() const { return static_cast<NodeListRootType>(m_rootType); }

    bool hasNameCache() const { return m_isNameCacheValid; }
    void setHasNameCache() const { m_isNameCacheValid = true; }

private:
    Element* traverseNextElement(unsigned& offsetInArray, Element* previous, ContainerNode* root) const;

    static void append(NodeCacheMap&, const AtomicString&, Element*);

    Element* elementBeforeOrAfterCachedElement(unsigned offset, ContainerNode* root) const;
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
    const unsigned m_shouldOnlyIncludeDirectChildren : 1;

    // From HTMLCollection
    mutable unsigned m_isNameCacheValid : 1;
    const unsigned m_collectionType : 5;
    const unsigned m_overridesItemAfter : 1;
    mutable unsigned m_isItemRefElementsCacheValid : 1;

    mutable NodeCacheMap m_idCache;
    mutable NodeCacheMap m_nameCache;
    mutable unsigned m_cachedElementsArrayOffset;
};

} // namespace

#endif
