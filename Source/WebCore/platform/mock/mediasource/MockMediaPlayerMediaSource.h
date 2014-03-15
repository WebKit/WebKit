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
    // MediaPlayer Engine Support
    static void registerMediaEngine(MediaEngineRegistrar);
    static PassOwnPtr<MediaPlayerPrivateInterface> create(MediaPlayer*);
    static void getSupportedTypes(HashSet<String>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    virtual ~MockMediaPlayerMediaSource();

    void advanceCurrentTime();
    void updateDuration(double);

    virtual MediaPlayer::ReadyState readyState() const override;
    void setReadyState(MediaPlayer::ReadyState);
    void setNetworkState(MediaPlayer::NetworkState);

private:
    MockMediaPlayerMediaSource(MediaPlayer*);

    // MediaPlayerPrivate Overrides
    virtual void load(const String& url) override;
    virtual void load(const String& url, MediaSourcePrivateClient*) override;
    virtual void cancelLoad() override;
    virtual void play() override;
    virtual void pause() override;
    virtual IntSize naturalSize() const override;
    virtual bool hasVideo() const override;
    virtual bool hasAudio() const override;
    virtual void setVisible(bool) override;
    virtual bool seeking() const override;
    virtual bool paused() const override;
    virtual MediaPlayer::NetworkState networkState() const override;
    virtual double maxTimeSeekableDouble() const override;
    virtual std::unique_ptr<PlatformTimeRanges> buffered() const override;
    virtual bool didLoadingProgress() const override;
    virtual void setSize(const IntSize&) override;
    virtual void paint(GraphicsContext*, const IntRect&) override;
    virtual double currentTimeDouble() const override;
    virtual double durationDouble() const override;
    virtual void seekWithTolerance(double time, double, double) override;
    virtual unsigned long totalVideoFrames() override;
    virtual unsigned long droppedVideoFrames() override;
    virtual unsigned long corruptedVideoFrames() override;
    virtual double totalFrameDelay() override;

    MediaPlayer* m_player;
    RefPtr<MediaSourcePrivateClient> m_mediaSource;
    RefPtr<MockMediaSourcePrivate> m_mediaSourcePrivate;

    MediaTime m_currentTime;
    double m_duration;
    MediaPlayer::ReadyState m_readyState;
    MediaPlayer::NetworkState m_networkState;
    bool m_playing;
};

}

#endif // ENABLE(MEDIA_SOURCE)

#endif // MockMediaPlayerMediaSource_h

