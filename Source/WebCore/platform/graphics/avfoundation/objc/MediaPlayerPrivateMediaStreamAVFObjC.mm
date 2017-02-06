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
#import "CoreMediaSoftLink.h"
#import "GraphicsContext.h"
#import "Logging.h"
#import "MediaStreamPrivate.h"
#import "MediaTimeAVFoundation.h"
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

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVSampleBufferDisplayLayer)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVSampleBufferRenderSynchronizer)

SOFT_LINK_CONSTANT(AVFoundation, AVAudioTimePitchAlgorithmSpectral, NSString*)
SOFT_LINK_CONSTANT(AVFoundation, AVAudioTimePitchAlgorithmVarispeed, NSString*)

#define AVAudioTimePitchAlgorithmSpectral getAVAudioTimePitchAlgorithmSpectral()
#define AVAudioTimePitchAlgorithmVarispeed getAVAudioTimePitchAlgorithmVarispeed()

using namespace WebCore;

@interface WebAVSampleBufferStatusChangeListener : NSObject {
    MediaPlayerPrivateMediaStreamAVFObjC* _parent;
    Vector<RetainPtr<AVSampleBufferDisplayLayer>> _layers;
}

- (id)initWithParent:(MediaPlayerPrivateMediaStreamAVFObjC*)callback;
- (void)invalidate;
- (void)beginObservingLayer:(AVSampleBufferDisplayLayer *)layer;
- (void)stopObservingLayer:(AVSampleBufferDisplayLayer *)layer;
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
    for (auto& layer : _layers)
        [layer removeObserver:self forKeyPath:@"status"];
    _layers.clear();

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    _parent = nullptr;
}

- (void)beginObservingLayer:(AVSampleBufferDisplayLayer*)layer
{
    ASSERT(_parent);
    ASSERT(!_layers.contains(layer));

    _layers.append(layer);
    [layer addObserver:self forKeyPath:@"status" options:NSKeyValueObservingOptionNew context:nullptr];
}

