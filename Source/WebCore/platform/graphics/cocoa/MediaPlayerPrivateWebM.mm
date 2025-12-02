/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "MediaPlayerPrivateWebM.h"

#if ENABLE(COCOA_WEBM_PLAYER)

#import "AudioMediaStreamTrackRenderer.h"
#import "AudioTrackPrivateWebM.h"
#import "AudioVideoRendererAVFObjC.h"
#import "FloatSize.h"
#import "GraphicsContext.h"
#import "GraphicsContextStateSaver.h"
#import "Logging.h"
#import "MediaPlaybackTarget.h"
#import "MediaPlayer.h"
#import "MediaSampleAVFObjC.h"
#import "MediaStrategy.h"
#import "NativeImage.h"
#import "NotImplemented.h"
#import "PixelBufferConformerCV.h"
#import "PlatformDynamicRangeLimitCocoa.h"
#import "PlatformMediaResourceLoader.h"
#import "PlatformStrategies.h"
#import "ResourceError.h"
#import "ResourceRequest.h"
#import "ResourceResponse.h"
#import "SampleMap.h"
#import "SecurityOrigin.h"
#import "TrackBuffer.h"
#import "VP9UtilitiesCocoa.h"
#import "VideoFrameCV.h"
#import "VideoTrackPrivateWebM.h"
#import "WebMResourceClient.h"
#import <AVFoundation/AVFoundation.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/MainThread.h>
#import <wtf/NativePromise.h>
#import <wtf/SoftLinking.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakPtr.h>
#import <wtf/WorkQueue.h>

#pragma mark - Soft Linking
#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

#pragma mark -

namespace WebCore {

using TrackType = TrackInfo::TrackType;

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaPlayerPrivateWebM);

static const MediaTime discontinuityTolerance = MediaTime(1, 1);

Ref<AudioVideoRenderer> MediaPlayerPrivateWebM::createRenderer(LoggerHelper& loggerHelper, HTMLMediaElementIdentifier mediaElementIdentifier, MediaPlayerIdentifier playerIdentifier)
{
    if (hasPlatformStrategies()) {
        if (RefPtr renderer = platformStrategies()->mediaStrategy()->createAudioVideoRenderer(&loggerHelper, mediaElementIdentifier, playerIdentifier))
            return renderer.releaseNonNull();
    }
    return AudioVideoRendererAVFObjC::create(Ref { loggerHelper.logger() }, loggerHelper.logIdentifier());
}

Ref<MediaPlayerPrivateWebM> MediaPlayerPrivateWebM::create(MediaPlayer& player)
{
    return adoptRef(*new MediaPlayerPrivateWebM(player));
}

MediaPlayerPrivateWebM::MediaPlayerPrivateWebM(MediaPlayer& player)
    : m_player(player)
    , m_parser(SourceBufferParserWebM::create().releaseNonNull())
    , m_appendQueue(WorkQueue::create("MediaPlayerPrivateWebM data parser queue"_s))
    , m_logger(player.mediaPlayerLogger())
    , m_logIdentifier(player.mediaPlayerLogIdentifier())
    , m_seekTimer(*this, &MediaPlayerPrivateWebM::seekInternal)
    , m_rendererSeekRequest(NativePromiseRequest::create())
    , m_playerIdentifier(MediaPlayerIdentifier::generate())
    , m_renderer(createRenderer(*this, player.clientIdentifier(), m_playerIdentifier))
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_parser->setLogger(m_logger, m_logIdentifier);
    m_parser->setDidParseInitializationDataCallback([weakThis = ThreadSafeWeakPtr { *this }, this] (InitializationSegment&& segment) {
        if (RefPtr protectedThis = weakThis.get())
            didParseInitializationData(WTFMove(segment));
    });

    m_parser->setDidProvideMediaDataCallback([weakThis = ThreadSafeWeakPtr { *this }, this] (Ref<MediaSampleAVFObjC>&& sample, TrackID trackId, const String& mediaType) {
        if (RefPtr protectedThis = weakThis.get())
            didProvideMediaDataForTrackId(WTFMove(sample), trackId, mediaType);
    });

#if HAVE(SPATIAL_TRACKING_LABEL)
    m_defaultSpatialTrackingLabel = player.defaultSpatialTrackingLabel();
    m_spatialTrackingLabel = player.spatialTrackingLabel();
#endif
}

MediaPlayerPrivateWebM::~MediaPlayerPrivateWebM()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    cancelPendingSeek();

    clearTracks();

    cancelLoad();
}

static HashSet<String>& mimeTypeCache()
{
    static NeverDestroyed cache = HashSet<String>();
    if (cache->isEmpty())
        cache->addAll(SourceBufferParserWebM::supportedMIMETypes());
    return cache;
}

void MediaPlayerPrivateWebM::getSupportedTypes(HashSet<String>& types)
{
    types = mimeTypeCache();
}

MediaPlayer::SupportsType MediaPlayerPrivateWebM::supportsType(const MediaEngineSupportParameters& parameters)
{
    if (parameters.isMediaSource || parameters.isMediaStream || parameters.requiresRemotePlayback)
        return MediaPlayer::SupportsType::IsNotSupported;

    return SourceBufferParserWebM::isContentTypeSupported(parameters.type, parameters.supportsLimitedMatroska);
}

void MediaPlayerPrivateWebM::setPreload(MediaPlayer::Preload preload)
{
    ALWAYS_LOG(LOGIDENTIFIER, " - ", static_cast<int>(preload));
    if (preload == std::exchange(m_preload, preload))
        return;
    doPreload();
}

void MediaPlayerPrivateWebM::doPreload()
{
    if (m_assetURL.isEmpty() || m_networkState >= MediaPlayerNetworkState::FormatError) {
        INFO_LOG(LOGIDENTIFIER, " - hasURL = ", static_cast<int>(m_assetURL.isEmpty()), " networkState = ", static_cast<int>(m_networkState));
        return;
    }

    RefPtr player = m_player.get();
    if (!player)
        return;

    auto mimeType = player->contentMIMEType();
    if (mimeType.isEmpty() || !mimeTypeCache().contains(mimeType)) {
        ERROR_LOG(LOGIDENTIFIER, "mime type = ", mimeType, " not supported");
        setNetworkState(MediaPlayer::NetworkState::FormatError);
        return;
    }

    if (m_preload >= MediaPlayer::Preload::MetaData && needsResourceClient()) {
        if (!createResourceClientIfNeeded()) {
            ERROR_LOG(LOGIDENTIFIER, "could not create resource client");
            setNetworkState(MediaPlayer::NetworkState::NetworkError);
            setReadyState(MediaPlayer::ReadyState::HaveNothing);
        } else
            setNetworkState(MediaPlayer::NetworkState::Loading);
    }

    if (m_preload > MediaPlayer::Preload::MetaData) {
        for (auto it = m_readyForMoreSamplesMap.begin(); it != m_readyForMoreSamplesMap.end(); ++it)
            notifyClientWhenReadyForMoreSamples(it->first);
    }
}

