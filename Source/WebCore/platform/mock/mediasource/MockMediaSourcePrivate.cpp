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
    : MediaSourcePrivate(client)
    , m_player(parent)
#if !RELEASE_LOG_DISABLED
    , m_logger(m_player->mediaPlayerLogger())
    , m_logIdentifier(m_player->mediaPlayerLogIdentifier())
#endif
{
#if !RELEASE_LOG_DISABLED
    client.setLogIdentifier(m_player->mediaPlayerLogIdentifier());
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
    outPrivate->setMediaSourceDuration(duration());

    return AddStatus::Ok;
}

void MockMediaSourcePrivate::durationChanged(const MediaTime& duration)
{
    MediaSourcePrivate::durationChanged(duration);
    if (m_player)
        m_player->updateDuration(duration);
}

void MockMediaSourcePrivate::markEndOfStream(EndOfStreamStatus status)
{
    if (m_player && status == EndOfStreamStatus::NoError)
        m_player->setNetworkState(MediaPlayer::NetworkState::Loaded);
    MediaSourcePrivate::markEndOfStream(status);
}

MediaPlayer::ReadyState MockMediaSourcePrivate::mediaPlayerReadyState() const
{
    if (m_player)
        return m_player->readyState();
    return MediaPlayer::ReadyState::HaveNothing;
}

void MockMediaSourcePrivate::setMediaPlayerReadyState(MediaPlayer::ReadyState readyState)
{
    if (m_player)
        m_player->setReadyState(readyState);
}

void MockMediaSourcePrivate::notifyActiveSourceBuffersChanged()
{
    if (m_player)
        m_player->notifyActiveSourceBuffersChanged();
}

MediaTime MockMediaSourcePrivate::currentMediaTime() const
{
    if (m_player)
        return m_player->currentMediaTime();
    return MediaTime::invalidTime();
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
