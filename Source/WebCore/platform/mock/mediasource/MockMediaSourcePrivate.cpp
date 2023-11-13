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
#include "Logging.h"
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
#if !RELEASE_LOG_DISABLED
    , m_logger(m_player.mediaPlayerLogger())
    , m_logIdentifier(m_player.mediaPlayerLogIdentifier())
#endif
{
#if !RELEASE_LOG_DISABLED
    client.setLogIdentifier(m_player.mediaPlayerLogIdentifier());
#endif
}

MockMediaSourcePrivate::~MockMediaSourcePrivate() = default;

MediaSourcePrivate::AddStatus MockMediaSourcePrivate::addSourceBuffer(const ContentType& contentType, bool, RefPtr<SourceBufferPrivate>& outPrivate)
{
    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = contentType;
    if (MockMediaPlayerMediaSource::supportsType(parameters) == MediaPlayer::SupportsType::IsNotSupported)
        return AddStatus::NotSupported;

    m_sourceBuffers.append(MockSourceBufferPrivate::create(*this));
    outPrivate = m_sourceBuffers.last();

    return AddStatus::Ok;
}

MediaTime MockMediaSourcePrivate::duration() const
{
    if (m_client)
        return m_client->duration();
    return MediaTime::invalidTime();
}

const PlatformTimeRanges& MockMediaSourcePrivate::buffered()
{
    if (m_client)
        return m_client->buffered();
    return PlatformTimeRanges::emptyRanges();
}

void MockMediaSourcePrivate::durationChanged(const MediaTime& duration)
{
    m_player.updateDuration(duration);
}

void MockMediaSourcePrivate::markEndOfStream(EndOfStreamStatus status)
{
    if (status == EosNoError)
        m_player.setNetworkState(MediaPlayer::NetworkState::Loaded);
    MediaSourcePrivate::markEndOfStream(status);
}

MediaPlayer::ReadyState MockMediaSourcePrivate::readyState() const
{
    return m_player.readyState();
}

void MockMediaSourcePrivate::setReadyState(MediaPlayer::ReadyState readyState)
{
    m_player.setReadyState(readyState);
}

void MockMediaSourcePrivate::notifyActiveSourceBuffersChanged()
{
    m_player.notifyActiveSourceBuffersChanged();
}

Ref<MediaTimePromise> MockMediaSourcePrivate::waitForTarget(const SeekTarget& target)
{
    if (!m_client)
        return MediaTimePromise::createAndReject(PlatformMediaError::ClientDisconnected);
    return m_client->waitForTarget(target);
}

Ref<MediaPromise> MockMediaSourcePrivate::seekToTime(const MediaTime& time)
{
    if (!m_client)
        return MediaPromise::createAndReject(PlatformMediaError::ClientDisconnected);
    return m_client->seekToTime(time);
}

MediaTime MockMediaSourcePrivate::currentMediaTime() const
{
    return m_player.currentMediaTime();
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

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MockMediaSourcePrivate::logChannel() const
{
    return LogMediaSource;
}
#endif

}

#endif
