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

#include "BlockStack.h"
#include "Handle.h"
#include "SentinelLinkedList.h"
#include "SinglyLinkedList.h"

namespace JSC {

class HandleHeap;
class JSGlobalData;
class JSValue;
class HeapRootMarker;

class Finalizer {
public:
    virtual void finalize(Handle<Unknown>, void*) = 0;
    virtual ~Finalizer() {}
};

class HandleHeap {
public:
    static HandleHeap* heapFor(HandleSlot);

    HandleHeap(JSGlobalData*);

    HandleSlot allocate();
    void deallocate(HandleSlot);
    
    void makeWeak(HandleSlot, Finalizer*, void* context);
    void makeSelfDestroying(HandleSlot, Finalizer*, void* context);

    void markStrongHandles(HeapRootMarker&);
    void updateAfterMark();
    
    // Should only be called during teardown.
    void clearWeakPointers();

    void writeBarrier(HandleSlot, const JSValue&);

#if !ASSERT_DISABLED
    Finalizer* getFinalizer(HandleSlot handle)
    {
        return toNode(handle)->finalizer();
    }
#endif

    unsigned protectedGlobalObjectCount();
    
private:
    typedef uintptr_t HandleHeapWithFlags;
    enum { FlagsMask = 3, WeakFlag = 1, SelfDestroyingFlag = 2 };
    class Node {
    public:
        Node(WTF::SentinelTag);
        Node(HandleHeap*);
        
        HandleSlot slot();
        HandleHeap* handleHeap();

        void setFinalizer(Finalizer*, void* context);
        Finalizer* finalizer();
        void* finalizerContext();

        void setPrev(Node*);
        Node* prev();

        void setNext(Node*);
        Node* next();

        bool isWeak();
        void makeWeak();
        
        bool isSelfDestroying();
        void makeSelfDestroying();

    private:
        JSValue m_value;
        HandleHeapWithFlags m_handleHeapWithFlags;
        Finalizer* m_finalizer;
        void* m_finalizerContext;
        Node* m_prev;
        Node* m_next;
    };

    static HandleSlot toHandle(Node*);
    static Node* toNode(HandleSlot);

    void grow();

    JSGlobalData* m_globalData;
    BlockStack<Node> m_blockStack;

    SentinelLinkedList<Node> m_strongList;
    SentinelLinkedList<Node> m_weakList;
    SentinelLinkedList<Node> m_immediateList;
    SinglyLinkedList<Node> m_freeList;
    Node* m_nextToFinalize;

#if !ASSERT_DISABLED
    bool m_handlingFinalizers;
#endif
};

inline HandleHeap* HandleHeap::heapFor(HandleSlot handle)
{
    return toNode(handle)->handleHeap();
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
    if (m_freeList.isEmpty())
        grow();

    Node* node = m_freeList.pop();
    new (node) Node(this);
    m_immediateList.push(node);
    return toHandle(node);
}

inline void HandleHeap::deallocate(HandleSlot handle)
{
    Node* node = toNode(handle);
    if (m_nextToFinalize == node) {
        m_nextToFinalize = node->next();
        ASSERT(m_nextToFinalize->next());
    }
    SentinelLinkedList<Node>::remove(node);
    m_freeList.push(node);
}

inline void HandleHeap::makeWeak(HandleSlot handle, Finalizer* finalizer, void* context)
{
    Node* node = toNode(handle);
    SentinelLinkedList<Node>::remove(node);
    node->setFinalizer(finalizer, context);
    node->makeWeak();
    if (handle->isCell() && *handle)
        m_weakList.push(node);
    else
        m_immediateList.push(node);
}

inline void HandleHeap::makeSelfDestroying(HandleSlot handle, Finalizer* finalizer, void* context)
{
    makeWeak(handle, finalizer, context);
    Node* node = toNode(handle);
    node->makeSelfDestroying();
}

inline HandleHeap::Node::Node(HandleHeap* handleHeap)
    : m_handleHeapWithFlags(reinterpret_cast<uintptr_t>(handleHeap))
    , m_finalizer(0)
    , m_finalizerContext(0)
{
}

inline HandleHeap::Node::Node(WTF::SentinelTag)
    : m_handleHeapWithFlags(0)
    , m_finalizer(0)
    , m_finalizerContext(0)
{
}

inline HandleSlot HandleHeap::Node::slot()
{
    return &m_value;
}

inline HandleHeap* HandleHeap::Node::handleHeap()
{
    return reinterpret_cast<HandleHeap*>(m_handleHeapWithFlags & ~FlagsMask);
}

inline void HandleHeap::Node::setFinalizer(Finalizer* finalizer, void* context)
{
    m_finalizer = finalizer;
    m_finalizerContext = context;
}

inline void HandleHeap::Node::makeWeak()
{
    ASSERT(!(m_handleHeapWithFlags & WeakFlag));
    m_handleHeapWithFlags |= WeakFlag;
}

inline bool HandleHeap::Node::isWeak()
{
    return !!(m_handleHeapWithFlags & WeakFlag);
}

inline void HandleHeap::Node::makeSelfDestroying()
{
    ASSERT(m_handleHeapWithFlags & WeakFlag);
    ASSERT(!(m_handleHeapWithFlags & SelfDestroyingFlag));
    m_handleHeapWithFlags |= SelfDestroyingFlag;
}

inline bool HandleHeap::Node::isSelfDestroying()
{
    return !!(m_handleHeapWithFlags & SelfDestroyingFlag);
}

inline Finalizer* HandleHeap::Node::finalizer()
{
    return m_finalizer;
}

inline void* HandleHeap::Node::finalizerContext()
{
    ASSERT(m_finalizer);
    return m_finalizerContext;
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

}

#endif
