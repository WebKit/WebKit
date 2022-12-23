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
#include <wtf/Forward.h>
#include <wtf/GetPtr.h>
#include <wtf/HashFunctions.h>
#include <wtf/RawPtrTraits.h>
#include <wtf/StdLibExtras.h>

#if OS(DARWIN)
#include <mach/vm_param.h>
#endif

namespace WTF {

#if CPU(ADDRESS64)
#if CPU(ARM64) && OS(DARWIN)
#if MACH_VM_MAX_ADDRESS_RAW < (1ULL << 36)
#define HAVE_36BIT_ADDRESS 1
#endif
#endif
#endif // CPU(ADDRESS64)

template <typename T>
class CompactPtr {
    WTF_MAKE_FAST_ALLOCATED;
public:
#if HAVE(36BIT_ADDRESS)
    // The CompactPtr algorithm relies on being able to shift
    // a 36-bit address right by 4 in order to fit in 32-bits.
    // The only way this is an ok thing to do without information
    // loss is if the if the address is always 16 bytes aligned i.e.
    // the lower 4 bits is always 0.
    using StorageType = uint32_t;
    static constexpr bool is32Bit = true;
#else
    using StorageType = uintptr_t;
    static constexpr bool is32Bit = false;
#endif
    static constexpr bool isCompactedType = true;

    ALWAYS_INLINE constexpr CompactPtr() = default;

    ALWAYS_INLINE constexpr CompactPtr(std::nullptr_t) { }

    ALWAYS_INLINE CompactPtr(T* ptr) { set(ptr); }

    ALWAYS_INLINE constexpr CompactPtr(const CompactPtr& o) : m_ptr(o.m_ptr) { }

    template <typename X>
    ALWAYS_INLINE constexpr CompactPtr(const CompactPtr<X>& o) : m_ptr(o.m_ptr) { static_assert(std::is_convertible_v<X*, T*>); }

    ALWAYS_INLINE CompactPtr(CompactPtr&& o) { swap(o); }

    template <typename X>
    ALWAYS_INLINE CompactPtr(CompactPtr<X>&& o)
        : m_ptr(o.m_ptr)
    { 
        static_assert(std::is_convertible_v<X*, T*>);
        std::exchange(o.m_ptr, 0);
    }

    ALWAYS_INLINE constexpr CompactPtr(HashTableDeletedValueType) : m_ptr(hashDeletedStorageValue) { }

    ALWAYS_INLINE ~CompactPtr() = default;

    T& operator*() const { return *get(); }

    ALWAYS_INLINE T* operator->() const { return get(); }

    bool operator!() const { return !get(); }

    explicit operator bool() const { return !!get(); }

    CompactPtr<T>& operator=(std::nullptr_t)
    {
        exchange(nullptr);
        return *this;
    }

    CompactPtr<T>& operator=(const CompactPtr& o)
    {
        CompactPtr copy(o);
        swap(copy);
        return *this;
    }

    template <typename X>
    CompactPtr<T>& operator=(const CompactPtr<X>& o)
    {
        static_assert(std::is_convertible_v<X*, T*>);
        CompactPtr copy(o);
        swap(copy);
        return *this;
    }

    CompactPtr<T>& operator=(T* optr)
    {
        CompactPtr copy(optr);
        swap(copy);
        return *this;
    }

    CompactPtr<T>& operator=(CompactPtr&& o)
    {
        CompactPtr moved(WTFMove(o));
        swap(moved);
        return *this;
    }

    template <typename X>
    CompactPtr<T>& operator=(CompactPtr<X>&& o)
    {
        static_assert(std::is_convertible_v<X*, T*>);
        CompactPtr moved(WTFMove(o));
        swap(moved);
        return *this;
    }

    T* get() const { return decode(m_ptr); }

    void set(T* ptr) { m_ptr = encode(ptr); }

    template <class U>
    T* exchange(U&& newValue)
    {
        T* oldValue = get();
        set(std::forward<U>(newValue));
        return oldValue;
    }

    void swap(std::nullptr_t) { set(nullptr); }

    void swap(CompactPtr& other) { std::swap(m_ptr, other.m_ptr); }

