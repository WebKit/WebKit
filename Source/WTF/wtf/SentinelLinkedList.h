/*
 * Copyright (C) 2011, 2016 Apple Inc. All rights reserved.
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

//    A SentinelLinkedList is a linked list with dummy head and tail sentinels,
//    which allow for branch-less insertion and removal, and removal without a
//    pointer to the list.
//    
//    Requires: Node is a concrete class with:
//        Node(SentinelTag);
//        void setPrev(Node*);
//        Node* prev() const;
//        void setNext(Node*);
//        Node* next() const;

#pragma once

#include <iterator>
#include <wtf/Noncopyable.h>
#include <wtf/Nonmovable.h>
#include <wtf/Packed.h>

namespace WTF {

enum SentinelTag { Sentinel };

template<typename T, typename PassedPtrTraits = RawPtrTraits<T>>
class BasicRawSentinelNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using PtrTraits = typename PassedPtrTraits::template RebindTraits<BasicRawSentinelNode>;

    BasicRawSentinelNode(SentinelTag)
    {
    }
    
    BasicRawSentinelNode() = default;
    
    void setPrev(BasicRawSentinelNode* prev) { m_prev = prev; }
    void setNext(BasicRawSentinelNode* next) { m_next = next; }
    
    T* prev() const { return static_cast<T*>(PtrTraits::unwrap(m_prev)); }
    T* next() const { return static_cast<T*>(PtrTraits::unwrap(m_next)); }
    
    bool isOnList() const
    {
        ASSERT(!!m_prev == !!m_next);
        return !!m_prev;
    }
    
    void remove();

    void prepend(BasicRawSentinelNode*);
    void append(BasicRawSentinelNode*);
    
private:
    typename PtrTraits::StorageType m_next { nullptr };
    typename PtrTraits::StorageType m_prev { nullptr };
};

template <typename T, typename RawNode = T> class SentinelLinkedList {
    WTF_MAKE_NONCOPYABLE(SentinelLinkedList);
    WTF_MAKE_NONMOVABLE(SentinelLinkedList);
public:
    template<typename RawNodeType, typename NodeType> class BaseIterator {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        explicit BaseIterator(RawNodeType* node)
            : m_node(node)
        {
        }
        
        auto& operator*() const { return static_cast<NodeType&>(*m_node); }

        auto* operator->() const { return static_cast<NodeType*>(m_node); }

        BaseIterator& operator++()
        {
            m_node = m_node->next();
            return *this;
        }
        
        BaseIterator& operator--()
        {
            m_node = m_node->prev();
            return *this;
        }
        
        bool operator==(const BaseIterator& other) const
        {
            return m_node == other.m_node;
        }

        bool operator!=(const BaseIterator& other) const
        {
            return !(*this == other);
        }

    private:
        RawNodeType* m_node;
    };

    using iterator = BaseIterator<RawNode, T>;
    using const_iterator = BaseIterator<const RawNode, const T>;

    SentinelLinkedList()
        : m_sentinel(Sentinel)
    {
        m_sentinel.setPrev(&m_sentinel);
        m_sentinel.setNext(&m_sentinel);
    }

    // Pushes to the front of the list. It's totally backwards from what you'd expect.
    void push(T*);

    // Appends to the end of the list.
    void append(T*);
    
    static void remove(T*);
    static void prepend(T* existingNode, T* newNode);
    static void append(T* existingNode, T* newNode);
    
    bool isOnList(T*);

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    
    bool isEmpty() { return begin() == end(); }
    
    template<typename Func>
    void forEach(const Func& func)
    {
        for (iterator iter = begin(); iter != end();) {
            iterator next = iter;
            ++next;
            func(&*iter);
            iter = next;
        }
    }
    
    void takeFrom(SentinelLinkedList<T, RawNode>&);
    
private:
    RawNode m_sentinel;
};

template <typename T, typename PtrTraits> void BasicRawSentinelNode<T, PtrTraits>::remove()
{
    SentinelLinkedList<T, BasicRawSentinelNode>::remove(static_cast<T*>(this));
}

template <typename T, typename PtrTraits> void BasicRawSentinelNode<T, PtrTraits>::prepend(BasicRawSentinelNode* node)
{
    SentinelLinkedList<T, BasicRawSentinelNode>::prepend(
        static_cast<T*>(this), static_cast<T*>(node));
}

template <typename T, typename PtrTraits> void BasicRawSentinelNode<T, PtrTraits>::append(BasicRawSentinelNode* node)
{
    SentinelLinkedList<T, BasicRawSentinelNode>::append(
        static_cast<T*>(this), static_cast<T*>(node));
}

template <typename T, typename RawNode> inline typename SentinelLinkedList<T, RawNode>::iterator SentinelLinkedList<T, RawNode>::begin()
{
    return iterator { m_sentinel.next() };
}

template <typename T, typename RawNode> inline typename SentinelLinkedList<T, RawNode>::iterator SentinelLinkedList<T, RawNode>::end()
{
    return iterator { &m_sentinel };
}

template <typename T, typename RawNode> inline typename SentinelLinkedList<T, RawNode>::const_iterator SentinelLinkedList<T, RawNode>::begin() const
{
    return const_iterator { m_sentinel.next() };
}

template <typename T, typename RawNode> inline typename SentinelLinkedList<T, RawNode>::const_iterator SentinelLinkedList<T, RawNode>::end() const
{
    return const_iterator { &m_sentinel };
}

template <typename T, typename RawNode> inline void SentinelLinkedList<T, RawNode>::push(T* node)
{
    ASSERT(node);
    ASSERT(!node->prev());
    ASSERT(!node->next());
    
    RawNode* prev = &m_sentinel;
    RawNode* next = m_sentinel.next();

    node->setPrev(prev);
    node->setNext(next);

    prev->setNext(node);
    next->setPrev(node);
}

template <typename T, typename RawNode> inline void SentinelLinkedList<T, RawNode>::append(T* node)
{
    ASSERT(node);
    ASSERT(!node->prev());
    ASSERT(!node->next());
    
    RawNode* prev = m_sentinel.prev();
    RawNode* next = &m_sentinel;

    node->setPrev(prev);
    node->setNext(next);

    prev->setNext(node);
    next->setPrev(node);
}

template <typename T, typename RawNode> inline void SentinelLinkedList<T, RawNode>::remove(T* node)
{
    ASSERT(node);
    ASSERT(!!node->prev());
    ASSERT(!!node->next());
    
    RawNode* prev = node->prev();
    RawNode* next = node->next();

    prev->setNext(next);
    next->setPrev(prev);
    
    node->setPrev(nullptr);
    node->setNext(nullptr);
}

template <typename T, typename RawNode>
inline void SentinelLinkedList<T, RawNode>::prepend(T* existingNode, T* newNode)
{
    ASSERT(existingNode);
    ASSERT(!!existingNode->prev());
    ASSERT(!!existingNode->next());
    ASSERT(newNode);
    ASSERT(!newNode->prev());
    ASSERT(!newNode->next());

    RawNode* prev = existingNode->prev();

    newNode->setNext(existingNode);
    newNode->setPrev(prev);
    
    prev->setNext(newNode);
    existingNode->setPrev(newNode);
}

template <typename T, typename RawNode>
inline void SentinelLinkedList<T, RawNode>::append(T* existingNode, T* newNode)
{
    ASSERT(existingNode);
    ASSERT(!!existingNode->prev());
    ASSERT(!!existingNode->next());
    ASSERT(newNode);
    ASSERT(!newNode->prev());
    ASSERT(!newNode->next());

    RawNode* next = existingNode->next();

    newNode->setNext(next);
    newNode->setPrev(existingNode);
    
    next->setPrev(newNode);
    existingNode->setNext(newNode);
}

template <typename T, typename RawNode> inline bool SentinelLinkedList<T, RawNode>::isOnList(T* node)
{
    if (!node->isOnList())
        return false;
    
    for (T* iter = begin(); iter != end(); iter = iter->next()) {
        if (iter == node)
            return true;
    }
    
    return false;
}

template <typename T, typename RawNode>
inline void SentinelLinkedList<T, RawNode>::takeFrom(SentinelLinkedList<T, RawNode>& other)
{
    if (other.isEmpty())
        return;

    m_sentinel.prev()->setNext(other.m_sentinel.next());
    other.m_sentinel.next()->setPrev(m_sentinel.prev());

    m_sentinel.setPrev(other.m_sentinel.prev());
    m_sentinel.prev()->setNext(&m_sentinel);

    other.m_sentinel.setNext(&other.m_sentinel);
    other.m_sentinel.setPrev(&other.m_sentinel);
}

template<typename T>
using PackedRawSentinelNode = BasicRawSentinelNode<T, PackedPtrTraits<T>>;

}

using WTF::BasicRawSentinelNode;
using WTF::PackedRawSentinelNode;
using WTF::SentinelLinkedList;
