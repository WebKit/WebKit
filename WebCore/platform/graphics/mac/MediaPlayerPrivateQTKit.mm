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

#import "config.h"

#if ENABLE(VIDEO)

#import "MediaPlayerPrivateQTKit.h"

#ifdef BUILDING_ON_TIGER
#import "AutodrainedPool.h"
#endif

#import "BlockExceptions.h"
#import "FrameView.h"
#import "GraphicsContext.h"
#import "KURL.h"
#import "SoftLinking.h"
#import "WebCoreSystemInterface.h"
#import <QTKit/QTKit.h>
#import <objc/objc-runtime.h>
#import <wtf/UnusedParam.h>

#if DRAW_FRAME_RATE
#import "Font.h"
#import "Frame.h"
#import "Document.h"
#import "RenderObject.h"
#import "RenderStyle.h"
#endif

#ifdef BUILDING_ON_TIGER
static IMP method_setImplementation(Method m, IMP imp)
{
    IMP result = m->method_imp;
    m->method_imp = imp;
    return result;
}
#endif

SOFT_LINK_FRAMEWORK(QTKit)

SOFT_LINK(QTKit, QTMakeTime, QTTime, (long long timeValue, long timeScale), (timeValue, timeScale))

SOFT_LINK_CLASS(QTKit, QTMovie)
SOFT_LINK_CLASS(QTKit, QTMovieView)

SOFT_LINK_POINTER(QTKit, QTMediaTypeAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMediaTypeBase, NSString *)
SOFT_LINK_POINTER(QTKit, QTMediaTypeMPEG, NSString *)
SOFT_LINK_POINTER(QTKit, QTMediaTypeSound, NSString *)
SOFT_LINK_POINTER(QTKit, QTMediaTypeText, NSString *)
SOFT_LINK_POINTER(QTKit, QTMediaTypeVideo, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieAskUnresolvedDataRefsAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieDataSizeAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieDidEndNotification, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieHasVideoAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieIsActiveAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieLoadStateAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieLoadStateDidChangeNotification, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieNaturalSizeAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMoviePreventExternalURLLinksAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieRateDidChangeNotification, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieSizeDidChangeNotification, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieTimeDidChangeNotification, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieTimeScaleAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieURLAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieVolumeDidChangeNotification, NSString *)
SOFT_LINK_POINTER(QTKit, QTSecurityPolicyNoCrossSiteAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTVideoRendererWebKitOnlyNewImageAvailableNotification, NSString *)

#define QTMovie getQTMovieClass()
#define QTMovieView getQTMovieViewClass()

#define QTMediaTypeAttribute getQTMediaTypeAttribute()
#define QTMediaTypeBase getQTMediaTypeBase()
#define QTMediaTypeMPEG getQTMediaTypeMPEG()
#define QTMediaTypeSound getQTMediaTypeSound()
#define QTMediaTypeText getQTMediaTypeText()
#define QTMediaTypeVideo getQTMediaTypeVideo()
#define QTMovieAskUnresolvedDataRefsAttribute getQTMovieAskUnresolvedDataRefsAttribute()
#define QTMovieDataSizeAttribute getQTMovieDataSizeAttribute()
#define QTMovieDidEndNotification getQTMovieDidEndNotification()
#define QTMovieHasVideoAttribute getQTMovieHasVideoAttribute()
#define QTMovieIsActiveAttribute getQTMovieIsActiveAttribute()
#define QTMovieLoadStateAttribute getQTMovieLoadStateAttribute()
#define QTMovieLoadStateDidChangeNotification getQTMovieLoadStateDidChangeNotification()
#define QTMovieNaturalSizeAttribute getQTMovieNaturalSizeAttribute()
#define QTMoviePreventExternalURLLinksAttribute getQTMoviePreventExternalURLLinksAttribute()
#define QTMovieRateDidChangeNotification getQTMovieRateDidChangeNotification()
#define QTMovieSizeDidChangeNotification getQTMovieSizeDidChangeNotification()
#define QTMovieTimeDidChangeNotification getQTMovieTimeDidChangeNotification()
#define QTMovieTimeScaleAttribute getQTMovieTimeScaleAttribute()
#define QTMovieURLAttribute getQTMovieURLAttribute()
#define QTMovieVolumeDidChangeNotification getQTMovieVolumeDidChangeNotification()
#define QTSecurityPolicyNoCrossSiteAttribute getQTSecurityPolicyNoCrossSiteAttribute()
#define QTVideoRendererWebKitOnlyNewImageAvailableNotification getQTVideoRendererWebKitOnlyNewImageAvailableNotification()

