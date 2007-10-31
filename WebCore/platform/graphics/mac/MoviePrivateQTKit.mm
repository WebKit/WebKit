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

#import "config.h"

#if ENABLE(VIDEO)
#import "MoviePrivateQTKit.h"

#import "BlockExceptions.h"
#import "DeprecatedString.h"
#import "GraphicsContext.h"
#import "IntRect.h"
#import "KURL.h"
#import <limits>
#import "MimeTypeRegistry.h"
#import "Movie.h"
#import <QTKit/QTKit.h>
#import "ScrollView.h"
#import "WebCoreSystemInterface.h"
#import "Widget.h"
#import "wtf/RetainPtr.h"

@interface WebCoreMovieObserver : NSObject
{
    WebCore::MoviePrivate* callback;
}
-(void)setCallback:(WebCore::MoviePrivate *)c;
-(void)loadStateChanged:(NSNotification *)notification;
-(void)rateChanged:(NSNotification *)notification;
-(void)sizeChanged:(NSNotification *)notification;
-(void)timeChanged:(NSNotification *)notification;
-(void)volumeChanged:(NSNotification *)notification;
-(void)didEnd:(NSNotification *)notification;
@end

namespace WebCore {
    
MoviePrivate::MoviePrivate(Movie* movie)
    : m_movie(movie)
    , m_qtMovie(nil)
    , m_qtMovieView(nil)
    , m_seekTo(-1)
    , m_endTime(std::numeric_limits<float>::infinity())
    , m_seekTimer(this, &MoviePrivate::seekTimerFired)
    , m_cuePointTimer(this, &MoviePrivate::cuePointTimerFired)
    , m_previousTimeCueTimerFired(0)
    , m_rateBeforeSeek(0)
    , m_networkState(Movie::Empty)
    , m_readyState(Movie::DataUnavailable)
    , m_startedPlaying(false)
    , m_blockStateUpdate(false)
    , m_isStreaming(false)
{
    m_objcObserver = [[[WebCoreMovieObserver alloc] init] autorelease];
    [m_objcObserver.get() setCallback: this];
}


MoviePrivate::~MoviePrivate()
{
    if (m_qtMovieView)
        [m_qtMovieView.get() removeFromSuperview];
    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];
}

void MoviePrivate::createQTMovie(String url)
{
    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];
    
    m_qtMovie = nil;
    
    NSError* error = nil;
    m_qtMovie = [[[QTMovie alloc] initWithURL:KURL(url.deprecatedString()).getNSURL() error:&error] autorelease];
    
    // FIXME: find a proper way to do this
    m_isStreaming = url.startsWith("rtsp:");
    
    if (!m_qtMovie)
        return;
    
    [m_qtMovie.get() setVolume: m_movie->volume()];
    [m_qtMovie.get() setMuted: m_movie->muted()];
    
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get()
                                             selector:@selector(loadStateChanged:) 
                                                 name:QTMovieLoadStateDidChangeNotification 
                                               object:m_qtMovie.get()];
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get()
                                             selector:@selector(rateChanged:) 
                                                 name:QTMovieRateDidChangeNotification 
                                               object:m_qtMovie.get()];
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get()
                                             selector:@selector(sizeChanged:) 
                                                 name:QTMovieSizeDidChangeNotification 
                                               object:m_qtMovie.get()];
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get()
                                             selector:@selector(timeChanged:) 
                                                 name:QTMovieTimeDidChangeNotification 
                                               object:m_qtMovie.get()];
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get()
                                             selector:@selector(volumeChanged:) 
                                                 name:QTMovieVolumeDidChangeNotification 
                                               object:m_qtMovie.get()];
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get()
                                             selector:@selector(didEnd:) 
                                                 name:QTMovieDidEndNotification 
                                               object:m_qtMovie.get()];
}

void MoviePrivate::createQTMovieView()
{
    if (m_qtMovieView) {
        [m_qtMovieView.get() removeFromSuperview];
        m_qtMovieView = nil;
    }
    if (!m_movie->m_parentWidget || !m_qtMovie)
        return;
    m_qtMovieView = [[[QTMovieView alloc] initWithFrame:m_movie->rect()] autorelease];
    NSView* parentView = static_cast<ScrollView*>(m_movie->m_parentWidget)->getDocumentView();
    [parentView addSubview:m_qtMovieView.get()];
    [m_qtMovieView.get() setMovie:m_qtMovie.get()];
    [m_qtMovieView.get() setControllerVisible:NO];
    [m_qtMovieView.get() setPreservesAspectRatio:YES];
}

