/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include <limits>
#include <wtf/Assertions.h>
#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>

namespace WTF {

template<typename DataType>
class CrossThreadQueue final {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(CrossThreadQueue);
public:
    CrossThreadQueue() = default;

    void append(DataType&&);

    DataType waitForMessage();
    std::optional<DataType> tryGetMessage();

    void kill();
    bool isKilled() const;
    bool isEmpty() const;

private:
    mutable Lock m_lock;
    Deque<DataType> m_queue WTF_GUARDED_BY_LOCK(m_lock);
    Condition m_condition;
    bool m_killed WTF_GUARDED_BY_LOCK(m_lock) { false };
};

template<typename DataType>
void CrossThreadQueue<DataType>::append(DataType&& message)
{
    Locker locker { m_lock };
    ASSERT(!m_killed);
    m_queue.append(WTFMove(message));
    m_condition.notifyOne();
}

template<typename DataType>
DataType CrossThreadQueue<DataType>::waitForMessage()
{
    Locker locker { m_lock };

    auto found = m_queue.end();
    while (!m_killed && found == m_queue.end()) {
        found = m_queue.begin();
        if (found != m_queue.end())
            break;

        m_condition.wait(m_lock);
    }
    if (m_killed)
        return { };

    return m_queue.takeFirst();
}

template<typename DataType>
std::optional<DataType> CrossThreadQueue<DataType>::tryGetMessage()
{
    Locker locker { m_lock };

    if (m_queue.isEmpty())
        return { };

    return m_queue.takeFirst();
}

template<typename DataType>
void CrossThreadQueue<DataType>::kill()
{
    Locker locker { m_lock };
    m_killed = true;
    m_condition.notifyAll();
}

template<typename DataType>
bool CrossThreadQueue<DataType>::isKilled() const
{
    Locker locker { m_lock };
    return m_killed;
}

template<typename DataType>
bool CrossThreadQueue<DataType>::isEmpty() const
{
    Locker locker { m_lock };
    return m_queue.isEmpty();
}

} // namespace WTF

using WTF::CrossThreadQueue;