// Older versions of the QTKit header don't have these constants.
#if !defined QTKIT_VERSION_MAX_ALLOWED || QTKIT_VERSION_MAX_ALLOWED <= QTKIT_VERSION_7_0
enum {
    QTMovieLoadStateError = -1L,
    QTMovieLoadStateLoaded  = 2000L,
    QTMovieLoadStatePlayable = 10000L,
    QTMovieLoadStatePlaythroughOK = 20000L,
    QTMovieLoadStateComplete = 100000L
};
#endif

using namespace WebCore;
using namespace std;

@interface WebCoreMovieObserver : NSObject
{
    MediaPlayerPrivate* m_callback;
    NSView* m_view;
    BOOL m_delayCallbacks;
}
-(id)initWithCallback:(MediaPlayerPrivate*)callback;
-(void)disconnect;
-(void)setView:(NSView*)view;
-(void)repaint;
-(void)setDelayCallbacks:(BOOL)shouldDelay;
-(void)loadStateChanged:(NSNotification *)notification;
-(void)rateChanged:(NSNotification *)notification;
-(void)sizeChanged:(NSNotification *)notification;
-(void)timeChanged:(NSNotification *)notification;
-(void)didEnd:(NSNotification *)notification;
@end

@protocol WebKitVideoRenderingDetails
-(void)setMovie:(id)movie;
-(void)drawInRect:(NSRect)rect;
@end

namespace WebCore {

static const float endPointTimerInterval = 0.020f;

#ifdef BUILDING_ON_TIGER
static const long minimumQuickTimeVersion = 0x07300000; // 7.3
#endif


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
    , m_objcObserver(AdoptNS, [[WebCoreMovieObserver alloc] initWithCallback:this])
    , m_seekTo(-1)
    , m_endTime(numeric_limits<float>::infinity())
    , m_seekTimer(this, &MediaPlayerPrivate::seekTimerFired)
    , m_endPointTimer(this, &MediaPlayerPrivate::endPointTimerFired)
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::DataUnavailable)
    , m_startedPlaying(false)
    , m_isStreaming(false)
    , m_visible(false)
#if DRAW_FRAME_RATE
    , m_frameCountWhilePlaying(0)
    , m_timeStartedPlaying(0)
    , m_timeStoppedPlaying(0)
#endif
{
}

MediaPlayerPrivate::~MediaPlayerPrivate()
{
    tearDownVideoRendering();

    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];
    [m_objcObserver.get() disconnect];
}

