/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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
#include "CARingBuffer.h"

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)

#include "CAAudioStreamDescription.h"
#include "Logging.h"
#include <Accelerate/Accelerate.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

CARingBuffer::CARingBuffer(size_t bytesPerFrame, size_t frameCount, uint32_t numChannelStreams)
    : m_pointers(numChannelStreams)
    , m_channelCount(numChannelStreams)
    , m_bytesPerFrame(bytesPerFrame)
    , m_frameCount(frameCount)
    , m_capacityBytes(computeCapacityBytes(bytesPerFrame, frameCount))
{
    ASSERT(WTF::isPowerOfTwo(frameCount));
}

CARingBuffer::~CARingBuffer() = default;

CheckedSize CARingBuffer::computeCapacityBytes(size_t bytesPerFrame, size_t frameCount)
{
    return CheckedSize { bytesPerFrame } * frameCount;
}

CheckedSize CARingBuffer::computeSizeForBuffers(size_t bytesPerFrame, size_t frameCount, uint32_t numChannelStreams)
{
    return computeCapacityBytes(bytesPerFrame, frameCount) * numChannelStreams;
}

void CARingBuffer::initialize()
{
    Byte* channelData = static_cast<Byte*>(data());
    for (auto& pointer : m_pointers) {
        pointer = channelData;
        channelData += m_capacityBytes;
    }
}

static void ZeroRange(Vector<Byte*>& pointers, size_t offset, size_t nbytes)
{
    for (auto& pointer : pointers)
        memset(pointer + offset, 0, nbytes);
}

static void StoreABL(Vector<Byte*>& pointers, size_t destOffset, const AudioBufferList* list, size_t srcOffset, size_t nbytes)
{
    ASSERT(list->mNumberBuffers == pointers.size());
    const AudioBuffer* src = list->mBuffers;
    for (auto& pointer : pointers) {
        if (srcOffset > src->mDataByteSize)
            continue;
        memcpy(pointer + destOffset, static_cast<Byte*>(src->mData) + srcOffset, std::min<size_t>(nbytes, src->mDataByteSize - srcOffset));
        ++src;
    }
}

static void FetchABL(AudioBufferList* list, size_t destOffset, Vector<Byte*>& pointers, size_t srcOffset, size_t nbytesTarget, CARingBuffer::FetchMode mode, bool shouldUpdateListDataByteSize)
{
    ASSERT(list->mNumberBuffers == pointers.size());
    auto bufferCount = std::min<size_t>(list->mNumberBuffers, pointers.size());
    for (size_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex) {
        auto& pointer = pointers[bufferIndex];
        auto& dest = list->mBuffers[bufferIndex];

        if (destOffset > dest.mDataByteSize)
            continue;

        auto* destinationData = static_cast<Byte*>(dest.mData) + destOffset;
        auto* sourceData = pointer + srcOffset;
        auto nbytes = std::min<size_t>(nbytesTarget, dest.mDataByteSize - destOffset);
        switch (mode) {
        case CARingBuffer::Copy:
            memcpy(destinationData, sourceData, nbytes);
            break;
        case CARingBuffer::MixInt16: {
            auto* destination = reinterpret_cast<int16_t*>(destinationData);
            auto* source = reinterpret_cast<int16_t*>(sourceData);
            for (size_t i = 0; i < nbytes / sizeof(int16_t); i++)
                destination[i] += source[i];
            break;
        }
        case CARingBuffer::MixInt32: {
            auto* destination = reinterpret_cast<int32_t*>(destinationData);
            vDSP_vaddi(destination, 1, reinterpret_cast<int32_t*>(sourceData), 1, destination, 1, nbytes / sizeof(int32_t));
            break;
        }
        case CARingBuffer::MixFloat32: {
            auto* destination = reinterpret_cast<float*>(destinationData);
            vDSP_vadd(destination, 1, reinterpret_cast<float*>(sourceData), 1, destination, 1, nbytes / sizeof(float));
            break;
        }
        case CARingBuffer::MixFloat64: {
            auto* destination = reinterpret_cast<double*>(destinationData);
            vDSP_vaddD(destination, 1, reinterpret_cast<double*>(sourceData), 1, destination, 1, nbytes / sizeof(double));
            break;
        }
        }

        if (shouldUpdateListDataByteSize)
            dest.mDataByteSize = destOffset + nbytes;
    }
}

