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
    class ExecState;
    
    class ScopeChainNode {
    public:
        ScopeChainNode(ScopeChainNode *n, JSObject *o)
            : next(n), object(o), refCount(1) { }

        ScopeChainNode *next;
        JSObject *object;
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

        ScopeChain(const ScopeChain &c) : _node(c._node)
            { if (_node) ++_node->refCount; }
        ScopeChain &operator=(const ScopeChain &);

        bool isEmpty() const { return !_node; }
        JSObject *top() const { return _node->object; }

        JSObject *bottom() const;

        ScopeChainIterator begin() const { return ScopeChainIterator(_node); }
        ScopeChainIterator end() const { return ScopeChainIterator(0); }

        void clear() { deref(); _node = 0; }
        void push(JSObject *);
        void push(const ScopeChain &);
        void replaceTop(JSObject*);
        void pop();
        
        void mark();

#ifndef NDEBUG        
        void print();
#endif
        
    private:
        ScopeChainNode *_node;
        
        void deref() { if (_node && --_node->refCount == 0) release(); }
        void ref() const;
        
        void release();
    };

inline void ScopeChain::ref() const
{
    for (ScopeChainNode *n = _node; n; n = n->next) {
        if (n->refCount++ != 0)
            break;
    }
}

inline ScopeChain &ScopeChain::operator=(const ScopeChain &c)
{
    c.ref();
    deref();
    _node = c._node;
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

inline void ScopeChain::push(JSObject *o)
{
    ASSERT(o);
    _node = new ScopeChainNode(_node, o);
}

inline void ScopeChain::replaceTop(JSObject* o)
{
    ASSERT(o);
    _node->object = o;
}

inline void ScopeChain::pop()
{
    ScopeChainNode *oldNode = _node;
    ASSERT(oldNode);
    ScopeChainNode *newNode = oldNode->next;
    _node = newNode;
    
    if (--oldNode->refCount != 0) {
        if (newNode)
            ++newNode->refCount;
    } else {
        delete oldNode;
    }
}

} // namespace KJS

#endif // KJS_SCOPE_CHAIN_H