void MediaPlayerPrivate::createQTMovie(const String& url)
{
    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];
    
    if (m_qtMovie) {
        destroyQTVideoRenderer();
        m_qtMovie = 0;
    }
    
    // Disable streaming support for now, <rdar://problem/5693967>
    if (protocolIs(url, "rtsp"))
        return;

    NSURL *cocoaURL = KURL(url);
    NSDictionary *movieAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
                                     cocoaURL, QTMovieURLAttribute,
                                     [NSNumber numberWithBool:YES], QTMoviePreventExternalURLLinksAttribute,
                                     [NSNumber numberWithBool:YES], QTSecurityPolicyNoCrossSiteAttribute,
                                     [NSNumber numberWithBool:NO], QTMovieAskUnresolvedDataRefsAttribute,
                                     [NSNumber numberWithBool:YES], @"QTMovieOpenForPlaybackAttribute",     // FIXME: Use defined attribute when required version of QT supports this attribute
                                     nil];
    
    NSError* error = nil;
    m_qtMovie.adoptNS([[QTMovie alloc] initWithAttributes:movieAttributes error:&error]);
    
    // FIXME: Find a proper way to detect streaming content.
    m_isStreaming = protocolIs(url, "rtsp");
    
    if (!m_qtMovie)
        return;
    
    [m_qtMovie.get() setVolume:m_player->volume()];
    
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
                                             selector:@selector(didEnd:) 
                                                 name:QTMovieDidEndNotification 
                                               object:m_qtMovie.get()];
}

static void mainThreadSetNeedsDisplay(id self, SEL)
{
    id movieView = [self superview];
    ASSERT(!movieView || [movieView isKindOfClass:[QTMovieView class]]);
    if (!movieView || ![movieView isKindOfClass:[QTMovieView class]])
        return;

    WebCoreMovieObserver* delegate = [movieView delegate];
    ASSERT(!delegate || [delegate isKindOfClass:[WebCoreMovieObserver class]]);
    if (!delegate || ![delegate isKindOfClass:[WebCoreMovieObserver class]])
        return;

    [delegate repaint];
}

static Class QTVideoRendererClass()
{
     static Class QTVideoRendererWebKitOnlyClass = NSClassFromString(@"QTVideoRendererWebKitOnly");
     return QTVideoRendererWebKitOnlyClass;
}

void MediaPlayerPrivate::createQTMovieView()
{
    detachQTMovieView();

    static bool addedCustomMethods = false;
    if (!addedCustomMethods) {
        Class QTMovieContentViewClass = NSClassFromString(@"QTMovieContentView");
        ASSERT(QTMovieContentViewClass);

        Method mainThreadSetNeedsDisplayMethod = class_getInstanceMethod(QTMovieContentViewClass, @selector(_mainThreadSetNeedsDisplay));
        ASSERT(mainThreadSetNeedsDisplayMethod);

        method_setImplementation(mainThreadSetNeedsDisplayMethod, reinterpret_cast<IMP>(mainThreadSetNeedsDisplay));
        addedCustomMethods = true;
    }

    m_qtMovieView.adoptNS([[QTMovieView alloc] init]);
    setRect(m_player->rect());
    NSView* parentView = m_player->frameView()->documentView();
    [parentView addSubview:m_qtMovieView.get()];
#ifdef BUILDING_ON_TIGER
    // setDelegate: isn't a public call in Tiger, so use performSelector to keep the compiler happy
    [m_qtMovieView.get() performSelector:@selector(setDelegate:) withObject:m_objcObserver.get()];    
#else
    [m_qtMovieView.get() setDelegate:m_objcObserver.get()];
#endif
    [m_objcObserver.get() setView:m_qtMovieView.get()];
    [m_qtMovieView.get() setMovie:m_qtMovie.get()];
    [m_qtMovieView.get() setControllerVisible:NO];
    [m_qtMovieView.get() setPreservesAspectRatio:NO];
    // the area not covered by video should be transparent
    [m_qtMovieView.get() setFillColor:[NSColor clearColor]];

    // If we're in a media document, allow QTMovieView to render in its default mode;
    // otherwise tell it to draw synchronously.
    // Note that we expect mainThreadSetNeedsDisplay to be invoked only when synchronous drawing is requested.
    if (!m_player->inMediaDocument())
        wkQTMovieViewSetDrawSynchronously(m_qtMovieView.get(), YES);
}

