/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "MediaPlayerPrivateMediaStreamAVFObjC.h"

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import "AVFoundationSPI.h"
#import "AudioTrackPrivateMediaStreamCocoa.h"
#import "Clock.h"
#import "GraphicsContextCG.h"
#import "Logging.h"
#import "MediaStreamPrivate.h"
#import "MediaTimeAVFoundation.h"
#import "PixelBufferConformerCV.h"
#import "VideoTrackPrivateMediaStream.h"
#import <AVFoundation/AVSampleBufferDisplayLayer.h>
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CATransaction.h>
#import <objc_runtime.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
#import "VideoFullscreenLayerManager.h"
#endif

#pragma mark - Soft Linking

#import "CoreMediaSoftLink.h"
#import "CoreVideoSoftLink.h"

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVSampleBufferDisplayLayer)

SOFT_LINK_POINTER(AVFoundation, AVLayerVideoGravityResizeAspect, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVLayerVideoGravityResizeAspectFill, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVLayerVideoGravityResize, NSString *)

#define AVLayerVideoGravityResizeAspect getAVLayerVideoGravityResizeAspect()
#define AVLayerVideoGravityResizeAspectFill getAVLayerVideoGravityResizeAspectFill()
#define AVLayerVideoGravityResize getAVLayerVideoGravityResize()

using namespace WebCore;

@interface WebAVSampleBufferStatusChangeListener : NSObject {
    MediaPlayerPrivateMediaStreamAVFObjC* _parent;
}

- (id)initWithParent:(MediaPlayerPrivateMediaStreamAVFObjC*)callback;
- (void)invalidate;
- (void)beginObservingLayers;
- (void)stopObservingLayers;
@end

@implementation WebAVSampleBufferStatusChangeListener

- (id)initWithParent:(MediaPlayerPrivateMediaStreamAVFObjC*)parent
{
    if (!(self = [super init]))
        return nil;

    _parent = parent;

    return self;
}

- (void)dealloc
{
    [self invalidate];
    [super dealloc];
}

- (void)invalidate
{
    [self stopObservingLayers];
    _parent = nullptr;
}

- (void)beginObservingLayers
{
    ASSERT(_parent);
    ASSERT(_parent->displayLayer());
    ASSERT(_parent->backgroundLayer());

    [_parent->displayLayer() addObserver:self forKeyPath:@"status" options:NSKeyValueObservingOptionNew context:nil];
    [_parent->displayLayer() addObserver:self forKeyPath:@"error" options:NSKeyValueObservingOptionNew context:nil];
    [_parent->backgroundLayer() addObserver:self forKeyPath:@"bounds" options:NSKeyValueObservingOptionNew context:nil];
}

- (void)stopObservingLayers
{
    if (!_parent)
        return;

    if (_parent->displayLayer()) {
        [_parent->displayLayer() removeObserver:self forKeyPath:@"status"];
        [_parent->displayLayer() removeObserver:self forKeyPath:@"error"];
    }
    if (_parent->backgroundLayer())
        [_parent->backgroundLayer() removeObserver:self forKeyPath:@"bounds"];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(keyPath);
    ASSERT(_parent);

    if (!_parent)
        return;

    if ([object isKindOfClass:getAVSampleBufferDisplayLayerClass()]) {
        RetainPtr<AVSampleBufferDisplayLayer> layer = (AVSampleBufferDisplayLayer *)object;
        ASSERT(layer.get() == _parent->displayLayer());

        if ([keyPath isEqualToString:@"status"]) {
            RetainPtr<NSNumber> status = [change valueForKey:NSKeyValueChangeNewKey];
            callOnMainThread([protectedSelf = RetainPtr<WebAVSampleBufferStatusChangeListener>(self), layer = WTFMove(layer), status = WTFMove(status)] {
                if (!protectedSelf->_parent)
                    return;

                protectedSelf->_parent->layerStatusDidChange(layer.get());
            });
            return;
        }

        if ([keyPath isEqualToString:@"error"]) {
            RetainPtr<NSNumber> status = [change valueForKey:NSKeyValueChangeNewKey];
            callOnMainThread([protectedSelf = RetainPtr<WebAVSampleBufferStatusChangeListener>(self), layer = WTFMove(layer), status = WTFMove(status)] {
                if (!protectedSelf->_parent)
                    return;

                protectedSelf->_parent->layerErrorDidChange(layer.get());
            });
            return;
        }
    }

    if ([[change valueForKey:NSKeyValueChangeNotificationIsPriorKey] boolValue])
        return;

    if ((CALayer *)object == _parent->backgroundLayer()) {
        if ([keyPath isEqualToString:@"bounds"]) {
            callOnMainThread([protectedSelf = RetainPtr<WebAVSampleBufferStatusChangeListener>(self)] {
                if (!protectedSelf->_parent)
                    return;

                protectedSelf->_parent->backgroundLayerBoundsChanged();
            });
        }
    }

}
@end

