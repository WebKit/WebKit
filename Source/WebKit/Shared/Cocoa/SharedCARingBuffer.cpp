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

#include "config.h"

#if USE(MEDIATOOLBOX)
#include "SharedCARingBuffer.h"

#include <WebCore/CARingBuffer.h>

namespace WebKit {

SharedCARingBufferBase::SharedCARingBufferBase(size_t bytesPerFrame, size_t frameCount, uint32_t numChannelStream, Ref<SharedMemory> storage)
    : CARingBuffer(bytesPerFrame, frameCount, numChannelStream)
    , m_storage(WTFMove(storage))
{
}

void* SharedCARingBufferBase::data()
{
    return static_cast<Byte*>(m_storage->data()) + sizeof(FrameBounds);
}

auto SharedCARingBufferBase::sharedFrameBounds() const -> FrameBounds&
{
    return *reinterpret_cast<FrameBounds*>(m_storage->data());
}

void SharedCARingBufferBase::getCurrentFrameBoundsWithoutUpdate(uint64_t& startFrame, uint64_t& endFrame)
{
    startFrame = m_startFrame;
    endFrame = m_endFrame;
}

void SharedCARingBufferBase::flush()
{
    m_startFrame = 0;
    m_endFrame = 0;
}

void SharedCARingBufferBase::updateFrameBounds()
{
    auto& sharedBounds = sharedFrameBounds();
    unsigned boundsBufferIndex = sharedBounds.boundsBufferIndex.load(std::memory_order_acquire);
    if (UNLIKELY(boundsBufferIndex >= boundsBufferSize)) {
        m_startFrame = 0;
        m_endFrame = 0;
        return;
    }
    auto pair = sharedBounds.boundsBuffer[boundsBufferIndex];
    m_startFrame = pair.first;
    m_endFrame = pair.second;
}

size_t SharedCARingBufferBase::size() const
{
    return m_storage->size() - sizeof(FrameBounds);
}

std::unique_ptr<ConsumerSharedCARingBuffer> ConsumerSharedCARingBuffer::map(const WebCore::CAAudioStreamDescription& format, size_t frameCount, ConsumerSharedCARingBuffer::Handle&& handle)
{
    frameCount = WTF::roundUpToPowerOfTwo(frameCount);
    auto bytesPerFrame = format.bytesPerFrame();
    auto numChannelStreams = format.numberOfChannelStreams();

    // Validate the parameters as they may be coming from an untrusted process.
    auto expectedStorageSize = computeSizeForBuffers(bytesPerFrame, frameCount, numChannelStreams) + sizeof(FrameBounds);
    if (expectedStorageSize.hasOverflowed()) {
        RELEASE_LOG_FAULT(Media, "ConsumerSharedCARingBuffer::map: Overflowed when trying to compute the storage size");
        return nullptr;
    }
    auto storage = SharedMemory::map(WTFMove(handle), SharedMemory::Protection::ReadOnly);
    if (!storage) {
        RELEASE_LOG_FAULT(Media, "ConsumerSharedCARingBuffer::map: Failed to map memory");
        return nullptr;
    }
    if (storage->size() < expectedStorageSize) {
        RELEASE_LOG_FAULT(Media, "ConsumerSharedCARingBuffer::map: Storage size is insufficient for format and frameCount");
        return nullptr;
    }

    std::unique_ptr<ConsumerSharedCARingBuffer> result { new ConsumerSharedCARingBuffer { bytesPerFrame, frameCount, numChannelStreams, storage.releaseNonNull() } };
    result->initialize();
    return result;
}

ProducerSharedCARingBuffer::Pair ProducerSharedCARingBuffer::allocate(const WebCore::CAAudioStreamDescription& format, size_t frameCount)
{
    frameCount = WTF::roundUpToPowerOfTwo(frameCount);
    auto bytesPerFrame = format.bytesPerFrame();
    auto numChannelStreams = format.numberOfChannelStreams();

    auto checkedSizeForBuffers = computeSizeForBuffers(bytesPerFrame, frameCount, numChannelStreams);
    if (checkedSizeForBuffers.hasOverflowed()) {
        RELEASE_LOG_FAULT(Media, "ProducerSharedCARingBuffer::allocate: Overflowed when trying to compute the storage size");
        return { nullptr, { } };
    }
    auto sharedMemory = SharedMemory::allocate(sizeof(FrameBounds) + checkedSizeForBuffers.value());
    if (!sharedMemory)
        return { nullptr, { } };

    ConsumerSharedCARingBuffer::Handle handle;
    if (!sharedMemory->createHandle(handle, SharedMemory::Protection::ReadOnly))
        return { nullptr, { } };

    new (NotNull, sharedMemory->data()) FrameBounds;
    std::unique_ptr<ProducerSharedCARingBuffer> result { new ProducerSharedCARingBuffer { bytesPerFrame, frameCount, numChannelStreams, sharedMemory.releaseNonNull() } };
    result->initialize();
    return { WTFMove(result), WTFMove(handle) };
}

void ProducerSharedCARingBuffer::setCurrentFrameBounds(uint64_t startFrame, uint64_t endFrame)
{
    m_startFrame = startFrame;
    m_endFrame = endFrame;

    auto& sharedBounds = sharedFrameBounds();
    unsigned indexToWrite = (sharedBounds.boundsBufferIndex.load(std::memory_order_acquire) + 1) % boundsBufferSize;
    sharedBounds.boundsBuffer[indexToWrite] = std::make_pair(startFrame, endFrame);
    sharedBounds.boundsBufferIndex.store(indexToWrite, std::memory_order_release);
}

} // namespace WebKit

#endif