void MediaPlayerPrivate::detachQTMovieView()
{
    if (m_qtMovieView) {
        [m_objcObserver.get() setView:nil];
#ifdef BUILDING_ON_TIGER
        // setDelegate: isn't a public call in Tiger, so use performSelector to keep the compiler happy
        [m_qtMovieView.get() performSelector:@selector(setDelegate:) withObject:nil];    
#else
        [m_qtMovieView.get() setDelegate:nil];
#endif
        [m_qtMovieView.get() removeFromSuperview];
        m_qtMovieView = nil;
    }
}

void MediaPlayerPrivate::createQTVideoRenderer()
{
    destroyQTVideoRenderer();

    m_qtVideoRenderer.adoptNS([[QTVideoRendererClass() alloc] init]);
    if (!m_qtVideoRenderer)
        return;
    
    // associate our movie with our instance of QTVideoRendererWebKitOnly
    [(id<WebKitVideoRenderingDetails>)m_qtVideoRenderer.get() setMovie:m_qtMovie.get()];    

    // listen to QTVideoRendererWebKitOnly's QTVideoRendererWebKitOnlyNewImageDidBecomeAvailableNotification
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get()
                                             selector:@selector(newImageAvailable:)
                                                 name:QTVideoRendererWebKitOnlyNewImageAvailableNotification
                                               object:m_qtVideoRenderer.get()];
}

void MediaPlayerPrivate::destroyQTVideoRenderer()
{
    if (!m_qtVideoRenderer)
        return;

    // stop observing the renderer's notifications before we toss it
    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()
                                                    name:QTVideoRendererWebKitOnlyNewImageAvailableNotification
                                                  object:m_qtVideoRenderer.get()];

    // disassociate our movie from our instance of QTVideoRendererWebKitOnly
    [(id<WebKitVideoRenderingDetails>)m_qtVideoRenderer.get() setMovie:nil];    

    m_qtVideoRenderer = nil;
}

void MediaPlayerPrivate::setUpVideoRendering()
{
    if (!m_player->frameView() || !m_qtMovie)
        return;

    if (m_player->inMediaDocument() || !QTVideoRendererClass() )
        createQTMovieView();
    else
        createQTVideoRenderer();
}

void MediaPlayerPrivate::tearDownVideoRendering()
{
    if (m_qtMovieView)
        detachQTMovieView();
    else
        destroyQTVideoRenderer();
}

QTTime MediaPlayerPrivate::createQTTime(float time) const
{
    if (!metaDataAvailable())
        return QTMakeTime(0, 600);
    long timeScale = [[m_qtMovie.get() attributeForKey:QTMovieTimeScaleAttribute] longValue];
    return QTMakeTime(time * timeScale, timeScale);
}

void MediaPlayerPrivate::load(const String& url)
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
    m_endPointTimer.stop();
    
    [m_objcObserver.get() setDelayCallbacks:YES];

    createQTMovie(url);

    [m_objcObserver.get() loadStateChanged:nil];
    [m_objcObserver.get() setDelayCallbacks:NO];
}

void MediaPlayerPrivate::play()
{
    if (!metaDataAvailable())
        return;
    m_startedPlaying = true;
#if DRAW_FRAME_RATE
    m_frameCountWhilePlaying = 0;
#endif
    [m_objcObserver.get() setDelayCallbacks:YES];
    [m_qtMovie.get() setRate:m_player->rate()];
    [m_objcObserver.get() setDelayCallbacks:NO];
    startEndPointTimerIfNeeded();
}

void MediaPlayerPrivate::pause()
{
    if (!metaDataAvailable())
        return;
    m_startedPlaying = false;
#if DRAW_FRAME_RATE
    m_timeStoppedPlaying = [NSDate timeIntervalSinceReferenceDate];
#endif
    [m_objcObserver.get() setDelayCallbacks:YES];
    [m_qtMovie.get() stop];
    [m_objcObserver.get() setDelayCallbacks:NO];
    m_endPointTimer.stop();
}

float MediaPlayerPrivate::duration() const
{
    if (!metaDataAvailable())
        return 0;
    QTTime time = [m_qtMovie.get() duration];
    if (time.flags == kQTTimeIsIndefinite)
        return numeric_limits<float>::infinity();
    return static_cast<float>(time.timeValue) / time.timeScale;
}

