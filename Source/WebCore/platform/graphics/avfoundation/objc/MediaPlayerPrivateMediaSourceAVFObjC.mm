/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "MediaPlayerPrivateMediaSourceAVFObjC.h"

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#import "HTMLMediaSource.h"
#import "MediaSourcePrivateAVFObjC.h"
#import "MediaTimeMac.h"
#import "PlatformClockCM.h"
#import "SoftLinking.h"
#import <AVFoundation/AVAsset.h>
#import <CoreMedia/CMSync.h>
#import <QuartzCore/CALayer.h>
#import <objc_runtime.h>
#import <wtf/Functional.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>

#pragma mark -
#pragma mark Soft Linking

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_FRAMEWORK_OPTIONAL(CoreMedia)

SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVAsset)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVURLAsset)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVSampleBufferAudioRenderer)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVSampleBufferDisplayLayer)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVSampleBufferRenderSynchronizer)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVStreamDataParser)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVVideoPerformanceMetrics)

typedef struct opaqueCMNotificationCenter *CMNotificationCenterRef;
typedef void (*CMNotificationCallback)(CMNotificationCenterRef inCenter, const void *inListener, CFStringRef inNotificationName, const void *inNotifyingObject, CFTypeRef inNotificationPayload);

SOFT_LINK(CoreMedia, CMNotificationCenterGetDefaultLocalCenter, CMNotificationCenterRef, (void), ());
SOFT_LINK(CoreMedia, CMNotificationCenterAddListener, OSStatus, (CMNotificationCenterRef center, const void* listener, CMNotificationCallback callback, CFStringRef notification, const void* object, UInt32 flags), (center, listener, callback, notification, object, flags))
SOFT_LINK(CoreMedia, CMNotificationCenterRemoveListener, OSStatus, (CMNotificationCenterRef center, const void* listener, CMNotificationCallback callback, CFStringRef notification, const void* object), (center, listener, callback, notification, object))
SOFT_LINK(CoreMedia, CMTimeGetSeconds, Float64, (CMTime time), (time))
SOFT_LINK(CoreMedia, CMTimebaseGetTime, CMTime, (CMTimebaseRef timebase), (timebase))

SOFT_LINK_CONSTANT(CoreMedia, kCMTimebaseNotification_EffectiveRateChanged, CFStringRef)
#define kCMTimebaseNotification_EffectiveRateChanged getkCMTimebaseNotification_EffectiveRateChanged()

#pragma mark -
#pragma mark AVSampleBufferDisplayLayer

@interface AVSampleBufferDisplayLayer : CALayer
@end

#pragma mark -
#pragma mark AVVideoPerformanceMetrics

@interface AVVideoPerformanceMetrics : NSObject
- (unsigned long)totalNumberOfVideoFrames;
- (unsigned long)numberOfDroppedVideoFrames;
- (unsigned long)numberOfCorruptedVideoFrames;
- (double)totalFrameDelay;
@end

@interface AVSampleBufferDisplayLayer (WebCoreAVSampleBufferDisplayLayerPrivate)
- (AVVideoPerformanceMetrics *)videoPerformanceMetrics;
@end

#pragma mark -
#pragma mark AVSampleBufferAudioRenderer

@interface AVSampleBufferAudioRenderer : NSObject
- (void)setVolume:(float)volume;
- (void)setMuted:(BOOL)muted;
@end

#pragma mark -
#pragma mark AVSampleBufferRenderSynchronizer

@interface AVSampleBufferRenderSynchronizer : NSObject
- (CMTimebaseRef)timebase;
- (float)rate;
- (void)setRate:(float)rate;
- (void)setRate:(float)rate time:(CMTime)time;
- (NSArray *)renderers;
- (void)addRenderer:(id)renderer;
- (void)removeRenderer:(id)renderer atTime:(CMTime)time withCompletionHandler:(void (^)(BOOL didRemoveRenderer))completionHandler;
- (id)addPeriodicTimeObserverForInterval:(CMTime)interval queue:(dispatch_queue_t)queue usingBlock:(void (^)(CMTime time))block;
- (void)removeTimeObserver:(id)observer;
@end

