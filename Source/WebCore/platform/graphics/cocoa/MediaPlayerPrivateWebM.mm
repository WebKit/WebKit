/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(ALTERNATE_WEBM_PLAYER)

#import "AudioTrackPrivateWebM.h"
#import "FloatSize.h"
#import "GraphicsContext.h"
#import "GraphicsContextStateSaver.h"
#import "IOSurface.h"
#import "Logging.h"
#import "MediaPlaybackTarget.h"
#import "MediaPlayer.h"
#import "MediaSampleAVFObjC.h"
#import "MediaSessionManagerCocoa.h"
#import "NativeImage.h"
#import "NotImplemented.h"
#import "PixelBufferConformerCV.h"
#import "PlatformMediaResourceLoader.h"
#import "ResourceError.h"
#import "ResourceRequest.h"
#import "ResourceResponse.h"
#import "SampleMap.h"
#import "SecurityOrigin.h"
#import "TextTrackRepresentation.h"
#import "TrackBuffer.h"
#import "VideoFrameCV.h"
#import "VideoLayerManagerObjC.h"
#import "VideoMediaSampleRenderer.h"
#import "VideoTrackPrivateWebM.h"
#import "WebCoreDecompressionSession.h"
#import "WebMResourceClient.h"
#import "WebSampleBufferVideoRendering.h"
#import <AVFoundation/AVFoundation.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
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

@interface AVSampleBufferDisplayLayer (Staging_100128644)
@property (assign, nonatomic) BOOL preventsAutomaticBackgroundingDuringVideoPlayback;
@end
#if ENABLE(LINEAR_MEDIA_PLAYER)
@interface AVSampleBufferVideoRenderer (Staging_127455709)
- (void)removeAllVideoTargets;
@end
#endif

#pragma mark -

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaPlayerPrivateWebM);

static const MediaTime discontinuityTolerance = MediaTime(1, 1);

MediaPlayerPrivateWebM::MediaPlayerPrivateWebM(MediaPlayer* player)
    : m_player(player)
    , m_synchronizer(adoptNS([PAL::allocAVSampleBufferRenderSynchronizerInstance() init]))
    , m_parser(SourceBufferParserWebM::create().releaseNonNull())
    , m_appendQueue(WorkQueue::create("MediaPlayerPrivateWebM data parser queue"_s))
    , m_logger(player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
    , m_videoLayerManager(makeUnique<VideoLayerManagerObjC>(m_logger, m_logIdentifier))
    , m_listener(WebAVSampleBufferListener::create(*this))
    , m_seekTimer(*this, &MediaPlayerPrivateWebM::seekInternal)
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

    // addPeriodicTimeObserverForInterval: throws an exception if you pass a non-numeric CMTime, so just use
    // an arbitrarily large time value of once an hour:
    __block WeakPtr weakThis { *this };
    m_timeJumpedObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::toCMTime(MediaTime::createWithDouble(3600)) queue:dispatch_get_main_queue() usingBlock:^(CMTime time) {
#if LOG_DISABLED
        UNUSED_PARAM(time);
#endif

        if (!weakThis)
            return;

        auto clampedTime = CMTIME_IS_NUMERIC(time) ? clampTimeToLastSeekTime(PAL::toMediaTime(time)) : MediaTime::zeroTime();
        ALWAYS_LOG(LOGIDENTIFIER, "synchronizer fired: time clamped = ", clampedTime, ", seeking = ", m_isSynchronizerSeeking, ", pending = ", !!m_pendingSeek);

        if (m_isSynchronizerSeeking && !m_pendingSeek) {
            m_isSynchronizerSeeking = false;
            maybeCompleteSeek();
        }
    }];
    ALWAYS_LOG(LOGIDENTIFIER, "synchronizer initial rate:", [m_synchronizer rate]);
    [m_synchronizer setRate:0];
#if ENABLE(LINEAR_MEDIA_PLAYER)
    setVideoTarget(player->videoTarget());
#endif

#if HAVE(SPATIAL_TRACKING_LABEL)
    m_defaultSpatialTrackingLabel = player->defaultSpatialTrackingLabel();
    m_spatialTrackingLabel = player->spatialTrackingLabel();
#endif
}

MediaPlayerPrivateWebM::~MediaPlayerPrivateWebM()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_durationObserver)
        [m_synchronizer removeTimeObserver:m_durationObserver.get()];
    if (m_videoFrameMetadataGatheringObserver)
        [m_synchronizer removeTimeObserver:m_videoFrameMetadataGatheringObserver.get()];
    if (m_timeJumpedObserver)
        [m_synchronizer removeTimeObserver:m_timeJumpedObserver.get()];

    destroyLayer();
    destroyDecompressionSession();
    destroyAudioRenderers();
    m_listener->invalidate();

    clearTracks();

    cancelLoad();
}

#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)

static bool isCopyDisplayedPixelBufferAvailable()
{
    static NeverDestroyed<std::optional<bool>> result;
    if (!result->has_value())
        result.get() = [PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(copyDisplayedPixelBuffer)];
    return MediaSessionManagerCocoa::mediaSourceInlinePaintingEnabled() && result.get();
}

#endif // HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)

