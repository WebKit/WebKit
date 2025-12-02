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

#include "DestinationColorSpace.h"
#include "MediaPlayer.h"
#include "MediaSourcePrivate.h"
#include "MediaSourcePrivateClient.h"
#include "MockMediaSourcePrivate.h"
#include <wtf/MainThread.h>
#include <wtf/NativePromise.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class MediaPlayerFactoryMediaSourceMock final : public MediaPlayerFactory {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MediaPlayerFactoryMediaSourceMock);
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::MockMSE; };

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer& player) const final { return adoptRef(*new MockMediaPlayerMediaSource(player)); }

    void getSupportedTypes(HashSet<String>& types) const final
    {
        return MockMediaPlayerMediaSource::getSupportedTypes(types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return MockMediaPlayerMediaSource::supportsType(parameters);
    }
};

// MediaPlayer Enigne Support
void MockMediaPlayerMediaSource::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(makeUnique<MediaPlayerFactoryMediaSourceMock>());
}

// FIXME: What does the word "cache" mean here?
static const HashSet<String>& mimeTypeCache()
{
    static NeverDestroyed cache = HashSet<String> {
        "video/mock"_s,
        "audio/mock"_s,
    };
    return cache;
}

void MockMediaPlayerMediaSource::getSupportedTypes(HashSet<String>& supportedTypes)
{
    supportedTypes = mimeTypeCache();
}

MediaPlayer::SupportsType MockMediaPlayerMediaSource::supportsType(const MediaEngineSupportParameters& parameters)
{
    if (!parameters.isMediaSource)
        return MediaPlayer::SupportsType::IsNotSupported;

    auto containerType = parameters.type.containerType().convertToASCIILowercase();
    if (containerType.isEmpty() || !mimeTypeCache().contains(containerType))
        return MediaPlayer::SupportsType::IsNotSupported;

    auto codecs = parameters.type.parameter(ContentType::codecsParameter());
    if (codecs.isEmpty())
        return MediaPlayer::SupportsType::MayBeSupported;

    if (codecs == "mock"_s || codecs == "kcom"_s)
        return MediaPlayer::SupportsType::IsSupported;

    return MediaPlayer::SupportsType::MayBeSupported;
}

MockMediaPlayerMediaSource::MockMediaPlayerMediaSource(MediaPlayer& player)
    : m_player(player)
{
}

MockMediaPlayerMediaSource::~MockMediaPlayerMediaSource() = default;

void MockMediaPlayerMediaSource::load(const String&)
{
    ASSERT_NOT_REACHED();
}

void MockMediaPlayerMediaSource::load(const URL&, const LoadOptions&, MediaSourcePrivateClient& source)
{
    if (RefPtr mediaSourcePrivate = downcast<MockMediaSourcePrivate>(source.mediaSourcePrivate())) {
        mediaSourcePrivate->setPlayer(this);
        m_mediaSourcePrivate = WTFMove(mediaSourcePrivate);
        source.reOpen();
    } else
        m_mediaSourcePrivate = MockMediaSourcePrivate::create(*this, source);
}

void MockMediaPlayerMediaSource::cancelLoad()
{
}

