/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "AudioDestinationCocoa.h"

#if ENABLE(WEB_AUDIO)

#include "AudioBus.h"
#include "AudioSession.h"
#include "Logging.h"
#include "MultiChannelResampler.h"
#include "PushPullFIFO.h"

namespace WebCore {

constexpr size_t kRenderBufferSize = 128;

constexpr size_t fifoSize = 96 * kRenderBufferSize;

CreateAudioDestinationCocoaOverride AudioDestinationCocoa::createOverride = nullptr;

std::unique_ptr<AudioDestination> AudioDestination::create(AudioIOCallback& callback, const String&, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
{
    // FIXME: make use of inputDeviceId as appropriate.

    // FIXME: Add support for local/live audio input.
    if (numberOfInputChannels)
        WTFLogAlways("AudioDestination::create(%u, %u, %f) - unhandled input channels", numberOfInputChannels, numberOfOutputChannels, sampleRate);

    if (numberOfOutputChannels > AudioSession::sharedSession().maximumNumberOfOutputChannels())
        WTFLogAlways("AudioDestination::create(%u, %u, %f) - unhandled output channels", numberOfInputChannels, numberOfOutputChannels, sampleRate);

    if (AudioDestinationCocoa::createOverride)
        return AudioDestinationCocoa::createOverride(callback, sampleRate);

    auto destination = makeUnique<AudioDestinationCocoa>(callback, numberOfOutputChannels, sampleRate);
    destination->configure();
    return destination;
}

float AudioDestination::hardwareSampleRate()
{
    return AudioSession::sharedSession().sampleRate();
}

unsigned long AudioDestination::maxChannelCount()
{
    return AudioSession::sharedSession().maximumNumberOfOutputChannels();
}

AudioDestinationCocoa::AudioDestinationCocoa(AudioIOCallback& callback, unsigned numberOfOutputChannels, float sampleRate)
    : m_outputUnit(0)
    , m_callback(callback)
    , m_outputBus(AudioBus::create(numberOfOutputChannels, kRenderBufferSize, false).releaseNonNull())
    , m_renderBus(AudioBus::create(numberOfOutputChannels, kRenderBufferSize).releaseNonNull())
    , m_fifo(makeUniqueRef<PushPullFIFO>(numberOfOutputChannels, fifoSize))
    , m_contextSampleRate(sampleRate)
{
    configure();

    auto hardwareSampleRate = this->hardwareSampleRate();
    if (sampleRate != hardwareSampleRate) {
        double scaleFactor = static_cast<double>(sampleRate) / hardwareSampleRate;
        m_resampler = makeUnique<MultiChannelResampler>(scaleFactor, numberOfOutputChannels, kRenderBufferSize);
    }
}

AudioDestinationCocoa::~AudioDestinationCocoa()
{
    if (m_outputUnit)
        AudioComponentInstanceDispose(m_outputUnit);
}

unsigned AudioDestinationCocoa::numberOfOutputChannels() const
{
    return m_renderBus->numberOfChannels();
}

unsigned AudioDestinationCocoa::framesPerBuffer() const
{
    return m_renderBus->length();
}

void AudioDestinationCocoa::start()
{
    LOG(Media, "AudioDestinationCocoa::start");
    OSStatus result = AudioOutputUnitStart(m_outputUnit);

    if (!result)
        setIsPlaying(true);
}

void AudioDestinationCocoa::stop()
{
    LOG(Media, "AudioDestinationCocoa::stop");
    OSStatus result = AudioOutputUnitStop(m_outputUnit);

    if (!result)
        setIsPlaying(false);
}

void AudioDestinationCocoa::setIsPlaying(bool isPlaying)
{
    if (m_isPlaying == isPlaying)
        return;

    m_isPlaying = isPlaying;
    m_callback.isPlayingDidChange();
}

void AudioDestinationCocoa::setAudioStreamBasicDescription(AudioStreamBasicDescription& streamFormat)
{
    const int bytesPerFloat = sizeof(Float32);
    const int bitsPerByte = 8;
    streamFormat.mSampleRate = hardwareSampleRate();
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    streamFormat.mBytesPerPacket = bytesPerFloat;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mBytesPerFrame = bytesPerFloat;
    streamFormat.mChannelsPerFrame = numberOfOutputChannels();
    streamFormat.mBitsPerChannel = bitsPerByte * bytesPerFloat;
}

static void assignAudioBuffersToBus(AudioBuffer* buffers, AudioBus& bus, UInt32 numberOfBuffers, UInt32 numberOfFrames, UInt32 frameOffset, UInt32 framesThisTime)
{
    for (UInt32 i = 0; i < numberOfBuffers; ++i) {
        UInt32 bytesPerFrame = buffers[i].mDataByteSize / numberOfFrames;
        UInt32 byteOffset = frameOffset * bytesPerFrame;
        auto* memory = reinterpret_cast<float*>(reinterpret_cast<char*>(buffers[i].mData) + byteOffset);
        bus.setChannelMemory(i, memory, framesThisTime);
    }
}

// Pulls on our provider to get rendered audio stream.
OSStatus AudioDestinationCocoa::render(const AudioTimeStamp* timestamp, UInt32 numberOfFrames, AudioBufferList* ioData)
{
    if (m_fifo->length() < numberOfFrames)
        return noErr;

    if (timestamp) {
        m_outputTimestamp = {
            Seconds { timestamp->mSampleTime / sampleRate() },
            MonotonicTime::fromMachAbsoluteTime(timestamp->mHostTime)
        };
    } else
        m_outputTimestamp = AudioIOPosition { };

    auto* buffers = ioData->mBuffers;
    UInt32 numberOfBuffers = ioData->mNumberBuffers;

    // Associate the destination data array with the output bus then fill the FIFO.
    assignAudioBuffersToBus(buffers, m_outputBus.get(), numberOfBuffers, numberOfFrames, 0, numberOfFrames);
    size_t framesToRender = m_fifo->pull(m_outputBus.ptr(), numberOfFrames);

    for (size_t pushedFrames = 0; pushedFrames < framesToRender; pushedFrames += kRenderBufferSize) {
        if (m_resampler)
            m_resampler->process(this, m_renderBus.ptr(), kRenderBufferSize);
        else
            m_callback.render(0, m_renderBus.ptr(), kRenderBufferSize, m_outputTimestamp);

        m_fifo->push(m_renderBus.ptr());
    }

    return noErr;
}

// DefaultOutputUnit callback
OSStatus AudioDestinationCocoa::inputProc(void* userData, AudioUnitRenderActionFlags*, const AudioTimeStamp* timestamp, UInt32 /*busNumber*/, UInt32 numberOfFrames, AudioBufferList* ioData)
{
    auto* audioOutput = static_cast<AudioDestinationCocoa*>(userData);
    return audioOutput->render(timestamp, numberOfFrames, ioData);
}

void AudioDestinationCocoa::provideInput(AudioBus* bus, size_t framesToProcess)
{
    ASSERT_UNUSED(framesToProcess, framesToProcess == kRenderBufferSize);
    m_callback.render(0, bus, kRenderBufferSize, m_outputTimestamp);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
