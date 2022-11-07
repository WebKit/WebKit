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

#pragma once

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)

#include "AudioStreamDescription.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <optional>
#include <wtf/CheckedArithmetic.h>
#include <wtf/Lock.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>

typedef struct AudioBufferList AudioBufferList;

namespace WebCore {

class CARingBuffer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT virtual ~CARingBuffer();

    enum Error {
        Ok,
        TooMuch, // fetch start time is earlier than buffer start time and fetch end time is later than buffer end time
    };

    WEBCORE_EXPORT Error store(const AudioBufferList*, size_t frameCount, uint64_t startFrame);

    enum FetchMode { Copy, MixInt16, MixInt32, MixFloat32, MixFloat64 };
    static FetchMode fetchModeForMixing(AudioStreamDescription::PCMFormat);
    WEBCORE_EXPORT bool fetchIfHasEnoughData(AudioBufferList*, size_t frameCount, uint64_t startFrame, FetchMode = Copy);

    // Fills buffer with silence if there is not enough data.
    WEBCORE_EXPORT void fetch(AudioBufferList*, size_t frameCount, uint64_t startFrame, FetchMode = Copy);

    virtual void flush() = 0;

    WEBCORE_EXPORT void getCurrentFrameBounds(uint64_t& startFrame, uint64_t& endFrame);

    uint32_t channelCount() const { return m_channelCount; }

protected:
    WEBCORE_EXPORT CARingBuffer(size_t bytesPerFrame, size_t frameCount, uint32_t numChannelStreams);
    WEBCORE_EXPORT void initialize();

    WEBCORE_EXPORT static CheckedSize computeCapacityBytes(size_t bytesPerFrame, size_t frameCount);
    WEBCORE_EXPORT static CheckedSize computeSizeForBuffers(size_t bytesPerFrame, size_t frameCount, uint32_t numChannelStreams);

    virtual void* data() = 0;
    virtual void getCurrentFrameBoundsWithoutUpdate(uint64_t& startTime, uint64_t& endTime) = 0;
    virtual void setCurrentFrameBounds(uint64_t startFrame, uint64_t endFrame) = 0;
    virtual void updateFrameBounds() { }
    virtual uint64_t currentStartFrame() const = 0;
    virtual uint64_t currentEndFrame() const = 0;
    virtual size_t size() const = 0;

private:
    size_t frameOffset(uint64_t frameNumber) const { return (frameNumber % m_frameCount) * m_bytesPerFrame; }
    void clipTimeBounds(uint64_t& startRead, uint64_t& endRead);
    void fetchInternal(AudioBufferList*, size_t frameCount, uint64_t startFrame, FetchMode);

    Vector<Byte*> m_pointers;
    const uint32_t m_channelCount;
    const size_t m_bytesPerFrame;
    const uint32_t m_frameCount;
    const size_t m_capacityBytes;
};

inline CARingBuffer::FetchMode CARingBuffer::fetchModeForMixing(AudioStreamDescription::PCMFormat format)
{
    switch (format) {
    case AudioStreamDescription::None:
        ASSERT_NOT_REACHED();
        return MixInt32;
    case AudioStreamDescription::Int16:
        return MixInt16;
    case AudioStreamDescription::Int32:
        return MixInt32;
    case AudioStreamDescription::Float32:
        return MixFloat32;
    case AudioStreamDescription::Float64:
        return MixFloat64;
    }
}


class InProcessCARingBuffer final : public CARingBuffer {
public:
    WEBCORE_EXPORT static std::unique_ptr<InProcessCARingBuffer> allocate(const WebCore::CAAudioStreamDescription& format, size_t frameCount);
    WEBCORE_EXPORT ~InProcessCARingBuffer();
    WEBCORE_EXPORT void flush() final;

protected:
    WEBCORE_EXPORT InProcessCARingBuffer(size_t bytesPerFrame, size_t frameCount, uint32_t numChannelStreams, Vector<uint8_t>&& buffer);
    void* data() final { return m_buffer.data(); }
    void getCurrentFrameBoundsWithoutUpdate(uint64_t& startTime, uint64_t& endTime) final;
    void setCurrentFrameBounds(uint64_t startFrame, uint64_t endFrame) final;
    uint64_t currentStartFrame() const final;
    uint64_t currentEndFrame() const final;
    size_t size() const final { return m_buffer.size(); }

private:
    struct TimeBounds {
        TimeBounds()
            : m_startFrame(0)
            , m_endFrame(0)
            , m_updateCounter(0)
        {
        }
        volatile uint64_t m_startFrame;
        volatile uint64_t m_endFrame;
        volatile uint32_t m_updateCounter;
    };

    Vector<uint8_t> m_buffer;
    Vector<TimeBounds> m_timeBoundsQueue;
    Lock m_currentFrameBoundsLock;
    std::atomic<int32_t> m_timeBoundsQueuePtr { 0 };
};

}

#endif // ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)