QTTime MoviePrivate::createQTTime(float time)
{
    if (!m_qtMovie)
        return QTMakeTime(0, 600);
    int timeScale = [[m_qtMovie.get() attributeForKey:QTMovieTimeScaleAttribute] intValue];
    return QTMakeTime((long long)(time * timeScale), timeScale);
}

void MoviePrivate::load(String url)
{
    if (m_networkState != Movie::Loading) {
        m_networkState = Movie::Loading;
        m_movie->networkStateChanged();
    }
    if (m_readyState != Movie::DataUnavailable) {
        m_readyState = Movie::DataUnavailable;
        m_movie->readyStateChanged();
    }
    cancelSeek();
    m_cuePointTimer.stop();
    createQTMovie(url);
    if (m_movie->visible())
        createQTMovieView();
    
    updateStates();
}

void MoviePrivate::play()
{
    cancelSeek();
    if (!m_qtMovie)
        return;
    m_startedPlaying = true;
    [m_qtMovie.get() setRate: m_movie->rate()];
    startCuePointTimerIfNeeded();
}

void MoviePrivate::pause()
{
    cancelSeek();
    if (!m_qtMovie)
        return;
    m_startedPlaying = false;
    [m_qtMovie.get() stop];
    m_cuePointTimer.stop();
}

float MoviePrivate::duration() const
{
    if (!m_qtMovie)
        return 0;
    QTTime time = [m_qtMovie.get() duration];
    if (time.flags == kQTTimeIsIndefinite)
        return std::numeric_limits<float>::infinity();
    return (float)time.timeValue / time.timeScale;
}

float MoviePrivate::currentTime() const
{
    if (!m_qtMovie)
        return 0;
    if (seeking())
        return m_seekTo;
    QTTime time = [m_qtMovie.get() currentTime];
    float current = (float)time.timeValue / time.timeScale;    
    current = std::min(current, m_endTime);
    return current;
}

void MoviePrivate::seek(float time)
{
    cancelSeek();
    
    if (!m_qtMovie)
        return;
    
    if (time > duration())
        time = duration();
    
    if (maxTimeLoaded() < time) {
        m_seekTo = time;
        m_seekTimer.startRepeating(0.5f);
        m_rateBeforeSeek = [m_qtMovie.get() rate];
        [m_qtMovie.get() setRate:0.0f];
        updateStates();
    } else {
        QTTime qttime = createQTTime(time);
        // setCurrentTime generates several event callbacks, update afterwards
        m_blockStateUpdate = true;
        [m_qtMovie.get() setCurrentTime: qttime];
        m_blockStateUpdate = false;
        updateStates();
    }
}

void MoviePrivate::setEndTime(float time)
{
    m_endTime = time;
    startCuePointTimerIfNeeded();
}

void MoviePrivate::addCuePoint(float time)
{
    // FIXME: simulate with timer for now
    startCuePointTimerIfNeeded();
}

void MoviePrivate::removeCuePoint(float time)
{
}

void MoviePrivate::clearCuePoints()
{
}

void MoviePrivate::startCuePointTimerIfNeeded()
{
    
    if ((m_endTime < duration() || !m_movie->m_cuePoints.isEmpty())
        && m_startedPlaying && !m_cuePointTimer.isActive()) {
        m_previousTimeCueTimerFired = currentTime();
        m_cuePointTimer.startRepeating(0.020f);
    }
}

void MoviePrivate::cancelSeek()
{
    if (m_seekTo > -1) {
        m_seekTo = -1;
        if (m_qtMovie)
            [m_qtMovie.get() setRate:m_rateBeforeSeek];
    }
    m_rateBeforeSeek = 0.0f;
    m_seekTimer.stop();
}

void MoviePrivate::seekTimerFired(Timer<MoviePrivate>*)
{        
    if (!m_qtMovie) {
        cancelSeek();
        return;
    }
    if (!seeking()) {
        updateStates();
        return;
    }
    
    if (maxTimeLoaded() > m_seekTo) {
        QTTime qttime = createQTTime(m_seekTo);
        // setCurrentTime generates several event callbacks, update afterwards
        m_blockStateUpdate = true;
        [m_qtMovie.get() setCurrentTime: qttime];
        m_blockStateUpdate = false;
        cancelSeek();
        updateStates();
    }

    Movie::NetworkState state = networkState();
    if (state == Movie::Empty || state == Movie::Loaded) {
        cancelSeek();
        updateStates();
    }
}

