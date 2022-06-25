/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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

#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WTF {

template<typename T, size_t BufferSize>
class SynchronizedFixedQueue final : public ThreadSafeRefCounted<SynchronizedFixedQueue<T, BufferSize>> {
public:
    static Ref<SynchronizedFixedQueue> create()
    {
        return adoptRef(*new SynchronizedFixedQueue());
    }

    void open()
    {
        Locker locker { m_lock };
        if (m_open)
            return;

        // Restore the queue to its original state.
        m_open = true;
        m_queue.clear();
    }

    void close()
    {
        Locker locker { m_lock };
        if (!m_open)
            return;

        // Wake all the sleeping threads up with a closing state.
        m_open = false;
        m_condition.notifyAll();
    }

    bool isOpen()
    {
        Locker locker { m_lock };
        return m_open;
    }

    bool enqueue(const T& value)
    {
        Locker locker { m_lock };

        // Wait for an empty place to be available in the queue.
        m_condition.wait(m_lock, [this] {
            assertIsHeld(m_lock);
            return !m_open || m_queue.size() < BufferSize;
        });

        // The queue is closing, exit immediately.
        if (!m_open)
            return false;

        // Add the item in the queue.
        m_queue.append(value);

        // Notify the other threads that an item was added to the queue.
        m_condition.notifyAll();
        return true;
    }

    bool dequeue(T& value)
    {
        Locker locker { m_lock };

        // Wait for an item to be added.
        m_condition.wait(m_lock, [this] {
            assertIsHeld(m_lock);
            return !m_open || m_queue.size();
        });

        // The queue is closing, exit immediately.
        if (!m_open)
            return false;

        // Get a copy from m_queue.first and then remove it.
        value = m_queue.first();
        m_queue.removeFirst();

        // Notify the other threads that an item was removed from the queue.
        m_condition.notifyAll();
        return true;
    }

private:
    SynchronizedFixedQueue()
    {
        static_assert(!((BufferSize - 1) & BufferSize), "BufferSize must be power of 2.");
    }

    Lock m_lock;
    Condition m_condition;

    bool m_open WTF_GUARDED_BY_LOCK(m_lock) { true };
    Deque<T, BufferSize> m_queue WTF_GUARDED_BY_LOCK(m_lock);
};

}

using WTF::SynchronizedFixedQueue;