void MediaPlayerPrivateWebM::load(const URL& url, const LoadOptions& options)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    setReadyState(MediaPlayer::ReadyState::HaveNothing);

    m_assetURL = url;
    if (options.supportsLimitedMatroska)
        m_parser->allowLimitedMatroska();

    m_renderer->setPreferences(options.videoRendererPreferences | VideoRendererPreference::PrefersDecompressionSession);

    m_renderer->notifyWhenErrorOccurs([weakThis = WeakPtr { *this }](PlatformMediaError error) {
        ensureOnMainThread([weakThis, error] {
            if (RefPtr protectedThis = weakThis.get()) {
                protectedThis->m_errored = true;
                if (RefPtr player = protectedThis->m_player.get(); player && error == PlatformMediaError::IPCError) {
                    player->reloadAndResumePlaybackIfNeeded();
                    return;
                }
                protectedThis->setNetworkState(MediaPlayer::NetworkState::DecodeError);
                protectedThis->setReadyState(MediaPlayer::ReadyState::HaveNothing);
            }
        });
    });

    m_renderer->notifyFirstFrameAvailable([weakThis = WeakPtr { *this }] {
        ensureOnMainThread([weakThis] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->setHasAvailableVideoFrame(true);
        });
    });

    m_renderer->notifyWhenRequiresFlushToResume([weakThis = WeakPtr { *this }] {
        ensureOnMainThread([weakThis] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->setLayerRequiresFlush();
        });
    });

    m_renderer->notifyRenderingModeChanged([weakThis = WeakPtr { *this }] {
        ensureOnMainThread([weakThis] {
            if (RefPtr protectedThis = weakThis.get()) {
                if (RefPtr player = protectedThis->m_player.get())
                    player->renderingModeChanged();
            }
        });
    });

    m_renderer->notifySizeChanged([weakThis = WeakPtr { *this }](const MediaTime&, FloatSize size) {
        ensureOnMainThread([weakThis, size] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->setNaturalSize(size);
        });
    });

    m_renderer->notifyEffectiveRateChanged([weakThis = WeakPtr { *this }](double) {
        ensureOnMainThread([weakThis] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->effectiveRateChanged();
        });
    });

    m_renderer->setPreferences(VideoRendererPreference::PrefersDecompressionSession);

    m_renderer->notifyVideoLayerSizeChanged([weakThis = WeakPtr { *this }](const MediaTime&, FloatSize size) {
        ensureOnMainThread([weakThis, size] {
            if (RefPtr protectedThis = weakThis.get()) {
                if (RefPtr player = protectedThis->m_player.get())
                    player->videoLayerSizeDidChange(size);
            }
        });
    });

    if (RefPtr player = m_player.get()) {
        m_renderer->setVolume(player->volume());
        m_renderer->setMuted(player->muted());
        m_renderer->setPreservesPitchAndCorrectionAlgorithm(player->preservesPitch(), player->pitchCorrectionAlgorithm());
#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
        m_renderer->setOutputDeviceId(player->audioOutputDeviceIdOverride());
#endif
#if ENABLE(LINEAR_MEDIA_PLAYER)
        m_renderer->setVideoTarget(player->videoTarget());
#endif
        m_renderer->setPresentationSize(player->presentationSize());
        m_renderer->renderingCanBeAcceleratedChanged(player->renderingCanBeAccelerated());
    }

    doPreload();
}

bool MediaPlayerPrivateWebM::needsResourceClient() const
{
    return !m_resourceClient && m_needsResourceClient;
}

bool MediaPlayerPrivateWebM::createResourceClientIfNeeded()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(needsResourceClient());

    RefPtr player = m_player.get();
    if (!player)
        return false;

    ResourceRequest request(URL { m_assetURL });
    request.setAllowCookies(true);
    if (m_contentReceived) {
        if (!m_contentLength)
            return false;
        if (m_contentLength <= m_contentReceived) {
            m_needsResourceClient = false;
            return true;
        }
        request.addHTTPHeaderField(HTTPHeaderName::Range, makeString("bytes="_s, m_contentReceived, '-', m_contentLength));
    }

    m_resourceClient = WebMResourceClient::create(*this, player->mediaResourceLoader(), WTFMove(request));

    return !!m_resourceClient;
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateWebM::load(const URL&, const LoadOptions&, MediaSourcePrivateClient&)
{
    ERROR_LOG(LOGIDENTIFIER, "tried to load as mediasource");

    setNetworkState(MediaPlayer::NetworkState::FormatError);
}
#endif

#if ENABLE(MEDIA_STREAM)
void MediaPlayerPrivateWebM::load(MediaStreamPrivate&)
{
    ERROR_LOG(LOGIDENTIFIER, "tried to load as mediastream");

    setNetworkState(MediaPlayer::NetworkState::FormatError);
}
#endif

void MediaPlayerPrivateWebM::dataLengthReceived(size_t length)
{
    callOnMainThread([protectedThis = Ref { *this }, length] {
        protectedThis->m_contentLength = length;
    });
}

void MediaPlayerPrivateWebM::dataReceived(const SharedBuffer& buffer)
{
    ALWAYS_LOG(LOGIDENTIFIER, "data length = ", buffer.size());

    callOnMainThread([protectedThis = Ref { *this }, this, size = buffer.size()] {
        setNetworkState(MediaPlayer::NetworkState::Loading);
        m_pendingAppends++;
        m_contentReceived += size;
    });

    invokeAsync(m_appendQueue, [buffer = Ref { buffer }, parser = m_parser]() mutable {
        return MediaPromise::createAndSettle(parser->appendData(WTFMove(buffer)));
    })->whenSettled(RunLoop::mainSingleton(), [weakThis = ThreadSafeWeakPtr { *this }](auto&& result) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->appendCompleted(!!result);
    });
}

void MediaPlayerPrivateWebM::loadFailed(const ResourceError& error)
{
    ERROR_LOG(LOGIDENTIFIER, "resource failed to load with code ", error.errorCode());
    callOnMainThread([protectedThis = Ref { *this }] {
        protectedThis->setNetworkState(MediaPlayer::NetworkState::NetworkError);
    });
}

void MediaPlayerPrivateWebM::loadFinished()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    callOnMainThread([protectedThis = Ref { *this }] {
        protectedThis->m_loadFinished = true;
        protectedThis->maybeFinishLoading();
    });
}

void MediaPlayerPrivateWebM::cancelLoad()
{
    if (RefPtr resourceClient = m_resourceClient) {
        resourceClient->stop();
        m_resourceClient = nullptr;
    }
}

PlatformLayer* MediaPlayerPrivateWebM::platformLayer() const
{
    return m_renderer->platformVideoLayer();
}

void MediaPlayerPrivateWebM::prepareToPlay()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    setPreload(MediaPlayer::Preload::Auto);
}