static HashSet<String>& mimeTypeCache()
{
    static NeverDestroyed cache = HashSet<String>();
    if (cache->isEmpty()) {
        auto types = SourceBufferParserWebM::supportedMIMETypes();
        cache->add(types.begin(), types.end());
    }
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
    
    return SourceBufferParserWebM::isContentTypeSupported(parameters.type);
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

    auto player = m_player.get();
    if (!player)
        return;

    auto mimeType = player->contentMIMEType();
    if (mimeType.isEmpty() || !mimeTypeCache().contains(mimeType)) {
        ERROR_LOG(LOGIDENTIFIER, "mime type = ", mimeType, " not supported");
        setNetworkState(MediaPlayer::NetworkState::FormatError);
        return;
    }

    if (m_preload >= MediaPlayer::Preload::MetaData && !m_resourceClient) {
        if (!createResourceClient()) {
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

void MediaPlayerPrivateWebM::load(const String& url)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    setReadyState(MediaPlayer::ReadyState::HaveNothing);

    m_assetURL = URL({ }, url);

    doPreload();
}

bool MediaPlayerPrivateWebM::createResourceClient()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    RefPtr player = m_player.get();
    if (!player)
        return false;

    ResourceRequest request(m_assetURL);
    request.setAllowCookies(true);
    if (m_contentReceived) {
        if (m_contentLength <= m_contentReceived)
            return false;
        request.addHTTPHeaderField(HTTPHeaderName::Range, makeString("bytes="_s, m_contentReceived, '-', m_contentLength));
    }

    m_resourceClient = WebMResourceClient::create(*this, player->mediaResourceLoader(), WTFMove(request));

    return !!m_resourceClient;
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateWebM::load(const URL&, const ContentType&, MediaSourcePrivateClient&)
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

    // FIXME: Remove const_cast once https://bugs.webkit.org/show_bug.cgi?id=243370 is fixed.
    SourceBufferParser::Segment segment(Ref { const_cast<SharedBuffer&>(buffer) });
    invokeAsync(m_appendQueue, [segment = WTFMove(segment), parser = m_parser]() mutable {
        return MediaPromise::createAndSettle(parser->appendData(WTFMove(segment)));
    })->whenSettled(RunLoop::main(), [weakThis = ThreadSafeWeakPtr { *this }](auto&& result) {
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
    if (m_resourceClient) {
        m_resourceClient->stop();
        m_resourceClient = nullptr;
    }
    setNetworkState(MediaPlayer::NetworkState::Idle);
}

PlatformLayer* MediaPlayerPrivateWebM::platformLayer() const
{
    if (!m_videoRenderer)
        return nullptr;
    return m_videoLayerManager->videoInlineLayer();
}

void MediaPlayerPrivateWebM::prepareToPlay()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    setPreload(MediaPlayer::Preload::Auto);
}

void MediaPlayerPrivateWebM::play()
{
#if PLATFORM(IOS_FAMILY)
    flushIfNeeded();
#endif

    m_isPlaying = true;
    if (!shouldBePlaying())
        return;

    [m_synchronizer setRate:m_rate];

    if (currentTime() >= duration())
        seekToTarget(SeekTarget::zero());
}

void MediaPlayerPrivateWebM::pause()
{
    m_isPlaying = false;
    [m_synchronizer setRate:0];
}

bool MediaPlayerPrivateWebM::paused() const
{
    return !m_isPlaying;
}

bool MediaPlayerPrivateWebM::timeIsProgressing() const
{
    return m_isPlaying && [m_synchronizer rate];
}

void MediaPlayerPrivateWebM::setPageIsVisible(bool visible)
{
    if (m_visible == visible)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, visible);
    m_visible = visible;

#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif
}

MediaTime MediaPlayerPrivateWebM::currentTime() const
{
    MediaTime synchronizerTime = clampTimeToLastSeekTime(PAL::toMediaTime(PAL::CMTimebaseGetTime([m_synchronizer timebase])));
    if (synchronizerTime < MediaTime::zeroTime())
        return MediaTime::zeroTime();

    return synchronizerTime;
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

    auto pendingSeek = std::exchange(m_pendingSeek, { }).value();
    m_lastSeekTime = pendingSeek.time;

    m_seekState = Seeking;

    MediaTime synchronizerTime = PAL::toMediaTime([m_synchronizer currentTime]);

    m_isSynchronizerSeeking = synchronizerTime != m_lastSeekTime;
    ALWAYS_LOG(LOGIDENTIFIER, "seekedTime = ", m_lastSeekTime, ", synchronizerTime = ", synchronizerTime, "synchronizer seeking = ", m_isSynchronizerSeeking);

    if (!m_isSynchronizerSeeking) {
        // In cases where the destination seek time precisely matches the synchronizer's existing time
        // no time jumped notification will be issued. In this case, just notify the MediaPlayer that
        // the seek completed successfully.
        maybeCompleteSeek();
        return;
    }

    flush();
    [m_synchronizer setRate:0 time:PAL::toCMTime(m_lastSeekTime)];

    for (auto& trackBufferPair : m_trackBufferMap) {
        TrackBuffer& trackBuffer = trackBufferPair.second;
        auto trackId = trackBufferPair.first;

        trackBuffer.setNeedsReenqueueing(true);
        reenqueueMediaForTime(trackBuffer, trackId, m_lastSeekTime);
    }

    maybeCompleteSeek();
}

void MediaPlayerPrivateWebM::maybeCompleteSeek()
{
    if (m_seekState == SeekCompleted)
        return;
    if (hasVideo() && !m_hasAvailableVideoFrame) {
        ALWAYS_LOG(LOGIDENTIFIER, "waiting for video frame");
        m_seekState = WaitingForAvailableFame;
        return;
    }
    m_seekState = Seeking;
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_isSynchronizerSeeking) {
        ALWAYS_LOG(LOGIDENTIFIER, "Synchronizer still seeking, bailing out");
        return;
    }
    m_seekState = SeekCompleted;
    if (shouldBePlaying())
        [m_synchronizer setRate:m_rate];
    if (auto player = m_player.get()) {
        player->seeked(m_lastSeekTime);
        player->timeChanged();
    }
}

bool MediaPlayerPrivateWebM::seeking() const
{
    return m_pendingSeek || m_seekState != SeekCompleted;
}

MediaTime MediaPlayerPrivateWebM::clampTimeToLastSeekTime(const MediaTime& time) const
{
    if (m_lastSeekTime.isFinite() && time < m_lastSeekTime)
        return m_lastSeekTime;

    return time;
}

bool MediaPlayerPrivateWebM::shouldBePlaying() const
{
    return m_isPlaying && !seeking();
}

void MediaPlayerPrivateWebM::setRateDouble(double rate)
{
    if (rate == m_rate)
        return;

    m_rate = std::max<double>(rate, 0);

    if (shouldBePlaying())
        [m_synchronizer setRate:m_rate];

    if (auto player = m_player.get())
        player->rateChanged();
}

double MediaPlayerPrivateWebM::effectiveRate() const
{
    return PAL::CMTimebaseGetRate([m_synchronizer timebase]);
}

void MediaPlayerPrivateWebM::setVolume(float volume)
{
    for (auto& pair : m_audioRenderers) {
        auto& renderer = pair.second;
        [renderer setVolume:volume];
    }
}

void MediaPlayerPrivateWebM::setMuted(bool muted)
{
    for (auto& pair : m_audioRenderers) {
        auto& renderer = pair.second;
        [renderer setMuted:muted];
    }
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
    if (auto player = m_player.get()) {
        player->bufferedTimeRangesChanged();
        player->seekableTimeRangesChanged();
    }
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

bool MediaPlayerPrivateWebM::updateLastPixelBuffer()
{
#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
    if (m_videoRenderer && isCopyDisplayedPixelBufferAvailable()) {
        if (RetainPtr pixelBuffer = m_videoRenderer->copyDisplayedPixelBuffer()) {
            INFO_LOG(LOGIDENTIFIER, "displayed pixelbuffer copied for time ", currentTime());
            m_lastPixelBuffer = WTFMove(pixelBuffer);
            return true;
        }
    }
#endif

    if (m_videoRenderer || !m_decompressionSession)
        return false;

    auto flags = !m_lastPixelBuffer ? WebCoreDecompressionSession::AllowLater : WebCoreDecompressionSession::ExactTime;
    auto newPixelBuffer = m_decompressionSession->imageForTime(currentTime(), flags);
    if (!newPixelBuffer)
        return false;

    m_lastPixelBuffer = WTFMove(newPixelBuffer);

    if (m_resourceOwner) {
        if (auto surface = CVPixelBufferGetIOSurface(m_lastPixelBuffer.get()))
            IOSurface::setOwnershipIdentity(surface, m_resourceOwner);
    }

    return true;
}

bool MediaPlayerPrivateWebM::updateLastImage()
{
    if (m_isGatheringVideoFrameMetadata) {
        if (!m_lastPixelBuffer)
            return false;
        if (m_sampleCount == m_lastConvertedSampleCount)
            return false;
        m_lastConvertedSampleCount = m_sampleCount;
    } else if (!updateLastPixelBuffer())
        return false;

    ASSERT(m_lastPixelBuffer);

    if (!m_rgbConformer) {
        auto attributes = @{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) };
        m_rgbConformer = makeUnique<PixelBufferConformerCV>((__bridge CFDictionaryRef)attributes);
    }

    m_lastImage = NativeImage::create(m_rgbConformer->createImageFromPixelBuffer(m_lastPixelBuffer.get()));
    return true;
}

void MediaPlayerPrivateWebM::paint(GraphicsContext& context, const FloatRect& rect)
{
    paintCurrentFrameInContext(context, rect);
}

void MediaPlayerPrivateWebM::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& outputRect)
{
    if (context.paintingDisabled())
        return;

    auto image = nativeImageForCurrentTime();
    if (!image)
        return;

    GraphicsContextStateSaver stateSaver(context);
    FloatRect imageRect { FloatPoint::zero(), image->size() };
    context.drawNativeImage(*image, outputRect, imageRect);
}