inline void ZeroABL(AudioBufferList* list, size_t destOffset, size_t nbytes)
{
    int nBuffers = list->mNumberBuffers;
    AudioBuffer* dest = list->mBuffers;
    while (--nBuffers >= 0) {
        if (destOffset > dest->mDataByteSize)
            continue;
        memset(static_cast<Byte*>(dest->mData) + destOffset, 0, std::min<size_t>(nbytes, dest->mDataByteSize - destOffset));
        ++dest;
    }
}

void CARingBuffer::setTimeBounds(TimeBounds bufferBounds)
{
    m_storeBounds = bufferBounds;

    auto& bounds = timeBoundsBuffer();
    unsigned index = (bounds.index.load(std::memory_order_acquire) + 1) % boundsBufferSize;
    bounds.buffer[index] = bufferBounds;
    bounds.index.store(index, std::memory_order_release);
}

CARingBuffer::TimeBounds CARingBuffer::getStoreTimeBounds()
{
    return m_storeBounds;
}

CARingBuffer::Error CARingBuffer::store(const AudioBufferList* list, size_t framesToWrite, uint64_t startFrame)
{
    if (!framesToWrite)
        return Ok;

    if (framesToWrite > m_frameCount)
        return TooMuch;

    uint64_t endFrame = startFrame + framesToWrite;

    if (startFrame < m_storeBounds.startFrame) {
        // Throw everything out when going backwards.
        setTimeBounds({ startFrame, startFrame });
    } else if (endFrame - m_storeBounds.startFrame <= m_frameCount) {
        // The buffer has not yet wrapped and will not need to.
        // No-op.
    } else {
        // Advance the start time past the region we are about to overwrite
        // starting one buffer of time behind where we're writing.
        uint64_t newStartFrame = endFrame - m_frameCount;
        uint64_t newEndFrame = std::max(newStartFrame, m_storeBounds.endFrame);
        setTimeBounds({ newStartFrame, newEndFrame });
    }

    // Write the new frames.
    size_t offset0;
    size_t offset1;

    if (startFrame > m_storeBounds.endFrame) {
        // We are skipping some samples, so zero the range we are skipping.
        offset0 = frameOffset(m_storeBounds.endFrame);
        offset1 = frameOffset(startFrame);
        if (offset0 < offset1)
            ZeroRange(m_pointers, offset0, offset1 - offset0);
        else {
            ZeroRange(m_pointers, offset0, m_capacityBytes - offset0);
            ZeroRange(m_pointers, 0, offset1);
        }
        offset0 = offset1;
    } else
        offset0 = frameOffset(startFrame);

    offset1 = frameOffset(endFrame);
    if (offset0 < offset1)
        StoreABL(m_pointers, offset0, list, 0, offset1 - offset0);
    else {
        size_t nbytes = m_capacityBytes - offset0;
        StoreABL(m_pointers, offset0, list, 0, nbytes);
        StoreABL(m_pointers, 0, list, nbytes, offset1);
    }

    // Now update the end time.
    setTimeBounds({ m_storeBounds.startFrame, endFrame });

    return Ok;
}

CARingBuffer::TimeBounds CARingBuffer::getFetchTimeBounds()
{
    auto& bounds = timeBoundsBuffer();
    unsigned index = bounds.index.load(std::memory_order_acquire);
    if (UNLIKELY(index >= boundsBufferSize)) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto fetchBounds = bounds.buffer[index];
    if (fetchBounds.endFrame - fetchBounds.startFrame > m_frameCount) {
        ASSERT_NOT_REACHED();
        return { };
    }
    return fetchBounds;
}

static CARingBuffer::TimeBounds clamp(CARingBuffer::TimeBounds value, CARingBuffer::TimeBounds limit)
{
    return {
        std::clamp(value.startFrame, limit.startFrame, limit.endFrame),
        std::clamp(value.endFrame, limit.startFrame, limit.endFrame)
    };
}