void MoviePrivate::cuePointTimerFired(Timer<MoviePrivate>*)
{
    float time = currentTime();
    float previousTime = m_previousTimeCueTimerFired;
    m_previousTimeCueTimerFired = time;
    
    // just do end for now
    if (time >= m_endTime) {
        pause();
        didEnd();
    }
    HashSet<float>::const_iterator end = m_movie->m_cuePoints.end();
    for (HashSet<float>::const_iterator it = m_movie->m_cuePoints.begin(); it != end; ++it) {
        float cueTime = *it;
        if (previousTime < cueTime && cueTime <= time)
            m_movie->cuePointReached(cueTime);
    }
}

bool MoviePrivate::paused() const
{
    if (!m_qtMovie)
        return true;
    return [m_qtMovie.get() rate] == 0.0f && (!seeking() || m_rateBeforeSeek == 0.0f);
}

bool MoviePrivate::seeking() const
{
    if (!m_qtMovie)
        return false;
    return m_seekTo >= 0;
}

IntSize MoviePrivate::naturalSize()
{
    if (!m_qtMovie)
        return IntSize();
    NSSize val = [[m_qtMovie.get() attributeForKey:QTMovieNaturalSizeAttribute] sizeValue];
    return IntSize(val);
}

bool MoviePrivate::hasVideo()
{
    if (!m_qtMovie)
        return false;
    BOOL val = [[m_qtMovie.get() attributeForKey: QTMovieHasVideoAttribute] boolValue];
    return val;
}

void MoviePrivate::setVolume(float volume)
{
    if (!m_qtMovie)
        return;
    [m_qtMovie.get() setVolume:volume];  
}

void MoviePrivate::setMuted(bool b)
{
    if (!m_qtMovie)
        return;
    [m_qtMovie.get() setMuted:b];
}

void MoviePrivate::setRate(float rate)
{
    if (!m_qtMovie)
        return;
    if (!paused())
        [m_qtMovie.get() setRate:rate];
}

int MoviePrivate::dataRate() const
{
    if (!m_qtMovie)
        return 0;
    return wkQTMovieDataRate(m_qtMovie.get()); 
}


Movie::NetworkState MoviePrivate::networkState()
{
    return m_networkState;
}

Movie::ReadyState MoviePrivate::readyState()
{
    return m_readyState;
}

float MoviePrivate::maxTimeBuffered()
{
    // rtsp streams are not buffered
    return m_isStreaming ? 0 : maxTimeLoaded();
}

float MoviePrivate::maxTimeSeekable()
{
    // infinite duration means live stream
    return isinf(duration()) ? 0 : maxTimeLoaded();
}

float MoviePrivate::maxTimeLoaded()
{
    if (!m_qtMovie)
        return 0;
    return wkQTMovieMaxTimeLoaded(m_qtMovie.get()); 
}

unsigned MoviePrivate::bytesLoaded()
{
    if (!m_qtMovie)
        return 0;
    float dur = duration();
    float maxTime = maxTimeLoaded();
    if (!dur)
        return 0;
    return totalBytes() * maxTime / dur;
}

bool MoviePrivate::totalBytesKnown()
{
    return totalBytes() > 0;
}

unsigned MoviePrivate::totalBytes()
{
    if (!m_qtMovie)
        return 0;
    return [[m_qtMovie.get() attributeForKey: QTMovieDataSizeAttribute] intValue];
}

void MoviePrivate::cancelLoad()
{
    // FIXME better way to do this?
    if (m_networkState < Movie::Loading || m_networkState == Movie::Loaded)
        return;
    
    if (m_qtMovieView) {
        [m_qtMovieView.get() removeFromSuperview];
        m_qtMovieView = nil;
    }
    m_qtMovie = nil;
    
    updateStates();
}