namespace WebCore {

#pragma mark -
#pragma mark MediaPlayerPrivateMediaStreamAVFObjC

static const double rendererLatency = 0.02;

MediaPlayerPrivateMediaStreamAVFObjC::MediaPlayerPrivateMediaStreamAVFObjC(MediaPlayer* player)
    : m_player(player)
    , m_weakPtrFactory(this)
    , m_statusChangeListener(adoptNS([[WebAVSampleBufferStatusChangeListener alloc] initWithParent:this]))
    , m_clock(Clock::create())
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    , m_videoFullscreenLayerManager(VideoFullscreenLayerManager::create())
#endif
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::MediaPlayerPrivateMediaStreamAVFObjC(%p)", this);
}

MediaPlayerPrivateMediaStreamAVFObjC::~MediaPlayerPrivateMediaStreamAVFObjC()
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::~MediaPlayerPrivateMediaStreamAVFObjC(%p)", this);

    [m_statusChangeListener invalidate];

    for (const auto& track : m_audioTrackMap.values())
        track->pause();

    if (m_mediaStreamPrivate) {
        m_mediaStreamPrivate->removeObserver(*this);

        for (auto& track : m_mediaStreamPrivate->tracks())
            track->removeObserver(*this);
    }

    destroyLayers();

    m_audioTrackMap.clear();
    m_videoTrackMap.clear();
}

#pragma mark -
#pragma mark MediaPlayer Factory Methods

void MediaPlayerPrivateMediaStreamAVFObjC::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (isAvailable())
        registrar([](MediaPlayer* player) { return std::make_unique<MediaPlayerPrivateMediaStreamAVFObjC>(player); }, getSupportedTypes,
            supportsType, 0, 0, 0, 0);
}

bool MediaPlayerPrivateMediaStreamAVFObjC::isAvailable()
{
    if (!AVFoundationLibrary() || !isCoreMediaFrameworkAvailable() || !getAVSampleBufferDisplayLayerClass())
        return false;

    return true;
}

void MediaPlayerPrivateMediaStreamAVFObjC::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types)
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> cache;
    types = cache;
}

MediaPlayer::SupportsType MediaPlayerPrivateMediaStreamAVFObjC::supportsType(const MediaEngineSupportParameters& parameters)
{
    if (parameters.isMediaStream)
        return MediaPlayer::IsSupported;

    return MediaPlayer::IsNotSupported;
}

#pragma mark -
#pragma mark AVSampleBuffer Methods

void MediaPlayerPrivateMediaStreamAVFObjC::removeOldSamplesFromPendingQueue(PendingSampleQueue& queue)
{
    MediaTime now = streamTime();
    while (!queue.isEmpty()) {
        if (queue.first()->decodeTime() > now)
            break;
        queue.removeFirst();
    };
}

void MediaPlayerPrivateMediaStreamAVFObjC::addSampleToPendingQueue(PendingSampleQueue& queue, MediaSample& sample)
{
    removeOldSamplesFromPendingQueue(queue);
    queue.append(sample);
}

void MediaPlayerPrivateMediaStreamAVFObjC::updateSampleTimes(MediaSample& sample, const MediaTime& timelineOffset, const char* loggingPrefix)
{
    LOG(MediaCaptureSamples, "%s(%p): original sample = %s", loggingPrefix, this, toString(sample).utf8().data());
    sample.offsetTimestampsBy(timelineOffset);
    LOG(MediaCaptureSamples, "%s(%p): adjusted sample = %s", loggingPrefix, this, toString(sample).utf8().data());

#if !LOG_DISABLED
    MediaTime now = streamTime();
    double delta = (sample.presentationTime() - now).toDouble();
    if (delta < 0)
        LOG(Media, "%s(%p): *NOTE* audio sample at time %s is %f seconds late", loggingPrefix, this, toString(now).utf8().data(), -delta);
    else if (delta < .01)
        LOG(Media, "%s(%p): *NOTE* audio sample at time %s is only %f seconds early", loggingPrefix, this, toString(now).utf8().data(), delta);
    else if (delta > .3)
        LOG(Media, "%s(%p): *NOTE* audio sample at time %s is %f seconds early!", loggingPrefix, this, toString(now).utf8().data(), delta);
#else
    UNUSED_PARAM(loggingPrefix);
#endif

}

MediaTime MediaPlayerPrivateMediaStreamAVFObjC::calculateTimelineOffset(const MediaSample& sample, double latency)
{
    MediaTime sampleTime = sample.outputPresentationTime();
    if (!sampleTime || !sampleTime.isValid())
        sampleTime = sample.presentationTime();
    MediaTime timelineOffset = streamTime() - sampleTime + MediaTime::createWithDouble(latency);
    if (timelineOffset.timeScale() != sampleTime.timeScale())
        timelineOffset = toMediaTime(CMTimeConvertScale(toCMTime(timelineOffset), sampleTime.timeScale(), kCMTimeRoundingMethod_Default));
    return timelineOffset;
}

