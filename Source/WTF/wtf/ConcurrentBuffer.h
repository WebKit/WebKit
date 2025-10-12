/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <wtf/Atomics.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashFunctions.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace WTF {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ConcurrentBuffer);

// ConcurrentBuffer is suitable for when you plan to store immutable data and sometimes append to it.
// It supports storing data that is not copy-constructable but bit-copyable.
template<typename T>
class ConcurrentBuffer final {
    WTF_MAKE_NONCOPYABLE(ConcurrentBuffer);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ConcurrentBuffer);
public:
    
    ConcurrentBuffer() = default;
    
    ~ConcurrentBuffer()
    {
        if (auto* array = m_array) {
            for (auto& item : array->span())
                item.~T();
        }
    }

    // Growing is not concurrent. This assumes you are holding some other lock before you do this.
    void growExact(size_t newSize)
    {
        auto* array = m_array;
        if (array && newSize <= array->size)
            return;
        auto newArray = createArray(newSize);
        // This allows us to do ConcurrentBuffer<std::unique_ptr<>>.
        // asMutableByteSpan() avoids triggering -Wclass-memaccess.
        if (array)
            memcpySpan(asMutableByteSpan(newArray->span()), asByteSpan(array->span()));
        for (auto& item : newArray->span().subspan(array ? array->size : 0))
            new (&item) T();
        WTF::storeStoreFence();
        m_array = newArray.get();
        WTF::storeStoreFence();
        m_allArrays.append(WTFMove(newArray));
    }
    
    void grow(size_t newSize)
    {
        size_t size = m_array ? m_array->size : 0;
        if (newSize <= size)
            return;
        growExact(std::max(newSize, size * 2));
    }
    
    struct Array {
        size_t size; // This is an immutable size.
        T data[1];

        std::span<T> span() { return unsafeMakeSpan(data, size); }
        std::span<const T> span() const { return unsafeMakeSpan(data, size); }
    };

    using ArrayPtr = std::unique_ptr<Array, NonDestructingDeleter<Array, ConcurrentBufferMalloc>>;
    
    Array* array() const { return m_array; }
    
    T& operator[](size_t index) { return m_array->span()[index]; }
    const T& operator[](size_t index) const { return m_array->span()[index]; }
    
private:
    ArrayPtr createArray(size_t size)
    {
        Checked<size_t> objectSize = sizeof(T);
        objectSize *= size;
        objectSize += static_cast<size_t>(OBJECT_OFFSETOF(Array, data));
        auto* ptr = static_cast<Array*>(ConcurrentBufferMalloc::malloc(objectSize));
        auto result = ArrayPtr(ptr, { });
        result->size = size;
        return result;
    }
    
    Array* m_array { nullptr };
    Vector<ArrayPtr> m_allArrays;
};

} // namespace WTF
