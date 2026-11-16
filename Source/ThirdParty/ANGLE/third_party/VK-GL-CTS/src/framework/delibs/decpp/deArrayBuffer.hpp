#ifndef _DEARRAYBUFFER_HPP
#define _DEARRAYBUFFER_HPP
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
 * \brief Array buffer
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deMemory.h"

#include <new>

namespace de
{
namespace detail
{

void *ArrayBuffer_AlignedMalloc(size_t numBytes, size_t alignment);
void ArrayBuffer_AlignedFree(void *);

} // namespace detail

//! Array buffer self-test.
void ArrayBuffer_selfTest(void);

/*--------------------------------------------------------------------*//*!
 * \brief Contiguous array that does not initialize its elements.
 *//*--------------------------------------------------------------------*/
template <typename T, size_t Alignment = (sizeof(T) > 4 ? 4 : sizeof(T)), size_t Stride = sizeof(T)>
class ArrayBuffer
{
public:
    DE_STATIC_ASSERT(Stride >= sizeof(T));

    ArrayBuffer(void) throw();
    ArrayBuffer(size_t numElements);
    ArrayBuffer(const T *ptr, size_t numElements);
    ArrayBuffer(const ArrayBuffer &other);
    ~ArrayBuffer(void) throw();
    ArrayBuffer &operator=(const ArrayBuffer &other);

    void clear(void) throw();
    void setStorage(size_t numElements); // !< \note after a succesful call buffer contents are undefined
    void swap(ArrayBuffer &other) throw();
    size_t size(void) const throw();
    bool empty(void) const throw();

    T *getElementPtr(size_t elementNdx) throw();
    const T *getElementPtr(size_t elementNdx) const throw();
    void *getPtr(void) throw();
    const void *getPtr(void) const throw();

private:
    void *m_ptr;
    size_t m_cap;
} DE_WARN_UNUSED_TYPE;

template <typename T, size_t Alignment, size_t Stride>
ArrayBuffer<T, Alignment, Stride>::ArrayBuffer(void) throw() : m_ptr(nullptr)
                                                             , m_cap(0)
{
}

template <typename T, size_t Alignment, size_t Stride>
ArrayBuffer<T, Alignment, Stride>::ArrayBuffer(size_t numElements) : m_ptr(nullptr)
                                                                   , m_cap(0)
{
    if (numElements)
    {
        // \note no need to allocate stride for the last element, sizeof(T) is enough. Also handles cases where sizeof(T) > Stride
        const size_t storageSize = (numElements - 1) * Stride + sizeof(T);
        void *const ptr          = detail::ArrayBuffer_AlignedMalloc(storageSize, Alignment);

        if (!ptr)
            throw std::bad_alloc();

        m_ptr = ptr;
        m_cap = numElements;
    }
}

template <typename T, size_t Alignment, size_t Stride>
ArrayBuffer<T, Alignment, Stride>::ArrayBuffer(const T *ptr, size_t numElements) : m_ptr(nullptr)
                                                                                 , m_cap(0)
{
    if (numElements)
    {
        // create new buffer of wanted size, copy to it, and swap to it
        ArrayBuffer<T, Alignment, Stride> tmp(numElements);

        if (Stride == sizeof(T))
        {
            // tightly packed
            const size_t storageSize = sizeof(T) * numElements;
            deMemcpy(tmp.m_ptr, ptr, (int)storageSize);
        }
        else
        {
            // sparsely packed
            for (size_t ndx = 0; ndx < numElements; ++ndx)
                *tmp.getElementPtr(ndx) = ptr[ndx];
        }

        swap(tmp);
    }
}

template <typename T, size_t Alignment, size_t Stride>
ArrayBuffer<T, Alignment, Stride>::ArrayBuffer(const ArrayBuffer<T, Alignment, Stride> &other)
    : m_ptr(nullptr)
    , m_cap(0)
{
    if (other.m_cap)
    {
        // copy to temporary and swap to it

        const size_t storageSize = (other.m_cap - 1) * Stride + sizeof(T);
        ArrayBuffer tmp(other.m_cap);

        deMemcpy(tmp.m_ptr, other.m_ptr, (int)storageSize);
        swap(tmp);
    }
}

template <typename T, size_t Alignment, size_t Stride>
ArrayBuffer<T, Alignment, Stride>::~ArrayBuffer(void) throw()
{
    clear();
}

template <typename T, size_t Alignment, size_t Stride>
ArrayBuffer<T, Alignment, Stride> &ArrayBuffer<T, Alignment, Stride>::operator=(const ArrayBuffer &other)
{
    ArrayBuffer copied(other);
    swap(copied);
    return *this;
}

template <typename T, size_t Alignment, size_t Stride>
void ArrayBuffer<T, Alignment, Stride>::clear(void) throw()
{
    detail::ArrayBuffer_AlignedFree(m_ptr);

    m_ptr = nullptr;
    m_cap = 0;
}

template <typename T, size_t Alignment, size_t Stride>
void ArrayBuffer<T, Alignment, Stride>::setStorage(size_t numElements)
{
    // create new buffer of the wanted size, swap to it
    ArrayBuffer<T, Alignment, Stride> newBuffer(numElements);
    swap(newBuffer);
}

template <typename T, size_t Alignment, size_t Stride>
void ArrayBuffer<T, Alignment, Stride>::swap(ArrayBuffer &other) throw()
{
    void *const otherPtr  = other.m_ptr;
    const size_t otherCap = other.m_cap;

    other.m_ptr = m_ptr;
    other.m_cap = m_cap;
    m_ptr       = otherPtr;
    m_cap       = otherCap;
}

template <typename T, size_t Alignment, size_t Stride>
size_t ArrayBuffer<T, Alignment, Stride>::size(void) const throw()
{
    return m_cap;
}

template <typename T, size_t Alignment, size_t Stride>
bool ArrayBuffer<T, Alignment, Stride>::empty(void) const throw()
{
    return size() == 0;
}

template <typename T, size_t Alignment, size_t Stride>
T *ArrayBuffer<T, Alignment, Stride>::getElementPtr(size_t elementNdx) throw()
{
    return (T *)(((uint8_t *)m_ptr) + Stride * elementNdx);
}

template <typename T, size_t Alignment, size_t Stride>
const T *ArrayBuffer<T, Alignment, Stride>::getElementPtr(size_t elementNdx) const throw()
{
    return (T *)(((uint8_t *)m_ptr) + Stride * elementNdx);
}

template <typename T, size_t Alignment, size_t Stride>
void *ArrayBuffer<T, Alignment, Stride>::getPtr(void) throw()
{
    return m_ptr;
}

template <typename T, size_t Alignment, size_t Stride>
const void *ArrayBuffer<T, Alignment, Stride>::getPtr(void) const throw()
{
    return m_ptr;
}

} // namespace de

#endif // _DEARRAYBUFFER_HPP
