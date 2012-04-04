/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef HandleHeap_h
#define HandleHeap_h

#include <wtf/BlockStack.h>
#include "Handle.h"
#include <wtf/HashCountedSet.h>
#include <wtf/SentinelLinkedList.h>
#include <wtf/SinglyLinkedList.h>

namespace JSC {

class HandleHeap;
class HeapRootVisitor;
class JSGlobalData;
class JSValue;
class SlotVisitor;

class HandleHeap {
public:
    static HandleHeap* heapFor(HandleSlot);

    HandleHeap(JSGlobalData*);
    
    JSGlobalData* globalData();

    HandleSlot allocate();
    void deallocate(HandleSlot);

    void visitStrongHandles(HeapRootVisitor&);

    JS_EXPORT_PRIVATE void writeBarrier(HandleSlot, const JSValue&);

    unsigned protectedGlobalObjectCount();

    template<typename Functor> void forEachStrongHandle(Functor&, const HashCountedSet<JSCell*>& skipSet);

private:
    class Node {
    public:
        Node(WTF::SentinelTag);
        Node(HandleHeap*);
        
        HandleSlot slot();
        HandleHeap* handleHeap();

        void setPrev(Node*);
        Node* prev();

        void setNext(Node*);
        Node* next();

    private:
        JSValue m_value;
        HandleHeap* m_handleHeap;
        Node* m_prev;
        Node* m_next;
    };

    static HandleSlot toHandle(Node*);
    static Node* toNode(HandleSlot);

    JS_EXPORT_PRIVATE void grow();
    
#if ENABLE(GC_VALIDATION) || !ASSERT_DISABLED
    bool isLiveNode(Node*);
#endif

    JSGlobalData* m_globalData;
    BlockStack<Node> m_blockStack;

    SentinelLinkedList<Node> m_strongList;
    SentinelLinkedList<Node> m_immediateList;
    SinglyLinkedList<Node> m_freeList;
    Node* m_nextToFinalize;
};

inline HandleHeap* HandleHeap::heapFor(HandleSlot handle)
{
    return toNode(handle)->handleHeap();
}

inline JSGlobalData* HandleHeap::globalData()
{
    return m_globalData;
}

inline HandleSlot HandleHeap::toHandle(Node* node)
{
    return reinterpret_cast<HandleSlot>(node);
}

inline HandleHeap::Node* HandleHeap::toNode(HandleSlot handle)
{
    return reinterpret_cast<Node*>(handle);
}

inline HandleSlot HandleHeap::allocate()
{
    // Forbid assignment to handles during the finalization phase, since it would violate many GC invariants.
    // File a bug with stack trace if you hit this.
    if (m_nextToFinalize)
        CRASH();
    if (m_freeList.isEmpty())
        grow();

    Node* node = m_freeList.pop();
    new (NotNull, node) Node(this);
    m_immediateList.push(node);
    return toHandle(node);
}

inline void HandleHeap::deallocate(HandleSlot handle)
{
    Node* node = toNode(handle);
    if (node == m_nextToFinalize) {
        ASSERT(m_nextToFinalize->next());
        m_nextToFinalize = m_nextToFinalize->next();
    }

    SentinelLinkedList<Node>::remove(node);
    m_freeList.push(node);
}

inline HandleHeap::Node::Node(HandleHeap* handleHeap)
    : m_handleHeap(handleHeap)
    , m_prev(0)
    , m_next(0)
{
}

inline HandleHeap::Node::Node(WTF::SentinelTag)
    : m_handleHeap(0)
    , m_prev(0)
    , m_next(0)
{
}

inline HandleSlot HandleHeap::Node::slot()
{
    return &m_value;
}

inline HandleHeap* HandleHeap::Node::handleHeap()
{
    return m_handleHeap;
}

inline void HandleHeap::Node::setPrev(Node* prev)
{
    m_prev = prev;
}

inline HandleHeap::Node* HandleHeap::Node::prev()
{
    return m_prev;
}

inline void HandleHeap::Node::setNext(Node* next)
{
    m_next = next;
}

inline HandleHeap::Node* HandleHeap::Node::next()
{
    return m_next;
}

template<typename Functor> void HandleHeap::forEachStrongHandle(Functor& functor, const HashCountedSet<JSCell*>& skipSet)
{
    Node* end = m_strongList.end();
    for (Node* node = m_strongList.begin(); node != end; node = node->next()) {
        JSValue value = *node->slot();
        if (!value || !value.isCell())
            continue;
        if (skipSet.contains(value.asCell()))
            continue;
        functor(value.asCell());
    }
}

}

#endif