float MediaPlayerPrivate::currentTime() const
{
    if (!metaDataAvailable())
        return 0;
    QTTime time = [m_qtMovie.get() currentTime];
    return min(static_cast<float>(time.timeValue) / time.timeScale, m_endTime);
}

void MediaPlayerPrivate::seek(float time)
{
    cancelSeek();
    
    if (!metaDataAvailable())
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
    [m_qtMovie.get() setCurrentTime:qttime];
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
    if (!metaDataAvailable()|| !seeking() || currentTime() == m_seekTo) {
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
    
    // just do end for now
    if (time >= m_endTime) {
        pause();
        didEnd();
    }
}

bool MediaPlayerPrivate::paused() const
{
    if (!metaDataAvailable())
        return true;
    return [m_qtMovie.get() rate] == 0;
}

bool MediaPlayerPrivate::seeking() const
{
    if (!metaDataAvailable())
        return false;
    return m_seekTo >= 0;
}

IntSize MediaPlayerPrivate::naturalSize() const
{
    if (!metaDataAvailable())
        return IntSize();
    return IntSize([[m_qtMovie.get() attributeForKey:QTMovieNaturalSizeAttribute] sizeValue]);
}

bool MediaPlayerPrivate::hasVideo() const
{
    if (!metaDataAvailable())
        return false;
    return [[m_qtMovie.get() attributeForKey:QTMovieHasVideoAttribute] boolValue];
}

void MediaPlayerPrivate::setVolume(float volume)
{
    if (!metaDataAvailable())
        return;
    [m_qtMovie.get() setVolume:volume];  
}

void MediaPlayerPrivate::setRate(float rate)
{
    if (!metaDataAvailable())
        return;
    if (!paused())
        [m_qtMovie.get() setRate:rate];
}

int MediaPlayerPrivate::dataRate() const
{
    if (!metaDataAvailable())
        return 0;
    return wkQTMovieDataRate(m_qtMovie.get()); 
}


float MediaPlayerPrivate::maxTimeBuffered() const
{
    // rtsp streams are not buffered
    return m_isStreaming ? 0 : maxTimeLoaded();
}

float MediaPlayerPrivate::maxTimeSeekable() const
{
    // infinite duration means live stream
    return isinf(duration()) ? 0 : maxTimeLoaded();
}

float MediaPlayerPrivate::maxTimeLoaded() const
{
    if (!metaDataAvailable())
        return 0;
    return wkQTMovieMaxTimeLoaded(m_qtMovie.get()); 
}

unsigned MediaPlayerPrivate::bytesLoaded() const
{
    float dur = duration();
    if (!dur)
        return 0;
    return totalBytes() * maxTimeLoaded() / dur;
}

bool MediaPlayerPrivate::totalBytesKnown() const
{
    return totalBytes() > 0;
}

unsigned MediaPlayerPrivate::totalBytes() const
{
    if (!metaDataAvailable())
        return 0;
    return [[m_qtMovie.get() attributeForKey:QTMovieDataSizeAttribute] intValue];
}

void MediaPlayerPrivate::cancelLoad()
{
    // FIXME: Is there a better way to check for this?
    if (m_networkState < MediaPlayer::Loading || m_networkState == MediaPlayer::Loaded)
        return;
    
    tearDownVideoRendering();
    m_qtMovie = nil;
    
    updateStates();
}

void MediaPlayerPrivate::updateStates()
{
    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;
    
    long loadState = m_qtMovie ? [[m_qtMovie.get() attributeForKey:QTMovieLoadStateAttribute] longValue] : static_cast<long>(QTMovieLoadStateError);
    
    if (loadState >= QTMovieLoadStateLoaded && m_networkState < MediaPlayer::LoadedMetaData && !m_player->inMediaDocument()) {
        unsigned enabledTrackCount;
        disableUnsupportedTracks(enabledTrackCount);
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
        // FIXME: This might not work correctly in streaming case, <rdar://problem/5693967>
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

    if (loadState >= QTMovieLoadStateLoaded && oldNetworkState < MediaPlayer::LoadedMetaData && m_player->visible())
        setUpVideoRendering();
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
    updateStates();
    m_player->timeChanged();
}

void MediaPlayerPrivate::didEnd()
{
    m_endPointTimer.stop();
    m_startedPlaying = false;
#if DRAW_FRAME_RATE
    m_timeStoppedPlaying = [NSDate timeIntervalSinceReferenceDate];
#endif
    updateStates();
    m_player->timeChanged();
}

void MediaPlayerPrivate::setRect(const IntRect& r) 
{ 
    if (!m_qtMovieView) 
        return;

    if (m_player->inMediaDocument())
        // We need the QTMovieView to be placed in the proper location for document mode.
        [m_qtMovieView.get() setFrame:r];
    else {
        // We don't really need the QTMovieView in any specific location so let's just get it out of the way
        // where it won't intercept events or try to bring up the context menu.
        IntRect farAwayButCorrectSize(r);
        farAwayButCorrectSize.move(-1000000, -1000000);
        [m_qtMovieView.get() setFrame:farAwayButCorrectSize];
    }   
}

void MediaPlayerPrivate::setVisible(bool b)
{
    if (m_visible != b) {
        m_visible = b;
        if (b) {
            if (m_networkState >= MediaPlayer::LoadedMetaData)
                setUpVideoRendering();
        } else
            tearDownVideoRendering();
    }
}

void MediaPlayerPrivate::repaint()
{
#if DRAW_FRAME_RATE
    if (m_startedPlaying) {
        m_frameCountWhilePlaying++;
        // to eliminate preroll costs from our calculation,
        // our frame rate calculation excludes the first frame drawn after playback starts
        if (1==m_frameCountWhilePlaying)
            m_timeStartedPlaying = [NSDate timeIntervalSinceReferenceDate];
    }
#endif
    m_player->repaint();
}

void MediaPlayerPrivate::paint(GraphicsContext* context, const IntRect& r)
{
    if (context->paintingDisabled())
        return;
    NSView *view = m_qtMovieView.get();
    id qtVideoRenderer = m_qtVideoRenderer.get();
    if (!view && !qtVideoRenderer)
        return;

    [m_objcObserver.get() setDelayCallbacks:YES];
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    context->save();
    context->translate(r.x(), r.y() + r.height());
    context->scale(FloatSize(1.0f, -1.0f));
    context->setImageInterpolationQuality(InterpolationLow);
    IntRect paintRect(IntPoint(0, 0), IntSize(r.width(), r.height()));
    
#ifdef BUILDING_ON_TIGER
    AutodrainedPool pool;
#endif
    NSGraphicsContext* newContext = [NSGraphicsContext graphicsContextWithGraphicsPort:context->platformContext() flipped:NO];

    // draw the current video frame
    if (qtVideoRenderer) {
        [NSGraphicsContext saveGraphicsState];
        [NSGraphicsContext setCurrentContext:newContext];
        [(id<WebKitVideoRenderingDetails>)qtVideoRenderer drawInRect:paintRect];
        [NSGraphicsContext restoreGraphicsState];
    } else
        [view displayRectIgnoringOpacity:paintRect inContext:newContext];

#if DRAW_FRAME_RATE
    // Draw the frame rate only after having played more than 10 frames.
    if (m_frameCountWhilePlaying > 10) {
        Frame* frame = m_player->frameView() ? m_player->frameView()->frame() : NULL;
        Document* document = frame ? frame->document() : NULL;
        RenderObject* renderer = document ? document->renderer() : NULL;
        RenderStyle* styleToUse = renderer ? renderer->style() : NULL;
        if (styleToUse) {
            double frameRate = (m_frameCountWhilePlaying - 1) / ( m_startedPlaying ? ([NSDate timeIntervalSinceReferenceDate] - m_timeStartedPlaying) :
                (m_timeStoppedPlaying - m_timeStartedPlaying) );
            String text = String::format("%1.2f", frameRate);
            TextRun textRun(text.characters(), text.length());
            const Color color(255, 0, 0);
            context->scale(FloatSize(1.0f, -1.0f));    
            context->setFont(styleToUse->font());
            context->setStrokeColor(color);
            context->setStrokeStyle(SolidStroke);
            context->setStrokeThickness(1.0f);
            context->setFillColor(color);
            context->drawText(textRun, IntPoint(2, -3));
        }
    }
#endif

    context->restore();
    END_BLOCK_OBJC_EXCEPTIONS;
    [m_objcObserver.get() setDelayCallbacks:NO];
}

static HashSet<String> mimeTypeCache()
{
    DEFINE_STATIC_LOCAL(HashSet<String>, cache, ());
    static bool typeListInitialized = false;

    if (!typeListInitialized) {
        NSArray* fileTypes = [QTMovie movieFileTypes:QTIncludeCommonTypes];
        int count = [fileTypes count];
        for (int n = 0; n < count; n++) {
            CFStringRef ext = reinterpret_cast<CFStringRef>([fileTypes objectAtIndex:n]);
            RetainPtr<CFStringRef> uti(AdoptCF, UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, ext, NULL));
            if (!uti)
                continue;
            RetainPtr<CFStringRef> mime(AdoptCF, UTTypeCopyPreferredTagWithClass(uti.get(), kUTTagClassMIMEType));
            if (!mime)
                continue;
            cache.add(mime.get());
        }
        typeListInitialized = true;
    }
    
    return cache;
} 

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>& types)
{
    types = mimeTypeCache();
} 