void MediaPlayerPrivateWebM::play()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    playInternal();
}

void MediaPlayerPrivateWebM::pause()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_renderer->pause();
}

bool MediaPlayerPrivateWebM::paused() const
{
    return m_renderer->paused();
}

bool MediaPlayerPrivateWebM::playAtHostTime(const MonotonicTime& hostTime)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    playInternal(hostTime);
    return true;
}

bool MediaPlayerPrivateWebM::pauseAtHostTime(const MonotonicTime& hostTime)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_renderer->pause(hostTime);
    return true;
}

void MediaPlayerPrivateWebM::playInternal(std::optional<MonotonicTime> hostTime)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    flushVideoIfNeeded();

    m_renderer->play(hostTime);

    if (!shouldBePlaying())
        return;

    if (currentTime() >= duration())
        seekToTarget(SeekTarget::zero());
}

bool MediaPlayerPrivateWebM::performTaskAtTime(Function<void(const MediaTime&)>&& task, const MediaTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER, time);

    m_renderer->performTaskAtTime(time, [task = WTFMove(task)](const MediaTime& time) mutable {
        ensureOnMainThread([time, task = WTFMove(task)] {
            task(time);
        });
    });
    return true;
}

void MediaPlayerPrivateWebM::audioOutputDeviceChanged()
{
#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    if (RefPtr player = m_player.get())
        m_renderer->setOutputDeviceId(player->audioOutputDeviceId());
#endif
}

bool MediaPlayerPrivateWebM::timeIsProgressing() const
{
    return m_renderer->timeIsProgressing();
}

void MediaPlayerPrivateWebM::setPageIsVisible(bool visible)
{
    if (m_visible == visible)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, visible);
    m_visible = visible;
    m_renderer->setIsVisible(visible);

#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif
}

MediaTime MediaPlayerPrivateWebM::currentTime() const
{
    return m_renderer->currentTime();
}

void MediaPlayerPrivateWebM::seekToTarget(const SeekTarget& target)
{
    ALWAYS_LOG(LOGIDENTIFIER, "time = ", target.time, ", negativeThreshold = ", target.negativeThreshold, ", positiveThreshold = ", target.positiveThreshold);

    m_pendingSeek = target;

    if (m_seekTimer.isActive())
        m_seekTimer.stop();
    m_seekTimer.startOneShot(0_s);
}

void MediaPlayerPrivateWebM::seekInternal()
{
    if (!m_pendingSeek)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, m_pendingSeek->time);

    auto pendingSeek = std::exchange(m_pendingSeek, { }).value();
    m_lastSeekTime = pendingSeek.time;

    cancelPendingSeek();

    m_seeking = true;

    m_renderer->prepareToSeek();

    waitForTimeBuffered(m_lastSeekTime)->whenSettled(RunLoop::mainSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, seekTime = m_lastSeekTime](auto&& result) {
        RefPtr protectedThis = weakThis.get();
        if (!result || !protectedThis)
            return; // seek cancelled.

        return protectedThis->startSeek(seekTime);
    });
}

void MediaPlayerPrivateWebM::cancelPendingSeek()
{
    if (m_rendererSeekRequest->hasCallback())
        m_rendererSeekRequest->disconnect();
    if (auto promise = std::exchange(m_waitForTimeBufferedPromise, std::nullopt))
        promise->reject();
}

void MediaPlayerPrivateWebM::startSeek(const MediaTime& seekTime)
{
    m_renderer->seekTo(seekTime)->whenSettled(RunLoop::mainSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, seekTime](auto&& result) {
        if (!result && result.error() != PlatformMediaError::RequiresFlushToResume)
            return; // cancelled.

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->m_rendererSeekRequest->complete();

        if (!result) {
            ASSERT(result.error() == PlatformMediaError::RequiresFlushToResume);
            protectedThis->flush();
            protectedThis->reenqueueMediaForTime(seekTime);
            // Try seeking again.
            return protectedThis->startSeek(seekTime);
        }
        protectedThis->completeSeek(*result);
    })->track(m_rendererSeekRequest.get());
}

void MediaPlayerPrivateWebM::completeSeek(const MediaTime& seekedTime)
{
    ALWAYS_LOG(LOGIDENTIFIER, "");

    m_seeking = false;

    monitorReadyState();

    if (RefPtr player = m_player.get()) {
        player->seeked(seekedTime);
        player->timeChanged();
    }
}

Ref<GenericPromise> MediaPlayerPrivateWebM::waitForTimeBuffered(const MediaTime& time)
{
    ASSERT(!m_waitForTimeBufferedPromise);

    if (m_buffered.containWithEpsilon(time, timeFudgeFactor())) {
        ALWAYS_LOG(LOGIDENTIFIER, "buffered contains seektime, resolving");
        return GenericPromise::createAndResolve();
    }

    setReadyState(MediaPlayer::ReadyState::HaveMetadata);

    ALWAYS_LOG(LOGIDENTIFIER, "buffered doesn't contain seektime waiting");
    m_waitForTimeBufferedPromise.emplace();
    return m_waitForTimeBufferedPromise->promise();
}

bool MediaPlayerPrivateWebM::seeking() const
{
    return m_pendingSeek || m_seeking;
}

bool MediaPlayerPrivateWebM::shouldBePlaying() const
{
    return !m_renderer->paused() && !seeking();
}

void MediaPlayerPrivateWebM::setRateDouble(double rate)
{
    if (rate == m_rate)
        return;

    m_rate = std::max<double>(rate, 0);

    m_renderer->setRate(m_rate);

    if (RefPtr player = m_player.get())
        player->rateChanged();
}

double MediaPlayerPrivateWebM::effectiveRate() const
{
    return m_renderer->effectiveRate();
}

void MediaPlayerPrivateWebM::setVolume(float volume)
{
    m_renderer->setVolume(volume);
}

void MediaPlayerPrivateWebM::setMuted(bool muted)
{
    m_renderer->setMuted(muted);
}

const PlatformTimeRanges& MediaPlayerPrivateWebM::buffered() const
{
    return m_buffered;
}

void MediaPlayerPrivateWebM::setBufferedRanges(PlatformTimeRanges timeRanges)
{
    if (m_buffered == timeRanges)
        return;
    m_buffered = WTFMove(timeRanges);
    if (RefPtr player = m_player.get()) {
        player->bufferedTimeRangesChanged();
        player->seekableTimeRangesChanged();
    }

    monitorReadyState();
}

