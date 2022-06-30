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
#import "Logging.h"
#import "MediaPlayer.h"
#import "MediaSampleAVFObjC.h"
#import "NotImplemented.h"
#import "PlatformMediaResourceLoader.h"
#import "ResourceError.h"
#import "ResourceRequest.h"
#import "ResourceResponse.h"
#import "SampleMap.h"
#import "SecurityOrigin.h"
#import "TextTrackRepresentation.h"
#import "TrackBuffer.h"
#import "VideoFrame.h"
#import "VideoLayerManagerObjC.h"
#import "VideoTrackPrivateWebM.h"
#import "WebMResourceClient.h"
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/WeakPtr.h>

#pragma mark - Soft Linking

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

@interface AVSampleBufferDisplayLayer (WebCoreAVSampleBufferDisplayLayerQueueManagementPrivate)
- (void)prerollDecodeWithCompletionHandler:(void (^)(BOOL success))block;
- (void)expectMinimumUpcomingSampleBufferPresentationTime: (CMTime)minimumUpcomingPresentationTime;
- (void)resetUpcomingSampleBufferPresentationTimeExpectations;
@end

#pragma mark -

namespace WebCore {

static const MediaTime discontinuityTolerance = MediaTime(1, 1);

MediaPlayerPrivateWebM::MediaPlayerPrivateWebM(MediaPlayer* player)
    : m_player(player)
    , m_synchronizer(adoptNS([PAL::allocAVSampleBufferRenderSynchronizerInstance() init]))
    , m_parser(adoptRef(*new SourceBufferParserWebM()))
    , m_logger(player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
    , m_videoLayerManager(makeUnique<VideoLayerManagerObjC>(m_logger, m_logIdentifier))
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_parser->setLogger(m_logger, m_logIdentifier);
}

MediaPlayerPrivateWebM::~MediaPlayerPrivateWebM()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_durationObserver)
        [m_synchronizer removeTimeObserver:m_durationObserver.get()];

    destroyLayer();
    destroyAudioRenderers();
    clearTracks();

    cancelLoad();
    abort();
}

static HashSet<String, ASCIICaseInsensitiveHash>& mimeTypeCache()
{
    static NeverDestroyed cache = HashSet<String, ASCIICaseInsensitiveHash>();
    if (cache->isEmpty()) {
        auto types = SourceBufferParserWebM::supportedMIMETypes();
        cache->add(types.begin(), types.end());
    }
    return cache;
}

void MediaPlayerPrivateWebM::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types)
{
    types = mimeTypeCache();
}

MediaPlayer::SupportsType MediaPlayerPrivateWebM::supportsType(const MediaEngineSupportParameters& parameters)
{
    if (parameters.isMediaSource || parameters.isMediaStream)
        return MediaPlayer::SupportsType::IsNotSupported;
    
    return SourceBufferParserWebM::isContentTypeSupported(parameters.type);
}

void MediaPlayerPrivateWebM::load(const String& url)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (!m_player)
        return;

    auto mimeType = m_player->contentMIMEType();
    if (mimeType.isEmpty() || !mimeTypeCache().contains(mimeType)) {
        ERROR_LOG(LOGIDENTIFIER, "mime type = ", mimeType, " not supported");
        setNetworkState(MediaPlayer::NetworkState::FormatError);
        return;
    }

    ResourceRequest request(url);
    request.setAllowCookies(true);
    request.setFirstPartyForCookies(URL(url));

    ResourceLoaderOptions loaderOptions(
        SendCallbackPolicy::SendCallbacks,
        ContentSniffingPolicy::DoNotSniffContent,
        DataBufferingPolicy::BufferData,
        StoredCredentialsPolicy::DoNotUse,
        ClientCredentialPolicy::CannotAskClientForCredentials,
        FetchOptions::Credentials::Omit,
        SecurityCheckPolicy::DoSecurityCheck,
        FetchOptions::Mode::NoCors,
        CertificateInfoPolicy::DoNotIncludeCertificateInfo,
        ContentSecurityPolicyImposition::DoPolicyCheck,
        DefersLoadingPolicy::AllowDefersLoading,
        CachingPolicy::DisallowCaching
    );
    loaderOptions.destination = FetchOptions::Destination::Video;

    auto loader = m_player->createResourceLoader();
    m_resourceClient = WebMResourceClient::create(*this, *loader, WTFMove(request));
    
    if (!m_resourceClient) {
        ERROR_LOG(LOGIDENTIFIER, "could not create resource client");
        setNetworkState(MediaPlayer::NetworkState::NetworkError);
        setReadyState(MediaPlayer::ReadyState::HaveNothing);
        return;
    }
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

