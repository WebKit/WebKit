#ifndef _DERINGBUFFER_HPP
#define _DERINGBUFFER_HPP
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
 * \brief Ring buffer template.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"

namespace de
{

void RingBuffer_selfTest(void);

/** Ring buffer template. */
template <typename T>
class RingBuffer
{
public:
    RingBuffer(int size);
    ~RingBuffer(void);

    void clear(void);
    void resize(int newSize);

    int getSize(void) const
    {
        return m_size;
    }
    int getNumElements(void) const
    {
        return m_numElements;
    }
    int getNumFree(void) const
    {
        return m_size - m_numElements;
    }

    void pushFront(const T &elem);
    void pushFront(const T *elemBuf, int count);

    void peekBack(T *elemBuf, int count) const;
    T peekBack(int offset) const;

    T popBack(void);
    void popBack(T *elemBuf, int count)
    {
        peekBack(elemBuf, count);
        popBack(count);
    }
    void popBack(int count);

protected:
    int m_numElements;
    int m_front;
    int m_back;

    T *m_buffer;
    int m_size;
};

// RingBuffer implementation.

template <typename T>
RingBuffer<T>::RingBuffer(int size) : m_numElements(0)
                                    , m_front(0)
                                    , m_back(0)
                                    , m_size(size)
{
    DE_ASSERT(size > 0);
    m_buffer = new T[m_size];
}

template <typename T>
RingBuffer<T>::~RingBuffer()
{
    delete[] m_buffer;
}

template <typename T>
void RingBuffer<T>::clear(void)
{
    m_numElements = 0;
    m_front       = 0;
    m_back        = 0;
}

template <typename T>
void RingBuffer<T>::resize(int newSize)
{
    DE_ASSERT(newSize >= m_numElements);
    T *buf = new T[newSize];

    try
    {
        // Copy old elements.
        for (int ndx = 0; ndx < m_numElements; ndx++)
            buf[ndx] = m_buffer[(m_back + ndx) % m_size];

        // Reset pointers.
        m_front = m_numElements;
        m_back  = 0;
        m_size  = newSize;

        std::swap(buf, m_buffer);
        delete[] buf;
    }
    catch (...)
    {
        delete[] buf;
        throw;
    }
}

template <typename T>
inline void RingBuffer<T>::pushFront(const T &elem)
{
    DE_ASSERT(getNumFree() > 0);
    m_buffer[m_front] = elem;
    m_front           = (m_front + 1) % m_size;
    m_numElements += 1;
}

template <typename T>
void RingBuffer<T>::pushFront(const T *elemBuf, int count)
{
    DE_ASSERT(de::inRange(count, 0, getNumFree()));
    for (int i = 0; i < count; i++)
        m_buffer[(m_front + i) % m_size] = elemBuf[i];
    m_front = (m_front + count) % m_size;
    m_numElements += count;
}

template <typename T>
inline T RingBuffer<T>::popBack()
{
    DE_ASSERT(getNumElements() > 0);
    int ndx = m_back;
    m_back  = (m_back + 1) % m_size;
    m_numElements -= 1;
    return m_buffer[ndx];
}

template <typename T>
inline T RingBuffer<T>::peekBack(int offset) const
{
    DE_ASSERT(de::inBounds(offset, 0, getNumElements()));
    return m_buffer[(m_back + offset) % m_size];
}

template <typename T>
void RingBuffer<T>::peekBack(T *elemBuf, int count) const
{
    DE_ASSERT(de::inRange(count, 0, getNumElements()));
    for (int i = 0; i < count; i++)
        elemBuf[i] = m_buffer[(m_back + i) % m_size];
}

template <typename T>
void RingBuffer<T>::popBack(int count)
{
    DE_ASSERT(de::inRange(count, 0, getNumElements()));
    m_back = (m_back + count) % m_size;
    m_numElements -= count;
}

} // namespace de

#endif // _DERINGBUFFER_HPP
