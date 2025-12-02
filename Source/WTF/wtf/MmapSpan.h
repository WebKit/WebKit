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

#if HAVE(MMAP)

#include <sys/mman.h>
#include <wtf/AllocSpanMixin.h>

namespace WTF {

// RAII class for allocating and holding mmap memory with std::span access.
template<typename T> class MmapSpan : public AllocSpanMixin<T> {
public:
    static MmapSpan tryAlloc(size_t sizeInBytes)
    {
        return mmap(nullptr, sizeInBytes, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1);
    }

    static MmapSpan mmap(void* addr, size_t size, int pageProtection, int options, int fileDescriptor)
    {
        auto* data = ::mmap(addr, size, pageProtection, options, fileDescriptor, 0);
        if (data == MAP_FAILED)
            return { };
        RELEASE_ASSERT((pageProtection & PROT_EXEC) || WTF_DATA_ADDRESS_IS_SANE(data));
        return MmapSpan { data, size };
    }

    MmapSpan() = default;
    MmapSpan(MmapSpan&& other)
        : AllocSpanMixin<T>(WTFMove(other))
    {
    }

    template<typename U>
    MmapSpan(MmapSpan<U>&& other) requires (std::is_same_v<T, uint8_t>)
        : AllocSpanMixin<T>(asWritableBytes(other.leakSpan()))
    {
    }

    ~MmapSpan()
    {
        auto data = this->mutableSpan();
        if (data.data())
            ::munmap(data.data(), data.size());
    }

    MmapSpan& operator=(MmapSpan&& other)
    {
        MmapSpan ptr { WTFMove(other) };
        this->swap(ptr);
        return *this;
    }

private:
    using AllocSpanMixin<T>::AllocSpanMixin;
};

} // namespace WTF

using WTF::MmapSpan;

#endif // HAVE(MMAP)