void MoviePrivate::updateStates()
{
    if (m_blockStateUpdate)
        return;
    
    Movie::NetworkState oldNetworkState = m_networkState;
    Movie::ReadyState oldReadyState = m_readyState;
    
    long loadState = m_qtMovie ? [[m_qtMovie.get() attributeForKey:QTMovieLoadStateAttribute] longValue] : -1;
    // "Loaded" is reserved for fully buffered movies, never the case when rtsp streaming
    if (loadState >= 100000 && !m_isStreaming) {
        // 100000 is kMovieLoadStateComplete
        if (m_networkState < Movie::Loaded)
            m_networkState = Movie::Loaded;
            m_readyState = Movie::CanPlayThrough;
    } else if (loadState >= 20000) {
        // 20000 is kMovieLoadStatePlaythroughOK
        if (m_networkState < Movie::LoadedFirstFrame && !seeking())
            m_networkState = Movie::LoadedFirstFrame;
        m_readyState = ([m_qtMovie.get() rate] == 0.0f && m_startedPlaying) ? Movie::DataUnavailable : Movie::CanPlayThrough;
    } else if (loadState >= 10000) {
        // 10000 is kMovieLoadStatePlayable
        if (m_networkState < Movie::LoadedFirstFrame && !seeking())
            m_networkState = Movie::LoadedFirstFrame;
        m_readyState = ([m_qtMovie.get() rate] == 0.0f && m_startedPlaying) ? Movie::DataUnavailable : Movie::CanPlay;
    } else if (loadState >= 2000) {
        // 10000 is kMovieLoadStateLoaded
        if (m_networkState < Movie::LoadedMetaData)
            m_networkState = Movie::LoadedMetaData;
        m_readyState = Movie::DataUnavailable;
    } else if (loadState >= 0) {
        if (m_networkState < Movie::Loading)
            m_networkState = Movie::Loading;
        m_readyState = Movie::DataUnavailable;        
    } else {
        m_networkState = Movie::LoadFailed;
        m_readyState = Movie::DataUnavailable; 
    }
    
    if (seeking())
        m_readyState = Movie::DataUnavailable;
    
    if (m_networkState != oldNetworkState)
        m_movie->networkStateChanged();
    if (m_readyState != oldReadyState)
        m_movie->readyStateChanged();
}

void MoviePrivate::loadStateChanged()
{
    updateStates();
}

void MoviePrivate::rateChanged()
{
    updateStates();
}

void MoviePrivate::sizeChanged()
{
}

void MoviePrivate::timeChanged()
{
    m_previousTimeCueTimerFired = -1;
    updateStates();
}

void MoviePrivate::volumeChanged()
{
    m_movie->volumeChanged();
}

void MoviePrivate::didEnd()
{
    m_cuePointTimer.stop();
    m_startedPlaying = false;
    m_movie->didEnd();
}

void MoviePrivate::setRect(const IntRect& r) 
{ 
    if (m_qtMovieView)
        [m_qtMovieView.get() setFrame: r];
}

void MoviePrivate::setVisible(bool b)
{
    if (b)
        createQTMovieView();
    else if (m_qtMovieView) {
        [m_qtMovieView.get() removeFromSuperview];
        m_qtMovieView = nil;
    }
}

void MoviePrivate::paint(GraphicsContext* p, const IntRect& r)
{
    if (p->paintingDisabled())
        return;
    NSView *view = m_qtMovieView.get();
    if (view == nil)
        return;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [view displayRectIgnoringOpacity:[view convertRect:r fromView:[view superview]]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void MoviePrivate::getSupportedTypes(HashSet<String>& types)
{
    NSArray* fileTypes = [QTMovie movieFileTypes:(QTMovieFileTypeOptions)0];
    int count = [fileTypes count];
    for (int n = 0; n < count; n++) {
        NSString* ext = (NSString*)[fileTypes objectAtIndex:n];
        CFStringRef uti = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, (CFStringRef)ext, NULL);
        if (!uti)
            continue;
        NSString* mime = (NSString*)UTTypeCopyPreferredTagWithClass(uti, kUTTagClassMIMEType);
        if (!mime)
            continue;
        types.add(String(mime));
    }
} 

}

@implementation WebCoreMovieObserver
-(void)loadStateChanged:(NSNotification *)notification
{
    callback->loadStateChanged();
}
-(void)rateChanged:(NSNotification *)notification
{
    callback->rateChanged();
}
-(void)sizeChanged:(NSNotification *)notification
{
    callback->sizeChanged();
}
-(void)timeChanged:(NSNotification *)notification
{
    callback->timeChanged();
}
-(void)volumeChanged:(NSNotification *)notification
{
    callback->volumeChanged();
}
-(void)didEnd:(NSNotification *)notification
{
    callback->didEnd();
}
-(void)setCallback:(WebCore::MoviePrivate *)c
{
    callback = c;
}
@end

#endif

