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

#include "Logging.h"
#include "NotImplemented.h"
#include "VectorMath.h"
#include <Accelerate/Accelerate.h>
#include <AudioToolbox/AudioConverter.h>
#include <pal/cf/CoreAudioExtras.h>
#include <wtf/SetForScope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ZippedRange.h>

#include <pal/cf/AudioToolboxSoftLink.h>

namespace WebCore {

Ref<AudioSampleBufferList> AudioSampleBufferList::create(const CAAudioStreamDescription& format, size_t maximumSampleCount)
{
    return adoptRef(*new AudioSampleBufferList(format, maximumSampleCount));
}

AudioSampleBufferList::AudioSampleBufferList(const CAAudioStreamDescription& format, size_t maximumSampleCount)
    : m_internalFormat(format)
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
    auto bufferSpan = span(bufferList);
    for (auto& buffer : bufferSpan) {
        switch (format) {
        case AudioStreamDescription::Int16: {
            auto data = mutableSpan<int16_t>(buffer);
            for (auto& value : data)
                value *= gain;
            break;
        }
        case AudioStreamDescription::Int32: {
            auto data = mutableSpan<int32_t>(buffer);
            for (auto& value : data)
                value *= gain;
            break;
        }
        case AudioStreamDescription::Float32: {
            auto data = mutableSpan<float>(buffer);
            vDSP_vsmul(data.data(), 1, &gain, data.data(), 1, data.size());
            break;
        }
        case AudioStreamDescription::Float64: {
            auto data = mutableSpan<double>(buffer);
            double gainAsDouble = gain;
            vDSP_vsmulD(data.data(), 1, &gainAsDouble, data.data(), 1, data.size());
            break;
        }
        case AudioStreamDescription::Uint8:
        case AudioStreamDescription::Int24:
            notImplemented();
            ASSERT_NOT_REACHED();
            break;
        case AudioStreamDescription::None:
            ASSERT_NOT_REACHED();
            break;
        }
    }
}

void AudioSampleBufferList::applyGain(float gain)
{
    applyGain(m_bufferList.get(), gain, m_internalFormat.format());
}

static void mixBuffers(WebAudioBufferList& destinationBuffer, const AudioBufferList& sourceBuffer, AudioStreamDescription::PCMFormat format, size_t frameCount)
{
    auto sourceBufferSpan = span(sourceBuffer);
    auto destinationBufferSpan = span(*destinationBuffer.list());
    for (auto [source, destination] : zippedRange(sourceBufferSpan, destinationBufferSpan)) {
        switch (format) {
        case AudioStreamDescription::Int16: {
            ASSERT(frameCount <= destination.mDataByteSize / 2);
            ASSERT(frameCount <= source.mDataByteSize / 2);

            auto sourceData = span<int16_t>(source).first(frameCount);
            auto destinationData = mutableSpan<int16_t>(destination).first(frameCount);
            for (auto [s, d] : zippedRange(sourceData, destinationData))
                d += s;
            break;
        }
        case AudioStreamDescription::Int32: {
            ASSERT(frameCount <= destination.mDataByteSize / sizeof(int32_t));
            ASSERT(frameCount <= source.mDataByteSize / sizeof(int32_t));

            auto sourceData = span<int32_t>(source).first(frameCount);
            auto destinationData = mutableSpan<int32_t>(destination).first(frameCount);
            VectorMath::add(destinationData, sourceData, destinationData);
            break;
        }
        case AudioStreamDescription::Float32: {
            ASSERT(frameCount <= destination.mDataByteSize / sizeof(float));
            ASSERT(frameCount <= source.mDataByteSize / sizeof(float));

            auto sourceData = span<float>(source).first(frameCount);
            auto destinationData = mutableSpan<float>(destination).first(frameCount);
            VectorMath::add(destinationData, sourceData, destinationData);
            break;
        }
        case AudioStreamDescription::Float64: {
            ASSERT(frameCount <= destination.mDataByteSize / sizeof(double));
            ASSERT(frameCount <= source.mDataByteSize / sizeof(double));

            auto sourceData = span<double>(source).first(frameCount);
            auto destinationData = mutableSpan<double>(destination).first(frameCount);
            VectorMath::add(destinationData, sourceData, destinationData);
            break;
        }
        case AudioStreamDescription::Uint8:
        case AudioStreamDescription::Int24:
            notImplemented();
            ASSERT_NOT_REACHED();
            break;
        case AudioStreamDescription::None:
            ASSERT_NOT_REACHED();
            break;
        }
    }
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

    mixBuffers(bufferList(), source.bufferList(), m_internalFormat.format(), frameCount);
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
        auto sourceData = span<uint8_t>(*source.bufferList().buffer(i));
        auto destination = mutableSpan<uint8_t>(*m_bufferList->buffer(i));
        memcpySpan(destination, sourceData.first(frameCount * m_internalFormat.bytesPerPacket()));
    }

    return 0;
}

