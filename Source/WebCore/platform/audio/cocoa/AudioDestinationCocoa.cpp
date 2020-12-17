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

AudioDestinationCocoa::AudioDestinationCocoa(AudioIOCallback& callback, unsigned numberOfOutputChannels, float sampleRate, bool configureAudioOutputUnit)
    : AudioDestination(callback)
    , m_audioOutputUnitAdaptor(*this)
    , m_outputBus(AudioBus::create(numberOfOutputChannels, AudioUtilities::renderQuantumSize, false).releaseNonNull())
    , m_renderBus(AudioBus::create(numberOfOutputChannels, AudioUtilities::renderQuantumSize).releaseNonNull())
    , m_fifo(makeUniqueRef<PushPullFIFO>(numberOfOutputChannels, fifoSize))
    , m_contextSampleRate(sampleRate)
{
    if (configureAudioOutputUnit)
        m_audioOutputUnitAdaptor.configure(hardwareSampleRate(), numberOfOutputChannels);

    auto hardwareSampleRate = this->hardwareSampleRate();
    if (sampleRate != hardwareSampleRate) {
        double scaleFactor = static_cast<double>(sampleRate) / hardwareSampleRate;
        m_resampler = makeUnique<MultiChannelResampler>(scaleFactor, numberOfOutputChannels, AudioUtilities::renderQuantumSize, [this](AudioBus* bus, size_t framesToProcess) {
            ASSERT_UNUSED(framesToProcess, framesToProcess == AudioUtilities::renderQuantumSize);
            callRenderCallback(nullptr, bus, AudioUtilities::renderQuantumSize, m_outputTimestamp);
        });
    }
}

AudioDestinationCocoa::~AudioDestinationCocoa() = default;

unsigned AudioDestinationCocoa::numberOfOutputChannels() const
{
    return m_renderBus->numberOfChannels();
}

unsigned AudioDestinationCocoa::framesPerBuffer() const
{
    return m_renderBus->length();
}

void AudioDestinationCocoa::start(Function<void(Function<void()>&&)>&& dispatchToRenderThread, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(isMainThread());
    LOG(Media, "AudioDestinationCocoa::start");
    {
        auto locker = holdLock(m_dispatchToRenderThreadLock);
        m_dispatchToRenderThread = WTFMove(dispatchToRenderThread);
    }
    startRendering(WTFMove(completionHandler));
}

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

void AudioDestinationCocoa::stop(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(isMainThread());
    LOG(Media, "AudioDestinationCocoa::stop");
    stopRendering(WTFMove(completionHandler));
    {
        auto locker = holdLock(m_dispatchToRenderThreadLock);
        m_dispatchToRenderThread = nullptr;
    }
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

void AudioDestinationCocoa::setIsPlaying(bool isPlaying)
{
    ASSERT(isMainThread());
    {
        auto locker = holdLock(m_isPlayingLock);
        if (m_isPlaying == isPlaying)
            return;

        m_isPlaying = isPlaying;
    }

    {
        auto locker = holdLock(m_callbackLock);
        if (m_callback)
            m_callback->isPlayingDidChange();
    }
}

void AudioDestinationCocoa::getAudioStreamBasicDescription(AudioStreamBasicDescription& streamFormat)
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

bool AudioDestinationCocoa::hasEnoughFrames(UInt32 numberOfFrames) const
{
    return m_fifo->length() >= numberOfFrames;
}

// Pulls on our provider to get rendered audio stream.
OSStatus AudioDestinationCocoa::render(double sampleTime, uint64_t hostTime, UInt32 numberOfFrames, AudioBufferList* ioData)
{
    ASSERT(!isMainThread());

    if (!hasEnoughFrames(numberOfFrames))
        return noErr;

    m_outputTimestamp = {
        Seconds { sampleTime / sampleRate() },
        MonotonicTime::fromMachAbsoluteTime(hostTime)
    };

    auto* buffers = ioData->mBuffers;
    auto numberOfBuffers = ioData->mNumberBuffers;

    // Associate the destination data array with the output bus then fill the FIFO.
    assignAudioBuffersToBus(buffers, m_outputBus.get(), numberOfBuffers, numberOfFrames, 0, numberOfFrames);
    size_t framesToRender;

    {
        auto locker = holdLock(m_fifoLock);
        framesToRender = m_fifo->pull(m_outputBus.ptr(), numberOfFrames);
    }

    // When there is a AudioWorklet, we do rendering on the AudioWorkletThread.
    auto locker = tryHoldLock(m_dispatchToRenderThreadLock);
    if (!locker || !m_dispatchToRenderThread)
        return -1;

    m_dispatchToRenderThread([this, protectedThis = makeRef(*this), framesToRender]() mutable {
        auto locker = tryHoldLock(m_isPlayingLock);
        if (locker && m_isPlaying)
            renderOnRenderingThead(framesToRender);
    });

    return noErr;
}

// This runs on the AudioWorkletThread when AudioWorklet is enabled, on the audio device's rendering thread otherwise.
void AudioDestinationCocoa::renderOnRenderingThead(size_t framesToRender)
{
    for (size_t pushedFrames = 0; pushedFrames < framesToRender; pushedFrames += AudioUtilities::renderQuantumSize) {
        if (m_resampler)
            m_resampler->process(m_renderBus.ptr(), AudioUtilities::renderQuantumSize);
        else
            callRenderCallback(nullptr, m_renderBus.ptr(), AudioUtilities::renderQuantumSize, m_outputTimestamp);

        auto locker = holdLock(m_fifoLock);
        m_fifo->push(m_renderBus.ptr());
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