void MediaPlayerPrivateWebM::updateBufferedFromTrackBuffers(bool ended)
{
    MediaTime highestEndTime = MediaTime::negativeInfiniteTime();
    for (auto& pair : m_trackBufferMap) {
        auto& trackBuffer = pair.second;
        if (!trackBuffer->buffered().length())
            continue;
        highestEndTime = std::max(highestEndTime, trackBuffer->maximumBufferedTime());
    }

    // NOTE: Short circuit the following if none of the TrackBuffers have buffered ranges to avoid generating
    // a single range of {0, 0}.
    if (highestEndTime.isNegativeInfinite()) {
        setBufferedRanges(PlatformTimeRanges());
        return;
    }

    PlatformTimeRanges intersectionRanges { MediaTime::zeroTime(), highestEndTime };

    for (auto& pair : m_trackBufferMap) {
        auto& trackBuffer = pair.second;
        if (!trackBuffer->buffered().length())
            continue;

        PlatformTimeRanges trackRanges = trackBuffer->buffered();

        if (ended)
            trackRanges.add(trackRanges.maximumBufferedTime(), highestEndTime);

        intersectionRanges.intersectWith(trackRanges);
    }

    setBufferedRanges(WTFMove(intersectionRanges));
}

void MediaPlayerPrivateWebM::updateDurationFromTrackBuffers()
{
    MediaTime highestEndTime = MediaTime::zeroTime();
    for (auto& pair : m_trackBufferMap) {
        auto& trackBuffer = pair.second;
        if (!trackBuffer->highestPresentationTimestamp())
            continue;
        highestEndTime = std::max(highestEndTime, trackBuffer->highestPresentationTimestamp());
    }

    setDuration(WTFMove(highestEndTime));
}

void MediaPlayerPrivateWebM::setLoadingProgresssed(bool loadingProgressed)
{
    INFO_LOG(LOGIDENTIFIER, loadingProgressed);
    m_loadingProgressed = loadingProgressed;
}

bool MediaPlayerPrivateWebM::didLoadingProgress() const
{
    return std::exchange(m_loadingProgressed, false);
}

RefPtr<NativeImage> MediaPlayerPrivateWebM::nativeImageForCurrentTime()
{
    updateLastImage();
    return m_lastImage;
}

bool MediaPlayerPrivateWebM::updateLastVideoFrame()
{
    RefPtr videoFrame = m_renderer->currentVideoFrame();
    if (!videoFrame)
        return false;

    INFO_LOG(LOGIDENTIFIER, "displayed pixelbuffer copied for time ", videoFrame->presentationTime());
    m_lastVideoFrame = WTFMove(videoFrame);
    return true;
}

bool MediaPlayerPrivateWebM::updateLastImage()
{
    if (m_isGatheringVideoFrameMetadata) {
        auto metrics = m_renderer->videoPlaybackQualityMetrics();
        auto sampleCount = metrics ? metrics->displayCompositedVideoFrames : 0;
        if (sampleCount == m_lastConvertedSampleCount)
            return false;
        m_lastConvertedSampleCount = sampleCount;
    }
    m_lastImage = m_renderer->currentNativeImage();
    return !!m_lastImage;
}

void MediaPlayerPrivateWebM::paint(GraphicsContext& context, const FloatRect& rect)
{
    paintCurrentFrameInContext(context, rect);
}

void MediaPlayerPrivateWebM::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& outputRect)
{
    m_renderer->paintCurrentVideoFrameInContext(context, outputRect);
}

RefPtr<VideoFrame> MediaPlayerPrivateWebM::videoFrameForCurrentTime()
{
    if (!m_isGatheringVideoFrameMetadata)
        updateLastVideoFrame();
    return m_lastVideoFrame;
}

DestinationColorSpace MediaPlayerPrivateWebM::colorSpace()
{
    updateLastImage();
    RefPtr lastImage = m_lastImage;
    return lastImage ? lastImage->colorSpace() : DestinationColorSpace::SRGB();
}

void MediaPlayerPrivateWebM::setNaturalSize(FloatSize size)
{
    auto oldSize = m_naturalSize;
    m_naturalSize = size;
    if (oldSize != m_naturalSize) {
        INFO_LOG(LOGIDENTIFIER, "was ", oldSize, ", is ", size);
        if (RefPtr player = m_player.get())
            player->sizeChanged();
    }
}

void MediaPlayerPrivateWebM::effectiveRateChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER, effectiveRate());
    if (RefPtr player = m_player.get())
        player->rateChanged();
}

void MediaPlayerPrivateWebM::setHasAudio(bool hasAudio)
{
    if (hasAudio == m_hasAudio)
        return;

    m_hasAudio = hasAudio;
    characteristicsChanged();
}

void MediaPlayerPrivateWebM::setHasVideo(bool hasVideo)
{
    if (hasVideo == m_hasVideo)
        return;

    m_hasVideo = hasVideo;
    characteristicsChanged();
}

void MediaPlayerPrivateWebM::setHasAvailableVideoFrame(bool hasAvailableVideoFrame)
{
    if (m_hasAvailableVideoFrame == hasAvailableVideoFrame)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, hasAvailableVideoFrame);
    m_hasAvailableVideoFrame = hasAvailableVideoFrame;

    if (!m_hasAvailableVideoFrame)
        return;

    if (RefPtr player = m_player.get())
        player->firstVideoFrameAvailable();

    if (m_readyState <= MediaPlayer::ReadyState::HaveMetadata) {
        setReadyState(MediaPlayer::ReadyState::HaveCurrentData);
        return;
    }

    if (!m_readyStateIsWaitingForAvailableFrame)
        return;

    m_readyStateIsWaitingForAvailableFrame = false;
    if (RefPtr player = m_player.get())
        player->readyStateChanged();
}

void MediaPlayerPrivateWebM::setDuration(MediaTime duration)
{
    if (duration == m_duration)
        return;

    m_renderer->notifyTimeReachedAndStall(duration, [weakThis = ThreadSafeWeakPtr { *this }](const MediaTime&) {
        ensureOnMainThread([weakThis] {
            if (RefPtr protectedThis = weakThis.get()) {
                protectedThis->m_renderer->pause();
                if (RefPtr player = protectedThis->m_player.get())
                    player->timeChanged();
            }
        });
    });

    m_duration = WTFMove(duration);
    if (RefPtr player = m_player.get())
        player->durationChanged();

    monitorReadyState();
}

