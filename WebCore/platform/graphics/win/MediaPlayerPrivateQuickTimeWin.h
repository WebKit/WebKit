/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef MediaPlayerPrivateQTKit_h
#define MediaPlayerPrivateQTKit_h

#if ENABLE(VIDEO)

#include "MediaPlayerPrivate.h"
#include "Timer.h"
#include <QTMovieWin.h>
#include <wtf/OwnPtr.h>

#ifndef DRAW_FRAME_RATE
#define DRAW_FRAME_RATE 0
#endif

namespace WebCore {

class GraphicsContext;
class IntSize;
class IntRect;
class String;

class MediaPlayerPrivate : public MediaPlayerPrivateInterface, public QTMovieWinClient {
public:
    static void registerMediaEngine(MediaEngineRegistrar);

    ~MediaPlayerPrivate();
    
    IntSize naturalSize() const;
    bool hasVideo() const;
    
    void load(const String& url);
    void cancelLoad();
    
    void play();
    void pause();    
    
    bool paused() const;
    bool seeking() const;
    
    float duration() const;
    float currentTime() const;
    void seek(float time);
    void setEndTime(float);
    
    void setRate(float);
    void setVolume(float);
    
    int dataRate() const;
    
    MediaPlayer::NetworkState networkState() const { return m_networkState; }
    MediaPlayer::ReadyState readyState() const { return m_readyState; }
    
    float maxTimeBuffered() const;
    float maxTimeSeekable() const;
    unsigned bytesLoaded() const;
    bool totalBytesKnown() const;
    unsigned totalBytes() const;
    
    void setVisible(bool);
    void setSize(const IntSize&);
    
    void loadStateChanged();
    void didEnd();
    
    void paint(GraphicsContext*, const IntRect&);
    

private:
    MediaPlayerPrivate(MediaPlayer*);

    void updateStates();
    void doSeek();
    void cancelSeek();
    void seekTimerFired(Timer<MediaPlayerPrivate>*);
    float maxTimeLoaded() const;
    void sawUnsupportedTracks();

    virtual void movieEnded(QTMovieWin*);
    virtual void movieLoadStateChanged(QTMovieWin*);
    virtual void movieTimeChanged(QTMovieWin*);
    virtual void movieNewImageAvailable(QTMovieWin*);

    // engine support
    static MediaPlayerPrivateInterface* create(MediaPlayer*);
    static void getSupportedTypes(HashSet<String>& types);
    static MediaPlayer::SupportsType supportsType(const String& type, const String& codecs);
    static bool isAvailable();

    MediaPlayer* m_player;
    OwnPtr<QTMovieWin> m_qtMovie;
    float m_seekTo;
    float m_endTime;
    Timer<MediaPlayerPrivate> m_seekTimer;
    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;
    unsigned m_enabledTrackCount;
    unsigned m_totalTrackCount;
    bool m_hasUnsupportedTracks;
    bool m_startedPlaying;
    bool m_isStreaming;
#if DRAW_FRAME_RATE
    int m_frameCountWhilePlaying;
    int m_timeStartedPlaying;
    int m_timeStoppedPlaying;
#endif
};

}

#endif
#endif
