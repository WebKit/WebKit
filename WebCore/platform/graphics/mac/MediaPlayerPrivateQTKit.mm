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
#import "MediaPlayerPrivateQTKit.h"

#import "BlockExceptions.h"
#import "DeprecatedString.h"
#import "GraphicsContext.h"
#import "IntRect.h"
#import "KURL.h"
#import <limits>
#import "MIMETypeRegistry.h"
#import "MediaPlayer.h"
#import <QTKit/QTKit.h>
#import "ScrollView.h"
#import "WebCoreSystemInterface.h"
#import "Widget.h"
#import "wtf/RetainPtr.h"

@interface WebCoreMovieObserver : NSObject
{
    WebCore::MediaPlayerPrivate* _callback;
    BOOL _delayCallbacks;
}
-(id)initWithCallback:(WebCore::MediaPlayerPrivate *)c;
-(void)disconnect;
-(void)setDelayCallbacks:(BOOL)b;
-(void)loadStateChanged:(NSNotification *)notification;
-(void)rateChanged:(NSNotification *)notification;
-(void)sizeChanged:(NSNotification *)notification;
-(void)timeChanged:(NSNotification *)notification;
-(void)volumeChanged:(NSNotification *)notification;
-(void)didEnd:(NSNotification *)notification;
@end

namespace WebCore {
    
MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_player(player)
    , m_qtMovie(nil)
    , m_qtMovieView(nil)
    , m_objcObserver(AdoptNS, [[WebCoreMovieObserver alloc] initWithCallback:this])
    , m_seekTo(-1)
    , m_endTime(std::numeric_limits<float>::infinity())
    , m_seekTimer(this, &MediaPlayerPrivate::seekTimerFired)
    , m_cuePointTimer(this, &MediaPlayerPrivate::cuePointTimerFired)
    , m_previousTimeCueTimerFired(0)
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::DataUnavailable)
    , m_startedPlaying(false)
    , m_isStreaming(false)
{
}


MediaPlayerPrivate::~MediaPlayerPrivate()
{
    if (m_qtMovieView)
        [m_qtMovieView.get() removeFromSuperview];
    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];
    [m_objcObserver.get() disconnect];
}

void MediaPlayerPrivate::createQTMovie(String url)
{
    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];
    
    m_qtMovie = nil;
    
    NSError* error = nil;
    m_qtMovie = [[[QTMovie alloc] initWithURL:KURL(url.deprecatedString()).getNSURL() error:&error] autorelease];
    
    // FIXME: find a proper way to do this
    m_isStreaming = url.startsWith("rtsp:");
    
    if (!m_qtMovie)
        return;
    
    [m_qtMovie.get() setVolume: m_player->volume()];
    [m_qtMovie.get() setMuted: m_player->muted()];
    
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

void MediaPlayerPrivate::createQTMovieView()
{
    if (m_qtMovieView) {
        [m_qtMovieView.get() removeFromSuperview];
        m_qtMovieView = nil;
    }
    if (!m_player->m_parentWidget || !m_qtMovie)
        return;
    m_qtMovieView = [[[QTMovieView alloc] initWithFrame:m_player->rect()] autorelease];
    NSView* parentView = static_cast<ScrollView*>(m_player->m_parentWidget)->getDocumentView();
    [parentView addSubview:m_qtMovieView.get()];
    [m_qtMovieView.get() setMovie:m_qtMovie.get()];
    [m_qtMovieView.get() setControllerVisible:NO];
    [m_qtMovieView.get() setPreservesAspectRatio:YES];
    // the area not covered by video should be transparent
    NSColor* transparent = [NSColor colorWithDeviceRed: 0.0f green: 0.0f blue: 0.0f alpha: 0.0f];
    [m_qtMovieView.get() setFillColor:transparent];
    wkQTMovieViewSetDrawSynchronously(m_qtMovieView.get(), YES);
}

QTTime MediaPlayerPrivate::createQTTime(float time)
{
    if (!m_qtMovie)
        return QTMakeTime(0, 600);
    int timeScale = [[m_qtMovie.get() attributeForKey:QTMovieTimeScaleAttribute] intValue];
    return QTMakeTime((long long)(time * timeScale), timeScale);
}