CGAffineTransform MediaPlayerPrivateMediaStreamAVFObjC::videoTransformationMatrix(MediaSample& sample)
{
    if (m_transformIsValid)
        return m_videoTransform;

    CMSampleBufferRef sampleBuffer = sample.platformSample().sample.cmSampleBuffer;
    CVPixelBufferRef pixelBuffer = static_cast<CVPixelBufferRef>(CMSampleBufferGetImageBuffer(sampleBuffer));
    size_t width = CVPixelBufferGetWidth(pixelBuffer);
    size_t height = CVPixelBufferGetHeight(pixelBuffer);
    if (!width || !height)
        return CGAffineTransformIdentity;

    ASSERT(m_videoOrientation >= MediaSample::VideoOrientation::Unknown);
    ASSERT(m_videoOrientation <= MediaSample::VideoOrientation::LandscapeLeft);

    // Unknown, Portrait, PortraitUpsideDown, LandscapeRight, LandscapeLeft,
#if PLATFORM(MAC)
    static float sensorAngle[] = { 0, 0, 180, 90, 270 };
#else
    static float sensorAngle[] = { 180, 180, 0, 90, 270 };
#endif
    float rotation = sensorAngle[static_cast<int>(m_videoOrientation)];
    m_videoTransform = CGAffineTransformMakeRotation(rotation * M_PI / 180);

    if (sample.videoMirrored())
        m_videoTransform = CGAffineTransformScale(m_videoTransform, -1, 1);

    m_transformIsValid = true;
    return m_videoTransform;
}

static void runWithoutAnimations(std::function<void()> function)
{
    [CATransaction begin];
    [CATransaction setAnimationDuration:0];
    [CATransaction setDisableActions:YES];
    function();
    [CATransaction commit];
}

void MediaPlayerPrivateMediaStreamAVFObjC::enqueueVideoSample(MediaStreamTrackPrivate& track, MediaSample& sample)
{
    ASSERT(m_videoTrackMap.contains(track.id()));

    if (&track != m_mediaStreamPrivate->activeVideoTrack())
        return;

    if (!m_imagePainter.mediaSample || m_displayMode != PausedImage) {
        m_imagePainter.mediaSample = &sample;
        m_imagePainter.cgImage = nullptr;
        if (m_readyState < MediaPlayer::ReadyState::HaveEnoughData)
            updateReadyState();
    }

    if (m_displayMode != LivePreview || (m_displayMode == PausedImage && m_imagePainter.mediaSample))
        return;

    auto videoTrack = m_videoTrackMap.get(track.id());
    MediaTime timelineOffset = videoTrack->timelineOffset();
    if (timelineOffset == MediaTime::invalidTime()) {
        timelineOffset = calculateTimelineOffset(sample, rendererLatency);
        videoTrack->setTimelineOffset(timelineOffset);
        LOG(MediaCaptureSamples, "MediaPlayerPrivateMediaStreamAVFObjC::enqueueVideoSample: timeline offset for track %s set to %s", track.id().utf8().data(), toString(timelineOffset).utf8().data());
    }

    updateSampleTimes(sample, timelineOffset, "MediaPlayerPrivateMediaStreamAVFObjC::enqueueVideoSample");

    if (m_sampleBufferDisplayLayer) {
        if (sample.videoOrientation() != m_videoOrientation || sample.videoMirrored() != m_videoMirrored) {
            m_videoOrientation = sample.videoOrientation();
            m_videoMirrored = sample.videoMirrored();
            m_transformIsValid = false;
        }

        if (m_videoSizeChanged || !m_transformIsValid) {
            runWithoutAnimations([this, &sample] {
                auto backgroundBounds = m_backgroundLayer.get().bounds;
                auto videoBounds = backgroundBounds;
                if (m_videoOrientation == MediaSample::VideoOrientation::LandscapeRight || m_videoOrientation == MediaSample::VideoOrientation::LandscapeLeft)
                    std::swap(videoBounds.size.width, videoBounds.size.height);
                m_sampleBufferDisplayLayer.get().bounds = videoBounds;
                m_sampleBufferDisplayLayer.get().position = { backgroundBounds.size.width / 2, backgroundBounds.size.height / 2};
                m_sampleBufferDisplayLayer.get().affineTransform = videoTransformationMatrix(sample);
                m_videoSizeChanged = false;
            });
        }

        if (![m_sampleBufferDisplayLayer isReadyForMoreMediaData]) {
            addSampleToPendingQueue(m_pendingVideoSampleQueue, sample);
            requestNotificationWhenReadyForVideoData();
            return;
        }

        [m_sampleBufferDisplayLayer enqueueSampleBuffer:sample.platformSample().sample.cmSampleBuffer];
    }

    if (!m_hasEverEnqueuedVideoFrame) {
        m_hasEverEnqueuedVideoFrame = true;
        m_player->firstVideoFrameAvailable();
    }
}

