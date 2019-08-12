/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include <array>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UnalignedAccess.h>

namespace WTF {

template<typename T>
class Packed {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static constexpr bool isPackedType = true;

    Packed()
        : Packed(T { })
    {
    }

    Packed(const T& value)
    {
        unalignedStore<T>(m_storage.data(), value);
    }

    T get() const
    {
        return unalignedLoad<T>(m_storage.data());
    }

    void set(const T& value)
    {
        unalignedStore<T>(m_storage.data(), value);
    }

    Packed<T>& operator=(const T& value)
    {
        set(value);
        return *this;
    }

    template<class U>
    T exchange(U&& newValue)
    {
        T oldValue = get();
        set(std::forward<U>(newValue));
        return oldValue;
    }

    void swap(Packed& other)
    {
        m_storage.swap(other.m_storage);
    }

    template<typename Other, typename = std::enable_if_t<Other::isPackedType>>
    void swap(Other& other)
    {
        T t1 = get();
        T t2 = other.get();
        set(t2);
        other.set(t1);
    }

    void swap(T& t2)
    {
        T t1 = get();
        std::swap(t1, t2);
        set(t1);
    }

private:
    std::array<uint8_t, sizeof(T)> m_storage;
};

// PackedAlignedPtr can take alignment parameter too. PackedAlignedPtr only uses this alignment information if it is profitable: we use
// alignment information only when we can reduce the size of the storage. Since the pointer width is 36 bits and JSCells are aligned to 16 bytes,
// we can use 4 bits in Darwin ARM64, we can compact cell pointer into 4 bytes (32 bits).
template<typename T, size_t alignment = alignof(T)>
class PackedAlignedPtr {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static_assert(hasOneBitSet(alignment), "Alignment needs to be power-of-two");
    static constexpr bool isPackedType = true;
    static constexpr unsigned alignmentShiftSizeIfProfitable = getLSBSetConstexpr(alignment);
    static constexpr unsigned storageSizeWithoutAlignmentShift = roundUpToMultipleOf<8>(WTF_CPU_EFFECTIVE_ADDRESS_WIDTH) / 8;
    static constexpr unsigned storageSizeWithAlignmentShift = roundUpToMultipleOf<8>(WTF_CPU_EFFECTIVE_ADDRESS_WIDTH - alignmentShiftSizeIfProfitable) / 8;
    static constexpr bool isAlignmentShiftProfitable = storageSizeWithoutAlignmentShift > storageSizeWithAlignmentShift;
    static constexpr unsigned alignmentShiftSize = isAlignmentShiftProfitable ? alignmentShiftSizeIfProfitable : 0;
    static constexpr unsigned storageSize = storageSizeWithAlignmentShift;

    constexpr PackedAlignedPtr()
        : m_storage()
    {
    }

    constexpr PackedAlignedPtr(std::nullptr_t)
        : m_storage()
    {
    }

    PackedAlignedPtr(T* value)
    {
        set(value);
    }

    T* get() const
    {
        // FIXME: PackedPtr<> can load memory with one mov by checking page boundary.
        // https://bugs.webkit.org/show_bug.cgi?id=197754
        uintptr_t value = 0;
#if CPU(LITTLE_ENDIAN)
        memcpy(&value, m_storage.data(), storageSize);
#else
        memcpy(bitwise_cast<uint8_t*>(&value) + (sizeof(void*) - storageSize), m_storage.data(), storageSize);
#endif
        if (isAlignmentShiftProfitable)
            value <<= alignmentShiftSize;
        return bitwise_cast<T*>(value);
    }

    void set(T* passedValue)
    {
        uintptr_t value = bitwise_cast<uintptr_t>(passedValue);
        if (isAlignmentShiftProfitable)
            value >>= alignmentShiftSize;
#if CPU(LITTLE_ENDIAN)
        memcpy(m_storage.data(), &value, storageSize);
#else
        memcpy(m_storage.data(), bitwise_cast<uint8_t*>(&value) + (sizeof(void*) - storageSize), storageSize);
#endif
    }

    void clear()
    {
        set(nullptr);
    }

    T* operator->() const { return get(); }
    T& operator*() const { return *get(); }
    bool operator!() const { return !get(); }
    explicit operator bool() const { return get(); }

    PackedAlignedPtr& operator=(T* value)
    {
        set(value);
        return *this;
    }

    template<class U>
    T exchange(U&& newValue)
    {
        T oldValue = get();
        set(std::forward<U>(newValue));
        return oldValue;
    }

    void swap(std::nullptr_t) { clear(); }

    void swap(PackedAlignedPtr& other)
    {
        m_storage.swap(other.m_storage);
    }

    template<typename Other, typename = std::enable_if_t<Other::isPackedType>>
    void swap(Other& other)
    {
        T t1 = get();
        T t2 = other.get();
        set(t2);
        other.set(t1);
    }

    void swap(T& t2)
    {
        T t1 = get();
        std::swap(t1, t2);
        set(t1);
    }

private:
    std::array<uint8_t, storageSize> m_storage;
};

template<typename T>
class Packed<T*> : public PackedAlignedPtr<T, 1> {
public:
    using Base = PackedAlignedPtr<T, 1>;
    using Base::Base;
};

template<typename T>
using PackedPtr = Packed<T*>;

template<typename T>
struct PackedPtrTraits {
    template<typename U> using RebindTraits = PackedPtrTraits<U>;

    using StorageType = PackedPtr<T>;

    template<class U> static ALWAYS_INLINE T* exchange(StorageType& ptr, U&& newValue) { return ptr.exchange(newValue); }

    template<typename Other> static ALWAYS_INLINE void swap(PackedPtr<T>& a, Other& b) { a.swap(b); }

    static ALWAYS_INLINE T* unwrap(const StorageType& ptr) { return ptr.get(); }
};

} // namespace WTF

using WTF::Packed;
using WTF::PackedAlignedPtr;
using WTF::PackedPtr;
