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
#include "AudioIOCallback.h"
#include "AudioSession.h"
#include "Logging.h"

namespace WebCore {

const int kRenderBufferSize = 128;

CreateAudioDestinationCocoaOverride AudioDestinationCocoa::createOverride = nullptr;

std::unique_ptr<AudioDestination> AudioDestination::create(AudioIOCallback& callback, const String&, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
{
    // FIXME: make use of inputDeviceId as appropriate.

    // FIXME: Add support for local/live audio input.
    if (numberOfInputChannels)
        WTFLogAlways("AudioDestination::create(%u, %u, %f) - unhandled input channels", numberOfInputChannels, numberOfOutputChannels, sampleRate);

    // FIXME: Add support for multi-channel (> stereo) output.
    if (numberOfOutputChannels != 2)
        WTFLogAlways("AudioDestination::create(%u, %u, %f) - unhandled output channels", numberOfInputChannels, numberOfOutputChannels, sampleRate);

    if (AudioDestinationCocoa::createOverride)
        return AudioDestinationCocoa::createOverride(callback, sampleRate);

    auto destination = makeUnique<AudioDestinationCocoa>(callback, sampleRate);
    destination->configure();
    return destination;
}

float AudioDestination::hardwareSampleRate()
{
    return AudioSession::sharedSession().sampleRate();
}

unsigned long AudioDestination::maxChannelCount()
{
    // FIXME: query the default audio hardware device to return the actual number
    // of channels of the device. Also see corresponding FIXME in create().
    // There is a small amount of code which assumes stereo which can be upgraded.
    return 0;
}

AudioDestinationCocoa::AudioDestinationCocoa(AudioIOCallback& callback, float sampleRate)
    : m_outputUnit(0)
    , m_callback(callback)
    , m_renderBus(AudioBus::create(2, kRenderBufferSize, false).releaseNonNull())
    , m_spareBus(AudioBus::create(2, kRenderBufferSize, true).releaseNonNull())
    , m_sampleRate(sampleRate)
{
    configure();
}

AudioDestinationCocoa::~AudioDestinationCocoa()
{
    if (m_outputUnit)
        AudioComponentInstanceDispose(m_outputUnit);
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

void AudioDestinationCocoa::setAudioStreamBasicDescription(AudioStreamBasicDescription& streamFormat, float sampleRate)
{
    const int bytesPerFloat = sizeof(Float32);
    const int bitsPerByte = 8;
    streamFormat.mSampleRate = sampleRate;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    streamFormat.mBytesPerPacket = bytesPerFloat;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mBytesPerFrame = bytesPerFloat;
    streamFormat.mChannelsPerFrame = 2;
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
OSStatus AudioDestinationCocoa::render(UInt32 numberOfFrames, AudioBufferList* ioData)
{
    auto* buffers = ioData->mBuffers;
    UInt32 numberOfBuffers = ioData->mNumberBuffers;
    UInt32 framesRemaining = numberOfFrames;
    UInt32 frameOffset = 0;
    while (framesRemaining > 0) {
        if (m_startSpareFrame < m_endSpareFrame) {
            ASSERT(m_startSpareFrame < m_endSpareFrame);
            UInt32 framesThisTime = std::min<UInt32>(m_endSpareFrame - m_startSpareFrame, numberOfFrames);
            assignAudioBuffersToBus(buffers, m_renderBus.get(), numberOfBuffers, numberOfFrames, frameOffset, framesThisTime);
            m_renderBus->copyFromRange(m_spareBus.get(), m_startSpareFrame, m_endSpareFrame);
            processBusAfterRender(m_renderBus.get(), framesThisTime);
            frameOffset += framesThisTime;
            framesRemaining -= framesThisTime;
            m_startSpareFrame += framesThisTime;
        }

        UInt32 framesThisTime = std::min<UInt32>(kRenderBufferSize, framesRemaining);
        assignAudioBuffersToBus(buffers, m_renderBus.get(), numberOfBuffers, numberOfFrames, frameOffset, framesThisTime);

        if (!framesThisTime)
            break;
        if (framesThisTime < kRenderBufferSize) {
            m_callback.render(0, m_spareBus.ptr(), kRenderBufferSize);
            m_renderBus->copyFromRange(m_spareBus.get(), 0, framesThisTime);
            m_startSpareFrame = framesThisTime;
            m_endSpareFrame = kRenderBufferSize;
        } else
            m_callback.render(0, m_renderBus.ptr(), framesThisTime);
        processBusAfterRender(m_renderBus.get(), framesThisTime);
        frameOffset += framesThisTime;
        framesRemaining -= framesThisTime;
    }

    return noErr;
}

// DefaultOutputUnit callback
OSStatus AudioDestinationCocoa::inputProc(void* userData, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32 /*busNumber*/, UInt32 numberOfFrames, AudioBufferList* ioData)
{
    auto* audioOutput = static_cast<AudioDestinationCocoa*>(userData);
    return audioOutput->render(numberOfFrames, ioData);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
