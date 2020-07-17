/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include <wtf/HashTraits.h>
#include <wtf/PtrTag.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

template<PtrTag tag>
class MetaAllocatorPtr {
public:
    MetaAllocatorPtr() = default;
    MetaAllocatorPtr(std::nullptr_t) { }

    explicit MetaAllocatorPtr(void* ptr)
        : m_ptr(tagCodePtr<tag>(ptr))
    {
        assertIsNotTagged(ptr);
    }

    explicit MetaAllocatorPtr(uintptr_t ptrAsInt)
        : MetaAllocatorPtr(reinterpret_cast<void*>(ptrAsInt))
    { }

    template<typename T = void*>
    T untaggedPtr() const { return bitwise_cast<T>(untagCodePtr<tag>(m_ptr)); }

    template<PtrTag newTag, typename T = void*>
    T retaggedPtr() const { return bitwise_cast<T>(retagCodePtr<tag, newTag>(m_ptr)); }

    // Disallow any casting operations (except for booleans).
    template<typename T, typename = std::enable_if_t<!std::is_same<T, bool>::value>>
    operator T() = delete;

    explicit operator bool() const { return !!m_ptr; }
    bool operator!() const { return !m_ptr; }

    bool operator==(MetaAllocatorPtr other) const { return m_ptr == other.m_ptr; }
    bool operator!=(MetaAllocatorPtr other) const { return m_ptr != other.m_ptr; }

    MetaAllocatorPtr operator+(size_t sizeInBytes) const { return MetaAllocatorPtr(untaggedPtr<uint8_t*>() + sizeInBytes); }
    MetaAllocatorPtr operator-(size_t sizeInBytes) const { return MetaAllocatorPtr(untaggedPtr<uint8_t*>() - sizeInBytes); }

    MetaAllocatorPtr& operator+=(size_t sizeInBytes)
    {
        return *this = *this + sizeInBytes;
    }

    MetaAllocatorPtr& operator-=(size_t sizeInBytes)
    {
        return *this = *this - sizeInBytes;
    }

    enum EmptyValueTag { EmptyValue };
    enum DeletedValueTag { DeletedValue };

    MetaAllocatorPtr(EmptyValueTag)
        : m_ptr(emptyValue())
    { }

    MetaAllocatorPtr(DeletedValueTag)
        : m_ptr(deletedValue())
    { }

    bool isEmptyValue() const { return m_ptr == emptyValue(); }
    bool isDeletedValue() const { return m_ptr == deletedValue(); }

    unsigned hash() const { return PtrHash<void*>::hash(m_ptr); }

private:
    static void* emptyValue() { return reinterpret_cast<void*>(1); }
    static void* deletedValue() { return reinterpret_cast<void*>(2); }

    void* m_ptr { nullptr };
};

template<PtrTag tag>
struct MetaAllocatorPtrHash {
    static unsigned hash(const MetaAllocatorPtr<tag>& ptr) { return ptr.hash(); }
    static bool equal(const MetaAllocatorPtr<tag>& a, const MetaAllocatorPtr<tag>& b)
    {
        return a == b;
    }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

template<typename> struct DefaultHash;
template<PtrTag tag> struct DefaultHash<MetaAllocatorPtr<tag>> : MetaAllocatorPtrHash<tag> { };

template<PtrTag tag> struct HashTraits<MetaAllocatorPtr<tag>> : public CustomHashTraits<MetaAllocatorPtr<tag>> { };

} // namespace WTF

using WTF::MetaAllocatorPtr;
