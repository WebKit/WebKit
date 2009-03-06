/*
 * Copyright (C) 2007, 2008, 2009 Apple, Inc.  All rights reserved.
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

#include "config.h"

#if ENABLE(VIDEO)
#include "MediaPlayerPrivateQuickTimeWin.h"

#include "GraphicsContext.h"
#include "KURL.h"
#include "QTMovieWin.h"
#include "ScrollView.h"
#include "StringHash.h"
#include <wtf/HashSet.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>

#if DRAW_FRAME_RATE
#include "Font.h"
#include "FrameView.h"
#include "Frame.h"
#include "Document.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "Windows.h"
#endif

using namespace std;

namespace WebCore {

MediaPlayerPrivateInterface* MediaPlayerPrivate::create(MediaPlayer* player) 
{ 
    return new MediaPlayerPrivate(player);
}

void MediaPlayerPrivate::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (isAvailable())
        registrar(create, getSupportedTypes, supportsType);
}

static const double endPointTimerInterval = 0.020;

MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_player(player)
    , m_seekTo(-1)
    , m_endTime(numeric_limits<float>::infinity())
    , m_seekTimer(this, &MediaPlayerPrivate::seekTimerFired)
    , m_endPointTimer(this, &MediaPlayerPrivate::endPointTimerFired)
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::DataUnavailable)
    , m_startedPlaying(false)
    , m_isStreaming(false)
#if DRAW_FRAME_RATE
    , m_frameCountWhilePlaying(0)
    , m_timeStartedPlaying(0)
    , m_timeStoppedPlaying(0)
#endif
{
}

MediaPlayerPrivate::~MediaPlayerPrivate()
{
}

void MediaPlayerPrivate::load(const String& url)
{
    if (!QTMovieWin::initializeQuickTime()) {
        m_networkState = MediaPlayer::LoadFailed;
        m_player->networkStateChanged();
        return;
    }

    if (m_networkState != MediaPlayer::Loading) {
        m_networkState = MediaPlayer::Loading;
        m_player->networkStateChanged();
    }
    if (m_readyState != MediaPlayer::DataUnavailable) {
        m_readyState = MediaPlayer::DataUnavailable;
        m_player->readyStateChanged();
    }
    cancelSeek();
    m_endPointTimer.stop();

    m_qtMovie.set(new QTMovieWin(this));
    m_qtMovie->load(url.characters(), url.length());
    m_qtMovie->setVolume(m_player->volume());
    m_qtMovie->setVisible(m_player->visible());
}

void MediaPlayerPrivate::play()
{
    if (!m_qtMovie)
        return;
    m_startedPlaying = true;
#if DRAW_FRAME_RATE
    m_frameCountWhilePlaying = 0;
#endif

    m_qtMovie->play();
    startEndPointTimerIfNeeded();
}

void MediaPlayerPrivate::pause()
{
    if (!m_qtMovie)
        return;
    m_startedPlaying = false;
#if DRAW_FRAME_RATE
    m_timeStoppedPlaying = GetTickCount();
#endif
    m_qtMovie->pause();
    m_endPointTimer.stop();
}

float MediaPlayerPrivate::duration() const
{
    if (!m_qtMovie)
        return 0;
    return m_qtMovie->duration();
}

float MediaPlayerPrivate::currentTime() const
{
    if (!m_qtMovie)
        return 0;
    return min(m_qtMovie->currentTime(), m_endTime);
}

void MediaPlayerPrivate::seek(float time)
{
    cancelSeek();
    
    if (!m_qtMovie)
        return;
    
    if (time > duration())
        time = duration();
    
    m_seekTo = time;
    if (maxTimeLoaded() >= m_seekTo)
        doSeek();
    else 
        m_seekTimer.start(0, 0.5f);
}
    
void MediaPlayerPrivate::doSeek() 
{
    float oldRate = m_qtMovie->rate();
    m_qtMovie->setRate(0);
    m_qtMovie->setCurrentTime(m_seekTo);
    float timeAfterSeek = currentTime();
    // restore playback only if not at end, othewise QTMovie will loop
    if (timeAfterSeek < duration() && timeAfterSeek < m_endTime)
        m_qtMovie->setRate(oldRate);
    cancelSeek();
}

void MediaPlayerPrivate::cancelSeek()
{
    m_seekTo = -1;
    m_seekTimer.stop();
}

void MediaPlayerPrivate::seekTimerFired(Timer<MediaPlayerPrivate>*)
{        
    if (!m_qtMovie || !seeking() || currentTime() == m_seekTo) {
        cancelSeek();
        updateStates();
        m_player->timeChanged(); 
        return;
    } 
    
    if (maxTimeLoaded() >= m_seekTo)
        doSeek();
    else {
        MediaPlayer::NetworkState state = networkState();
        if (state == MediaPlayer::Empty || state == MediaPlayer::Loaded) {
            cancelSeek();
            updateStates();
            m_player->timeChanged();
        }
    }
}

void MediaPlayerPrivate::setEndTime(float time)
{
    m_endTime = time;
    startEndPointTimerIfNeeded();
}

void MediaPlayerPrivate::startEndPointTimerIfNeeded()
{
    if (m_endTime < duration() && m_startedPlaying && !m_endPointTimer.isActive())
        m_endPointTimer.startRepeating(endPointTimerInterval);
}

void MediaPlayerPrivate::endPointTimerFired(Timer<MediaPlayerPrivate>*)
{
    float time = currentTime();
    if (time >= m_endTime) {
        pause();
        didEnd();
    }
}

bool MediaPlayerPrivate::paused() const
{
    if (!m_qtMovie)
        return true;
    return m_qtMovie->rate() == 0.0f;
}

bool MediaPlayerPrivate::seeking() const
{
    if (!m_qtMovie)
        return false;
    return m_seekTo >= 0;
}

IntSize MediaPlayerPrivate::naturalSize() const
{
    if (!m_qtMovie)
        return IntSize();
    int width;
    int height;
    m_qtMovie->getNaturalSize(width, height);
    return IntSize(width, height);
}

bool MediaPlayerPrivate::hasVideo() const
{
    if (!m_qtMovie)
        return false;
    return m_qtMovie->hasVideo();
}

void MediaPlayerPrivate::setVolume(float volume)
{
    if (!m_qtMovie)
        return;
    m_qtMovie->setVolume(volume);
}

void MediaPlayerPrivate::setRate(float rate)
{
    if (!m_qtMovie)
        return;
    if (!paused())
        m_qtMovie->setRate(rate);
}

int MediaPlayerPrivate::dataRate() const
{
    // This is not used at the moment
    return 0;
}

float MediaPlayerPrivate::maxTimeBuffered() const
{
    // rtsp streams are not buffered
    return m_isStreaming ? 0 : maxTimeLoaded();
}

float MediaPlayerPrivate::maxTimeSeekable() const
{
    // infinite duration means live stream
    return !isfinite(duration()) ? 0 : maxTimeLoaded();
}

float MediaPlayerPrivate::maxTimeLoaded() const
{
    if (!m_qtMovie)
        return 0;
    return m_qtMovie->maxTimeLoaded(); 
}

unsigned MediaPlayerPrivate::bytesLoaded() const
{
    if (!m_qtMovie)
        return 0;
    float dur = duration();
    float maxTime = maxTimeLoaded();
    if (!dur)
        return 0;
    return totalBytes() * maxTime / dur;
}

bool MediaPlayerPrivate::totalBytesKnown() const
{
    return totalBytes() > 0;
}

unsigned MediaPlayerPrivate::totalBytes() const
{
    if (!m_qtMovie)
        return 0;
    return m_qtMovie->dataSize();
}

void MediaPlayerPrivate::cancelLoad()
{
    if (m_networkState < MediaPlayer::Loading || m_networkState == MediaPlayer::Loaded)
        return;
    
    // Cancel the load by destroying the movie.
    m_qtMovie.clear();
    
    updateStates();
}

void MediaPlayerPrivate::updateStates()
{
    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;
  
    long loadState = m_qtMovie ? m_qtMovie->loadState() : QTMovieLoadStateError;

    if (loadState >= QTMovieLoadStateLoaded && m_networkState < MediaPlayer::LoadedMetaData && !m_player->inMediaDocument()) {
        unsigned enabledTrackCount;
        m_qtMovie->disableUnsupportedTracks(enabledTrackCount);
        // FIXME: We should differentiate between load errors and decode errors <rdar://problem/5605692>
        if (!enabledTrackCount)
            loadState = QTMovieLoadStateError;
    }

    // "Loaded" is reserved for fully buffered movies, never the case when streaming
    if (loadState >= QTMovieLoadStateComplete && !m_isStreaming) {
        if (m_networkState < MediaPlayer::Loaded)
            m_networkState = MediaPlayer::Loaded;
        m_readyState = MediaPlayer::CanPlayThrough;
    } else if (loadState >= QTMovieLoadStatePlaythroughOK) {
        if (m_networkState < MediaPlayer::LoadedFirstFrame && !seeking())
            m_networkState = MediaPlayer::LoadedFirstFrame;
        m_readyState = MediaPlayer::CanPlayThrough;
    } else if (loadState >= QTMovieLoadStatePlayable) {
        if (m_networkState < MediaPlayer::LoadedFirstFrame && !seeking())
            m_networkState = MediaPlayer::LoadedFirstFrame;
        m_readyState = currentTime() < maxTimeLoaded() ? MediaPlayer::CanPlay : MediaPlayer::DataUnavailable;
    } else if (loadState >= QTMovieLoadStateLoaded) {
        if (m_networkState < MediaPlayer::LoadedMetaData)
            m_networkState = MediaPlayer::LoadedMetaData;
        m_readyState = MediaPlayer::DataUnavailable;
    } else if (loadState > QTMovieLoadStateError) {
        if (m_networkState < MediaPlayer::Loading)
            m_networkState = MediaPlayer::Loading;
        m_readyState = MediaPlayer::DataUnavailable;        
    } else {
        m_networkState = MediaPlayer::LoadFailed;
        m_readyState = MediaPlayer::DataUnavailable; 
    }

    if (seeking())
        m_readyState = MediaPlayer::DataUnavailable;
    
    if (m_networkState != oldNetworkState)
        m_player->networkStateChanged();
    if (m_readyState != oldReadyState)
        m_player->readyStateChanged();
}


void MediaPlayerPrivate::didEnd()
{
    m_endPointTimer.stop();
    m_startedPlaying = false;
#if DRAW_FRAME_RATE
    m_timeStoppedPlaying = GetTickCount();
#endif
    updateStates();
    m_player->timeChanged();
}

void MediaPlayerPrivate::setSize(const IntSize& size) 
{ 
    if (m_qtMovie)
        m_qtMovie->setSize(size.width(), size.height());
}

void MediaPlayerPrivate::setVisible(bool b)
{
    if (!m_qtMovie)
        return;
    m_qtMovie->setVisible(b);
}

void MediaPlayerPrivate::paint(GraphicsContext* p, const IntRect& r)
{
    if (p->paintingDisabled() || !m_qtMovie)
        return;
    HDC hdc = p->getWindowsContext(r);
    m_qtMovie->paint(hdc, r.x(), r.y());
    p->releaseWindowsContext(hdc, r);

#if DRAW_FRAME_RATE
    if (m_frameCountWhilePlaying > 10) {
        Frame* frame = m_player->frameView() ? m_player->frameView()->frame() : NULL;
        Document* document = frame ? frame->document() : NULL;
        RenderObject* renderer = document ? document->renderer() : NULL;
        RenderStyle* styleToUse = renderer ? renderer->style() : NULL;
        if (styleToUse) {
            double frameRate = (m_frameCountWhilePlaying - 1) / (0.001 * ( m_startedPlaying ? (GetTickCount() - m_timeStartedPlaying) :
                (m_timeStoppedPlaying - m_timeStartedPlaying) ));
            String text = String::format("%1.2f", frameRate);
            TextRun textRun(text.characters(), text.length());
            const Color color(255, 0, 0);
            p->save();
            p->translate(r.x(), r.y() + r.height());
            p->setFont(styleToUse->font());
            p->setStrokeColor(color);
            p->setStrokeStyle(SolidStroke);
            p->setStrokeThickness(1.0f);
            p->setFillColor(color);
            p->drawText(textRun, IntPoint(2, -3));
            p->restore();
        }
    }
#endif
}

static HashSet<String> mimeTypeCache()
{
    DEFINE_STATIC_LOCAL(HashSet<String>, typeCache, ());
    static bool typeListInitialized = false;

    if (!typeListInitialized) {
        unsigned count = QTMovieWin::countSupportedTypes();
        for (unsigned n = 0; n < count; n++) {
            const UChar* character;
            unsigned len;
            QTMovieWin::getSupportedType(n, character, len);
            if (len)
                typeCache.add(String(character, len));
        }

        typeListInitialized = true;
    }
    
    return typeCache;
} 

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>& types)
{
    types = mimeTypeCache();
} 

bool MediaPlayerPrivate::isAvailable()
{
    return QTMovieWin::initializeQuickTime();
}

MediaPlayer::SupportsType MediaPlayerPrivate::supportsType(const String& type, const String& codecs)
{
    // only return "IsSupported" if there is no codecs parameter for now as there is no way to ask QT if it supports an
    //  extended MIME type
    return mimeTypeCache().contains(type) ? (!codecs.isEmpty() ? MediaPlayer::MayBeSupported : MediaPlayer::IsSupported) : MediaPlayer::IsNotSupported;
}

void MediaPlayerPrivate::movieEnded(QTMovieWin* movie)
{
    ASSERT(m_qtMovie.get() == movie);
    didEnd();
}

void MediaPlayerPrivate::movieLoadStateChanged(QTMovieWin* movie)
{
    ASSERT(m_qtMovie.get() == movie);
    updateStates();
}

void MediaPlayerPrivate::movieTimeChanged(QTMovieWin* movie)
{
    ASSERT(m_qtMovie.get() == movie);
    updateStates();
    m_player->timeChanged();
}

void MediaPlayerPrivate::movieNewImageAvailable(QTMovieWin* movie)
{
    ASSERT(m_qtMovie.get() == movie);
#if DRAW_FRAME_RATE
    if (m_startedPlaying) {
        m_frameCountWhilePlaying++;
        // to eliminate preroll costs from our calculation,
        // our frame rate calculation excludes the first frame drawn after playback starts
        if (1==m_frameCountWhilePlaying)
            m_timeStartedPlaying = GetTickCount();
    }
#endif
    m_player->repaint();
}

}

#endif

