/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "WebAudioSourceProviderCocoa.h"

#if ENABLE(WEB_AUDIO)

#import "AudioBus.h"
#import "AudioChannel.h"
#import "AudioSourceProviderClient.h"
#import "AudioUtilities.h"
#import "Logging.h"
#import "MultiChannelResampler.h"
#import "PitchShiftAudioUnit.h"
#import "WebAudioBufferList.h"
#import <objc/runtime.h>
#import <wtf/IndexedRange.h>
#import <wtf/MainThread.h>

#if !LOG_DISABLED
#import <wtf/StringPrintStream.h>
#endif

#import <pal/cf/AudioToolboxSoftLink.h>
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

WebAudioSourceProviderCocoa::WebAudioSourceProviderCocoa()
{
}

WebAudioSourceProviderCocoa::~WebAudioSourceProviderCocoa()
{
    if (m_converter) {
        PAL::AudioConverterDispose(m_converter);
        m_converter = nullptr;
    }
}

void WebAudioSourceProviderCocoa::setClient(WeakPtr<AudioSourceProviderClient>&& client)
{
    if (m_client == client)
        return;
    m_client = WTF::move(client);

    hasNewClient(m_client.get());
}

void WebAudioSourceProviderCocoa::setPlaybackRate(double playbackRate)
{
    if (m_playbackRate == playbackRate)
        return;

    Locker locker { m_lock };
    m_playbackRate = playbackRate;

    if (!m_outputDescription)
        return;

    if (m_pitchShifter)
        m_pitchShifter->setRate(m_playbackRate);
    m_multiChannelResampler = makeUnique<MultiChannelResampler>(m_playbackRate, m_outputDescription->numberOfChannels(), AudioUtilities::renderQuantumSize, [weakThis = ThreadSafeWeakPtr { *this }](AudioBus& inputBus, size_t numberOfFrames) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->provideInputInternal(inputBus, numberOfFrames);
    });

}

void WebAudioSourceProviderCocoa::setPreservesPitch(bool preservesPitch)
{
    if (m_preservesPitch == preservesPitch)
        return;

    Locker locker { m_lock };
    m_preservesPitch = preservesPitch;
}

void WebAudioSourceProviderCocoa::setVolume(double volume)
{
    if (m_volume == volume)
        return;

    Locker locker { m_lock };
    m_volume = volume;
}

void WebAudioSourceProviderCocoa::audioStorageChanged(std::unique_ptr<CARingBuffer>&& ringBuffer, const AudioStreamDescription& description)
{
    Locker locker { m_lock };
    m_ringBuffer = WTF::move(ringBuffer);

    ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
    auto& basicDescription = *std::get<const AudioStreamBasicDescription*>(description.platformDescription().description);
    if (!m_inputDescription || m_inputDescription->streamDescription() != basicDescription)
        prepare(basicDescription);
}

void WebAudioSourceProviderCocoa::provideInput(AudioBus& bus, size_t framesToProcess)
{
    if (!m_lock.tryLock()) {
        bus.zero();
        return;
    }
    Locker locker { AdoptLock, m_lock };

    if (!m_playbackRate) {
        bus.zero();
        return;
    }

    if (m_pitchShifter && m_preservesPitch && m_playbackRate != 1.0)
        m_pitchShifter->render(bus, framesToProcess);
    else if (m_multiChannelResampler && !m_preservesPitch && m_playbackRate != 1.0)
        m_multiChannelResampler->process(bus, framesToProcess);
    else
        provideInputInternal(bus, framesToProcess);

    if (m_volume < 1.0)
        bus.copyWithGainFrom(bus, m_volume);
}

