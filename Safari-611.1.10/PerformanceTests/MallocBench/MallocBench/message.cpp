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
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <stdlib.h>
#include <strings.h>
#include <thread>
#include <vector>

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

class WorkQueue {
public:
    WorkQueue()
    {
        m_thread = std::thread([&] {
            while (true) {
                std::function<void()> target;
                {
                    std::unique_lock<std::mutex> locker(m_mutex);
                    m_condition.wait(locker, [&] { return !m_queue.empty(); });
                    auto queued = m_queue.front();
                    m_queue.pop_front();
                    if (!queued)
                        return;
                    target = std::move(queued);
                }
                target();
            }
        });
    }

    ~WorkQueue() {
        {
            std::unique_lock<std::mutex> locker(m_mutex);
            m_queue.push_back(nullptr);
            m_condition.notify_one();
        }
        m_thread.join();
    }

    void dispatchAsync(std::function<void()> target)
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        m_queue.push_back(target);
        m_condition.notify_one();
    }

    void dispatchSync(std::function<void()> target)
    {
        std::mutex syncMutex;
        std::condition_variable syncCondition;

        std::unique_lock<std::mutex> locker(syncMutex);
        bool done = false;
        dispatchAsync([&] {
            target();
            {
                std::unique_lock<std::mutex> locker(syncMutex);
                done = true;
                syncCondition.notify_one();
            }
        });
        syncCondition.wait(locker, [&] { return done; });
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::deque<std::function<void()>> m_queue;
    std::thread m_thread;
};

void benchmark_message_one(CommandLine& commandLine)
{
    if (commandLine.isParallel())
        abort();

    const size_t times = 2048;
    const size_t quantum = 16;

    WorkQueue workQueue;
    for (size_t i = 0; i < times; i += quantum) {
        for (size_t j = 0; j < quantum; ++j) {
            Message* message = new Message;
            workQueue.dispatchAsync([message] {
                size_t hash = message->hash();
                if (hash)
                    abort();
                delete message;
            });
        }
        workQueue.dispatchSync([] { });
    }
    workQueue.dispatchSync([] { });
}

void benchmark_message_many(CommandLine& commandLine)
{
    if (commandLine.isParallel())
        abort();

    const size_t times = 768;
    const size_t quantum = 16;

    const size_t queueCount = cpuCount() - 1;
    auto queues = std::make_unique<WorkQueue[]>(queueCount);
    for (size_t i = 0; i < times; i += quantum) {
        for (size_t j = 0; j < quantum; ++j) {
            for (size_t k = 0; k < queueCount; ++k) {
                Message* message = new Message;
                queues[k].dispatchAsync([message] {
                    size_t hash = message->hash();
                    if (hash)
                        abort();
                    delete message;
                });
            }
        }

        for (size_t i = 0; i < queueCount; ++i)
            queues[i].dispatchSync([] { });
    }

    for (size_t i = 0; i < queueCount; ++i)
        queues[i].dispatchSync([] { });
}
