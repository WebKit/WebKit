/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <pal/spi/cf/CoreAudioSPI.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemFree.h>
#include <wtf/SystemMalloc.h>

namespace PAL {

template<typename T>
inline std::span<const T> span(const AudioBuffer& buffer)
{
    return unsafeMakeSpan(static_cast<const T*>(buffer.mData), buffer.mDataByteSize / sizeof(T));
}

template<typename T>
inline std::span<T> mutableSpan(AudioBuffer& buffer)
{
    return unsafeMakeSpan(static_cast<T*>(buffer.mData), buffer.mDataByteSize / sizeof(T));
}

inline std::span<AudioBuffer> span(AudioBufferList& list)
{
    return unsafeMakeSpan(list.mBuffers, list.mNumberBuffers);
}

inline std::span<const AudioBuffer> span(const AudioBufferList& list)
{
    return unsafeMakeSpan(list.mBuffers, list.mNumberBuffers);
}

inline size_t allocationSize(const AudioBufferList& list)
{
    return offsetof(AudioBufferList, mBuffers) + sizeof(AudioBuffer) * std::max<uint32_t>(1U, list.mNumberBuffers);
}

// AudioBufferList is a variable-length struct, so create on the heap with a generic new() operator
// with a custom size, and initialize the struct manually.
enum class ShouldZeroMemory : bool { No, Yes };
inline std::unique_ptr<AudioBufferList, WTF::SystemFree<AudioBufferList>> createAudioBufferList(uint32_t bufferCount, ShouldZeroMemory shouldZeroMemory)
{
    CheckedSize bufferListSize = offsetof(AudioBufferList, mBuffers);
    bufferListSize += CheckedSize { sizeof(AudioBuffer) } * std::max<uint32_t>(1, bufferCount);
    auto bufferList = adoptSystemMalloc(shouldZeroMemory == ShouldZeroMemory::Yes
        ? SystemMallocBase<AudioBufferList>::zeroedMalloc(bufferListSize.value())
        : SystemMallocBase<AudioBufferList>::malloc(bufferListSize.value()));
    bufferList->mNumberBuffers = bufferCount;
    ASSERT(allocationSize(*bufferList) == bufferListSize.value());
    return bufferList;
}

} // namespace PAL

using PAL::mutableSpan;
using PAL::span;