void MediaPlayerPrivateMediaStreamAVFObjC::requestNotificationWhenReadyForVideoData()
{
    [m_sampleBufferDisplayLayer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^ {
        [m_sampleBufferDisplayLayer stopRequestingMediaData];

        while (!m_pendingVideoSampleQueue.isEmpty()) {
            if (![m_sampleBufferDisplayLayer isReadyForMoreMediaData]) {
                requestNotificationWhenReadyForVideoData();
                return;
            }

            auto sample = m_pendingVideoSampleQueue.takeFirst();
            enqueueVideoSample(*m_activeVideoTrack.get(), sample.get());
        }
    }];
}

AudioSourceProvider* MediaPlayerPrivateMediaStreamAVFObjC::audioSourceProvider()
{
    // FIXME: This should return a mix of all audio tracks - https://bugs.webkit.org/show_bug.cgi?id=160305
    return nullptr;
}

void MediaPlayerPrivateMediaStreamAVFObjC::layerErrorDidChange(AVSampleBufferDisplayLayer* layer)
{
    UNUSED_PARAM(layer);
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::layerErrorDidChange(%p) - error = %s", this, [[layer.error localizedDescription] UTF8String]);
}

void MediaPlayerPrivateMediaStreamAVFObjC::layerStatusDidChange(AVSampleBufferDisplayLayer* layer)
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::layerStatusDidChange(%p) - status = %d", this, (int)layer.status);

    if (layer.status != AVQueuedSampleBufferRenderingStatusRendering)
        return;
    if (!m_sampleBufferDisplayLayer || !m_activeVideoTrack || layer != m_sampleBufferDisplayLayer)
        return;

    auto track = m_videoTrackMap.get(m_activeVideoTrack->id());
    if (track)
        track->setTimelineOffset(MediaTime::invalidTime());
}

void MediaPlayerPrivateMediaStreamAVFObjC::flushRenderers()
{
    if (m_sampleBufferDisplayLayer)
        [m_sampleBufferDisplayLayer flush];
}

void MediaPlayerPrivateMediaStreamAVFObjC::flushAndRemoveVideoSampleBuffers()
{
    [m_sampleBufferDisplayLayer flushAndRemoveImage];
}

void MediaPlayerPrivateMediaStreamAVFObjC::ensureLayers()
{
    if (m_sampleBufferDisplayLayer)
        return;

    if (!m_mediaStreamPrivate || !m_mediaStreamPrivate->activeVideoTrack() || !m_mediaStreamPrivate->activeVideoTrack()->enabled())
        return;

    m_sampleBufferDisplayLayer = adoptNS([allocAVSampleBufferDisplayLayerInstance() init]);
    if (!m_sampleBufferDisplayLayer) {
        LOG_ERROR("MediaPlayerPrivateMediaStreamAVFObjC::ensureLayers: +[AVSampleBufferDisplayLayer alloc] failed.");
        return;
    }

    m_sampleBufferDisplayLayer.get().backgroundColor = cachedCGColor(Color::black);
    m_sampleBufferDisplayLayer.get().anchorPoint = { .5, .5 };
    m_sampleBufferDisplayLayer.get().needsDisplayOnBoundsChange = YES;
    m_sampleBufferDisplayLayer.get().videoGravity = AVLayerVideoGravityResizeAspectFill;

    m_backgroundLayer = adoptNS([[CALayer alloc] init]);
    m_backgroundLayer.get().backgroundColor = cachedCGColor(Color::black);
    m_backgroundLayer.get().needsDisplayOnBoundsChange = YES;

    [m_statusChangeListener beginObservingLayers];

    [m_backgroundLayer addSublayer:m_sampleBufferDisplayLayer.get()];

#ifndef NDEBUG
    [m_sampleBufferDisplayLayer setName:@"MediaPlayerPrivateMediaStreamAVFObjC AVSampleBufferDisplayLayer"];
    [m_backgroundLayer setName:@"MediaPlayerPrivateMediaStreamAVFObjC AVSampleBufferDisplayLayer parent"];
#endif

    updateRenderingMode();
    
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    m_videoFullscreenLayerManager->setVideoLayer(m_backgroundLayer.get(), snappedIntRect(m_player->client().mediaPlayerContentBoxRect()).size());
#endif
}

