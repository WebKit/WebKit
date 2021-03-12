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

#include "Decoder.h"
#include "IPCSemaphore.h"
#include "SharedMemory.h"
#include <wtf/Atomics.h>
#include <wtf/Optional.h>

namespace IPC {

// StreamConnectionBuffer is a shared "bi-partite" circular buffer supporting
// variable length messages, specific data alignment with mandated minimum size.
// StreamClientConnection and StreamServerConnection use StreamConnectionBuffer to communicate.
//
// The "bi-partite" is here to mean that writes and reads to the buffer must be
// continuous.
//
// The client (sender or receiver) "acquires" some amount of buffer data. The data pointer
// location depends on the amount of data being acquired. The buffer returns all the available
// capacity -- this may be less than acquired.
//
// The client "releases" the some amount of data, at least the amount acquired but potentially
// more.
//
// Both sender and receiver must release the same amount of data at the same step of the acquire/release sequence.
// Suppose sender releases first 8 and then 166 bytes. Upon the first acquire, receiver
// must also release 8 bytes and then upon the second acquire 166 bytes.
// Due to how alignment and minimum length affect the position at which the memory can
// be referred to, the receiver cannot "read two times simultaneously" and release 174 bytes.
//
// The circular buffer has following implementation:
// * Sender owns the data between [senderOffset, receiverOffset[. When senderOffset ==
// receiverOffset, sender owns all the data.
// * Receiver owns the data between [receiverOffset, senderOffset[.
// * The buffer can hold maximum of size - 1 values. The last value is reserved for
// indicating that the buffer is full. FIXME: Maybe would be simpler implementation if
// it would use the "wrap" flag instead of the hole as the indicator. This would move
// the alignedSpan implementation to the StreamConnectionBuffer.
// * All atomic variable loads are untrusted, so they're clamped. Violations are not reported,
// though.
// See SharedDisplayListHandle.
class StreamConnectionBuffer {
public:
    explicit StreamConnectionBuffer(size_t memorySize);
    StreamConnectionBuffer(StreamConnectionBuffer&&);
    ~StreamConnectionBuffer();
    StreamConnectionBuffer& operator=(StreamConnectionBuffer&&);

    size_t wrapOffset(size_t offset) const
    {
        ASSERT(offset <= dataSize());
        if (offset == dataSize())
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

    Atomic<size_t>& senderOffset() { return header().senderOffset; }
    Atomic<size_t>& receiverOffset() { return header().receiverOffset; }
    uint8_t* data() const { return static_cast<uint8_t*>(m_sharedMemory->data()) + headerSize(); }
    size_t dataSize() const { return m_dataSize; }
    Semaphore& senderWaitSemaphore() { return m_senderWaitSemaphore; }

    // Tag value stored in Header::senderOffset when receiver is sleeping, e.g. not running.
    static constexpr size_t senderOffsetReceiverIsSleepingTag = 1 << 31;
    // Tag value stored in Header::receiverOffset when sender is waiting, e.g. waiting on
    // a semaphore.
    static constexpr size_t receiverOffsetSenderIsWaitingTag = 1 << 31;

    static constexpr size_t maximumSize() { return std::min(senderOffsetReceiverIsSleepingTag, senderOffsetReceiverIsSleepingTag) - 1; }
    void encode(Encoder&) const;
    static Optional<StreamConnectionBuffer> decode(Decoder&);

private:
    StreamConnectionBuffer(Ref<WebKit::SharedMemory>&&, size_t memorySize, Semaphore&& senderWaitSemaphore);

    struct Header {
        Atomic<size_t> receiverOffset;
        // Padding so that the variables mostly accessed by different processes do not share a cache line.
        // This is an attempt to avoid cache-line induced reduction of parallel access.
        alignas(sizeof(uint64_t[2])) Atomic<size_t> senderOffset;
    };
    Header& header() const { return *reinterpret_cast<Header*>(m_sharedMemory->data()); }
    static constexpr size_t headerSize() { return roundUpToMultipleOf<alignof(std::max_align_t)>(sizeof(Header)); }

    size_t m_dataSize { 0 };
    Ref<WebKit::SharedMemory> m_sharedMemory;
    Semaphore m_senderWaitSemaphore;
};

}
