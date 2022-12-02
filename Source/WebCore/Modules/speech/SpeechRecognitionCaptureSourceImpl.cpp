/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "SpeechRecognitionCaptureSourceImpl.h"

#if ENABLE(MEDIA_STREAM)

#include "SpeechRecognitionUpdate.h"
#include <wtf/CryptographicallyRandomNumber.h>

#if PLATFORM(COCOA)
#include "CAAudioStreamDescription.h"
#include "WebAudioBufferList.h"
#endif

namespace WebCore {

#if !RELEASE_LOG_DISABLED
static const void* nextLogIdentifier()
{
    static uint64_t logIdentifier = cryptographicallyRandomNumber<uint32_t>();
    return reinterpret_cast<const void*>(++logIdentifier);
}

static RefPtr<Logger>& nullLogger()
{
    static NeverDestroyed<RefPtr<Logger>> logger;
    return logger;
}
#endif

SpeechRecognitionCaptureSourceImpl::SpeechRecognitionCaptureSourceImpl(SpeechRecognitionConnectionClientIdentifier identifier, DataCallback&& dataCallback, StateUpdateCallback&& stateUpdateCallback, Ref<RealtimeMediaSource>&& source)
    : m_clientIdentifier(identifier)
    , m_dataCallback(WTFMove(dataCallback))
    , m_stateUpdateCallback(WTFMove(stateUpdateCallback))
    , m_source(WTFMove(source))
{
#if !RELEASE_LOG_DISABLED
    if (!nullLogger().get()) {
        nullLogger() = Logger::create(this);
        nullLogger()->setEnabled(this, false);
    }

    m_source->setLogger(*nullLogger(), nextLogIdentifier());
#endif

    m_source->addAudioSampleObserver(*this);
    m_source->addObserver(*this);
    m_source->start();

    initializeWeakPtrFactory();
}

SpeechRecognitionCaptureSourceImpl::~SpeechRecognitionCaptureSourceImpl()
{
    m_source->removeAudioSampleObserver(*this);
    m_source->removeObserver(*this);
    m_source->stop();
}

#if PLATFORM(COCOA)
bool SpeechRecognitionCaptureSourceImpl::updateDataSource(const CAAudioStreamDescription& audioDescription)
{
    if (!m_dataSourceLock.tryLock())
        return false;

    Locker locker { AdoptLock, m_dataSourceLock };

    auto dataSource = AudioSampleDataSource::create(audioDescription.sampleRate() * 1, m_source.get());
    if (dataSource->setInputFormat(audioDescription)) {
        callOnMainThread([this, weakThis = WeakPtr { *this }] {
            if (weakThis)
                m_stateUpdateCallback(SpeechRecognitionUpdate::createError(m_clientIdentifier, SpeechRecognitionError { SpeechRecognitionErrorType::AudioCapture, "Unable to set input format"_s }));
        });
        return false;
    }

    if (dataSource->setOutputFormat(audioDescription)) {
        callOnMainThread([this, weakThis = WeakPtr { *this }] {
            if (weakThis)
                m_stateUpdateCallback(SpeechRecognitionUpdate::createError(m_clientIdentifier, SpeechRecognitionError { SpeechRecognitionErrorType::AudioCapture, "Unable to set output format"_s }));
        });
        return false;
    }

    m_dataSource = WTFMove(dataSource);
    return true;
}

void SpeechRecognitionCaptureSourceImpl::pullSamplesAndCallDataCallback(const MediaTime& time, const CAAudioStreamDescription& audioDescription, size_t sampleCount)
{
    ASSERT(isMainThread());

    auto data = WebAudioBufferList { audioDescription, sampleCount };
    {
        Locker locker { m_dataSourceLock };
        m_dataSource->pullSamples(*data.list(), sampleCount, time.timeValue(), 0, AudioSampleDataSource::Copy);
    }

    m_dataCallback(time, data, audioDescription, sampleCount);
}
#endif

// FIXME: It is unclear why it is safe to use m_dataSource without locking in this function.
void SpeechRecognitionCaptureSourceImpl::audioSamplesAvailable(const MediaTime& time, const PlatformAudioData& data, const AudioStreamDescription& description, size_t sampleCount) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
#if PLATFORM(COCOA)
    DisableMallocRestrictionsForCurrentThreadScope scope;

    ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
    auto audioDescription = toCAAudioStreamDescription(description);
    if (!m_dataSource || !m_dataSource->inputDescription() || *m_dataSource->inputDescription() != description) {
        if (!updateDataSource(audioDescription))
            return;
    }

    m_dataSource->pushSamples(time, data, sampleCount);

    callOnMainThread([weakThis = WeakPtr { *this }, time, audioDescription, sampleCount] {
        if (weakThis)
            weakThis->pullSamplesAndCallDataCallback(time, audioDescription, sampleCount);
    });
#else
    m_dataCallback(time, data, description, sampleCount);
#endif
}

void SpeechRecognitionCaptureSourceImpl::sourceStarted()
{
    ASSERT(isMainThread());
    m_stateUpdateCallback(SpeechRecognitionUpdate::create(m_clientIdentifier, SpeechRecognitionUpdateType::AudioStart));
}

void SpeechRecognitionCaptureSourceImpl::sourceStopped()
{
    ASSERT(isMainThread());
    ASSERT(m_source->captureDidFail());
    m_stateUpdateCallback(SpeechRecognitionUpdate::createError(m_clientIdentifier, SpeechRecognitionError { SpeechRecognitionErrorType::AudioCapture, "Source is stopped"_s }));
}

void SpeechRecognitionCaptureSourceImpl::sourceMutedChanged()
{
    ASSERT(isMainThread());
    m_stateUpdateCallback(SpeechRecognitionUpdate::createError(m_clientIdentifier, SpeechRecognitionError { SpeechRecognitionErrorType::AudioCapture, "Source is muted"_s }));
}

void SpeechRecognitionCaptureSourceImpl::mute()
{
    m_source->setMuted(true);
}

} // namespace WebCore

#endif