#if !HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
void MediaPlayerPrivateWebM::willBeAskedToPaintGL()
{
    // We have been asked to paint into a WebGL canvas, so take that as a signal to create
    // a decompression session, even if that means the native video can't also be displayed
    // in page.
    if (m_hasBeenAskedToPaintGL)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);
    m_hasBeenAskedToPaintGL = true;
    acceleratedRenderingStateChanged();
}
#endif

RefPtr<VideoFrame> MediaPlayerPrivateWebM::videoFrameForCurrentTime()
{
    if (!m_isGatheringVideoFrameMetadata) {
        // FIXME: This method is synchronous in order to
        // work around https://bugs.webkit.org/show_bug.cgi?id=228997
        // on builds without AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER
        const auto shouldWaitForFrame = m_hasAvailableVideoFrameSemaphore && m_decompressionSession;
        if (shouldWaitForFrame)
            m_hasAvailableVideoFrameSemaphore->waitFor(100_ms);
        updateLastPixelBuffer();
    }
    if (!m_lastPixelBuffer)
        return nullptr;
    return VideoFrameCV::create(currentTime(), false, VideoFrame::Rotation::None, RetainPtr { m_lastPixelBuffer });
}

DestinationColorSpace MediaPlayerPrivateWebM::colorSpace()
{
    updateLastImage();
    return m_lastImage ? m_lastImage->colorSpace() : DestinationColorSpace::SRGB();
}

void MediaPlayerPrivateWebM::setNaturalSize(FloatSize size)
{
    auto oldSize = m_naturalSize;
    m_naturalSize = size;
    if (oldSize != m_naturalSize) {
        INFO_LOG(LOGIDENTIFIER, "was ", oldSize, ", is ", size);
        if (auto player = m_player.get())
            player->sizeChanged();
    }
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

    if (auto player = m_player.get())
        player->firstVideoFrameAvailable();

    if (m_seekState == WaitingForAvailableFame)
        maybeCompleteSeek();

    setReadyState(MediaPlayer::ReadyState::HaveEnoughData);
}

void MediaPlayerPrivateWebM::setDuration(MediaTime duration)
{
    if (duration == m_duration)
        return;

    if (m_durationObserver)
        [m_synchronizer removeTimeObserver:m_durationObserver.get()];

    NSArray* times = @[[NSValue valueWithCMTime:PAL::toCMTime(duration)]];

    auto logSiteIdentifier = LOGIDENTIFIER;
    DEBUG_LOG(logSiteIdentifier, duration);
    UNUSED_PARAM(logSiteIdentifier);

    m_durationObserver = [m_synchronizer addBoundaryTimeObserverForTimes:times queue:dispatch_get_main_queue() usingBlock:[weakThis = ThreadSafeWeakPtr { *this }, duration, logSiteIdentifier, this] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        MediaTime now = currentTime();
        ALWAYS_LOG(logSiteIdentifier, "boundary time observer called, now = ", now);

        pause();
        if (now < duration) {
            ERROR_LOG(logSiteIdentifier, "ERROR: boundary time observer called before duration");
            [m_synchronizer setRate:0 time:PAL::toCMTime(duration)];
        }
        if (auto player = m_player.get())
            player->timeChanged();

    }];

    m_duration = WTFMove(duration);
    if (auto player = m_player.get())
        player->durationChanged();
}

void MediaPlayerPrivateWebM::setNetworkState(MediaPlayer::NetworkState state)
{
    if (state == m_networkState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, state);
    m_networkState = state;
    if (auto player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateWebM::setReadyState(MediaPlayer::ReadyState state)
{
    if (state == m_readyState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, state);
    m_readyState = state;
    
    if (auto player = m_player.get())
        player->readyStateChanged();
}

void MediaPlayerPrivateWebM::characteristicsChanged()
{
    if (auto player = m_player.get())
        player->characteristicChanged();
}

bool MediaPlayerPrivateWebM::shouldEnsureLayerOrVideoRenderer() const
{
#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
    auto player = m_player.get();
    return isCopyDisplayedPixelBufferAvailable()
        && ((m_videoRenderer && !CGRectIsEmpty(m_videoRenderer->bounds()))
            || (player && !player->presentationSize().isEmpty()));
#else
    return !m_hasBeenAskedToPaintGL;
#endif
}

void MediaPlayerPrivateWebM::setPresentationSize(const IntSize& newSize)
{
    if (m_hasVideo && !m_videoRenderer && !newSize.isEmpty())
        updateDisplayLayerAndDecompressionSession();
}

void MediaPlayerPrivateWebM::acceleratedRenderingStateChanged()
{
    if (m_hasVideo)
        updateDisplayLayerAndDecompressionSession();
}

void MediaPlayerPrivateWebM::updateDisplayLayerAndDecompressionSession()
{
    if (shouldEnsureLayerOrVideoRenderer()) {
        auto needsRenderingModeChanged = destroyDecompressionSession();
        ensureLayerOrVideoRenderer(needsRenderingModeChanged);
        return;
    }

    destroyLayerOrVideoRenderer();
    ensureDecompressionSession();
}

RetainPtr<PlatformLayer> MediaPlayerPrivateWebM::createVideoFullscreenLayer()
{
    return adoptNS([[CALayer alloc] init]);
}

void MediaPlayerPrivateWebM::setVideoFullscreenLayer(PlatformLayer *videoFullscreenLayer, WTF::Function<void()>&& completionHandler)
{
    updateLastImage();
    m_videoLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler), m_lastImage ? m_lastImage->platformImage() : nullptr);
}

void MediaPlayerPrivateWebM::setVideoFullscreenFrame(FloatRect frame)
{
    m_videoLayerManager->setVideoFullscreenFrame(frame);
}

void MediaPlayerPrivateWebM::syncTextTrackBounds()
{
    m_videoLayerManager->syncTextTrackBounds();
}

void MediaPlayerPrivateWebM::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    auto* representationLayer = representation ? representation->platformLayer() : nil;
    m_videoLayerManager->setTextTrackRepresentationLayer(representationLayer);
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

    if (auto player = m_player.get())
        player->currentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless());
}

bool MediaPlayerPrivateWebM::isCurrentPlaybackTargetWireless() const
{
    if (!m_playbackTarget)
        return false;

    auto hasTarget = m_shouldPlayToTarget && m_playbackTarget->hasActiveRoute();
    INFO_LOG(LOGIDENTIFIER, hasTarget);
    return hasTarget;
}
#endif

void MediaPlayerPrivateWebM::enqueueSample(Ref<MediaSample>&& sample, TrackID trackId)
{
    if (!isEnabledVideoTrackID(trackId) && !m_audioRenderers.contains(trackId))
        return;

    auto logSiteIdentifier = LOGIDENTIFIER;
    DEBUG_LOG(logSiteIdentifier, "track ID = ", trackId, ", sample = ", sample.get());

    PlatformSample platformSample = sample->platformSample();

    CMFormatDescriptionRef formatDescription = PAL::CMSampleBufferGetFormatDescription(platformSample.sample.cmSampleBuffer);
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

        FloatSize formatSize = FloatSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(formatDescription, true, true));
        if (formatSize != m_naturalSize)
            setNaturalSize(formatSize);

        if (m_decompressionSession)
            m_decompressionSession->enqueueSample(platformSample.sample.cmSampleBuffer, !sample->isNonDisplaying());

        if (!m_videoRenderer)
            return;

        m_videoRenderer->enqueueSample(platformSample.sample.cmSampleBuffer, !sample->isNonDisplaying());
        WebSampleBufferVideoRendering *renderer = m_videoRenderer->renderer();
