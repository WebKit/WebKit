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
#include "SpanCoreAudio.h"
#include "VectorMath.h"
#include <Accelerate/Accelerate.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ZippedRange.h>
#include <wtf/text/ParsingUtilities.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CARingBuffer);
WTF_MAKE_TZONE_ALLOCATED_IMPL(InProcessCARingBuffer);

CARingBuffer::CARingBuffer(size_t bytesPerFrame, size_t frameCount, uint32_t numChannelStreams)
    : m_channels(numChannelStreams)
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
    auto channelData = span();
    for (auto& channel : m_channels)
        channel = consumeSpan(channelData, m_capacityBytes);
}

static void ZeroRange(Vector<std::span<Byte>>& channels, size_t offset, size_t nbytes)
{
    for (auto& channel : channels)
        zeroSpan(channel.subspan(offset, nbytes));
}

static void StoreABL(Vector<std::span<Byte>>& channels, size_t destOffset, const AudioBufferList* list, size_t srcOffset, size_t nbytes)
{
    ASSERT(list->mNumberBuffers == channels.size());
    auto buffers = span(*list);
    const AudioBuffer* src = list->mBuffers;
    for (auto& channel : channels) {
        if (srcOffset > buffers[0].mDataByteSize)
            continue;
        auto srcData = span<Byte>(buffers[0]);
        memcpySpan(channel.subspan(destOffset), srcData.subspan(srcOffset, std::min<size_t>(nbytes, src->mDataByteSize - srcOffset)));
        skip(buffers, 1);
    }
}

static void FetchABL(AudioBufferList* list, size_t destOffset, Vector<std::span<Byte>>& channels, size_t srcOffset, size_t nbytesTarget, CARingBuffer::FetchMode mode, bool shouldUpdateListDataByteSize)
{
    ASSERT(list->mNumberBuffers == channels.size());
    auto buffers = span(*list);
    auto bufferCount = std::min<size_t>(list->mNumberBuffers, channels.size());
    for (size_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex) {
        auto& channel = channels[bufferIndex];
        auto& dest = buffers[bufferIndex];

        if (destOffset > dest.mDataByteSize)
            continue;

        auto destinationData = mutableSpan<uint8_t>(dest).subspan(destOffset);
        auto sourceData = channel.subspan(srcOffset);
        auto nbytes = std::min<size_t>(nbytesTarget, dest.mDataByteSize - destOffset);
        switch (mode) {
        case CARingBuffer::Copy:
            memcpySpan(destinationData, sourceData.first(nbytes));
            break;
        case CARingBuffer::MixInt16: {
            size_t frameCount = nbytes / sizeof(int16_t);
            auto source = spanReinterpretCast<const int16_t>(sourceData).first(frameCount);
            auto destination = spanReinterpretCast<int16_t>(destinationData).first(frameCount);
            for (auto [s, d] : zippedRange(source, destination))
                d += s;
            break;
        }
        case CARingBuffer::MixInt32: {
            size_t frameCount = nbytes / sizeof(int32_t);
            auto source = spanReinterpretCast<const int32_t>(sourceData).first(frameCount);
            auto destination = spanReinterpretCast<int32_t>(destinationData).first(frameCount);
            VectorMath::add(destination, source, destination);
            break;
        }
        case CARingBuffer::MixFloat32: {
            size_t frameCount = nbytes / sizeof(float);
            auto source = spanReinterpretCast<const float>(sourceData).first(frameCount);
            auto destination = spanReinterpretCast<float>(destinationData).first(frameCount);
            VectorMath::add(destination, source, destination);
            break;
        }
        case CARingBuffer::MixFloat64: {
            size_t frameCount = nbytes / sizeof(double);
            auto source = spanReinterpretCast<const double>(sourceData).first(frameCount);
            auto destination = spanReinterpretCast<double>(destinationData).first(frameCount);
            VectorMath::add(destination, source, destination);
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
    auto destinations = span(*list);
    while (--nBuffers >= 0) {
        if (destOffset > destinations[0].mDataByteSize)
            continue;
        auto dataSpan = mutableSpan<uint8_t>(destinations[0]).subspan(destOffset);
        if (nbytes < dataSpan.size())
            dataSpan = dataSpan.first(nbytes);
        zeroSpan(dataSpan);
        skip(destinations, 1);
    }
}

void CARingBuffer::setTimeBounds(TimeBounds bufferBounds)
{
    auto& bounds = timeBoundsBuffer();
    bounds.store(bufferBounds);
    m_storeBounds = bufferBounds;
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
            ZeroRange(m_channels, offset0, offset1 - offset0);
        else {
            ZeroRange(m_channels, offset0, m_capacityBytes - offset0);
            ZeroRange(m_channels, 0, offset1);
        }
        offset0 = offset1;
    } else
        offset0 = frameOffset(startFrame);

    offset1 = frameOffset(endFrame);
    if (offset0 < offset1)
        StoreABL(m_channels, offset0, list, 0, offset1 - offset0);
    else {
        size_t nbytes = m_capacityBytes - offset0;
        StoreABL(m_channels, offset0, list, 0, nbytes);
        StoreABL(m_channels, 0, list, nbytes, offset1);
    }

    // Now update the end time.
    setTimeBounds({ m_storeBounds.startFrame, endFrame });

    return Ok;
}

CARingBuffer::TimeBounds CARingBuffer::getFetchTimeBounds()
{
    auto bounds = timeBoundsBuffer().load();
    bounds.startFrame = std::min(bounds.startFrame, std::numeric_limits<uint64_t>::max() - m_frameCount);
    bounds.endFrame = std::clamp(bounds.endFrame, bounds.startFrame, bounds.startFrame + m_frameCount);
    return bounds;
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
        FetchABL(list, destStartByteOffset, m_channels, offset0, nbytes, mode, true);
    } else {
        nbytes = m_capacityBytes - offset0;
        FetchABL(list, destStartByteOffset, m_channels, offset0, nbytes, mode, !offset1);
        if (offset1)
            FetchABL(list, destStartByteOffset + nbytes, m_channels, 0, offset1, mode, true);
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