MediaPlayer::SupportsType MediaPlayerPrivate::supportsType(const String& type, const String& codecs)
{
    // only return "IsSupported" if there is no codecs parameter for now as there is no way to ask QT if it supports an
    //  extended MIME type yet
    return mimeTypeCache().contains(type) ? (codecs && !codecs.isEmpty() ? MediaPlayer::MayBeSupported : MediaPlayer::IsSupported) : MediaPlayer::IsNotSupported;
}

bool MediaPlayerPrivate::isAvailable()
{
#ifdef BUILDING_ON_TIGER
    SInt32 version;
    OSErr result;
    result = Gestalt(gestaltQuickTime, &version);
    if (result != noErr) {
        LOG_ERROR("No QuickTime available. Disabling <video> and <audio> support.");
        return false;
    }
    if (version < minimumQuickTimeVersion) {
        LOG_ERROR("QuickTime version %x detected, at least %x required. Disabling <video> and <audio> support.", version, minimumQuickTimeVersion);
        return false;
    }
    return true;
#else
    // On 10.5 and higher, QuickTime will always be new enough for <video> and <audio> support, so we just check that the framework can be loaded.
    return QTKitLibrary();
#endif
}
    
void MediaPlayerPrivate::disableUnsupportedTracks(unsigned& enabledTrackCount)
{
    if (!m_qtMovie) {
        enabledTrackCount = 0;
        return;
    }
    
    static HashSet<String>* allowedTrackTypes = 0;
    if (!allowedTrackTypes) {
        allowedTrackTypes = new HashSet<String>;
        allowedTrackTypes->add(QTMediaTypeVideo);
        allowedTrackTypes->add(QTMediaTypeSound);
        allowedTrackTypes->add(QTMediaTypeText);
        allowedTrackTypes->add(QTMediaTypeBase);
        allowedTrackTypes->add(QTMediaTypeMPEG);
        allowedTrackTypes->add("clcp");
        allowedTrackTypes->add("sbtl");
    }
    
    NSArray *tracks = [m_qtMovie.get() tracks];
    
    unsigned trackCount = [tracks count];
    enabledTrackCount = trackCount;
    for (unsigned trackIndex = 0; trackIndex < trackCount; trackIndex++) {
        // Grab the track at the current index. If there isn't one there, then
        // we can move onto the next one.
        QTTrack *track = [tracks objectAtIndex:trackIndex];
        if (!track)
            continue;
        
        // Check to see if the track is disabled already, we should move along.
        // We don't need to re-disable it.
        if (![track isEnabled])
            continue;
        
        // Grab the track's media. We're going to check to see if we need to
        // disable the tracks. They could be unsupported.
        QTMedia *trackMedia = [track media];
        if (!trackMedia)
            continue;
        
        // Grab the media type for this track.
        NSString *mediaType = [trackMedia attributeForKey:QTMediaTypeAttribute];
        if (!mediaType)
            continue;
        
        // Test whether the media type is in our white list.
        if (!allowedTrackTypes->contains(mediaType)) {
            // If this track type is not allowed, then we need to disable it.
            [track setEnabled:NO];
            --enabledTrackCount;
        }
        
        // Disable chapter tracks. These are most likely to lead to trouble, as
        // they will be composited under the video tracks, forcing QT to do extra
        // work.
        QTTrack *chapterTrack = [track performSelector:@selector(chapterlist)];
        if (!chapterTrack)
            continue;
        
        // Try to grab the media for the track.
        QTMedia *chapterMedia = [chapterTrack media];
        if (!chapterMedia)
            continue;
        
        // Grab the media type for this track.
        id chapterMediaType = [chapterMedia attributeForKey:QTMediaTypeAttribute];
        if (!chapterMediaType)
            continue;
        
        // Check to see if the track is a video track. We don't care about
        // other non-video tracks.
        if (![chapterMediaType isEqual:QTMediaTypeVideo])
            continue;
        
        // Check to see if the track is already disabled. If it is, we
        // should move along.
        if (![chapterTrack isEnabled])
            continue;
        
        // Disable the evil, evil track.
        [chapterTrack setEnabled:NO];
        --enabledTrackCount;
    }
}

}