#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_READYFORDISPLAY)
        if (AVSampleBufferDisplayLayer *displayLayer = m_videoRenderer->displayLayer()) {
            // FIXME (117934497): Remove staging code once -[AVSampleBufferDisplayLayer isReadyForDisplay] is available in SDKs used by WebKit builders
            if ([displayLayer respondsToSelector:@selector(isReadyForDisplay)])
                return;
        }
#endif
        if (m_hasAvailableVideoFrame || sample->isNonDisplaying())
            return;

        DEBUG_LOG(LOGIDENTIFIER, "adding buffer attachment");

        [renderer prerollDecodeWithCompletionHandler:[this, weakThis = ThreadSafeWeakPtr { *this }, logSiteIdentifier = LOGIDENTIFIER] (BOOL success) mutable {
            ensureOnMainThread([this, weakThis = WTFMove(weakThis), logSiteIdentifier, success] () {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;

                if (!success || !m_videoRenderer) {
                    ERROR_LOG(logSiteIdentifier, "prerollDecodeWithCompletionHandler failed");
                    return;
                }

                videoRendererReadyForDisplayChanged(m_videoRenderer->renderer(), true);
            });
        }];

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

    auto itRenderer = m_audioRenderers.find(trackId);
    ASSERT(itRenderer != m_audioRenderers.end());
    [itRenderer->second enqueueSampleBuffer:platformSample.sample.cmSampleBuffer];
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

void MediaPlayerPrivateWebM::reenqueueMediaForTime(TrackBuffer& trackBuffer, TrackID trackId, const MediaTime& time, NeedsFlush needsFlush)
{
    if (needsFlush == NeedsFlush::Yes)
        flushTrack(trackId);
    if (trackBuffer.reenqueueMediaForTime(time, timeFudgeFactor()))
        provideMediaData(trackBuffer, trackId);
}

void MediaPlayerPrivateWebM::notifyClientWhenReadyForMoreSamples(TrackID trackId)
{
    if (isEnabledVideoTrackID(trackId)) {
        if (m_decompressionSession) {
            m_decompressionSession->requestMediaDataWhenReady([weakThis = ThreadSafeWeakPtr { *this }, this, trackId] {
                if (RefPtr protectedThis = weakThis.get())
                    didBecomeReadyForMoreSamples(trackId);
            });
        }
        if (m_videoRenderer) {
            m_videoRenderer->requestMediaDataWhenReady([weakThis = ThreadSafeWeakPtr { *this }, this, trackId] {
                if (RefPtr protectedThis = weakThis.get())
                    didBecomeReadyForMoreSamples(trackId);
            });
        }
        return;
    }
    
    if (auto itAudioRenderer = m_audioRenderers.find(trackId); itAudioRenderer != m_audioRenderers.end()) {
        ThreadSafeWeakPtr weakThis { *this };
        [itAudioRenderer->second requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
            if (RefPtr protectedThis = weakThis.get())
                didBecomeReadyForMoreSamples(trackId);
        }];
    }
}

void MediaPlayerPrivateWebM::setMinimumUpcomingPresentationTime(TrackID trackId, const MediaTime& presentationTime)
{
    if (isEnabledVideoTrackID(trackId) && m_videoRenderer)
        m_videoRenderer->expectMinimumUpcomingSampleBufferPresentationTime(presentationTime);
}

void MediaPlayerPrivateWebM::clearMinimumUpcomingPresentationTime(TrackID trackId)
{
    if (isEnabledVideoTrackID(trackId) && m_videoRenderer)
        m_videoRenderer->resetUpcomingSampleBufferPresentationTimeExpectations();
}

bool MediaPlayerPrivateWebM::isReadyForMoreSamples(TrackID trackId)
{
    if (isEnabledVideoTrackID(trackId)) {
#if PLATFORM(IOS_FAMILY)
        if (m_displayLayerWasInterrupted)
            return false;
#endif
        if (m_decompressionSession)
            return m_decompressionSession->isReadyForMoreMediaData();
        
        return m_videoRenderer->isReadyForMoreMediaData();
    }

    if (auto itAudioRenderer = m_audioRenderers.find(trackId); itAudioRenderer != m_audioRenderers.end())
        return [itAudioRenderer->second isReadyForMoreMediaData];

    return false;
}

void MediaPlayerPrivateWebM::didBecomeReadyForMoreSamples(TrackID trackId)
{
    INFO_LOG(LOGIDENTIFIER, trackId);

    if (isEnabledVideoTrackID(trackId)) {
        if (m_decompressionSession)
            m_decompressionSession->stopRequestingMediaData();
        if (m_videoRenderer)
            m_videoRenderer->stopRequestingMediaData();
    } else if (auto itAudioRenderer = m_audioRenderers.find(trackId); itAudioRenderer != m_audioRenderers.end())
        [itAudioRenderer->second stopRequestingMediaData];
    else
        return;

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
    auto it = m_trackBufferMap.find(trackId);
    if (it == m_trackBufferMap.end())
        return;

    provideMediaData(it->second, trackId);
}

void MediaPlayerPrivateWebM::provideMediaData(TrackBuffer& trackBuffer, TrackID trackId)
{
    if (m_errored)
        return;

    unsigned enqueuedSamples = 0;

    if (trackBuffer.needsMinimumUpcomingPresentationTimeUpdating() && isEnabledVideoTrackID(trackId)) {
        trackBuffer.setMinimumEnqueuedPresentationTime(MediaTime::invalidTime());
        clearMinimumUpcomingPresentationTime(trackId);
    }

    while (!trackBuffer.decodeQueue().empty()) {
        if (!isReadyForMoreSamples(trackId)) {
            DEBUG_LOG(LOGIDENTIFIER, "bailing early, track id ", trackId, " is not ready for more data");
            notifyClientWhenReadyForMoreSamples(trackId);
            break;
        }

        Ref sample = trackBuffer.decodeQueue().begin()->second;

        if (sample->decodeTime() > trackBuffer.enqueueDiscontinuityBoundary()) {
            DEBUG_LOG(LOGIDENTIFIER, "bailing early because of unbuffered gap, new sample: ", sample->decodeTime(), " >= the current discontinuity boundary: ", trackBuffer.enqueueDiscontinuityBoundary());
            break;
        }

        // Remove the sample from the decode queue now.
        trackBuffer.decodeQueue().erase(trackBuffer.decodeQueue().begin());

        MediaTime samplePresentationEnd = sample->presentationTime() + sample->duration();
        if (trackBuffer.highestEnqueuedPresentationTime().isInvalid() || samplePresentationEnd > trackBuffer.highestEnqueuedPresentationTime())
            trackBuffer.setHighestEnqueuedPresentationTime(WTFMove(samplePresentationEnd));

        trackBuffer.setLastEnqueuedDecodeKey({ sample->decodeTime(), sample->presentationTime() });
        trackBuffer.setEnqueueDiscontinuityBoundary(sample->decodeTime() + sample->duration() + discontinuityTolerance);

        enqueueSample(WTFMove(sample), trackId);
        ++enqueuedSamples;
    }

    if (isEnabledVideoTrackID(trackId) && trackBuffer.updateMinimumUpcomingPresentationTime())
        setMinimumUpcomingPresentationTime(trackId, trackBuffer.minimumEnqueuedPresentationTime());

    DEBUG_LOG(LOGIDENTIFIER, "enqueued ", enqueuedSamples, " samples, ", static_cast<uint64_t>(trackBuffer.decodeQueue().size()), " remaining");
}

