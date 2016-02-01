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

#import "AVAudioCaptureSource.h"
#import "AVVideoCaptureSource.h"
#import "AudioTrackPrivateMediaStream.h"
#import "Clock.h"
#import "GraphicsContext.h"
#import "Logging.h"
#import "MediaStreamPrivate.h"
#import "VideoTrackPrivateMediaStream.h"
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CATransaction.h>
#import <objc_runtime.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>

#pragma mark - Soft Linking

#import "CoreMediaSoftLink.h"

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

namespace WebCore {

#pragma mark -
#pragma mark MediaPlayerPrivateMediaStreamAVFObjC

MediaPlayerPrivateMediaStreamAVFObjC::MediaPlayerPrivateMediaStreamAVFObjC(MediaPlayer* player)
    : m_player(player)
    , m_weakPtrFactory(this)
    , m_clock(Clock::create())
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::MediaPlayerPrivateMediaStreamAVFObjC(%p)", this);
}

MediaPlayerPrivateMediaStreamAVFObjC::~MediaPlayerPrivateMediaStreamAVFObjC()
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::~MediaPlayerPrivateMediaStreamAVFObjC(%p)", this);
    if (m_mediaStreamPrivate)
        m_mediaStreamPrivate->removeObserver(*this);
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
    return AVFoundationLibrary() && isCoreMediaFrameworkAvailable();
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

    m_previewLayer = nullptr;
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
    if (!m_videoBackgroundLayer || m_displayMode == None)
        return nullptr;

    return m_videoBackgroundLayer.get();
}

MediaPlayerPrivateMediaStreamAVFObjC::DisplayMode MediaPlayerPrivateMediaStreamAVFObjC::currentDisplayMode() const
{
    if (m_ended || m_intrinsicSize.isEmpty() || !metaDataAvailable() || !m_videoBackgroundLayer)
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

    if (m_displayMode == None)
        return;

    [CATransaction begin];
    [CATransaction setAnimationDuration:0];
    [CATransaction setDisableActions:YES];

    do {
        if (m_displayMode < LivePreview) {

            if (m_displayMode == PausedImage) {
                if (m_videoBackgroundLayer.get().contents)
                    break;

                RefPtr<Image> image = m_mediaStreamPrivate->currentFrameImage();
                if (!image) {
                    m_displayMode = PaintItBlack;
                    continue;
                }

                m_pausedImage = image->getCGImageRef();
                if (!m_pausedImage) {
                    m_displayMode = PaintItBlack;
                    continue;
                }

                m_videoBackgroundLayer.get().contents = (id)m_pausedImage.get();
                m_videoBackgroundLayer.get().backgroundColor = nil;
            } else {
                m_videoBackgroundLayer.get().contents = nil;
                m_videoBackgroundLayer.get().backgroundColor = cachedCGColor(Color::black);
                m_pausedImage = nullptr;
            }

            m_previewLayer.get().hidden = true;

        } else {

            m_previewLayer.get().hidden = false;
            m_videoBackgroundLayer.get().contents = nil;
            m_pausedImage = nullptr;
        }

        break;
    } while (1);

    [CATransaction commit];
}

void MediaPlayerPrivateMediaStreamAVFObjC::play()
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::play(%p)", this);

    if (!metaDataAvailable() || m_playing || m_ended)
        return;

    m_clock->start();
    m_playing = true;
    m_haveEverPlayed = true;
    updateDisplayMode();
    updateReadyState();
}

void MediaPlayerPrivateMediaStreamAVFObjC::pause()
{
    LOG(Media, "MediaPlayerPrivateMediaStreamAVFObjC::pause(%p)", this);

    if (!metaDataAvailable() || !m_playing || m_ended)
        return;

    m_clock->stop();
    m_playing = false;
    updateDisplayMode();
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

    // FIXME: Set volume once we actually play audio.
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

    if (m_mediaStreamPrivate->active()) {
        if (!m_haveEverPlayed)
            return MediaPlayer::ReadyState::HaveFutureData;
        return MediaPlayer::ReadyState::HaveEnoughData;
    }

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

    if (m_videoBackgroundLayer || !m_player || !m_player->client().mediaPlayerRenderingCanBeAccelerated(m_player))
        return;

    if (!m_mediaStreamPrivate || !m_mediaStreamPrivate->platformLayer())
        return;

    createPreviewLayers();
}

