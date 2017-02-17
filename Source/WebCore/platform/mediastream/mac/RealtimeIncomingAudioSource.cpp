/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RealtimeIncomingAudioSource.h"

#if USE(LIBWEBRTC)

#include "RealtimeMediaSourceSettings.h"
#include "WebAudioSourceProviderAVFObjC.h"

#include "CoreMediaSoftLink.h"

namespace WebCore {

Ref<RealtimeIncomingAudioSource> RealtimeIncomingAudioSource::create(rtc::scoped_refptr<webrtc::AudioTrackInterface>&& audioTrack, String&& audioTrackId)
{
    return adoptRef(*new RealtimeIncomingAudioSource(WTFMove(audioTrack), WTFMove(audioTrackId)));
}

RealtimeIncomingAudioSource::RealtimeIncomingAudioSource(rtc::scoped_refptr<webrtc::AudioTrackInterface>&& audioTrack, String&& audioTrackId)
    : RealtimeMediaSource(WTFMove(audioTrackId), RealtimeMediaSource::Type::Audio, String())
    , m_audioTrack(WTFMove(audioTrack))
{
}

void RealtimeIncomingAudioSource::OnData(const void* audioData, int bitsPerSample, int sampleRate, size_t numberOfChannels, size_t numberOfFrames)
{
    // FIXME: Implement this.
    UNUSED_PARAM(audioData);
    UNUSED_PARAM(bitsPerSample);
    UNUSED_PARAM(sampleRate);
    UNUSED_PARAM(numberOfChannels);
    UNUSED_PARAM(numberOfFrames);
}

void RealtimeIncomingAudioSource::startProducingData()
{
    if (m_isProducingData)
        return;

    m_isProducingData = true;
    if (m_audioTrack)
        m_audioTrack->AddSink(this);
}

void RealtimeIncomingAudioSource::stopProducingData()
{
    if (m_isProducingData)
        return;

    m_isProducingData = false;
    if (m_audioTrack)
        m_audioTrack->RemoveSink(this);
}


RefPtr<RealtimeMediaSourceCapabilities> RealtimeIncomingAudioSource::capabilities() const
{
    return m_capabilities;
}

const RealtimeMediaSourceSettings& RealtimeIncomingAudioSource::settings() const
{
    return m_currentSettings;
}

RealtimeMediaSourceSupportedConstraints& RealtimeIncomingAudioSource::supportedConstraints()
{
    return m_supportedConstraints;
}

AudioSourceProvider* RealtimeIncomingAudioSource::audioSourceProvider()
{
    if (!m_audioSourceProvider) {
        m_audioSourceProvider = WebAudioSourceProviderAVFObjC::create(*this);
        const auto* description = CMAudioFormatDescriptionGetStreamBasicDescription(m_formatDescription.get());
        m_audioSourceProvider->prepare(description);
    }

    return m_audioSourceProvider.get();
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
