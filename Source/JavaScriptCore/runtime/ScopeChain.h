/*
 *  Copyright (C) 2003, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef ScopeChain_h
#define ScopeChain_h

#include "FastAllocBase.h"

namespace JSC {

    class JSGlobalData;
    class JSGlobalObject;
    class JSObject;
    class MarkStack;
    class ScopeChainIterator;
    
    class ScopeChainNode : public FastAllocBase {
    public:
        ScopeChainNode(ScopeChainNode* next, JSObject* object, JSGlobalData* globalData, JSGlobalObject* globalObject, JSObject* globalThis)
            : next(next)
            , object(object)
            , globalData(globalData)
            , globalObject(globalObject)
            , globalThis(globalThis)
            , refCount(1)
        {
            ASSERT(globalData);
            ASSERT(globalObject);
        }
#ifndef NDEBUG
        // Due to the number of subtle and timing dependent bugs that have occurred due
        // to deleted but still "valid" ScopeChainNodes we now deliberately clobber the
        // contents in debug builds.
        ~ScopeChainNode()
        {
            next = 0;
            object = 0;
            globalData = 0;
            globalObject = 0;
            globalThis = 0;
        }
#endif

        ScopeChainNode* next;
        JSObject* object;
        JSGlobalData* globalData;
        JSGlobalObject* globalObject;
        JSObject* globalThis;
        int refCount;

        void deref() { ASSERT(refCount); if (--refCount == 0) { release();} }
        void ref() { ASSERT(refCount); ++refCount; }
        void release();

        // Before calling "push" on a bare ScopeChainNode, a client should
        // logically "copy" the node. Later, the client can "deref" the head
        // of its chain of ScopeChainNodes to reclaim all the nodes it added
        // after the logical copy, leaving nodes added before the logical copy
        // (nodes shared with other clients) untouched.
        ScopeChainNode* copy()
        {
            ref();
            return this;
        }

        ScopeChainNode* push(JSObject*);
        ScopeChainNode* pop();

        ScopeChainIterator begin() const;
        ScopeChainIterator end() const;

#ifndef NDEBUG        
        void print() const;
#endif
    };

    inline ScopeChainNode* ScopeChainNode::push(JSObject* o)
    {
        ASSERT(o);
        return new ScopeChainNode(this, o, globalData, globalObject, globalThis);
    }

    inline ScopeChainNode* ScopeChainNode::pop()
    {
        ASSERT(next);
        ScopeChainNode* result = next;

        if (--refCount != 0)
            ++result->refCount;
        else
            delete this;

        return result;
    }

    inline void ScopeChainNode::release()
    {
        // This function is only called by deref(),
        // Deref ensures these conditions are true.
        ASSERT(refCount == 0);
        ScopeChainNode* n = this;
        do {
            ScopeChainNode* next = n->next;
            delete n;
            n = next;
        } while (n && --n->refCount == 0);
    }

    class ScopeChainIterator {
    public:
        ScopeChainIterator(const ScopeChainNode* node)
            : m_node(node)
        {
        }

        JSObject* const & operator*() const { return m_node->object; }
        JSObject* const * operator->() const { return &(operator*()); }
    
        ScopeChainIterator& operator++() { m_node = m_node->next; return *this; }

        // postfix ++ intentionally omitted

        bool operator==(const ScopeChainIterator& other) const { return m_node == other.m_node; }
        bool operator!=(const ScopeChainIterator& other) const { return m_node != other.m_node; }

    private:
        const ScopeChainNode* m_node;
    };

    inline ScopeChainIterator ScopeChainNode::begin() const
    {
        return ScopeChainIterator(this); 
    }

    inline ScopeChainIterator ScopeChainNode::end() const
    { 
        return ScopeChainIterator(0); 
    }

    class NoScopeChain {};

    class ScopeChain {
        friend class JIT;
    public:
        ScopeChain(NoScopeChain)
            : m_node(0)
        {
        }

        ScopeChain(JSObject* o, JSGlobalData* globalData, JSGlobalObject* globalObject, JSObject* globalThis)
            : m_node(new ScopeChainNode(0, o, globalData, globalObject, globalThis))
        {
        }

        ScopeChain(const ScopeChain& c)
            : m_node(c.m_node->copy())
        {
        }

        ScopeChain& operator=(const ScopeChain& c);

        explicit ScopeChain(ScopeChainNode* node)
            : m_node(node->copy())
        {
        }

        ~ScopeChain()
        {
            if (m_node)
                m_node->deref();
#ifndef NDEBUG
            m_node = 0;
#endif
        }

        void swap(ScopeChain&);

        ScopeChainNode* node() const { return m_node; }

        JSObject* top() const { return m_node->object; }

        ScopeChainIterator begin() const { return m_node->begin(); }
        ScopeChainIterator end() const { return m_node->end(); }

        void push(JSObject* o) { m_node = m_node->push(o); }

        void pop() { m_node = m_node->pop(); }
        void clear() { m_node->deref(); m_node = 0; }
        
        JSGlobalObject* globalObject() const { return m_node->globalObject; }

        void markAggregate(MarkStack&) const;

        // Caution: this should only be used if the codeblock this is being used
        // with needs a full scope chain, otherwise this returns the depth of
        // the preceeding call frame
        //
        // Returns the depth of the current call frame's scope chain
        int localDepth() const;

#ifndef NDEBUG        
        void print() const { m_node->print(); }
#endif

    private:
        ScopeChainNode* m_node;
    };

    inline void ScopeChain::swap(ScopeChain& o)
    {
        ScopeChainNode* tmp = m_node;
        m_node = o.m_node;
        o.m_node = tmp;
    }

    inline ScopeChain& ScopeChain::operator=(const ScopeChain& c)
    {
        ScopeChain tmp(c);
        swap(tmp);
        return *this;
    }

} // namespace JSC

#endif // ScopeChain_h