OSStatus AudioSampleBufferList::copyTo(AudioBufferList& buffer, size_t frameCount)
{
    if (frameCount > m_sampleCount)
        return kAudio_ParamError;
    if (buffer.mNumberBuffers > m_bufferList->bufferCount())
        return kAudio_ParamError;

    auto sourceBuffers = span(*m_bufferList->list());
    auto destinationBuffers = span(buffer);
    for (auto [source, destination] : zippedRange(sourceBuffers, destinationBuffers)) {
        auto sourceData = span<uint8_t>(source);
        auto destinationData = mutableSpan<uint8_t>(destination);
        memcpySpan(destinationData, sourceData.first(frameCount * m_internalFormat.bytesPerPacket()));
    }

    return 0;
}

OSStatus AudioSampleBufferList::mixFrom(const AudioBufferList& source, size_t frameCount)
{
    if (frameCount > m_sampleCount)
        return kAudio_ParamError;
    if (source.mNumberBuffers > m_bufferList->bufferCount())
        return kAudio_ParamError;

    mixBuffers(bufferList(), source, m_internalFormat.format(), frameCount);
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
    zeroABL(m_bufferList.get(), m_internalFormat.bytesPerPacket() * m_sampleCapacity);
}

void AudioSampleBufferList::zeroABL(AudioBufferList& bufferList, size_t byteCount)
{
    auto bufferSpan = span(bufferList);
    for (auto buffer : bufferSpan)
        zeroSpan(mutableSpan<uint8_t>(buffer).first(byteCount));
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

    auto contextBuffers = span(context.buffer);
    auto ioDataBuffers = span(*ioData);
    for (auto [ioDataBuffer, contextBuffer] : zippedRange(ioDataBuffers, contextBuffers)) {
        ioDataBuffer.mData = contextBuffer.mData;
        ioDataBuffer.mDataByteSize = context.packetsAvailableToConvert * context.bytesPerPacket;
    }
    context.packetsAvailableToConvert = 0;

    return 0;
}

OSStatus AudioSampleBufferList::copyFrom(const AudioBufferList& source, size_t frameCount, AudioConverterRef converter)
{
    reset();

    AudioStreamBasicDescription inputFormat;
    UInt32 propertyDataSize = sizeof(inputFormat);
    PAL::AudioConverterGetProperty(converter, kAudioConverterCurrentInputStreamDescription, &propertyDataSize, &inputFormat);
    ASSERT(frameCount <= span(source)[0].mDataByteSize / inputFormat.mBytesPerPacket);

    AudioConverterFromABLContext context { source, frameCount, inputFormat.mBytesPerPacket };

#if !LOG_DISABLED
    AudioStreamBasicDescription outputFormat;
    propertyDataSize = sizeof(outputFormat);
    PAL::AudioConverterGetProperty(converter, kAudioConverterCurrentOutputStreamDescription, &propertyDataSize, &outputFormat);

    ASSERT(CAAudioStreamDescription(outputFormat).numberOfChannelStreams() == m_bufferList->bufferCount());
    for (uint32_t i = 0; i < m_bufferList->bufferCount(); ++i) {
        ASSERT(m_bufferList->buffer(i)->mData);
        ASSERT(m_bufferList->buffer(i)->mDataByteSize);
    }
#endif

    UInt32 samplesConverted = m_sampleCapacity;
    OSStatus err = PAL::AudioConverterFillComplexBuffer(converter, audioConverterFromABLCallback, &context, &samplesConverted, m_bufferList->list(), nullptr);
    if (!err || err == kRanOutOfInputDataStatus) {
        m_sampleCount = samplesConverted;
        return 0;
    }

    RELEASE_LOG_ERROR(Media, "AudioSampleBufferList::copyFrom(%p) AudioConverterFillComplexBuffer returned error %d (%.4s)", this, (int)err, (char*)&err);
    m_sampleCount = std::min(m_sampleCapacity, static_cast<size_t>(samplesConverted));
    zero();
    return err;
}

OSStatus AudioSampleBufferList::copyFrom(CARingBuffer& ringBuffer, size_t sampleCount, uint64_t startFrame, CARingBuffer::FetchMode mode)
{
    reset();
    ringBuffer.fetch(bufferList().list(), sampleCount, startFrame, mode);

    m_sampleCount = sampleCount;
    return 0;
}

} // namespace WebCore