void MediaPlayerPrivateMediaStreamAVFObjC::createPreviewLayers()
{
    if (!m_videoBackgroundLayer) {
        m_videoBackgroundLayer = adoptNS([[CALayer alloc] init]);
        m_videoBackgroundLayer.get().name = @"MediaPlayerPrivateMediaStreamAVFObjC preview background layer";
    }

    if (!m_previewLayer) {
        m_previewLayer = m_mediaStreamPrivate->platformLayer();
        if (m_previewLayer) {
            m_previewLayer.get().contentsGravity = kCAGravityResize;
            m_previewLayer.get().anchorPoint = CGPointZero;
            if (!m_playing)
                m_previewLayer.get().hidden = true;

            [m_videoBackgroundLayer addSublayer:m_previewLayer.get()];
        }
    }

    renderingModeChanged();
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

template <typename RefT, typename PassRefT>
void updateTracksOfType(HashMap<String, RefT>& trackMap, RealtimeMediaSource::Type trackType, MediaStreamTrackPrivateVector& currentTracks, RefT (*itemFactory)(MediaStreamTrackPrivate&), MediaPlayer* player, void (MediaPlayer::*removedFunction)(PassRefT), void (MediaPlayer::*addedFunction)(PassRefT), std::function<void(RefT, int)> configureCallback)
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

    for (auto& track : removedTracks)
        (player->*removedFunction)(track);

    for (auto& track : addedTracks)
        (player->*addedFunction)(track);
}

void MediaPlayerPrivateMediaStreamAVFObjC::updateTracks()
{
    MediaStreamTrackPrivateVector currentTracks = m_mediaStreamPrivate->tracks();
    bool selectedVideoTrackChanged = false;

    std::function<void(RefPtr<AudioTrackPrivateMediaStream>, int)> enableAudioTrack = [this](RefPtr<AudioTrackPrivateMediaStream> track, int index)
    {
        track->setTrackIndex(index);
        track->setEnabled(track->streamTrack()->enabled() && !track->streamTrack()->muted());
    };
    updateTracksOfType(m_audioTrackMap, RealtimeMediaSource::Audio, currentTracks, &AudioTrackPrivateMediaStream::create, m_player, &MediaPlayer::removeAudioTrack, &MediaPlayer::addAudioTrack, enableAudioTrack);

    std::function<void(RefPtr<VideoTrackPrivateMediaStream>, int)> enableVideoTrack = [this, &selectedVideoTrackChanged](RefPtr<VideoTrackPrivateMediaStream> track, int index)
    {
        bool wasSelected = track->selected();
        track->setTrackIndex(index);
        track->setSelected(track->streamTrack() == m_mediaStreamPrivate->activeVideoTrack());
        if (wasSelected != track->selected())
            selectedVideoTrackChanged = true;
    };
    updateTracksOfType(m_videoTrackMap, RealtimeMediaSource::Video, currentTracks, &VideoTrackPrivateMediaStream::create, m_player, &MediaPlayer::removeVideoTrack, &MediaPlayer::addVideoTrack, enableVideoTrack);

    if (selectedVideoTrackChanged) {
        if (m_previewLayer)
            m_previewLayer = nullptr;

        createPreviewLayers();
    }
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
    if (m_displayMode == None || !metaDataAvailable() || context.paintingDisabled() || !m_haveEverPlayed)
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

void MediaPlayerPrivateMediaStreamAVFObjC::scheduleDeferredTask(std::function<void()> function)
{
    ASSERT(function);
    auto weakThis = createWeakPtr();
    callOnMainThread([weakThis, function] {
        if (!weakThis)
            return;

        function();
    });
}

}

#endif
