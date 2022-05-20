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
#include <wtf/RawPtrTraits.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

template<typename T> struct DefaultCompactPtrTraits {
#if PLATFORM(IOS_FAMILY)
    static_assert(MACH_VM_MAX_ADDRESS <= (1ull << 36));
    using StorageType = uint32_t;
#else
    using StorageType = T*;
#endif

    static ALWAYS_INLINE constexpr StorageType encode(T* ptr)
    {
#if PLATFORM(IOS_FAMILY)
        ASSERT(!(reinterpret_cast<uintptr_t>(ptr) & 0xF));
        return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ptr) >> 4);
#else
        return ptr;
#endif
    }

    static ALWAYS_INLINE constexpr T* decode(const StorageType& ptr)
    {
#if PLATFORM(IOS_FAMILY)
        return reinterpret_cast<T*>(static_cast<uintptr_t>(ptr) << 4);
#else
        return ptr;
#endif
    }
};

template<typename T, typename CompactPtrTraits = DefaultCompactPtrTraits<T>>
class CompactPtr {

public:
    ALWAYS_INLINE constexpr CompactPtr() : m_ptr(CompactPtrTraits::encode(nullptr)) { }
    ALWAYS_INLINE constexpr CompactPtr(std::nullptr_t) : m_ptr(CompactPtrTraits::encode(nullptr)) { }
    ALWAYS_INLINE CompactPtr(T* ptr) : m_ptr(CompactPtrTraits::encode(ptr)) { }
    ALWAYS_INLINE CompactPtr(const CompactPtr& o) : m_ptr(o.m_ptr) { }
    ALWAYS_INLINE CompactPtr(CompactPtr&& o) : m_ptr(o.m_ptr) { }

    ALWAYS_INLINE ~CompactPtr() = default;

    T& operator*() const
    {
        T* ptr = CompactPtrTraits::decode(m_ptr);
        ASSERT(ptr);
        return *ptr;
    }

    ALWAYS_INLINE T* operator->() const { return CompactPtrTraits::decode(m_ptr); }

    bool operator!() const { return !CompactPtrTraits::decode(m_ptr); }

    explicit operator bool() const { return !!CompactPtrTraits::decode(m_ptr); }

    CompactPtr& operator=(std::nullptr_t)
    {
        m_ptr = CompactPtrTraits::encode(nullptr);
        return *this;
    }

    CompactPtr& operator=(const CompactPtr& o)
    {
        m_ptr = o.m_ptr;
        return *this;
    }

    CompactPtr& operator=(T* ptr)
    {
        m_ptr = CompactPtrTraits::encode(ptr);
        return *this;
    }

    template<typename X, typename Y> CompactPtr& operator=(const CompactPtr<X>& o)
    {
        m_ptr = o.m_ptr;
        return *this;
    }

    CompactPtr& operator=(CompactPtr&& o)
    {
        m_ptr = o.m_ptr;
        return *this;
    }

    template<typename X, typename Y> CompactPtr& operator=(CompactPtr<X>&& o)
    {
        m_ptr = o.m_ptr;
        return *this;
    }

    T* get() const { return CompactPtrTraits::decode(m_ptr); }

private:
    typename CompactPtrTraits::StorageType m_ptr;
};

} // namespace WTF

using WTF::CompactPtr;
