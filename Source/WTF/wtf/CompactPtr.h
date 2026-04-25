/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/GetPtr.h>
#include <wtf/HashFunctions.h>
#include <wtf/Platform.h>
#include <wtf/RawPtrTraits.h>
#include <wtf/StdLibExtras.h>

#if OS(DARWIN)
#include <mach/vm_param.h>
#endif

namespace WTF {

#if HAVE(36BIT_ADDRESS)

// The reason we need OutsizedCompactPtr is that the OS linker / loader may choose
// to place statically allocated objects at addresses that don't fit within 36-bits
// (though address of heap allocated objects always fit in 36-bits). As such, they
// will not fit in the default 32-bit encoding of CompactPtrs.
//
// We observe that:
// 1. The OS will never allocate objects (heap or otherwise) within the __PAGEZERO
//    region.
// 2. The number of such statically allocated objects that we'll ever store in
//    CompactPtrs are finite and small-ish (on the order of < 1100 instances).
//
// Hence, we can use the addresses within __PAGEZERO to represent indexes into a
// table of OutsizedCompactPtrs where the full (> 36-bits) pointer value is actually
// stored.
//
// __PAGEZERO is currently around 4G in size. However, we'll conservatively reserve
// only the first 256K of addresses for OutsizedCompactPtrs. This allows us to
// encode up to 16K outsized pointers.
//
// Meanwhile, we should also reduce the number of statically allocated objects that
// can be stored in CompactPtrs. It would be ideal if the number of such objects
// reduce to way under 1022. With that, we would be able to encode all those pointers
// even if the size of __PAGEZERO is literally reduced to the size of 1 16K page.
// Until then, we'll work with the 256K heuristic.

class OutsizedCompactPtr {
public:
    using Encoded = uint32_t;

    WTF_EXPORT_PRIVATE static Encoded encode(void*);
    WTF_EXPORT_PRIVATE static void* decode(Encoded);

    // 0 is reserved for the empty value.
    // 1 is reserved for CompactPtr::hashDeletedStorageValue.
    // So, the min encoding for an OutsizedCompactPtr can only be 2.
    static constexpr uint32_t bitsShift = 4;
    static constexpr uintptr_t addressRangeForOutsizedPtrEncoding = 256 * 1024;

    constexpr static Encoded minEncoding = 2;
    constexpr static Encoded maxEncoding = addressRangeForOutsizedPtrEncoding >> bitsShift;
};
#endif // HAVE(36BIT_ADDRESS)


template <typename T>
class CompactPtr {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CompactPtr);
public:
#if HAVE(36BIT_ADDRESS)
    // The CompactPtr algorithm relies on shifting a 36-bit address
    // right by 4 bits to fit within 32 bits.
    // This operation is lossless only if the address is always
    // 16-byte aligned, meaning the lower 4 bits are always 0.
    using StorageType = uint32_t;
    static constexpr bool is32Bit = true;
#else
    using StorageType = uintptr_t;
    static constexpr bool is32Bit = false;
#endif
    static constexpr bool isCompactedType = true;
    static_assert(::allowCompactPointers<T*>());

    ALWAYS_INLINE constexpr CompactPtr() = default;

    ALWAYS_INLINE constexpr CompactPtr(std::nullptr_t) { }

    ALWAYS_INLINE CompactPtr(T* ptr) { set(ptr); }

    ALWAYS_INLINE constexpr CompactPtr(const CompactPtr& other) : m_ptr(other.m_ptr) { }

    template <typename X>
    ALWAYS_INLINE constexpr CompactPtr(const CompactPtr<X>& other) : m_ptr(other.m_ptr) { static_assert(std::is_convertible_v<X*, T*>); }

    ALWAYS_INLINE CompactPtr(CompactPtr&& other) { swap(other); }

    template <typename X>
    ALWAYS_INLINE CompactPtr(CompactPtr<X>&& other)
        : m_ptr(other.m_ptr)
    { 
        static_assert(std::is_convertible_v<X*, T*>);
        std::exchange(other.m_ptr, 0);
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

    CompactPtr<T>& operator=(const CompactPtr& other)
    {
        if (&other == this)
            return *this;

        CompactPtr copy(other);
        swap(copy);
        return *this;
    }

    template <typename X>
    CompactPtr<T>& operator=(const CompactPtr<X>& other)
    {
        static_assert(std::is_convertible_v<X*, T*>);
        CompactPtr copy(other);
        swap(copy);
        return *this;
    }

    CompactPtr<T>& operator=(T* optr)
    {
        CompactPtr copy(optr);
        swap(copy);
        return *this;
    }

    CompactPtr<T>& operator=(CompactPtr&& other)
    {
        CompactPtr moved(WTF::move(other));
        swap(moved);
        return *this;
    }

    template <typename X>
    CompactPtr<T>& operator=(CompactPtr<X>&& other)
    {
        static_assert(std::is_convertible_v<X*, T*>);
        CompactPtr moved(WTF::move(other));
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

    template<typename Other>
        requires Other::isCompactedType
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
        uintptr_t intPtr = std::bit_cast<uintptr_t>(ptr);
#if HAVE(36BIT_ADDRESS)
        static_assert(alignof(T) >= (1ULL << bitsShift));
        ASSERT(!(intPtr & alignmentMask));
        StorageType encoded = static_cast<StorageType>(intPtr >> bitsShift);
        if ((static_cast<uintptr_t>(encoded) << bitsShift) != intPtr) [[unlikely]]
            return std::bit_cast<StorageType>(OutsizedCompactPtr::encode(std::bit_cast<void*>(ptr)));
        return encoded;
#else
        return intPtr;
#endif
    }

    static ALWAYS_INLINE T* decode(StorageType ptr)
    {
#if HAVE(36BIT_ADDRESS)
        static_assert(alignof(T) >= (1ULL << bitsShift));
        static_assert(OutsizedCompactPtr::bitsShift == bitsShift);
        static_assert(OutsizedCompactPtr::minEncoding > hashDeletedStorageValue);

        if (ptr >= OutsizedCompactPtr::minEncoding && ptr < OutsizedCompactPtr::maxEncoding) [[unlikely]]
            return std::bit_cast<T*>(OutsizedCompactPtr::decode(ptr));
        return std::bit_cast<T*>(static_cast<uintptr_t>(ptr) << bitsShift);
#else
        return std::bit_cast<T*>(ptr);
#endif
    }

    bool isHashTableDeletedValue() const { return m_ptr == hashDeletedStorageValue; }

    template<typename U>
    friend bool operator==(const CompactPtr& a, const CompactPtr<U>& b)
    {
        return a.m_ptr == b.m_ptr;
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

template <typename T>
struct GetPtrHelper<CompactPtr<T>> {
    using PtrType = T*;
    using UnderlyingType = T;
    static T* getPtr(const CompactPtr<T>& p) { return const_cast<T*>(p.get()); }
};

template <typename T>
struct IsSmartPtr<CompactPtr<T>> {
    static constexpr bool value = true;
    static constexpr bool isNullable = true;
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
