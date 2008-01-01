/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef KJS_SCOPE_CHAIN_H
#define KJS_SCOPE_CHAIN_H

#include <wtf/Assertions.h>

namespace KJS {

    class JSObject;
    struct ScopeChainHeapNode;
    
    struct ScopeChainNode {
        JSObject* object;
        ScopeChainHeapNode* next;
    };

    struct ScopeChainHeapNode : ScopeChainNode {
        int refCount;
    };

    class ScopeChainIterator {
    public:
        ScopeChainIterator(ScopeChainNode *node) : m_node(node) {}

        JSObject * const & operator*() const { return m_node->object; }
        JSObject * const * operator->() const { return &(operator*()); }
    
        ScopeChainIterator& operator++() { m_node = m_node->next; return *this; }

        // postfix ++ intentionally omitted

        bool operator==(const ScopeChainIterator& other) const { return m_node == other.m_node; }
        bool operator!=(const ScopeChainIterator& other) const { return m_node != other.m_node; }

    private:
        ScopeChainNode *m_node;
    };

    class ScopeChain {
    public:
        typedef ScopeChainIterator const_iterator;
        typedef JSObject* ValueType;

        ScopeChain() : _node(0) { }
        ~ScopeChain() { deref(); }

        ScopeChain(const ScopeChain&);
        ScopeChain &operator=(const ScopeChain &);

        bool isEmpty() const { return !_node; }
        JSObject *top() const { return _node->object; }

        JSObject *bottom() const;

        ScopeChainIterator begin() const { return ScopeChainIterator(_node); }
        ScopeChainIterator end() const { return ScopeChainIterator(0); }

        void clear() { deref(); _node = 0; }
        void push(JSObject *);
        void pop();
        
        void mark();

#ifndef NDEBUG        
        void print();
#endif
        
    private:
        mutable ScopeChainNode* _node;
        ScopeChainNode m_initialTopNode;

        ScopeChainHeapNode* moveToHeap() const;

        void deref();
        void ref() const;
    };

inline ScopeChainHeapNode* ScopeChain::moveToHeap() const
{
    if (_node != &m_initialTopNode)
        return static_cast<ScopeChainHeapNode*>(_node);
    ScopeChainHeapNode* heapNode = new ScopeChainHeapNode;
    heapNode->object = m_initialTopNode.object;
    heapNode->next = m_initialTopNode.next;
    heapNode->refCount = 1;
    _node = heapNode;
    return heapNode;
}

inline ScopeChain::ScopeChain(const ScopeChain& otherChain)
{
    if (!otherChain._node)
        _node = 0;
    else {
        ScopeChainHeapNode* top = otherChain.moveToHeap();
        ++top->refCount;
        _node = top;
    }
}

inline void ScopeChain::ref() const
{
    ASSERT(_node != &m_initialTopNode);
    for (ScopeChainHeapNode* n = static_cast<ScopeChainHeapNode*>(_node); n; n = n->next) {
        if (n->refCount++)
            break;
    }
}

inline void ScopeChain::deref()
{
    ScopeChainHeapNode* node = static_cast<ScopeChainHeapNode*>(_node);
    if (node == &m_initialTopNode)
        node = node->next;
    ScopeChainHeapNode* next;
    for (; node && --node->refCount == 0; node = next) {
        next = node->next;
        delete node;
    }
}

inline ScopeChain &ScopeChain::operator=(const ScopeChain& otherChain)
{
    otherChain.moveToHeap();
    otherChain.ref();
    deref();
    _node = otherChain._node;
    return *this;
}

inline JSObject *ScopeChain::bottom() const
{
    ScopeChainNode *last = 0;
    for (ScopeChainNode *n = _node; n; n = n->next)
        last = n;
    if (!last)
        return 0;
    return last->object;
}

inline void ScopeChain::push(JSObject* o)
{
    ASSERT(o);
    ScopeChainHeapNode* heapNode = moveToHeap();
    m_initialTopNode.object = o;
    m_initialTopNode.next = heapNode;
    _node = &m_initialTopNode;
}

inline void ScopeChain::pop()
{
    ScopeChainNode *oldNode = _node;
    ASSERT(oldNode);
    ScopeChainHeapNode *newNode = oldNode->next;
    _node = newNode;

    if (oldNode != &m_initialTopNode) {
        ScopeChainHeapNode* oldHeapNode = static_cast<ScopeChainHeapNode*>(oldNode);
        if (--oldHeapNode->refCount != 0) {
            if (newNode)
                ++newNode->refCount;
        } else {
            delete oldHeapNode;
        }
    }
}

} // namespace KJS

#endif // KJS_SCOPE_CHAIN_H
