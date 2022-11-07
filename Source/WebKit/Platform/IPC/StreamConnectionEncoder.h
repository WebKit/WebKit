/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include "ArgumentCoders.h"
#include "MessageNames.h"
#include <wtf/StdLibExtras.h>

namespace IPC {

template<typename, typename> struct ArgumentCoder;

// IPC encoder which:
//  - Encodes to a caller buffer with fixed size, does not resize, stops when size runs out
//  - Does not initialize alignment gaps
//
class StreamConnectionEncoder final {
public:
    // Stream message needs to be at least size of StreamSetDestinationID message.
    static constexpr size_t minimumMessageSize = sizeof(MessageName) + sizeof(uint64_t);
    static constexpr size_t messageAlignment = alignof(MessageName);
    static constexpr bool isIPCEncoder = true;

    StreamConnectionEncoder(MessageName messageName, uint8_t* stream, size_t streamCapacity)
        : m_buffer(stream)
        , m_bufferCapacity(streamCapacity)
    {
        *this << messageName;
    }

    ~StreamConnectionEncoder() = default;

    bool encodeFixedLengthData(const uint8_t* data, size_t size, size_t alignment)
    {
        size_t bufferPointer = static_cast<size_t>(reinterpret_cast<intptr_t>(m_buffer + m_encodedSize));
        size_t newBufferPointer = roundUpToMultipleOf(alignment, bufferPointer);
        if (newBufferPointer < bufferPointer)
            return false;
        intptr_t alignedSize = m_encodedSize + (newBufferPointer - bufferPointer);
        if (!reserve(alignedSize, size))
            return false;
        uint8_t* buffer = m_buffer + alignedSize;
        memcpy(buffer, data, size);
        m_encodedSize = alignedSize + size;
        return true;
    }

    template<typename T>
    StreamConnectionEncoder& operator<<(T&& t)
    {
        ArgumentCoder<std::remove_cvref_t<T>, void>::encode(*this, std::forward<T>(t));
        return *this;
    }

    size_t size() const { ASSERT(isValid()); return m_encodedSize; }
    bool isValid() const { return m_bufferCapacity; }
    operator bool() const { return isValid(); }
private:
    bool reserve(size_t alignedSize, size_t additionalSize)
    {
        size_t size = alignedSize + additionalSize;
        if (size < alignedSize || size > m_bufferCapacity) {
            m_bufferCapacity = 0;
            return false;
        }
        return true;
    }
    uint8_t* m_buffer;
    size_t m_bufferCapacity;
    size_t m_encodedSize { 0 };
};

} // namespace IPC
