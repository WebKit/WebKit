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
#import "MediaStreamTrackAudioSourceProviderCocoa.h"

#if ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)

#import "LibWebRTCAudioModule.h"

namespace WebCore {

Ref<MediaStreamTrackAudioSourceProviderCocoa> MediaStreamTrackAudioSourceProviderCocoa::create(MediaStreamTrackPrivate& source)
{
    return adoptRef(*new MediaStreamTrackAudioSourceProviderCocoa(source));
}

MediaStreamTrackAudioSourceProviderCocoa::MediaStreamTrackAudioSourceProviderCocoa(MediaStreamTrackPrivate& source)
    : m_captureSource(makeWeakPtr(source))
    , m_source(source.source())
{
#if USE(LIBWEBRTC)
    if (m_source->isIncomingAudioSource())
        setPollSamplesCount(LibWebRTCAudioModule::PollSamplesCount + 1);
#endif
}

MediaStreamTrackAudioSourceProviderCocoa::~MediaStreamTrackAudioSourceProviderCocoa()
{
    ASSERT(!m_connected);
    m_source->removeAudioSampleObserver(*this);
}

void MediaStreamTrackAudioSourceProviderCocoa::hasNewClient(AudioSourceProviderClient* client)
{
    bool shouldBeConnected = !!client;
    if (m_connected == shouldBeConnected)
        return;

    m_connected = shouldBeConnected;
    if (!client) {
        m_captureSource->removeObserver(*this);
        m_source->removeAudioSampleObserver(*this);
        return;
    }

    m_enabled = m_captureSource->enabled();
    m_captureSource->addObserver(*this);
    m_source->addAudioSampleObserver(*this);
}

void MediaStreamTrackAudioSourceProviderCocoa::trackEnabledChanged(MediaStreamTrackPrivate& track)
{
    m_enabled = track.enabled();
}

// May get called on a background thread.
void MediaStreamTrackAudioSourceProviderCocoa::audioSamplesAvailable(const MediaTime&, const PlatformAudioData& data, const AudioStreamDescription& description, size_t frameCount)
{
    if (!m_enabled)
        return;

    receivedNewAudioSamples(data, description, frameCount);
}

}

#endif // ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)
