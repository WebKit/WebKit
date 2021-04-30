/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

namespace WebCore {

AudioMediaStreamTrackRendererCocoa::AudioMediaStreamTrackRendererCocoa() = default;

AudioMediaStreamTrackRendererCocoa::~AudioMediaStreamTrackRendererCocoa() = default;

void AudioMediaStreamTrackRendererCocoa::start()
{
    clear();

    if (auto* formatDescription = AudioMediaStreamTrackRendererUnit::singleton().formatDescription())
        m_outputDescription = makeUnique<CAAudioStreamDescription>(*formatDescription);
}

void AudioMediaStreamTrackRendererCocoa::stop()
{
    if (m_dataSource)
        AudioMediaStreamTrackRendererUnit::singleton().removeSource(*m_dataSource);
}

void AudioMediaStreamTrackRendererCocoa::clear()
{
    stop();

    m_dataSource = nullptr;
    m_outputDescription = { };
}

void AudioMediaStreamTrackRendererCocoa::setVolume(float volume)
{
    AudioMediaStreamTrackRenderer::setVolume(volume);
    if (m_dataSource)
        m_dataSource->setVolume(volume);
}

void AudioMediaStreamTrackRendererCocoa::setAudioOutputDevice(const String& deviceId)
{
    // FIXME: We should create a unit for ourselves here or use the default unit if deviceId is matching.
    AudioMediaStreamTrackRendererUnit::singleton().setAudioOutputDevice(deviceId);
    m_shouldReset = true;
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
    if (!m_dataSource || m_shouldReset || !m_dataSource->inputDescription() || *m_dataSource->inputDescription() != description) {
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

        callOnMainThread([this, weakThis = makeWeakPtr(this), oldSource = m_dataSource, newSource = dataSource]() mutable {
            if (!weakThis)
                return;

            if (oldSource)
                AudioMediaStreamTrackRendererUnit::singleton().removeSource(*oldSource);

            newSource->setVolume(volume());
            AudioMediaStreamTrackRendererUnit::singleton().addSource(WTFMove(newSource));
        });
        m_dataSource = WTFMove(dataSource);
        m_shouldReset = false;
    }

    m_dataSource->pushSamples(sampleTime, audioData, sampleCount);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