void MediaPlayerPrivate::load(String url)
{
    if (m_networkState != MediaPlayer::Loading) {
        m_networkState = MediaPlayer::Loading;
        m_player->networkStateChanged();
    }
    if (m_readyState != MediaPlayer::DataUnavailable) {
        m_readyState = MediaPlayer::DataUnavailable;
        m_player->readyStateChanged();
    }
    cancelSeek();
    m_cuePointTimer.stop();
    
    [m_objcObserver.get() setDelayCallbacks:YES];

    createQTMovie(url);
    if (m_player->visible())
        createQTMovieView();

    [m_objcObserver.get() loadStateChanged:nil];
    [m_objcObserver.get() setDelayCallbacks:NO];
}

void MediaPlayerPrivate::play()
{
    if (!m_qtMovie)
        return;
    m_startedPlaying = true;
    [m_objcObserver.get() setDelayCallbacks:YES];
    [m_qtMovie.get() setRate: m_player->rate()];
    [m_objcObserver.get() setDelayCallbacks:NO];
    startCuePointTimerIfNeeded();
}

void MediaPlayerPrivate::pause()
{
    if (!m_qtMovie)
        return;
    m_startedPlaying = false;
    [m_objcObserver.get() setDelayCallbacks:YES];
    [m_qtMovie.get() stop];
    [m_objcObserver.get() setDelayCallbacks:NO];
    m_cuePointTimer.stop();
}

float MediaPlayerPrivate::duration() const
{
    if (!m_qtMovie)
        return 0;
    QTTime time = [m_qtMovie.get() duration];
    if (time.flags == kQTTimeIsIndefinite)
        return std::numeric_limits<float>::infinity();
    return (float)time.timeValue / time.timeScale;
}

float MediaPlayerPrivate::currentTime() const
{
    if (!m_qtMovie)
        return 0;
    QTTime time = [m_qtMovie.get() currentTime];
    float current = (float)time.timeValue / time.timeScale;    
    current = std::min(current, m_endTime);
    return current;
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
    QTTime qttime = createQTTime(m_seekTo);
    // setCurrentTime generates several event callbacks, update afterwards
    [m_objcObserver.get() setDelayCallbacks:YES];
    float oldRate = [m_qtMovie.get() rate];
    [m_qtMovie.get() setRate:0];
    [m_qtMovie.get() setCurrentTime: qttime];
    float timeAfterSeek = currentTime();
    // restore playback only if not at end, othewise QTMovie will loop
    if (timeAfterSeek < duration() && timeAfterSeek < m_endTime)
        [m_qtMovie.get() setRate:oldRate];
    cancelSeek();
    [m_objcObserver.get() setDelayCallbacks:NO];
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
    startCuePointTimerIfNeeded();
}

void MediaPlayerPrivate::addCuePoint(float time)
{
    // FIXME: simulate with timer for now
    startCuePointTimerIfNeeded();
}

void MediaPlayerPrivate::removeCuePoint(float time)
{
}

void MediaPlayerPrivate::clearCuePoints()
{
}

void MediaPlayerPrivate::startCuePointTimerIfNeeded()
{
    if ((m_endTime < duration() || !m_player->m_cuePoints.isEmpty())
        && m_startedPlaying && !m_cuePointTimer.isActive()) {
        m_previousTimeCueTimerFired = currentTime();
        m_cuePointTimer.startRepeating(0.020f);
    }
}

void MediaPlayerPrivate::cuePointTimerFired(Timer<MediaPlayerPrivate>*)
{
    float time = currentTime();
    float previousTime = m_previousTimeCueTimerFired;
    m_previousTimeCueTimerFired = time;
    
    // just do end for now
    if (time >= m_endTime) {
        pause();
        didEnd();
    }
    HashSet<float>::const_iterator end = m_player->m_cuePoints.end();
    for (HashSet<float>::const_iterator it = m_player->m_cuePoints.begin(); it != end; ++it) {
        float cueTime = *it;
        if (previousTime < cueTime && cueTime <= time)
            m_player->cuePointReached(cueTime);
    }
}

bool MediaPlayerPrivate::paused() const
{
    if (!m_qtMovie)
        return true;
    return [m_qtMovie.get() rate] == 0.0f;
}

bool MediaPlayerPrivate::seeking() const
{
    if (!m_qtMovie)
        return false;
    return m_seekTo >= 0;
}

