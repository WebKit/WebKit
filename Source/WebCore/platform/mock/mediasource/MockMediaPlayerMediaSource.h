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
#include <wtf/MediaTime.h>

namespace WebCore {

class MediaSource;
class MockMediaSourcePrivate;

class MockMediaPlayerMediaSource : public MediaPlayerPrivateInterface {
public:
    explicit MockMediaPlayerMediaSource(MediaPlayer*);

    // MediaPlayer Engine Support
    WEBCORE_EXPORT static void registerMediaEngine(MediaEngineRegistrar);
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    virtual ~MockMediaPlayerMediaSource();

    void advanceCurrentTime();
    void updateDuration(const MediaTime&);

    MediaPlayer::ReadyState readyState() const override;
    void setReadyState(MediaPlayer::ReadyState);
    void setNetworkState(MediaPlayer::NetworkState);
    void waitForSeekCompleted();
    void seekCompleted();

private:
    // MediaPlayerPrivate Overrides
    void load(const String& url) override;
    void load(const String& url, MediaSourcePrivateClient*) override;
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) override { }
#endif
    void cancelLoad() override;
    void play() override;
    void pause() override;
    FloatSize naturalSize() const override;
    bool hasVideo() const override;
    bool hasAudio() const override;
    void setVisible(bool) override;
    bool seeking() const override;
    bool paused() const override;
    MediaPlayer::NetworkState networkState() const override;
    MediaTime maxMediaTimeSeekable() const override;
    std::unique_ptr<PlatformTimeRanges> buffered() const override;
    bool didLoadingProgress() const override;
    void setSize(const IntSize&) override;
    void paint(GraphicsContext&, const FloatRect&) override;
    MediaTime currentMediaTime() const override;
    MediaTime durationMediaTime() const override;
    void seekWithTolerance(const MediaTime&, const MediaTime&, const MediaTime&) override;
    Optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() override;

    MediaPlayer* m_player;
    RefPtr<MockMediaSourcePrivate> m_mediaSourcePrivate;

    MediaTime m_currentTime;
    MediaTime m_duration;
    MediaPlayer::ReadyState m_readyState;
    MediaPlayer::NetworkState m_networkState;
    bool m_playing;
    bool m_seekCompleted;
};

}

#endif // ENABLE(MEDIA_SOURCE)

#endif // MockMediaPlayerMediaSource_h

