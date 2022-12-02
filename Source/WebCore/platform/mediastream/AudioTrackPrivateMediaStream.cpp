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

#if ENABLE(MEDIA_STREAM)

#include "AudioMediaStreamTrackRenderer.h"
#include "Logging.h"
#include "RealtimeIncomingAudioSource.h"

#if USE(LIBWEBRTC)
#include "LibWebRTCAudioModule.h"
#endif

namespace WebCore {

AudioTrackPrivateMediaStream::AudioTrackPrivateMediaStream(MediaStreamTrackPrivate& track)
    : m_streamTrack(track)
    , m_audioSource(track.source())
    , m_id(track.id())
    , m_label(track.label())
    , m_renderer(createRenderer(*this))
{
    track.addObserver(*this);
}

AudioTrackPrivateMediaStream::~AudioTrackPrivateMediaStream()
{
    clear();
}

#if USE(LIBWEBRTC)
static RefPtr<LibWebRTCAudioModule> audioModuleFromSource(RealtimeMediaSource& source)
{
    if (!is<RealtimeIncomingAudioSource>(source))
        return nullptr;
    return downcast<RealtimeIncomingAudioSource>(source).audioModule();
}
#endif

std::unique_ptr<AudioMediaStreamTrackRenderer> AudioTrackPrivateMediaStream::createRenderer(AudioTrackPrivateMediaStream& stream)
{
    auto& track = stream.m_streamTrack.get();
    return AudioMediaStreamTrackRenderer::create(AudioMediaStreamTrackRenderer::Init {
        [stream = WeakPtr { stream }] {
            if (stream)
                stream->createNewRenderer();
        }
#if USE(LIBWEBRTC)
        , audioModuleFromSource(stream.m_audioSource.get())
#endif
#if !RELEASE_LOG_DISABLED
        , track.logger()
        , track.logIdentifier()
#endif
    });
}

void AudioTrackPrivateMediaStream::clear()
{
    if (m_isCleared)
        return;

    m_isCleared = true;

    if (m_isPlaying)
        m_audioSource->removeAudioSampleObserver(*this);

    streamTrack().removeObserver(*this);
    if (m_renderer)
        m_renderer->clear();
}

void AudioTrackPrivateMediaStream::play()
{
    if (m_shouldPlay)
        return;

    m_shouldPlay = true;
    updateRenderer();
}

void AudioTrackPrivateMediaStream::pause()
{
    m_shouldPlay = false;
    updateRenderer();
}

void AudioTrackPrivateMediaStream::setMuted(bool muted)
{
    m_muted = muted;
    updateRenderer();
}

void AudioTrackPrivateMediaStream::setVolume(float volume)
{
    if (m_renderer)
        m_renderer->setVolume(volume);
    updateRenderer();
}

void AudioTrackPrivateMediaStream::setAudioOutputDevice(const String& deviceId)
{
    if (m_renderer)
        m_renderer->setAudioOutputDevice(deviceId);
}

float AudioTrackPrivateMediaStream::volume() const
{
    if (m_renderer)
        return m_renderer->volume();
    return 1;
}

// May get called on a background thread.
void AudioTrackPrivateMediaStream::audioSamplesAvailable(const MediaTime& sampleTime, const PlatformAudioData& audioData, const AudioStreamDescription& description, size_t sampleCount)
{
    if (m_renderer)
        m_renderer->pushSamples(sampleTime, audioData, description, sampleCount);
}

void AudioTrackPrivateMediaStream::trackMutedChanged(MediaStreamTrackPrivate&)
{
    updateRenderer();
}

void AudioTrackPrivateMediaStream::trackEnabledChanged(MediaStreamTrackPrivate&)
{
    updateRenderer();
}

void AudioTrackPrivateMediaStream::trackEnded(MediaStreamTrackPrivate&)
{
    updateRenderer();
}

void AudioTrackPrivateMediaStream::updateRenderer()
{
    if (!m_shouldPlay || !volume() || m_muted || streamTrack().muted() || streamTrack().ended() || !streamTrack().enabled()) {
        stopRenderer();
        return;
    }
    startRenderer();
}

void AudioTrackPrivateMediaStream::startRenderer()
{
    ASSERT(isMainThread());
    if (m_isPlaying || !m_renderer)
        return;

    m_isPlaying = true;
    m_renderer->start([protectedThis = Ref { *this }] {
        if (protectedThis->m_isPlaying)
            protectedThis->m_audioSource->addAudioSampleObserver(protectedThis.get());
    });
}

void AudioTrackPrivateMediaStream::stopRenderer()
{
    ASSERT(isMainThread());
    if (!m_isPlaying)
        return;

    m_isPlaying = false;
    m_audioSource->removeAudioSampleObserver(*this);
    if (m_renderer)
        m_renderer->stop();
}

void AudioTrackPrivateMediaStream::createNewRenderer()
{
    bool isPlaying = m_isPlaying;
    stopRenderer();

    float volume = this->volume();
    m_renderer = createRenderer(*this);
    if (m_renderer)
        m_renderer->setVolume(volume);

    if (isPlaying)
        startRenderer();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