void MediaPlayerPrivateMediaStreamAVFObjC::destroyLayers()
{
    [m_statusChangeListener stopObservingLayers];
    if (m_sampleBufferDisplayLayer) {
        m_pendingVideoSampleQueue.clear();
        [m_sampleBufferDisplayLayer stopRequestingMediaData];
        [m_sampleBufferDisplayLayer flush];
        m_sampleBufferDisplayLayer = nullptr;
    }
    m_backgroundLayer = nullptr;

    updateRenderingMode();
    
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    m_videoFullscreenLayerManager->didDestroyVideoLayer();
#endif
}

#pragma mark -
#pragma mark MediaPlayerPrivateInterface Overrides

void MediaPlayerPrivateMediaStreamAVFObjC::load(const String&)
{
    // This media engine only supports MediaStream URLs.
    scheduleDeferredTask([this] {
        setNetworkState(MediaPlayer::FormatError);
    });
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateMediaStreamAVFObjC::load(const String&, MediaSourcePrivateClient*)
{
    // This media engine only supports MediaStream URLs.
    scheduleDeferredTask([this] {
        setNetworkState(MediaPlayer::FormatError);
    });
}
#endif

void MediaPlayerPrivateMediaStreamAVFObjC::load(MediaStreamPrivate& stream)
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::load(%p)", this);

    m_intrinsicSize = FloatSize();

    m_mediaStreamPrivate = &stream;
    m_mediaStreamPrivate->addObserver(*this);
    m_ended = !m_mediaStreamPrivate->active();

    scheduleDeferredTask([this] {
        updateTracks();
        setNetworkState(MediaPlayer::Idle);
        updateReadyState();
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::cancelLoad()
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::cancelLoad(%p)", this);
    if (playing())
        pause();
}

void MediaPlayerPrivateMediaStreamAVFObjC::prepareToPlay()
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::prepareToPlay(%p)", this);
}

PlatformLayer* MediaPlayerPrivateMediaStreamAVFObjC::platformLayer() const
{
    if (!m_backgroundLayer || m_displayMode == None)
        return nullptr;

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    return m_videoFullscreenLayerManager->videoInlineLayer();
#else
    return m_backgroundLayer.get();
#endif
}

PlatformLayer* MediaPlayerPrivateMediaStreamAVFObjC::displayLayer()
{
    return m_sampleBufferDisplayLayer.get();
}

PlatformLayer* MediaPlayerPrivateMediaStreamAVFObjC::backgroundLayer()
{
    return m_backgroundLayer.get();
}

MediaPlayerPrivateMediaStreamAVFObjC::DisplayMode MediaPlayerPrivateMediaStreamAVFObjC::currentDisplayMode() const
{
    if (m_ended || m_intrinsicSize.isEmpty() || !metaDataAvailable() || !m_sampleBufferDisplayLayer)
        return None;

    if (auto* track = m_mediaStreamPrivate->activeVideoTrack()) {
        if (!track->enabled() || track->muted())
            return PaintItBlack;
    }

    if (playing()) {
        if (!m_mediaStreamPrivate->isProducingData())
            return PausedImage;
        return LivePreview;
    }

    if (m_playbackState == PlaybackState::None)
        return PaintItBlack;

    return PausedImage;
}

bool MediaPlayerPrivateMediaStreamAVFObjC::updateDisplayMode()
{
    DisplayMode displayMode = currentDisplayMode();

    if (displayMode == m_displayMode)
        return false;
    m_displayMode = displayMode;

    if (m_sampleBufferDisplayLayer) {
        runWithoutAnimations([this] {
            m_sampleBufferDisplayLayer.get().hidden = m_displayMode < PausedImage;
        });
    }

    return true;
}

void MediaPlayerPrivateMediaStreamAVFObjC::play()
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::play(%p)", this);

    if (!metaDataAvailable() || playing() || m_ended)
        return;

    m_playbackState = PlaybackState::Playing;
    if (!m_clock->isRunning())
        m_clock->start();

    for (const auto& track : m_audioTrackMap.values())
        track->play();

    m_shouldDisplayFirstVideoFrame = true;
    updateDisplayMode();

    scheduleDeferredTask([this] {
        updateReadyState();
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::pause()
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::pause(%p)", this);

    if (!metaDataAvailable() || !playing() || m_ended)
        return;

    m_pausedTime = currentMediaTime();
    m_playbackState = PlaybackState::Paused;

    for (const auto& track : m_audioTrackMap.values())
        track->pause();

    updateDisplayMode();
    flushRenderers();
}

void MediaPlayerPrivateMediaStreamAVFObjC::setVolume(float volume)
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::setVolume(%p)", this);

    if (m_volume == volume)
        return;

    m_volume = volume;
    for (const auto& track : m_audioTrackMap.values())
        track->setVolume(m_muted ? 0 : m_volume);
}

void MediaPlayerPrivateMediaStreamAVFObjC::setMuted(bool muted)
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::setMuted(%p)", this);

    if (muted == m_muted)
        return;

    m_muted = muted;
    for (const auto& track : m_audioTrackMap.values())
        track->setVolume(m_muted ? 0 : m_volume);
}

