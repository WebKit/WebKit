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
#import "WebAudioSourceProviderAVFObjC.h"

#if ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)

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

Ref<WebAudioSourceProviderAVFObjC> WebAudioSourceProviderAVFObjC::create(MediaStreamTrackPrivate& source)
{
    return adoptRef(*new WebAudioSourceProviderAVFObjC(source));
}

WebAudioSourceProviderAVFObjC::WebAudioSourceProviderAVFObjC(MediaStreamTrackPrivate& source)
    : m_captureSource(&source)
{
}

WebAudioSourceProviderAVFObjC::~WebAudioSourceProviderAVFObjC()
{
    std::lock_guard<Lock> lock(m_mutex);

    if (m_connected && m_captureSource)
        m_captureSource->removeObserver(*this);
}

void WebAudioSourceProviderAVFObjC::provideInput(AudioBus* bus, size_t framesToProcess)
{
    std::unique_lock<Lock> lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock() || !m_dataSource) {
        bus->zero();
        return;
    }

    if (m_writeCount <= m_readCount) {
        bus->zero();
        return;
    }

    WebAudioBufferList list { m_outputDescription.value() };
    for (unsigned i = 0; i < bus->numberOfChannels(); ++i) {
        AudioChannel& channel = *bus->channel(i);
        if (i >= list.bufferCount()) {
            channel.zero();
            continue;
        }
        auto* buffer = list.buffer(i);
        buffer->mNumberChannels = 1;
        buffer->mData = channel.mutableData();
        buffer->mDataByteSize = channel.length() * sizeof(float);
    }

    m_dataSource->pullSamples(*list.list(), framesToProcess, m_readCount, 0, AudioSampleDataSource::Copy);
    m_readCount += framesToProcess;
}

void WebAudioSourceProviderAVFObjC::setClient(AudioSourceProviderClient* client)
{
    if (m_client == client)
        return;

    m_client = client;

    if (!m_captureSource)
        return;

    if (m_client && !m_connected) {
        m_connected = true;
        m_captureSource->addObserver(*this);
    } else if (!m_client && m_connected) {
        m_captureSource->removeObserver(*this);
        m_connected = false;
    }
}

void WebAudioSourceProviderAVFObjC::prepare(const AudioStreamBasicDescription& format)
{
    std::lock_guard<Lock> lock(m_mutex);

    LOG(Media, "WebAudioSourceProviderAVFObjC::prepare(%p)", this);

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

    if (!m_dataSource)
        m_dataSource = AudioSampleDataSource::create(kRingBufferDuration * sampleRate);
    m_dataSource->setInputFormat(m_inputDescription.value());
    m_dataSource->setOutputFormat(m_outputDescription.value());

    callOnMainThread([protectedThis = makeRef(*this), numberOfChannels, sampleRate] {
        if (protectedThis->m_client)
            protectedThis->m_client->setFormat(numberOfChannels, sampleRate);
    });
}

void WebAudioSourceProviderAVFObjC::unprepare()
{
    std::lock_guard<Lock> lock(m_mutex);

    m_inputDescription = WTF::nullopt;
    m_outputDescription = WTF::nullopt;
    m_dataSource = nullptr;
    m_listBufferSize = 0;
    if (m_captureSource) {
        m_captureSource->removeObserver(*this);
        m_captureSource = nullptr;
    }
}

// May get called on a background thread.
void WebAudioSourceProviderAVFObjC::audioSamplesAvailable(MediaStreamTrackPrivate& track, const MediaTime&, const PlatformAudioData& data, const AudioStreamDescription& description, size_t frameCount)
{
    if (!track.enabled())
        return;

    ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
    auto& basicDescription = *WTF::get<const AudioStreamBasicDescription*>(description.platformDescription().description);
    if (!m_inputDescription || m_inputDescription->streamDescription() != basicDescription)
        prepare(basicDescription);

    if (!m_dataSource)
        return;

    m_dataSource->pushSamples(MediaTime(m_writeCount, m_outputDescription->sampleRate()), data, frameCount);

    m_writeCount += frameCount;
}

}

#endif // ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)
