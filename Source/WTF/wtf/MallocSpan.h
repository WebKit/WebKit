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
#include <wtf/AllocSpanMixin.h>
#include <wtf/FastMalloc.h>
#include <wtf/MallocCommon.h>
#include <wtf/SystemMalloc.h>
#include <wtf/TypeTraits.h>

// MallocSpan is a smart pointer class that wraps a std::span and calls fastFree in its destructor.

namespace WTF {

template<typename T, typename Malloc = FastMalloc> class MallocSpan : public AllocSpanMixin<T> {
    WTF_MAKE_CONFIGURABLE_ALLOCATED(Malloc);
public:
    MallocSpan() = default;

    MallocSpan(MallocSpan&& other)
        : AllocSpanMixin<T>(WTFMove(other))
    {
    }

    template<typename U>
    MallocSpan(MallocSpan<U, Malloc>&& other) requires (std::is_same_v<T, uint8_t>)
        : AllocSpanMixin<T>(asWritableBytes(other.leakSpan()))
    {
    }

    ~MallocSpan()
    {
        auto span = this->mutableSpan();
        if constexpr (parameterCount(Malloc::free) == 2)
            Malloc::free(span.data(), span.size());
        else
            Malloc::free(span.data());
    }

    MallocSpan& operator=(MallocSpan&& other)
    {
        MallocSpan ptr { WTFMove(other) };
        this->swap(ptr);
        return *this;
    }

    static MallocSpan malloc(size_t sizeInBytes)
    {
        return MallocSpan { Malloc::malloc(sizeInBytes), sizeInBytes };
    }

    static MallocSpan zeroedMalloc(size_t sizeInBytes)
    {
        return MallocSpan { Malloc::zeroedMalloc(sizeInBytes), sizeInBytes };
    }

    static MallocSpan alignedMalloc(size_t alignment, size_t sizeInBytes)
    {
        return MallocSpan { Malloc::alignedMalloc(alignment, sizeInBytes), sizeInBytes };
    }

    static MallocSpan tryAlloc(size_t sizeInBytes)
    {
        return tryMalloc(sizeInBytes);
    }

    static MallocSpan tryMalloc(size_t sizeInBytes)
    {
        auto* ptr = Malloc::tryMalloc(sizeInBytes);
        if (!ptr)
            return { };
        return MallocSpan { ptr, sizeInBytes };
    }

    static MallocSpan tryZeroedMalloc(size_t sizeInBytes)
    {
        auto* ptr = Malloc::tryZeroedMalloc(sizeInBytes);
        if (!ptr)
            return { };
        return MallocSpan { ptr, sizeInBytes };
    }

    static MallocSpan tryAlignedMalloc(size_t alignment, size_t sizeInBytes)
    {
        auto* ptr = Malloc::tryAlignedMalloc(alignment, sizeInBytes);
        if (!ptr)
            return { };
        return MallocSpan { ptr, sizeInBytes };
    }

    void realloc(size_t newSizeInBytes)
    {
        RELEASE_ASSERT(!(newSizeInBytes % sizeof(T)));
        MallocSpan other { Malloc::realloc(this->leakSpan().data(), newSizeInBytes), newSizeInBytes };
        this->swap(other);
    }

private:
    using AllocSpanMixin<T>::AllocSpanMixin;

    template<typename U, typename OtherMalloc> friend MallocSpan<U, OtherMalloc> adoptMallocSpan(std::span<U>);
};

// Specialization for SystemMalloc to access <typename T>.
template<typename T> class MallocSpan<T, SystemMalloc> : public MallocSpan<T, SystemMallocBase<T>> {
public:
    using Base = MallocSpan<T, SystemMallocBase<T>>;
    using Base::Base;

    MallocSpan() = default;
    MallocSpan(MallocSpan&&) = default;
    MallocSpan& operator=(MallocSpan&&) = default;

    MallocSpan& operator=(Base&& other)
    {
        Base::operator=(WTFMove(other));
        return *this;
    }
};

template<typename U, typename OtherMalloc> MallocSpan<U, OtherMalloc> adoptMallocSpan(std::span<U> span)
{
    return MallocSpan<U, OtherMalloc>(span.data(), span.size_bytes());
}

} // namespace WTF

using WTF::MallocSpan;
using WTF::adoptMallocSpan;
