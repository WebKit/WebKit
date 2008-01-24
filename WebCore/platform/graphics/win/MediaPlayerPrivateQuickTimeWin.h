/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#include "MediaPlayer.h"
#include "Timer.h"
#include <QTMovieWin.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

class GraphicsContext;
class IntSize;
class IntRect;
class String;

class MediaPlayerPrivate : QTMovieWinClient, Noncopyable {
public:
    MediaPlayerPrivate(MediaPlayer*);
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
    void setRect(const IntRect&);
    
    void loadStateChanged();
    void didEnd();
    
    void paint(GraphicsContext*, const IntRect&);
    
    static void getSupportedTypes(HashSet<String>& types);
    static bool isAvailable();

private:
    void updateStates();
    void doSeek();
    void cancelSeek();
    void seekTimerFired(Timer<MediaPlayerPrivate>*);
    void endPointTimerFired(Timer<MediaPlayerPrivate>*);
    float maxTimeLoaded() const;
    void startEndPointTimerIfNeeded();

    virtual void movieEnded(QTMovieWin*);
    virtual void movieLoadStateChanged(QTMovieWin*);
    virtual void movieTimeChanged(QTMovieWin*);
    virtual void movieNewImageAvailable(QTMovieWin*);
      
    MediaPlayer* m_player;
    OwnPtr<QTMovieWin> m_qtMovie;
    float m_seekTo;
    float m_endTime;
    Timer<MediaPlayerPrivate> m_seekTimer;
    Timer<MediaPlayerPrivate> m_endPointTimer;
    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;
    bool m_startedPlaying;
    bool m_isStreaming;
};

}

#endif
#endif
