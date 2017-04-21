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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AudioSampleBufferList.h"

#if ENABLE(MEDIA_STREAM)

#include "Logging.h"
#include "VectorMath.h"
#include <Accelerate/Accelerate.h>
#include <AudioToolbox/AudioConverter.h>
#include <wtf/SetForScope.h>

namespace WebCore {

using namespace VectorMath;

Ref<AudioSampleBufferList> AudioSampleBufferList::create(const CAAudioStreamDescription& format, size_t maximumSampleCount)
{
    return adoptRef(*new AudioSampleBufferList(format, maximumSampleCount));
}

AudioSampleBufferList::AudioSampleBufferList(const CAAudioStreamDescription& format, size_t maximumSampleCount)
    : m_internalFormat(makeUniqueRef<CAAudioStreamDescription>(format))
    , m_sampleCapacity(maximumSampleCount)
    , m_maxBufferSizePerChannel(maximumSampleCount * format.bytesPerFrame() / format.numberOfChannelStreams())
    , m_bufferList(makeUniqueRef<WebAudioBufferList>(m_internalFormat, m_maxBufferSizePerChannel))
{
    ASSERT(format.sampleRate() >= 0);
}

AudioSampleBufferList::~AudioSampleBufferList() = default;

void AudioSampleBufferList::setSampleCount(size_t count)
{
    ASSERT(count <= m_sampleCapacity);
    if (count <= m_sampleCapacity)
        m_sampleCount = count;
}

void AudioSampleBufferList::applyGain(AudioBufferList& bufferList, float gain, AudioStreamDescription::PCMFormat format)
{
    for (uint32_t i = 0; i < bufferList.mNumberBuffers; ++i) {
        switch (format) {
        case AudioStreamDescription::Int16: {
            int16_t* buffer = static_cast<int16_t*>(bufferList.mBuffers[i].mData);
            int frameCount = bufferList.mBuffers[i].mDataByteSize / sizeof(int16_t);
            for (int i = 0; i < frameCount; i++)
                buffer[i] *= gain;
            break;
        }
        case AudioStreamDescription::Int32: {
            int32_t* buffer = static_cast<int32_t*>(bufferList.mBuffers[i].mData);
            int frameCount = bufferList.mBuffers[i].mDataByteSize / sizeof(int32_t);
            for (int i = 0; i < frameCount; i++)
                buffer[i] *= gain;
            break;
            break;
        }
        case AudioStreamDescription::Float32: {
            float* buffer = static_cast<float*>(bufferList.mBuffers[i].mData);
            vDSP_vsmul(buffer, 1, &gain, buffer, 1, bufferList.mBuffers[i].mDataByteSize / sizeof(float));
            break;
        }
        case AudioStreamDescription::Float64: {
            double* buffer = static_cast<double*>(bufferList.mBuffers[i].mData);
            double gainAsDouble = gain;
            vDSP_vsmulD(buffer, 1, &gainAsDouble, buffer, 1, bufferList.mBuffers[i].mDataByteSize / sizeof(double));
            break;
        }
        case AudioStreamDescription::None:
            ASSERT_NOT_REACHED();
            break;
        }
    }
}

void AudioSampleBufferList::applyGain(float gain)
{
    applyGain(m_bufferList.get(), gain, m_internalFormat->format());
}

OSStatus AudioSampleBufferList::mixFrom(const AudioSampleBufferList& source, size_t frameCount)
{
    ASSERT(source.streamDescription() == streamDescription());

    if (source.streamDescription() != streamDescription())
        return kAudio_ParamError;

    if (frameCount > source.sampleCount())
        frameCount = source.sampleCount();

    if (frameCount > m_sampleCapacity)
        return kAudio_ParamError;

    m_sampleCount = frameCount;

    auto& sourceBuffer = source.bufferList();
    for (uint32_t i = 0; i < m_bufferList->bufferCount(); i++) {
        switch (m_internalFormat->format()) {
        case AudioStreamDescription::Int16: {
            int16_t* destination = static_cast<int16_t*>(m_bufferList->buffer(i)->mData);
            int16_t* source = static_cast<int16_t*>(sourceBuffer.buffer(i)->mData);
            for (size_t i = 0; i < frameCount; i++)
                destination[i] += source[i];
            break;
        }
        case AudioStreamDescription::Int32: {
            int32_t* destination = static_cast<int32_t*>(m_bufferList->buffer(i)->mData);
            vDSP_vaddi(destination, 1, reinterpret_cast<int32_t*>(sourceBuffer.buffer(i)->mData), 1, destination, 1, frameCount);
            break;
        }
        case AudioStreamDescription::Float32: {
            float* destination = static_cast<float*>(m_bufferList->buffer(i)->mData);
            vDSP_vadd(destination, 1, reinterpret_cast<float*>(sourceBuffer.buffer(i)->mData), 1, destination, 1, frameCount);
            break;
        }
        case AudioStreamDescription::Float64: {
            double* destination = static_cast<double*>(m_bufferList->buffer(i)->mData);
            vDSP_vaddD(destination, 1, reinterpret_cast<double*>(sourceBuffer.buffer(i)->mData), 1, destination, 1, frameCount);
            break;
        }
        case AudioStreamDescription::None:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    return 0;
}

OSStatus AudioSampleBufferList::copyFrom(const AudioSampleBufferList& source, size_t frameCount)
{
    ASSERT(source.streamDescription() == streamDescription());

    if (source.streamDescription() != streamDescription())
        return kAudio_ParamError;

    if (frameCount > source.sampleCount())
        frameCount = source.sampleCount();

    if (frameCount > m_sampleCapacity)
        return kAudio_ParamError;

    m_sampleCount = frameCount;

    for (uint32_t i = 0; i < m_bufferList->bufferCount(); i++) {
        uint8_t* sourceData = static_cast<uint8_t*>(source.bufferList().buffer(i)->mData);
        uint8_t* destination = static_cast<uint8_t*>(m_bufferList->buffer(i)->mData);
        memcpy(destination, sourceData, frameCount * m_internalFormat->bytesPerPacket());
    }

    return 0;
}

OSStatus AudioSampleBufferList::copyTo(AudioBufferList& buffer, size_t frameCount)
{
    if (frameCount > m_sampleCount)
        return kAudio_ParamError;
    if (buffer.mNumberBuffers > m_bufferList->bufferCount())
        return kAudio_ParamError;

    for (uint32_t i = 0; i < buffer.mNumberBuffers; i++) {
        uint8_t* sourceData = static_cast<uint8_t*>(m_bufferList->buffer(i)->mData);
        uint8_t* destination = static_cast<uint8_t*>(buffer.mBuffers[i].mData);
        memcpy(destination, sourceData, frameCount * m_internalFormat->bytesPerPacket());
    }

    return 0;
}

void AudioSampleBufferList::reset()
{
    m_sampleCount = 0;
    m_timestamp = 0;
    m_hostTime = -1;

    m_bufferList->reset();
}

void AudioSampleBufferList::zero()
{
    zeroABL(m_bufferList.get(), m_internalFormat->bytesPerPacket() * m_sampleCapacity);
}

void AudioSampleBufferList::zeroABL(AudioBufferList& buffer, size_t byteCount)
{
    for (uint32_t i = 0; i < buffer.mNumberBuffers; ++i)
        memset(buffer.mBuffers[i].mData, 0, byteCount);
}

struct AudioConverterFromABLContext {
    const AudioBufferList& buffer;
    size_t packetsAvailableToConvert;
    size_t bytesPerPacket;
};

static const OSStatus kRanOutOfInputDataStatus = '!mor';

static OSStatus audioConverterFromABLCallback(AudioConverterRef, UInt32* ioNumberDataPackets, AudioBufferList* ioData, AudioStreamPacketDescription**, void* inRefCon)
{
    if (!ioNumberDataPackets || !ioData || !inRefCon) {
        LOG_ERROR("AudioSampleBufferList::audioConverterCallback() invalid input to AudioConverterInput");
        return kAudioConverterErr_UnspecifiedError;
    }

    auto& context = *static_cast<AudioConverterFromABLContext*>(inRefCon);
    if (!context.packetsAvailableToConvert) {
        *ioNumberDataPackets = 0;
        return kRanOutOfInputDataStatus;
    }

    *ioNumberDataPackets = static_cast<UInt32>(context.packetsAvailableToConvert);

    for (uint32_t i = 0; i < ioData->mNumberBuffers; ++i) {
        ioData->mBuffers[i].mData = context.buffer.mBuffers[i].mData;
        ioData->mBuffers[i].mDataByteSize = context.packetsAvailableToConvert * context.bytesPerPacket;
    }
    context.packetsAvailableToConvert = 0;

    return 0;
}

OSStatus AudioSampleBufferList::copyFrom(const AudioBufferList& source, size_t frameCount, AudioConverterRef converter)
{
    reset();

    AudioStreamBasicDescription inputFormat;
    UInt32 propertyDataSize = sizeof(inputFormat);
    AudioConverterGetProperty(converter, kAudioConverterCurrentInputStreamDescription, &propertyDataSize, &inputFormat);
    ASSERT(frameCount <= source.mBuffers[0].mDataByteSize / inputFormat.mBytesPerPacket);

    AudioConverterFromABLContext context { source, frameCount, inputFormat.mBytesPerPacket };

#if !LOG_DISABLED
    AudioStreamBasicDescription outputFormat;
    propertyDataSize = sizeof(outputFormat);
    AudioConverterGetProperty(converter, kAudioConverterCurrentOutputStreamDescription, &propertyDataSize, &outputFormat);

    ASSERT(CAAudioStreamDescription(outputFormat).numberOfChannelStreams() == m_bufferList->bufferCount());
    for (uint32_t i = 0; i < m_bufferList->bufferCount(); ++i) {
        ASSERT(m_bufferList->buffer(i)->mData);
        ASSERT(m_bufferList->buffer(i)->mDataByteSize);
    }
#endif

    UInt32 samplesConverted = m_sampleCapacity;
    OSStatus err = AudioConverterFillComplexBuffer(converter, audioConverterFromABLCallback, &context, &samplesConverted, m_bufferList->list(), nullptr);
    if (!err || err == kRanOutOfInputDataStatus) {
        m_sampleCount = samplesConverted;
        return 0;
    }

    LOG_ERROR("AudioSampleBufferList::copyFrom(%p) AudioConverterFillComplexBuffer returned error %d (%.4s)", this, (int)err, (char*)&err);
    m_sampleCount = std::min(m_sampleCapacity, static_cast<size_t>(samplesConverted));
    zero();
    return err;
}

OSStatus AudioSampleBufferList::copyFrom(AudioSampleBufferList& source, size_t frameCount, AudioConverterRef converter)
{
    return copyFrom(source.bufferList(), frameCount, converter);
}

OSStatus AudioSampleBufferList::copyFrom(CARingBuffer& ringBuffer, size_t sampleCount, uint64_t startFrame, CARingBuffer::FetchMode mode)
{
    reset();
    if (ringBuffer.fetch(bufferList().list(), sampleCount, startFrame, mode) != CARingBuffer::Ok)
        return kAudio_ParamError;

    m_sampleCount = sampleCount;
    return 0;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
