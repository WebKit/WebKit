/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <cstdint>
#include <utility>
#include <wtf/CompactRefPtr.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

template <typename T, typename Traits = CompactPtrTraits<T>>
class CompactPtr {
public:
    ALWAYS_INLINE constexpr CompactPtr()
        : m_ptr(nullptr)
    {
    }

    ALWAYS_INLINE constexpr CompactPtr(std::nullptr_t)
        : m_ptr(nullptr)
    {
    }

    ALWAYS_INLINE CompactPtr(T* ptr)
        : m_ptr(ptr)
    {
    }

    ALWAYS_INLINE CompactPtr(const CompactPtr& o)
        : m_ptr(o.m_ptr)
    {
    }

    ALWAYS_INLINE CompactPtr(CompactPtr&& o)
        : m_ptr(o.m_ptr)
    {
    }

    ALWAYS_INLINE ~CompactPtr() = default;

    T& operator*() const
    {
        T* ptr = Traits::unwrap(m_ptr);
        ASSERT(ptr);
        return *ptr;
    }

    ALWAYS_INLINE T* operator->() const { return Traits::unwrap(m_ptr); }

    bool operator!() const { return !m_ptr; }

    explicit operator bool() const { return !!m_ptr; }

    CompactPtr& operator=(std::nullptr_t)
    {
        Traits::exchange(m_ptr, nullptr);
        return *this;
    }

    CompactPtr& operator=(const CompactPtr& o)
    {
        CompactPtr ptr = o;
        Traits::swap(m_ptr, ptr);
        return *this;
    }

    CompactPtr& operator=(T* optr)
    {
        CompactPtr ptr = optr;
        Traits::swap(m_ptr, ptr);
        return *this;
    }

    template <typename X, typename Y>
    CompactPtr& operator=(const CompactPtr<X>& o)
    {
        CompactPtr ptr = o;
        Traits::swap(m_ptr, ptr);
        return *this;
    }

    CompactPtr& operator=(CompactPtr&& o)
    {
        CompactPtr ptr = WTFMove(o);
        Traits::swap(m_ptr, ptr);
        return *this;
    }

    template <typename X, typename Y>
    CompactPtr& operator=(CompactPtr<X>&& o)
    {
        CompactPtr ptr = WTFMove(o);
        Traits::swap(m_ptr, ptr);
        return *this;
    }

    T* get() const { return Traits::unwrap(m_ptr); }

private:
    typename Traits::StorageType m_ptr;
};

} // namespace WTF

using WTF::CompactPtr;
