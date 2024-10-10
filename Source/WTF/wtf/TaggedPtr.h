/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

namespace WTF {

// TaggingTraits should have the following API:
// TaggingTraits {
//     using StorageType = ...;
//     using TagType = ...;
//     static constexpr TagType defaultTag;
//     static StorageType encode(const T*, TagType);
//     static T* extractPtr(StorageType);
//     static TagType extractTag(StorageType);
// }
// FIXME: This could have a concept, which would make diagnosing TaggingTraits API errors easier.
template<typename T, typename TaggingTraits>
class TaggedPtr {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using StorageType = typename TaggingTraits::StorageType;
    using TagType = typename TaggingTraits::TagType;

    TaggedPtr() = default;
    TaggedPtr(const T* ptr, TagType tag = TaggingTraits::defaultTag)
        : m_ptr(TaggingTraits::encode(ptr, tag))
    { }

    TagType tag() const { return TaggingTraits::extractTag(m_ptr); }
    const T* ptr() const { return TaggingTraits::extractPtr(m_ptr); }
    T* ptr() { return TaggingTraits::extractPtr(m_ptr); }

    void set(const T* t, TagType tag) { m_ptr = TaggingTraits::encode(t, tag); }
    void setTag(TagType tag) { m_ptr = TaggingTraits::encode(ptr(), tag); }

    TaggedPtr& operator=(const T* t)
    {
        m_ptr = TaggingTraits::encode(t, tag());
        return *this;
    }

private:
    StorageType m_ptr { TaggingTraits::encode(nullptr, TaggingTraits::defaultTag) };
};

// NB. This class relies on top byte ignore on ARM64 CPUs as it assumes pointer reads are more common than pointer writes.
// So the pointer it returns from extractPtr could have high bits set on those CPUs. This is notable for places that want to
// play with the bits of the pointer e.g. JSC's NativeCallee.
template<typename T, typename Enum, Enum defaultEnumTag = static_cast<Enum>(0)>
struct EnumTaggingTraits {
    using StorageType = uintptr_t;
    using TagType = Enum;
    static constexpr TagType defaultTag = defaultEnumTag;

    static StorageType encode(const T* ptr, TagType tag)
    {
        ASSERT_WITH_MESSAGE((static_cast<StorageType>(tag) | tagMask32Bit) == tagMask32Bit, "Tag is too big for 32-bit storage");
        ASSERT(fromStorage(toStorage(tag)) == tag);
#if CPU(ARM64) && CPU(ADDRESS64)
        // We could be re-encoding an old pointer so we need to strip any potential old tag.
        return (bitwise_cast<StorageType>(ptr) & ptrMask) | toStorage(tag);
#else
        return bitwise_cast<StorageType>(ptr) | toStorage(tag);
#endif
    }

#if CPU(ARM64) && CPU(ADDRESS64)
    // This class relies on top byte ignore on ARM64 CPUs.
    static T* extractPtr(StorageType storage) { return bitwise_cast<T*>(storage); }
#elif CPU(ADDRESS64)
    static T* extractPtr(StorageType storage) { return bitwise_cast<T*>(storage & ptrMask); }
#else
    static T* extractPtr(StorageType storage) { return bitwise_cast<T*>(storage & ~tagMask32Bit); }
#endif

    static TagType extractTag(StorageType storage) { return fromStorage(storage); }

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
    static StorageType encode(const T* ptr, TagType) { return bitwise_cast<StorageType>(ptr); }
    static T* extractPtr(StorageType storage) { return bitwise_cast<T*>(storage); }
    static TagType extractTag(StorageType) { return defaultTag; }
};

} // namespace WTF

using WTF::TaggedPtr;
using WTF::EnumTaggingTraits;
using WTF::NoTaggingTraits;
