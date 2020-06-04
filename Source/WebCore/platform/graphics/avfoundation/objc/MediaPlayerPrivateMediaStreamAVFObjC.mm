/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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

#import "AudioTrackPrivateMediaStream.h"
#import "GraphicsContextCG.h"
#import "Logging.h"
#import "LocalSampleBufferDisplayLayer.h"
#import "MediaStreamPrivate.h"
#import "PixelBufferConformerCV.h"
#import "VideoLayerManagerObjC.h"
#import "VideoTrackPrivateMediaStream.h"
#import <CoreGraphics/CGAffineTransform.h>
#import <objc_runtime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/system/Clock.h>
#import <wtf/Lock.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>

#import "CoreVideoSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

@interface WebRootSampleBufferBoundsChangeListener : NSObject {
    Function<void()> _callback;
    RetainPtr<CALayer> _rootLayer;
}

- (id)initWithCallback:(Function<void()>&&) callback;
- (void)invalidate;
- (void)begin:(CALayer*) layer;
- (void)stop;
@end

@implementation WebRootSampleBufferBoundsChangeListener

- (id)initWithCallback:(Function<void()>&&) callback
{
    if (!(self = [super init]))
        return nil;

    _callback = WTFMove(callback);

    return self;
}

- (void)dealloc
{
    [self invalidate];
    [super dealloc];
}

- (void)invalidate
{
    [self stop];
    _callback = nullptr;
}

- (void)begin:(CALayer*) layer
{
    ASSERT(_callback);
    _rootLayer = layer;
    [_rootLayer addObserver:self forKeyPath:@"bounds" options:NSKeyValueObservingOptionNew context:nil];
}

- (void)stop
{
    if (!_rootLayer)
        return;

    [_rootLayer removeObserver:self forKeyPath:@"bounds"];
    _rootLayer = nullptr;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(keyPath);

    if ([[change valueForKey:NSKeyValueChangeNotificationIsPriorKey] boolValue])
        return;

    if ((CALayer *)object == _rootLayer.get()) {
        if ([keyPath isEqualToString:@"bounds"]) {
            if (isMainThread()) {
                if (_callback)
                    _callback();
                return;
            }

            callOnMainThread([protectedSelf = RetainPtr<WebRootSampleBufferBoundsChangeListener>(self)] {
                if (protectedSelf->_callback)
                    protectedSelf->_callback();
            });
        }
    }

}
@end

namespace WebCore {
using namespace PAL;

#pragma mark -
#pragma mark MediaPlayerPrivateMediaStreamAVFObjC

MediaPlayerPrivateMediaStreamAVFObjC::MediaPlayerPrivateMediaStreamAVFObjC(MediaPlayer* player)
    : m_player(player)
    , m_clock(PAL::Clock::create())
    , m_logger(player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
    , m_videoLayerManager(makeUnique<VideoLayerManagerObjC>(m_logger, m_logIdentifier))
{
    INFO_LOG(LOGIDENTIFIER);
    // MediaPlayerPrivateMediaStreamAVFObjC::processNewVideoSample expects a weak pointer to be created in the constructor.
    m_boundsChangeListener = adoptNS([[WebRootSampleBufferBoundsChangeListener alloc] initWithCallback:[this, weakThis = makeWeakPtr(this)] {
        if (!weakThis)
            return;
        rootLayerBoundsDidChange();
    }]);
}

MediaPlayerPrivateMediaStreamAVFObjC::~MediaPlayerPrivateMediaStreamAVFObjC()
{
    INFO_LOG(LOGIDENTIFIER);

    for (const auto& track : m_audioTrackMap.values())
        track->pause();

    if (m_mediaStreamPrivate)
        m_mediaStreamPrivate->removeObserver(*this);

    for (auto& track : m_audioTrackMap.values())
        track->streamTrack().removeObserver(*this);

    for (auto& track : m_videoTrackMap.values())
        track->streamTrack().removeObserver(*this);

    if (m_activeVideoTrack)
        m_activeVideoTrack->streamTrack().source().removeVideoSampleObserver(*this);

    [m_boundsChangeListener invalidate];

    destroyLayers();

    auto audioTrackMap = WTFMove(m_audioTrackMap);
    for (auto& track : audioTrackMap.values())
        track->clear();

    m_videoTrackMap.clear();
}

#pragma mark -
#pragma mark MediaPlayer Factory Methods

class MediaPlayerFactoryMediaStreamAVFObjC final : public MediaPlayerFactory {
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMediaStream; };

    std::unique_ptr<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer* player) const final
    {
        return makeUnique<MediaPlayerPrivateMediaStreamAVFObjC>(player);
    }

