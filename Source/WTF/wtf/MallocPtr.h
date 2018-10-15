/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include <wtf/FastMalloc.h>

// MallocPtr is a smart pointer class that calls fastFree in its destructor.
// It is intended to be used for pointers where the C++ lifetime semantics
// (calling constructors and destructors) is not desired. 

namespace WTF {

template<typename T, typename Malloc = FastMalloc> class MallocPtr {
public:
    MallocPtr()
        : m_ptr(nullptr)
    {
    }

    MallocPtr(std::nullptr_t)
        : m_ptr(nullptr)
    {
    }

    MallocPtr(MallocPtr&& other)
        : m_ptr(other.leakPtr())
    {
    }

    ~MallocPtr()
    {
        Malloc::free(m_ptr);
    }

    T* get() const
    {
        return m_ptr;
    }

    T *leakPtr() WARN_UNUSED_RETURN
    {
        return std::exchange(m_ptr, nullptr);
    }

    explicit operator bool() const
    {
        return m_ptr;
    }

    bool operator!() const
    {
        return !m_ptr;
    }

    T& operator*() const
    {
        ASSERT(m_ptr);
        return *m_ptr;
    }

    T* operator->() const
    {
        return m_ptr;
    }

    MallocPtr& operator=(MallocPtr&& other)
    {
        MallocPtr ptr = WTFMove(other);
        swap(ptr);

        return *this;
    }

    void swap(MallocPtr& other)
    {
        std::swap(m_ptr, other.m_ptr);
    }

    template<typename U> friend MallocPtr<U> adoptMallocPtr(U*);

    static MallocPtr malloc(size_t size)
    {
        return MallocPtr { static_cast<T*>(Malloc::malloc(size)) };
    }

    static MallocPtr tryMalloc(size_t size)
    {
        return MallocPtr { static_cast<T*>(Malloc::tryMalloc(size)) };
    }

    void realloc(size_t newSize)
    {
        m_ptr = static_cast<T*>(Malloc::realloc(m_ptr, newSize));
    }

private:
    explicit MallocPtr(T* ptr)
        : m_ptr(ptr)
    {
    }

    T* m_ptr;
};

static_assert(sizeof(MallocPtr<int>) == sizeof(int*), "");

template<typename U> MallocPtr<U> adoptMallocPtr(U* ptr)
{
    return MallocPtr<U>(ptr);
}

} // namespace WTF

using WTF::MallocPtr;
using WTF::adoptMallocPtr;