void MediaPlayerPrivateWebM::setNetworkState(MediaPlayer::NetworkState state)
{
    if (state == m_networkState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, state);
    m_networkState = state;
    if (RefPtr player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateWebM::setReadyState(MediaPlayer::ReadyState state)
{
    if (state == m_readyState)
        return;

    m_readyState = state;
    bool waitingOnAvailableFrame = m_readyState >= MediaPlayer::ReadyState::HaveCurrentData && hasVideo() && !m_hasAvailableVideoFrame;
    ALWAYS_LOG(LOGIDENTIFIER, state, " waitingOnAvailableVideoFrame: ", waitingOnAvailableFrame);

    m_readyStateIsWaitingForAvailableFrame = waitingOnAvailableFrame;
    if (waitingOnAvailableFrame)
        return;

    if (RefPtr player = m_player.get())
        player->readyStateChanged();
}

void MediaPlayerPrivateWebM::characteristicsChanged()
{
    if (RefPtr player = m_player.get())
        player->characteristicChanged();
}

void MediaPlayerPrivateWebM::setPreservesPitch(bool preservesPitch)
{
    ALWAYS_LOG(LOGIDENTIFIER, preservesPitch);
    if (RefPtr player = m_player.get())
        m_renderer->setPreservesPitchAndCorrectionAlgorithm(preservesPitch, player->pitchCorrectionAlgorithm());
}

void MediaPlayerPrivateWebM::setPresentationSize(const IntSize& newSize)
{
    m_renderer->setPresentationSize(newSize);
}

void MediaPlayerPrivateWebM::acceleratedRenderingStateChanged()
{
    RefPtr player = m_player.get();
    m_renderer->renderingCanBeAcceleratedChanged(player ? player->renderingCanBeAccelerated() : false);
}

RetainPtr<PlatformLayer> MediaPlayerPrivateWebM::createVideoFullscreenLayer()
{
    return adoptNS([[CALayer alloc] init]);
}

void MediaPlayerPrivateWebM::setVideoFullscreenLayer(PlatformLayer *videoFullscreenLayer, WTF::Function<void()>&& completionHandler)
{
    m_renderer->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler));
}

void MediaPlayerPrivateWebM::setVideoFullscreenFrame(const FloatRect& frame)
{
    m_renderer->setVideoFullscreenFrame(frame);
}

void MediaPlayerPrivateWebM::syncTextTrackBounds()
{
    m_renderer->syncTextTrackBounds();
}

void MediaPlayerPrivateWebM::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    m_renderer->setTextTrackRepresentation(representation);
}

String MediaPlayerPrivateWebM::engineDescription() const
{
    static NeverDestroyed<String> description(MAKE_STATIC_STRING_IMPL("Cocoa WebM Engine"));
    return description;
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void MediaPlayerPrivateWebM::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& target)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_playbackTarget = WTFMove(target);
}

void MediaPlayerPrivateWebM::setShouldPlayToPlaybackTarget(bool shouldPlayToTarget)
{
    if (shouldPlayToTarget == m_shouldPlayToTarget)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, shouldPlayToTarget);
    m_shouldPlayToTarget = shouldPlayToTarget;

    if (RefPtr player = m_player.get())
        player->currentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless());
}

bool MediaPlayerPrivateWebM::isCurrentPlaybackTargetWireless() const
{
    RefPtr playbackTarget = m_playbackTarget;
    if (!playbackTarget)
        return false;

    auto hasTarget = m_shouldPlayToTarget && playbackTarget->hasActiveRoute();
    INFO_LOG(LOGIDENTIFIER, hasTarget);
    return hasTarget;
}
#endif

void MediaPlayerPrivateWebM::enqueueSample(Ref<MediaSample>&& sample, TrackID trackId)
{
    auto logSiteIdentifier = LOGIDENTIFIER;
    DEBUG_LOG(logSiteIdentifier, "track ID = ", trackId, ", sample = ", sample.get());

    PlatformSample platformSample = sample->platformSample();

    CMFormatDescriptionRef formatDescription = PAL::CMSampleBufferGetFormatDescription(platformSample.cmSampleBuffer());
    ASSERT(formatDescription);
    if (!formatDescription) {
        ERROR_LOG(logSiteIdentifier, "Received sample with a null formatDescription. Bailing.");
        return;
    }
    auto mediaType = PAL::CMFormatDescriptionGetMediaType(formatDescription);

    if (isEnabledVideoTrackID(trackId)) {
        // AVSampleBufferDisplayLayer will throw an un-documented exception if passed a sample
        // whose media type is not kCMMediaType_Video. This condition is exceptional; we should
        // never enqueue a non-video sample in a AVSampleBufferDisplayLayer.
        ASSERT(mediaType == kCMMediaType_Video);
        if (mediaType != kCMMediaType_Video) {
            ERROR_LOG(logSiteIdentifier, "Expected sample of type '", FourCC(kCMMediaType_Video), "', got '", FourCC(mediaType), "'. Bailing.");
            return;
        }
        m_renderer->enqueueSample(trackIdentifierFor(trackId), WTFMove(sample));
        return;
    }
    // AVSampleBufferAudioRenderer will throw an un-documented exception if passed a sample
    // whose media type is not kCMMediaType_Audio. This condition is exceptional; we should
    // never enqueue a non-video sample in a AVSampleBufferAudioRenderer.
    ASSERT(mediaType == kCMMediaType_Audio);
    if (mediaType != kCMMediaType_Audio) {
        ERROR_LOG(logSiteIdentifier, "Expected sample of type '", FourCC(kCMMediaType_Audio), "', got '", FourCC(mediaType), "'. Bailing.");
        return;
    }

    if (m_readyState < MediaPlayer::ReadyState::HaveEnoughData && !m_enabledVideoTrackID)
        setReadyState(MediaPlayer::ReadyState::HaveEnoughData);

    m_renderer->enqueueSample(trackIdentifierFor(trackId), WTFMove(sample));
}

void MediaPlayerPrivateWebM::reenqueSamples(TrackID trackId, NeedsFlush needsFlush)
{
    auto it = m_trackBufferMap.find(trackId);
    if (it == m_trackBufferMap.end())
        return;
    TrackBuffer& trackBuffer = it->second;
    trackBuffer.setNeedsReenqueueing(true);
    reenqueueMediaForTime(trackBuffer, trackId, currentTime(), needsFlush);
}

void MediaPlayerPrivateWebM::reenqueueMediaForTime(const MediaTime& time)
{
    for (auto& trackBufferPair : m_trackBufferMap) {
        TrackBuffer& trackBuffer = trackBufferPair.second;
        auto trackId = trackBufferPair.first;
        reenqueueMediaForTime(trackBuffer, trackId, time, NeedsFlush::No);
    }
}

void MediaPlayerPrivateWebM::reenqueueMediaForTime(TrackBuffer& trackBuffer, TrackID trackId, const MediaTime& time, NeedsFlush needsFlush)
{
    if (needsFlush == NeedsFlush::Yes)
        m_renderer->flushTrack(trackIdentifierFor(trackId));

    if (trackBuffer.reenqueueMediaForTime(time, timeFudgeFactor(), m_loadFinished))
        provideMediaData(trackBuffer, trackId);
}

void MediaPlayerPrivateWebM::notifyClientWhenReadyForMoreSamples(TrackID trackId)
{
    if (m_requestReadyForMoreSamplesSetMap[trackId])
        return;
    m_requestReadyForMoreSamplesSetMap[trackId] = true;

    auto trackIdentifier = maybeTrackIdentifierFor(trackId);
    if (!trackIdentifier)
        return; // track hasn't been enabled yet.
    m_renderer->requestMediaDataWhenReady(*trackIdentifier)->whenSettled(RunLoop::mainSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, trackId](auto&& result) {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && result)
            protectedThis->didBecomeReadyForMoreSamples(trackId);
    });
}