    void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types) const final
    {
        return MediaPlayerPrivateMediaStreamAVFObjC::getSupportedTypes(types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return MediaPlayerPrivateMediaStreamAVFObjC::supportsType(parameters);
    }
};

void MediaPlayerPrivateMediaStreamAVFObjC::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (!isAvailable())
        return;

    registrar(makeUnique<MediaPlayerFactoryMediaStreamAVFObjC>());
}

bool MediaPlayerPrivateMediaStreamAVFObjC::isAvailable()
{
    return isAVFoundationFrameworkAvailable() && isCoreMediaFrameworkAvailable() && getAVSampleBufferDisplayLayerClass();
}

void MediaPlayerPrivateMediaStreamAVFObjC::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types)
{
    // FIXME: Is it really correct to list no supported types?
    types.clear();
}

MediaPlayer::SupportsType MediaPlayerPrivateMediaStreamAVFObjC::supportsType(const MediaEngineSupportParameters& parameters)
{
    return parameters.isMediaStream ? MediaPlayer::SupportsType::IsSupported : MediaPlayer::SupportsType::IsNotSupported;
}

#pragma mark -
#pragma mark AVSampleBuffer Methods

static inline CGAffineTransform videoTransformationMatrix(MediaSample& sample)
{
    CMSampleBufferRef sampleBuffer = sample.platformSample().sample.cmSampleBuffer;
    CVPixelBufferRef pixelBuffer = static_cast<CVPixelBufferRef>(CMSampleBufferGetImageBuffer(sampleBuffer));
    size_t width = CVPixelBufferGetWidth(pixelBuffer);
    size_t height = CVPixelBufferGetHeight(pixelBuffer);
    if (!width || !height)
        return CGAffineTransformIdentity;

    auto videoTransform = CGAffineTransformMakeRotation(static_cast<int>(sample.videoRotation()) * M_PI / 180);
    if (sample.videoMirrored())
        videoTransform = CGAffineTransformScale(videoTransform, -1, 1);

    return videoTransform;
}

void MediaPlayerPrivateMediaStreamAVFObjC::videoSampleAvailable(MediaSample& sample)
{
    processNewVideoSample(sample, sample.videoRotation() != m_videoRotation || sample.videoMirrored() != m_videoMirrored);
    enqueueVideoSample(sample);
}

void MediaPlayerPrivateMediaStreamAVFObjC::enqueueVideoSample(MediaSample& sample)
{
    auto locker = tryHoldLock(m_sampleBufferDisplayLayerLock);
    if (!locker)
        return;

    if (!m_canEnqueueDisplayLayer || !m_sampleBufferDisplayLayer || m_sampleBufferDisplayLayer->didFail())
        return;

    if (sample.videoRotation() != m_videoRotation || sample.videoMirrored() != m_videoMirrored) {
        m_videoRotation = sample.videoRotation();
        m_videoMirrored = sample.videoMirrored();
        m_sampleBufferDisplayLayer->updateAffineTransform(videoTransformationMatrix(sample));
        m_shouldUpdateDisplayLayer = true;
    }
    if (m_shouldUpdateDisplayLayer) {
        m_sampleBufferDisplayLayer->updateBoundsAndPosition(m_sampleBufferDisplayLayer->rootLayer().bounds, m_videoRotation);
        m_shouldUpdateDisplayLayer = false;
    }

    m_sampleBufferDisplayLayer->enqueueSample(sample);
}

