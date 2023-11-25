/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef MockMediaPlayerMediaSource_h
#define MockMediaPlayerMediaSource_h

#if ENABLE(MEDIA_SOURCE)

#include "MediaPlayerPrivate.h"
#include <wtf/Logger.h>
#include <wtf/MediaTime.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class MediaSource;
class MockMediaSourcePrivate;

class MockMediaPlayerMediaSource final
    : public MediaPlayerPrivateInterface
    , public RefCounted<MockMediaPlayerMediaSource>
    , public CanMakeWeakPtr<MockMediaPlayerMediaSource> {
public:
    explicit MockMediaPlayerMediaSource(MediaPlayer*);

    // MediaPlayer Engine Support
    WEBCORE_EXPORT static void registerMediaEngine(MediaEngineRegistrar);
    static void getSupportedTypes(HashSet<String>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    virtual ~MockMediaPlayerMediaSource();

    void ref() final { RefCounted::ref(); }
    void deref() final { RefCounted::deref(); }

    void advanceCurrentTime();
    MediaTime currentMediaTime() const override;
    bool currentMediaTimeMayProgress() const override;
    void notifyActiveSourceBuffersChanged() final;
    void updateDuration(const MediaTime&);

    MediaPlayer::ReadyState readyState() const override;
    void setReadyState(MediaPlayer::ReadyState);
    void setNetworkState(MediaPlayer::NetworkState);

#if !RELEASE_LOG_DISABLED
    const void* mediaPlayerLogIdentifier() { return m_player.get()->mediaPlayerLogIdentifier(); }
    const Logger& mediaPlayerLogger() { return m_player.get()->mediaPlayerLogger(); }
#endif

private:
    // MediaPlayerPrivate Overrides
    void load(const String& url) override;
    void load(const URL&, const ContentType&, MediaSourcePrivateClient&) override;
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) override { }
#endif
    void cancelLoad() override;
    void play() override;
    void pause() override;
    FloatSize naturalSize() const override;
    bool hasVideo() const override;
    bool hasAudio() const override;
    void setPageIsVisible(bool, String&& sceneIdentifier) final;
    void seekToTarget(const SeekTarget&) final;
    bool seeking() const final;
    bool paused() const override;
    MediaPlayer::NetworkState networkState() const override;
    MediaTime maxMediaTimeSeekable() const override;
    const PlatformTimeRanges& buffered() const override;
    bool didLoadingProgress() const override;
    void setPresentationSize(const IntSize&) override;
    void paint(GraphicsContext&, const FloatRect&) override;
    MediaTime durationMediaTime() const override;
    std::optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() override;
    DestinationColorSpace colorSpace() override;

    ThreadSafeWeakPtr<MediaPlayer> m_player;
    RefPtr<MockMediaSourcePrivate> m_mediaSourcePrivate;

    MediaTime m_currentTime;
    MediaTime m_duration;
    MediaPlayer::ReadyState m_readyState { MediaPlayer::ReadyState::HaveNothing };
    MediaPlayer::NetworkState m_networkState { MediaPlayer::NetworkState::Empty };
    bool m_playing { false };
    bool m_seekCompleted { false };
};

}

#endif // ENABLE(MEDIA_SOURCE)

#endif // MockMediaPlayerMediaSource_h