namespace WebCore {

#pragma mark -
#pragma mark MediaPlayerPrivateMediaSourceAVFObjC

static void CMTimebaseEffectiveRateChangedCallback(CMNotificationCenterRef, const void *listener, CFStringRef, const void *, CFTypeRef)
{
    MediaPlayerPrivateMediaSourceAVFObjC* player = (MediaPlayerPrivateMediaSourceAVFObjC*)listener;
    callOnMainThread(bind(&MediaPlayerPrivateMediaSourceAVFObjC::effectiveRateChanged, player));
}

MediaPlayerPrivateMediaSourceAVFObjC::MediaPlayerPrivateMediaSourceAVFObjC(MediaPlayer* player)
    : m_player(player)
    , m_weakPtrFactory(this)
    , m_synchronizer(adoptNS([[getAVSampleBufferRenderSynchronizerClass() alloc] init]))
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_rate(1)
    , m_playing(0)
    , m_seeking(false)
    , m_loadingProgressed(false)
{
    CMTimebaseRef timebase = [m_synchronizer timebase];
    CMNotificationCenterRef nc = CMNotificationCenterGetDefaultLocalCenter();
    CMNotificationCenterAddListener(nc, this, CMTimebaseEffectiveRateChangedCallback, kCMTimebaseNotification_EffectiveRateChanged, timebase, 0);

    // addPeriodicTimeObserverForInterval: throws an exception if you pass a non-numeric CMTime, so just use
    // an arbitrarily large time value of once an hour:
    __block auto weakThis = createWeakPtr();
    m_timeJumpedObserver = [m_synchronizer addPeriodicTimeObserverForInterval:toCMTime(MediaTime::createWithDouble(3600)) queue:dispatch_get_main_queue() usingBlock:^(CMTime){
        // FIXME: Remove the below once <rdar://problem/15798050> is fixed.
        if (!weakThis)
            return;

        if (m_seeking)
            m_seeking = false;
        m_player->timeChanged();
    }];
}

MediaPlayerPrivateMediaSourceAVFObjC::~MediaPlayerPrivateMediaSourceAVFObjC()
{
    CMTimebaseRef timebase = [m_synchronizer timebase];
    CMNotificationCenterRef nc = CMNotificationCenterGetDefaultLocalCenter();
    CMNotificationCenterRemoveListener(nc, this, CMTimebaseEffectiveRateChangedCallback, kCMTimebaseNotification_EffectiveRateChanged, timebase);

    [m_synchronizer removeTimeObserver:m_timeJumpedObserver.get()];
}

#pragma mark -
#pragma mark MediaPlayer Factory Methods

void MediaPlayerPrivateMediaSourceAVFObjC::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (isAvailable())
        registrar(create, getSupportedTypes, supportsType, 0, 0, 0);
}

PassOwnPtr<MediaPlayerPrivateInterface> MediaPlayerPrivateMediaSourceAVFObjC::create(MediaPlayer* player)
{
    return adoptPtr(new MediaPlayerPrivateMediaSourceAVFObjC(player));
}

bool MediaPlayerPrivateMediaSourceAVFObjC::isAvailable()
{
    return AVFoundationLibrary()
        && CoreMediaLibrary()
        && getAVStreamDataParserClass()
        && getAVSampleBufferAudioRendererClass()
        && getAVSampleBufferRenderSynchronizerClass()
        && class_getInstanceMethod(getAVSampleBufferAudioRendererClass(), @selector(setMuted:));
}

static HashSet<String> mimeTypeCache()
{
    DEFINE_STATIC_LOCAL(HashSet<String>, cache, ());
    static bool typeListInitialized = false;

    if (typeListInitialized)
        return cache;
    typeListInitialized = true;

    NSArray *types = [getAVURLAssetClass() audiovisualMIMETypes];
    for (NSString *mimeType in types)
        cache.add(mimeType);
    
    return cache;
} 

void MediaPlayerPrivateMediaSourceAVFObjC::getSupportedTypes(HashSet<String>& types)
{
    types = mimeTypeCache();
}