bool MediaPlayerPrivateWebM::isReadyForMoreSamples(TrackID trackId)
{
    auto trackIdentifier = maybeTrackIdentifierFor(trackId);
    return trackIdentifier && m_renderer->isReadyForMoreSamples(*trackIdentifier);
}

void MediaPlayerPrivateWebM::didBecomeReadyForMoreSamples(TrackID trackId)
{
    INFO_LOG(LOGIDENTIFIER, trackId);

    m_requestReadyForMoreSamplesSetMap[trackId] = false;

    provideMediaData(trackId);
}

void MediaPlayerPrivateWebM::appendCompleted(bool success)
{
    assertIsMainThread();

    ASSERT(m_pendingAppends > 0);
    m_pendingAppends--;
    INFO_LOG(LOGIDENTIFIER, "pending appends = ", m_pendingAppends, " success = ", success);
    setLoadingProgresssed(true);
    m_errored |= !success;
    if (!m_errored)
        updateBufferedFromTrackBuffers(m_loadFinished && !m_pendingAppends);

    if (m_waitForTimeBufferedPromise && m_buffered.containWithEpsilon(m_lastSeekTime, timeFudgeFactor())) {
        ALWAYS_LOG(LOGIDENTIFIER, "can continue seeking data is now buffered");
        m_waitForTimeBufferedPromise->resolve();
        m_waitForTimeBufferedPromise.reset();
    }
    maybeFinishLoading();
}

void MediaPlayerPrivateWebM::maybeFinishLoading()
{
    if (m_loadFinished && !m_pendingAppends) {
        if (!m_hasVideo && !m_hasAudio) {
            ERROR_LOG(LOGIDENTIFIER, "could not load audio or video tracks");
            setNetworkState(MediaPlayer::NetworkState::FormatError);
            setReadyState(MediaPlayer::ReadyState::HaveNothing);
            return;
        }
        if (m_errored) {
            ERROR_LOG(LOGIDENTIFIER, "parsing error");
            setNetworkState(m_readyState >= MediaPlayer::ReadyState::HaveMetadata ? MediaPlayer::NetworkState::DecodeError : MediaPlayer::NetworkState::FormatError);
            return;
        }
        setNetworkState(MediaPlayer::NetworkState::Idle);

        updateDurationFromTrackBuffers();
    }
}

void MediaPlayerPrivateWebM::provideMediaData(TrackID trackId)
{
    if (auto it = m_trackBufferMap.find(trackId); it != m_trackBufferMap.end())
        provideMediaData(it->second, trackId);
}

void MediaPlayerPrivateWebM::provideMediaData(TrackBuffer& trackBuffer, TrackID trackId)
{
    if (m_errored)
        return;

    if (trackBuffer.needsReenqueueing())
        return;
    if (isEnabledVideoTrackID(trackId) && m_layerRequiresFlush)
        return;

    unsigned enqueuedSamples = 0;

    while (true) {
        if (!isReadyForMoreSamples(trackId)) {
            DEBUG_LOG(LOGIDENTIFIER, "bailing early, track id ", trackId, " is not ready for more data");
            notifyClientWhenReadyForMoreSamples(trackId);
            break;
        }

        RefPtr sample = trackBuffer.nextSample();
        if (!sample)
            break;
        enqueueSample(sample.releaseNonNull(), trackId);
        ++enqueuedSamples;
    }

    DEBUG_LOG(LOGIDENTIFIER, "enqueued ", enqueuedSamples, " samples, ", trackBuffer.remainingSamples(), " remaining");
}

void MediaPlayerPrivateWebM::trackDidChangeSelected(VideoTrackPrivate& track, bool selected)
{
    auto trackId = track.id();

    if (!m_trackBufferMap.contains(trackId))
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "video trackID = ", trackId, ", selected = ", selected);

    if (selected) {
        m_enabledVideoTrackID = trackId;
        m_readyForMoreSamplesMap[trackId] = true;
        m_trackIdentifiers.emplace(trackId, m_renderer->addTrack(TrackType::Video));
        return;
    }

    if (!isEnabledVideoTrackID(trackId))
        return;

    m_enabledVideoTrackID.reset();
    m_renderer->removeTrack(trackIdentifierFor(trackId));
    m_trackIdentifiers.erase(trackId);
    m_readyForMoreSamplesMap.erase(trackId);
}

void MediaPlayerPrivateWebM::trackDidChangeEnabled(AudioTrackPrivate& track, bool enabled)
{
    auto trackId = track.id();

    if (!m_trackBufferMap.contains(trackId))
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "audio trackID = ", trackId, ", enabled = ", enabled);

    if (enabled) {
        auto trackIdentifier = m_renderer->addTrack(TrackType::Audio);
        m_trackIdentifiers.emplace(trackId, trackIdentifier);
        if (!m_errored) {
            m_readyForMoreSamplesMap[trackId] = true;
            characteristicsChanged();
        }
        m_renderer->notifyTrackNeedsReenqueuing(trackIdentifier, [weakThis = WeakPtr { *this }, trackId](TrackIdentifier, const MediaTime&) {
            ensureOnMainThread([weakThis, trackId] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->reenqueSamples(trackId, NeedsFlush::No);
            });
        });
        return;
    }

    m_renderer->removeTrack(trackIdentifierFor(trackId));
    m_trackIdentifiers.erase(trackId);
    m_readyForMoreSamplesMap.erase(trackId);
}

void MediaPlayerPrivateWebM::didParseInitializationData(InitializationSegment&& segment)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_preload == MediaPlayer::Preload::MetaData && !m_loadFinished)
        cancelLoad();

    clearTracks();

    RefPtr player = m_player.get();
    for (auto videoTrackInfo : segment.videoTracks) {
        if (videoTrackInfo.track) {
            // FIXME: Use downcast instead.
            auto track = unsafeRefPtrDowncast<VideoTrackPrivateWebM>(videoTrackInfo.track);
#if PLATFORM(IOS_FAMILY)
            if (shouldCheckHardwareSupport() && (videoTrackInfo.description->codec() == "vp8"_s || (videoTrackInfo.description->codec() == "vp9"_s && !vp9HardwareDecoderAvailable()))) {
                m_errored = true;
                return;
            }
#endif
            addTrackBuffer(track->id(), WTFMove(videoTrackInfo.description));

            track->setSelectedChangedCallback([weakThis = ThreadSafeWeakPtr { *this }] (VideoTrackPrivate& track, bool selected) {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;

                auto videoTrackSelectedChanged = [weakThis, trackRef = Ref { track }, selected] {
                    if (RefPtr protectedThis = weakThis.get())
                        protectedThis->trackDidChangeSelected(trackRef, selected);
                };

                if (!protectedThis->m_processingInitializationSegment) {
                    videoTrackSelectedChanged();
                    return;
                }
            });

            if (m_videoTracks.isEmpty()) {
                setNaturalSize({ float(track->width()), float(track->height()) });
                track->setSelected(true);
            }

            m_videoTracks.append(track);
            if (player)
                player->addVideoTrack(*track);
        }
    }

    for (auto audioTrackInfo : segment.audioTracks) {
        if (audioTrackInfo.track) {
            // FIXME: Use downcast instead.
            auto track = unsafeRefPtrDowncast<AudioTrackPrivateWebM>(audioTrackInfo.track);
            addTrackBuffer(track->id(), WTFMove(audioTrackInfo.description));

            track->setEnabledChangedCallback([weakThis = ThreadSafeWeakPtr { *this }] (AudioTrackPrivate& track, bool enabled) {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;

                auto audioTrackEnabledChanged = [weakThis, trackRef = Ref { track }, enabled] {
                    if (RefPtr protectedThis = weakThis.get())
                        protectedThis->trackDidChangeEnabled(trackRef, enabled);
                };

                if (!protectedThis->m_processingInitializationSegment) {
                    audioTrackEnabledChanged();
                    return;
                }
            });

            if (m_audioTracks.isEmpty())
                track->setEnabled(true);

            m_audioTracks.append(track);
            if (player)
                player->addAudioTrack(*track);
        }
    }

    setReadyState(MediaPlayer::ReadyState::HaveMetadata);

    if (segment.duration.isValid())
        setDuration(WTFMove(segment.duration));
    else
        setDuration(MediaTime::positiveInfiniteTime());
}