void MediaPlayerPrivateMediaStreamAVFObjC::processNewVideoSample(MediaSample& sample,  bool hasChangedOrientation)
{
    if (!isMainThread()) {
        callOnMainThread([weakThis = makeWeakPtr(this), sample = makeRef(sample), hasChangedOrientation]() mutable {
            if (weakThis)
                weakThis->processNewVideoSample(sample.get(), hasChangedOrientation);
        });
        return;
    }

    if (!m_activeVideoTrack)
        return;

    if (hasChangedOrientation)
        m_videoTransform = { };

    if (!m_imagePainter.mediaSample || m_displayMode != PausedImage) {
        m_imagePainter.mediaSample = &sample;
        m_imagePainter.cgImage = nullptr;
        if (m_readyState < MediaPlayer::ReadyState::HaveEnoughData)
            updateReadyState();
    }

    if (m_displayMode != LivePreview && !m_waitingForFirstImage)
        return;

    if (!m_hasEverEnqueuedVideoFrame) {
        m_hasEverEnqueuedVideoFrame = true;
        m_player->firstVideoFrameAvailable();
    }

    if (m_waitingForFirstImage) {
        m_waitingForFirstImage = false;
        updateDisplayMode();
    }
}

AudioSourceProvider* MediaPlayerPrivateMediaStreamAVFObjC::audioSourceProvider()
{
    // FIXME: This should return a mix of all audio tracks - https://bugs.webkit.org/show_bug.cgi?id=160305
    return nullptr;
}

void MediaPlayerPrivateMediaStreamAVFObjC::sampleBufferDisplayLayerStatusDidChange(SampleBufferDisplayLayer&)
{
}

void MediaPlayerPrivateMediaStreamAVFObjC::applicationDidBecomeActive()
{
    if (m_sampleBufferDisplayLayer && m_sampleBufferDisplayLayer->didFail()) {
        flushRenderers();
        if (m_imagePainter.mediaSample)
            enqueueVideoSample(*m_imagePainter.mediaSample);
        updateDisplayMode();
    }
}

void MediaPlayerPrivateMediaStreamAVFObjC::flushRenderers()
{
    if (m_sampleBufferDisplayLayer)
        m_sampleBufferDisplayLayer->flush();
}

