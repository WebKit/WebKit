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

MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_player(player)
    , m_seekTo(-1)
    , m_endTime(numeric_limits<float>::infinity())
    , m_seekTimer(this, &MediaPlayerPrivate::seekTimerFired)
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_enabledTrackCount(0)
    , m_totalTrackCount(0)
    , m_hasUnsupportedTracks(false)
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
        // FIXME: is this the right error to return?
        m_networkState = MediaPlayer::DecodeError; 
        m_player->networkStateChanged();
        return;
    }

    if (m_networkState != MediaPlayer::Loading) {
        m_networkState = MediaPlayer::Loading;
        m_player->networkStateChanged();
    }
    if (m_readyState != MediaPlayer::HaveNothing) {
        m_readyState = MediaPlayer::HaveNothing;
        m_player->readyStateChanged();
    }
    cancelSeek();

    m_qtMovie.set(new QTMovieWin(this));
    m_qtMovie->load(url.characters(), url.length(), m_player->preservesPitch());
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
    if (oldRate)
        m_qtMovie->setRate(0);
    m_qtMovie->setCurrentTime(m_seekTo);
    float timeAfterSeek = currentTime();
    // restore playback only if not at end, othewise QTMovie will loop
    if (oldRate && timeAfterSeek < duration() && timeAfterSeek < m_endTime)
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
    m_qtMovie->setRate(rate);
}

void MediaPlayerPrivate::setPreservesPitch(bool preservesPitch)
{
    if (!m_qtMovie)
        return;
    m_qtMovie->setPreservesPitch(preservesPitch);
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

    if (loadState >= QTMovieLoadStateLoaded && m_readyState < MediaPlayer::HaveMetadata) {
        m_qtMovie->disableUnsupportedTracks(m_enabledTrackCount, m_totalTrackCount);
        if (m_player->inMediaDocument()) {
            if (!m_enabledTrackCount || m_enabledTrackCount != m_totalTrackCount) {
                // This is a type of media that we do not handle directly with a <video> 
                // element, eg. QuickTime VR, a movie with a sprite track, etc. Tell the 
                // MediaPlayerClient that we won't support it.
                sawUnsupportedTracks();
                return;
            }
        } else if (!m_enabledTrackCount)
            loadState = QTMovieLoadStateError;
    }

    // "Loaded" is reserved for fully buffered movies, never the case when streaming
    if (loadState >= QTMovieLoadStateComplete && !m_isStreaming) {
        m_networkState = MediaPlayer::Loaded;
        m_readyState = MediaPlayer::HaveEnoughData;
    } else if (loadState >= QTMovieLoadStatePlaythroughOK) {
        m_readyState = MediaPlayer::HaveEnoughData;
    } else if (loadState >= QTMovieLoadStatePlayable) {
        // FIXME: This might not work correctly in streaming case, <rdar://problem/5693967>
        m_readyState = currentTime() < maxTimeLoaded() ? MediaPlayer::HaveFutureData : MediaPlayer::HaveCurrentData;
    } else if (loadState >= QTMovieLoadStateLoaded) {
        m_readyState = MediaPlayer::HaveMetadata;
    } else if (loadState > QTMovieLoadStateError) {
        m_networkState = MediaPlayer::Loading;
        m_readyState = MediaPlayer::HaveNothing;        
    } else {
        if (m_player->inMediaDocument()) {
            // Something went wrong in the loading of media within a standalone file. 
            // This can occur with chained ref movies that eventually resolve to a
            // file we don't support.
            sawUnsupportedTracks();
            return;
        }

        float loaded = maxTimeLoaded();
        if (!loaded)
            m_readyState = MediaPlayer::HaveNothing;

        if (!m_enabledTrackCount)
            m_networkState = MediaPlayer::FormatError;
        else {
            // FIXME: We should differentiate between load/network errors and decode errors <rdar://problem/5605692>
            if (loaded > 0)
                m_networkState = MediaPlayer::DecodeError;
            else
                m_readyState = MediaPlayer::HaveNothing;
        }
    }

    if (seeking())
        m_readyState = MediaPlayer::HaveNothing;
    
    if (m_networkState != oldNetworkState)
        m_player->networkStateChanged();
    if (m_readyState != oldReadyState)
        m_player->readyStateChanged();
}

void MediaPlayerPrivate::sawUnsupportedTracks()
{
    m_qtMovie->setDisabled(true);
    m_hasUnsupportedTracks = true;
    m_player->mediaPlayerClient()->mediaPlayerSawUnsupportedTracks(m_player);
}

void MediaPlayerPrivate::didEnd()
{
    if (m_hasUnsupportedTracks)
        return;

    m_startedPlaying = false;
#if DRAW_FRAME_RATE
    m_timeStoppedPlaying = GetTickCount();
#endif
    updateStates();
    m_player->timeChanged();
}

void MediaPlayerPrivate::setSize(const IntSize& size) 
{ 
    if (m_hasUnsupportedTracks || !m_qtMovie)
        return;
    m_qtMovie->setSize(size.width(), size.height());
}

void MediaPlayerPrivate::setVisible(bool b)
{
    if (m_hasUnsupportedTracks || !m_qtMovie)
        return;
    m_qtMovie->setVisible(b);
}

void MediaPlayerPrivate::paint(GraphicsContext* p, const IntRect& r)
{
    if (p->paintingDisabled() || !m_qtMovie || m_hasUnsupportedTracks)
        return;

    bool usingTempBitmap = false;
    OwnPtr<GraphicsContext::WindowsBitmap> bitmap;
    HDC hdc = p->getWindowsContext(r);
    if (!hdc) {
        // The graphics context doesn't have an associated HDC so create a temporary
        // bitmap where QTMovieWin can draw the frame and we can copy it.
        usingTempBitmap = true;
        bitmap.set(p->createWindowsBitmap(r.size()));
        hdc = bitmap->hdc();

        // FIXME: is this necessary??
        XFORM xform;
        xform.eM11 = 1.0f;
        xform.eM12 = 0.0f;
        xform.eM21 = 0.0f;
        xform.eM22 = 1.0f;
        xform.eDx = -r.x();
        xform.eDy = -r.y();
        SetWorldTransform(hdc, &xform);
    }

    m_qtMovie->paint(hdc, r.x(), r.y());
    if (usingTempBitmap)
        p->drawWindowsBitmap(bitmap.get(), r.topLeft());
    else
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
    return mimeTypeCache().contains(type) ? (codecs.isEmpty() ? MediaPlayer::MayBeSupported : MediaPlayer::IsSupported) : MediaPlayer::IsNotSupported;
}

void MediaPlayerPrivate::movieEnded(QTMovieWin* movie)
{
    if (m_hasUnsupportedTracks)
        return;

    ASSERT(m_qtMovie.get() == movie);
    didEnd();
}

void MediaPlayerPrivate::movieLoadStateChanged(QTMovieWin* movie)
{
    if (m_hasUnsupportedTracks)
        return;

    ASSERT(m_qtMovie.get() == movie);
    updateStates();
}

void MediaPlayerPrivate::movieTimeChanged(QTMovieWin* movie)
{
    if (m_hasUnsupportedTracks)
        return;

    ASSERT(m_qtMovie.get() == movie);
    updateStates();
    m_player->timeChanged();
}

void MediaPlayerPrivate::movieNewImageAvailable(QTMovieWin* movie)
{
    if (m_hasUnsupportedTracks)
        return;

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

bool MediaPlayerPrivate::hasSingleSecurityOrigin() const
{
    // We tell quicktime to disallow resources that come from different origins
    // so we all media is single origin.
    return true;
}

}

#endif

