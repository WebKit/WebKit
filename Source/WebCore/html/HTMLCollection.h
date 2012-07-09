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

#include "Node.h"
#include "CollectionType.h"
#include "DynamicNodeList.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class Document;
class Element;
class NodeList;

class HTMLCollectionCacheBase : public DynamicNodeListCacheBase {
public:
    HTMLCollectionCacheBase(CollectionType type, bool includeChildren)
        : DynamicNodeListCacheBase(RootedAtNode, AlwaysInvalidate) // These two flags are never used
        , m_cachedElementsArrayOffset(0)
        , m_cacheTreeVersion(0)
        , m_hasNameCache(false)
        , m_type(type)
        , m_includeChildren(includeChildren)
    {
        ASSERT(static_cast<CollectionType>(m_type) == type);
    }

    CollectionType type() const { return static_cast<CollectionType>(m_type); }

protected:
    void clearCache(uint64_t currentDomTreeVersion) const
    {
        DynamicNodeListCacheBase::clearCache();
        m_idCache.clear();
        m_nameCache.clear();
        m_cachedElementsArrayOffset = 0;
        m_cacheTreeVersion = currentDomTreeVersion;
        m_hasNameCache = false;
    }

    using DynamicNodeListCacheBase::setItemCache;
    void setItemCache(Node* item, unsigned offset, unsigned elementsArrayOffset) const
    {
        setItemCache(item, offset);
        m_cachedElementsArrayOffset = elementsArrayOffset;
    }
    unsigned cachedElementsArrayOffset() const { return m_cachedElementsArrayOffset; }

    bool includeChildren() const { return m_includeChildren; }
    uint64_t cacheTreeVersion() const { return m_cacheTreeVersion; }

    typedef HashMap<AtomicStringImpl*, OwnPtr<Vector<Element*> > > NodeCacheMap;
    Vector<Element*>* idCache(const AtomicString& name) const { return m_idCache.get(name.impl()); }
    Vector<Element*>* nameCache(const AtomicString& name) const { return m_nameCache.get(name.impl()); }
    void appendIdCache(const AtomicString& name, Element* element) const { append(m_idCache, name, element); }
    void appendNameCache(const AtomicString& name, Element* element) const { append(m_nameCache, name, element); }

    bool hasNameCache() const { return m_hasNameCache; }
    void setHasNameCache() const { m_hasNameCache = true; }

    static void append(NodeCacheMap&, const AtomicString&, Element*);

private:
    using DynamicNodeListCacheBase::isRootedAtDocument;
    using DynamicNodeListCacheBase::shouldInvalidateOnAttributeChange;
    using DynamicNodeListCacheBase::clearCache;

    mutable NodeCacheMap m_idCache;
    mutable NodeCacheMap m_nameCache;
    mutable unsigned m_cachedElementsArrayOffset;
    mutable uint64_t m_cacheTreeVersion;

    // FIXME: Move these bit flags to DynamicNodeListCacheBase to pack them better.
    mutable unsigned m_hasNameCache : 1;
    const unsigned m_type : 5; // CollectionType
    const unsigned m_includeChildren : 1;
};

class HTMLCollection : public RefCounted<HTMLCollection>, public HTMLCollectionCacheBase {
public:
    static PassRefPtr<HTMLCollection> create(Node* base, CollectionType);
    virtual ~HTMLCollection();

    // DOM API
    unsigned length() const;
    virtual Node* item(unsigned index) const;
    virtual Node* namedItem(const AtomicString& name) const;
    PassRefPtr<NodeList> tags(const String&);

    // Non-DOM API
    virtual bool hasNamedItem(const AtomicString& name) const;
    void namedItems(const AtomicString& name, Vector<RefPtr<Node> >&) const;
    bool isEmpty() const
    {
        invalidateCacheIfNeeded();
        if (isLengthCacheValid())
            return !cachedLength();
        if (isItemCacheValid())
            return !cachedItem();
        return !item(0);
    }
    bool hasExactlyOneItem() const
    {
        invalidateCacheIfNeeded();
        if (isLengthCacheValid())
            return cachedLength() == 1;
        if (isItemCacheValid())
            return cachedItem() && !cachedItemOffset() && !item(1);
        return item(0) && !item(1);
    }

    Node* base() const { return m_base.get(); }

    void invalidateCache();
    void invalidateCacheIfNeeded() const;
protected:
    HTMLCollection(Node* base, CollectionType);

    virtual void updateNameCache() const;
    virtual Element* itemAfter(Node*) const;

private:
    bool checkForNameMatch(Element*, bool checkName, const AtomicString& name) const;

    virtual unsigned calcLength() const;

    bool isAcceptableElement(Element*) const;

    RefPtr<Node> m_base;
};

} // namespace

#endif
