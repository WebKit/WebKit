/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#include <bit>
#include <concepts>

namespace WTF {

template<typename Trait, typename T>
concept TaggingTraits = requires (const T* ptr, typename Trait::StorageType storage, typename Trait::TagType tag) {
    { Trait::defaultTag } -> std::convertible_to<typename Trait::TagType>;
    { Trait::wrap(ptr, tag) } -> std::convertible_to<typename Trait::StorageType>;
    { Trait::unwrapPtr(storage) } -> std::convertible_to<T*>;
    { Trait::unwrapTag(storage) } -> std::convertible_to<typename Trait::TagType>;
};

template<typename T, TaggingTraits<T> Traits>
class TaggedPtr {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(TaggedPtr);
public:
    using StorageType = typename Traits::StorageType;
    using TagType = typename Traits::TagType;

    TaggedPtr() = default;
    TaggedPtr(const T* ptr, TagType tag = Traits::defaultTag)
        : m_ptr(Traits::wrap(ptr, tag))
    { }

    TagType tag() const { return Traits::unwrapTag(m_ptr); }
    const T* ptr() const { return Traits::unwrapPtr(m_ptr); }
    T* ptr() { return Traits::unwrapPtr(m_ptr); }

    void set(const T* t, TagType tag) { m_ptr = Traits::wrap(t, tag); }
    void setTag(TagType tag) { m_ptr = Traits::wrap(ptr(), tag); }

    TaggedPtr& operator=(const T* t)
    {
        m_ptr = Traits::wrap(t, tag());
        return *this;
    }

    const T* operator->() const { return ptr(); }
    T* operator->() { return ptr(); }

private:
    StorageType m_ptr { Traits::wrap(nullptr, Traits::defaultTag) };
};


template<typename T, typename Enum, Enum defaultEnumTag = static_cast<Enum>(0)>
struct EnumTaggingTraits {
    using StorageType = uintptr_t;
    using TagType = Enum;
    static constexpr TagType defaultTag = defaultEnumTag;

    static StorageType wrap(const T* ptr, TagType tag)
    {
        ASSERT_WITH_MESSAGE((static_cast<StorageType>(tag) | tagMask32Bit) == tagMask32Bit, "Tag is too big for 32-bit storage");
        ASSERT(fromStorage(toStorage(tag)) == tag);
        return std::bit_cast<StorageType>(ptr) | toStorage(tag);
    }

#if CPU(ADDRESS64)
    static T* unwrapPtr(StorageType storage) { return std::bit_cast<T*>(storage & ptrMask); }
#else
    static T* unwrapPtr(StorageType storage) { return std::bit_cast<T*>(storage & ~tagMask32Bit); }
#endif

    static TagType unwrapTag(StorageType storage) { return fromStorage(storage); }

    static constexpr StorageType tagMask32Bit = (1 << (alignof(std::remove_pointer_t<T>) - 1)) - 1;
#if CPU(ADDRESS64)
    static constexpr unsigned tagShift = sizeof(StorageType) * CHAR_BIT - CHAR_BIT + 4; // Save the bottom four bits of the high byte for other uses.
    static constexpr StorageType ptrMask = (1ull << tagShift) - 1;
    static StorageType toStorage(TagType tag) { return static_cast<StorageType>(tag) << tagShift; }
    static TagType fromStorage(StorageType storage) { return static_cast<TagType>(storage >> tagShift); }
#else
    static StorageType toStorage(TagType tag) { return static_cast<StorageType>(tag); }
    static TagType fromStorage(StorageType storage) { return static_cast<TagType>(storage & tagMask32Bit); }
#endif
};

// Useful for places where you sometimes want to tag and sometimes not based on template parameters.
template<typename T>
struct NoTaggingTraits {
    using StorageType = uintptr_t;
    using TagType = unsigned;
    static constexpr TagType defaultTag = 0;
    static StorageType wrap(const T* ptr, TagType) { return std::bit_cast<StorageType>(ptr); }
    static T* unwrapPtr(StorageType storage) { return std::bit_cast<T*>(storage); }
    static TagType unwrapTag(StorageType) { return defaultTag; }
};

class TaggedBits60 {
public:
TaggedBits60(uint64_t bits, uint8_t tag)
    {
        uint64_t bigTag = static_cast<uint64_t>(tag);
        ASSERT((bigTag << tagShift) >> tagShift == tag);
        ASSERT(!(bits & ~ptrMask));
        m_bits = bits | (bigTag << tagShift);
    }

    template <typename T>
    TaggedBits60(T* bits, uint8_t tag)
        : TaggedBits60(std::bit_cast<uintptr_t>(bits), tag)
    {
    }

    TaggedBits60(std::nullptr_t)
        : m_bits(0)
    {
    }

    uint64_t bits() const { return m_bits & ptrMask; }
    void* ptr() const { return std::bit_cast<void*>(static_cast<uintptr_t>(bits())); }
    uint8_t tag() const { return (m_bits & ~ptrMask) >> tagShift; }

private:
    static constexpr size_t tagShift = 60;
    static constexpr uint64_t ptrMask = (1ull << tagShift) - 1;
    uint64_t m_bits;
};

} // namespace WTF

using WTF::TaggedPtr;
using WTF::EnumTaggingTraits;
using WTF::NoTaggingTraits;
