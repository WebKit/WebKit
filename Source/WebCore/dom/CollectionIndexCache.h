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

template <class Collection, class NodeType>
class CollectionIndexCache {
public:
    CollectionIndexCache();

    unsigned nodeCount(const Collection&);
    NodeType* nodeAt(const Collection&, unsigned index);

    bool hasValidCache() const { return m_currentNode || m_nodeCountValid || m_listValid; }
    void invalidate();
    size_t memoryCost() { return m_cachedList.capacity() * sizeof(NodeType*); }

private:

    unsigned computeNodeCountUpdatingListCache(const Collection&);
    NodeType* nodeBeforeCached(const Collection&, unsigned);
    NodeType* nodeAfterCached(const Collection&, unsigned);

    NodeType* m_currentNode;
    unsigned m_currentIndex;
    unsigned m_nodeCount;
    Vector<NodeType*> m_cachedList;
    bool m_nodeCountValid : 1;
    bool m_listValid : 1;
};

template <class Collection, class NodeType>
inline CollectionIndexCache<Collection, NodeType>::CollectionIndexCache()
    : m_currentNode(nullptr)
    , m_currentIndex(0)
    , m_nodeCount(0)
    , m_nodeCountValid(false)
    , m_listValid(false)
{
}

template <class Collection, class NodeType>
inline unsigned CollectionIndexCache<Collection, NodeType>::nodeCount(const Collection& collection)
{
    if (!m_nodeCountValid) {
        if (!hasValidCache())
            collection.willValidateIndexCache();
        m_nodeCount = computeNodeCountUpdatingListCache(collection);
        m_nodeCountValid = true;
    }

    return m_nodeCount;
}

template <class Collection, class NodeType>
unsigned CollectionIndexCache<Collection, NodeType>::computeNodeCountUpdatingListCache(const Collection& collection)
{
    NodeType* first = collection.collectionFirst();
    if (!first)
        return 0;

    unsigned oldCapacity = m_cachedList.capacity();
    NodeType* currentNode = first;
    while (currentNode) {
        m_cachedList.append(currentNode);
        unsigned traversed;
        currentNode = collection.collectionTraverseForward(*currentNode, 1, traversed);
        ASSERT(traversed == (currentNode ? 1 : 0));
    }
    m_listValid = true;

    if (unsigned capacityDifference = m_cachedList.capacity() - oldCapacity)
        reportExtraMemoryCostForCollectionIndexCache(capacityDifference * sizeof(NodeType*));

    return m_cachedList.size();
}

template <class Collection, class NodeType>
inline NodeType* CollectionIndexCache<Collection, NodeType>::nodeBeforeCached(const Collection& collection, unsigned index)
{
    ASSERT(m_currentNode);
    ASSERT(index < m_currentIndex);

    bool firstIsCloser = index < m_currentIndex - index;
    if (firstIsCloser || !collection.collectionCanTraverseBackward()) {
        m_currentNode = collection.collectionFirst();
        m_currentIndex = 0;
        if (index)
            m_currentNode = collection.collectionTraverseForward(*m_currentNode, index, m_currentIndex);
        ASSERT(m_currentNode);
        return m_currentNode;
    }

    m_currentNode = collection.collectionTraverseBackward(*m_currentNode, m_currentIndex - index);
    m_currentIndex = index;

    ASSERT(m_currentNode);
    return m_currentNode;
}

template <class Collection, class NodeType>
inline NodeType* CollectionIndexCache<Collection, NodeType>::nodeAfterCached(const Collection& collection, unsigned index)
{
    ASSERT(m_currentNode);
    ASSERT(index > m_currentIndex);
    ASSERT(!m_nodeCountValid || index < m_nodeCount);

    bool lastIsCloser = m_nodeCountValid && m_nodeCount - index < index - m_currentIndex;
    if (lastIsCloser && collection.collectionCanTraverseBackward()) {
        ASSERT(hasValidCache());
        m_currentNode = collection.collectionLast();
        if (index < m_nodeCount - 1)
            m_currentNode = collection.collectionTraverseBackward(*m_currentNode, m_nodeCount - index - 1);
        m_currentIndex = index;
        ASSERT(m_currentNode);
        return m_currentNode;
    }

    if (!hasValidCache())
        collection.willValidateIndexCache();

    unsigned traversedCount;
    m_currentNode = collection.collectionTraverseForward(*m_currentNode, index - m_currentIndex, traversedCount);
    m_currentIndex = m_currentIndex + traversedCount;

    ASSERT(m_currentNode ||  m_currentIndex < index);

    if (!m_currentNode && !m_nodeCountValid) {
        // Failed to find the index but at least we now know the size.
        m_nodeCount = m_currentIndex + 1;
        m_nodeCountValid = true;
    }
    ASSERT(hasValidCache());
    return m_currentNode;
}

template <class Collection, class NodeType>
inline NodeType* CollectionIndexCache<Collection, NodeType>::nodeAt(const Collection& collection, unsigned index)
{
    if (m_nodeCountValid && index >= m_nodeCount)
        return nullptr;

    if (m_listValid)
        return m_cachedList[index];

    if (m_currentNode) {
        if (index > m_currentIndex)
            return nodeAfterCached(collection, index);
        if (index < m_currentIndex)
            return nodeBeforeCached(collection, index);
        return m_currentNode;
    }

    bool lastIsCloser = m_nodeCountValid && m_nodeCount - index < index;
    if (lastIsCloser && collection.collectionCanTraverseBackward()) {
        ASSERT(hasValidCache());
        m_currentNode = collection.collectionLast();
        if (index < m_nodeCount - 1)
            m_currentNode = collection.collectionTraverseBackward(*m_currentNode, m_nodeCount - index - 1);
        m_currentIndex = index;
        ASSERT(m_currentNode);
        return m_currentNode;
    }

    if (!hasValidCache())
        collection.willValidateIndexCache();

    m_currentNode = collection.collectionFirst();
    m_currentIndex = 0;
    if (index && m_currentNode) {
        m_currentNode = collection.collectionTraverseForward(*m_currentNode, index, m_currentIndex);
        ASSERT(m_currentNode || m_currentIndex < index);
    }
    if (!m_currentNode && !m_nodeCountValid) {
        // Failed to find the index but at least we now know the size.
        m_nodeCount = m_currentIndex + 1;
        m_nodeCountValid = true;
    }
    ASSERT(hasValidCache());
    return m_currentNode;
}

template <class Collection, class NodeType>
void CollectionIndexCache<Collection, NodeType>::invalidate()
{
    m_currentNode = nullptr;
    m_nodeCountValid = false;
    m_listValid = false;
    m_cachedList.shrink(0);
}


}

#endif
