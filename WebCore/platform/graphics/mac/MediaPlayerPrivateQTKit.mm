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

#import "config.h"

#if ENABLE(VIDEO)

#import "MediaPlayerPrivateQTKit.h"

#import "BlockExceptions.h"
#import "DeprecatedString.h"
#import "GraphicsContext.h"
#import "KURL.h"
#import "ScrollView.h"
#import "SoftLinking.h"
#import "WebCoreSystemInterface.h"
#import <QTKit/QTKit.h>
#import <objc/objc-runtime.h>

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

SOFT_LINK_POINTER(QTKit, QTMovieDataSizeAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieDidEndNotification, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieHasVideoAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieLoadStateAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieLoadStateDidChangeNotification, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieNaturalSizeAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieRateDidChangeNotification, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieSizeDidChangeNotification, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieTimeDidChangeNotification, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieTimeScaleAttribute, NSString *)
SOFT_LINK_POINTER(QTKit, QTMovieVolumeDidChangeNotification, NSString *)

#define QTMovie getQTMovieClass()
#define QTMovieView getQTMovieViewClass()

#define QTMovieDataSizeAttribute getQTMovieDataSizeAttribute()
#define QTMovieDidEndNotification getQTMovieDidEndNotification()
#define QTMovieHasVideoAttribute getQTMovieHasVideoAttribute()
#define QTMovieLoadStateAttribute getQTMovieLoadStateAttribute()
#define QTMovieLoadStateDidChangeNotification getQTMovieLoadStateDidChangeNotification()
#define QTMovieNaturalSizeAttribute getQTMovieNaturalSizeAttribute()
#define QTMovieRateDidChangeNotification getQTMovieRateDidChangeNotification()
#define QTMovieSizeDidChangeNotification getQTMovieSizeDidChangeNotification()
#define QTMovieTimeDidChangeNotification getQTMovieTimeDidChangeNotification()
#define QTMovieTimeScaleAttribute getQTMovieTimeScaleAttribute()
#define QTMovieVolumeDidChangeNotification getQTMovieVolumeDidChangeNotification()

// Older versions of the QTKit header don't have these constants.
#if !defined QTKIT_VERSION_MAX_ALLOWED || QTKIT_VERSION_MAX_ALLOWED <= QTKIT_VERSION_7_0
enum {
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
    BOOL m_delayCallbacks;
}
-(id)initWithCallback:(MediaPlayerPrivate*)callback;
-(void)disconnect;
-(void)repaint;
-(void)setDelayCallbacks:(BOOL)shouldDelay;
-(void)loadStateChanged:(NSNotification *)notification;
-(void)rateChanged:(NSNotification *)notification;
-(void)sizeChanged:(NSNotification *)notification;
-(void)timeChanged:(NSNotification *)notification;
-(void)volumeChanged:(NSNotification *)notification;
-(void)didEnd:(NSNotification *)notification;
@end

namespace WebCore {

static const float cuePointTimerInterval = 0.020f;
    
MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_player(player)
    , m_objcObserver(AdoptNS, [[WebCoreMovieObserver alloc] initWithCallback:this])
    , m_seekTo(-1)
    , m_endTime(numeric_limits<float>::infinity())
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
    detachQTMovieView();

    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];
    [m_objcObserver.get() disconnect];
}

void MediaPlayerPrivate::createQTMovie(const String& url)
{
    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];
    
    NSError* error = nil;
    m_qtMovie.adoptNS([[QTMovie alloc] initWithURL:KURL(url.deprecatedString()).getNSURL() error:&error]);
    
    // FIXME: Find a proper way to detect streaming content.
    m_isStreaming = url.startsWith("rtsp:");
    
    if (!m_qtMovie)
        return;
    
    [m_qtMovie.get() setVolume:m_player->volume()];
    [m_qtMovie.get() setMuted:m_player->muted()];
    
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

static void mainThreadSetNeedsDisplay(id self, SEL _cmd)
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

