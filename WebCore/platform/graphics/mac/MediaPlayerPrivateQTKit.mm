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

SOFT_LINK_POINTER(QTKit, QTTrackMediaTypeAttribute, NSString *)
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
SOFT_LINK_POINTER(QTKit, QTMovieCurrentSizeAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMoviePreventExternalURLLinksAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieRateDidChangeNotification, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieSizeDidChangeNotification, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieTimeDidChangeNotification, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieTimeScaleAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieURLAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieVolumeDidChangeNotification, NSString *)
SOFT_LINK_POINTER(QTKit, QTSecurityPolicyNoCrossSiteAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTVideoRendererWebKitOnlyNewImageAvailableNotification, NSString *)
#ifndef BUILDING_ON_TIGER
SOFT_LINK_POINTER(QTKit, QTMovieApertureModeClean, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieApertureModeAttribute, NSString *)
#endif

#define QTMovie getQTMovieClass()
#define QTMovieView getQTMovieViewClass()

#define QTTrackMediaTypeAttribute getQTTrackMediaTypeAttribute()
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
#define QTMovieCurrentSizeAttribute getQTMovieCurrentSizeAttribute()
#define QTMoviePreventExternalURLLinksAttribute getQTMoviePreventExternalURLLinksAttribute()
#define QTMovieRateDidChangeNotification getQTMovieRateDidChangeNotification()
#define QTMovieSizeDidChangeNotification getQTMovieSizeDidChangeNotification()
#define QTMovieTimeDidChangeNotification getQTMovieTimeDidChangeNotification()
#define QTMovieTimeScaleAttribute getQTMovieTimeScaleAttribute()
#define QTMovieURLAttribute getQTMovieURLAttribute()
#define QTMovieVolumeDidChangeNotification getQTMovieVolumeDidChangeNotification()
#define QTSecurityPolicyNoCrossSiteAttribute getQTSecurityPolicyNoCrossSiteAttribute()
#define QTVideoRendererWebKitOnlyNewImageAvailableNotification getQTVideoRendererWebKitOnlyNewImageAvailableNotification()
#ifndef BUILDING_ON_TIGER
#define QTMovieApertureModeClean getQTMovieApertureModeClean()
#define QTMovieApertureModeAttribute getQTMovieApertureModeAttribute()
#endif

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
    , m_seekTimer(this, &MediaPlayerPrivate::seekTimerFired)
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_startedPlaying(false)
    , m_isStreaming(false)
    , m_visible(false)
    , m_rect()
    , m_scaleFactor(1, 1)
    , m_enabledTrackCount(0)
    , m_totalTrackCount(0)
    , m_hasUnsupportedTracks(false)
    , m_duration(-1.0f)
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
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
                                     [NSNumber numberWithBool:YES], @"QTMovieOpenForPlaybackAttribute",
#endif
#ifndef BUILDING_ON_TIGER
                                     QTMovieApertureModeClean, QTMovieApertureModeAttribute,
#endif
                                     nil];
    
    NSError *error = nil;
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

    // In updateState(), we track when maxTimeLoaded() == duration().
    // In newer version of QuickTime, a notification is emitted when maxTimeLoaded changes.
    // In older version of QuickTime, QTMovieLoadStateDidChangeNotification be fired.
    if (NSString *maxTimeLoadedChangeNotification = wkQTMovieMaxTimeLoadedChangeNotification()) {
        [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get()
                                                 selector:@selector(loadStateChanged:) 
                                                     name:maxTimeLoadedChangeNotification
                                                   object:m_qtMovie.get()];        
    }

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
    if (!m_player->inMediaDocument() && !addedCustomMethods) {
        Class QTMovieContentViewClass = NSClassFromString(@"QTMovieContentView");
        ASSERT(QTMovieContentViewClass);

        Method mainThreadSetNeedsDisplayMethod = class_getInstanceMethod(QTMovieContentViewClass, @selector(_mainThreadSetNeedsDisplay));
        ASSERT(mainThreadSetNeedsDisplayMethod);

        method_setImplementation(mainThreadSetNeedsDisplayMethod, reinterpret_cast<IMP>(mainThreadSetNeedsDisplay));
        addedCustomMethods = true;
    }

    // delay callbacks as we *will* get notifications during setup
    [m_objcObserver.get() setDelayCallbacks:YES];

    m_qtMovieView.adoptNS([[QTMovieView alloc] init]);
    setSize(m_player->size());
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

    [m_objcObserver.get() setDelayCallbacks:NO];
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
    if (m_readyState != MediaPlayer::HaveNothing) {
        m_readyState = MediaPlayer::HaveNothing;
        m_player->readyStateChanged();
    }
    cancelSeek();
    
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
    return static_cast<float>(time.timeValue) / time.timeScale;
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
    if (oldRate && timeAfterSeek < duration())
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