void MockMediaPlayerMediaSource::play()
{
    m_playing = 1;
    callOnMainThread([protectedThis = Ref { *this }] {
        protectedThis->advanceCurrentTime();
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
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    return mediaSourcePrivate ? mediaSourcePrivate->hasVideo() : false;
}

bool MockMediaPlayerMediaSource::hasAudio() const
{
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    return mediaSourcePrivate ? mediaSourcePrivate->hasAudio() : false;
}

void MockMediaPlayerMediaSource::setPageIsVisible(bool)
{
}

bool MockMediaPlayerMediaSource::seeking() const
{
    return !!m_lastSeekTarget;
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
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    return mediaSourcePrivate ? mediaSourcePrivate->mediaPlayerReadyState() : MediaPlayer::ReadyState::HaveNothing;
}

void MockMediaPlayerMediaSource::readyStateFromMediaSourceChanged()
{
    assertIsMainThread();
    if (RefPtr player = m_player.get())
        player->readyStateChanged();
}

void MockMediaPlayerMediaSource::mediaSourceHasRetrievedAllData()
{
    setNetworkState(MediaPlayer::NetworkState::Loaded);
}

MediaTime MockMediaPlayerMediaSource::maxTimeSeekable() const
{
    return m_duration;
}

const PlatformTimeRanges& MockMediaPlayerMediaSource::buffered() const
{
    ASSERT_NOT_REACHED();
    return PlatformTimeRanges::emptyRanges();
}

bool MockMediaPlayerMediaSource::didLoadingProgress() const
{
    return false;
}

void MockMediaPlayerMediaSource::setPresentationSize(const IntSize&)
{
}

void MockMediaPlayerMediaSource::paint(GraphicsContext&, const FloatRect&)
{
}

MediaTime MockMediaPlayerMediaSource::currentTime() const
{
    return m_lastSeekTarget ? m_lastSeekTarget->time : m_currentTime;
}

bool MockMediaPlayerMediaSource::timeIsProgressing() const
{
    if (!m_playing)
        return false;
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    return mediaSourcePrivate && mediaSourcePrivate->hasFutureTime(currentTime());
}

void MockMediaPlayerMediaSource::notifyActiveSourceBuffersChanged()
{
    if (auto player = m_player.get())
        player->activeSourceBuffersChanged();
}

MediaTime MockMediaPlayerMediaSource::duration() const
{
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    return mediaSourcePrivate ? mediaSourcePrivate->duration() : MediaTime::zeroTime();
}

RefPtr<MockMediaSourcePrivate> MockMediaPlayerMediaSource::protectedMediaSourcePrivate()
{
    return m_mediaSourcePrivate;
}

void MockMediaPlayerMediaSource::seekToTarget(const SeekTarget& target)
{
    m_lastSeekTarget = target;
    protectedMediaSourcePrivate()->waitForTarget(target)->whenSettled(RunLoop::currentSingleton(), [weakThis = WeakPtr { this }](auto&& result) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !result)
            return;

        const auto seekTime = *result;
        protectedThis->protectedMediaSourcePrivate()->seekToTime(seekTime);
        protectedThis->m_lastSeekTarget.reset();
        protectedThis->m_currentTime = seekTime;

        if (RefPtr player = protectedThis->m_player.get()) {
            player->seeked(seekTime);
            player->timeChanged();
        }

        if (protectedThis->m_playing) {
            callOnMainThread([protectedThis = WTFMove(protectedThis)] {
                protectedThis->advanceCurrentTime();
            });
        }
    });
}

void MockMediaPlayerMediaSource::advanceCurrentTime()
{
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    if (!mediaSourcePrivate)
        return;

    auto buffered = mediaSourcePrivate->buffered();
    size_t pos = buffered.find(m_currentTime);
    if (pos == notFound)
        return;

    bool ignoreError;
    m_currentTime = std::min(m_duration, buffered.end(pos, ignoreError));
    if (auto player = m_player.get())
        player->timeChanged();
}

void MockMediaPlayerMediaSource::updateDuration(const MediaTime& duration)
{
    if (m_duration == duration)
        return;

    m_duration = duration;
    if (auto player = m_player.get())
        player->durationChanged();
}

void MockMediaPlayerMediaSource::setNetworkState(MediaPlayer::NetworkState networkState)
{
    if (networkState == m_networkState)
        return;

    m_networkState = networkState;
    if (auto player = m_player.get())
        player->networkStateChanged();
}

std::optional<VideoPlaybackQualityMetrics> MockMediaPlayerMediaSource::videoPlaybackQualityMetrics()
{
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    return mediaSourcePrivate ? mediaSourcePrivate->videoPlaybackQualityMetrics() : std::nullopt;
}

DestinationColorSpace MockMediaPlayerMediaSource::colorSpace()
{
    return DestinationColorSpace::SRGB();
}

}

#endif
