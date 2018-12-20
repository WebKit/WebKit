/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
#include "MockMediaPlayerMediaSource.h"

#if ENABLE(MEDIA_SOURCE)

#include "MediaPlayer.h"
#include "MediaSourcePrivateClient.h"
#include "MockMediaSourcePrivate.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// MediaPlayer Enigne Support
void MockMediaPlayerMediaSource::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar([] (MediaPlayer* player) { return std::make_unique<MockMediaPlayerMediaSource>(player); }, getSupportedTypes,
        supportsType, 0, 0, 0, 0);
}

// FIXME: What does the word "cache" mean here?
static const HashSet<String, ASCIICaseInsensitiveHash>& mimeTypeCache()
{
    static const auto cache = makeNeverDestroyed(HashSet<String, ASCIICaseInsensitiveHash> {
        "video/mock",
        "audio/mock",
    });
    return cache;
}

void MockMediaPlayerMediaSource::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& supportedTypes)
{
    supportedTypes = mimeTypeCache();
}

MediaPlayer::SupportsType MockMediaPlayerMediaSource::supportsType(const MediaEngineSupportParameters& parameters)
{
    if (!parameters.isMediaSource)
        return MediaPlayer::IsNotSupported;

    auto containerType = parameters.type.containerType();
    if (containerType.isEmpty() || !mimeTypeCache().contains(containerType))
        return MediaPlayer::IsNotSupported;

    auto codecs = parameters.type.parameter(ContentType::codecsParameter());
    if (codecs.isEmpty())
        return MediaPlayer::MayBeSupported;

    if (codecs == "mock" || codecs == "kcom")
        return MediaPlayer::IsSupported;

    return MediaPlayer::MayBeSupported;
}

MockMediaPlayerMediaSource::MockMediaPlayerMediaSource(MediaPlayer* player)
    : m_player(player)
    , m_currentTime(MediaTime::zeroTime())
    , m_readyState(MediaPlayer::HaveNothing)
    , m_networkState(MediaPlayer::Empty)
    , m_playing(false)
    , m_seekCompleted(true)
{
}

MockMediaPlayerMediaSource::~MockMediaPlayerMediaSource() = default;

void MockMediaPlayerMediaSource::load(const String&)
{
    ASSERT_NOT_REACHED();
}

void MockMediaPlayerMediaSource::load(const String&, MediaSourcePrivateClient* source)
{
    m_mediaSourcePrivate = MockMediaSourcePrivate::create(*this, *source);
}

void MockMediaPlayerMediaSource::cancelLoad()
{
}

void MockMediaPlayerMediaSource::play()
{
    m_playing = 1;
    callOnMainThread([this] {
        advanceCurrentTime();
    });
}

void MockMediaPlayerMediaSource::pause()
{
    m_playing = 0;
}

FloatSize MockMediaPlayerMediaSource::naturalSize() const
{
    return FloatSize();
}

bool MockMediaPlayerMediaSource::hasVideo() const
{
    return m_mediaSourcePrivate ? m_mediaSourcePrivate->hasVideo() : false;
}

bool MockMediaPlayerMediaSource::hasAudio() const
{
    return m_mediaSourcePrivate ? m_mediaSourcePrivate->hasAudio() : false;
}

void MockMediaPlayerMediaSource::setVisible(bool)
{
}

bool MockMediaPlayerMediaSource::seeking() const
{
    return !m_seekCompleted;
}

bool MockMediaPlayerMediaSource::paused() const
{
    return !m_playing;
}

MediaPlayer::NetworkState MockMediaPlayerMediaSource::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MockMediaPlayerMediaSource::readyState() const
{
    return m_readyState;
}

MediaTime MockMediaPlayerMediaSource::maxMediaTimeSeekable() const
{
    return m_duration;
}

std::unique_ptr<PlatformTimeRanges> MockMediaPlayerMediaSource::buffered() const
{
    if (m_mediaSourcePrivate)
        return m_mediaSourcePrivate->buffered();

    return std::make_unique<PlatformTimeRanges>();
}

bool MockMediaPlayerMediaSource::didLoadingProgress() const
{
    return false;
}

void MockMediaPlayerMediaSource::setSize(const IntSize&)
{
}

void MockMediaPlayerMediaSource::paint(GraphicsContext&, const FloatRect&)
{
}

MediaTime MockMediaPlayerMediaSource::currentMediaTime() const
{
    return m_currentTime;
}

MediaTime MockMediaPlayerMediaSource::durationMediaTime() const
{
    return m_mediaSourcePrivate ? m_mediaSourcePrivate->duration() : MediaTime::zeroTime();
}

void MockMediaPlayerMediaSource::seekWithTolerance(const MediaTime& time, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance)
{
    if (!negativeTolerance && !positiveTolerance) {
        m_currentTime = time;
        m_mediaSourcePrivate->seekToTime(time);
    } else
        m_currentTime = m_mediaSourcePrivate->seekToTime(time, negativeTolerance, positiveTolerance);

    if (m_seekCompleted) {
        m_player->timeChanged();

        if (m_playing)
            callOnMainThread([this] {
                advanceCurrentTime();
            });
    }
}

void MockMediaPlayerMediaSource::advanceCurrentTime()
{
    if (!m_mediaSourcePrivate)
        return;

    auto buffered = m_mediaSourcePrivate->buffered();
    size_t pos = buffered->find(m_currentTime);
    if (pos == notFound)
        return;

    bool ignoreError;
    m_currentTime = std::min(m_duration, buffered->end(pos, ignoreError));
    m_player->timeChanged();
}

void MockMediaPlayerMediaSource::updateDuration(const MediaTime& duration)
{
    if (m_duration == duration)
        return;

    m_duration = duration;
    m_player->durationChanged();
}

void MockMediaPlayerMediaSource::setReadyState(MediaPlayer::ReadyState readyState)
{
    if (readyState == m_readyState)
        return;

    m_readyState = readyState;
    m_player->readyStateChanged();
}

void MockMediaPlayerMediaSource::setNetworkState(MediaPlayer::NetworkState networkState)
{
    if (networkState == m_networkState)
        return;

    m_networkState = networkState;
    m_player->networkStateChanged();
}

void MockMediaPlayerMediaSource::waitForSeekCompleted()
{
    m_seekCompleted = false;
}

void MockMediaPlayerMediaSource::seekCompleted()
{
    if (m_seekCompleted)
        return;
    m_seekCompleted = true;

    m_player->timeChanged();

    if (m_playing)
        callOnMainThread([this] {
            advanceCurrentTime();
        });
}

Optional<VideoPlaybackQualityMetrics> MockMediaPlayerMediaSource::videoPlaybackQualityMetrics()
{
    return m_mediaSourcePrivate ? m_mediaSourcePrivate->videoPlaybackQualityMetrics() : WTF::nullopt;
}

}

#endif
