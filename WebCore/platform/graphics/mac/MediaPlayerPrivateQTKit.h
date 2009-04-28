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
#include "FloatSize.h"
#include <wtf/RetainPtr.h>

#ifdef __OBJC__
#import <QTKit/QTTime.h>
@class QTMovie;
@class QTMovieView;
@class QTVideoRendererWebKitOnly;
@class WebCoreMovieObserver;
#else
class QTMovie;
class QTMovieView;
class QTTime;
class QTVideoRendererWebKitOnly;
class WebCoreMovieObserver;
#endif

#ifndef DRAW_FRAME_RATE
#define DRAW_FRAME_RATE 0
#endif

namespace WebCore {

class MediaPlayerPrivate : public MediaPlayerPrivateInterface {
public:
    static void registerMediaEngine(MediaEngineRegistrar);

    ~MediaPlayerPrivate();

    void repaint();
    void loadStateChanged();
    void rateChanged();
    void sizeChanged();
    void timeChanged();
    void didEnd();

private:
    MediaPlayerPrivate(MediaPlayer*);

    // engine support
    static MediaPlayerPrivateInterface* create(MediaPlayer* player);
    static void getSupportedTypes(HashSet<String>& types);
    static MediaPlayer::SupportsType supportsType(const String& type, const String& codecs);
    static bool isAvailable();

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
    
    void setRate(float);
    void setVolume(float);

    void setEndTime(float time);

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
    
    void paint(GraphicsContext*, const IntRect&);

    void createQTMovie(const String& url);
    void setUpVideoRendering();
    void tearDownVideoRendering();
    void createQTMovieView();
    void detachQTMovieView();
    void createQTVideoRenderer();
    void destroyQTVideoRenderer();
    QTTime createQTTime(float time) const;
    
    void updateStates();
    void doSeek();
    void cancelSeek();
    void seekTimerFired(Timer<MediaPlayerPrivate>*);
    float maxTimeLoaded() const;
    void disableUnsupportedTracks();
    
    void sawUnsupportedTracks();
    void cacheMovieScale();
    bool metaDataAvailable() const { return m_qtMovie && m_readyState >= MediaPlayer::HaveMetadata; }

    MediaPlayer* m_player;
    RetainPtr<QTMovie> m_qtMovie;
    RetainPtr<QTMovieView> m_qtMovieView;
    RetainPtr<QTVideoRendererWebKitOnly> m_qtVideoRenderer;
    RetainPtr<WebCoreMovieObserver> m_objcObserver;
    float m_seekTo;
    Timer<MediaPlayerPrivate> m_seekTimer;
    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;
    bool m_startedPlaying;
    bool m_isStreaming;
    bool m_visible;
    IntRect m_rect;
    FloatSize m_scaleFactor;
    unsigned m_enabledTrackCount;
    unsigned m_totalTrackCount;
    bool m_hasUnsupportedTracks;
    float m_duration;
#if DRAW_FRAME_RATE
    int  m_frameCountWhilePlaying;
    double m_timeStartedPlaying;
    double m_timeStoppedPlaying;
#endif
};

}

#endif
#endif
