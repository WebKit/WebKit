/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MessageQueue_h
#define MessageQueue_h

#include "Threading.h"
#include <wtf/Assertions.h>
#include <wtf/Deque.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

    template<typename DataType>
    class MessageQueue : Noncopyable {
    public:
        MessageQueue() : m_killed(false) {}
        
        void append(const DataType&);
        void prepend(const DataType&);
        bool waitForMessage(DataType&);
        bool tryGetMessage(DataType&);
        void kill();
        bool killed() const { return m_killed; }

    private:
        Mutex m_mutex;
        ThreadCondition m_condition;
        Deque<DataType> m_queue;
        bool m_killed;
    };

    template<typename DataType>
    inline void MessageQueue<DataType>::append(const DataType& message)
    {
        MutexLocker lock(m_mutex);
        m_queue.append(message);
        m_condition.signal();
    }

    template<typename DataType>
    inline void MessageQueue<DataType>::prepend(const DataType& message)
    {
        MutexLocker lock(m_mutex);
        m_queue.prepend(message);
        m_condition.signal();
    }

    template<typename DataType>
    inline bool MessageQueue<DataType>::waitForMessage(DataType& result)
    {
        MutexLocker lock(m_mutex);
        if (m_killed)
            return false;
        
        if (m_queue.isEmpty())
            m_condition.wait(m_mutex);
        if (m_killed)
            return false;

        ASSERT(!m_queue.isEmpty());
        result = m_queue.first();
        m_queue.removeFirst();
        return true;
    }

    template<typename DataType>
    inline bool MessageQueue<DataType>::tryGetMessage(DataType& result)
    {
        MutexLocker lock(m_mutex);
        if (m_killed)
            return false;
        if (m_queue.isEmpty())
            return false;

        result = m_queue.first();
        m_queue.removeFirst();
        return true;
    }

    template<typename DataType>
    inline void MessageQueue<DataType>::kill()
    {
        MutexLocker lock(m_mutex);
        m_killed = true;
        m_condition.broadcast();
    }
}

#endif // MessageQueue_h