void MediaPlayerPrivateWebM::trackDidChangeSelected(VideoTrackPrivate& track, bool selected)
{
    auto trackId = track.id();

    if (!m_trackBufferMap.contains(trackId))
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "video trackID = ", trackId, ", selected = ", selected);

    if (selected) {
        m_enabledVideoTrackID = trackId;
        updateDisplayLayerAndDecompressionSession();
        return;
    }
    
    if (isEnabledVideoTrackID(trackId)) {
        m_enabledVideoTrackID.reset();
        m_readyForMoreSamplesMap.erase(trackId);
        if (m_videoRenderer)
            m_videoRenderer->stopRequestingMediaData();
        if (m_decompressionSession)
            m_decompressionSession->stopRequestingMediaData();
    }
}

void MediaPlayerPrivateWebM::trackDidChangeEnabled(AudioTrackPrivate& track, bool enabled)
{
    auto trackId = track.id();

    if (!m_trackBufferMap.contains(trackId))
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "audio trackID = ", trackId, ", enabled = ", enabled);

    if (enabled) {
        addAudioRenderer(trackId);
        m_readyForMoreSamplesMap[trackId] = true;
        return;
    }

    m_readyForMoreSamplesMap.erase(trackId);
    removeAudioRenderer(trackId);
}

void MediaPlayerPrivateWebM::didParseInitializationData(InitializationSegment&& segment)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_preload == MediaPlayer::Preload::MetaData)
        cancelLoad();

    clearTracks();

    if (segment.duration.isValid())
        setDuration(WTFMove(segment.duration));
    else
        setDuration(MediaTime::positiveInfiniteTime());

    auto player = m_player.get();
    for (auto videoTrackInfo : segment.videoTracks) {
        if (videoTrackInfo.track) {
            auto track = static_pointer_cast<VideoTrackPrivateWebM>(videoTrackInfo.track);
#if PLATFORM(IOS_FAMILY)
            if (shouldCheckHardwareSupport() && (videoTrackInfo.description->codec() == "vp8"_s || (videoTrackInfo.description->codec() == "vp9"_s && !(canLoad_VideoToolbox_VTIsHardwareDecodeSupported() && VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9))))) {
                m_errored = true;
                return;
            }
#endif
            addTrackBuffer(track->id(), WTFMove(videoTrackInfo.description));

            track->setSelectedChangedCallback([weakThis = ThreadSafeWeakPtr { *this }, this] (VideoTrackPrivate& track, bool selected) {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;

                auto videoTrackSelectedChanged = [weakThis, this, trackRef = Ref { track }, selected] {
                    if (RefPtr protectedThis = weakThis.get())
                        trackDidChangeSelected(trackRef, selected);
                };

                if (!m_processingInitializationSegment) {
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
            auto track = static_pointer_cast<AudioTrackPrivateWebM>(audioTrackInfo.track);
            addTrackBuffer(track->id(), WTFMove(audioTrackInfo.description));

            track->setEnabledChangedCallback([weakThis = ThreadSafeWeakPtr { *this }, this] (AudioTrackPrivate& track, bool enabled) {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;

                auto audioTrackEnabledChanged = [weakThis, this, trackRef = Ref { track }, enabled] {
                    if (RefPtr protectedThis = weakThis.get())
                        trackDidChangeEnabled(trackRef, enabled);
                };

                if (!m_processingInitializationSegment) {
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
}

void MediaPlayerPrivateWebM::didProvideMediaDataForTrackId(Ref<MediaSampleAVFObjC>&& originalSample, TrackID trackId, const String& mediaType)
{
    UNUSED_PARAM(mediaType);

    auto it = m_trackBufferMap.find(trackId);
    if (it == m_trackBufferMap.end())
        return;
    TrackBuffer& trackBuffer = it->second;
    Ref sample = WTFMove(originalSample);

    MediaTime microsecond(1, 1000000);
    if (!trackBuffer.roundedTimestampOffset().isValid())
        trackBuffer.setRoundedTimestampOffset(-sample->presentationTime(), sample->presentationTime().timeScale(), microsecond);

    sample->offsetTimestampsBy(trackBuffer.roundedTimestampOffset());
    trackBuffer.samples().addSample(sample);

    DecodeOrderSampleMap::KeyType decodeKey(sample->decodeTime(), sample->presentationTime());
    trackBuffer.decodeQueue().insert(DecodeOrderSampleMap::MapType::value_type(decodeKey, sample));

    trackBuffer.setLastDecodeTimestamp(sample->decodeTime());
    trackBuffer.setLastFrameDuration(sample->duration());

    auto presentationTimestamp = sample->presentationTime();
    auto presentationEndTime = presentationTimestamp + sample->duration();
    if (trackBuffer.highestPresentationTimestamp().isInvalid() || presentationEndTime > trackBuffer.highestPresentationTimestamp())
        trackBuffer.setHighestPresentationTimestamp(presentationEndTime);

    // Eliminate small gaps between buffered ranges by coalescing
    // disjoint ranges separated by less than a "fudge factor".
    auto nearestToPresentationStartTime = trackBuffer.buffered().nearest(presentationTimestamp);
    if (nearestToPresentationStartTime.isValid() && (presentationTimestamp - nearestToPresentationStartTime).isBetween(MediaTime::zeroTime(), timeFudgeFactor()))
        presentationTimestamp = nearestToPresentationStartTime;

    auto nearestToPresentationEndTime = trackBuffer.buffered().nearest(presentationEndTime);
    if (nearestToPresentationEndTime.isValid() && (nearestToPresentationEndTime - presentationEndTime).isBetween(MediaTime::zeroTime(), timeFudgeFactor()))
        presentationEndTime = nearestToPresentationEndTime;

    trackBuffer.addBufferedRange(presentationTimestamp, presentationEndTime);

    if (m_preload <= MediaPlayer::Preload::MetaData) {
        m_readyForMoreSamplesMap[trackId] = true;
        return;
    }
    notifyClientWhenReadyForMoreSamples(trackId);
}

void MediaPlayerPrivateWebM::flush()
{
    if (m_videoTracks.size())
        flushVideo();

    if (!m_audioTracks.size())
        return;

    for (auto& pair : m_audioRenderers) {
        auto& renderer = pair.second;
        flushAudio(renderer.get());
    }
}

#if PLATFORM(IOS_FAMILY)
void MediaPlayerPrivateWebM::flushIfNeeded()
{
    if (!m_displayLayerWasInterrupted)
        return;

    m_displayLayerWasInterrupted = false;
    if (m_videoTracks.size())
        flushVideo();

    // We initiatively enqueue samples instead of waiting for the
    // media data requests from m_decompressionSession and m_displayLayer.
    // In addition, we need to enqueue a sync sample (IDR video frame) first.
    if (m_decompressionSession)
        m_decompressionSession->stopRequestingMediaData();
    if (m_videoRenderer)
        m_videoRenderer->stopRequestingMediaData();

    if (m_enabledVideoTrackID)
        reenqueSamples(*m_enabledVideoTrackID);
}
#endif

void MediaPlayerPrivateWebM::flushTrack(TrackID trackId)
{
    DEBUG_LOG(LOGIDENTIFIER, trackId);

    if (isEnabledVideoTrackID(trackId)) {
        flushVideo();
        return;
    }

    if (auto itAudioRenderer = m_audioRenderers.find(trackId); itAudioRenderer != m_audioRenderers.end())
        flushAudio(itAudioRenderer->second.get());
}

void MediaPlayerPrivateWebM::flushVideo()
{
    DEBUG_LOG(LOGIDENTIFIER);
    if (m_videoRenderer)
        m_videoRenderer->flush();
    
    if (m_decompressionSession) {
        m_decompressionSession->flush();
        if (!m_hasAvailableVideoFrameSemaphore)
            m_hasAvailableVideoFrameSemaphore = makeUnique<BinarySemaphore>();
        registerNotifyWhenHasAvailableVideoFrame();
    }
    setHasAvailableVideoFrame(false);
}

void MediaPlayerPrivateWebM::flushAudio(AVSampleBufferAudioRenderer *renderer)
{
    DEBUG_LOG(LOGIDENTIFIER);
    [renderer flush];
}

void MediaPlayerPrivateWebM::addTrackBuffer(TrackID trackId, RefPtr<MediaDescription>&& description)
{
    ASSERT(!m_trackBufferMap.contains(trackId));

    setHasAudio(m_hasAudio || description->isAudio());
    setHasVideo(m_hasVideo || description->isVideo());

    auto trackBuffer = TrackBuffer::create(WTFMove(description), discontinuityTolerance);
    trackBuffer->setLogger(logger(), logIdentifier());
    m_trackBufferMap.try_emplace(trackId, WTFMove(trackBuffer));
}

void MediaPlayerPrivateWebM::ensureLayer()
{
    if (m_sampleBufferDisplayLayer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_sampleBufferDisplayLayer = adoptNS([PAL::allocAVSampleBufferDisplayLayerInstance() init]);
    if (!m_sampleBufferDisplayLayer)
        return;

    [m_sampleBufferDisplayLayer setName:@"MediaPlayerPrivateWebM AVSampleBufferDisplayLayer"];
    [m_sampleBufferDisplayLayer setVideoGravity: (m_shouldMaintainAspectRatio ? AVLayerVideoGravityResizeAspect : AVLayerVideoGravityResize)];

    configureLayerOrVideoRenderer(m_sampleBufferDisplayLayer.get());

    if (RefPtr player = m_player.get())
        m_videoLayerManager->setVideoLayer(m_sampleBufferDisplayLayer.get(), player->presentationSize());
}

void MediaPlayerPrivateWebM::ensureDecompressionSession()
{
    if (m_decompressionSession)
        return;
    
    m_hasAvailableVideoFrameSemaphore = makeUnique<BinarySemaphore>();

    m_decompressionSession = WebCoreDecompressionSession::createOpenGL();
    m_decompressionSession->setTimebase([m_synchronizer timebase]);
    m_decompressionSession->setResourceOwner(m_resourceOwner);

    registerNotifyWhenHasAvailableVideoFrame();
    
    if (auto player = m_player.get())
        player->renderingModeChanged();
}

void MediaPlayerPrivateWebM::addAudioRenderer(TrackID trackId)
{
    if (m_audioRenderers.contains(trackId))
        return;

    auto renderer = adoptNS([PAL::allocAVSampleBufferAudioRendererInstance() init]);

    if (!renderer) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferAudioRenderer init] returned nil! bailing!");
        ASSERT_NOT_REACHED();

        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

    auto player = m_player.get();
    if (!player)
        return;

    [renderer setMuted:player->muted()];
    [renderer setVolume:player->volume()];
    [renderer setAudioTimePitchAlgorithm:(player->preservesPitch() ? AVAudioTimePitchAlgorithmSpectral : AVAudioTimePitchAlgorithmVarispeed)];

#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    auto deviceId = player->audioOutputDeviceIdOverride();
    if (!deviceId.isNull() && renderer) {
        if (deviceId.isEmpty())
            renderer.get().audioOutputDeviceUniqueID = nil;
        else
            renderer.get().audioOutputDeviceUniqueID = deviceId;
    }
#endif

    @try {
        [m_synchronizer addRenderer:renderer.get()];
    } @catch(NSException *exception) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer addRenderer:] threw an exception: ", exception.name, ", reason : ", exception.reason);
        ASSERT_NOT_REACHED();

        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

    characteristicsChanged();

    m_audioRenderers.try_emplace(trackId, renderer);
    m_listener->beginObservingAudioRenderer(renderer.get());
}

void MediaPlayerPrivateWebM::removeAudioRenderer(TrackID trackId)
{
    auto itRenderer = m_audioRenderers.find(trackId);
    if (itRenderer == m_audioRenderers.end())
        return;
    destroyAudioRenderer(itRenderer->second);
    m_audioRenderers.erase(trackId);
}

void MediaPlayerPrivateWebM::destroyLayer()
{
    if (!m_sampleBufferDisplayLayer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:m_sampleBufferDisplayLayer.get() atTime:currentTime completionHandler:nil];

    m_videoLayerManager->didDestroyVideoLayer();
    m_sampleBufferDisplayLayer = nullptr;
}

MediaPlayerEnums::NeedsRenderingModeChanged MediaPlayerPrivateWebM::destroyDecompressionSession()
{
    if (!m_decompressionSession)
        return MediaPlayerEnums::NeedsRenderingModeChanged::No;

    m_decompressionSession->invalidate();
    m_decompressionSession = nullptr;
    m_hasAvailableVideoFrameSemaphore = nullptr;
    setHasAvailableVideoFrame(false);
    return MediaPlayerEnums::NeedsRenderingModeChanged::Yes;
}

void MediaPlayerPrivateWebM::ensureVideoRenderer()
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (m_sampleBufferVideoRenderer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_sampleBufferVideoRenderer = adoptNS([PAL::allocAVSampleBufferVideoRendererInstance() init]);
    if (!m_sampleBufferVideoRenderer)
        return;

    [m_sampleBufferVideoRenderer addVideoTarget:m_videoTarget.get()];

    configureLayerOrVideoRenderer(m_sampleBufferVideoRenderer.get());
#endif // ENABLE(LINEAR_MEDIA_PLAYER)
}

void MediaPlayerPrivateWebM::destroyVideoRenderer()
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (!m_sampleBufferVideoRenderer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:m_sampleBufferVideoRenderer.get() atTime:currentTime completionHandler:nil];

    if ([m_sampleBufferVideoRenderer respondsToSelector:@selector(removeAllVideoTargets)])
        [m_sampleBufferVideoRenderer removeAllVideoTargets];
    m_sampleBufferVideoRenderer = nullptr;
#endif // ENABLE(LINEAR_MEDIA_PLAYER)
}

void MediaPlayerPrivateWebM::destroyAudioRenderer(RetainPtr<AVSampleBufferAudioRenderer> renderer)
{
    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:renderer.get() atTime:currentTime completionHandler:nil];

    m_listener->stopObservingAudioRenderer(renderer.get());
    [renderer flush];
    [renderer stopRequestingMediaData];
}

void MediaPlayerPrivateWebM::destroyAudioRenderers()
{
    for (auto& pair : m_audioRenderers) {
        auto& renderer = pair.second;
        destroyAudioRenderer(renderer);
    }
    m_audioRenderers.clear();
}

void MediaPlayerPrivateWebM::clearTracks()
{
    auto player = m_player.get();
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

void MediaPlayerPrivateWebM::registerNotifyWhenHasAvailableVideoFrame()
{
    if (!m_decompressionSession)
        return;
    
    m_decompressionSession->notifyWhenHasAvailableVideoFrame([weakThis = WeakPtr { *this }, this] {
        if (weakThis) {
            setHasAvailableVideoFrame(true);
            if (m_hasAvailableVideoFrameSemaphore) {
                m_hasAvailableVideoFrameSemaphore->signal();
                m_hasAvailableVideoFrameSemaphore = nullptr;
            }
        }
    });
}

void MediaPlayerPrivateWebM::startVideoFrameMetadataGathering()
{
    if (m_videoFrameMetadataGatheringObserver)
        return;
    ASSERT(m_synchronizer);
    m_isGatheringVideoFrameMetadata = true;
    acceleratedRenderingStateChanged();

    // FIXME: We should use a CADisplayLink to get updates on rendering, for now we emulate with addPeriodicTimeObserverForInterval.
    m_videoFrameMetadataGatheringObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::CMTimeMake(1, 60) queue:dispatch_get_main_queue() usingBlock:[weakThis = WeakPtr { *this }, this](CMTime currentTime) {
        ensureOnMainThread([weakThis, this, currentTime] {
            if (weakThis)
                checkNewVideoFrameMetadata(currentTime);
        });
    }];
}

void MediaPlayerPrivateWebM::stopVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = false;
    acceleratedRenderingStateChanged();
    m_videoFrameMetadata = { };

    ASSERT(m_videoFrameMetadataGatheringObserver);
    if (m_videoFrameMetadataGatheringObserver)
        [m_synchronizer removeTimeObserver:m_videoFrameMetadataGatheringObserver.get()];
    m_videoFrameMetadataGatheringObserver = nil;
}

void MediaPlayerPrivateWebM::checkNewVideoFrameMetadata(CMTime currentTime)
{
    auto player = m_player.get();
    if (!player)
        return;

    if (!updateLastPixelBuffer())
        return;

    VideoFrameMetadata metadata;
    metadata.width = m_naturalSize.width();
    metadata.height = m_naturalSize.height();
    metadata.presentedFrames = ++m_sampleCount;
    metadata.presentationTime = PAL::CMTimeGetSeconds(currentTime);

    m_videoFrameMetadata = metadata;
    player->onNewVideoFrameMetadata(WTFMove(metadata), m_lastPixelBuffer.get());
}

WTFLogChannel& MediaPlayerPrivateWebM::logChannel() const
{
    return LogMedia;
}

class MediaPlayerFactoryWebM final : public MediaPlayerFactory {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MediaPlayerFactoryWebM);
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::CocoaWebM; };

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer* player) const final
    {
        return adoptRef(*new MediaPlayerPrivateWebM(player));
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
        && PAL::getAVSampleBufferAudioRendererClass()
        && PAL::getAVSampleBufferRenderSynchronizerClass()
        && class_getInstanceMethod(PAL::getAVSampleBufferAudioRendererClass(), @selector(setMuted:));
}

