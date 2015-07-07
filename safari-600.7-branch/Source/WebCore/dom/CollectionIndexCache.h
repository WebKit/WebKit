/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CollectionIndexCache_h
#define CollectionIndexCache_h

#include <wtf/Vector.h>

namespace WebCore {

void reportExtraMemoryCostForCollectionIndexCache(size_t);

template <class Collection, class Iterator>
class CollectionIndexCache {
public:
    explicit CollectionIndexCache(const Collection&);

    typedef typename std::iterator_traits<Iterator>::value_type NodeType;

    unsigned nodeCount(const Collection&);
    NodeType* nodeAt(const Collection&, unsigned index);

    bool hasValidCache(const Collection& collection) const { return m_current != collection.collectionEnd() || m_nodeCountValid || m_listValid; }
    void invalidate(const Collection&);
    size_t memoryCost() { return m_cachedList.capacity() * sizeof(NodeType*); }

private:
    unsigned computeNodeCountUpdatingListCache(const Collection&);
    NodeType* traverseBackwardTo(const Collection&, unsigned);
    NodeType* traverseForwardTo(const Collection&, unsigned);

    Iterator m_current;
    unsigned m_currentIndex;
    unsigned m_nodeCount;
    Vector<NodeType*> m_cachedList;
    bool m_nodeCountValid : 1;
    bool m_listValid : 1;
};

template <class Collection, class Iterator>
inline CollectionIndexCache<Collection, Iterator>::CollectionIndexCache(const Collection& collection)
    : m_current(collection.collectionEnd())
    , m_currentIndex(0)
    , m_nodeCount(0)
    , m_nodeCountValid(false)
    , m_listValid(false)
{
}

template <class Collection, class Iterator>
inline unsigned CollectionIndexCache<Collection, Iterator>::nodeCount(const Collection& collection)
{
    if (!m_nodeCountValid) {
        if (!hasValidCache(collection))
            collection.willValidateIndexCache();
        m_nodeCount = computeNodeCountUpdatingListCache(collection);
        m_nodeCountValid = true;
    }

    return m_nodeCount;
}

template <class Collection, class Iterator>
unsigned CollectionIndexCache<Collection, Iterator>::computeNodeCountUpdatingListCache(const Collection& collection)
{
    auto current = collection.collectionBegin();
    auto end = collection.collectionEnd();
    if (current == end)
        return 0;

    unsigned oldCapacity = m_cachedList.capacity();
    while (current != end) {
        m_cachedList.append(&*current);
        unsigned traversed;
        collection.collectionTraverseForward(current, 1, traversed);
        ASSERT(traversed == (current != end ? 1 : 0));
    }
    m_listValid = true;

    if (unsigned capacityDifference = m_cachedList.capacity() - oldCapacity)
        reportExtraMemoryCostForCollectionIndexCache(capacityDifference * sizeof(NodeType*));

    return m_cachedList.size();
}

template <class Collection, class Iterator>
inline typename CollectionIndexCache<Collection, Iterator>::NodeType* CollectionIndexCache<Collection, Iterator>::traverseBackwardTo(const Collection& collection, unsigned index)
{
    ASSERT(m_current != collection.collectionEnd());
    ASSERT(index < m_currentIndex);

    bool firstIsCloser = index < m_currentIndex - index;
    if (firstIsCloser || !collection.collectionCanTraverseBackward()) {
        m_current = collection.collectionBegin();
        m_currentIndex = 0;
        if (index)
            collection.collectionTraverseForward(m_current, index, m_currentIndex);
        ASSERT(m_current != collection.collectionEnd());
        return &*m_current;
    }

    collection.collectionTraverseBackward(m_current, m_currentIndex - index);
    m_currentIndex = index;

    ASSERT(m_current != collection.collectionEnd());
    return &*m_current;
}

template <class Collection, class Iterator>
inline typename CollectionIndexCache<Collection, Iterator>::NodeType* CollectionIndexCache<Collection, Iterator>::traverseForwardTo(const Collection& collection, unsigned index)
{
    ASSERT(m_current != collection.collectionEnd());
    ASSERT(index > m_currentIndex);
    ASSERT(!m_nodeCountValid || index < m_nodeCount);

    bool lastIsCloser = m_nodeCountValid && m_nodeCount - index < index - m_currentIndex;
    if (lastIsCloser && collection.collectionCanTraverseBackward()) {
        ASSERT(hasValidCache(collection));
        m_current = collection.collectionLast();
        if (index < m_nodeCount - 1)
            collection.collectionTraverseBackward(m_current, m_nodeCount - index - 1);
        m_currentIndex = index;
        ASSERT(m_current != collection.collectionEnd());
        return &*m_current;
    }

    if (!hasValidCache(collection))
        collection.willValidateIndexCache();

    unsigned traversedCount;
    collection.collectionTraverseForward(m_current, index - m_currentIndex, traversedCount);
    m_currentIndex = m_currentIndex + traversedCount;

    if (m_current == collection.collectionEnd()) {
        ASSERT(m_currentIndex < index);
        // Failed to find the index but at least we now know the size.
        m_nodeCount = m_currentIndex + 1;
        m_nodeCountValid = true;
        return nullptr;
    }
    ASSERT(hasValidCache(collection));
    return &*m_current;
}

template <class Collection, class Iterator>
inline typename CollectionIndexCache<Collection, Iterator>::NodeType* CollectionIndexCache<Collection, Iterator>::nodeAt(const Collection& collection, unsigned index)
{
    if (m_nodeCountValid && index >= m_nodeCount)
        return nullptr;

    if (m_listValid)
        return m_cachedList[index];

    auto end = collection.collectionEnd();
    if (m_current != end) {
        if (index > m_currentIndex)
            return traverseForwardTo(collection, index);
        if (index < m_currentIndex)
            return traverseBackwardTo(collection, index);
        return &*m_current;
    }

    bool lastIsCloser = m_nodeCountValid && m_nodeCount - index < index;
    if (lastIsCloser && collection.collectionCanTraverseBackward()) {
        ASSERT(hasValidCache(collection));
        m_current = collection.collectionLast();
        if (index < m_nodeCount - 1)
            collection.collectionTraverseBackward(m_current, m_nodeCount - index - 1);
        m_currentIndex = index;
        ASSERT(m_current != end);
        return &*m_current;
    }

    if (!hasValidCache(collection))
        collection.willValidateIndexCache();

    m_current = collection.collectionBegin();
    m_currentIndex = 0;
    if (index && m_current != end) {
        collection.collectionTraverseForward(m_current, index, m_currentIndex);
        ASSERT(m_current != end || m_currentIndex < index);
    }
    if (m_current == end) {
        // Failed to find the index but at least we now know the size.
        m_nodeCount = m_currentIndex + 1;
        m_nodeCountValid = true;
        return nullptr;
    }
    ASSERT(hasValidCache(collection));
    return &*m_current;
}

template <class Collection, class Iterator>
void CollectionIndexCache<Collection, Iterator>::invalidate(const Collection& collection)
{
    m_current = collection.collectionEnd();
    m_nodeCountValid = false;
    m_listValid = false;
    m_cachedList.shrink(0);
}


}

#endif