bool WebAudioSourceProviderCocoa::provideInputInternal(AudioBus& bus, size_t framesToProcess)
{
    if (!m_ringBuffer) {
        bus.zero();
        return false;
    }

    auto [startFrame, endFrame, writeAhead] = m_ringBuffer->getFetchTimeBounds();

    if (m_readCount < startFrame) {
        if (endFrame > writeAhead)
            m_readCount = endFrame - writeAhead;
        else
            m_readCount = startFrame;
    }

    if (m_readCount >= endFrame) {
        bus.zero();
        m_underflowed = true;
        return false;
    }

    size_t framesAvailable = static_cast<size_t>(endFrame - m_readCount);
    if (framesAvailable < framesToProcess) {
        bus.zero();
        framesToProcess = framesAvailable;
    }

    if (m_underflowed) {
        // Wait for enough future data to be written before restarting:
        if (framesAvailable < writeAhead) {
            bus.zero();
            return false;
        }
        m_underflowed = false;
    }

    ASSERT(bus.numberOfChannels() == m_ringBuffer->channelCount());
    if (bus.numberOfChannels() != m_ringBuffer->channelCount()) {
        bus.zero();
        return false;
    }

    for (auto [i, buffer] : indexedRange(span(*m_list))) {
        AudioChannel* channel = bus.channel(i);
        buffer.mNumberChannels = 1;
        buffer.mData = channel->mutableData();
        buffer.mDataByteSize = channel->length() * sizeof(float);
    }

    m_ringBuffer->fetch(m_list.get(), framesToProcess, m_readCount);
    m_readCount += framesToProcess;

    if (m_converter)
        PAL::AudioConverterConvertComplexBuffer(m_converter, framesToProcess, m_list.get(), m_list.get());

    return true;
}

void WebAudioSourceProviderCocoa::prepare(const AudioStreamBasicDescription& format)
{
    DisableMallocRestrictionsForCurrentThreadScope scope;

    m_inputDescription = CAAudioStreamDescription(format);
    int numberOfChannels = format.mChannelsPerFrame;
    double sampleRate = format.mSampleRate;
    ASSERT(sampleRate >= 0);

    const int bytesPerFloat = sizeof(Float32);
    const int bitsPerByte = 8;
    const bool isFloat = true;
    const bool isBigEndian = false;
    const bool isNonInterleaved = true;
    AudioStreamBasicDescription outputDescription { };
    FillOutASBDForLPCM(outputDescription, sampleRate, numberOfChannels, bitsPerByte * bytesPerFloat, bitsPerByte * bytesPerFloat, isFloat, isBigEndian, isNonInterleaved);
    m_outputDescription = CAAudioStreamDescription(outputDescription);
    m_list = PAL::createAudioBufferList(numberOfChannels, PAL::ShouldZeroMemory::Yes);

    m_pitchShifter = makeUnique<PitchShiftAudioUnit>(CAAudioStreamDescription(*m_outputDescription));
    m_pitchShifter->setRate(m_playbackRate);
    m_pitchShifter->setInputCallback([weakThis = ThreadSafeWeakPtr { *this }](AudioBus& inputBus, size_t numberOfFrames) {
        if (RefPtr protectedThis = weakThis.get())
            return protectedThis->provideInputInternal(inputBus, numberOfFrames);
        return false;
    });

    m_multiChannelResampler = makeUnique<MultiChannelResampler>(m_playbackRate, numberOfChannels, AudioUtilities::renderQuantumSize, [weakThis = ThreadSafeWeakPtr { *this }](AudioBus& inputBus, size_t numberOfFrames) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->provideInputInternal(inputBus, numberOfFrames);
    });

    if (*m_inputDescription != *m_outputDescription) {
        if (m_converter) {
            PAL::AudioConverterDispose(m_converter);
            m_converter = nullptr;
        }
        PAL::AudioConverterNew(&m_inputDescription->streamDescription(), &m_outputDescription->streamDescription(), &m_converter);
    }

    callOnMainThread([protectedThis = Ref { *this }, numberOfChannels, sampleRate] {
        if (protectedThis->m_client)
            protectedThis->m_client->setFormat(numberOfChannels, sampleRate);
    });
}

// May get called on a background thread.
void WebAudioSourceProviderCocoa::receivedNewAudioSamples(const PlatformAudioData& data, const AudioStreamDescription&, size_t frameCount)
{
    if (!m_ringBuffer)
        return;

    auto [startFrame, endFrame, writeAhead] = m_ringBuffer->getStoreTimeBounds();
    m_ringBuffer->store(downcast<WebAudioBufferList>(data).list(), frameCount, endFrame, writeAhead);
}

void WebAudioSourceProviderCocoa::setNeedsFlush()
{
    if (!m_ringBuffer)
        return;
    auto [startFrame, endFrame, writeAhead] = m_ringBuffer->getFetchTimeBounds();
    m_readCount = startFrame;
    m_underflowed = true;
}

}

#endif // ENABLE(WEB_AUDIO)