void MediaPlayerPrivateWebM::dataReceived(const SharedBuffer&)
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

void MediaPlayerPrivateWebM::loadFailed(const ResourceError& error)
{
    ERROR_LOG(LOGIDENTIFIER, "resource failed to load with code ", error.errorCode());
    setNetworkState(MediaPlayer::NetworkState::NetworkError);
}

void MediaPlayerPrivateWebM::loadFinished(const FragmentedSharedBuffer& fragmentedBuffer)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    auto buffer = fragmentedBuffer.makeContiguous();
    append(buffer);

    if (!m_hasVideo && !m_hasAudio) {
        ERROR_LOG(LOGIDENTIFIER, "could not load audio or video tracks");
        setNetworkState(MediaPlayer::NetworkState::FormatError);
        setReadyState(MediaPlayer::ReadyState::HaveNothing);
        return;
    }
    
    updateBufferedFromTrackBuffers(true);
    updateDurationFromTrackBuffers();
    setReadyState(MediaPlayer::ReadyState::HaveEnoughData);
    setNetworkState(MediaPlayer::NetworkState::Idle);
    if (m_hasVideo)
        m_player->firstVideoFrameAvailable();
}

void MediaPlayerPrivateWebM::cancelLoad()
{
    if (m_resourceClient) {
        m_resourceClient->stop();
        m_resourceClient = nullptr;
    }
}

PlatformLayer* MediaPlayerPrivateWebM::platformLayer() const
{
    if (!m_displayLayer)
        return nullptr;
    return m_videoLayerManager->videoInlineLayer();
}

void MediaPlayerPrivateWebM::play()
{
#if PLATFORM(IOS_FAMILY)
    flushIfNeeded();
#endif

    [m_synchronizer setRate:m_rate];

    if (currentMediaTime() >= durationMediaTime())
        seek(MediaTime::zeroTime());
}

void MediaPlayerPrivateWebM::pause()
{
    [m_synchronizer setRate:0];
}

bool MediaPlayerPrivateWebM::paused() const
{
    return ![m_synchronizer rate];
}

void MediaPlayerPrivateWebM::setPageIsVisible(bool visible)
{
    if (m_visible == visible)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, visible);
    m_visible = visible;
    if (visible)
        ensureLayer();
}

MediaTime MediaPlayerPrivateWebM::currentMediaTime() const
{
    MediaTime synchronizerTime = PAL::toMediaTime(PAL::CMTimebaseGetTime([m_synchronizer timebase]));
    if (synchronizerTime < MediaTime::zeroTime())
        return MediaTime::zeroTime();
    if (synchronizerTime > durationMediaTime())
        return durationMediaTime();

    return synchronizerTime;
}

void MediaPlayerPrivateWebM::seek(const MediaTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER, "time = ", time);

    for (auto& trackBufferPair : m_trackBufferMap) {
        TrackBuffer& trackBuffer = trackBufferPair.value;
        auto trackId = trackBufferPair.key;

        trackBuffer.setNeedsReenqueueing(true);
        reenqueueMediaForTime(trackBuffer, trackId, time);
    }
    [m_synchronizer setRate:effectiveRate() time:PAL::toCMTime(time)];
    m_player->timeChanged();
}

void MediaPlayerPrivateWebM::setRateDouble(double rate)
{
    if (rate == m_rate)
        return;

    m_rate = std::max<double>(rate, 0);

    if (!paused())
        [m_synchronizer setRate:m_rate];

    m_player->rateChanged();
}

double MediaPlayerPrivateWebM::effectiveRate() const
{
    return PAL::CMTimebaseGetRate([m_synchronizer timebase]);
}

