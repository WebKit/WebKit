/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2011, 2012, 2013, 2014 Apple Inc. All rights reserved.
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
#include <memory>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

class Element;

class CollectionNamedElementCache {
public:
#ifndef ASSERT_DISABLED
    CollectionNamedElementCache : m_didPopulateCalled(false) { }
#endif

    const Vector<Element*>* findElementsWithId(const AtomicString& id) const { return find(m_idToElementsMap, id); }
    const Vector<Element*>* findElementsWithName(const AtomicString& name) const { return find(m_nameToElementsMap, name); }

    void appendIdCache(const AtomicString& id, Element* element) { return append(m_idToElementsMap, id, element); }
    void appendNameCache(const AtomicString& name, Element* element)  { return append(m_nameToElementsMap, name, element); }

    void didPopulate()
    {
#ifndef ASSERT_DISABLED
        m_didPopulateCalled = true;
#endif
        if (size_t cost = memoryCost())
            reportExtraMemoryCostForCollectionIndexCache(cost);
    }
    size_t memoryCost() const { return (m_idToElementsMap.size() + m_nameToElementsMap.size()) * sizeof(Element*); }

private:
    typedef HashMap<AtomicStringImpl*, Vector<Element*>> StringToElementsMap;

    const Vector<Element*>* find(const StringToElementsMap& map, const AtomicString& key) const
    {
#ifndef ASSERT_DISABLED
        ASSERT(m_didPopulateCalled);
#endif
        auto it = map.find(key.impl());
        return it != map.end() ? &it->value : nullptr;
    }

    static void append(StringToElementsMap& map, const AtomicString& key, Element* element)
    {
        map.add(key.impl(), Vector<Element*>()).iterator->value.append(element);
    }

    StringToElementsMap m_idToElementsMap;
    StringToElementsMap m_nameToElementsMap;
#ifndef ASSERT_DISABLED
    bool m_didPopulateCalled;
#endif
};

class HTMLCollection : public ScriptWrappable, public RefCounted<HTMLCollection> {
public:
    static PassRef<HTMLCollection> create(ContainerNode& base, CollectionType);
    virtual ~HTMLCollection();

    // DOM API
    unsigned length() const;
    Node* item(unsigned offset) const;
    virtual Node* namedItem(const AtomicString& name) const;
    PassRefPtr<NodeList> tags(const String&);

    // Non-DOM API
    bool hasNamedItem(const AtomicString& name) const;
    void namedItems(const AtomicString& name, Vector<Ref<Element>>&) const;
    size_t memoryCost() const { return m_indexCache.memoryCost() + (m_namedElementCache ? m_namedElementCache->memoryCost() : 0); }

    enum RootType {
        IsRootedAtNode,
        IsRootedAtDocument
    };
    bool isRootedAtDocument() const { return m_rootType == IsRootedAtDocument; }
    NodeListInvalidationType invalidationType() const { return static_cast<NodeListInvalidationType>(m_invalidationType); }
    CollectionType type() const { return static_cast<CollectionType>(m_collectionType); }
    ContainerNode& ownerNode() const { return const_cast<ContainerNode&>(m_ownerNode.get()); }
    void invalidateCache(const QualifiedName* attrName) const
    {
        if (!attrName || shouldInvalidateTypeOnAttributeChange(invalidationType(), *attrName))
            invalidateCache(document());
        else if (hasNamedElementCache() && (*attrName == HTMLNames::idAttr || *attrName == HTMLNames::nameAttr))
            invalidateNamedElementCache(document());
    }
    virtual void invalidateCache(Document&) const;

    // For CollectionIndexCache
    Element* collectionBegin() const;
    Element* collectionLast() const;
    Element* collectionEnd() const { return nullptr; }
    void collectionTraverseForward(Element*&, unsigned count, unsigned& traversedCount) const;
    void collectionTraverseBackward(Element*&, unsigned count) const;
    bool collectionCanTraverseBackward() const { return !m_usesCustomForwardOnlyTraversal; }
    void willValidateIndexCache() const { document().registerCollection(const_cast<HTMLCollection&>(*this)); }

    bool hasNamedElementCache() const { return !!m_namedElementCache; }

protected:
    enum ElementTraversalType { NormalTraversal, CustomForwardOnlyTraversal };
    HTMLCollection(ContainerNode& base, CollectionType, ElementTraversalType = NormalTraversal);

    virtual void updateNamedElementCache() const;

    Document& document() const { return m_ownerNode->document(); }
    ContainerNode& rootNode() const;
    bool usesCustomForwardOnlyTraversal() const { return m_usesCustomForwardOnlyTraversal; }

    RootType rootType() const { return static_cast<RootType>(m_rootType); }

    CollectionNamedElementCache& createNameItemCache() const
    {
        ASSERT(!m_namedElementCache);
        m_namedElementCache = std::make_unique<CollectionNamedElementCache>();
        document().collectionCachedIdNameMap(*this);
        return *m_namedElementCache;
    }

    const CollectionNamedElementCache& namedItemCaches() const
    {
        ASSERT(!!m_namedElementCache);
        return *m_namedElementCache;
    }

private:
    Element* iterateForPreviousElement(Element* current) const;
    Element* firstElement(ContainerNode& root) const;
    Element* traverseForward(Element& current, unsigned count, unsigned& traversedCount, ContainerNode& root) const;

    virtual Element* customElementAfter(Element*) const { ASSERT_NOT_REACHED(); return nullptr; }
    
    void invalidateNamedElementCache(Document&) const;

    Ref<ContainerNode> m_ownerNode;

    mutable CollectionIndexCache<HTMLCollection, Element*> m_indexCache;
    mutable std::unique_ptr<CollectionNamedElementCache> m_namedElementCache;

    const unsigned m_collectionType : 5;
    const unsigned m_invalidationType : 4;
    const unsigned m_rootType : 1;
    const unsigned m_shouldOnlyIncludeDirectChildren : 1;
    const unsigned m_usesCustomForwardOnlyTraversal : 1;
};

} // namespace

#endif