MediaPlayer::SupportsType MediaPlayerPrivateMediaSourceAVFObjC::supportsType(const MediaEngineSupportParameters& parameters)
{
    // This engine does not support non-media-source sources.
    if (!parameters.isMediaSource)
        return MediaPlayer::IsNotSupported;

    if (!mimeTypeCache().contains(parameters.type))
        return MediaPlayer::IsNotSupported;

    // The spec says:
    // "Implementors are encouraged to return "maybe" unless the type can be confidently established as being supported or not."
    if (parameters.codecs.isEmpty())
        return MediaPlayer::MayBeSupported;

    NSString *typeString = [NSString stringWithFormat:@"%@; codecs=\"%@\"", (NSString *)parameters.type, (NSString *)parameters.codecs];
    return [getAVURLAssetClass() isPlayableExtendedMIMEType:typeString] ? MediaPlayer::IsSupported : MediaPlayer::MayBeSupported;;
}

#pragma mark -
#pragma mark MediaPlayerPrivateInterface Overrides

void MediaPlayerPrivateMediaSourceAVFObjC::load(const String&)
{
    // This media engine only supports MediaSource URLs.
    m_networkState = MediaPlayer::FormatError;
    m_player->networkStateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::load(const String& url, PassRefPtr<HTMLMediaSource> source)
{
    UNUSED_PARAM(url);

    m_mediaSource = source;
    m_mediaSourcePrivate = MediaSourcePrivateAVFObjC::create(this);

    m_mediaSource->setPrivateAndOpen(*m_mediaSourcePrivate);
}

void MediaPlayerPrivateMediaSourceAVFObjC::cancelLoad()
{
}

void MediaPlayerPrivateMediaSourceAVFObjC::prepareToPlay()
{
}

PlatformMedia MediaPlayerPrivateMediaSourceAVFObjC::platformMedia() const
{
    PlatformMedia pm;
    pm.type = PlatformMedia::AVFoundationAssetType;
    pm.media.avfAsset = m_asset.get();
    return pm;
}

PlatformLayer* MediaPlayerPrivateMediaSourceAVFObjC::platformLayer() const
{
    return m_sampleBufferDisplayLayer.get();
}

void MediaPlayerPrivateMediaSourceAVFObjC::play()
{
    callOnMainThread(WTF::bind(&MediaPlayerPrivateMediaSourceAVFObjC::playInternal, this));
}

void MediaPlayerPrivateMediaSourceAVFObjC::playInternal()
{
    m_playing = true;
    [m_synchronizer setRate:m_rate];
}

void MediaPlayerPrivateMediaSourceAVFObjC::pause()
{
    callOnMainThread(WTF::bind(&MediaPlayerPrivateMediaSourceAVFObjC::pauseInternal, this));
}

void MediaPlayerPrivateMediaSourceAVFObjC::pauseInternal()
{
    m_playing = false;
    [m_synchronizer setRate:0];
}

bool MediaPlayerPrivateMediaSourceAVFObjC::paused() const
{
    return !m_playing;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVolume(float volume)
{
    for (auto it = m_sampleBufferAudioRenderers.begin(), end = m_sampleBufferAudioRenderers.end(); it != end; ++it)
        [*it setVolume:volume];
}

bool MediaPlayerPrivateMediaSourceAVFObjC::supportsScanning() const
{
    return true;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setMuted(bool muted)
{
    for (auto it = m_sampleBufferAudioRenderers.begin(), end = m_sampleBufferAudioRenderers.end(); it != end; ++it)
        [*it setMuted:muted];
}

IntSize MediaPlayerPrivateMediaSourceAVFObjC::naturalSize() const
{
    return m_mediaSourcePrivate->naturalSize();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::hasVideo() const
{
    if (!m_mediaSourcePrivate)
        return false;

    return m_mediaSourcePrivate->hasVideo();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::hasAudio() const
{
    if (!m_mediaSourcePrivate)
        return false;

    return m_mediaSourcePrivate->hasAudio();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVisible(bool)
{
    // No-op.
}

double MediaPlayerPrivateMediaSourceAVFObjC::durationDouble() const
{
    return m_mediaSource->duration();
}

double MediaPlayerPrivateMediaSourceAVFObjC::currentTimeDouble() const
{
    return CMTimeGetSeconds(CMTimebaseGetTime([m_synchronizer timebase]));
}

double MediaPlayerPrivateMediaSourceAVFObjC::startTimeDouble() const
{
    return 0;
}

double MediaPlayerPrivateMediaSourceAVFObjC::initialTime() const
{
    return 0;
}

void MediaPlayerPrivateMediaSourceAVFObjC::seekWithTolerance(double time, double negativeThreshold, double positiveThreshold)
{
    m_seeking = true;
    callOnMainThread(WTF::bind(&MediaPlayerPrivateMediaSourceAVFObjC::seekInternal, this, time, negativeThreshold, positiveThreshold));
}

void MediaPlayerPrivateMediaSourceAVFObjC::seekInternal(double time, double negativeThreshold, double positiveThreshold)
{
    MediaTime seekTime = m_mediaSourcePrivate ? m_mediaSourcePrivate->seekToTime(MediaTime::createWithDouble(time), MediaTime::createWithDouble(positiveThreshold), MediaTime::createWithDouble(negativeThreshold)) : MediaTime::zeroTime();

    [m_synchronizer setRate:(m_playing ? m_rate : 0) time:toCMTime(seekTime)];
}

bool MediaPlayerPrivateMediaSourceAVFObjC::seeking() const
{
    return m_seeking;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setRateDouble(double rate)
{
    m_rate = rate;
    if (m_playing)
        [m_synchronizer setRate:m_rate];
}

MediaPlayer::NetworkState MediaPlayerPrivateMediaSourceAVFObjC::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivateMediaSourceAVFObjC::readyState() const
{
    return m_readyState;
}

PassRefPtr<TimeRanges> MediaPlayerPrivateMediaSourceAVFObjC::seekable() const
{
    return TimeRanges::create(minTimeSeekable(), maxTimeSeekableDouble());
}

double MediaPlayerPrivateMediaSourceAVFObjC::maxTimeSeekableDouble() const
{
    return durationDouble();
}

double MediaPlayerPrivateMediaSourceAVFObjC::minTimeSeekable() const
{
    return startTimeDouble();
}

PassRefPtr<TimeRanges> MediaPlayerPrivateMediaSourceAVFObjC::buffered() const
{
    return m_mediaSource->buffered();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::didLoadingProgress() const
{
    bool loadingProgressed = m_loadingProgressed;
    m_loadingProgressed = false;
    return loadingProgressed;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setSize(const IntSize&)
{
    // No-op.
}

void MediaPlayerPrivateMediaSourceAVFObjC::paint(GraphicsContext*, const IntRect&)
{
    // FIXME(125157): Implement painting.
}

void MediaPlayerPrivateMediaSourceAVFObjC::paintCurrentFrameInContext(GraphicsContext*, const IntRect&)
{
    // FIXME(125157): Implement painting.
}

bool MediaPlayerPrivateMediaSourceAVFObjC::hasAvailableVideoFrame() const
{
    return m_hasAvailableVideoFrame;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::supportsAcceleratedRendering() const
{
    return true;
}

void MediaPlayerPrivateMediaSourceAVFObjC::acceleratedRenderingStateChanged()
{
    if (m_player->mediaPlayerClient()->mediaPlayerRenderingCanBeAccelerated(m_player))
        ensureLayer();
    else
        destroyLayer();
}

MediaPlayer::MovieLoadType MediaPlayerPrivateMediaSourceAVFObjC::movieLoadType() const
{
    return MediaPlayer::StoredStream;
}

void MediaPlayerPrivateMediaSourceAVFObjC::prepareForRendering()
{
    // No-op.
}

String MediaPlayerPrivateMediaSourceAVFObjC::engineDescription() const
{
    static NeverDestroyed<String> description(ASCIILiteral("AVFoundation MediaSource Engine"));
    return description;
}

String MediaPlayerPrivateMediaSourceAVFObjC::languageOfPrimaryAudioTrack() const
{
    // FIXME(125158): implement languageOfPrimaryAudioTrack()
    return emptyString();
}

size_t MediaPlayerPrivateMediaSourceAVFObjC::extraMemoryCost() const
{
    return 0;
}

unsigned long MediaPlayerPrivateMediaSourceAVFObjC::totalVideoFrames()
{
    return [[m_sampleBufferDisplayLayer videoPerformanceMetrics] totalNumberOfVideoFrames];
}

unsigned long MediaPlayerPrivateMediaSourceAVFObjC::droppedVideoFrames()
{
    return [[m_sampleBufferDisplayLayer videoPerformanceMetrics] numberOfDroppedVideoFrames];
}

unsigned long MediaPlayerPrivateMediaSourceAVFObjC::corruptedVideoFrames()
{
    return [[m_sampleBufferDisplayLayer videoPerformanceMetrics] numberOfCorruptedVideoFrames];
}

double MediaPlayerPrivateMediaSourceAVFObjC::totalFrameDelay()
{
    return [[m_sampleBufferDisplayLayer videoPerformanceMetrics] totalFrameDelay];
}

#pragma mark -
#pragma mark Utility Methods

void MediaPlayerPrivateMediaSourceAVFObjC::ensureLayer()
{
    if (m_sampleBufferDisplayLayer)
        return;

    m_sampleBufferDisplayLayer = adoptNS([[getAVSampleBufferDisplayLayerClass() alloc] init]);
    [m_synchronizer addRenderer:m_sampleBufferDisplayLayer.get()];
}

void MediaPlayerPrivateMediaSourceAVFObjC::destroyLayer()
{
    if (!m_sampleBufferDisplayLayer)
        return;

    CMTime currentTime = CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:m_sampleBufferDisplayLayer.get() atTime:currentTime withCompletionHandler:^(BOOL){
        // No-op.
    }];
    m_sampleBufferDisplayLayer = nullptr;
}

void MediaPlayerPrivateMediaSourceAVFObjC::durationChanged()
{
    m_player->durationChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::effectiveRateChanged()
{
    m_player->rateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::sizeChanged()
{
    m_player->sizeChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setReadyState(MediaPlayer::ReadyState readyState)
{
    if (m_readyState == readyState)
        return;

    m_readyState = readyState;
    m_player->readyStateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setNetworkState(MediaPlayer::NetworkState networkState)
{
    if (m_networkState == networkState)
        return;

    m_networkState = networkState;
    m_player->networkStateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::addDisplayLayer(AVSampleBufferDisplayLayer* displayLayer)
{
    ASSERT(displayLayer);
    if (displayLayer == m_sampleBufferDisplayLayer)
        return;

    m_sampleBufferDisplayLayer = displayLayer;
    [m_synchronizer addRenderer:m_sampleBufferDisplayLayer.get()];
    m_player->mediaPlayerClient()->mediaPlayerRenderingModeChanged(m_player);

    // FIXME: move this somewhere appropriate:
    m_player->firstVideoFrameAvailable();
}

void MediaPlayerPrivateMediaSourceAVFObjC::removeDisplayLayer(AVSampleBufferDisplayLayer* displayLayer)
{
    if (displayLayer != m_sampleBufferDisplayLayer)
        return;

    CMTime currentTime = CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:m_sampleBufferDisplayLayer.get() atTime:currentTime withCompletionHandler:^(BOOL){
        // No-op.
    }];

    m_sampleBufferDisplayLayer = nullptr;
    m_player->mediaPlayerClient()->mediaPlayerRenderingModeChanged(m_player);
}

void MediaPlayerPrivateMediaSourceAVFObjC::addAudioRenderer(AVSampleBufferAudioRenderer* audioRenderer)
{
    if (m_sampleBufferAudioRenderers.contains(audioRenderer))
        return;

    m_sampleBufferAudioRenderers.append(audioRenderer);
    [m_synchronizer addRenderer:audioRenderer];
    m_player->mediaPlayerClient()->mediaPlayerRenderingModeChanged(m_player);
}

void MediaPlayerPrivateMediaSourceAVFObjC::removeAudioRenderer(AVSampleBufferAudioRenderer* audioRenderer)
{
    size_t pos = m_sampleBufferAudioRenderers.find(audioRenderer);
    if (pos == notFound)
        return;

    CMTime currentTime = CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:audioRenderer atTime:currentTime withCompletionHandler:^(BOOL){
        // No-op.
    }];

    m_sampleBufferAudioRenderers.remove(pos);
    m_player->mediaPlayerClient()->mediaPlayerRenderingModeChanged(m_player);
}

}

#endif