void MediaPlayerPrivate::setEndTime(float)
{
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

    // In spite of the name of this method, return QTMovieNaturalSizeAttribute transformed by the 
    // initial movie scale because the spec says intrinsic size is:
    //
    //    ... the dimensions of the resource in CSS pixels after taking into account the resource's 
    //    dimensions, aspect ratio, clean aperture, resolution, and so forth, as defined for the 
    //    format used by the resource
    
    NSSize naturalSize = [[m_qtMovie.get() attributeForKey:QTMovieNaturalSizeAttribute] sizeValue];
    return IntSize(naturalSize.width * m_scaleFactor.width(), naturalSize.height * m_scaleFactor.height());
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
    if (!metaDataAvailable())
        return 0;

    // infinite duration means live stream
    if (isinf(duration()))
        return 0;

    return wkQTMovieMaxTimeSeekable(m_qtMovie.get());
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

void MediaPlayerPrivate::cacheMovieScale()
{
    NSSize initialSize = NSZeroSize;
    NSSize naturalSize = [[m_qtMovie.get() attributeForKey:QTMovieNaturalSizeAttribute] sizeValue];

#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    // QTMovieCurrentSizeAttribute is not allowed with instances of QTMovie that have been 
    // opened with QTMovieOpenForPlaybackAttribute, so ask for the display transform attribute instead.
    NSAffineTransform *displayTransform = [m_qtMovie.get() attributeForKey:@"QTMoviePreferredTransformAttribute"];
    if (displayTransform)
        initialSize = [displayTransform transformSize:naturalSize];
    else {
        initialSize.width = naturalSize.width;
        initialSize.height = naturalSize.height;
    }
#else
    initialSize = [[m_qtMovie.get() attributeForKey:QTMovieCurrentSizeAttribute] sizeValue];
#endif

    if (naturalSize.width)
        m_scaleFactor.setWidth(initialSize.width / naturalSize.width);
    if (naturalSize.height)
        m_scaleFactor.setHeight(initialSize.height / naturalSize.height);
}

void MediaPlayerPrivate::updateStates()
{
    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;
    
    long loadState = m_qtMovie ? [[m_qtMovie.get() attributeForKey:QTMovieLoadStateAttribute] longValue] : static_cast<long>(QTMovieLoadStateError);

    if (loadState >= QTMovieLoadStateLoaded && m_readyState < MediaPlayer::HaveMetadata) {
        disableUnsupportedTracks();
        if (m_player->inMediaDocument()) {
            if (!m_enabledTrackCount || m_enabledTrackCount != m_totalTrackCount) {
                // This is a type of media that we do not handle directly with a <video> 
                // element, likely streamed media or QuickTime VR. Tell the MediaPlayerClient
                // that we noticed.
                sawUnsupportedTracks();
                return;
            }
        } else if (!m_enabledTrackCount) {
            loadState = QTMovieLoadStateError;
        }
        
        if (loadState != QTMovieLoadStateError)
            cacheMovieScale();
    }

    BOOL completelyLoaded = !m_isStreaming && (loadState >= QTMovieLoadStateComplete);

    // Note: QT indicates that we are fully loaded with QTMovieLoadStateComplete.
    // However newer versions of QT do not, so we check maxTimeLoaded against duration.
    if (!completelyLoaded && !m_isStreaming && metaDataAvailable())
        completelyLoaded = maxTimeLoaded() == duration();

    if (completelyLoaded) {
        // "Loaded" is reserved for fully buffered movies, never the case when streaming
        m_networkState = MediaPlayer::Loaded;
        m_readyState = MediaPlayer::HaveEnoughData;
    } else if (loadState >= QTMovieLoadStatePlaythroughOK) {
        m_readyState = MediaPlayer::HaveEnoughData;
        m_networkState = MediaPlayer::Loading;
    } else if (loadState >= QTMovieLoadStatePlayable) {
        // FIXME: This might not work correctly in streaming case, <rdar://problem/5693967>
        m_readyState = currentTime() < maxTimeLoaded() ? MediaPlayer::HaveFutureData : MediaPlayer::HaveCurrentData;
        m_networkState = MediaPlayer::Loading;
    } else if (loadState >= QTMovieLoadStateLoaded) {
        m_readyState = MediaPlayer::HaveMetadata;
        m_networkState = MediaPlayer::Loading;
    } else if (loadState > QTMovieLoadStateError) {
        m_readyState = MediaPlayer::HaveNothing;
        m_networkState = MediaPlayer::Loading;
    } else {
        if (m_player->inMediaDocument()) {
            // Something went wrong in the loading of media within a standalone file. 
            // This can occur with chained refmovies pointing to streamed media.
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

    if (loadState >= QTMovieLoadStateLoaded && (!m_qtMovieView && !m_qtVideoRenderer) && m_player->visible())
        setUpVideoRendering();

    if (loadState >= QTMovieLoadStateLoaded) {
        float dur = duration();
        if (dur != m_duration) {
            if (m_duration != -1.0f)
                m_player->durationChanged();
            m_duration = dur;
        }
    }
}

void MediaPlayerPrivate::loadStateChanged()
{
    if (!m_hasUnsupportedTracks)
        updateStates();
}

void MediaPlayerPrivate::rateChanged()
{
    if (m_hasUnsupportedTracks)
        return;

    updateStates();
    m_player->rateChanged();
}

void MediaPlayerPrivate::sizeChanged()
{
    if (!m_hasUnsupportedTracks)
        m_player->sizeChanged();
}

void MediaPlayerPrivate::timeChanged()
{
    if (m_hasUnsupportedTracks)
        return;

    updateStates();
    m_player->timeChanged();
}

void MediaPlayerPrivate::didEnd()
{
    if (m_hasUnsupportedTracks)
        return;

    m_startedPlaying = false;
#if DRAW_FRAME_RATE
    m_timeStoppedPlaying = [NSDate timeIntervalSinceReferenceDate];
#endif
    updateStates();
    m_player->timeChanged();
}

void MediaPlayerPrivate::setSize(const IntSize&) 
{ 
    // Don't resize the view now because [view setFrame] also resizes the movie itself, and because
    // the renderer calls this function immediately when we report a size change (QTMovieSizeDidChangeNotification)
    // we can get into a feedback loop observing the size change and resetting the size, and this can cause
    // QuickTime to miss resetting a movie's size when the media size changes (as happens with an rtsp movie
    // once the rtsp server sends the track sizes). Instead we remember the size passed to paint() and resize
    // the view when it changes.
    // <rdar://problem/6336092> REGRESSION: rtsp movie does not resize correctly
}

void MediaPlayerPrivate::setVisible(bool b)
{
    if (m_visible != b) {
        m_visible = b;
        if (b) {
            if (m_readyState >= MediaPlayer::HaveMetadata)
                setUpVideoRendering();
        } else
            tearDownVideoRendering();
    }
}

void MediaPlayerPrivate::repaint()
{
    if (m_hasUnsupportedTracks)
        return;

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
    if (context->paintingDisabled() || m_hasUnsupportedTracks)
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
    } else {
        if (m_rect != r) {
             m_rect = r;
            if (m_player->inMediaDocument()) {
                // the QTMovieView needs to be placed in the proper location for document mode
                [view setFrame:m_rect];
            }
            else {
                // We don't really need the QTMovieView in any specific location so let's just get it out of the way
                // where it won't intercept events or try to bring up the context menu.
                IntRect farAwayButCorrectSize(m_rect);
                farAwayButCorrectSize.move(-1000000, -1000000);
                [view setFrame:farAwayButCorrectSize];
            }
        }

        if (m_player->inMediaDocument()) {
            // If we're using a QTMovieView in a media document, the view may get layer-backed. AppKit won't update
            // the layer hosting correctly if we call displayRectIgnoringOpacity:inContext:, so use displayRectIgnoringOpacity:
            // in this case. See <rdar://problem/6702882>.
            [view displayRectIgnoringOpacity:paintRect];
        } else
            [view displayRectIgnoringOpacity:paintRect inContext:newContext];
    }

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

static void addFileTypesToCache(NSArray * fileTypes, HashSet<String> &cache)
{
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
}

static HashSet<String> mimeCommonTypesCache()
{
    DEFINE_STATIC_LOCAL(HashSet<String>, cache, ());
    static bool typeListInitialized = false;

    if (!typeListInitialized) {
        typeListInitialized = true;
        NSArray* fileTypes = [QTMovie movieFileTypes:QTIncludeCommonTypes];
        addFileTypesToCache(fileTypes, cache);
    }
    
    return cache;
} 

static HashSet<String> mimeModernTypesCache()
{
    DEFINE_STATIC_LOCAL(HashSet<String>, cache, ());
    static bool typeListInitialized = false;
    
    if (!typeListInitialized) {
        typeListInitialized = true;
        NSArray* fileTypes = [QTMovie movieFileTypes:(QTMovieFileTypeOptions)wkQTIncludeOnlyModernMediaFileTypes()];
        addFileTypesToCache(fileTypes, cache);
    }
    
    return cache;
} 

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>& types)
{
    // Note: this method starts QTKitServer if it isn't already running when in 64-bit because it has to return the list 
    // of every MIME type supported by QTKit.
    types = mimeCommonTypesCache();
} 

MediaPlayer::SupportsType MediaPlayerPrivate::supportsType(const String& type, const String& codecs)
{
    // Only return "IsSupported" if there is no codecs parameter for now as there is no way to ask QT if it supports an
    // extended MIME type yet.

    // We check the "modern" type cache first, as it doesn't require QTKitServer to start.
    if (mimeModernTypesCache().contains(type) || mimeCommonTypesCache().contains(type))
        return (codecs && !codecs.isEmpty() ? MediaPlayer::MayBeSupported : MediaPlayer::IsSupported);

    return MediaPlayer::IsNotSupported;
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
    
void MediaPlayerPrivate::disableUnsupportedTracks()
{
    if (!m_qtMovie) {
        m_enabledTrackCount = 0;
        m_totalTrackCount = 0;
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
        allowedTrackTypes->add("clcp"); // Closed caption
        allowedTrackTypes->add("sbtl"); // Subtitle
        allowedTrackTypes->add("odsm"); // MPEG-4 object descriptor stream
        allowedTrackTypes->add("sdsm"); // MPEG-4 scene description stream
        allowedTrackTypes->add("tmcd"); // timecode
        allowedTrackTypes->add("tc64"); // timcode-64
    }
    
    NSArray *tracks = [m_qtMovie.get() tracks];
    
    m_totalTrackCount = [tracks count];
    m_enabledTrackCount = m_totalTrackCount;
    for (unsigned trackIndex = 0; trackIndex < m_totalTrackCount; trackIndex++) {
        // Grab the track at the current index. If there isn't one there, then
        // we can move onto the next one.
        QTTrack *track = [tracks objectAtIndex:trackIndex];
        if (!track)
            continue;
        
        // Check to see if the track is disabled already, we should move along.
        // We don't need to re-disable it.
        if (![track isEnabled])
            continue;
        
        // Get the track's media type.
        NSString *mediaType = [track attributeForKey:QTTrackMediaTypeAttribute];
        if (!mediaType)
            continue;

        // Test whether the media type is in our white list.
        if (!allowedTrackTypes->contains(mediaType)) {
            // If this track type is not allowed, then we need to disable it.
            [track setEnabled:NO];
            --m_enabledTrackCount;
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
        --m_enabledTrackCount;
    }
}

void MediaPlayerPrivate::sawUnsupportedTracks()
{
    m_hasUnsupportedTracks = true;
    m_player->mediaPlayerClient()->mediaPlayerSawUnsupportedTracks(m_player);
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