bool CARingBuffer::fetchIfHasEnoughData(AudioBufferList* list, size_t frameCount, uint64_t startRead, FetchMode mode)
{
    auto bufferBounds = getFetchTimeBounds();
    if (startRead < bufferBounds.startFrame || startRead + frameCount > bufferBounds.endFrame)
        return false;
    fetchInternal(list, frameCount, startRead, mode, bufferBounds);
    return true;
}

void CARingBuffer::fetch(AudioBufferList* list, size_t frameCount, uint64_t startRead, FetchMode mode)
{
    fetchInternal(list, frameCount, startRead, mode, getFetchTimeBounds());
}

void CARingBuffer::fetchInternal(AudioBufferList* list, size_t nFrames, uint64_t startRead, FetchMode mode, TimeBounds bufferBounds)
{
    if (!nFrames)
        return;

    uint64_t endRead = startRead + nFrames;
    
    uint64_t startRead0 = startRead;
    uint64_t endRead0 = endRead;
    {
        auto fetchBounds = clamp({ startRead, endRead }, bufferBounds);
        startRead = fetchBounds.startFrame;
        endRead = fetchBounds.endFrame;
    }

    if (startRead == endRead) {
        ZeroABL(list, 0, nFrames * m_bytesPerFrame);
        return;
    }
    
    size_t byteSize = static_cast<size_t>((endRead - startRead) * m_bytesPerFrame);
    
    size_t destStartByteOffset = static_cast<size_t>(std::max<uint64_t>(0, (startRead - startRead0) * m_bytesPerFrame));
    
    if (destStartByteOffset > 0)
        ZeroABL(list, 0, std::min<size_t>(nFrames * m_bytesPerFrame, destStartByteOffset));

    size_t destEndSize = static_cast<size_t>(std::max<uint64_t>(0, endRead0 - endRead));
    if (destEndSize > 0)
        ZeroABL(list, destStartByteOffset + byteSize, destEndSize * m_bytesPerFrame);

    size_t offset0 = frameOffset(startRead);
    size_t offset1 = frameOffset(endRead);
    size_t nbytes;
    
    if (offset0 < offset1) {
        nbytes = offset1 - offset0;
        FetchABL(list, destStartByteOffset, m_pointers, offset0, nbytes, mode, true);
    } else {
        nbytes = m_capacityBytes - offset0;
        FetchABL(list, destStartByteOffset, m_pointers, offset0, nbytes, mode, !offset1);
        if (offset1)
            FetchABL(list, destStartByteOffset + nbytes, m_pointers, 0, offset1, mode, true);
        nbytes += offset1;
    }
}

std::unique_ptr<InProcessCARingBuffer> InProcessCARingBuffer::allocate(const WebCore::CAAudioStreamDescription& format, size_t frameCount)
{
    frameCount = WTF::roundUpToPowerOfTwo(frameCount);
    auto bytesPerFrame = format.bytesPerFrame();
    auto numChannelStreams = format.numberOfChannelStreams();

    auto checkedSizeForBuffers = computeSizeForBuffers(bytesPerFrame, frameCount, numChannelStreams);
    if (checkedSizeForBuffers.hasOverflowed()) {
        RELEASE_LOG_FAULT(Media, "InProcessCARingBuffer::allocate: Overflowed when trying to compute the storage size");
        return nullptr;
    }
    auto sizeForBuffers = checkedSizeForBuffers.value();
    Vector<uint8_t> buffer;
    if (!buffer.tryReserveCapacity(sizeForBuffers)) {
        RELEASE_LOG_FAULT(Media, "InProcessCARingBuffer::allocate: Failed to allocate buffer of the requested size: %lu", sizeForBuffers);
        return nullptr;
    }
    buffer.grow(sizeForBuffers);
    std::unique_ptr<InProcessCARingBuffer> result { new InProcessCARingBuffer { bytesPerFrame, frameCount, numChannelStreams, WTFMove(buffer) } };
    result->initialize();
    return result;
}

InProcessCARingBuffer::InProcessCARingBuffer(size_t bytesPerFrame, size_t frameCount, uint32_t numChannelStreams, Vector<uint8_t>&& buffer)
    : CARingBuffer(bytesPerFrame, frameCount, numChannelStreams)
    , m_buffer(WTFMove(buffer))
{
}

InProcessCARingBuffer::~InProcessCARingBuffer() = default;

}

#endif // ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)