bool MediaPlayerPrivateWebM::isEnabledVideoTrackID(TrackID trackID) const
{
    return m_enabledVideoTrackID && *m_enabledVideoTrackID == trackID;
}

bool MediaPlayerPrivateWebM::hasSelectedVideo() const
{
    return !!m_enabledVideoTrackID;
}

void MediaPlayerPrivateWebM::videoRendererDidReceiveError(WebSampleBufferVideoRendering *renderer, NSError *error)
{
#if PLATFORM(IOS_FAMILY)
    if (renderer.status == AVQueuedSampleBufferRenderingStatusFailed && [error.domain isEqualToString:@"AVFoundationErrorDomain"] && error.code == AVErrorOperationInterrupted) {
        m_displayLayerWasInterrupted = true;
        return;
    }
#else
    UNUSED_PARAM(renderer);
    UNUSED_PARAM(error);
#endif
    setNetworkState(MediaPlayer::NetworkState::DecodeError);
    setReadyState(MediaPlayer::ReadyState::HaveNothing);
    m_errored = true;
}

void MediaPlayerPrivateWebM::audioRendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError*)
{
    setNetworkState(MediaPlayer::NetworkState::DecodeError);
    setReadyState(MediaPlayer::ReadyState::HaveNothing);
    m_errored = true;
}

