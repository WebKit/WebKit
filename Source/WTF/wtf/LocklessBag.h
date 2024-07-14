/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Atomics.h>
#include <wtf/Noncopyable.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

// This a simple single consumer, multiple producer Bag data structure.

template<typename T>
class LocklessBag final {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(LocklessBag);
public:
    struct Node {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        T data;
        Node* next;
    };

    LocklessBag()
        : m_head(nullptr)
    {
    }

    enum PushResult { Empty, NonEmpty };
    PushResult add(T&& element)
    {
        Node* newNode = new Node();
        newNode->data = std::forward<T>(element);

        Node* oldHead;
        m_head.transaction([&] (Node*& head) {
            oldHead = head;
            newNode->next = head;
            head = newNode;
            return true;
        });

        return oldHead == nullptr ? Empty : NonEmpty;
    }

    // CONSUMER FUNCTIONS: Everything below here is only safe to call from the consumer thread.

    // This function is actually safe to call from more than one thread, but ONLY if no thread can call consumeAll.
    void iterate(const Invocable<void(const T&)> auto& func)
    {
        Node* node = m_head.load();
        while (node) {
            func(node->data);
            node = node->next;
        }
    }

    void consumeAll(const Invocable<void(T&&)> auto& func)
    {
        consumeAllWithNode([&] (T&& data, Node* node) {
            func(WTFMove(data));
            delete node;
        });
    }

    void consumeAllWithNode(const Invocable<void(T&&, Node*)> auto& func)
    {
        Node* node = m_head.exchange(nullptr);
        while (node) {
            Node* oldNode = node;
            node = node->next;
            func(WTFMove(oldNode->data), oldNode);
        }
    }

    ~LocklessBag()
    {
        consumeAll([] (T&&) { });
    }

private:
    Atomic<Node*> m_head;
};
    
} // namespace WTF
