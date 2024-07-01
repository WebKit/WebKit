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
#import "AudioSampleDataSource.h"
#import "AudioSourceProviderClient.h"
#import "Logging.h"
#import "WebAudioBufferList.h"
#import <objc/runtime.h>
#import <wtf/MainThread.h>

#if !LOG_DISABLED
#import <wtf/StringPrintStream.h>
#endif

#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

static const double kRingBufferDuration = 1;

WebAudioSourceProviderCocoa::WebAudioSourceProviderCocoa()
{
}

WebAudioSourceProviderCocoa::~WebAudioSourceProviderCocoa()
{
}

void WebAudioSourceProviderCocoa::setClient(WeakPtr<AudioSourceProviderClient>&& client)
{
    if (m_client == client)
        return;
    m_client = WTFMove(client);
    hasNewClient(m_client.get());
}

void WebAudioSourceProviderCocoa::provideInput(AudioBus* bus, size_t framesToProcess)
{
    if (!m_lock.tryLock()) {
        bus->zero();
        return;
    }
    Locker locker { AdoptLock, m_lock };
    if (!m_dataSource || !m_audioBufferList) {
        bus->zero();
        return;
    }

    if (m_writeCount <= m_readCount) {
        bus->zero();
        return;
    }

    if (bus->numberOfChannels() < m_audioBufferList->bufferCount()) {
        bus->zero();
        return;
    }

    for (unsigned i = 0; i < bus->numberOfChannels(); ++i) {
        auto& channel = *bus->channel(i);
        if (i >= m_audioBufferList->bufferCount()) {
            channel.zero();
            continue;
        }
        auto* buffer = m_audioBufferList->buffer(i);
        buffer->mNumberChannels = 1;
        buffer->mData = channel.mutableData();
        buffer->mDataByteSize = channel.length() * sizeof(float);
    }

    ASSERT(framesToProcess <= bus->length());
    m_dataSource->pullSamples(*m_audioBufferList->list(), framesToProcess, m_readCount, 0, AudioSampleDataSource::Copy);
    m_readCount += framesToProcess;
}

void WebAudioSourceProviderCocoa::prepare(const AudioStreamBasicDescription& format)
{
    DisableMallocRestrictionsForCurrentThreadScope scope;

    Locker locker { m_lock };

    LOG(Media, "WebAudioSourceProviderCocoa::prepare(%p)", this);

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
    m_audioBufferList = makeUnique<WebAudioBufferList>(m_outputDescription.value());

    if (!m_dataSource)
        m_dataSource = AudioSampleDataSource::create(kRingBufferDuration * sampleRate, loggerHelper(), m_pollSamplesCount);
    m_dataSource->setInputFormat(m_inputDescription.value());
    m_dataSource->setOutputFormat(m_outputDescription.value());

    callOnMainThread([protectedThis = Ref { *this }, numberOfChannels, sampleRate] {
        if (protectedThis->m_client)
            protectedThis->m_client->setFormat(numberOfChannels, sampleRate);
    });
}

// May get called on a background thread.
void WebAudioSourceProviderCocoa::receivedNewAudioSamples(const PlatformAudioData& data, const AudioStreamDescription& description, size_t frameCount)
{
    ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
    auto& basicDescription = *std::get<const AudioStreamBasicDescription*>(description.platformDescription().description);
    if (!m_inputDescription || m_inputDescription->streamDescription() != basicDescription)
        prepare(basicDescription);

    if (!m_dataSource)
        return;

    m_dataSource->pushSamples(MediaTime(m_writeCount, m_inputDescription->sampleRate()), data, frameCount);

    m_writeCount += frameCount;
}

}

#endif // ENABLE(WEB_AUDIO)
