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
#include "message.h"
#include <dispatch/dispatch.h>
#include <stdlib.h>
#include <strings.h>

#include "mbmalloc.h"

namespace {

size_t hash(size_t hash, unsigned short a, unsigned short b)
{
    hash += a ^ b;
    return hash;
}

class Node {
    static const size_t payloadCount = 128;
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
        : m_payload()
    {
    }

    size_t hash(size_t hash)
    {
        for (size_t i = 0; i < payloadCount; i += 2)
            hash = ::hash(hash, m_payload[i], m_payload[i + 1]);
        return hash;
    }

private:
    unsigned short m_payload[payloadCount];
};

class Message {
    static const size_t nodeCount = 1 * 1024;

public:
    void* operator new(size_t size)
    {
        return mbmalloc(size);
    }

    void operator delete(void* p, size_t size)
    {
        mbfree(p, size);
    }

    Message()
        : m_buffer(static_cast<Node**>(mbmalloc(nodeCount * sizeof(Node**))))
    {
        for (size_t i = 0; i < nodeCount; ++i)
            m_buffer[i] = new Node;
    }
    
    ~Message()
    {
        for (size_t i = 0; i < nodeCount; ++i)
            delete m_buffer[i];
        mbfree(m_buffer, nodeCount * sizeof(Node**));
    }

    size_t hash()
    {
        size_t hash = 0;
        for (size_t i = 0; i < nodeCount; ++i)
            hash = m_buffer[i]->hash(hash);
        return hash;
    }

private:
    Node** m_buffer;
};

} // namespace

void benchmark_message_one(bool isParallel)
{
    if (isParallel)
        abort();

    const size_t times = 2048;
    const size_t quantum = 16;

    dispatch_queue_t queue = dispatch_queue_create("message", 0);

    for (size_t i = 0; i < times; i += quantum) {
        for (size_t j = 0; j < quantum; ++j) {
            Message* message = new Message;
            dispatch_async(queue, ^{
                size_t hash = message->hash();
                if (hash)
                    abort();
                delete message;
            });
        }
        dispatch_sync(queue, ^{ });
    }

    dispatch_sync(queue, ^{ });

    dispatch_release(queue);
}

void benchmark_message_many(bool isParallel)
{
    if (isParallel)
        abort();

    const size_t times = 768;
    const size_t quantum = 16;

    const size_t queueCount = cpuCount() - 1;
    dispatch_queue_t queues[queueCount];
    for (size_t i = 0; i < queueCount; ++i)
        queues[i] = dispatch_queue_create("message", 0);

    for (size_t i = 0; i < times; i += quantum) {
        for (size_t j = 0; j < quantum; ++j) {
            for (size_t k = 0; k < queueCount; ++k) {
                Message* message = new Message;
                dispatch_async(queues[k], ^{
                    size_t hash = message->hash();
                    if (hash)
                        abort();
                    delete message;
                });
            }
        }

        for (size_t i = 0; i < queueCount; ++i)
            dispatch_sync(queues[i], ^{ });
    }

    for (size_t i = 0; i < queueCount; ++i)
        dispatch_sync(queues[i], ^{ });

    for (size_t i = 0; i < queueCount; ++i)
        dispatch_release(queues[i]);
}
