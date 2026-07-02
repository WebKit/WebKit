#ifndef _DETHREADSAFERINGBUFFER_HPP
#define _DETHREADSAFERINGBUFFER_HPP
/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Thread-safe ring buffer template.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deMutex.hpp"
#include "deSemaphore.hpp"

#include <vector>

namespace de
{

void ThreadSafeRingBuffer_selfTest(void);

/** Thread-safe ring buffer template. */
template <typename T>
class ThreadSafeRingBuffer
{
public:
    ThreadSafeRingBuffer(size_t size);
    ~ThreadSafeRingBuffer(void)
    {
    }

    void pushFront(const T &elem);
    bool tryPushFront(const T &elem);
    T popBack(void);
    bool tryPopBack(T &dst);

protected:
    void pushFrontInternal(const T &elem);
    T popBackInternal(void);

    const size_t m_size;
    std::vector<T> m_elements;

    size_t m_front;
    size_t m_back;

    Mutex m_writeMutex;
    Mutex m_readMutex;

    Semaphore m_fill;
    Semaphore m_empty;
};

// ThreadSafeRingBuffer implementation.

template <typename T>
ThreadSafeRingBuffer<T>::ThreadSafeRingBuffer(size_t size)
    : m_size(size + 1)
    , m_elements(m_size)
    , m_front(0)
    , m_back(0)
    , m_fill(0)
    , m_empty((int)size)
{
    // Semaphores currently only support INT_MAX
    DE_ASSERT(size > 0 && size < 0x7fffffff);
}

template <typename T>
inline void ThreadSafeRingBuffer<T>::pushFrontInternal(const T &elem)
{
    m_elements[m_front] = elem;
    m_front             = (m_front + 1) % m_size;
}

template <typename T>
inline T ThreadSafeRingBuffer<T>::popBackInternal()
{
    const size_t ndx = m_back;
    m_back           = (m_back + 1) % m_size;
    return m_elements[ndx];
}

template <typename T>
void ThreadSafeRingBuffer<T>::pushFront(const T &elem)
{
    m_writeMutex.lock();
    m_empty.decrement();
    pushFrontInternal(elem);
    m_fill.increment();
    m_writeMutex.unlock();
}

template <typename T>
bool ThreadSafeRingBuffer<T>::tryPushFront(const T &elem)
{
    if (!m_writeMutex.tryLock())
        return false;

    const bool success = m_empty.tryDecrement();

    if (success)
    {
        pushFrontInternal(elem);
        m_fill.increment();
    }

    m_writeMutex.unlock();
    return success;
}

template <typename T>
T ThreadSafeRingBuffer<T>::popBack()
{
    m_readMutex.lock();
    m_fill.decrement();
    T elem = popBackInternal();
    m_empty.increment();
    m_readMutex.unlock();
    return elem;
}

template <typename T>
bool ThreadSafeRingBuffer<T>::tryPopBack(T &dst)
{
    if (!m_readMutex.tryLock())
        return false;

    bool success = m_fill.tryDecrement();

    if (success)
    {
        dst = popBackInternal();
        m_empty.increment();
    }

    m_readMutex.unlock();

    return success;
}

} // namespace de

#endif // _DETHREADSAFERINGBUFFER_HPP