bool MediaPlayerPrivateMediaStreamAVFObjC::hasVideo() const
{
    if (!metaDataAvailable())
        return false;
    
    return m_mediaStreamPrivate->hasVideo();
}

bool MediaPlayerPrivateMediaStreamAVFObjC::hasAudio() const
{
    if (!metaDataAvailable())
        return false;
    
    return m_mediaStreamPrivate->hasAudio();
}

MediaTime MediaPlayerPrivateMediaStreamAVFObjC::durationMediaTime() const
{
    return MediaTime::positiveInfiniteTime();
}

MediaTime MediaPlayerPrivateMediaStreamAVFObjC::currentMediaTime() const
{
    if (paused())
        return m_pausedTime;

    return streamTime();
}

MediaTime MediaPlayerPrivateMediaStreamAVFObjC::streamTime() const
{
    return MediaTime::createWithDouble(m_clock->currentTime());
}

MediaPlayer::NetworkState MediaPlayerPrivateMediaStreamAVFObjC::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivateMediaStreamAVFObjC::readyState() const
{
    return m_readyState;
}

MediaPlayer::ReadyState MediaPlayerPrivateMediaStreamAVFObjC::currentReadyState()
{
    if (!m_mediaStreamPrivate || !m_mediaStreamPrivate->active() || !m_mediaStreamPrivate->tracks().size())
        return MediaPlayer::ReadyState::HaveNothing;

    bool allTracksAreLive = true;
    for (auto& track : m_mediaStreamPrivate->tracks()) {
        if (!track->enabled() || track->readyState() != MediaStreamTrackPrivate::ReadyState::Live) {
            allTracksAreLive = false;
            break;
        }

        if (track == m_mediaStreamPrivate->activeVideoTrack() && !m_imagePainter.mediaSample) {
            allTracksAreLive = false;
            break;
        }
    }

    if (!allTracksAreLive && m_previousReadyState == MediaPlayer::ReadyState::HaveNothing)
        return MediaPlayer::ReadyState::HaveMetadata;

    return MediaPlayer::ReadyState::HaveEnoughData;
}

void MediaPlayerPrivateMediaStreamAVFObjC::updateReadyState()
{
    MediaPlayer::ReadyState newReadyState = currentReadyState();

    if (newReadyState != m_readyState)
        setReadyState(newReadyState);
}

