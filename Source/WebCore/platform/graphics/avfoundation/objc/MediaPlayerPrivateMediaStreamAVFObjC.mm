/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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
#import "AudioTrackPrivateMediaStream.h"
#import "Clock.h"
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

#import "CoreMediaSoftLink.h"

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVSampleBufferDisplayLayer)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVSampleBufferRenderSynchronizer)

namespace WebCore {

#pragma mark -
#pragma mark MediaPlayerPrivateMediaStreamAVFObjC

MediaPlayerPrivateMediaStreamAVFObjC::MediaPlayerPrivateMediaStreamAVFObjC(MediaPlayer* player)
    : m_player(player)
    , m_weakPtrFactory(this)
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
    if (m_mediaStreamPrivate) {
        m_mediaStreamPrivate->removeObserver(*this);

        for (auto& track : m_mediaStreamPrivate->tracks())
            track->removeObserver(*this);
    }

    m_audioTrackMap.clear();
    m_videoTrackMap.clear();

    destroyLayer();
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

void MediaPlayerPrivateMediaStreamAVFObjC::enqueueAudioSampleBufferFromTrack(MediaStreamTrackPrivate&, MediaSample&)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=159836
}

void MediaPlayerPrivateMediaStreamAVFObjC::requestNotificationWhenReadyForMediaData()
{
    [m_sampleBufferDisplayLayer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^ {
        [m_sampleBufferDisplayLayer stopRequestingMediaData];

        while (!m_sampleQueue.isEmpty()) {
            if (![m_sampleBufferDisplayLayer isReadyForMoreMediaData]) {
                requestNotificationWhenReadyForMediaData();
                return;
            }

            auto sample = m_sampleQueue.takeFirst();
            enqueueVideoSampleBuffer(sample.get());
        }
    }];
}

void MediaPlayerPrivateMediaStreamAVFObjC::enqueueVideoSampleBuffer(MediaSample& sample)
{
    if (m_sampleBufferDisplayLayer) {
        if (![m_sampleBufferDisplayLayer isReadyForMoreMediaData]) {
            m_sampleQueue.append(sample);
            requestNotificationWhenReadyForMediaData();
            return;
        }

        [m_sampleBufferDisplayLayer enqueueSampleBuffer:sample.platformSample().sample.cmSampleBuffer];
    }

    m_isFrameDisplayed = true;
    if (!m_hasEverEnqueuedVideoFrame) {
        m_hasEverEnqueuedVideoFrame = true;
        m_player->firstVideoFrameAvailable();
        updatePausedImage();
    }
}

void MediaPlayerPrivateMediaStreamAVFObjC::prepareVideoSampleBufferFromTrack(MediaStreamTrackPrivate& track, MediaSample& sample)
{
    if (&track != m_mediaStreamPrivate->activeVideoTrack() || !shouldEnqueueVideoSampleBuffer())
        return;

    enqueueVideoSampleBuffer(sample);
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
    if (!m_mediaStreamPrivate || haveVideoLayer())
        return;

    CALayer *videoLayer = nil;
    if (m_mediaStreamPrivate->activeVideoTrack()) {
        m_videoPreviewPlayer = m_mediaStreamPrivate->activeVideoTrack()->preview();
        if (m_videoPreviewPlayer)
            videoLayer = m_videoPreviewPlayer->platformLayer();
    }

    if (!videoLayer) {
        m_sampleBufferDisplayLayer = adoptNS([allocAVSampleBufferDisplayLayerInstance() init]);
        videoLayer = m_sampleBufferDisplayLayer.get();
#ifndef NDEBUG
        [m_sampleBufferDisplayLayer setName:@"MediaPlayerPrivateMediaStreamAVFObjC AVSampleBufferDisplayLayer"];
#endif
        m_sampleBufferDisplayLayer.get().backgroundColor = cachedCGColor(Color::black);

#if PLATFORM(MAC)
        m_synchronizer = adoptNS([allocAVSampleBufferRenderSynchronizerInstance() init]);
        [m_synchronizer addRenderer:m_sampleBufferDisplayLayer.get()];
#endif
    }

    renderingModeChanged();
    
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    m_videoFullscreenLayerManager->setVideoLayer(videoLayer, snappedIntRect(m_player->client().mediaPlayerContentBoxRect()).size());
#endif
}

