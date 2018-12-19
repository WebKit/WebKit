/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "MockMediaSourcePrivate.h"

#if ENABLE(MEDIA_SOURCE)

#include "ContentType.h"
#include "MediaSourcePrivateClient.h"
#include "MockMediaPlayerMediaSource.h"
#include "MockSourceBufferPrivate.h"

namespace WebCore {

Ref<MockMediaSourcePrivate> MockMediaSourcePrivate::create(MockMediaPlayerMediaSource& parent, MediaSourcePrivateClient& client)
{
    auto source = adoptRef(*new MockMediaSourcePrivate(parent, client));
    client.setPrivateAndOpen(source.copyRef());
    return source;
}

MockMediaSourcePrivate::MockMediaSourcePrivate(MockMediaPlayerMediaSource& parent, MediaSourcePrivateClient& client)
    : m_player(parent)
    , m_client(client)
{
}

MockMediaSourcePrivate::~MockMediaSourcePrivate()
{
    for (auto& buffer : m_sourceBuffers)
        buffer->clearMediaSource();
}

MediaSourcePrivate::AddStatus MockMediaSourcePrivate::addSourceBuffer(const ContentType& contentType, RefPtr<SourceBufferPrivate>& outPrivate)
{
    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = contentType;
    if (MockMediaPlayerMediaSource::supportsType(parameters) == MediaPlayer::IsNotSupported)
        return NotSupported;

    m_sourceBuffers.append(MockSourceBufferPrivate::create(this));
    outPrivate = m_sourceBuffers.last();

    return Ok;
}

void MockMediaSourcePrivate::removeSourceBuffer(SourceBufferPrivate* buffer)
{
    ASSERT(m_sourceBuffers.contains(buffer));
    m_activeSourceBuffers.removeFirst(buffer);
    m_sourceBuffers.removeFirst(buffer);
}

MediaTime MockMediaSourcePrivate::duration()
{
    return m_client->duration();
}

std::unique_ptr<PlatformTimeRanges> MockMediaSourcePrivate::buffered()
{
    return m_client->buffered();
}

void MockMediaSourcePrivate::durationChanged()
{
    m_player.updateDuration(duration());
}

void MockMediaSourcePrivate::markEndOfStream(EndOfStreamStatus status)
{
    if (status == EosNoError)
        m_player.setNetworkState(MediaPlayer::Loaded);
    m_isEnded = true;
}

void MockMediaSourcePrivate::unmarkEndOfStream()
{
    m_isEnded = false;
}

MediaPlayer::ReadyState MockMediaSourcePrivate::readyState() const
{
    return m_player.readyState();
}

void MockMediaSourcePrivate::setReadyState(MediaPlayer::ReadyState readyState)
{
    m_player.setReadyState(readyState);
}

void MockMediaSourcePrivate::waitForSeekCompleted()
{
    m_player.waitForSeekCompleted();
}

void MockMediaSourcePrivate::seekCompleted()
{
    m_player.seekCompleted();
}

void MockMediaSourcePrivate::sourceBufferPrivateDidChangeActiveState(MockSourceBufferPrivate* buffer, bool active)
{
    if (active && !m_activeSourceBuffers.contains(buffer))
        m_activeSourceBuffers.append(buffer);

    if (!active)
        m_activeSourceBuffers.removeFirst(buffer);
}

static bool MockSourceBufferPrivateHasAudio(MockSourceBufferPrivate* sourceBuffer)
{
    return sourceBuffer->hasAudio();
}

bool MockMediaSourcePrivate::hasAudio() const
{
    return std::any_of(m_activeSourceBuffers.begin(), m_activeSourceBuffers.end(), MockSourceBufferPrivateHasAudio);
}

static bool MockSourceBufferPrivateHasVideo(MockSourceBufferPrivate* sourceBuffer)
{
    return sourceBuffer->hasVideo();
}

bool MockMediaSourcePrivate::hasVideo() const
{
    return std::any_of(m_activeSourceBuffers.begin(), m_activeSourceBuffers.end(), MockSourceBufferPrivateHasVideo);
}

void MockMediaSourcePrivate::seekToTime(const MediaTime& time)
{
    m_client->seekToTime(time);
}

MediaTime MockMediaSourcePrivate::seekToTime(const MediaTime& targetTime, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold)
{
    MediaTime seekTime = targetTime;
    for (auto& buffer : m_activeSourceBuffers) {
        MediaTime sourceSeekTime = buffer->fastSeekTimeForMediaTime(targetTime, negativeThreshold, positiveThreshold);
        if (abs(targetTime - sourceSeekTime) > abs(targetTime - seekTime))
            seekTime = sourceSeekTime;
    }

    return seekTime;
}

std::optional<VideoPlaybackQualityMetrics> MockMediaSourcePrivate::videoPlaybackQualityMetrics()
{
    return VideoPlaybackQualityMetrics {
        m_totalVideoFrames,
        m_droppedVideoFrames,
        m_corruptedVideoFrames,
        m_totalFrameDelay.toDouble(),
        0,
    };
}

}

#endif