void MediaPlayerPrivateWebM::videoRendererReadyForDisplayChanged(WebSampleBufferVideoRendering *renderer, bool isReadyForDisplay)
{
    if (!m_videoRenderer || renderer != m_videoRenderer->renderer() || !isReadyForDisplay)
        return;

    auto currentTime = PAL::CMTimebaseGetTime(renderer.timebase);
    ALWAYS_LOG(LOGIDENTIFIER, "m_isSynchronizerSeeking:", m_isSynchronizerSeeking, " layer.basetime:", PAL::toMediaTime(currentTime));

    setHasAvailableVideoFrame(true);
}

void MediaPlayerPrivateWebM::ensureLayerOrVideoRenderer(MediaPlayerEnums::NeedsRenderingModeChanged needsRenderingModeChanged)
{
    switch (acceleratedVideoMode()) {
    case AcceleratedVideoMode::Layer:
        destroyVideoRenderer();
        FALLTHROUGH;
    case AcceleratedVideoMode::StagedLayer:
        ensureLayer();
        break;
    case AcceleratedVideoMode::VideoRenderer:
        destroyLayer();
        FALLTHROUGH;
    case AcceleratedVideoMode::StagedVideoRenderer:
        ensureVideoRenderer();
        break;
    }

    RetainPtr renderer = layerOrVideoRenderer();

    if (!renderer) {
        ERROR_LOG(LOGIDENTIFIER, "Failed to create AVSampleBufferDisplayLayer or AVSampleBufferVideoRenderer");
        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER, acceleratedVideoMode(), ", renderer=", !!renderer);

    switch (acceleratedVideoMode()) {
    case AcceleratedVideoMode::Layer:
#if ENABLE(LINEAR_MEDIA_PLAYER)
        if (!m_usingLinearMediaPlayer)
            needsRenderingModeChanged = MediaPlayerEnums::NeedsRenderingModeChanged::Yes;
#else
        needsRenderingModeChanged = MediaPlayerEnums::NeedsRenderingModeChanged::Yes;
#endif
        FALLTHROUGH;
    case AcceleratedVideoMode::VideoRenderer:
        setVideoRenderer(renderer.get());
        break;
    case AcceleratedVideoMode::StagedLayer:
        needsRenderingModeChanged = MediaPlayerEnums::NeedsRenderingModeChanged::Yes;
        FALLTHROUGH;
    case AcceleratedVideoMode::StagedVideoRenderer:
        stageVideoRenderer(renderer.get());
        break;
    }

    switch (needsRenderingModeChanged) {
    case MediaPlayerEnums::NeedsRenderingModeChanged::Yes:
        if (RefPtr player = m_player.get())
            player->renderingModeChanged();
        break;
    case MediaPlayerEnums::NeedsRenderingModeChanged::No:
        break;
    }
}

void MediaPlayerPrivateWebM::setShouldDisableHDR(bool shouldDisable)
{
    if (![m_sampleBufferDisplayLayer respondsToSelector:@selector(setToneMapToStandardDynamicRange:)])
        return;

    ALWAYS_LOG(LOGIDENTIFIER, shouldDisable);
    [m_sampleBufferDisplayLayer setToneMapToStandardDynamicRange:shouldDisable];
}

void MediaPlayerPrivateWebM::playerContentBoxRectChanged(const LayoutRect& newRect)
{
    if (!layerOrVideoRenderer() && !newRect.isEmpty())
        updateDisplayLayerAndDecompressionSession();
}