void MediaPlayerPrivate::createQTMovieView()
{
    detachQTMovieView();

    if (!m_player->m_parentWidget || !m_qtMovie)
        return;

    static bool addedCustomMethods = false;
    if (!addedCustomMethods) {
        Class QTMovieContentViewClass = NSClassFromString(@"QTMovieContentView");
        ASSERT(QTMovieContentViewClass);

        Method mainThreadSetNeedsDisplayMethod = class_getInstanceMethod(QTMovieContentViewClass, @selector(_mainThreadSetNeedsDisplay));
        ASSERT(mainThreadSetNeedsDisplayMethod);

        method_setImplementation(mainThreadSetNeedsDisplayMethod, reinterpret_cast<IMP>(mainThreadSetNeedsDisplay));
        addedCustomMethods = true;
    }

    m_qtMovieView.adoptNS([[QTMovieView alloc] initWithFrame:m_player->rect()]);
    NSView* parentView = static_cast<ScrollView*>(m_player->m_parentWidget)->getDocumentView();
    [parentView addSubview:m_qtMovieView.get()];
    [m_qtMovieView.get() setDelegate:m_objcObserver.get()];
    [m_qtMovieView.get() setMovie:m_qtMovie.get()];
    [m_qtMovieView.get() setControllerVisible:NO];
    [m_qtMovieView.get() setPreservesAspectRatio:YES];
    // the area not covered by video should be transparent
    [m_qtMovieView.get() setFillColor:[NSColor clearColor]];
    wkQTMovieViewSetDrawSynchronously(m_qtMovieView.get(), YES);
}

void MediaPlayerPrivate::detachQTMovieView()
{
    if (m_qtMovieView) {
        [m_qtMovieView.get() setDelegate:nil];
        [m_qtMovieView.get() removeFromSuperview];
        m_qtMovieView = nil;
    }
}

QTTime MediaPlayerPrivate::createQTTime(float time) const
{
    if (!m_qtMovie)
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
    [m_qtMovie.get() setRate:m_player->rate()];
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
        return numeric_limits<float>::infinity();
    return static_cast<float>(time.timeValue) / time.timeScale;
}

float MediaPlayerPrivate::currentTime() const
{
    if (!m_qtMovie)
        return 0;
    QTTime time = [m_qtMovie.get() currentTime];
    return min(static_cast<float>(time.timeValue) / time.timeScale, m_endTime);
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

void MediaPlayerPrivate::addCuePoint(float /*time*/)
{
    // FIXME: Eventually we'd like an approach that doesn't involve a timer.
    startCuePointTimerIfNeeded();
}

void MediaPlayerPrivate::removeCuePoint(float /*time*/)
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
        m_cuePointTimer.startRepeating(cuePointTimerInterval);
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

    // Make a copy since m_cuePoints could change as we deliver JavaScript calls.
    Vector<float> cuePoints;
    copyToVector(m_player->m_cuePoints, cuePoints);
    size_t numCuePoints = cuePoints.size();
    for (size_t i = 0; i < numCuePoints; ++i) {
        float cueTime = cuePoints[i];
        if (previousTime < cueTime && cueTime <= time)
            m_player->cuePointReached(cueTime);
    }
}

bool MediaPlayerPrivate::paused() const
{
    if (!m_qtMovie)
        return true;
    return [m_qtMovie.get() rate] == 0;
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
    return IntSize([[m_qtMovie.get() attributeForKey:QTMovieNaturalSizeAttribute] sizeValue]);
}

bool MediaPlayerPrivate::hasVideo() const
{
    if (!m_qtMovie)
        return false;
    return [[m_qtMovie.get() attributeForKey:QTMovieHasVideoAttribute] boolValue];
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
    if (!m_qtMovie)
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
    if (!m_qtMovie)
        return 0;
    return [[m_qtMovie.get() attributeForKey:QTMovieDataSizeAttribute] intValue];
}

void MediaPlayerPrivate::cancelLoad()
{
    // FIXME: Is there a better way to check for this?
    if (m_networkState < MediaPlayer::Loading || m_networkState == MediaPlayer::Loaded)
        return;
    
    detachQTMovieView();
    m_qtMovie = nil;
    
    updateStates();
}

