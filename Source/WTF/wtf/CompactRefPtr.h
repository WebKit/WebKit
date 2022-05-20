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
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

#if CPU(ADDRESS64)
#if MACH_VM_MAX_ADDRESS_RAW < (1ULL << 36)
#define HAVE_36BIT_ADDRESS 1
#endif
#endif

template <typename T>
class Compacted {
    WTF_MAKE_FAST_ALLOCATED;

public:
#if HAVE(36BIT_ADDRESS)
    using StorageSize = uint32_t;
#else
    using StorageSize = uintptr_t;
#endif
    static constexpr bool isCompactedType = true;

    Compacted()
    {
        set(nullptr);
    }

    Compacted(nullptr_t)
    {
        set(nullptr);
    }

    Compacted(T* ptr)
    {
        set(ptr);
    }

    Compacted<T>& operator=(T* ptr)
    {
        set(ptr);
        return *this;
    }

    T* operator->() const { return get(); }

    T& operator*() const { return *get(); }

    bool operator!() const { return !get(); }

    explicit operator bool() const { return !!get(); }

    T* get() const
    {
        return decode(m_ptr);
    }

    void set(T* ptr)
    {
        m_ptr = encode(ptr);
    }

    template <class U>
    T* exchange(U&& newValue)
    {
        T* oldValue = get();
        set(std::forward<U>(newValue));
        return oldValue;
    }

    void swap(nullptr_t) { set(nullptr); }

    void swap(Compacted& other)
    {
        T* t1 = get();
        T* t2 = other.get();
        set(t2);
        other.set(t1);
    }

    template <typename Other, typename = std::enable_if_t<Other::isCompactedType>>
    void swap(Other& other)
    {
        T* t1 = get();
        T* t2 = other.get();
        set(t2);
        other.set(t1);
    }

    void swap(T* t2)
    {
        T* t1 = get();
        std::swap(t1, t2);
        set(t1);
    }

    static ALWAYS_INLINE constexpr StorageSize encode(uintptr_t ptr)
    {
#if HAVE(36BIT_ADDRESS)
        ASSERT(!(ptr & 0xF));
        return static_cast<StorageSize>(ptr >> 4);
#else
        return ptr;
#endif
    }

    ALWAYS_INLINE constexpr StorageSize encode(T* ptr) const
    {
        return encode(reinterpret_cast<uintptr_t>(ptr));
    }

    ALWAYS_INLINE constexpr T* decode(const StorageSize& ptr) const
    {
#if HAVE(36BIT_ADDRESS)
        return reinterpret_cast<T*>(static_cast<uintptr_t>(ptr) << 4);
#else
        return reinterpret_cast<T*>(ptr);
#endif
    }

private:
    StorageSize m_ptr;
};

template <typename T>
struct GetPtrHelper<Compacted<T>> {
    using PtrType = T*;
    static T* getPtr(const Compacted<T>& p) { return const_cast<T*>(p.get()); }
};

template <typename T>
struct IsSmartPtr<Compacted<T>> {
    static constexpr bool value = true;
};

template <typename T>
struct CompactPtrTraits {
    template <typename U>
    using RebindTraits = RawPtrTraits<U>;

    using StorageType = Compacted<T>;

    template <typename U>
    static ALWAYS_INLINE T* exchange(StorageType& ptr, U&& newValue) { return ptr.exchange(newValue); }

    template <typename Other>
    static ALWAYS_INLINE void swap(StorageType& a, Other& b) { a.swap(b); }

    static ALWAYS_INLINE T* unwrap(const StorageType& ptr) { return ptr.get(); }

    static StorageType hashTableDeletedValue() { return bitwise_cast<StorageType>(static_cast<uintptr_t>(-1)); }
    static ALWAYS_INLINE bool isHashTableDeletedValue(const StorageType& ptr) { return ptr == hashTableDeletedValue(); }
};

template <typename T>
using CompactRefPtr = RefPtr<T, CompactPtrTraits<T>>;

} // namespace WTF

using WTF::CompactRefPtr;