void MediaPlayerPrivateWebM::didProvideMediaDataForTrackId(Ref<MediaSampleAVFObjC>&& sample, TrackID trackId, const String& mediaType)
{
    UNUSED_PARAM(mediaType);

    auto it = m_trackBufferMap.find(trackId);
    if (it == m_trackBufferMap.end())
        return;
    TrackBuffer& trackBuffer = it->second;

    trackBuffer.addSample(sample);

    if (m_preload <= MediaPlayer::Preload::MetaData) {
        m_readyForMoreSamplesMap[trackId] = true;
        return;
    }
    if (m_seeking || m_layerRequiresFlush)
        return;
    notifyClientWhenReadyForMoreSamples(trackId);
}

void MediaPlayerPrivateWebM::flush()
{
    m_renderer->flush();
    setHasAvailableVideoFrame(false);
    setAllTracksForReenqueuing();
}

void MediaPlayerPrivateWebM::setAllTracksForReenqueuing()
{
    for (auto& trackBufferPair : m_trackBufferMap) {
        TrackBuffer& trackBuffer = trackBufferPair.second;
        trackBuffer.setNeedsReenqueueing(true);
    }
}

void MediaPlayerPrivateWebM::setTrackForReenqueuing(TrackID trackId)
{
    if (auto it = m_trackBufferMap.find(trackId); it != m_trackBufferMap.end()) {
        TrackBuffer& trackBuffer = it->second;
        trackBuffer.setNeedsReenqueueing(true);
    }
}

void MediaPlayerPrivateWebM::flushVideoIfNeeded()
{
    ALWAYS_LOG(LOGIDENTIFIER, "layerRequiresFlush: ", m_layerRequiresFlush);
    if (!m_layerRequiresFlush)
        return;

    m_layerRequiresFlush = false;

    if (m_enabledVideoTrackID)
        reenqueSamples(*m_enabledVideoTrackID, NeedsFlush::Yes);
}

void MediaPlayerPrivateWebM::addTrackBuffer(TrackID trackId, RefPtr<MediaDescription>&& description)
{
    ASSERT(!m_trackBufferMap.contains(trackId));

    setHasAudio(m_hasAudio || description->isAudio());
    setHasVideo(m_hasVideo || description->isVideo());

    auto trackBuffer = TrackBuffer::create(WTFMove(description), discontinuityTolerance);
    trackBuffer->setLogger(protectedLogger(), logIdentifier());
    m_trackBufferMap.try_emplace(trackId, WTFMove(trackBuffer));
    m_requestReadyForMoreSamplesSetMap[trackId] = false;
}

void MediaPlayerPrivateWebM::clearTracks()
{
    RefPtr player = m_player.get();
    for (auto& track : m_videoTracks) {
        track->setSelectedChangedCallback(nullptr);
        if (player)
            player->removeVideoTrack(*track);
    }
    m_videoTracks.clear();

    for (auto& track : m_audioTracks) {
        track->setEnabledChangedCallback(nullptr);
        if (player)
            player->removeAudioTrack(*track);
    }
    m_audioTracks.clear();
}

void MediaPlayerPrivateWebM::startVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = true;
    m_renderer->notifyWhenHasAvailableVideoFrame([weakThis = WeakPtr { *this }](const MediaTime& presentationTime, double displayTime) {
        ensureOnMainThread([weakThis, presentationTime, displayTime] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->checkNewVideoFrameMetadata(presentationTime, displayTime);
        });
    });
}

void MediaPlayerPrivateWebM::stopVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = false;
    m_videoFrameMetadata = { };
    m_renderer->notifyWhenHasAvailableVideoFrame(nullptr);
}

void MediaPlayerPrivateWebM::checkNewVideoFrameMetadata(const MediaTime& presentationTime, double displayTime)
{
    RefPtr player = m_player.get();
    if (!player)
        return;

    if (!updateLastVideoFrame())
        return;

    Ref lastVideoFrame = *m_lastVideoFrame;

#ifndef NDEBUG
    if (lastVideoFrame->presentationTime() != presentationTime)
        ALWAYS_LOG(LOGIDENTIFIER, "notification of new frame delayed retrieved:", m_lastVideoFrame->presentationTime(), " expected:", presentationTime);
#else
    UNUSED_PARAM(presentationTime);
#endif
    VideoFrameMetadata metadata;
    metadata.width = m_naturalSize.width();
    metadata.height = m_naturalSize.height();
    auto metrics = m_renderer->videoPlaybackQualityMetrics();
    metadata.presentedFrames = metrics ? metrics->displayCompositedVideoFrames : 0;
    metadata.presentationTime = displayTime;
    metadata.expectedDisplayTime = displayTime;
    metadata.mediaTime = lastVideoFrame->presentationTime().toDouble();

    m_videoFrameMetadata = metadata;
    player->onNewVideoFrameMetadata(WTFMove(metadata), lastVideoFrame->pixelBuffer());
}

void MediaPlayerPrivateWebM::setResourceOwner(const ProcessIdentity& resourceOwner)
{
    m_renderer->setResourceOwner(resourceOwner);
}

WTFLogChannel& MediaPlayerPrivateWebM::logChannel() const
{
    return LogMedia;
}

class MediaPlayerFactoryWebM final : public MediaPlayerFactory {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MediaPlayerFactoryWebM);
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::CocoaWebM; };

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer& player) const final
    {
        return MediaPlayerPrivateWebM::create(player);
    }

    void getSupportedTypes(HashSet<String>& types) const final
    {
        return MediaPlayerPrivateWebM::getSupportedTypes(types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return MediaPlayerPrivateWebM::supportsType(parameters);
    }
};