- (void)stopObservingLayer:(AVSampleBufferDisplayLayer*)layer
{
    ASSERT(_parent);
    ASSERT(_layers.contains(layer));

    [layer removeObserver:self forKeyPath:@"status"];
    _layers.remove(_layers.find(layer));
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(keyPath);
    ASSERT(_parent);

    RetainPtr<WebAVSampleBufferStatusChangeListener> protectedSelf = self;
    if ([object isKindOfClass:getAVSampleBufferDisplayLayerClass()]) {
        RetainPtr<AVSampleBufferDisplayLayer> layer = (AVSampleBufferDisplayLayer *)object;
        RetainPtr<NSNumber> status = [change valueForKey:NSKeyValueChangeNewKey];

        ASSERT(_layers.contains(layer.get()));
        ASSERT([keyPath isEqualToString:@"status"]);

        callOnMainThread([protectedSelf = WTFMove(protectedSelf), layer = WTFMove(layer), status = WTFMove(status)] {
            if (!protectedSelf->_parent)
                return;

            protectedSelf->_parent->layerStatusDidChange(layer.get(), status.get());
        });

    } else
        ASSERT_NOT_REACHED();
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
    for (const auto& track : m_audioTrackMap.values())
        track->pause();

    if (m_mediaStreamPrivate) {
        m_mediaStreamPrivate->removeObserver(*this);

        for (auto& track : m_mediaStreamPrivate->tracks())
            track->removeObserver(*this);
    }

    destroyLayer();

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

#if PLATFORM(MAC)
    if (!getAVSampleBufferRenderSynchronizerClass())
        return false;
#endif

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
        LOG(Media, "%s(%p): *NOTE* audio sample at time %s is only %s seconds early", loggingPrefix, this, toString(now).utf8().data(), delta);
    else if (delta > .3)
        LOG(Media, "%s(%p): *NOTE* audio sample at time %s is %s seconds early!", loggingPrefix, this, toString(now).utf8().data(), delta);
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

void MediaPlayerPrivateMediaStreamAVFObjC::enqueueVideoSample(MediaStreamTrackPrivate& track, MediaSample& sample)
{
    ASSERT(m_videoTrackMap.contains(track.id()));

    if (&track != m_mediaStreamPrivate->activeVideoTrack())
        return;

    m_hasReceivedMedia = true;
    updateReadyState();
    if (m_displayMode != LivePreview || (m_displayMode == PausedImage && m_isFrameDisplayed))
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
        if (![m_sampleBufferDisplayLayer isReadyForMoreMediaData]) {
            addSampleToPendingQueue(m_pendingVideoSampleQueue, sample);
            requestNotificationWhenReadyForVideoData();
            return;
        }

        [m_sampleBufferDisplayLayer enqueueSampleBuffer:sample.platformSample().sample.cmSampleBuffer];
    }

    m_isFrameDisplayed = true;
    if (!m_hasEverEnqueuedVideoFrame) {
        m_hasEverEnqueuedVideoFrame = true;
        if (m_displayMode == PausedImage)
            updatePausedImage();
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

void MediaPlayerPrivateMediaStreamAVFObjC::layerStatusDidChange(AVSampleBufferDisplayLayer* layer, NSNumber* status)
{
    if (status.integerValue != AVQueuedSampleBufferRenderingStatusRendering)
        return;

    if (layer != m_sampleBufferDisplayLayer || !m_activeVideoTrack)
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

bool MediaPlayerPrivateMediaStreamAVFObjC::shouldEnqueueVideoSampleBuffer() const
{
    if (m_displayMode == LivePreview)
        return true;

    if (m_displayMode == PausedImage && !m_isFrameDisplayed)
        return true;

    return false;
}

void MediaPlayerPrivateMediaStreamAVFObjC::flushAndRemoveVideoSampleBuffers()
{
    [m_sampleBufferDisplayLayer flushAndRemoveImage];
    m_isFrameDisplayed = false;
}

void MediaPlayerPrivateMediaStreamAVFObjC::ensureLayer()
{
    if (m_sampleBufferDisplayLayer)
        return;

    m_sampleBufferDisplayLayer = adoptNS([allocAVSampleBufferDisplayLayerInstance() init]);
#ifndef NDEBUG
    [m_sampleBufferDisplayLayer setName:@"MediaPlayerPrivateMediaStreamAVFObjC AVSampleBufferDisplayLayer"];
#endif
    m_sampleBufferDisplayLayer.get().backgroundColor = cachedCGColor(Color::black);
    [m_statusChangeListener beginObservingLayer:m_sampleBufferDisplayLayer.get()];

    renderingModeChanged();
    
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    m_videoFullscreenLayerManager->setVideoLayer(m_sampleBufferDisplayLayer.get(), snappedIntRect(m_player->client().mediaPlayerContentBoxRect()).size());
#endif
}

void MediaPlayerPrivateMediaStreamAVFObjC::destroyLayer()
{
    if (!m_sampleBufferDisplayLayer)
        return;

    if (m_sampleBufferDisplayLayer) {
        m_pendingVideoSampleQueue.clear();
        [m_statusChangeListener stopObservingLayer:m_sampleBufferDisplayLayer.get()];
        [m_sampleBufferDisplayLayer stopRequestingMediaData];
        [m_sampleBufferDisplayLayer flush];
    }

    renderingModeChanged();
    
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
    if (m_playing)
        pause();
}

void MediaPlayerPrivateMediaStreamAVFObjC::prepareToPlay()
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::prepareToPlay(%p)", this);
}

PlatformLayer* MediaPlayerPrivateMediaStreamAVFObjC::platformLayer() const
{
    if (!m_sampleBufferDisplayLayer || m_displayMode == None)
        return nullptr;

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    return m_videoFullscreenLayerManager->videoInlineLayer();
#else

    return m_sampleBufferDisplayLayer.get();
#endif
}

MediaPlayerPrivateMediaStreamAVFObjC::DisplayMode MediaPlayerPrivateMediaStreamAVFObjC::currentDisplayMode() const
{
    if (m_ended || m_intrinsicSize.isEmpty() || !metaDataAvailable() || !m_sampleBufferDisplayLayer)
        return None;

    if (m_mediaStreamPrivate->activeVideoTrack() && !m_mediaStreamPrivate->activeVideoTrack()->enabled())
        return PaintItBlack;

    if (m_playing) {
        if (!m_mediaStreamPrivate->isProducingData())
            return PausedImage;
        return LivePreview;
    }

    return PausedImage;
}

void MediaPlayerPrivateMediaStreamAVFObjC::updateDisplayMode()
{
    DisplayMode displayMode = currentDisplayMode();

    if (displayMode == m_displayMode)
        return;
    m_displayMode = displayMode;

    if (m_displayMode < PausedImage && m_sampleBufferDisplayLayer)
        flushAndRemoveVideoSampleBuffers();
}

void MediaPlayerPrivateMediaStreamAVFObjC::updatePausedImage()
{
    if (m_displayMode < PausedImage)
        return;

    RefPtr<Image> image = m_mediaStreamPrivate->currentFrameImage();
    ASSERT(image);
    if (!image)
        return;

    m_pausedImage = image->nativeImage();
    ASSERT(m_pausedImage);
}

void MediaPlayerPrivateMediaStreamAVFObjC::play()
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::play(%p)", this);

    if (!metaDataAvailable() || m_playing || m_ended)
        return;

    m_playing = true;
    if (!m_clock->isRunning())
        m_clock->start();

    for (const auto& track : m_audioTrackMap.values())
        track->play();

    m_haveEverPlayed = true;
    scheduleDeferredTask([this] {
        updateDisplayMode();
        updateReadyState();
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::pause()
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::pause(%p)", this);

    if (!metaDataAvailable() || !m_playing || m_ended)
        return;

    m_pausedTime = currentMediaTime();
    m_playing = false;

    for (const auto& track : m_audioTrackMap.values())
        track->pause();

    updateDisplayMode();
    updatePausedImage();
    flushRenderers();
}

bool MediaPlayerPrivateMediaStreamAVFObjC::paused() const
{
    return !m_playing;
}

void MediaPlayerPrivateMediaStreamAVFObjC::setVolume(float volume)
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::setVolume(%p)", this);

    if (m_volume == volume)
        return;

    m_volume = volume;
    for (const auto& track : m_audioTrackMap.values())
        track->setVolume(m_volume);
}

void MediaPlayerPrivateMediaStreamAVFObjC::setMuted(bool muted)
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::setMuted(%p)", this);

    if (muted == m_muted)
        return;

    m_muted = muted;
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
    if (!m_playing)
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
    if (!m_mediaStreamPrivate)
        return MediaPlayer::ReadyState::HaveNothing;

    // https://w3c.github.io/mediacapture-main/ Change 8. from July 4, 2013.
    // FIXME: Only update readyState to HAVE_ENOUGH_DATA when all active tracks have sent a sample buffer.
    if (m_mediaStreamPrivate->active() && m_hasReceivedMedia)
        return MediaPlayer::ReadyState::HaveEnoughData;

    updateDisplayMode();

    if (m_displayMode == PausedImage)
        return MediaPlayer::ReadyState::HaveCurrentData;

    return MediaPlayer::ReadyState::HaveMetadata;
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
        if (ended && m_playing)
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

void MediaPlayerPrivateMediaStreamAVFObjC::updateIntrinsicSize(const FloatSize& size)
{
    if (size == m_intrinsicSize)
        return;

    m_intrinsicSize = size;
}

void MediaPlayerPrivateMediaStreamAVFObjC::renderingModeChanged()
{
    updateDisplayMode();
    scheduleDeferredTask([this] {
        if (m_player)
            m_player->client().mediaPlayerRenderingModeChanged(m_player);
    });

}

void MediaPlayerPrivateMediaStreamAVFObjC::characteristicsChanged()
{
    bool sizeChanged = false;

    FloatSize intrinsicSize = m_mediaStreamPrivate->intrinsicSize();
    if (intrinsicSize.height() != m_intrinsicSize.height() || intrinsicSize.width() != m_intrinsicSize.width()) {
        updateIntrinsicSize(intrinsicSize);
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

    if (!m_hasReceivedMedia) {
        m_hasReceivedMedia = true;
        updateReadyState();
    }

    if (!m_playing || streamTime().toDouble() < 0)
        return;

    switch (track.type()) {
    case RealtimeMediaSource::None:
        // Do nothing.
        break;
    case RealtimeMediaSource::Audio:
        break;
    case RealtimeMediaSource::Video:
        if (&track == m_activeVideoTrack.get())
            enqueueVideoSample(track, mediaSample);
        break;
    }
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

        ensureLayer();
        m_sampleBufferDisplayLayer.get().hidden = hideVideoLayer;
        m_pendingSelectedTrackCheck = false;
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
            m_player->addAudioTrack(*track);
            break;
        case TrackState::Configure:
            track->setTrackIndex(index);
            bool enabled = track->streamTrack().enabled() && !track->streamTrack().muted();
            track->setEnabled(enabled);
            break;
        }
    };
    updateTracksOfType(m_audioTrackMap, RealtimeMediaSource::Audio, currentTracks, &AudioTrackPrivateMediaStreamCocoa::create, setAudioTrackState);

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
    updateTracksOfType(m_videoTrackMap, RealtimeMediaSource::Video, currentTracks, &VideoTrackPrivateMediaStream::create, setVideoTrackState);
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

void MediaPlayerPrivateMediaStreamAVFObjC::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& rect)
{
    if (m_displayMode == None || !metaDataAvailable() || context.paintingDisabled())
        return;

    if (m_displayMode == LivePreview)
        m_mediaStreamPrivate->paintCurrentFrameInContext(context, rect);
    else {
        GraphicsContextStateSaver stateSaver(context);
        context.translate(rect.x(), rect.y() + rect.height());
        context.scale(FloatSize(1, -1));
        IntRect paintRect(IntPoint(0, 0), IntSize(rect.width(), rect.height()));
        context.setImageInterpolationQuality(InterpolationLow);

        if (m_displayMode == PausedImage && m_pausedImage)
            CGContextDrawImage(context.platformContext(), CGRectMake(0, 0, paintRect.width(), paintRect.height()), m_pausedImage.get());
        else
            context.fillRect(paintRect, Color::black);
    }
}

void MediaPlayerPrivateMediaStreamAVFObjC::acceleratedRenderingStateChanged()
{
    if (m_player->client().mediaPlayerRenderingCanBeAccelerated(m_player))
        ensureLayer();
    else
        destroyLayer();
}

String MediaPlayerPrivateMediaStreamAVFObjC::engineDescription() const
{
    static NeverDestroyed<String> description(ASCIILiteral("AVFoundation MediaStream Engine"));
    return description;
}

bool MediaPlayerPrivateMediaStreamAVFObjC::shouldBePlaying() const
{
    return m_playing && m_readyState >= MediaPlayer::HaveFutureData;
}

void MediaPlayerPrivateMediaStreamAVFObjC::setReadyState(MediaPlayer::ReadyState readyState)
{
    if (m_readyState == readyState)
        return;

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

void MediaPlayerPrivateMediaStreamAVFObjC::scheduleDeferredTask(Function<void ()>&& function)
{
    ASSERT(function);
    callOnMainThread([weakThis = createWeakPtr(), function = WTFMove(function)] {
        if (!weakThis)
            return;

        function();
    });
}

}

#endif
