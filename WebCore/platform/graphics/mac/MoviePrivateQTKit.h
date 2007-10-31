/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef MoviePrivateQTKit_h
#define MoviePrivateQTKit_h

#if ENABLE(VIDEO)

#include "Movie.h"
#include "Timer.h"
#include "wtf/RetainPtr.h"
#include "wtf/Noncopyable.h"

#ifdef __OBJC__
#import "QTKit/QTTime.h"
@class QTMovie;
@class QTMovieView;
@class WebCoreMovieObserver;
#else
class QTMovie;
class QTMovieView;
class QTTime;
class WebCoreMovieObserver;
#endif

namespace WebCore {

class GraphicsContext;
class IntSize;
class IntRect;
class String;

class MoviePrivate : Noncopyable
{
public:
    MoviePrivate(Movie* m);
    ~MoviePrivate();
    
    IntSize naturalSize();
    bool hasVideo();
    
    void load(String url);
    void cancelLoad();
    
    void play();
    void pause();    
    
    bool paused() const;
    bool seeking() const;
    
    float duration() const;
    float currentTime() const;
    void seek(float time);
    void setEndTime(float time);
    
    void addCuePoint(float time);
    void removeCuePoint(float time);
    void clearCuePoints();
    
    void setRate(float);
    void setVolume(float);
    void setMuted(bool);
    
    int dataRate() const;
    
    Movie::NetworkState networkState();
    Movie::ReadyState readyState();
    
    float maxTimeBuffered();
    float maxTimeSeekable();
    unsigned bytesLoaded();
    bool totalBytesKnown();
    unsigned totalBytes();
    
    void setVisible(bool);
    void setRect(const IntRect& r);
    
    void loadStateChanged();
    void rateChanged();
    void sizeChanged();
    void timeChanged();
    void volumeChanged();
    void didEnd();
    
    void paint(GraphicsContext* p, const IntRect& r);
    
    void createQTMovie(String url);
    void createQTMovieView();
    QTTime createQTTime(float time);
    
    static void getSupportedTypes(HashSet<String>& types);
    
private:
    
    void updateStates();
    void cancelSeek();
    void seekTimerFired(Timer<MoviePrivate>*);
    void cuePointTimerFired(Timer<MoviePrivate>*);
    float maxTimeLoaded();
    void startCuePointTimerIfNeeded();
    
private:    
    Movie* m_movie;
    RetainPtr<QTMovie> m_qtMovie;
    RetainPtr<QTMovieView> m_qtMovieView;
    RetainPtr<WebCoreMovieObserver> m_objcObserver;
    float m_seekTo;
    float m_endTime;
    Timer<MoviePrivate> m_seekTimer;
    Timer<MoviePrivate> m_cuePointTimer;
    float m_previousTimeCueTimerFired;
    float m_rateBeforeSeek;
    Movie::NetworkState m_networkState;
    Movie::ReadyState m_readyState;
    bool m_startedPlaying;
    bool m_blockStateUpdate;
    bool m_isStreaming;
};

}

#endif
#endif