void MediaPlayerPrivateMediaStreamAVFObjC::destroyLayer()
{
    if (!haveVideoLayer())
        return;

    m_videoPreviewPlayer = nullptr;

    if (m_sampleBufferDisplayLayer) {
        [m_sampleBufferDisplayLayer stopRequestingMediaData];
        [m_sampleBufferDisplayLayer flush];
#if PLATFORM(MAC)
        CMTime currentTime = CMTimebaseGetTime([m_synchronizer timebase]);
        [m_synchronizer removeRenderer:m_sampleBufferDisplayLayer.get() atTime:currentTime withCompletionHandler:^(BOOL) {
            // No-op.
        }];
        m_sampleBufferDisplayLayer = nullptr;
#endif
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
    if (!haveVideoLayer() || m_displayMode == None)
        return nullptr;

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    return m_videoFullscreenLayerManager->videoInlineLayer();
#else
    if (m_videoPreviewPlayer)
        return m_videoPreviewPlayer->platformLayer();

    return m_sampleBufferDisplayLayer.get();
#endif
}

MediaPlayerPrivateMediaStreamAVFObjC::DisplayMode MediaPlayerPrivateMediaStreamAVFObjC::currentDisplayMode() const
{
    if (m_ended || m_intrinsicSize.isEmpty() || !metaDataAvailable() || !haveVideoLayer())
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

    m_clock->start();
    m_playing = true;

    if (m_videoPreviewPlayer)
        m_videoPreviewPlayer->play();
#if PLATFORM(MAC)
    else
        [m_synchronizer setRate:1];
#endif

    for (const auto& track : m_audioTrackMap.values()) {
        if (!track->enabled() || !track->streamTrack()->preview())
            continue;

        track->streamTrack()->preview()->play();
    }

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

    m_pausedTime = m_clock->currentTime();
    m_playing = false;

    if (m_videoPreviewPlayer)
        m_videoPreviewPlayer->pause();
#if PLATFORM(MAC)
    else
        [m_synchronizer setRate:0];
#endif

    for (const auto& track : m_audioTrackMap.values()) {
        if (!track->enabled() || !track->streamTrack()->preview())
            continue;

        track->streamTrack()->preview()->pause();
    }

    updateDisplayMode();
    updatePausedImage();
}

bool MediaPlayerPrivateMediaStreamAVFObjC::paused() const
{
    return !m_playing;
}

void MediaPlayerPrivateMediaStreamAVFObjC::internalSetVolume(float volume, bool internal)
{
    if (!internal)
        m_volume = volume;

    if (!metaDataAvailable())
        return;

    for (const auto& track : m_audioTrackMap.values()) {
        if (!track->enabled() || !track->streamTrack()->preview())
            continue;

        track->streamTrack()->preview()->setVolume(volume);
    }
}

void MediaPlayerPrivateMediaStreamAVFObjC::setVolume(float volume)
{
    internalSetVolume(volume, false);
}

void MediaPlayerPrivateMediaStreamAVFObjC::setMuted(bool muted)
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::setMuted(%p)", this);

    if (muted == m_muted)
        return;

    m_muted = muted;
    
    internalSetVolume(muted ? 0 : m_volume, true);
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
    return MediaTime::createWithDouble(m_playing ? m_clock->currentTime() : m_pausedTime);
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

    switch (track.type()) {
    case RealtimeMediaSource::None:
        // Do nothing.
        break;
    case RealtimeMediaSource::Audio:
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=159836
        break;
    case RealtimeMediaSource::Video:
        prepareVideoSampleBufferFromTrack(track, mediaSample);
        m_hasReceivedMedia = true;
        scheduleDeferredTask([this] {
            updateReadyState();
        });
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

template <typename RefT, typename PassRefT>
void updateTracksOfType(HashMap<String, RefT>& trackMap, RealtimeMediaSource::Type trackType, MediaStreamTrackPrivateVector& currentTracks, RefT (*itemFactory)(MediaStreamTrackPrivate&), MediaPlayer* player, void (MediaPlayer::*removedFunction)(PassRefT), void (MediaPlayer::*addedFunction)(PassRefT), std::function<void(RefT, int)> configureCallback, MediaStreamTrackPrivate::Observer* trackObserver)
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
        MediaStreamTrackPrivate* streamTrack = track->streamTrack();
        if (currentTracks.contains(streamTrack))
            continue;

        removedTracks.append(track);
        trackMap.remove(streamTrack->id());
    }

    for (auto& track : addedPrivateTracks) {
        RefT newTrack = itemFactory(*track.get());
        trackMap.add(track->id(), newTrack);
        addedTracks.append(newTrack);
    }

    int index = 0;
    for (const auto& track : trackMap.values())
        configureCallback(track, index++);

    for (auto& track : removedTracks) {
        (player->*removedFunction)(*track);
        track->streamTrack()->removeObserver(*trackObserver);
    }

    for (auto& track : addedTracks) {
        (player->*addedFunction)(*track);
        track->streamTrack()->addObserver(*trackObserver);
    }
}

void MediaPlayerPrivateMediaStreamAVFObjC::updateTracks()
{
    MediaStreamTrackPrivateVector currentTracks = m_mediaStreamPrivate->tracks();

    std::function<void(RefPtr<AudioTrackPrivateMediaStream>, int)> enableAudioTrack = [this](auto track, int index)
    {
        track->setTrackIndex(index);
        track->setEnabled(track->streamTrack()->enabled() && !track->streamTrack()->muted());
    };
    updateTracksOfType(m_audioTrackMap, RealtimeMediaSource::Audio, currentTracks, &AudioTrackPrivateMediaStream::create, m_player, &MediaPlayer::removeAudioTrack, &MediaPlayer::addAudioTrack, enableAudioTrack, (MediaStreamTrackPrivate::Observer*) this);

    std::function<void(RefPtr<VideoTrackPrivateMediaStream>, int)> enableVideoTrack = [this](auto track, int index)
    {
        track->setTrackIndex(index);
        bool selected = track->streamTrack() == m_mediaStreamPrivate->activeVideoTrack();
        track->setSelected(selected);

        if (selected)
            ensureLayer();
    };
    updateTracksOfType(m_videoTrackMap, RealtimeMediaSource::Video, currentTracks, &VideoTrackPrivateMediaStream::create, m_player, &MediaPlayer::removeVideoTrack, &MediaPlayer::addVideoTrack, enableVideoTrack, (MediaStreamTrackPrivate::Observer*) this);
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
