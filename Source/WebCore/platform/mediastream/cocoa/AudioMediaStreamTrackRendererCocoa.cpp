/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include "AudioMediaStreamTrackRendererCocoa.h"

#if ENABLE(MEDIA_STREAM)

#include "AudioMediaStreamTrackRendererUnit.h"
#include "AudioSampleDataSource.h"
#include "CAAudioStreamDescription.h"
#include "LibWebRTCAudioModule.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

AudioMediaStreamTrackRendererCocoa::AudioMediaStreamTrackRendererCocoa(Init&& init)
    : AudioMediaStreamTrackRenderer(WTFMove(init))
    , m_resetObserver([this] { reset(); })
{
}

AudioMediaStreamTrackRendererCocoa::~AudioMediaStreamTrackRendererCocoa()
{
    ASSERT(!m_registeredDataSource);
}

void AudioMediaStreamTrackRendererCocoa::start(CompletionHandler<void()>&& callback)
{
    clear();

    AudioMediaStreamTrackRendererUnit::singleton().retrieveFormatDescription([weakThis = WeakPtr { *this }, callback = WTFMove(callback)](auto formatDescription) mutable {
        if (weakThis && formatDescription) {
            weakThis->m_outputDescription = *formatDescription;
            weakThis->m_shouldRecreateDataSource = true;
        }
        callback();
    });
}

BaseAudioMediaStreamTrackRendererUnit& AudioMediaStreamTrackRendererCocoa::rendererUnit()
{
    if (auto* audioModule = this->audioModule())
        return audioModule->incomingAudioMediaStreamTrackRendererUnit();
    return AudioMediaStreamTrackRendererUnit::singleton();
}

void AudioMediaStreamTrackRendererCocoa::stop()
{
    ASSERT(isMainThread());

    if (m_registeredDataSource)
        rendererUnit().removeSource(*m_registeredDataSource);
}

void AudioMediaStreamTrackRendererCocoa::clear()
{
    stop();

    setRegisteredDataSource(nullptr);
    m_outputDescription = std::nullopt;
}

void AudioMediaStreamTrackRendererCocoa::setVolume(float volume)
{
    ASSERT(isMainThread());

    AudioMediaStreamTrackRenderer::setVolume(volume);
    if (m_registeredDataSource)
        m_registeredDataSource->setVolume(volume);
}

void AudioMediaStreamTrackRendererCocoa::reset()
{
    ASSERT(isMainThread());

    if (m_registeredDataSource)
        m_registeredDataSource->recomputeSampleOffset();
}

void AudioMediaStreamTrackRendererCocoa::setAudioOutputDevice(const String& deviceId)
{
    // FIXME: We should create a unit for ourselves here or use the default unit if deviceId is matching.
    rendererUnit().setAudioOutputDevice(deviceId);
    m_shouldRecreateDataSource = true;
}

void AudioMediaStreamTrackRendererCocoa::setRegisteredDataSource(RefPtr<AudioSampleDataSource>&& source)
{
    ASSERT(isMainThread());

    if (m_registeredDataSource)
        rendererUnit().removeSource(*m_registeredDataSource);

    if (!m_outputDescription)
        return;

    m_registeredDataSource = WTFMove(source);
    if (!m_registeredDataSource)
        return;

    m_registeredDataSource->setLogger(logger(), logIdentifier());
    m_registeredDataSource->setVolume(volume());
    rendererUnit().addResetObserver(m_resetObserver);
    rendererUnit().addSource(*m_registeredDataSource);
}

static unsigned pollSamplesCount()
{
#if USE(LIBWEBRTC)
    return LibWebRTCAudioModule::PollSamplesCount + 1;
#else
    return 2;
#endif
}

void AudioMediaStreamTrackRendererCocoa::pushSamples(const MediaTime& sampleTime, const PlatformAudioData& audioData, const AudioStreamDescription& description, size_t sampleCount)
{
    ASSERT(!isMainThread());
    ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
    if (!m_dataSource || m_shouldRecreateDataSource || !m_dataSource->inputDescription() || *m_dataSource->inputDescription() != description) {
        DisableMallocRestrictionsForCurrentThreadScope scope;

        // FIXME: For non libwebrtc sources, we can probably reduce poll samples count to 2.
        
        auto dataSource = AudioSampleDataSource::create(description.sampleRate() * 0.5, *this, pollSamplesCount());

        if (dataSource->setInputFormat(toCAAudioStreamDescription(description))) {
            ERROR_LOG(LOGIDENTIFIER, "Unable to set the input format of data source");
            return;
        }

        if (!m_outputDescription || dataSource->setOutputFormat(*m_outputDescription)) {
            ERROR_LOG(LOGIDENTIFIER, "Unable to set the output format of data source");
            return;
        }

        callOnMainThread([weakThis = WeakPtr { *this }, newSource = dataSource]() mutable {
            if (weakThis)
                weakThis->setRegisteredDataSource(WTFMove(newSource));
        });
        m_dataSource = WTFMove(dataSource);
        m_shouldRecreateDataSource = false;
    }

    m_dataSource->pushSamples(sampleTime, audioData, sampleCount);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