void MediaPlayerPrivateMediaStreamAVFObjC::ensureLayers()
{
    if (m_sampleBufferDisplayLayer)
        return;

    auto* activeVideoTrack = this->activeVideoTrack();
    if (!activeVideoTrack || !activeVideoTrack->enabled())
        return;

    m_canEnqueueDisplayLayer = false;
    m_sampleBufferDisplayLayer = SampleBufferDisplayLayer::create(*this);
    ERROR_LOG_IF(!m_sampleBufferDisplayLayer, LOGIDENTIFIER, "Creating the SampleBufferDisplayLayer failed.");
    if (!m_sampleBufferDisplayLayer)
        return;

    if (activeVideoTrack->source().isCaptureSource())
        m_sampleBufferDisplayLayer->setRenderPolicy(SampleBufferDisplayLayer::RenderPolicy::Immediately);

    auto size = snappedIntRect(m_player->playerContentBoxRect()).size();
    m_sampleBufferDisplayLayer->initialize(hideRootLayer(), size, [this, weakThis = makeWeakPtr(this), size](auto didSucceed) {
        if (!didSucceed) {
            ERROR_LOG(LOGIDENTIFIER, "Initializing the SampleBufferDisplayLayer failed.");
            m_sampleBufferDisplayLayer = nullptr;
            return;
        }
        updateRenderingMode();
        m_shouldUpdateDisplayLayer = true;

        m_videoLayerManager->setVideoLayer(m_sampleBufferDisplayLayer->rootLayer(), size);

        [m_boundsChangeListener begin:m_sampleBufferDisplayLayer->rootLayer()];

        m_canEnqueueDisplayLayer = true;
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::destroyLayers()
{
    auto locker = holdLock(m_sampleBufferDisplayLayerLock);

    m_canEnqueueDisplayLayer = false;
    if (m_sampleBufferDisplayLayer)
        m_sampleBufferDisplayLayer = nullptr;

    updateRenderingMode();
    
    m_videoLayerManager->didDestroyVideoLayer();
}

#pragma mark -
#pragma mark MediaPlayerPrivateInterface Overrides

void MediaPlayerPrivateMediaStreamAVFObjC::load(const String&)
{
    // This media engine only supports MediaStream URLs.
    scheduleDeferredTask([this] {
        setNetworkState(MediaPlayer::NetworkState::FormatError);
    });
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateMediaStreamAVFObjC::load(const String&, MediaSourcePrivateClient*)
{
    // This media engine only supports MediaStream URLs.
    scheduleDeferredTask([this] {
        setNetworkState(MediaPlayer::NetworkState::FormatError);
    });
}
#endif

void MediaPlayerPrivateMediaStreamAVFObjC::load(MediaStreamPrivate& stream)
{
    INFO_LOG(LOGIDENTIFIER);

    m_intrinsicSize = FloatSize();

    m_mediaStreamPrivate = &stream;
    m_mediaStreamPrivate->addObserver(*this);
    m_ended = !m_mediaStreamPrivate->active();

    scheduleDeferredTask([this] {
        updateTracks();
        setNetworkState(MediaPlayer::NetworkState::Idle);
        updateReadyState();
    });
}

MediaStreamTrackPrivate* MediaPlayerPrivateMediaStreamAVFObjC::activeVideoTrack() const
{
    return (m_mediaStreamPrivate && m_player->isVideoPlayer()) ? m_mediaStreamPrivate->activeVideoTrack() : nullptr;
}

bool MediaPlayerPrivateMediaStreamAVFObjC::didPassCORSAccessCheck() const
{
    // We are only doing a check on the active video track since the sole consumer of this API is canvas.
    // FIXME: We should change the name of didPassCORSAccessCheck if it is expected to stay like this.
    const auto* track = activeVideoTrack();
    return !track || !track->isIsolated();
}

void MediaPlayerPrivateMediaStreamAVFObjC::cancelLoad()
{
    INFO_LOG(LOGIDENTIFIER);
    if (playing())
        pause();
}

void MediaPlayerPrivateMediaStreamAVFObjC::prepareToPlay()
{
    INFO_LOG(LOGIDENTIFIER);
}

PlatformLayer* MediaPlayerPrivateMediaStreamAVFObjC::platformLayer() const
{
    if (!m_sampleBufferDisplayLayer || !m_sampleBufferDisplayLayer->rootLayer() || m_displayMode == None)
        return nullptr;

    return m_videoLayerManager->videoInlineLayer();
}

MediaPlayerPrivateMediaStreamAVFObjC::DisplayMode MediaPlayerPrivateMediaStreamAVFObjC::currentDisplayMode() const
{
    if (m_intrinsicSize.isEmpty() || !metaDataAvailable())
        return None;

    if (auto* track = activeVideoTrack()) {
        if (!track->enabled() || track->muted() || track->ended())
            return PaintItBlack;
    }

    if (m_waitingForFirstImage)
        return WaitingForFirstImage;

    if (playing() && !m_ended) {
        if (!m_mediaStreamPrivate->isProducingData())
            return PausedImage;
        return LivePreview;
    }

    if (m_playbackState == PlaybackState::None || m_ended)
        return PaintItBlack;

    return PausedImage;
}

bool MediaPlayerPrivateMediaStreamAVFObjC::updateDisplayMode()
{
    DisplayMode displayMode = currentDisplayMode();

    if (displayMode == m_displayMode)
        return false;

    INFO_LOG(LOGIDENTIFIER, "updated to ", static_cast<int>(displayMode));
    m_displayMode = displayMode;

    if (m_sampleBufferDisplayLayer)
        m_sampleBufferDisplayLayer->updateDisplayMode(m_displayMode < PausedImage, hideRootLayer());

    return true;
}

void MediaPlayerPrivateMediaStreamAVFObjC::play()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (!metaDataAvailable() || playing() || m_ended)
        return;

    m_playbackState = PlaybackState::Playing;
    if (!m_clock->isRunning())
        m_clock->start();

    for (const auto& track : m_audioTrackMap.values())
        track->play();

    updateDisplayMode();

    scheduleDeferredTask([this] {
        updateReadyState();
        if (m_player)
            m_player->rateChanged();
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::pause()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (!metaDataAvailable() || !playing() || m_ended)
        return;

    m_pausedTime = currentMediaTime();
    m_playbackState = PlaybackState::Paused;

    for (const auto& track : m_audioTrackMap.values())
        track->pause();

    updateDisplayMode();
    flushRenderers();

    scheduleDeferredTask([this] {
        if (m_player)
            m_player->rateChanged();
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::setVolume(float volume)
{
    if (m_volume == volume)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, volume);
    m_volume = volume;
    for (const auto& track : m_audioTrackMap.values())
        track->setVolume(m_volume);
}

void MediaPlayerPrivateMediaStreamAVFObjC::setMuted(bool muted)
{
    if (muted == m_muted)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, muted);
    m_muted = muted;
    for (const auto& track : m_audioTrackMap.values())
        track->setMuted(m_muted);
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

void MediaPlayerPrivateMediaStreamAVFObjC::setVisible(bool visible)
{
    if (m_visible == visible)
        return;

    m_visible = visible;
    if (m_visible)
        flushRenderers();
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
    if (!m_mediaStreamPrivate || !m_mediaStreamPrivate->active() || !m_mediaStreamPrivate->hasTracks())
        return MediaPlayer::ReadyState::HaveNothing;

    bool waitingForImage = activeVideoTrack() && !m_imagePainter.mediaSample;
    if (waitingForImage && (!m_haveSeenMetadata || m_waitingForFirstImage))
        return MediaPlayer::ReadyState::HaveNothing;

    bool allTracksAreLive = !waitingForImage;
    if (allTracksAreLive) {
        m_mediaStreamPrivate->forEachTrack([&](auto& track) {
            if (!track.enabled() || track.readyState() != MediaStreamTrackPrivate::ReadyState::Live)
                allTracksAreLive = false;
        });
    }

    if (m_waitingForFirstImage || (!allTracksAreLive && !m_haveSeenMetadata))
        return MediaPlayer::ReadyState::HaveMetadata;

    return MediaPlayer::ReadyState::HaveEnoughData;
}

void MediaPlayerPrivateMediaStreamAVFObjC::updateReadyState()
{
    MediaPlayer::ReadyState newReadyState = currentReadyState();

    if (newReadyState != m_readyState) {
        ALWAYS_LOG(LOGIDENTIFIER, "updated to ", (int)newReadyState);
        setReadyState(newReadyState);
    }
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
            if (m_player) {
                m_player->timeChanged();
                m_player->characteristicChanged();
            }
        }
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::updateRenderingMode()
{
    if (!updateDisplayMode())
        return;

    scheduleDeferredTask([this] {
        m_videoTransform = { };
        if (m_player)
            m_player->renderingModeChanged();
    });

}

void MediaPlayerPrivateMediaStreamAVFObjC::characteristicsChanged()
{
    bool sizeChanged = false;

    FloatSize intrinsicSize = m_mediaStreamPrivate->intrinsicSize();
    if (intrinsicSize.height() != m_intrinsicSize.height() || intrinsicSize.width() != m_intrinsicSize.width()) {
        m_intrinsicSize = intrinsicSize;
        sizeChanged = true;
        if (m_playbackState == PlaybackState::None)
            m_playbackState = PlaybackState::Paused;
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

void MediaPlayerPrivateMediaStreamAVFObjC::readyStateChanged(MediaStreamTrackPrivate&)
{
    scheduleDeferredTask([this] {
        updateReadyState();
    });
}

bool MediaPlayerPrivateMediaStreamAVFObjC::supportsPictureInPicture() const
{
#if PLATFORM(IOS_FAMILY)
    for (const auto& track : m_videoTrackMap.values()) {
        if (track->streamTrack().isCaptureTrack())
            return false;
    }
#endif
    
    return true;
}

RetainPtr<PlatformLayer> MediaPlayerPrivateMediaStreamAVFObjC::createVideoFullscreenLayer()
{
    return adoptNS([[CALayer alloc] init]);
}

void MediaPlayerPrivateMediaStreamAVFObjC::setVideoFullscreenLayer(PlatformLayer* videoFullscreenLayer, WTF::Function<void()>&& completionHandler)
{
    updateCurrentFrameImage();
    m_videoLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler), m_imagePainter.cgImage);
}

void MediaPlayerPrivateMediaStreamAVFObjC::setVideoFullscreenFrame(FloatRect frame)
{
    m_videoLayerManager->setVideoFullscreenFrame(frame);
}

enum class TrackState {
    Add,
    Remove,
    Configure
};

template <typename RefT>
void updateTracksOfType(HashMap<String, RefT>& trackMap, RealtimeMediaSource::Type trackType, MediaStreamTrackPrivateVector& currentTracks, RefT (*itemFactory)(MediaStreamTrackPrivate&), const Function<void(std::reference_wrapper<typename std::remove_pointer<typename RefT::PtrTraits::StorageType>::type>, int, TrackState)>& configureTrack)
{
    Vector<RefT> removedTracks;
    Vector<RefT> addedTracks;
    Vector<Ref<MediaStreamTrackPrivate>> addedPrivateTracks;

    for (const auto& track : currentTracks) {
        if (track->type() != trackType)
            continue;

        if (!trackMap.contains(track->id()))
            addedPrivateTracks.append(*track);
    }

    for (const auto& track : trackMap.values()) {
        auto& streamTrack = track->streamTrack();
        if (currentTracks.contains(&streamTrack))
            continue;

        removedTracks.append(track);
    }
    for (auto& track : removedTracks)
        trackMap.remove(track->streamTrack().id());

    for (auto& track : addedPrivateTracks) {
        RefT newTrack = itemFactory(track.get());
        trackMap.add(track->id(), newTrack.copyRef());
        addedTracks.append(WTFMove(newTrack));
    }

    int index = 0;
    for (auto& track : removedTracks)
        configureTrack(track.get(), index++, TrackState::Remove);

    index = 0;
    for (auto& track : addedTracks)
        configureTrack(track.get(), index++, TrackState::Add);

    index = 0;
    for (const auto& track : trackMap.values())
        configureTrack(track.get(), index++, TrackState::Configure);
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
        if (auto* activeVideoTrack = this->activeVideoTrack()) {
            for (const auto& track : m_videoTrackMap.values()) {
                if (&track->streamTrack() == activeVideoTrack) {
                    m_activeVideoTrack = track.ptr();
                    if (track->selected())
                        hideVideoLayer = false;
                    break;
                }
            }
        }

        if (oldVideoTrack != m_activeVideoTrack) {
            m_imagePainter.reset();
            if (m_displayMode == None)
                m_waitingForFirstImage = true;
        }
        ensureLayers();
        if (m_sampleBufferDisplayLayer) {
            if (!m_activeVideoTrack)
                m_sampleBufferDisplayLayer->clearEnqueuedSamples();
            m_sampleBufferDisplayLayer->updateDisplayMode(hideVideoLayer || m_displayMode < PausedImage, hideRootLayer());
        }

        m_pendingSelectedTrackCheck = false;
        updateDisplayMode();

        if (oldVideoTrack != m_activeVideoTrack) {
            if (oldVideoTrack)
                oldVideoTrack->streamTrack().source().removeVideoSampleObserver(*this);
            if (m_activeVideoTrack) {
                if (m_sampleBufferDisplayLayer && m_activeVideoTrack->streamTrack().source().isCaptureSource())
                    m_sampleBufferDisplayLayer->setRenderPolicy(SampleBufferDisplayLayer::RenderPolicy::Immediately);
                m_activeVideoTrack->streamTrack().source().addVideoSampleObserver(*this);
            }
        }
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::updateTracks()
{
    MediaStreamTrackPrivateVector currentTracks = m_mediaStreamPrivate->tracks();

    auto setAudioTrackState = [this](AudioTrackPrivateMediaStream& track, int index, TrackState state)
    {
        switch (state) {
        case TrackState::Remove:
            track.streamTrack().removeObserver(*this);
            track.clear();
            m_player->removeAudioTrack(track);
            break;
        case TrackState::Add:
            track.streamTrack().addObserver(*this);
            m_player->addAudioTrack(track);
            break;
        case TrackState::Configure:
            track.setTrackIndex(index);
            track.setVolume(m_volume);
            track.setMuted(m_muted);
            track.setEnabled(track.streamTrack().enabled() && !track.streamTrack().muted());
            if (playing())
                track.play();
            break;
        }
    };
    updateTracksOfType(m_audioTrackMap, RealtimeMediaSource::Type::Audio, currentTracks, &AudioTrackPrivateMediaStream::create, WTFMove(setAudioTrackState));

    if (!m_player->isVideoPlayer())
        return;

    auto setVideoTrackState = [this](VideoTrackPrivateMediaStream& track, int index, TrackState state)
    {
        switch (state) {
        case TrackState::Remove:
            track.streamTrack().removeObserver(*this);
            m_player->removeVideoTrack(track);
            checkSelectedVideoTrack();
            break;
        case TrackState::Add:
            track.streamTrack().addObserver(*this);
            m_player->addVideoTrack(track);
            break;
        case TrackState::Configure:
            track.setTrackIndex(index);
            bool selected = &track.streamTrack() == activeVideoTrack();
            track.setSelected(selected);
            checkSelectedVideoTrack();
            break;
        }
    };
    updateTracksOfType(m_videoTrackMap, RealtimeMediaSource::Type::Video, currentTracks, &VideoTrackPrivateMediaStream::create, WTFMove(setVideoTrackState));
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateMediaStreamAVFObjC::seekable() const
{
    return makeUnique<PlatformTimeRanges>();
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateMediaStreamAVFObjC::buffered() const
{
    return makeUnique<PlatformTimeRanges>();
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
        m_imagePainter.pixelBufferConformer = makeUnique<PixelBufferConformerCV>((__bridge CFDictionaryRef)@{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) });

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
    if (m_displayMode == PaintItBlack) {
        context.fillRect(IntRect(IntPoint(), IntSize(destRect.width(), destRect.height())), Color::black);
        return;
    }

    if (!m_imagePainter.cgImage || !m_imagePainter.mediaSample)
        return;

    auto image = m_imagePainter.cgImage.get();
    FloatRect imageRect(0, 0, CGImageGetWidth(image), CGImageGetHeight(image));
    if (!m_videoTransform)
        m_videoTransform = videoTransformationMatrix(*m_imagePainter.mediaSample);
    AffineTransform videoTransform = *m_videoTransform;
    FloatRect transformedDestRect = videoTransform.inverse().valueOr(AffineTransform()).mapRect(destRect);
    context.concatCTM(videoTransform);
    context.drawNativeImage(image, imageRect.size(), transformedDestRect, imageRect);
}

void MediaPlayerPrivateMediaStreamAVFObjC::acceleratedRenderingStateChanged()
{
    if (m_player->renderingCanBeAccelerated())
        ensureLayers();
    else
        destroyLayers();
}

String MediaPlayerPrivateMediaStreamAVFObjC::engineDescription() const
{
    static NeverDestroyed<String> description(MAKE_STATIC_STRING_IMPL("AVFoundation MediaStream Engine"));
    return description;
}

void MediaPlayerPrivateMediaStreamAVFObjC::setReadyState(MediaPlayer::ReadyState readyState)
{
    if (m_readyState == readyState)
        return;

    if (readyState != MediaPlayer::ReadyState::HaveNothing)
        m_haveSeenMetadata = true;
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

void MediaPlayerPrivateMediaStreamAVFObjC::setBufferingPolicy(MediaPlayer::BufferingPolicy policy)
{
    if (policy != MediaPlayer::BufferingPolicy::Default && m_sampleBufferDisplayLayer)
        m_sampleBufferDisplayLayer->flushAndRemoveImage();
}

void MediaPlayerPrivateMediaStreamAVFObjC::scheduleDeferredTask(Function<void ()>&& function)
{
    ASSERT(function);
    callOnMainThread([weakThis = makeWeakPtr(*this), function = WTFMove(function)] {
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

void MediaPlayerPrivateMediaStreamAVFObjC::rootLayerBoundsDidChange()
{
    m_shouldUpdateDisplayLayer = true;
}

WTFLogChannel& MediaPlayerPrivateMediaStreamAVFObjC::logChannel() const
{
    return LogMedia;
}

}

#endif
