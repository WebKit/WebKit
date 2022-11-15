/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if USE(MEDIATOOLBOX)

#include "SharedMemory.h"
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/CARingBuffer.h>
#include <wtf/Atomics.h>
#include <wtf/Function.h>

namespace WebKit {

class SharedCARingBufferBase : public WebCore::CARingBuffer {
protected:
    SharedCARingBufferBase(size_t bytesPerFrame, size_t frameCount, uint32_t numChannelStream, Ref<SharedMemory>);
    void* data() final { return static_cast<Byte*>(m_storage->data()) + sizeof(TimeBoundsBuffer); }
    TimeBoundsBuffer& timeBoundsBuffer() final { return *reinterpret_cast<TimeBoundsBuffer*>(m_storage->data()); }

    Ref<SharedMemory> m_storage;
};

class ConsumerSharedCARingBuffer final : public SharedCARingBufferBase {
public:
    struct Handle {
        SharedMemory::Handle memory;
        size_t frameCount;
        void takeOwnershipOfMemory(MemoryLedger ledger) { memory.takeOwnershipOfMemory(ledger); }
        template <typename Encoder> void encode(Encoder&) const;
        template <typename Decoder> static std::optional<Handle> decode(Decoder&);
    };
    // FIXME: Remove this deprecated constructor.
    static std::unique_ptr<ConsumerSharedCARingBuffer> map(const WebCore::CAAudioStreamDescription& format, Handle&& handle)
    {
        return map(format.bytesPerFrame(), format.numberOfChannelStreams(), WTFMove(handle));
    }
    static std::unique_ptr<ConsumerSharedCARingBuffer> map(uint32_t bytesPerFrame, uint32_t numChannelStreams, Handle&&);
protected:
    using SharedCARingBufferBase::SharedCARingBufferBase;
};

class ProducerSharedCARingBuffer final : public SharedCARingBufferBase {
public:
    struct Pair {
        std::unique_ptr<ProducerSharedCARingBuffer> producer;
        ConsumerSharedCARingBuffer::Handle consumer;
    };
    static Pair allocate(const WebCore::CAAudioStreamDescription& format, size_t frameCount);
protected:
    using SharedCARingBufferBase::SharedCARingBufferBase;
};

template <typename Encoder>
void ConsumerSharedCARingBuffer::Handle::encode(Encoder& encoder) const
{
    encoder << memory << frameCount;
}

template <typename Decoder>
std::optional<ConsumerSharedCARingBuffer::Handle> ConsumerSharedCARingBuffer::Handle::decode(Decoder& decoder)
{
    auto memory = decoder.template decode<SharedMemory::Handle>();
    auto frameCount = decoder.template decode<size_t>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    return Handle { WTFMove(*memory), *frameCount };
}

}

#endif