void MediaPlayerPrivateWebM::setVolume(float volume)
{
    for (auto& renderer : m_audioRenderers.values())
        [renderer setVolume:volume];
}

void MediaPlayerPrivateWebM::setMuted(bool muted)
{
    for (auto& renderer : m_audioRenderers.values())
        [renderer setMuted:muted];
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateWebM::seekable() const
{
    return makeUnique<PlatformTimeRanges>(minMediaTimeSeekable(), maxMediaTimeSeekable());
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateWebM::buffered() const
{
    if (!m_buffered)
        return makeUnique<PlatformTimeRanges>();

    return makeUnique<PlatformTimeRanges>(m_buffered->ranges());
}

void MediaPlayerPrivateWebM::setBufferedRanges(PlatformTimeRanges timeRanges)
{
    m_buffered->ranges() = WTFMove(timeRanges);
}

void MediaPlayerPrivateWebM::updateBufferedFromTrackBuffers(bool ended)
{
    MediaTime highestEndTime = MediaTime::negativeInfiniteTime();
    for (auto& trackBuffer : m_trackBufferMap.values()) {
        if (!trackBuffer.get().buffered().length())
            continue;
        highestEndTime = std::max(highestEndTime, trackBuffer.get().maximumBufferedTime());
    }

    // NOTE: Short circuit the following if none of the TrackBuffers have buffered ranges to avoid generating
    // a single range of {0, 0}.
    if (highestEndTime.isNegativeInfinite()) {
        setBufferedRanges(PlatformTimeRanges());
        return;
    }

    PlatformTimeRanges intersectionRanges { MediaTime::zeroTime(), highestEndTime };

    for (auto& trackBuffer : m_trackBufferMap.values()) {
        if (!trackBuffer.get().buffered().length())
            continue;

        PlatformTimeRanges trackRanges = trackBuffer.get().buffered();

        if (ended)
            trackRanges.add(trackRanges.maximumBufferedTime(), highestEndTime);

        intersectionRanges.intersectWith(trackRanges);
    }

    setBufferedRanges(WTFMove(intersectionRanges));
}

void MediaPlayerPrivateWebM::updateDurationFromTrackBuffers()
{
    MediaTime highestEndTime = MediaTime::zeroTime();
    for (auto& trackBuffer : m_trackBufferMap.values()) {
        if (!trackBuffer->highestPresentationTimestamp())
            continue;
        highestEndTime = std::max(highestEndTime, trackBuffer->highestPresentationTimestamp());
    }
    
    setDuration(WTFMove(highestEndTime));
}

bool MediaPlayerPrivateWebM::didLoadingProgress() const
{
    return false;
}

void MediaPlayerPrivateWebM::setNaturalSize(FloatSize size)
{
    FloatSize oldSize = m_naturalSize;
    m_naturalSize = size;
    if (oldSize != m_naturalSize) {
        INFO_LOG(LOGIDENTIFIER, "was ", oldSize.width(), " x ", oldSize.height(), ", is ", size.width(), " x ", size.height());
        m_player->sizeChanged();
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

    m_durationObserver = [m_synchronizer addBoundaryTimeObserverForTimes:times queue:dispatch_get_main_queue() usingBlock:[weakThis = WeakPtr { *this }, duration, logSiteIdentifier, this] {
        if (!weakThis)
            return;

        MediaTime now = weakThis->currentMediaTime();
        ALWAYS_LOG(logSiteIdentifier, "boundary time observer called, now = ", now);

        weakThis->pause();
        if (now < duration) {
            ERROR_LOG(logSiteIdentifier, "ERROR: boundary time observer called before duration");
            [weakThis->m_synchronizer setRate:0 time:PAL::toCMTime(duration)];
        }
        weakThis->m_player->timeChanged();

    }];

    m_duration = WTFMove(duration);
    m_player->durationChanged();
}

void MediaPlayerPrivateWebM::setNetworkState(MediaPlayer::NetworkState state)
{
    if (state == m_networkState)
        return;

    m_networkState = state;
    m_player->networkStateChanged();
}

void MediaPlayerPrivateWebM::setReadyState(MediaPlayer::ReadyState state)
{
    if (state == m_readyState)
        return;

    m_readyState = state;
    m_player->readyStateChanged();
}

void MediaPlayerPrivateWebM::characteristicsChanged()
{
    m_player->characteristicChanged();
}

RetainPtr<PlatformLayer> MediaPlayerPrivateWebM::createVideoFullscreenLayer()
{
    return adoptNS([[CALayer alloc] init]);
}

void MediaPlayerPrivateWebM::setVideoFullscreenLayer(PlatformLayer *videoFullscreenLayer, WTF::Function<void()>&& completionHandler)
{
    m_videoLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler), nullptr);
}

void MediaPlayerPrivateWebM::setVideoFullscreenFrame(FloatRect frame)
{
    m_videoLayerManager->setVideoFullscreenFrame(frame);
}

bool MediaPlayerPrivateWebM::requiresTextTrackRepresentation() const
{
    return m_videoLayerManager->videoFullscreenLayer();
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

void MediaPlayerPrivateWebM::enqueueSample(Ref<MediaSample>&& sample, uint64_t trackId)
{
    if (trackId != m_enabledVideoTrackID && !m_audioRenderers.contains(trackId))
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

    if (trackId == m_enabledVideoTrackID) {
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

        if (!m_displayLayer)
            return;

        [m_displayLayer enqueueSampleBuffer:platformSample.sample.cmSampleBuffer];
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

    auto renderer = m_audioRenderers.get(trackId);
    [renderer enqueueSampleBuffer:platformSample.sample.cmSampleBuffer];
}

void MediaPlayerPrivateWebM::reenqueSamples(uint64_t trackId)
{
    auto* trackBuffer = m_trackBufferMap.get(trackId);
    if (!trackBuffer)
        return;
    trackBuffer->setNeedsReenqueueing(true);
    reenqueueMediaForTime(*trackBuffer, trackId, currentMediaTime());
}

void MediaPlayerPrivateWebM::reenqueueMediaForTime(TrackBuffer& trackBuffer, uint64_t trackId, const MediaTime& time)
{
    flushTrack(trackId);
    if (trackBuffer.reenqueueMediaForTime(time, timeFudgeFactor()))
        provideMediaData(trackBuffer, trackId);
}

void MediaPlayerPrivateWebM::notifyClientWhenReadyForMoreSamples(uint64_t trackId)
{
    if (trackId == m_enabledVideoTrackID) {
        if (m_displayLayer) {
            WeakPtr weakThis { *this };
            [m_displayLayer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
                if (weakThis)
                    weakThis->didBecomeReadyForMoreSamples(trackId);
            }];
        }
        return;
    }
    
    if (m_audioRenderers.contains(trackId)) {
        WeakPtr weakThis { *this };
        [m_audioRenderers.get(trackId) requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
            if (weakThis)
                weakThis->didBecomeReadyForMoreSamples(trackId);
        }];
    }
}

bool MediaPlayerPrivateWebM::canSetMinimumUpcomingPresentationTime(uint64_t trackId) const
{
    return trackId == m_enabledVideoTrackID
        && [PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(expectMinimumUpcomingSampleBufferPresentationTime:)];
}

void MediaPlayerPrivateWebM::setMinimumUpcomingPresentationTime(uint64_t trackId, const MediaTime& presentationTime)
{
    ASSERT(canSetMinimumUpcomingPresentationTime(trackId));
    if (canSetMinimumUpcomingPresentationTime(trackId))
        [m_displayLayer expectMinimumUpcomingSampleBufferPresentationTime:PAL::toCMTime(presentationTime)];
}

void MediaPlayerPrivateWebM::clearMinimumUpcomingPresentationTime(uint64_t trackId)
{
    ASSERT(canSetMinimumUpcomingPresentationTime(trackId));
    if (canSetMinimumUpcomingPresentationTime(trackId))
        [m_displayLayer resetUpcomingSampleBufferPresentationTimeExpectations];
}

bool MediaPlayerPrivateWebM::isReadyForMoreSamples(uint64_t trackId)
{
    if (trackId == m_enabledVideoTrackID) {
#if PLATFORM(IOS_FAMILY)
        if (m_displayLayerWasInterrupted)
            return false;
#endif
        return [m_displayLayer isReadyForMoreMediaData];
    }

    if (m_audioRenderers.contains(trackId))
        return [m_audioRenderers.get(trackId) isReadyForMoreMediaData];

    return false;
}

void MediaPlayerPrivateWebM::didBecomeReadyForMoreSamples(uint64_t trackId)
{
    INFO_LOG(LOGIDENTIFIER, trackId);

    if (trackId == m_enabledVideoTrackID)
        [m_displayLayer stopRequestingMediaData];
    else if (m_audioRenderers.contains(trackId))
        [m_audioRenderers.get(trackId) stopRequestingMediaData];
    else
        return;

    provideMediaData(trackId);
}

void MediaPlayerPrivateWebM::provideMediaData(uint64_t trackId)
{
    auto* trackBuffer = m_trackBufferMap.get(trackId);
    if (!trackBuffer)
        return;

    provideMediaData(*trackBuffer, trackId);
}

void MediaPlayerPrivateWebM::provideMediaData(TrackBuffer& trackBuffer, uint64_t trackId)
{
    unsigned enqueuedSamples = 0;

    if (trackBuffer.needsMinimumUpcomingPresentationTimeUpdating() && canSetMinimumUpcomingPresentationTime(trackId)) {
        trackBuffer.setMinimumEnqueuedPresentationTime(MediaTime::invalidTime());
        clearMinimumUpcomingPresentationTime(trackId);
    }

    while (!trackBuffer.decodeQueue().empty()) {
        if (!isReadyForMoreSamples(trackId)) {
            DEBUG_LOG(LOGIDENTIFIER, "bailing early, track id ", trackId, " is not ready for more data");
            notifyClientWhenReadyForMoreSamples(trackId);
            break;
        }

        auto sample = trackBuffer.decodeQueue().begin()->second;

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

        enqueueSample(sample.releaseNonNull(), trackId);
        ++enqueuedSamples;
    }

    if (canSetMinimumUpcomingPresentationTime(trackId) && trackBuffer.updateMinimumUpcomingPresentationTime())
        setMinimumUpcomingPresentationTime(trackId, trackBuffer.minimumEnqueuedPresentationTime());

    DEBUG_LOG(LOGIDENTIFIER, "enqueued ", enqueuedSamples, " samples, ", static_cast<uint64_t>(trackBuffer.decodeQueue().size()), " remaining");
}

void MediaPlayerPrivateWebM::trackDidChangeSelected(VideoTrackPrivate& track, bool selected)
{
    auto trackId = track.trackUID().value_or(-1);

    auto* trackBuffer = m_trackBufferMap.get(trackId);
    if (!trackBuffer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "video trackID = ", trackId, ", selected = ", selected);

    if (selected) {
        ensureLayer();
        m_enabledVideoTrackID = trackId;
        reenqueSamples(trackId);
        return;
    }
    
    if (m_enabledVideoTrackID == trackId)
        m_enabledVideoTrackID = -1;
}

void MediaPlayerPrivateWebM::trackDidChangeEnabled(AudioTrackPrivate& track, bool enabled)
{
    auto trackId = track.trackUID().value_or(-1);

    auto* trackBuffer = m_trackBufferMap.get(trackId);
    if (!trackBuffer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "audio trackID = ", trackId, ", enabled = ", enabled);

    if (enabled) {
        addAudioRenderer(trackId);
        reenqueSamples(trackId);
        return;
    }
    
    removeAudioRenderer(trackId);
}

void MediaPlayerPrivateWebM::didParseInitializationData(InitializationSegment&& segment)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    clearTracks();

    if (segment.duration.isValid())
        setDuration(WTFMove(segment.duration));
    else
        setDuration(MediaTime::positiveInfiniteTime());

    for (auto videoTrackInfo : segment.videoTracks) {
        if (videoTrackInfo.track && videoTrackInfo.track->trackUID()) {
            auto track = static_pointer_cast<VideoTrackPrivateWebM>(videoTrackInfo.track);
            addTrackBuffer(track->trackUID().value(), WTFMove(videoTrackInfo.description));

            track->setSelectedChangedCallback([weakThis = WeakPtr { *this }, this] (VideoTrackPrivate& track, bool selected) {
                if (!weakThis)
                    return;

                auto videoTrackSelectedChanged = [weakThis, this, trackRef = Ref { track }, selected] {
                    if (!weakThis)
                        return;
                    trackDidChangeSelected(trackRef, selected);
                };

                if (!m_processingInitializationSegment) {
                    videoTrackSelectedChanged();
                    return;
                }
            });

            if (m_videoTracks.isEmpty())
                track->setSelected(true);

            m_videoTracks.append(track);
            m_player->addVideoTrack(*track);
        }
    }

    for (auto audioTrackInfo : segment.audioTracks) {
        if (audioTrackInfo.track && audioTrackInfo.track->trackUID()) {
            auto track = static_pointer_cast<AudioTrackPrivateWebM>(audioTrackInfo.track);
            addTrackBuffer(track->trackUID().value(), WTFMove(audioTrackInfo.description));

            track->setEnabledChangedCallback([weakThis = WeakPtr { *this }, this] (AudioTrackPrivate& track, bool enabled) {
                if (!weakThis)
                    return;

                auto audioTrackEnabledChanged = [weakThis, this, trackRef = Ref { track }, enabled] {
                    if (!weakThis)
                        return;

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
            m_player->addAudioTrack(*track);
        }
    }
}

void MediaPlayerPrivateWebM::didEncounterErrorDuringParsing(int32_t code)
{
    ERROR_LOG(LOGIDENTIFIER, code);

    m_parsingSucceeded = false;
}

void MediaPlayerPrivateWebM::didProvideMediaDataForTrackId(Ref<MediaSampleAVFObjC>&& originalSample, uint64_t trackId, const String& mediaType)
{
    UNUSED_PARAM(mediaType);

    auto* trackBuffer = m_trackBufferMap.get(trackId);
    if (!trackBuffer)
        return;

    auto sample = WTFMove(originalSample);

    MediaTime microsecond(1, 1000000);
    if (!trackBuffer->roundedTimestampOffset().isValid())
        trackBuffer->setRoundedTimestampOffset(-sample->presentationTime(), sample->presentationTime().timeScale(), microsecond);
        
    sample->offsetTimestampsBy(trackBuffer->roundedTimestampOffset());
    trackBuffer->samples().addSample(sample);

    DecodeOrderSampleMap::KeyType decodeKey(sample->decodeTime(), sample->presentationTime());
    trackBuffer->decodeQueue().insert(DecodeOrderSampleMap::MapType::value_type(decodeKey, &sample.get()));

    trackBuffer->setLastDecodeTimestamp(sample->decodeTime());
    trackBuffer->setLastFrameDuration(sample->duration());
    
    auto presentationTimestamp = sample->presentationTime();
    auto presentationEndTime = presentationTimestamp + sample->duration();
    if (trackBuffer->highestPresentationTimestamp().isInvalid() || presentationEndTime > trackBuffer->highestPresentationTimestamp())
        trackBuffer->setHighestPresentationTimestamp(presentationEndTime);
    
    // Eliminate small gaps between buffered ranges by coalescing
    // disjoint ranges separated by less than a "fudge factor".
    auto nearestToPresentationStartTime = trackBuffer->buffered().nearest(presentationTimestamp);
    if (nearestToPresentationStartTime.isValid() && (presentationTimestamp - nearestToPresentationStartTime).isBetween(MediaTime::zeroTime(), timeFudgeFactor()))
        presentationTimestamp = nearestToPresentationStartTime;

    auto nearestToPresentationEndTime = trackBuffer->buffered().nearest(presentationEndTime);
    if (nearestToPresentationEndTime.isValid() && (nearestToPresentationEndTime - presentationEndTime).isBetween(MediaTime::zeroTime(), timeFudgeFactor()))
        presentationEndTime = nearestToPresentationEndTime;

    trackBuffer->addBufferedRange(presentationTimestamp, presentationEndTime);

    notifyClientWhenReadyForMoreSamples(trackId);
}

void MediaPlayerPrivateWebM::append(SharedBuffer& buffer)
{
    ALWAYS_LOG(LOGIDENTIFIER, "data length = ", buffer.size());

    m_parser->setDidParseInitializationDataCallback([weakThis = WeakPtr { *this }, abortCalled = m_abortCalled] (InitializationSegment&& segment) {
        if (!weakThis || abortCalled != weakThis->m_abortCalled)
            return;

        weakThis->didParseInitializationData(WTFMove(segment));
    });

    m_parser->setDidEncounterErrorDuringParsingCallback([weakThis = WeakPtr { *this }, abortCalled = m_abortCalled] (int32_t errorCode) {
        if (!weakThis || abortCalled != weakThis->m_abortCalled)
            return;
        weakThis->didEncounterErrorDuringParsing(errorCode);
    });

    m_parser->setDidProvideMediaDataCallback([weakThis = WeakPtr { *this }, abortCalled = m_abortCalled] (Ref<MediaSampleAVFObjC>&& sample, uint64_t trackId, const String& mediaType) {
        if (!weakThis || abortCalled != weakThis->m_abortCalled)
            return;
        weakThis->didProvideMediaDataForTrackId(WTFMove(sample), trackId, mediaType);
    });
    m_parser->setCallOnClientThreadCallback([](auto&& function) {
        function();
    });

    m_parsingSucceeded = true;

    SourceBufferParser::Segment segment(Ref { buffer });
    m_parser->appendData(WTFMove(segment));
}

void MediaPlayerPrivateWebM::abort()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_abortCalled++;
}

void MediaPlayerPrivateWebM::flush()
{
    if (m_videoTracks.size())
        flushVideo();

    if (!m_audioTracks.size())
        return;

    for (auto& renderer : m_audioRenderers.values())
        flushAudio(renderer.get());
}

#if PLATFORM(IOS_FAMILY)
void MediaPlayerPrivateWebM::flushIfNeeded()
{
    if (!m_displayLayerWasInterrupted)
        return;

    m_displayLayerWasInterrupted = false;
    if (m_videoTracks.size())
        flushVideo();

    [m_displayLayer stopRequestingMediaData];

    reenqueSamples(m_enabledVideoTrackID);
}
#endif

void MediaPlayerPrivateWebM::flushTrack(uint64_t trackId)
{
    DEBUG_LOG(LOGIDENTIFIER, trackId);

    if (trackId == m_enabledVideoTrackID) {
        flushVideo();
        return;
    }
    
    if (m_audioRenderers.contains(trackId))
        flushAudio(m_audioRenderers.get(trackId).get());
}

void MediaPlayerPrivateWebM::flushVideo()
{
    DEBUG_LOG(LOGIDENTIFIER);
    [m_displayLayer flush];
}

void MediaPlayerPrivateWebM::flushAudio(AVSampleBufferAudioRenderer *renderer)
{
    DEBUG_LOG(LOGIDENTIFIER);
    [renderer flush];
}

void MediaPlayerPrivateWebM::addTrackBuffer(uint64_t trackId, RefPtr<MediaDescription>&& description)
{
    ASSERT(!m_trackBufferMap.contains(trackId));

    setHasAudio(m_hasAudio || description->isAudio());
    setHasVideo(m_hasVideo || description->isVideo());

    auto trackBuffer = TrackBuffer::create(WTFMove(description), discontinuityTolerance);
    trackBuffer->setLogger(logger(), logIdentifier());
    m_trackBufferMap.add(trackId, WTFMove(trackBuffer));
}

void MediaPlayerPrivateWebM::ensureLayer()
{
    if (m_displayLayer)
        return;

    m_displayLayer = adoptNS([PAL::allocAVSampleBufferDisplayLayerInstance() init]);
    [m_displayLayer setName:@"MediaPlayerPrivateWebM AVSampleBufferDisplayLayer"];

    ERROR_LOG_IF(!m_displayLayer, LOGIDENTIFIER, "Creating the AVSampleBufferDisplayLayer failed.");
    if (!m_displayLayer)
        return;

    @try {
        [m_synchronizer addRenderer:m_displayLayer.get()];
    } @catch(NSException *exception) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer addRenderer:] threw an exception: ", [[exception name] UTF8String], ", reason : ", [[exception reason] UTF8String]);
        ASSERT_NOT_REACHED();

        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

    m_videoLayerManager->setVideoLayer(m_displayLayer.get(), snappedIntRect(m_player->playerContentBoxRect()).size());

    if (m_enabledVideoTrackID != notFound) {
        WeakPtr weakThis { *this };
        [m_displayLayer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
            if (weakThis)
                weakThis->didBecomeReadyForMoreSamples(m_enabledVideoTrackID);
        }];
    }

    m_player->renderingModeChanged();
}