IntSize MediaPlayerPrivate::naturalSize()
{
    if (!m_qtMovie)
        return IntSize();
    NSSize val = [[m_qtMovie.get() attributeForKey:QTMovieNaturalSizeAttribute] sizeValue];
    return IntSize(val);
}

bool MediaPlayerPrivate::hasVideo()
{
    if (!m_qtMovie)
        return false;
    BOOL val = [[m_qtMovie.get() attributeForKey: QTMovieHasVideoAttribute] boolValue];
    return val;
}

void MediaPlayerPrivate::setVolume(float volume)
{
    if (!m_qtMovie)
        return;
    [m_qtMovie.get() setVolume:volume];  
}

void MediaPlayerPrivate::setMuted(bool b)
{
    if (!m_qtMovie)
        return;
    [m_qtMovie.get() setMuted:b];
}

void MediaPlayerPrivate::setRate(float rate)
{
    if (!m_qtMovie)
        return;
    if (!paused())
        [m_qtMovie.get() setRate:rate];
}

int MediaPlayerPrivate::dataRate() const
{
    if (!m_qtMovie)
        return 0;
    return wkQTMovieDataRate(m_qtMovie.get()); 
}


MediaPlayer::NetworkState MediaPlayerPrivate::networkState()
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivate::readyState()
{
    return m_readyState;
}

float MediaPlayerPrivate::maxTimeBuffered()
{
    // rtsp streams are not buffered
    return m_isStreaming ? 0 : maxTimeLoaded();
}

float MediaPlayerPrivate::maxTimeSeekable()
{
    // infinite duration means live stream
    return isinf(duration()) ? 0 : maxTimeLoaded();
}

float MediaPlayerPrivate::maxTimeLoaded()
{
    if (!m_qtMovie)
        return 0;
    return wkQTMovieMaxTimeLoaded(m_qtMovie.get()); 
}

unsigned MediaPlayerPrivate::bytesLoaded()
{
    if (!m_qtMovie)
        return 0;
    float dur = duration();
    float maxTime = maxTimeLoaded();
    if (!dur)
        return 0;
    return totalBytes() * maxTime / dur;
}

bool MediaPlayerPrivate::totalBytesKnown()
{
    return totalBytes() > 0;
}

unsigned MediaPlayerPrivate::totalBytes()
{
    if (!m_qtMovie)
        return 0;
    return [[m_qtMovie.get() attributeForKey: QTMovieDataSizeAttribute] intValue];
}

void MediaPlayerPrivate::cancelLoad()
{
    // FIXME better way to do this?
    if (m_networkState < MediaPlayer::Loading || m_networkState == MediaPlayer::Loaded)
        return;
    
    if (m_qtMovieView) {
        [m_qtMovieView.get() removeFromSuperview];
        m_qtMovieView = nil;
    }
    m_qtMovie = nil;
    
    updateStates();
}

