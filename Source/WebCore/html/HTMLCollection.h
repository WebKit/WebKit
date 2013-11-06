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

#include "CollectionIndexCache.h"
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
    static PassRefPtr<HTMLCollection> create(ContainerNode& base, CollectionType);
    virtual ~HTMLCollection();

    // DOM API
    unsigned length() const;
    Node* item(unsigned offset) const;
    virtual Node* namedItem(const AtomicString& name) const;
    PassRefPtr<NodeList> tags(const String&);

    // Non-DOM API
    bool hasNamedItem(const AtomicString& name) const;
    void namedItems(const AtomicString& name, Vector<Ref<Element>>&) const;

    bool isRootedAtDocument() const { return m_rootType == NodeListIsRootedAtDocument; }
    NodeListInvalidationType invalidationType() const { return static_cast<NodeListInvalidationType>(m_invalidationType); }
    CollectionType type() const { return static_cast<CollectionType>(m_collectionType); }
    ContainerNode& ownerNode() const { return const_cast<ContainerNode&>(m_ownerNode.get()); }
    void invalidateCache(const QualifiedName* attrName) const
    {
        if (!attrName || shouldInvalidateTypeOnAttributeChange(invalidationType(), *attrName))
            invalidateCache();
        else if (*attrName == HTMLNames::idAttr || *attrName == HTMLNames::nameAttr)
            invalidateIdNameCacheMaps();
    }
    virtual void invalidateCache() const;
    void invalidateIdNameCacheMaps() const;

    // For CollectionIndexCache
    Element* collectionFirst() const;
    Element* collectionLast() const;
    Element* collectionTraverseForward(Element&, unsigned count, unsigned& traversedCount) const;
    Element* collectionTraverseBackward(Element&, unsigned count) const;
    bool collectionCanTraverseBackward() const { return !m_usesCustomForwardOnlyTraversal; }

protected:
    enum ElementTraversalType { NormalTraversal, CustomForwardOnlyTraversal };
    HTMLCollection(ContainerNode& base, CollectionType, ElementTraversalType = NormalTraversal);

    virtual void updateNameCache() const;

    typedef HashMap<AtomicStringImpl*, OwnPtr<Vector<Element*>>> NodeCacheMap;
    Vector<Element*>* idCache(const AtomicString& name) const { return m_idCache.get(name.impl()); }
    Vector<Element*>* nameCache(const AtomicString& name) const { return m_nameCache.get(name.impl()); }
    void appendIdCache(const AtomicString& name, Element* element) const { append(m_idCache, name, element); }
    void appendNameCache(const AtomicString& name, Element* element) const { append(m_nameCache, name, element); }

    Document& document() const { return m_ownerNode->document(); }
    ContainerNode& rootNode() const;
    bool usesCustomForwardOnlyTraversal() const { return m_usesCustomForwardOnlyTraversal; }

    bool isItemRefElementsCacheValid() const { return m_isItemRefElementsCacheValid; }
    void setItemRefElementsCacheValid() const { m_isItemRefElementsCacheValid = true; }

    NodeListRootType rootType() const { return static_cast<NodeListRootType>(m_rootType); }

    bool hasNameCache() const { return m_isNameCacheValid; }
    void setHasNameCache() const { m_isNameCacheValid = true; }

private:
    static void append(NodeCacheMap&, const AtomicString&, Element*);

    Element* iterateForPreviousElement(Element* current) const;
    Element* firstElement(ContainerNode& root) const;
    Element* traverseForward(Element& current, unsigned count, unsigned& traversedCount, ContainerNode& root) const;

    virtual Element* customElementAfter(Element*) const { ASSERT_NOT_REACHED(); return nullptr; }

    Ref<ContainerNode> m_ownerNode;

    mutable CollectionIndexCache<HTMLCollection, Element> m_indexCache;

    const unsigned m_rootType : 2;
    const unsigned m_invalidationType : 4;
    const unsigned m_shouldOnlyIncludeDirectChildren : 1;

    mutable unsigned m_isNameCacheValid : 1;
    const unsigned m_collectionType : 5;
    const unsigned m_usesCustomForwardOnlyTraversal : 1;
    mutable unsigned m_isItemRefElementsCacheValid : 1;

    mutable NodeCacheMap m_idCache;
    mutable NodeCacheMap m_nameCache;
};

} // namespace

#endif
