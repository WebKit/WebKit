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
#include "AudioTrackPrivateMediaStream.h"

#if ENABLE(VIDEO_TRACK) && ENABLE(MEDIA_STREAM)

#include "AudioMediaStreamTrackRenderer.h"
#include "Logging.h"

namespace WebCore {

AudioTrackPrivateMediaStream::AudioTrackPrivateMediaStream(MediaStreamTrackPrivate& track)
    : m_streamTrack(track)
    , m_id(track.id())
    , m_label(track.label())
    , m_timelineOffset(MediaTime::invalidTime())
    , m_renderer { AudioMediaStreamTrackRenderer::create() }
{
    track.addObserver(*this);
}

AudioTrackPrivateMediaStream::~AudioTrackPrivateMediaStream()
{
    clear();
}

#if !RELEASE_LOG_DISABLED
void AudioTrackPrivateMediaStream::setLogger(const Logger& logger, const void* identifier)
{
    TrackPrivateBase::setLogger(logger, identifier);
    m_renderer->setLogger(logger, identifier);
}
#endif

void AudioTrackPrivateMediaStream::clear()
{
    if (m_isCleared)
        return;

    m_isCleared = true;
    streamTrack().removeObserver(*this);

    m_renderer->clear();
}

void AudioTrackPrivateMediaStream::playInternal()
{
    ASSERT(isMainThread());

    if (m_isPlaying)
        return;

    m_isPlaying = true;
    m_autoPlay = false;

    m_renderer->start();
}

void AudioTrackPrivateMediaStream::play()
{
    playInternal();
}

void AudioTrackPrivateMediaStream::pause()
{
    ASSERT(isMainThread());

    if (!m_isPlaying)
        return;

    m_isPlaying = false;
    m_autoPlay = false;

    m_renderer->stop();
}

void AudioTrackPrivateMediaStream::setVolume(float volume)
{
    m_renderer->setVolume(volume);
}

float AudioTrackPrivateMediaStream::volume() const
{
    return m_renderer->volume();
}

// May get called on a background thread.
void AudioTrackPrivateMediaStream::audioSamplesAvailable(MediaStreamTrackPrivate&, const MediaTime& sampleTime, const PlatformAudioData& audioData, const AudioStreamDescription& description, size_t sampleCount)
{
    if (!m_isPlaying) {
        m_renderer->stop();
        return;
    }

    m_renderer->pushSamples(sampleTime, audioData, description, sampleCount);

    if (m_autoPlay && !m_hasStartedAutoplay) {
        m_hasStartedAutoplay = true;
        callOnMainThread([this, protectedThis = makeRef(*this)] {
            if (m_autoPlay)
                playInternal();
        });
    }
}

void AudioTrackPrivateMediaStream::trackMutedChanged(MediaStreamTrackPrivate&)
{
    updateRendererMutedState();
}

void AudioTrackPrivateMediaStream::trackEnabledChanged(MediaStreamTrackPrivate&)
{
    updateRendererMutedState();
}

void AudioTrackPrivateMediaStream::updateRendererMutedState()
{
    m_renderer->setMuted(streamTrack().muted() || streamTrack().ended() || !streamTrack().enabled());
}

void AudioTrackPrivateMediaStream::trackEnded(MediaStreamTrackPrivate&)
{
    pause();
}

} // namespace WebCore

#endif // ENABLE(VIDEO_TRACK) && ENABLE(MEDIA_STREAM)