void MediaPlayerPrivate::updateStates()
{
    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;
    
    long loadState = m_qtMovie ? [[m_qtMovie.get() attributeForKey:QTMovieLoadStateAttribute] longValue] : -1;
    // "Loaded" is reserved for fully buffered movies, never the case when rtsp streaming
    if (loadState >= 100000 && !m_isStreaming) {
        // 100000 is kMovieLoadStateComplete
        if (m_networkState < MediaPlayer::Loaded)
            m_networkState = MediaPlayer::Loaded;
        m_readyState = MediaPlayer::CanPlayThrough;
    } else if (loadState >= 20000) {
        // 20000 is kMovieLoadStatePlaythroughOK
        if (m_networkState < MediaPlayer::LoadedFirstFrame && !seeking())
            m_networkState = MediaPlayer::LoadedFirstFrame;
        m_readyState = ([m_qtMovie.get() rate] == 0.0f && m_startedPlaying) ? MediaPlayer::DataUnavailable : MediaPlayer::CanPlayThrough;
    } else if (loadState >= 10000) {
        // 10000 is kMovieLoadStatePlayable
        if (m_networkState < MediaPlayer::LoadedFirstFrame && !seeking())
            m_networkState = MediaPlayer::LoadedFirstFrame;
        m_readyState = ([m_qtMovie.get() rate] == 0.0f && m_startedPlaying) ? MediaPlayer::DataUnavailable : MediaPlayer::CanPlay;
    } else if (loadState >= 2000) {
        // 10000 is kMovieLoadStateLoaded
        if (m_networkState < MediaPlayer::LoadedMetaData)
            m_networkState = MediaPlayer::LoadedMetaData;
        m_readyState = MediaPlayer::DataUnavailable;
    } else if (loadState >= 0) {
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

void MediaPlayerPrivate::loadStateChanged()
{
    updateStates();
}

void MediaPlayerPrivate::rateChanged()
{
    updateStates();
}

void MediaPlayerPrivate::sizeChanged()
{
}

void MediaPlayerPrivate::timeChanged()
{
    m_previousTimeCueTimerFired = -1;
    updateStates();
    m_player->timeChanged();
}

void MediaPlayerPrivate::volumeChanged()
{
    m_player->volumeChanged();
}

void MediaPlayerPrivate::didEnd()
{
    m_cuePointTimer.stop();
    m_startedPlaying = false;
    updateStates();
    m_player->timeChanged();
}

void MediaPlayerPrivate::setRect(const IntRect& r) 
{ 
    if (m_qtMovieView)
        [m_qtMovieView.get() setFrame: r];
}

void MediaPlayerPrivate::setVisible(bool b)
{
    if (b)
        createQTMovieView();
    else if (m_qtMovieView) {
        [m_qtMovieView.get() removeFromSuperview];
        m_qtMovieView = nil;
    }
}

void MediaPlayerPrivate::paint(GraphicsContext* p, const IntRect& r)
{
    if (p->paintingDisabled())
        return;
    NSView *view = m_qtMovieView.get();
    if (view == nil)
        return;
    [m_objcObserver.get() setDelayCallbacks:YES];
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [view displayRectIgnoringOpacity:[view convertRect:r fromView:[view superview]]];
    END_BLOCK_OBJC_EXCEPTIONS;
    [m_objcObserver.get() setDelayCallbacks:NO];
}

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>& types)
{
    NSArray* fileTypes = [QTMovie movieFileTypes:(QTMovieFileTypeOptions)0];
    int count = [fileTypes count];
    for (int n = 0; n < count; n++) {
        NSString* ext = (NSString*)[fileTypes objectAtIndex:n];
        CFStringRef uti = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, (CFStringRef)ext, NULL);
        if (!uti)
            continue;
        CFStringRef mime = UTTypeCopyPreferredTagWithClass(uti, kUTTagClassMIMEType);
        CFRelease(uti);
        if (!mime)
            continue;
        types.add(String((NSString*)mime));
        CFRelease(mime);
    }
} 

}

@implementation WebCoreMovieObserver
-(id)initWithCallback:(WebCore::MediaPlayerPrivate *)c
{
    _callback = c;
    return [super init];
}
-(void)disconnect
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    _callback = 0;
}
-(void)loadStateChanged:(NSNotification *)notification
{
    if (_delayCallbacks)
        [self performSelector:@selector(loadStateChanged:) withObject:nil afterDelay:0];
    else
        _callback->loadStateChanged();
}
-(void)rateChanged:(NSNotification *)notification
{
    if (_delayCallbacks)
        [self performSelector:@selector(rateChanged:) withObject:nil afterDelay:0];
    else
        _callback->rateChanged();
}
-(void)sizeChanged:(NSNotification *)notification
{
    if (_delayCallbacks)
        [self performSelector:@selector(sizeChanged:) withObject:nil afterDelay:0];
    else
        _callback->sizeChanged();
}
-(void)timeChanged:(NSNotification *)notification
{
    if (_delayCallbacks)
        [self performSelector:@selector(timeChanged:) withObject:nil afterDelay:0];
    else
        _callback->timeChanged();
}
-(void)volumeChanged:(NSNotification *)notification
{
    if (_delayCallbacks)
        [self performSelector:@selector(volumeChanged:) withObject:nil afterDelay:0];
    else
        _callback->volumeChanged();
}
-(void)didEnd:(NSNotification *)notification
{
    if (_delayCallbacks)
        [self performSelector:@selector(didEnd:) withObject:nil afterDelay:0];
    else
        _callback->didEnd();
}
-(void)setDelayCallbacks:(BOOL)b
{
    _delayCallbacks = b;
}
@end

#endif

