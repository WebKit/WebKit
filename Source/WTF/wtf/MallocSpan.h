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

#include <utility>
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TypeTraits.h>

// MallocSpan is a smart pointer class that wraps a std::span and calls fastFree in its destructor.

namespace WTF {

template<typename T, typename Malloc = FastMalloc> class MallocSpan {
    WTF_MAKE_NONCOPYABLE(MallocSpan);
public:
    MallocSpan() = default;

    MallocSpan(MallocSpan&& other)
        : m_span(other.leakSpan())
    {
    }

    template<typename U>
    MallocSpan(MallocSpan<U, Malloc>&& other) requires (std::is_same_v<T, uint8_t>)
        : m_span(asWritableBytes(other.leakSpan()))
    {
    }

    ~MallocSpan()
    {
        if constexpr (parameterCount(Malloc::free) == 2)
            Malloc::free(m_span.data(), m_span.size());
        else
            Malloc::free(m_span.data());
    }

    MallocSpan& operator=(MallocSpan&& other)
    {
        MallocSpan ptr = WTFMove(other);
        swap(ptr);
        return *this;
    }

    void swap(MallocSpan& other)
    {
        std::swap(m_span, other.m_span);
    }

    size_t sizeInBytes() const { return m_span.size_bytes(); }

    std::span<const T> span() const { return spanConstCast<const T>(m_span); }
    std::span<T> mutableSpan() { return m_span; }
    std::span<T> leakSpan() WARN_UNUSED_RETURN { return std::exchange(m_span, std::span<T>()); }

    T& operator[](size_t i) { return m_span[i]; }
    const T& operator[](size_t i) const { return m_span[i]; }

    explicit operator bool() const
    {
        return m_span.data();
    }

    bool operator!() const
    {
        return !m_span.data();
    }

    static MallocSpan malloc(size_t sizeInBytes)
    {
        return MallocSpan { static_cast<T*>(Malloc::malloc(sizeInBytes)), sizeInBytes };
    }

    static MallocSpan zeroedMalloc(size_t sizeInBytes)
    {
        return MallocSpan { static_cast<T*>(Malloc::zeroedMalloc(sizeInBytes)), sizeInBytes };
    }

#if HAVE(MMAP)
    static MallocSpan mmap(size_t sizeInBytes, int pageProtection, int options, int fileDescriptor)
    {
        return MallocSpan { static_cast<T*>(Malloc::mmap(sizeInBytes, pageProtection, options, fileDescriptor)), sizeInBytes };
    }
#endif

    static MallocSpan tryMalloc(size_t sizeInBytes)
    {
        auto* ptr = Malloc::tryMalloc(sizeInBytes);
        if (!ptr)
            return { };
        return MallocSpan { static_cast<T*>(ptr), sizeInBytes };
    }

    static MallocSpan tryZeroedMalloc(size_t sizeInBytes)
    {
        auto* ptr = Malloc::tryZeroedMalloc(sizeInBytes);
        if (!ptr)
            return { };
        return MallocSpan { static_cast<T*>(ptr), sizeInBytes };
    }

    void realloc(size_t newSizeInBytes)
    {
        RELEASE_ASSERT(!(newSizeInBytes % sizeof(T)));
        m_span = unsafeMakeSpan(static_cast<T*>(Malloc::realloc(m_span.data(), newSizeInBytes)), newSizeInBytes / sizeof(T));
    }

private:
    template<typename U, typename OtherMalloc> friend MallocSpan<U, OtherMalloc> adoptMallocSpan(std::span<U>);

    explicit MallocSpan(T* ptr, size_t sizeInBytes)
        : m_span(unsafeMakeSpan(ptr, sizeInBytes / sizeof(T)))
    {
        RELEASE_ASSERT(!(sizeInBytes % sizeof(T)));
    }

    std::span<T> m_span;
};

template<typename U, typename OtherMalloc> MallocSpan<U, OtherMalloc> adoptMallocSpan(std::span<U> span)
{
    return MallocSpan<U, OtherMalloc>(span.data(), span.size_bytes());
}

} // namespace WTF

using WTF::MallocSpan;
using WTF::adoptMallocSpan;
