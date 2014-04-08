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

#ifndef Vector_h
#define Vector_h

#include "Inline.h"
#include "VMAllocate.h"
#include <cstddef>
#include <string>

namespace bmalloc {

// A replacement for std::vector that allocates using vmAllocate instead of
// malloc, shrinks automatically, and supports "popping" from the middle.

template<typename T>
class Vector {
    static_assert(std::is_trivially_destructible<T>::value, "Vector must have a trivial destructor.");
public:
    Vector(const Vector&) = delete;
    Vector& operator=(const Vector&) = delete;

    Vector();
    ~Vector();

    T* begin() { return m_buffer; }
    T* end() { return m_buffer + m_size; }

    size_t size() { return m_size; }
    size_t capacity() { return m_capacity; }
    
    T& operator[](size_t);
    T& last() { return m_buffer[m_size - 1]; }

    void push(const T&);
    void push(const T*, const T*);
    T pop();
    T pop(size_t);
    T pop(const T* it) { return pop(it - begin()); }

    void shrink(size_t);

private:
    static const size_t growFactor = 2;
    static const size_t shrinkFactor = 4;
    static const size_t initialCapacity = vmPageSize;

    void growCapacity(size_t size);
    void shrinkCapacity();
    void reallocateBuffer(size_t);

    T* m_buffer;
    size_t m_size;
    size_t m_capacity;
};

template<typename T>
inline Vector<T>::Vector()
    : m_buffer(0)
    , m_size(0)
    , m_capacity(0)
{
}

template<typename T>
Vector<T>::~Vector()
{
    vmDeallocate(m_buffer, vmSize(m_capacity * sizeof(T)));
}

template<typename T>
inline T& Vector<T>::operator[](size_t i)
{
    BASSERT(i < m_size);
    return m_buffer[i];
}

template<typename T>
INLINE void Vector<T>::push(const T& value)
{
    if (m_size == m_capacity)
        growCapacity(m_size);
    m_buffer[m_size++] = value;
}

template<typename T>
void Vector<T>::push(const T* begin, const T* end)
{
    size_t newSize = m_size + (end - begin);
    if (newSize > m_capacity)
        growCapacity(newSize);
    memcpy(this->end(), begin, (end - begin) * sizeof(T));
    m_size = newSize;
}

template<typename T>
inline T Vector<T>::pop()
{
    BASSERT(m_size);
    T value = m_buffer[m_size - 1];
    shrink(m_size - 1);
    return value;
}

template<typename T>
inline T Vector<T>::pop(size_t i)
{
    BASSERT(i < m_size);
    std::swap(m_buffer[i], last());
    return pop();
}

template<typename T>
inline void Vector<T>::shrink(size_t size)
{
    BASSERT(size <= m_size);
    m_size = size;
    if (m_capacity > initialCapacity && m_size < m_capacity / shrinkFactor)
        shrinkCapacity();
}

template<typename T>
void Vector<T>::reallocateBuffer(size_t newCapacity)
{
    size_t vmSize = bmalloc::vmSize(newCapacity * sizeof(T));
    T* newBuffer = static_cast<T*>(vmAllocate(vmSize));
    if (m_buffer) {
        memcpy(newBuffer, m_buffer, m_size * sizeof(T));
        vmDeallocate(m_buffer, bmalloc::vmSize(m_capacity * sizeof(T)));
    }

    m_buffer = newBuffer;
    m_capacity = vmSize / sizeof(T);
}

template<typename T>
NO_INLINE void Vector<T>::shrinkCapacity()
{
    size_t newCapacity = max(initialCapacity, m_capacity / shrinkFactor);
    reallocateBuffer(newCapacity);
}

template<typename T>
NO_INLINE void Vector<T>::growCapacity(size_t size)
{
    size_t newCapacity = max(initialCapacity, size * growFactor);
    reallocateBuffer(newCapacity);
}

} // namespace bmalloc

#endif // Vector_h
