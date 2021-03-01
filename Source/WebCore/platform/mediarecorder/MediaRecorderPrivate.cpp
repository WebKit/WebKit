/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "MediaRecorderPrivate.h"

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamPrivate.h"

namespace WebCore {

constexpr unsigned LargeAudioBitRate = 192000;
constexpr unsigned LargeVideoBitRate = 10000000;

MediaRecorderPrivate::AudioVideoSelectedTracks MediaRecorderPrivate::selectTracks(MediaStreamPrivate& stream)
{
    AudioVideoSelectedTracks selectedTracks;
    stream.forEachTrack([&](auto& track) {
        if (track.ended())
            return;
        switch (track.type()) {
        case RealtimeMediaSource::Type::Video: {
            auto& settings = track.settings();
            if (!selectedTracks.videoTrack && settings.supportsWidth() && settings.supportsHeight())
                selectedTracks.videoTrack = &track;
            break;
        }
        case RealtimeMediaSource::Type::Audio:
            if (!selectedTracks.audioTrack)
                selectedTracks.audioTrack = &track;
            break;
        case RealtimeMediaSource::Type::None:
            break;
        }
    });
    return selectedTracks;
}

void MediaRecorderPrivate::checkTrackState(const MediaStreamTrackPrivate& track)
{
    if (&track.source() == m_audioSource.get()) {
        m_shouldMuteAudio = track.muted() || !track.enabled();
        return;
    }
    if (&track.source() == m_videoSource.get())
        m_shouldMuteVideo = track.muted() || !track.enabled();
}

void MediaRecorderPrivate::stop(CompletionHandler<void()>&& completionHandler)
{
    setAudioSource(nullptr);
    setVideoSource(nullptr);
    stopRecording(WTFMove(completionHandler));
}

void MediaRecorderPrivate::pause(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!m_pausedAudioSource);
    ASSERT(!m_pausedVideoSource);

    m_pausedAudioSource = m_audioSource;
    m_pausedVideoSource = m_videoSource;

    setAudioSource(nullptr);
    setVideoSource(nullptr);

    pauseRecording(WTFMove(completionHandler));
}

void MediaRecorderPrivate::resume(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(m_pausedAudioSource || m_pausedVideoSource);

    setAudioSource(WTFMove(m_pausedAudioSource));
    setVideoSource(WTFMove(m_pausedVideoSource));

    resumeRecording(WTFMove(completionHandler));
}

void MediaRecorderPrivate::updateOptions(MediaRecorderPrivateOptions& options)
{
    // FIXME: Add support for options.bitsPerSecond.
    if (!options.audioBitsPerSecond)
        options.audioBitsPerSecond = LargeAudioBitRate;
    if (!options.videoBitsPerSecond)
        options.videoBitsPerSecond = LargeVideoBitRate;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