    template <typename Other, typename = std::enable_if_t<Other::isCompactedType>>
    void swap(Other& other)
    {
        T* t1 = get();
        T* t2 = other.get();
        set(t2);
        other.set(t1);
    }

    void swap(T*& t2)
    {
        T* t1 = get();
        std::swap(t1, t2);
        set(t1);
    }

    static ALWAYS_INLINE StorageType encode(T* ptr)
    {
        uintptr_t intPtr = bitwise_cast<uintptr_t>(ptr);
#if HAVE(36BIT_ADDRESS)
        static_assert(alignof(T) >= (1ULL << bitsShift));
        ASSERT(!(intPtr & alignmentMask));
        StorageType encoded = static_cast<StorageType>(intPtr >> bitsShift);
        ASSERT(decode(encoded) == ptr);
        return encoded;
#else
        return intPtr;
#endif
    }

    static ALWAYS_INLINE T* decode(StorageType ptr)
    {
#if HAVE(36BIT_ADDRESS)
        static_assert(alignof(T) >= (1ULL << bitsShift));
        return bitwise_cast<T*>(static_cast<uintptr_t>(ptr) << bitsShift);
#else
        return bitwise_cast<T*>(ptr);
#endif
    }

    bool isHashTableDeletedValue() const { return m_ptr == hashDeletedStorageValue; }

    template<typename U>
    friend bool operator==(const CompactPtr& a, const CompactPtr<U>& b)
    {
        return a.m_ptr == b.m_ptr;
    }

    template<typename U>
    friend bool operator!=(const CompactPtr& a, const CompactPtr<U>& b)
    {
        return a.m_ptr != b.m_ptr;
    }

    StorageType storage() const { return m_ptr; }

private:
    template <typename X>
    friend class CompactPtr;

    static constexpr uint32_t bitsShift = 4;
    static constexpr uintptr_t alignmentMask = (1ull << bitsShift) - 1;
    static constexpr StorageType hashDeletedStorageValue = 1; // 0x16 (encoded as 1) is within the first unmapped page for nullptr. Thus, it never appears.

    StorageType m_ptr { 0 };
};

template<typename T, typename U>
inline bool operator==(const CompactPtr<T>& a, U* b)
{
    return a.get() == b;
}

template<typename T, typename U>
inline bool operator==(T* a, const CompactPtr<U>& b)
{
    return a == b.get();
}

template<typename T, typename U>
inline bool operator!=(const CompactPtr<T>& a, U* b)
{
    return !(a == b);
}

template<typename T, typename U>
inline bool operator!=(T* a, const CompactPtr<U>& b)
{
    return !(a == b);
}

template <typename T>
struct GetPtrHelper<CompactPtr<T>> {
    using PtrType = T*;
    static T* getPtr(const CompactPtr<T>& p) { return const_cast<T*>(p.get()); }
};

template <typename T>
struct IsSmartPtr<CompactPtr<T>> {
    static constexpr bool value = true;
};

template <typename T>
struct CompactPtrTraits {
    template <typename U>
    using RebindTraits = RawPtrTraits<U>;

    using StorageType = CompactPtr<T>;

    static constexpr bool is32Bit = StorageType::is32Bit;

    template <typename U>
    static ALWAYS_INLINE T* exchange(StorageType& ptr, U&& newValue) { return ptr.exchange(newValue); }

    template <typename Other>
    static ALWAYS_INLINE void swap(StorageType& a, Other& b) { a.swap(b); }

    static ALWAYS_INLINE T* unwrap(const StorageType& ptr) { return ptr.get(); }

    static StorageType hashTableDeletedValue() { return StorageType { HashTableDeletedValue }; }
    static ALWAYS_INLINE bool isHashTableDeletedValue(const StorageType& ptr) { return ptr.isHashTableDeletedValue(); }
};

template<typename P> struct DefaultHash<CompactPtr<P>> {
    using PtrType = CompactPtr<P>;
    static unsigned hash(PtrType key) { return IntHash<typename PtrType::StorageType>::hash(key.storage()); }
    static bool equal(PtrType a, PtrType b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

} // namespace WTF

using WTF::CompactPtr;
using WTF::CompactPtrTraits;