void MediaPlayerPrivateWebM::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (!isAvailable())
        return;

    registrar(makeUnique<MediaPlayerFactoryWebM>());
}

bool MediaPlayerPrivateWebM::isAvailable()
{
    return SourceBufferParserWebM::isAvailable()
        && PAL::isAVFoundationFrameworkAvailable()
        && PAL::isCoreMediaFrameworkAvailable()
        && PAL::getAVSampleBufferAudioRendererClassSingleton()
        && PAL::getAVSampleBufferRenderSynchronizerClassSingleton()
        && class_getInstanceMethod(PAL::getAVSampleBufferAudioRendererClassSingleton(), @selector(setMuted:));
}

bool MediaPlayerPrivateWebM::isEnabledVideoTrackID(TrackID trackID) const
{
    return m_enabledVideoTrackID && *m_enabledVideoTrackID == trackID;
}

bool MediaPlayerPrivateWebM::hasSelectedVideo() const
{
    return !!m_enabledVideoTrackID;
}

void MediaPlayerPrivateWebM::setShouldDisableHDR(bool shouldDisable)
{
    m_renderer->setShouldDisableHDR(shouldDisable);
}

void MediaPlayerPrivateWebM::setPlatformDynamicRangeLimit(PlatformDynamicRangeLimit platformDynamicRangeLimit)
{
    m_renderer->setPlatformDynamicRangeLimit(platformDynamicRangeLimit);
}

void MediaPlayerPrivateWebM::playerContentBoxRectChanged(const LayoutRect& newRect)
{
    m_renderer->contentBoxRectChanged(newRect);
}

void MediaPlayerPrivateWebM::setShouldMaintainAspectRatio(bool shouldMaintainAspectRatio)
{
    m_renderer->setShouldMaintainAspectRatio(shouldMaintainAspectRatio);
}

#if HAVE(SPATIAL_TRACKING_LABEL)
String MediaPlayerPrivateWebM::defaultSpatialTrackingLabel() const
{
    return m_defaultSpatialTrackingLabel;
}

void MediaPlayerPrivateWebM::setDefaultSpatialTrackingLabel(const String& defaultSpatialTrackingLabel)
{
    if (m_defaultSpatialTrackingLabel == defaultSpatialTrackingLabel)
        return;
    m_defaultSpatialTrackingLabel = defaultSpatialTrackingLabel;
    updateSpatialTrackingLabel();
}

String MediaPlayerPrivateWebM::spatialTrackingLabel() const
{
    return m_spatialTrackingLabel;
}

void MediaPlayerPrivateWebM::setSpatialTrackingLabel(const String& spatialTrackingLabel)
{
    if (m_spatialTrackingLabel == spatialTrackingLabel)
        return;
    m_spatialTrackingLabel = spatialTrackingLabel;
    updateSpatialTrackingLabel();
}

void MediaPlayerPrivateWebM::updateSpatialTrackingLabel()
{
#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
    RefPtr player = m_player.get();
    m_renderer->setSpatialTrackingInfo(player && player->prefersSpatialAudioExperience(), player ? player->soundStageSize() : MediaPlayer::SoundStageSize::Auto, player ? player->sceneIdentifier() : emptyString(), m_defaultSpatialTrackingLabel, m_spatialTrackingLabel);
#else
    m_renderer->setSpatialTrackingInfo(false, MediaPlayer::SoundStageSize::Auto, { }, m_defaultSpatialTrackingLabel, m_spatialTrackingLabel);
#endif
}
#endif

#if ENABLE(LINEAR_MEDIA_PLAYER)
void MediaPlayerPrivateWebM::setVideoTarget(const PlatformVideoTarget& videoTarget)
{
    ALWAYS_LOG(LOGIDENTIFIER, !!videoTarget);
    m_renderer->setVideoTarget(videoTarget);
}
#endif

#if PLATFORM(IOS_FAMILY)
void MediaPlayerPrivateWebM::sceneIdentifierDidChange()
{
#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif
}

void MediaPlayerPrivateWebM::applicationWillResignActive()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_renderer->applicationWillResignActive();
    m_applicationIsActive = false;
}

void MediaPlayerPrivateWebM::applicationDidBecomeActive()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_applicationIsActive = true;
    flushVideoIfNeeded();
}
#endif

void MediaPlayerPrivateWebM::isInFullscreenOrPictureInPictureChanged(bool isInFullscreenOrPictureInPicture)
{
    m_renderer->isInFullscreenOrPictureInPictureChanged(isInFullscreenOrPictureInPicture);
}

AudioVideoRenderer::TrackIdentifier MediaPlayerPrivateWebM::trackIdentifierFor(TrackID trackID) const
{
    auto it = m_trackIdentifiers.find(trackID);
    ASSERT(it != m_trackIdentifiers.end());
    return it->second;
}

std::optional<AudioVideoRenderer::TrackIdentifier> MediaPlayerPrivateWebM::maybeTrackIdentifierFor(TrackID trackID) const
{
    if (auto it = m_trackIdentifiers.find(trackID); it != m_trackIdentifiers.end())
        return it->second;
    return { };
}

void MediaPlayerPrivateWebM::setLayerRequiresFlush()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_layerRequiresFlush = true;
#if PLATFORM(IOS_FAMILY)
    if (m_applicationIsActive)
        flushVideoIfNeeded();
#else
    flushVideoIfNeeded();
#endif
}

std::optional<VideoPlaybackQualityMetrics> MediaPlayerPrivateWebM::videoPlaybackQualityMetrics()
{
    return m_renderer->videoPlaybackQualityMetrics();
}

WebCore::HostingContext MediaPlayerPrivateWebM::hostingContext() const
{
    return m_renderer->hostingContext();
}

void MediaPlayerPrivateWebM::setVideoLayerSizeFenced(const WebCore::FloatSize& size, WTF::MachSendRightAnnotated&& sendRightAnnotated)
{
    m_renderer->setVideoLayerSizeFenced(size, WTFMove(sendRightAnnotated));
}

void MediaPlayerPrivateWebM::monitorReadyState()
{
    if (!m_buffered.length())
        return;
    // If we have data up to 3s ahead, we can assume that we can play without interruption.
    constexpr double kHaveEnoughDataThreshold = 3;
    auto currentTime = this->currentTime();
    MediaTime aheadTime = std::min(duration(), currentTime + MediaTime::createWithDouble(kHaveEnoughDataThreshold));
    PlatformTimeRanges neededBufferedRange { currentTime, std::max(currentTime, aheadTime) };
    setReadyState(m_buffered.containWithEpsilon(neededBufferedRange, MediaTime(2002, 24000)) ? MediaPlayer::ReadyState::HaveEnoughData : MediaPlayer::ReadyState::HaveFutureData);
}

} // namespace WebCore

#endif // ENABLE(COCOA_WEBM_PLAYER)