void MediaPlayerPrivateWebM::addAudioRenderer(uint64_t trackId)
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

    [renderer setMuted:m_player->muted()];
    [renderer setVolume:m_player->volume()];
    [renderer setAudioTimePitchAlgorithm:(m_player->preservesPitch() ? AVAudioTimePitchAlgorithmSpectral : AVAudioTimePitchAlgorithmVarispeed)];

#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    auto deviceId = m_player->audioOutputDeviceIdOverride();
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
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer addRenderer:] threw an exception: ", [[exception name] UTF8String], ", reason : ", [[exception reason] UTF8String]);
        ASSERT_NOT_REACHED();

        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

    characteristicsChanged();

    WeakPtr weakThis { *this };
    [renderer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
        if (weakThis)
            weakThis->didBecomeReadyForMoreSamples(trackId);
    }];

    m_audioRenderers.set(trackId, renderer);
}

void MediaPlayerPrivateWebM::removeAudioRenderer(uint64_t trackId)
{
    auto renderer = m_audioRenderers.get(trackId);
    destroyAudioRenderer(renderer);
    m_audioRenderers.remove(trackId);
}

void MediaPlayerPrivateWebM::destroyLayer()
{
    if (!m_displayLayer)
        return;

    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:m_displayLayer.get() atTime:currentTime withCompletionHandler:^(BOOL){
        // No-op.
    }];

    m_videoLayerManager->didDestroyVideoLayer();
    [m_displayLayer flush];
    [m_displayLayer stopRequestingMediaData];
    m_displayLayer = nullptr;
    m_player->renderingModeChanged();
}