void MediaPlayerPrivateWebM::setShouldMaintainAspectRatio(bool shouldMaintainAspectRatio)
{
    if (m_shouldMaintainAspectRatio == shouldMaintainAspectRatio)
        return;

    m_shouldMaintainAspectRatio = shouldMaintainAspectRatio;
    if (!m_sampleBufferDisplayLayer)
        return;

    [CATransaction begin];
    [CATransaction setAnimationDuration:0];
    [CATransaction setDisableActions:YES];

    [m_sampleBufferDisplayLayer setVideoGravity: (m_shouldMaintainAspectRatio ? AVLayerVideoGravityResizeAspect : AVLayerVideoGravityResize)];

    [CATransaction commit];
}

#if HAVE(SPATIAL_TRACKING_LABEL)
const String& MediaPlayerPrivateWebM::defaultSpatialTrackingLabel() const
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

const String& MediaPlayerPrivateWebM::spatialTrackingLabel() const
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
    auto *renderer = m_sampleBufferVideoRenderer ? m_sampleBufferVideoRenderer.get() : [m_sampleBufferDisplayLayer sampleBufferRenderer];
    if (!m_spatialTrackingLabel.isNull()) {
        INFO_LOG(LOGIDENTIFIER, "Explicitly set STSLabel: ", m_spatialTrackingLabel);
        renderer.STSLabel = m_spatialTrackingLabel;
        return;
    }

    if (renderer && m_visible) {
        // Let AVSBRS manage setting the spatial tracking label in its video renderer itself.
        INFO_LOG(LOGIDENTIFIER, "Has visible renderer, set STSLabel: nil");
        renderer.STSLabel = nil;
        return;
    }

    if (m_audioRenderers.empty()) {
        // If there are no audio renderers, there's nothing to do.
        INFO_LOG(LOGIDENTIFIER, "No audio renderers - no-op");
        return;
    }

    // If there is no video renderer, use the default spatial tracking label if available, or
    // the session's spatial tracking label if not, and set the label directly on each audio
    // renderer.
    AVAudioSession *session = [PAL::getAVAudioSessionClass() sharedInstance];
    String defaultLabel;
    if (!m_defaultSpatialTrackingLabel.isNull()) {
        INFO_LOG(LOGIDENTIFIER, "Default STSLabel: ", m_defaultSpatialTrackingLabel);
        defaultLabel = m_defaultSpatialTrackingLabel;
    } else {
        INFO_LOG(LOGIDENTIFIER, "AVAudioSession label: ", session.spatialTrackingLabel);
        defaultLabel = session.spatialTrackingLabel;
    }
    for (auto& renderer : m_audioRenderers)
        [(__bridge AVSampleBufferAudioRenderer *)renderer.second.get() setSTSLabel:defaultLabel];
}
#endif

void MediaPlayerPrivateWebM::destroyLayerOrVideoRenderer()
{
    destroyLayer();
    destroyVideoRenderer();

    setVideoRenderer(nil);
    setHasAvailableVideoFrame(false);

    if (RefPtr player = m_player.get())
        player->renderingModeChanged();
}

void MediaPlayerPrivateWebM::configureLayerOrVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif

    renderer.preventsDisplaySleepDuringVideoPlayback = NO;

    if ([renderer respondsToSelector:@selector(setPreventsAutomaticBackgroundingDuringVideoPlayback:)])
        renderer.preventsAutomaticBackgroundingDuringVideoPlayback = NO;

    @try {
        [m_synchronizer addRenderer:renderer];
    } @catch(NSException *exception) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer addRenderer:] threw an exception: ", exception.name, ", reason : ", exception.reason);
        ASSERT_NOT_REACHED();

        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }
}

void MediaPlayerPrivateWebM::configureVideoRenderer(VideoMediaSampleRenderer& videoRenderer)
{
    videoRenderer.setResourceOwner(m_resourceOwner);
    m_listener->beginObservingVideoRenderer(videoRenderer.renderer());
}

void MediaPlayerPrivateWebM::invalidateVideoRenderer(VideoMediaSampleRenderer& videoRenderer)
{
    videoRenderer.flush();
    videoRenderer.stopRequestingMediaData();
    m_listener->stopObservingVideoRenderer(videoRenderer.renderer());

}

void MediaPlayerPrivateWebM::setVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
    if (m_videoRenderer && renderer == m_videoRenderer->renderer()) {
        if (RefPtr expiringVideoRenderer = std::exchange(m_expiringVideoRenderer, nullptr))
            invalidateVideoRenderer(*expiringVideoRenderer);
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER, "!!renderer = ", !!renderer);
    ASSERT(!renderer || !m_decompressionSession || hasSelectedVideo());

    if (m_videoRenderer)
        invalidateVideoRenderer(*std::exchange(m_videoRenderer, nullptr));

    if (!renderer)
        return;

    m_videoRenderer = VideoMediaSampleRenderer::create(renderer);
    configureVideoRenderer(*m_videoRenderer);
}

void MediaPlayerPrivateWebM::stageVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
    if (m_videoRenderer && renderer == m_videoRenderer->renderer())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "!!renderer = ", !!renderer);
    ASSERT(!renderer || !m_decompressionSession || hasSelectedVideo());

    if (m_expiringVideoRenderer)
        invalidateVideoRenderer(*std::exchange(m_expiringVideoRenderer, nullptr));

    m_expiringVideoRenderer = WTFMove(m_videoRenderer);
    m_videoRenderer = VideoMediaSampleRenderer::create(renderer);
    configureVideoRenderer(*m_videoRenderer);
    if (m_enabledVideoTrackID)
        reenqueSamples(*m_enabledVideoTrackID, NeedsFlush::No);
}

MediaPlayerPrivateWebM::AcceleratedVideoMode MediaPlayerPrivateWebM::acceleratedVideoMode() const
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (m_usingLinearMediaPlayer) {
        RefPtr player = m_player.get();
        if (player && player->isInFullscreenOrPictureInPicture()) {
            if (m_videoTarget)
                return AcceleratedVideoMode::VideoRenderer;
            return AcceleratedVideoMode::StagedLayer;
        }

        if (m_videoTarget)
            return AcceleratedVideoMode::StagedVideoRenderer;
    }
#endif // ENABLE(LINEAR_MEDIA_PLAYER)

    return AcceleratedVideoMode::Layer;
}

WebSampleBufferVideoRendering *MediaPlayerPrivateWebM::layerOrVideoRenderer() const
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    switch (acceleratedVideoMode()) {
    case AcceleratedVideoMode::Layer:
    case AcceleratedVideoMode::StagedLayer:
        return m_sampleBufferDisplayLayer.get();
    case AcceleratedVideoMode::VideoRenderer:
    case AcceleratedVideoMode::StagedVideoRenderer:
        return m_sampleBufferVideoRenderer.get();
    }
#else
    return m_sampleBufferDisplayLayer.get();
#endif
}

#if ENABLE(LINEAR_MEDIA_PLAYER)
void MediaPlayerPrivateWebM::setVideoTarget(const PlatformVideoTarget& videoTarget)
{
    ALWAYS_LOG(LOGIDENTIFIER, !!videoTarget);
    if (!!videoTarget)
        m_usingLinearMediaPlayer = true;
    m_videoTarget = videoTarget;
    updateDisplayLayerAndDecompressionSession();
}
#endif

void MediaPlayerPrivateWebM::isInFullscreenOrPictureInPictureChanged(bool isInFullscreenOrPictureInPicture)
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    ALWAYS_LOG(LOGIDENTIFIER, isInFullscreenOrPictureInPicture);
    if (!m_usingLinearMediaPlayer)
        return;
    updateDisplayLayerAndDecompressionSession();
#else
    UNUSED_PARAM(isInFullscreenOrPictureInPicture);
#endif
}

} // namespace WebCore

#endif // ENABLE(ALTERNATE_WEBM_PLAYER)
