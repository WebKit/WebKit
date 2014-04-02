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
#include "fragment.h"
#include <stdlib.h>
#include <strings.h>

#include "mbmalloc.h"

namespace {

class Node {
public:
    void* operator new(size_t size)
    {
        return mbmalloc(size);
    }

    void operator delete(void* p, size_t size)
    {
        mbfree(p, size);
    }

    Node()
        : m_next(0)
        , m_payload()
    {
    }

    Node(Node* next)
        : m_next(next)
        , m_payload()
    {
    }
    
    Node* next() { return m_next; }
    
    void validate()
    {
        for (size_t i = 0; i < sizeof(m_payload); ++i) {
            if (m_payload[i])
                abort();
        }
    }

private:
    Node* m_next;
    char m_payload[32 - sizeof(Node*)];
};

} // namespace

void validate(Node* head)
{
    for (Node* node = head; node; node = node->next())
        node->validate();
}

void benchmark_fragment(bool isParallel)
{
    size_t nodeCount = 128 * 1024;
    if (isParallel)
        nodeCount /= cpuCount();
    size_t replaceCount = nodeCount / 4;
    size_t times = 25;

    srandom(0); // For consistency between runs.

    for (size_t i = 0; i < times; ++i) {
        Node** nodes = static_cast<Node**>(mbmalloc(nodeCount * sizeof(Node*)));
        for (size_t i = 0; i < nodeCount; ++i)
            nodes[i] = new Node;

        for (size_t i = 0; i < replaceCount; ++i) {
            size_t node = random() % nodeCount;

            delete nodes[node];
            nodes[node] = new Node;
        }

        for (size_t node = 0; node < nodeCount; ++node)
            delete nodes[node];
        mbfree(nodes, nodeCount * sizeof(Node*));
    }
}

void benchmark_fragment_iterate(bool isParallel)
{
    size_t nodeCount = 512 * 1024;
    size_t times = 32;
    if (isParallel)
        nodeCount /= cpuCount();
    size_t replaceCount = nodeCount / 4;

    srandom(0); // For consistency between runs.

    Node** nodes = static_cast<Node**>(mbmalloc(nodeCount * sizeof(Node*)));
    for (size_t i = 0; i < nodeCount; ++i)
        nodes[i] = new Node;

    Node* head = 0;
    for (size_t i = 0; i < replaceCount; ++i) {
        size_t node = random() % nodeCount;

        delete nodes[node];
        nodes[node] = 0;
        head = new Node(head);
    }
    
    for (size_t i = 0; i < times; ++i)
        validate(head);

    for (Node* next ; head; head = next) {
        next = head->next();
        delete head;
    }

    for (size_t node = 0; node < nodeCount; ++node) {
        if (!nodes[node])
            continue;
        delete nodes[node];
    }
    mbfree(nodes, nodeCount * sizeof(Node*));
}
