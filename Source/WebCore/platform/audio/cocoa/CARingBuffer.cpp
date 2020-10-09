/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include <Accelerate/Accelerate.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <wtf/MathExtras.h>

const uint32_t kGeneralRingTimeBoundsQueueSize = 32;
const uint32_t kGeneralRingTimeBoundsQueueMask = kGeneralRingTimeBoundsQueueSize - 1;

namespace WebCore {

CARingBuffer::CARingBuffer()
    : m_buffers(makeUniqueRef<CARingBufferStorageVector>())
    , m_timeBoundsQueue(kGeneralRingTimeBoundsQueueSize)
{
}

CARingBuffer::~CARingBuffer()
{
    deallocate();
}

CARingBuffer::CARingBuffer(UniqueRef<CARingBufferStorage>&& storage)
    : m_buffers(WTFMove(storage))
    , m_timeBoundsQueue(kGeneralRingTimeBoundsQueueSize)
{
}

void CARingBuffer::allocate(const CAAudioStreamDescription& format, size_t frameCount)
{
    m_description = format;
    deallocate();

    frameCount = WTF::roundUpToPowerOfTwo(frameCount);

    m_channelCount = format.numberOfChannelStreams();
    m_bytesPerFrame = format.bytesPerFrame();
    m_frameCount = frameCount;
    m_frameCountMask = frameCount - 1;
    m_capacityBytes = m_bytesPerFrame * frameCount;

    m_buffers->allocate(m_capacityBytes * m_channelCount);

    m_pointers.resize(m_channelCount);
    Byte* channelData = static_cast<Byte*>(m_buffers->data());

    for (auto& pointer : m_pointers) {
        pointer = channelData;
        channelData += m_capacityBytes;
    }

    flush();
}

void CARingBuffer::deallocate()
{
    m_buffers->deallocate();
    m_pointers.clear();
    m_channelCount = 0;
    m_capacityBytes = 0;
    m_frameCount = 0;
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

static void FetchABL(AudioBufferList* list, size_t destOffset, Vector<Byte*>& pointers, size_t srcOffset, size_t nbytes, AudioStreamDescription::PCMFormat format, CARingBuffer::FetchMode mode)
{
    ASSERT(list->mNumberBuffers == pointers.size());
    AudioBuffer* dest = list->mBuffers;
    for (auto& pointer : pointers) {
        if (destOffset > dest->mDataByteSize)
            continue;

        nbytes = std::min<size_t>(nbytes, dest->mDataByteSize - destOffset);
        if (mode == CARingBuffer::Copy)
            memcpy(static_cast<Byte*>(dest->mData) + destOffset, pointer + srcOffset, nbytes);
        else {
            switch (format) {
            case AudioStreamDescription::Int16: {
                int16_t* destination = static_cast<int16_t*>(dest->mData);
                int16_t* source = reinterpret_cast<int16_t*>(pointer + srcOffset);
                for (size_t i = 0; i < nbytes / sizeof(int16_t); i++)
                    destination[i] += source[i];
                break;
            }
            case AudioStreamDescription::Int32: {
                int32_t* destination = static_cast<int32_t*>(dest->mData);
                vDSP_vaddi(destination, 1, reinterpret_cast<int32_t*>(pointer + srcOffset), 1, destination, 1, nbytes / sizeof(int32_t));
                break;
            }
            case AudioStreamDescription::Float32: {
                float* destination = static_cast<float*>(dest->mData);
                vDSP_vadd(destination, 1, reinterpret_cast<float*>(pointer + srcOffset), 1, destination, 1, nbytes / sizeof(float));
                break;
            }
            case AudioStreamDescription::Float64: {
                double* destination = static_cast<double*>(dest->mData);
                vDSP_vaddD(destination, 1, reinterpret_cast<double*>(pointer + srcOffset), 1, destination, 1, nbytes / sizeof(double));
                break;
            }
            case AudioStreamDescription::None:
                ASSERT_NOT_REACHED();
                break;
            }
        }
        ++dest;
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

void CARingBuffer::flush()
{
    LockHolder locker(m_currentFrameBoundsLock);
    for (auto& timeBounds : m_timeBoundsQueue) {
        timeBounds.m_startFrame = 0;
        timeBounds.m_endFrame = 0;
        timeBounds.m_updateCounter = 0;
    }
    m_timeBoundsQueuePtr = 0;
}

CARingBuffer::Error CARingBuffer::store(const AudioBufferList* list, size_t framesToWrite, uint64_t startFrame)
{
    if (!framesToWrite)
        return Ok;

    if (framesToWrite > m_frameCount)
        return TooMuch;

    uint64_t endFrame = startFrame + framesToWrite;

    if (startFrame < currentEndFrame()) {
        // Throw everything out when going backwards.
        setCurrentFrameBounds(startFrame, startFrame);
    } else if (endFrame - currentStartFrame() <= m_frameCount) {
        // The buffer has not yet wrapped and will not need to.
        // No-op.
    } else {
        // Advance the start time past the region we are about to overwrite
        // starting one buffer of time behind where we're writing.
        uint64_t newStartFrame = endFrame - m_frameCount;
        uint64_t newEndFrame = std::max(newStartFrame, currentEndFrame());
        setCurrentFrameBounds(newStartFrame, newEndFrame);
    }

    // Write the new frames.
    size_t offset0;
    size_t offset1;
    uint64_t curEnd = currentEndFrame();

    if (startFrame > curEnd) {
        // We are skipping some samples, so zero the range we are skipping.
        offset0 = frameOffset(curEnd);
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
    setCurrentFrameBounds(currentStartFrame(), endFrame);

    return Ok;
}

void CARingBuffer::setCurrentFrameBounds(uint64_t startTime, uint64_t endTime)
{
    LockHolder locker(m_currentFrameBoundsLock);
    uint32_t nextPtr = m_timeBoundsQueuePtr.load() + 1;
    uint32_t index = nextPtr & kGeneralRingTimeBoundsQueueMask;

    m_timeBoundsQueue[index].m_startFrame = startTime;
    m_timeBoundsQueue[index].m_endFrame = endTime;
    m_timeBoundsQueue[index].m_updateCounter = nextPtr;
    m_timeBoundsQueuePtr++;
}

void CARingBuffer::getCurrentFrameBounds(uint64_t &startTime, uint64_t &endTime)
{
    uint32_t curPtr = m_timeBoundsQueuePtr.load();
    uint32_t index = curPtr & kGeneralRingTimeBoundsQueueMask;
    CARingBuffer::TimeBounds& bounds = m_timeBoundsQueue[index];

    startTime = bounds.m_startFrame;
    endTime = bounds.m_endFrame;
}

void CARingBuffer::clipTimeBounds(uint64_t& startRead, uint64_t& endRead)
{
    uint64_t startTime;
    uint64_t endTime;

    getCurrentFrameBounds(startTime, endTime);

    if (startRead > endTime || endRead < startTime) {
        endRead = startRead;
        return;
    }

    startRead = std::max(startRead, startTime);
    endRead = std::min(endRead, endTime);
    endRead = std::max(endRead, startRead);
}

uint64_t CARingBuffer::currentStartFrame() const
{
    uint32_t index = m_timeBoundsQueuePtr.load() & kGeneralRingTimeBoundsQueueMask;
    return m_timeBoundsQueue[index].m_startFrame;
}

uint64_t CARingBuffer::currentEndFrame() const
{
    uint32_t index = m_timeBoundsQueuePtr.load() & kGeneralRingTimeBoundsQueueMask;
    return m_timeBoundsQueue[index].m_endFrame;
}

CARingBuffer::Error CARingBuffer::fetch(AudioBufferList* list, size_t nFrames, uint64_t startRead, FetchMode mode)
{
    if (!nFrames)
        return Ok;
    
    startRead = std::max<uint64_t>(0, startRead);
    
    uint64_t endRead = startRead + nFrames;
    
    uint64_t startRead0 = startRead;
    uint64_t endRead0 = endRead;
    
    clipTimeBounds(startRead, endRead);

    if (startRead == endRead) {
        ZeroABL(list, 0, nFrames * m_bytesPerFrame);
        return Ok;
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
        FetchABL(list, destStartByteOffset, m_pointers, offset0, nbytes, m_description.format(), mode);
    } else {
        nbytes = m_capacityBytes - offset0;
        FetchABL(list, destStartByteOffset, m_pointers, offset0, nbytes, m_description.format(), mode);
        if (offset1)
            FetchABL(list, destStartByteOffset + nbytes, m_pointers, 0, offset1, m_description.format(), mode);
        nbytes += offset1;
    }
    
    int channelCount = list->mNumberBuffers;
    AudioBuffer* dest = list->mBuffers;
    while (--channelCount >= 0) {
        dest->mDataByteSize = nbytes;
        dest++;
    }
    
    return Ok;
}

}

#endif // ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)
