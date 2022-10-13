/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "SharedMemory.h"
#include <wtf/Atomics.h>
#include <wtf/Ref.h>
#include <wtf/Span.h>

namespace IPC {
class Decoder;
class Encoder;

// StreamConnectionBuffer is a shared "bi-partite" circular buffer supporting variable length messages, specific data
// alignment with mandated minimum size. StreamClientConnection and StreamServerConnection use StreamConnectionBuffer to
// communicate.
//
// The "bi-partite" is here to mean that writes and reads to the buffer must be continuous.
//
// The caller (client or server) "acquires" some amount of buffer data. The data pointer location depends on the amount
// of data being acquired. The buffer returns all the available capacity -- this may be less than acquired.
//
// The caller "releases" the some amount of data, at least the amount acquired but potentially more.
//
// Both the client and the server must release the same amount of data at the same step of the acquire/release sequence.
// Suppose the client releases first 8 and then 166 bytes. Upon the first acquire, the server must also release 8 bytes
// and then upon the second acquire 166 bytes. Due to how alignment and minimum length affect the position at which the
// memory can be referred to, the server cannot "read two times simultaneously" and release 174 bytes.
//
// The buffer also supports synchronous replies via "releaseAll/acquireAll" call pattern. The communication protocol can
// establish cases where the client transfers ownership of the data to the server. In these cases the server will
// acknowledge the read with "releaseAll" operation. The client will then read the reply via "acquireAll" operation. The
// reply will be written to the beginning of the buffer. This mandates that in-place data references in the buffer
// cannot be used simultaneously for message data and the reply data. In current IPC implementation, the message
// processing uses the in-place data references, while the reply data is first constructed to allocated memory and then
// copied to the message buffer.
//
// The circular buffer has following implementation:
// * The client owns the data between [clientOffset, serverOffset[. When clientOffset == serverOffset, the client owns
//   all the data.
// * The server owns the data between [serverOffset, clientOffset[.
// * The exception to the above is when communication protocol can contain messages that denote that the server will own
//   all the data.
// * The buffer can hold maximum of size - 1 values. The last value is reserved for indicating that the buffer is full.
//   FIXME: Maybe would be simpler implementation if it would use the "wrap" flag instead of the hole as the indicator.
//   This would move the alignedSpan implementation to the StreamConnectionBuffer.
// * All atomic variable loads are untrusted, so they're clamped. Violations are not reported, though.
class StreamConnectionBuffer {
public:
    explicit StreamConnectionBuffer(size_t memorySize);
    StreamConnectionBuffer(StreamConnectionBuffer&&);
    ~StreamConnectionBuffer();
    StreamConnectionBuffer& operator=(StreamConnectionBuffer&&);

    size_t wrapOffset(size_t offset) const
    {
        ASSERT(offset <= dataSize());
        if (offset >= dataSize())
            return 0;
        return offset;
    }

    template<size_t messageAlignment>
    size_t alignOffset(size_t offset, size_t acquireSize) const
    {
        offset = roundUpToMultipleOf<messageAlignment>(offset);
        if (offset + acquireSize >= dataSize())
            offset = 0;
        return offset;
    }

    // Value denoting the client index to the buffer, with special tag values.
    // The client trusts this. The server converts the value to trusted size_t offset with a special function.
    enum ClientOffset : size_t {
        // Tag value stored in when the server is sleeping, e.g. not running.
        serverIsSleepingTag = 1u << 31
    };

    // Value denoting the server index to the buffer, with special tag values.
    // The client trusts this. The server converts the value to trusted size_t offset with a special function.
    enum ServerOffset : size_t {
        // Tag value stored when the client is waiting, e.g. waiting on a semaphore.
        clientIsWaitingTag = 1u << 31
    };

    Atomic<ClientOffset>& clientOffset() { return header().clientOffset; }
    Atomic<ServerOffset>& serverOffset() { return header().serverOffset; }
    uint8_t* data() const { return static_cast<uint8_t*>(m_sharedMemory->data()) + headerSize(); }
    size_t dataSize() const { return m_dataSize; }

    static constexpr size_t maximumSize() { return std::min(static_cast<size_t>(ClientOffset::serverIsSleepingTag), static_cast<size_t>(ClientOffset::serverIsSleepingTag)) - 1; }
    void encode(Encoder&) const;
    static std::optional<StreamConnectionBuffer> decode(Decoder&);

    Span<uint8_t> headerForTesting();
    Span<uint8_t> dataForTesting();

private:
    StreamConnectionBuffer(Ref<WebKit::SharedMemory>&&);

    struct Header {
        Atomic<ServerOffset> serverOffset;
        // Padding so that the variables mostly accessed by different processes do not share a cache line.
        // This is an attempt to avoid cache-line induced reduction of parallel access.
        // Use 128 bytes since that's the cache line size on ARM64, and enough to cover other platforms where 64 bytes is common.
        alignas(128) Atomic<ClientOffset> clientOffset;
    };

#undef HEADER_POINTER_ALIGNMENT

    Header& header() const { return *reinterpret_cast<Header*>(m_sharedMemory->data()); }
    static constexpr size_t headerSize() { return roundUpToMultipleOf<alignof(std::max_align_t)>(sizeof(Header)); }

    static constexpr bool sharedMemorySizeIsValid(size_t size) { return headerSize() < size && size <= headerSize() + maximumSize(); }

    size_t m_dataSize { 0 };
    Ref<WebKit::SharedMemory> m_sharedMemory;
};

}