void MediaPlayerPrivateWebM::destroyAudioRenderer(RetainPtr<AVSampleBufferAudioRenderer> renderer)
{
    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:renderer.get() atTime:currentTime withCompletionHandler:^(BOOL){
        // No-op.
    }];

    [renderer flush];
    [renderer stopRequestingMediaData];
}

void MediaPlayerPrivateWebM::destroyAudioRenderers()
{
    for (auto& renderer : m_audioRenderers.values())
        destroyAudioRenderer(renderer);
    m_audioRenderers.clear();
    m_player->renderingModeChanged();
}

void MediaPlayerPrivateWebM::clearTracks()
{
    for (auto& track : m_videoTracks)
        track->setSelectedChangedCallback(nullptr);
    m_videoTracks.clear();

    for (auto& track : m_audioTracks)
        track->setEnabledChangedCallback(nullptr);
    m_audioTracks.clear();
}

WTFLogChannel& MediaPlayerPrivateWebM::logChannel() const
{
    return LogMedia;
}

class MediaPlayerFactoryWebM final : public MediaPlayerFactory {
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::CocoaWebM; };

    std::unique_ptr<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer* player) const final
    {
        return makeUnique<MediaPlayerPrivateWebM>(player);
    }

    void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types) const final
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
    return PAL::isAVFoundationFrameworkAvailable()
        && PAL::isCoreMediaFrameworkAvailable()
        && PAL::getAVSampleBufferAudioRendererClass()
        && PAL::getAVSampleBufferRenderSynchronizerClass()
        && class_getInstanceMethod(PAL::getAVSampleBufferAudioRendererClass(), @selector(setMuted:));
}

} // namespace WebCore

#endif // ENABLE(ALTERNATE_WEBM_PLAYER)
