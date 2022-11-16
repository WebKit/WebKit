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
#include "AudioUtilities.h"
#include "Logging.h"
#include "MultiChannelResampler.h"
#include "PushPullFIFO.h"
#include <algorithm>

namespace WebCore {

constexpr size_t fifoSize = 96 * AudioUtilities::renderQuantumSize;

CreateAudioDestinationCocoaOverride AudioDestinationCocoa::createOverride = nullptr;

Ref<AudioDestination> AudioDestination::create(AudioIOCallback& callback, const String&, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
{
    // FIXME: make use of inputDeviceId as appropriate.

    // FIXME: Add support for local/live audio input.
    if (numberOfInputChannels)
        WTFLogAlways("AudioDestination::create(%u, %u, %f) - unhandled input channels", numberOfInputChannels, numberOfOutputChannels, sampleRate);

    if (numberOfOutputChannels > AudioSession::sharedSession().maximumNumberOfOutputChannels())
        WTFLogAlways("AudioDestination::create(%u, %u, %f) - unhandled output channels", numberOfInputChannels, numberOfOutputChannels, sampleRate);

    if (AudioDestinationCocoa::createOverride)
        return AudioDestinationCocoa::createOverride(callback, sampleRate);

    auto destination = adoptRef(*new AudioDestinationCocoa(callback, numberOfOutputChannels, sampleRate));
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
    : AudioDestinationResampler(callback, numberOfOutputChannels, sampleRate, hardwareSampleRate())
    , m_audioOutputUnitAdaptor(*this)
{
    m_audioOutputUnitAdaptor.configure(hardwareSampleRate(), numberOfOutputChannels);
}

AudioDestinationCocoa::~AudioDestinationCocoa() = default;

void AudioDestinationCocoa::startRendering(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(isMainThread());
    auto success = m_audioOutputUnitAdaptor.start() == noErr;
    if (success)
        setIsPlaying(true);

    callOnMainThread([completionHandler = WTFMove(completionHandler), success]() mutable {
        completionHandler(success);
    });
}

void AudioDestinationCocoa::stopRendering(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(isMainThread());
    auto success = m_audioOutputUnitAdaptor.stop() == noErr;
    if (success)
        setIsPlaying(false);

    callOnMainThread([completionHandler = WTFMove(completionHandler), success]() mutable {
        completionHandler(success);
    });
}

// Pulls on our provider to get rendered audio stream.
OSStatus AudioDestinationCocoa::render(double sampleTime, uint64_t hostTime, UInt32 numberOfFrames, AudioBufferList* ioData)
{
    ASSERT(!isMainThread());

    auto* buffers = ioData->mBuffers;
    auto numberOfBuffers = std::min<UInt32>(ioData->mNumberBuffers, m_outputBus->numberOfChannels());

    // Associate the destination data array with the output bus then fill the FIFO.
    for (UInt32 i = 0; i < numberOfBuffers; ++i) {
        auto* memory = reinterpret_cast<float*>(buffers[i].mData);
        size_t channelNumberOfFrames = std::min<size_t>(numberOfFrames, buffers[i].mDataByteSize / sizeof(float));
        m_outputBus->setChannelMemory(i, memory, channelNumberOfFrames);
    }
    auto framesToRender = pullRendered(numberOfFrames);
    bool success = AudioDestinationResampler::render(sampleTime, MonotonicTime::fromMachAbsoluteTime(hostTime), framesToRender);
    return success ? noErr : -1;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