void MediaPlayerPrivate::updateStates()
{
    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;
    
    long loadState = m_qtMovie ? [[m_qtMovie.get() attributeForKey:QTMovieLoadStateAttribute] longValue] : -1;
    // "Loaded" is reserved for fully buffered movies, never the case when streaming
    if (loadState >= QTMovieLoadStateComplete && !m_isStreaming) {
        if (m_networkState < MediaPlayer::Loaded)
            m_networkState = MediaPlayer::Loaded;
        m_readyState = MediaPlayer::CanPlayThrough;
    } else if (loadState >= QTMovieLoadStatePlaythroughOK) {
        if (m_networkState < MediaPlayer::LoadedFirstFrame && !seeking())
            m_networkState = MediaPlayer::LoadedFirstFrame;
        m_readyState = ([m_qtMovie.get() rate] == 0 && m_startedPlaying) ? MediaPlayer::DataUnavailable : MediaPlayer::CanPlayThrough;
    } else if (loadState >= QTMovieLoadStatePlayable) {
        if (m_networkState < MediaPlayer::LoadedFirstFrame && !seeking())
            m_networkState = MediaPlayer::LoadedFirstFrame;
        m_readyState = ([m_qtMovie.get() rate] == 0 && m_startedPlaying) ? MediaPlayer::DataUnavailable : MediaPlayer::CanPlay;
    } else if (loadState >= QTMovieLoadStateLoaded) {
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
        [m_qtMovieView.get() setFrame:r];
}

void MediaPlayerPrivate::setVisible(bool b)
{
    if (b)
        createQTMovieView();
    else
        detachQTMovieView();
}

void MediaPlayerPrivate::repaint()
{
    m_player->repaint();
}

void MediaPlayerPrivate::paint(GraphicsContext* context, const IntRect& r)
{
    if (context->paintingDisabled())
        return;
    NSView *view = m_qtMovieView.get();
    if (view == nil)
        return;
    [m_objcObserver.get() setDelayCallbacks:YES];
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    context->save();
    context->translate(r.x(), r.y() + r.height());
    context->scale(FloatSize(1.0f, -1.0f));
    IntRect paintRect(IntPoint(0, 0), IntSize(r.width(), r.height()));
    NSGraphicsContext* newContext = [NSGraphicsContext graphicsContextWithGraphicsPort:context->platformContext() flipped:NO];
    [view displayRectIgnoringOpacity:paintRect inContext:newContext];
    context->restore();
    END_BLOCK_OBJC_EXCEPTIONS;
    [m_objcObserver.get() setDelayCallbacks:NO];
}

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>& types)
{
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
        types.add(mime.get());
    }
} 

}

@implementation WebCoreMovieObserver

- (id)initWithCallback:(MediaPlayerPrivate *)callback
{
    m_callback = callback;
    return [super init];
}

- (void)disconnect
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    m_callback = 0;
}

-(void)repaint
{
    if (m_delayCallbacks)
        [self performSelector:_cmd withObject:nil afterDelay:0.];
    else if (m_callback)
        m_callback->repaint();
}

- (void)loadStateChanged:(NSNotification *)notification
{
    if (m_delayCallbacks)
        [self performSelector:_cmd withObject:nil afterDelay:0];
    else
        m_callback->loadStateChanged();
}

- (void)rateChanged:(NSNotification *)notification
{
    if (m_delayCallbacks)
        [self performSelector:_cmd withObject:nil afterDelay:0];
    else
        m_callback->rateChanged();
}

- (void)sizeChanged:(NSNotification *)notification
{
    if (m_delayCallbacks)
        [self performSelector:_cmd withObject:nil afterDelay:0];
    else
        m_callback->sizeChanged();
}

- (void)timeChanged:(NSNotification *)notification
{
    if (m_delayCallbacks)
        [self performSelector:_cmd withObject:nil afterDelay:0];
    else
        m_callback->timeChanged();
}

- (void)volumeChanged:(NSNotification *)notification
{
    if (m_delayCallbacks)
        [self performSelector:_cmd withObject:nil afterDelay:0];
    else
        m_callback->volumeChanged();
}

- (void)didEnd:(NSNotification *)notification
{
    if (m_delayCallbacks)
        [self performSelector:_cmd withObject:nil afterDelay:0];
    else
        m_callback->didEnd();
}

- (void)setDelayCallbacks:(BOOL)shouldDelay
{
    m_delayCallbacks = shouldDelay;
}

@end

#endif
