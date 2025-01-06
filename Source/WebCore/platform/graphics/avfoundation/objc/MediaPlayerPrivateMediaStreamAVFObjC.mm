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

#import "AudioMediaStreamTrackRenderer.h"
#import "AudioTrackPrivateMediaStream.h"
#import "GraphicsContextCG.h"
#import "LocalSampleBufferDisplayLayer.h"
#import "Logging.h"
#import "MediaPlayer.h"
#import "MediaSessionManagerCocoa.h"
#import "MediaStreamPrivate.h"
#import "PixelBufferConformerCV.h"
#import "VideoFrame.h"
#import "VideoFrameMetadata.h"
#import "VideoLayerManagerObjC.h"
#import "VideoTrackPrivateMediaStream.h"
#import <objc_runtime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/HexNumber.h>
#import <wtf/Lock.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

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
            ensureOnMainThread([retainedSelf = retainPtr(self)] {
                if (retainedSelf->_callback)
                    retainedSelf->_callback();
            });
        }
    }

}
@end

namespace WebCore {

#pragma mark -
#pragma mark MediaPlayerPrivateMediaStreamAVFObjC

MediaPlayerPrivateMediaStreamAVFObjC::NativeImageCreator MediaPlayerPrivateMediaStreamAVFObjC::m_nativeImageCreator = nullptr;
void MediaPlayerPrivateMediaStreamAVFObjC::setNativeImageCreator(NativeImageCreator&& callback)
{
    m_nativeImageCreator = WTFMove(callback);
}

MediaPlayerPrivateMediaStreamAVFObjC::MediaPlayerPrivateMediaStreamAVFObjC(MediaPlayer* player)
    : m_player(player)
    , m_videoRotation { VideoFrameRotation::None }
    , m_logger(player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
    , m_videoLayerManager(makeUnique<VideoLayerManagerObjC>(m_logger, m_logIdentifier))
{
    INFO_LOG(LOGIDENTIFIER);
    // MediaPlayerPrivateMediaStreamAVFObjC::processNewVideoFrame expects a weak pointer to be created in the constructor.
    m_boundsChangeListener = adoptNS([[WebRootSampleBufferBoundsChangeListener alloc] initWithCallback:[this, weakThis = WeakPtr { *this }] {
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
        m_activeVideoTrack->streamTrack().source().removeVideoFrameObserver(*this);

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
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MediaPlayerFactoryMediaStreamAVFObjC);
public:
    MediaPlayerFactoryMediaStreamAVFObjC()
    {
        MediaSessionManagerCocoa::ensureCodecsRegistered();
    }

private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMediaStream; };

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer* player) const final
    {
        return adoptRef(*new MediaPlayerPrivateMediaStreamAVFObjC(player));
    }

    void getSupportedTypes(HashSet<String>& types) const final
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
    return PAL::isAVFoundationFrameworkAvailable() && PAL::isCoreMediaFrameworkAvailable() && PAL::getAVSampleBufferDisplayLayerClass();
}

void MediaPlayerPrivateMediaStreamAVFObjC::getSupportedTypes(HashSet<String>& types)
{
    // FIXME: Is it really correct to list no supported types?
    types.clear();
}

MediaPlayer::SupportsType MediaPlayerPrivateMediaStreamAVFObjC::supportsType(const MediaEngineSupportParameters& parameters)
{
    return (parameters.isMediaStream && !parameters.requiresRemotePlayback) ? MediaPlayer::SupportsType::IsSupported : MediaPlayer::SupportsType::IsNotSupported;
}

#pragma mark -
#pragma mark AVSampleBuffer Methods

void MediaPlayerPrivateMediaStreamAVFObjC::videoFrameAvailable(VideoFrame& videoFrame, VideoFrameTimeMetadata metadata)
{
    auto presentationTime = MonotonicTime::now().secondsSinceEpoch();
    processNewVideoFrame(videoFrame, metadata, presentationTime);
    enqueueVideoFrame(videoFrame);
}

void MediaPlayerPrivateMediaStreamAVFObjC::enqueueVideoFrame(VideoFrame& videoFrame)
{
    if (!m_isPageVisible || !m_isVisibleInViewPort)
        return;

    if (!m_sampleBufferDisplayLayerLock.tryLock())
        return;
    Locker locker { AdoptLock, m_sampleBufferDisplayLayerLock };

    if (!m_canEnqueueDisplayLayer || !m_sampleBufferDisplayLayer || m_sampleBufferDisplayLayer->didFail())
        return;

    if (!m_isActiveVideoTrackEnabled) {
        if (!m_hasEnqueuedBlackFrame) {
            m_hasEnqueuedBlackFrame = true;
            m_sampleBufferDisplayLayer->enqueueBlackFrameFrom(videoFrame);
        }
        return;
    }

    m_hasEnqueuedBlackFrame = false;
    m_sampleBufferDisplayLayer->enqueueVideoFrame(videoFrame);
}

void MediaPlayerPrivateMediaStreamAVFObjC::reenqueueCurrentVideoFrameIfNeeded()
{
    if (!m_currentVideoFrameLock.tryLock())
        return;
    Locker locker { AdoptLock, m_currentVideoFrameLock };

    if (!m_currentVideoFrame && !m_imagePainter.videoFrame)
        return;

    enqueueVideoFrame(m_currentVideoFrame ? *m_currentVideoFrame : *m_imagePainter.videoFrame);
}

void MediaPlayerPrivateMediaStreamAVFObjC::processNewVideoFrame(VideoFrame& videoFrame, VideoFrameTimeMetadata metadata, Seconds presentationTime)
{
    if (!isMainThread()) {
        {
            Locker locker { m_currentVideoFrameLock };
            m_currentVideoFrame = &videoFrame;
        }
        scheduleDeferredTask([this, metadata, presentationTime]() mutable {
            RefPtr<VideoFrame> videoFrame;
            {
                Locker locker { m_currentVideoFrameLock };
                videoFrame = WTFMove(m_currentVideoFrame);
            }
            if (!videoFrame)
                return;

            processNewVideoFrame(*videoFrame, metadata, presentationTime);
        });
        return;
    }

    if (!m_activeVideoTrack)
        return;

    if (!m_imagePainter.videoFrame || m_displayMode != PausedImage) {
        m_imagePainter.videoFrame = &videoFrame;
        m_imagePainter.cgImage = nullptr;
        if (m_readyState < MediaPlayer::ReadyState::HaveEnoughData)
            updateReadyState();
    }

    m_presentationTime = presentationTime;
    auto videoFrameSize = videoFrame.presentationSize();
    if (videoFrame.rotation() == VideoFrame::Rotation::Left || videoFrame.rotation() == VideoFrame::Rotation::Right)
        videoFrameSize = { videoFrameSize.height(), videoFrameSize.width() };
    m_sampleMetadata = metadata;
    ++m_sampleCount;

    if (!m_intrinsicSize.isEmpty()) {
        if (videoFrameSize.width() != m_intrinsicSize.width() || videoFrameSize.height() != m_intrinsicSize.height()) {
            m_intrinsicSize = videoFrameSize;
            scheduleTaskForCharacteristicsChanged(SizeChanged::Yes);
        }
    }

    if (m_displayMode != LivePreview && !m_waitingForFirstImage)
        return;

    if (!m_hasEverEnqueuedVideoFrame) {
        m_hasEverEnqueuedVideoFrame = true;
        if (auto player = m_player.get())
            player->firstVideoFrameAvailable();
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

void MediaPlayerPrivateMediaStreamAVFObjC::sampleBufferDisplayLayerStatusDidFail()
{
    destroyLayers();
    updateLayersAsNeeded();
}

#if PLATFORM(IOS_FAMILY)
bool MediaPlayerPrivateMediaStreamAVFObjC::canShowWhileLocked() const
{
    auto player = m_player.get();
    return player && player->canShowWhileLocked();
}
#endif

void MediaPlayerPrivateMediaStreamAVFObjC::applicationDidBecomeActive()
{
    if (m_sampleBufferDisplayLayer && m_sampleBufferDisplayLayer->didFail()) {
        flushRenderers();
        if (m_imagePainter.videoFrame)
            enqueueVideoFrame(*m_imagePainter.videoFrame);
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

    auto player = m_player.get();
    if (!player)
        return;

    auto size = IntSize { player->videoLayerSize() };
    if (size.isEmpty())
        size = player->presentationSize();
    if (size.isEmpty() || m_intrinsicSize.isEmpty())
        return;

    m_canEnqueueDisplayLayer = false;
    m_sampleBufferDisplayLayer = SampleBufferDisplayLayer::create(*this);
    ERROR_LOG_IF(!m_sampleBufferDisplayLayer, LOGIDENTIFIER, "Creating the SampleBufferDisplayLayer failed.");
    if (!m_sampleBufferDisplayLayer)
        return;

    if (!playing())
        m_sampleBufferDisplayLayer->pause();

    if (activeVideoTrack->source().isCaptureSource())
        m_sampleBufferDisplayLayer->setRenderPolicy(SampleBufferDisplayLayer::RenderPolicy::Immediately);

    m_sampleBufferDisplayLayer->initialize(hideRootLayer(), size, m_shouldMaintainAspectRatio, [weakThis = WeakPtr { *this }, weakLayer = ThreadSafeWeakPtr { *m_sampleBufferDisplayLayer }, size](auto didSucceed) {
        auto layer = weakLayer.get();
        if (weakThis && layer && layer.get() == weakThis->m_sampleBufferDisplayLayer.get())
            weakThis->layersAreInitialized(size, didSucceed);
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::setPresentationSize(const IntSize& newSize)
{
    if (!m_sampleBufferDisplayLayer && !newSize.isEmpty())
        updateLayersAsNeeded();
}

void MediaPlayerPrivateMediaStreamAVFObjC::layersAreInitialized(IntSize size, bool didSucceed)
{
    if (!didSucceed) {
        ERROR_LOG(LOGIDENTIFIER, "Initializing the SampleBufferDisplayLayer failed.");
        m_sampleBufferDisplayLayer = nullptr;
        updateLayersAsNeeded();
        return;
    }

    scheduleRenderingModeChanged();

    m_sampleBufferDisplayLayer->setLogIdentifier(makeString(hex(logIdentifier())));
    if (m_storedBounds)
        m_sampleBufferDisplayLayer->updateBoundsAndPosition(*m_storedBounds);

    m_sampleBufferDisplayLayer->updateDisplayMode(m_displayMode < PausedImage, hideRootLayer());

    m_videoLayerManager->setVideoLayer(m_sampleBufferDisplayLayer->rootLayer(), size);

    [m_boundsChangeListener begin:m_sampleBufferDisplayLayer->rootLayer()];

    m_canEnqueueDisplayLayer = true;

    if (m_layerHostingContextIDCallback)
        m_layerHostingContextIDCallback(m_sampleBufferDisplayLayer->hostingContextID());
}

void MediaPlayerPrivateMediaStreamAVFObjC::destroyLayers()
{
    Locker locker { m_sampleBufferDisplayLayerLock };

    m_canEnqueueDisplayLayer = false;
    if (m_sampleBufferDisplayLayer)
        m_sampleBufferDisplayLayer = nullptr;

    scheduleRenderingModeChanged();
    
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
void MediaPlayerPrivateMediaStreamAVFObjC::load(const URL&, const ContentType&, MediaSourcePrivateClient&)
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

    m_intrinsicSize = { };

    m_mediaStreamPrivate = &stream;
    m_mediaStreamPrivate->addObserver(*this);
    m_ended = !m_mediaStreamPrivate->active();

    updateTracks();

    scheduleDeferredTask([this] {
        setNetworkState(MediaPlayer::NetworkState::Idle);
        updateReadyState();
    });
}

MediaStreamTrackPrivate* MediaPlayerPrivateMediaStreamAVFObjC::activeVideoTrack() const
{
    auto player = m_player.get();
    return (m_mediaStreamPrivate && player && player->isVideoPlayer()) ? m_mediaStreamPrivate->activeVideoTrack() : nullptr;
}

bool MediaPlayerPrivateMediaStreamAVFObjC::didPassCORSAccessCheck() const
{
    return true;
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
    if (m_startTime.isInvalid())
        m_startTime = MediaTime::createWithDouble(MonotonicTime::now().secondsSinceEpoch().value());

    for (const auto& track : m_audioTrackMap.values())
        track->play();

    if (m_sampleBufferDisplayLayer)
        m_sampleBufferDisplayLayer->play();
    updateDisplayMode();
    reenqueueCurrentVideoFrameIfNeeded();

    scheduleDeferredTask([this] {
        updateReadyState();
        if (auto player = m_player.get())
            player->rateChanged();
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::pause()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (!metaDataAvailable() || !playing() || m_ended)
        return;

    m_pausedTime = currentTime();
    m_playbackState = PlaybackState::Paused;

    for (const auto& track : m_audioTrackMap.values())
        track->pause();

    if (m_sampleBufferDisplayLayer)
        m_sampleBufferDisplayLayer->pause();
    updateDisplayMode();
    flushRenderers();

    scheduleDeferredTask([this] {
        if (auto player = m_player.get())
            player->rateChanged();
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
    return !m_videoTrackMap.isEmpty();
}

bool MediaPlayerPrivateMediaStreamAVFObjC::hasAudio() const
{
    return !m_audioTrackMap.isEmpty();
}

void MediaPlayerPrivateMediaStreamAVFObjC::setPageIsVisible(bool isVisible)
{
    if (m_isPageVisible == isVisible)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, isVisible);
    m_isPageVisible = isVisible;
    flushRenderers();
    reenqueueCurrentVideoFrameIfNeeded();
}

void MediaPlayerPrivateMediaStreamAVFObjC::setVisibleForCanvas(bool)
{
}

void MediaPlayerPrivateMediaStreamAVFObjC::setVisibleInViewport(bool isVisible)
{
    m_isVisibleInViewPort = isVisible;
}

MediaTime MediaPlayerPrivateMediaStreamAVFObjC::duration() const
{
    return MediaTime::positiveInfiniteTime();
}

MediaTime MediaPlayerPrivateMediaStreamAVFObjC::currentTime() const
{
    if (paused())
        return m_pausedTime;

    return MediaTime::createWithDouble(MonotonicTime::now().secondsSinceEpoch().value()) - m_startTime;
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

    bool waitingForImage = activeVideoTrack() && !m_imagePainter.videoFrame;
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
            if (auto player = m_player.get()) {
                player->timeChanged();
                player->characteristicChanged();
            }
        }
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::updateRenderingMode()
{
    if (updateDisplayMode())
        scheduleRenderingModeChanged();
}

void MediaPlayerPrivateMediaStreamAVFObjC::scheduleRenderingModeChanged()
{
    scheduleDeferredTask([this] {
        if (auto player = m_player.get())
            player->renderingModeChanged();
        reenqueueCurrentVideoFrameIfNeeded();
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::characteristicsChanged()
{
    SizeChanged sizeChanged = SizeChanged::No;

    IntSize intrinsicSize = m_mediaStreamPrivate->intrinsicSize();
    if (intrinsicSize.isEmpty() || m_intrinsicSize.isEmpty()) {
        if (intrinsicSize.height() != m_intrinsicSize.height() || intrinsicSize.width() != m_intrinsicSize.width()) {
            m_intrinsicSize = intrinsicSize;
            sizeChanged = SizeChanged::Yes;
            if (m_playbackState == PlaybackState::None)
                m_playbackState = PlaybackState::Paused;
        }
    }

    updateTracks();
    updateDisplayMode();

    scheduleTaskForCharacteristicsChanged(sizeChanged);
}

void MediaPlayerPrivateMediaStreamAVFObjC::scheduleTaskForCharacteristicsChanged(SizeChanged sizeChanged)
{
    scheduleDeferredTask([this, sizeChanged] {
        updateReadyState();

        auto player = m_player.get();
        if (!player)
            return;

        player->characteristicChanged();
        if (sizeChanged == SizeChanged::Yes)
            player->sizeChanged();

        if (!m_sampleBufferDisplayLayer && !m_intrinsicSize.isEmpty())
            updateLayersAsNeeded();
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

#if ENABLE(VIDEO_PRESENTATION_MODE)
RetainPtr<PlatformLayer> MediaPlayerPrivateMediaStreamAVFObjC::createVideoFullscreenLayer()
{
    return adoptNS([[CALayer alloc] init]);
}

void MediaPlayerPrivateMediaStreamAVFObjC::setVideoFullscreenLayer(PlatformLayer* videoFullscreenLayer, WTF::Function<void()>&& completionHandler)
{
    updateCurrentFrameImage();
    m_videoLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler), m_imagePainter.cgImage ? m_imagePainter.cgImage->platformImage() : nullptr);
}

void MediaPlayerPrivateMediaStreamAVFObjC::setVideoFullscreenFrame(FloatRect frame)
{
    m_videoLayerManager->setVideoFullscreenFrame(frame);
}
#endif

enum class TrackState {
    Add,
    Remove,
    Configure
};

enum class TrackKind {
    Audio,
    Video
};

template <typename RefT>
void updateTracksOfKind(MemoryCompactRobinHoodHashMap<String, RefT>& trackMap, TrackKind trackKind, MediaStreamTrackPrivateVector& currentTracks, RefT (*itemFactory)(MediaStreamTrackPrivate&), const Function<void(std::reference_wrapper<typename std::remove_pointer<typename RefT::PtrTraits::StorageType>::type>, int, TrackState)>& configureTrack)
{
    Vector<RefT> removedTracks;
    Vector<RefT> addedTracks;
    Vector<Ref<MediaStreamTrackPrivate>> addedPrivateTracks;

    bool wantsVideo = trackKind == TrackKind::Video;
    for (const auto& track : currentTracks) {
        if (wantsVideo != track->isVideo())
            continue;

        if (!trackMap.contains(track->id()))
            addedPrivateTracks.append(track);
    }

    for (const auto& track : trackMap.values()) {
        auto& streamTrack = track->streamTrack();
        if (currentTracks.containsIf([&streamTrack](auto& track) { return track.ptr() == &streamTrack; }))
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

    updateLayersAsNeeded();

    if (m_sampleBufferDisplayLayer) {
        if (!m_activeVideoTrack)
            m_sampleBufferDisplayLayer->clearVideoFrames();
        m_sampleBufferDisplayLayer->updateDisplayMode(hideVideoLayer || m_displayMode < PausedImage, hideRootLayer());
    }

    updateDisplayMode();

    if (oldVideoTrack != m_activeVideoTrack) {
        if (oldVideoTrack)
            oldVideoTrack->streamTrack().source().removeVideoFrameObserver(*this);
        m_isActiveVideoTrackEnabled = m_activeVideoTrack ? m_activeVideoTrack->streamTrack().enabled() : true;
        if (m_activeVideoTrack) {
            if (m_sampleBufferDisplayLayer && m_activeVideoTrack->streamTrack().source().isCaptureSource())
                m_sampleBufferDisplayLayer->setRenderPolicy(SampleBufferDisplayLayer::RenderPolicy::Immediately);
            m_activeVideoTrack->streamTrack().source().addVideoFrameObserver(*this);
            ALWAYS_LOG(LOGIDENTIFIER, "observing video source ", m_activeVideoTrack->streamTrack().logIdentifier());
        }
    } else
        m_isActiveVideoTrackEnabled = m_activeVideoTrack ? m_activeVideoTrack->streamTrack().enabled() : true;
}

void MediaPlayerPrivateMediaStreamAVFObjC::updateTracks()
{
    MediaStreamTrackPrivateVector currentTracks = m_mediaStreamPrivate->tracks();

    auto player = m_player.get();
    if (!player)
        return;

    auto deviceId = player->audioOutputDeviceIdOverride();
    auto setAudioTrackState = [this, player, &deviceId](AudioTrackPrivateMediaStream& track, int index, TrackState state)
    {
        switch (state) {
        case TrackState::Remove:
            track.streamTrack().removeObserver(*this);
            track.clear();
            player->removeAudioTrack(track);
            break;
        case TrackState::Add:
            track.streamTrack().addObserver(*this);
            player->addAudioTrack(track);
            break;
        case TrackState::Configure:
            track.setTrackIndex(index);
            track.setVolume(m_volume);
            track.setMuted(m_muted);
            track.setEnabled(track.streamTrack().enabled() && !track.streamTrack().muted());
            if (!deviceId.isNull())
                track.setAudioOutputDevice(deviceId);

            if (playing())
                track.play();
            break;
        }
    };
    updateTracksOfKind(m_audioTrackMap, TrackKind::Audio, currentTracks, &AudioTrackPrivateMediaStream::create, WTFMove(setAudioTrackState));

    if (!player->isVideoPlayer())
        return;

    auto setVideoTrackState = [this, player](VideoTrackPrivateMediaStream& track, int index, TrackState state)
    {
        switch (state) {
        case TrackState::Remove:
            track.streamTrack().removeObserver(*this);
            player->removeVideoTrack(track);
            checkSelectedVideoTrack();
            break;
        case TrackState::Add:
            track.streamTrack().addObserver(*this);
            player->addVideoTrack(track);
            break;
        case TrackState::Configure:
            track.setTrackIndex(index);
            bool selected = &track.streamTrack() == activeVideoTrack();
            track.setSelected(selected);
            checkSelectedVideoTrack();
            break;
        }
    };
    updateTracksOfKind(m_videoTrackMap, TrackKind::Video, currentTracks, &VideoTrackPrivateMediaStream::create, WTFMove(setVideoTrackState));
}

const PlatformTimeRanges& MediaPlayerPrivateMediaStreamAVFObjC::seekable() const
{
    return PlatformTimeRanges::emptyRanges();
}

const PlatformTimeRanges& MediaPlayerPrivateMediaStreamAVFObjC::buffered() const
{
    return PlatformTimeRanges::emptyRanges();
}

void MediaPlayerPrivateMediaStreamAVFObjC::paint(GraphicsContext& context, const FloatRect& rect)
{
    paintCurrentFrameInContext(context, rect);
}

void MediaPlayerPrivateMediaStreamAVFObjC::updateCurrentFrameImage()
{
    if (m_imagePainter.cgImage || !m_imagePainter.videoFrame)
        return;

    if (m_nativeImageCreator) {
        m_imagePainter.cgImage = m_nativeImageCreator(*m_imagePainter.videoFrame);
        return;
    }

    if (!m_imagePainter.pixelBufferConformer)
        m_imagePainter.pixelBufferConformer = makeUnique<PixelBufferConformerCV>((__bridge CFDictionaryRef)@{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) });

    ASSERT(m_imagePainter.pixelBufferConformer);
    if (!m_imagePainter.pixelBufferConformer)
        return;

    if (auto pixelBuffer = m_imagePainter.videoFrame->pixelBuffer())
        m_imagePainter.cgImage = NativeImage::create(m_imagePainter.pixelBufferConformer->createImageFromPixelBuffer(pixelBuffer));
}

static inline CGAffineTransform videoTransformationMatrix(VideoFrame& videoFrame)
{
    auto size = videoFrame.presentationSize();
    size_t width = static_cast<size_t>(size.width());
    size_t height = static_cast<size_t>(size.height());
    if (!width || !height)
        return CGAffineTransformIdentity;

    auto videoTransform = CGAffineTransformMakeRotation(static_cast<int>(videoFrame.rotation()) * M_PI / 180);
    if (videoFrame.isMirrored())
        videoTransform = CGAffineTransformScale(videoTransform, -1, 1);

    return videoTransform;
}

void MediaPlayerPrivateMediaStreamAVFObjC::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& destRect)
{
    if (m_displayMode == None || !metaDataAvailable() || context.paintingDisabled())
        return;

    if (m_displayMode != PaintItBlack && m_imagePainter.videoFrame)
        updateCurrentFrameImage();

    GraphicsContextStateSaver stateSaver(context);
    if (m_displayMode == PaintItBlack) {
        context.fillRect(IntRect(IntPoint(), IntSize(destRect.width(), destRect.height())), Color::black);
        return;
    }

    if (!m_imagePainter.cgImage || !m_imagePainter.videoFrame)
        return;

    auto& image = m_imagePainter.cgImage;
    FloatRect imageRect { FloatPoint::zero(), image->size() };
    AffineTransform videoTransform = videoTransformationMatrix(*m_imagePainter.videoFrame);
    FloatRect transformedDestRect = valueOrDefault(videoTransform.inverse()).mapRect(destRect);
    context.concatCTM(videoTransform);
    context.drawNativeImage(*image, transformedDestRect, imageRect);
}

RefPtr<VideoFrame> MediaPlayerPrivateMediaStreamAVFObjC::videoFrameForCurrentTime()
{
    if (m_displayMode == None || !metaDataAvailable())
        return nullptr;
    if (m_displayMode == PaintItBlack)
        return nullptr;
    return m_imagePainter.videoFrame;
}

DestinationColorSpace MediaPlayerPrivateMediaStreamAVFObjC::colorSpace()
{
    updateCurrentFrameImage();
    return m_imagePainter.cgImage ? m_imagePainter.cgImage->colorSpace() : DestinationColorSpace::SRGB();
}

void MediaPlayerPrivateMediaStreamAVFObjC::updateLayersAsNeeded()
{
    auto player = m_player.get();
    if (player && player->renderingCanBeAccelerated())
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

    if (auto player = m_player.get())
        player->readyStateChanged();
}

void MediaPlayerPrivateMediaStreamAVFObjC::setNetworkState(MediaPlayer::NetworkState networkState)
{
    if (m_networkState == networkState)
        return;

    m_networkState = networkState;
    if (auto player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateMediaStreamAVFObjC::setBufferingPolicy(MediaPlayer::BufferingPolicy policy)
{
    if (policy != MediaPlayer::BufferingPolicy::Default && m_sampleBufferDisplayLayer)
        m_sampleBufferDisplayLayer->flushAndRemoveImage();
}

void MediaPlayerPrivateMediaStreamAVFObjC::audioOutputDeviceChanged()
{
    auto player = m_player.get();
    if (!player)
        return;
    auto deviceId = player->audioOutputDeviceId();
    if (deviceId.isEmpty())
        deviceId = AudioMediaStreamTrackRenderer::defaultDeviceID();
    for (auto& audioTrack : m_audioTrackMap.values())
        audioTrack->setAudioOutputDevice(deviceId);
}

void MediaPlayerPrivateMediaStreamAVFObjC::scheduleDeferredTask(Function<void ()>&& function)
{
    ASSERT(function);
    callOnMainThread([weakThis = WeakPtr { *this }, function = WTFMove(function)] {
        if (!weakThis)
            return;
        auto protectedMediaPlayer = RefPtr { weakThis->m_player.get() };
        function();
    });
}

void MediaPlayerPrivateMediaStreamAVFObjC::CurrentFramePainter::reset()
{
    cgImage = nullptr;
    videoFrame = nullptr;
    pixelBufferConformer = nullptr;
}

void MediaPlayerPrivateMediaStreamAVFObjC::rootLayerBoundsDidChange()
{
    if (!m_isMediaLayerRehosting)
        return;

    Locker locker { m_sampleBufferDisplayLayerLock };
    if (m_sampleBufferDisplayLayer)
        m_sampleBufferDisplayLayer->updateBoundsAndPosition(m_sampleBufferDisplayLayer->rootLayer().bounds);
}

WTFLogChannel& MediaPlayerPrivateMediaStreamAVFObjC::logChannel() const
{
    return LogMedia;
}

std::optional<VideoFrameMetadata> MediaPlayerPrivateMediaStreamAVFObjC::videoFrameMetadata()
{
    if (m_sampleCount == m_lastVideoFrameMetadataSampleCount)
        return { };
    m_lastVideoFrameMetadataSampleCount = m_sampleCount;

    VideoFrameMetadata metadata;
    metadata.width = m_intrinsicSize.width();
    metadata.height = m_intrinsicSize.height();
    metadata.presentedFrames = m_sampleCount;
    metadata.presentationTime = m_presentationTime.seconds();
    metadata.expectedDisplayTime = m_presentationTime.seconds();
    metadata.processingDuration = m_sampleMetadata.processingDuration;
    if (m_sampleMetadata.captureTime)
        metadata.captureTime = m_sampleMetadata.captureTime->seconds();
    if (m_sampleMetadata.receiveTime)
        metadata.receiveTime = m_sampleMetadata.receiveTime->seconds();
    metadata.rtpTimestamp = m_sampleMetadata.rtpTimestamp;

    return metadata;
}

LayerHostingContextID MediaPlayerPrivateMediaStreamAVFObjC::hostingContextID() const
{
    return m_sampleBufferDisplayLayer ? m_sampleBufferDisplayLayer->hostingContextID() : 0;
}

void MediaPlayerPrivateMediaStreamAVFObjC::setVideoLayerSizeFenced(const FloatSize& size, WTF::MachSendRight&& fence)
{
    m_isMediaLayerRehosting = false;

    if (!m_sampleBufferDisplayLayer || size.isEmpty())
        return;

    m_storedBounds = m_sampleBufferDisplayLayer->rootLayer().bounds;
    m_storedBounds->size = size;
    m_sampleBufferDisplayLayer->updateBoundsAndPosition(*m_storedBounds, WTFMove(fence));
}

void MediaPlayerPrivateMediaStreamAVFObjC::requestHostingContextID(LayerHostingContextIDCallback&& callback)
{
    if (auto contextID = hostingContextID()) {
        callback(contextID);
        return;
    }
    m_layerHostingContextIDCallback = WTFMove(callback);
}

void MediaPlayerPrivateMediaStreamAVFObjC::setShouldMaintainAspectRatio(bool shouldMaintainAspectRatio)
{
    m_shouldMaintainAspectRatio = shouldMaintainAspectRatio;
    if (m_sampleBufferDisplayLayer)
        m_sampleBufferDisplayLayer->setShouldMaintainAspectRatio(shouldMaintainAspectRatio);
}

}

#endif
