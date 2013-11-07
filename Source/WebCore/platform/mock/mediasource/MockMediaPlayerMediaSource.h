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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

    virtual MediaPlayer::ReadyState readyState() const OVERRIDE;
    void setReadyState(MediaPlayer::ReadyState);

private:
    MockMediaPlayerMediaSource(MediaPlayer*);

    // MediaPlayerPrivate Overrides
    virtual void load(const String& url) OVERRIDE;
    virtual void load(const String& url, PassRefPtr<HTMLMediaSource>) OVERRIDE;
    virtual void cancelLoad() OVERRIDE;
    virtual void play() OVERRIDE;
    virtual void pause() OVERRIDE;
    virtual IntSize naturalSize() const OVERRIDE;
    virtual bool hasVideo() const OVERRIDE;
    virtual bool hasAudio() const OVERRIDE;
    virtual void setVisible(bool) OVERRIDE;
    virtual bool seeking() const OVERRIDE;
    virtual bool paused() const OVERRIDE;
    virtual MediaPlayer::NetworkState networkState() const OVERRIDE;
    virtual PassRefPtr<TimeRanges> buffered() const OVERRIDE;
    virtual bool didLoadingProgress() const OVERRIDE;
    virtual void setSize(const IntSize&) OVERRIDE;
    virtual void paint(GraphicsContext*, const IntRect&) OVERRIDE;
    virtual double currentTimeDouble() const OVERRIDE;
    virtual double durationDouble() const OVERRIDE;
    virtual void seekDouble(double time) OVERRIDE;

    MediaPlayer* m_player;
    RefPtr<HTMLMediaSource> m_mediaSource;
    RefPtr<MockMediaSourcePrivate> m_mediaSourcePrivate;

    double m_currentTime;
    double m_duration;
    MediaPlayer::ReadyState m_readyState;
    MediaPlayer::NetworkState m_networkState;
    bool m_playing;
};

}

#endif // ENABLE(MEDIA_SOURCE)

#endif // MockMediaPlayerMediaSource_h