void MediaPlayerPrivateMediaStreamAVFObjC::activeStatusChanged()
{
    scheduleDeferredTask([this] {
        bool ended = !m_mediaStreamPrivate->active();
        if (ended && playing())
            pause();

        updateReadyState();
        updateDisplayMode();

        if (ended != m_ended) {
            m_ended = ended;
            if (m_player)
                m_player->timeChanged();
        }
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::updateRenderingMode()
{
    if (!updateDisplayMode())
        return;

    scheduleDeferredTask([this] {
        m_transformIsValid = false;
        if (m_player)
            m_player->client().mediaPlayerRenderingModeChanged(m_player);
    });

}

void MediaPlayerPrivateMediaStreamAVFObjC::characteristicsChanged()
{
    bool sizeChanged = false;

    FloatSize intrinsicSize = m_mediaStreamPrivate->intrinsicSize();
    if (intrinsicSize.height() != m_intrinsicSize.height() || intrinsicSize.width() != m_intrinsicSize.width()) {
        m_intrinsicSize = intrinsicSize;
        sizeChanged = true;
    }

    updateTracks();
    updateDisplayMode();

    scheduleDeferredTask([this, sizeChanged] {
        updateReadyState();

        if (!m_player)
            return;

        m_player->characteristicChanged();
        if (sizeChanged) {
            m_player->sizeChanged();
        }
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::didAddTrack(MediaStreamTrackPrivate&)
{
    updateTracks();
}

void MediaPlayerPrivateMediaStreamAVFObjC::didRemoveTrack(MediaStreamTrackPrivate&)
{
    updateTracks();
}

void MediaPlayerPrivateMediaStreamAVFObjC::sampleBufferUpdated(MediaStreamTrackPrivate& track, MediaSample& mediaSample)
{
    ASSERT(track.id() == mediaSample.trackID());
    ASSERT(mediaSample.platformSample().type == PlatformSample::CMSampleBufferType);
    ASSERT(m_mediaStreamPrivate);

    if (streamTime().toDouble() < 0)
        return;

    switch (track.type()) {
    case RealtimeMediaSource::Type::None:
        // Do nothing.
        break;
    case RealtimeMediaSource::Type::Audio:
        break;
    case RealtimeMediaSource::Type::Video:
        if (&track == m_activeVideoTrack.get())
            enqueueVideoSample(track, mediaSample);
        break;
    }
}

void MediaPlayerPrivateMediaStreamAVFObjC::readyStateChanged(MediaStreamTrackPrivate&)
{
    scheduleDeferredTask([this] {
        updateReadyState();
    });
}

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
void MediaPlayerPrivateMediaStreamAVFObjC::setVideoFullscreenLayer(PlatformLayer *videoFullscreenLayer, std::function<void()> completionHandler)
{
    m_videoFullscreenLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, completionHandler);
}

void MediaPlayerPrivateMediaStreamAVFObjC::setVideoFullscreenFrame(FloatRect frame)
{
    m_videoFullscreenLayerManager->setVideoFullscreenFrame(frame);
}
#endif

typedef enum {
    Add,
    Remove,
    Configure
} TrackState;

template <typename RefT>
void updateTracksOfType(HashMap<String, RefT>& trackMap, RealtimeMediaSource::Type trackType, MediaStreamTrackPrivateVector& currentTracks, RefT (*itemFactory)(MediaStreamTrackPrivate&), const Function<void(RefT, int, TrackState)>& configureTrack)
{
    Vector<RefT> removedTracks;
    Vector<RefT> addedTracks;
    Vector<RefPtr<MediaStreamTrackPrivate>> addedPrivateTracks;

    for (const auto& track : currentTracks) {
        if (track->type() != trackType)
            continue;

        if (!trackMap.contains(track->id()))
            addedPrivateTracks.append(track);
    }

    for (const auto& track : trackMap.values()) {
        auto& streamTrack = track->streamTrack();
        if (currentTracks.contains(&streamTrack))
            continue;

        removedTracks.append(track);
        trackMap.remove(streamTrack.id());
    }

    for (auto& track : addedPrivateTracks) {
        RefT newTrack = itemFactory(*track.get());
        trackMap.add(track->id(), newTrack);
        addedTracks.append(newTrack);
    }

    int index = 0;
    for (auto& track : removedTracks)
        configureTrack(track, index++, TrackState::Remove);

    index = 0;
    for (auto& track : addedTracks)
        configureTrack(track, index++, TrackState::Add);

    index = 0;
    for (const auto& track : trackMap.values())
        configureTrack(track, index++, TrackState::Configure);
}

void MediaPlayerPrivateMediaStreamAVFObjC::checkSelectedVideoTrack()
{
    if (m_pendingSelectedTrackCheck)
        return;

    m_pendingSelectedTrackCheck = true;
    scheduleDeferredTask([this] {
        auto oldVideoTrack = m_activeVideoTrack;
        bool hideVideoLayer = true;
        m_activeVideoTrack = nullptr;
        if (m_mediaStreamPrivate->activeVideoTrack()) {
            for (const auto& track : m_videoTrackMap.values()) {
                if (&track->streamTrack() == m_mediaStreamPrivate->activeVideoTrack()) {
                    m_activeVideoTrack = m_mediaStreamPrivate->activeVideoTrack();
                    if (track->selected())
                        hideVideoLayer = false;
                    break;
                }
            }
        }

        if (oldVideoTrack != m_activeVideoTrack)
            m_imagePainter.reset();
        ensureLayers();
        m_sampleBufferDisplayLayer.get().hidden = hideVideoLayer;
        m_pendingSelectedTrackCheck = false;
        updateDisplayMode();
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::updateTracks()
{
    MediaStreamTrackPrivateVector currentTracks = m_mediaStreamPrivate->tracks();

    Function<void(RefPtr<AudioTrackPrivateMediaStreamCocoa>, int, TrackState)>  setAudioTrackState = [this](auto track, int index, TrackState state)
    {
        switch (state) {
        case TrackState::Remove:
            m_player->removeAudioTrack(*track);
            break;
        case TrackState::Add:
            track->streamTrack().addObserver(*this);
            m_player->addAudioTrack(*track);
            break;
        case TrackState::Configure:
            track->setTrackIndex(index);
            bool enabled = track->streamTrack().enabled() && !track->streamTrack().muted();
            track->setEnabled(enabled);
            break;
        }
    };
    updateTracksOfType(m_audioTrackMap, RealtimeMediaSource::Type::Audio, currentTracks, &AudioTrackPrivateMediaStreamCocoa::create, setAudioTrackState);

    Function<void(RefPtr<VideoTrackPrivateMediaStream>, int, TrackState)> setVideoTrackState = [&](auto track, int index, TrackState state)
    {
        switch (state) {
        case TrackState::Remove:
            track->streamTrack().removeObserver(*this);
            m_player->removeVideoTrack(*track);
            checkSelectedVideoTrack();
            break;
        case TrackState::Add:
            track->streamTrack().addObserver(*this);
            m_player->addVideoTrack(*track);
            break;
        case TrackState::Configure:
            track->setTrackIndex(index);
            bool selected = &track->streamTrack() == m_mediaStreamPrivate->activeVideoTrack();
            track->setSelected(selected);
            checkSelectedVideoTrack();
            break;
        }
    };
    updateTracksOfType(m_videoTrackMap, RealtimeMediaSource::Type::Video, currentTracks, &VideoTrackPrivateMediaStream::create, setVideoTrackState);
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateMediaStreamAVFObjC::seekable() const
{
    return std::make_unique<PlatformTimeRanges>();
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateMediaStreamAVFObjC::buffered() const
{
    return std::make_unique<PlatformTimeRanges>();
}

void MediaPlayerPrivateMediaStreamAVFObjC::paint(GraphicsContext& context, const FloatRect& rect)
{
    paintCurrentFrameInContext(context, rect);
}

void MediaPlayerPrivateMediaStreamAVFObjC::updateCurrentFrameImage()
{
    if (m_imagePainter.cgImage || !m_imagePainter.mediaSample)
        return;

    if (!m_imagePainter.pixelBufferConformer)
        m_imagePainter.pixelBufferConformer = std::make_unique<PixelBufferConformerCV>((CFDictionaryRef)@{ (NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) });

    ASSERT(m_imagePainter.pixelBufferConformer);
    if (!m_imagePainter.pixelBufferConformer)
        return;

    auto pixelBuffer = static_cast<CVPixelBufferRef>(CMSampleBufferGetImageBuffer(m_imagePainter.mediaSample->platformSample().sample.cmSampleBuffer));
    m_imagePainter.cgImage = m_imagePainter.pixelBufferConformer->createImageFromPixelBuffer(pixelBuffer);
}

void MediaPlayerPrivateMediaStreamAVFObjC::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& destRect)
{
    if (m_displayMode == None || !metaDataAvailable() || context.paintingDisabled())
        return;

    if (m_displayMode != PaintItBlack && m_imagePainter.mediaSample)
        updateCurrentFrameImage();

    GraphicsContextStateSaver stateSaver(context);
    if (m_displayMode == PaintItBlack || !m_imagePainter.cgImage || !m_imagePainter.mediaSample) {
        context.fillRect(IntRect(IntPoint(), IntSize(destRect.width(), destRect.height())), Color::black);
        return;
    }

    auto image = m_imagePainter.cgImage.get();
    FloatRect imageRect(0, 0, CGImageGetWidth(image), CGImageGetHeight(image));
    AffineTransform videoTransform = videoTransformationMatrix(*m_imagePainter.mediaSample);
    FloatRect transformedDestRect = videoTransform.inverse().value_or(AffineTransform()).mapRect(destRect);
    context.concatCTM(videoTransform);
    context.drawNativeImage(image, imageRect.size(), transformedDestRect, imageRect);
}

void MediaPlayerPrivateMediaStreamAVFObjC::acceleratedRenderingStateChanged()
{
    if (m_player->client().mediaPlayerRenderingCanBeAccelerated(m_player))
        ensureLayers();
    else
        destroyLayers();
}

String MediaPlayerPrivateMediaStreamAVFObjC::engineDescription() const
{
    static NeverDestroyed<String> description(ASCIILiteral("AVFoundation MediaStream Engine"));
    return description;
}

void MediaPlayerPrivateMediaStreamAVFObjC::setReadyState(MediaPlayer::ReadyState readyState)
{
    if (m_readyState == readyState)
        return;

    m_previousReadyState = m_readyState;
    m_readyState = readyState;
    characteristicsChanged();

    m_player->readyStateChanged();
}

void MediaPlayerPrivateMediaStreamAVFObjC::setNetworkState(MediaPlayer::NetworkState networkState)
{
    if (m_networkState == networkState)
        return;

    m_networkState = networkState;
    m_player->networkStateChanged();
}

void MediaPlayerPrivateMediaStreamAVFObjC::setShouldBufferData(bool shouldBuffer)
{
    if (!shouldBuffer)
        flushAndRemoveVideoSampleBuffers();
}

void MediaPlayerPrivateMediaStreamAVFObjC::scheduleDeferredTask(Function<void ()>&& function)
{
    ASSERT(function);
    callOnMainThread([weakThis = createWeakPtr(), function = WTFMove(function)] {
        if (!weakThis)
            return;

        function();
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::CurrentFramePainter::reset()
{
    cgImage = nullptr;
    mediaSample = nullptr;
    pixelBufferConformer = nullptr;
}

void MediaPlayerPrivateMediaStreamAVFObjC::backgroundLayerBoundsChanged()
{
    if (!m_backgroundLayer || !m_sampleBufferDisplayLayer)
        return;

    m_videoSizeChanged = true;
}

}

#endif