@implementation WebCoreMovieObserver

- (id)initWithCallback:(MediaPlayerPrivate*)callback
{
    m_callback = callback;
    return [super init];
}

- (void)disconnect
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    m_callback = 0;
}

-(NSMenu*)menuForEventDelegate:(NSEvent*)theEvent
{
    // Get the contextual menu from the QTMovieView's superview, the frame view
    return [[m_view superview] menuForEvent:theEvent];
}

-(void)setView:(NSView*)view
{
    m_view = view;
}

-(void)repaint
{
    if (m_delayCallbacks)
        [self performSelector:_cmd withObject:nil afterDelay:0.];
    else if (m_callback)
        m_callback->repaint();
}

- (void)loadStateChanged:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    if (m_delayCallbacks)
        [self performSelector:_cmd withObject:nil afterDelay:0];
    else
        m_callback->loadStateChanged();
}

- (void)rateChanged:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    if (m_delayCallbacks)
        [self performSelector:_cmd withObject:nil afterDelay:0];
    else
        m_callback->rateChanged();
}

- (void)sizeChanged:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    if (m_delayCallbacks)
        [self performSelector:_cmd withObject:nil afterDelay:0];
    else
        m_callback->sizeChanged();
}

- (void)timeChanged:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    if (m_delayCallbacks)
        [self performSelector:_cmd withObject:nil afterDelay:0];
    else
        m_callback->timeChanged();
}

- (void)didEnd:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    if (m_delayCallbacks)
        [self performSelector:_cmd withObject:nil afterDelay:0];
    else
        m_callback->didEnd();
}

- (void)newImageAvailable:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    [self repaint];
}

- (void)setDelayCallbacks:(BOOL)shouldDelay
{
    m_delayCallbacks = shouldDelay;
}

@end

#endif
