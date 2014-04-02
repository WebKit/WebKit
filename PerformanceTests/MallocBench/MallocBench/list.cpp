/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "CPUCount.h"
#include "list.h"
#include <stdlib.h>
#include <strings.h>

#include "mbmalloc.h"

namespace {

struct Node {
    void* operator new(size_t size)
    {
        return mbmalloc(size);
    }

    void operator delete(void* p, size_t size)
    {
        mbfree(p, size);
    }

    Node(Node* next, size_t payloadSize)
        : m_refCount(1)
        , m_next(next)
        , m_payload(static_cast<char*>(mbmalloc(payloadSize)))
        , m_payloadSize(payloadSize)
    {
        if (m_next)
            m_next->ref();
        bzero(m_payload, payloadSize);
    }

    ~Node()
    {
        if (m_next)
            m_next->deref();
        mbfree(m_payload, m_payloadSize);
    }

    void ref()
    {
        ++m_refCount;
    }

    void deref()
    {
        if (m_refCount == 1)
            delete this;
        else
            --m_refCount;
    }

    Node* takeNext()
    {
        Node* tmp = m_next;
        m_next = 0;
        return tmp;
    }
    
    bool validate()
    {
        if (m_payload[0])
            return false;
        return true;
    }

    unsigned m_refCount;
    Node* m_next;
    char* m_payload;
    size_t m_payloadSize;
};

} // namespace

void benchmark_list_allocate(bool isParallel)
{
    Node* head = 0;
    size_t times = 96;
    size_t nodes = 32 * 1024;
    if (isParallel) {
        nodes /= cpuCount();
        times *= 2;
    }
    
    for (size_t time = 0; time < times; ++time) {
        // Construct a list of nodes.
        for (size_t node = 0; node < nodes; ++node) {
            Node* oldHead = head;
            head = new Node(oldHead, (nodes & (64 - 1)) | 1);
            if (oldHead)
                oldHead->deref();
        }

        // Tear down the list.
        while (head) {
            Node* tmp = head->takeNext();
            head->deref();
            head = tmp;
        }
    }
}

void benchmark_list_traverse(bool isParallel)
{
    Node* head = 0;
    size_t times = 1 * 1024;
    size_t nodes = 32 * 1024;
    if (isParallel) {
        nodes /= cpuCount();
        times *= 4;
    }

    // Construct a list of nodes.
    for (size_t node = 0; node < nodes; ++node) {
        Node* oldHead = head;
        head = new Node(oldHead, (nodes & (64 - 1)) | 1);
        if (oldHead)
            oldHead->deref();
    }

    // Validate the list.
    for (size_t time = 0; time < times; ++time) {
        for (Node* node = head; node; node = node->m_next) {
            if (!node->validate())
                abort();
        }
    }

    // Tear down the list.
    while (head) {
        Node* tmp = head->takeNext();
        head->deref();
        head = tmp;
    }
}
