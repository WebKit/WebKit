/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "AudioVideoRendererAVFObjC.h"

#import "AudioMediaStreamTrackRenderer.h"
#import "CDMFairPlayStreaming.h"
#import "CDMInstanceFairPlayStreamingAVFObjC.h"
#import "CDMSessionAVContentKeySession.h"
#import "EffectiveRateChangedListener.h"
#import "FormatDescriptionUtilities.h"
#import "GraphicsContext.h"
#import "LayoutRect.h"
#import "Logging.h"
#import "MediaSampleAVFObjC.h"
#import "MediaSessionManagerCocoa.h"
#import "NativeImage.h"
#import "PixelBufferConformerCV.h"
#import "PlatformDynamicRangeLimitCocoa.h"
#import "SpatialAudioExperienceHelper.h"
#import "TextTrackRepresentation.h"
#import "Timer.h"
#import "VideoFrameCV.h"
#import "VideoLayerManagerObjC.h"
#import "VideoMediaSampleRenderer.h"
#import "WebSampleBufferVideoRendering.h"

#import <AVFoundation/AVFoundation.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/BlockPtr.h>
#import <wtf/MainThread.h>
#import <wtf/SoftLinking.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakPtr.h>
#import <wtf/WorkQueue.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/darwin/DispatchExtras.h>

#pragma mark - Soft Linking
#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

@interface AVSampleBufferDisplayLayer (Staging_100128644)
@property (assign, nonatomic) BOOL preventsAutomaticBackgroundingDuringVideoPlayback;
@end

@interface AVSampleBufferDisplayLayer (WebCoreSampleBufferKeySession) <AVContentKeyRecipient>
@end

@interface AVSampleBufferAudioRenderer (WebCoreSampleBufferKeySession) <AVContentKeyRecipient>
@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AudioVideoRendererAVFObjC);

AudioVideoRendererAVFObjC::AudioVideoRendererAVFObjC(const Logger& originalLogger, uint64_t logSiteIdentifier)
    : m_logger(originalLogger)
    , m_logIdentifier(logSiteIdentifier)
    , m_videoLayerManager(makeUniqueRef<VideoLayerManagerObjC>(originalLogger, logSiteIdentifier))
    , m_synchronizer([adoptNS(PAL::allocAVSampleBufferRenderSynchronizerInstance()) init])
    , m_listener(WebAVSampleBufferListener::create(*this))
    , m_startupTime(MonotonicTime::now())
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    , m_keyStatusesChangedObserver(Observer<void()>::create([weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->tryToEnqueueBlockedSamples();
    }))
#endif
{
    // addPeriodicTimeObserverForInterval: throws an exception if you pass a non-numeric CMTime, so just use
    // an arbitrarily large time value of once an hour:
    __block WeakPtr weakThis { *this };
    m_timeJumpedObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::toCMTime(MediaTime::createWithDouble(3600)) queue:mainDispatchQueueSingleton() usingBlock:^(CMTime time) {
#if LOG_DISABLED
        UNUSED_PARAM(time);
#endif
        if (!weakThis)
            return;

        auto clampedTime = CMTIME_IS_NUMERIC(time) ? clampTimeToLastSeekTime(PAL::toMediaTime(time)) : MediaTime::zeroTime();
        ALWAYS_LOG(LOGIDENTIFIER, "synchronizer fired: time clamped = ", clampedTime, ", seeking = ", m_isSynchronizerSeeking);

        m_isSynchronizerSeeking = false;
        maybeCompleteSeek();
    }];
    stall();
}

AudioVideoRendererAVFObjC::~AudioVideoRendererAVFObjC()
{
    if (RefPtr rateChangeListener = std::exchange(m_effectiveRateChangedListener, { }))
        rateChangeListener->stop();
    cancelSeekingPromiseIfNeeded();
    cancelTimeReachedAction();
    cancelTimeObserver();
    if (m_timeJumpedObserver)
        [m_synchronizer removeTimeObserver:m_timeJumpedObserver.get()];
    if (m_videoFrameMetadataGatheringObserver)
        [m_synchronizer removeTimeObserver:m_videoFrameMetadataGatheringObserver.get()];
    if (m_performTaskObserver)
        [m_synchronizer removeTimeObserver:m_performTaskObserver.get()];

    destroyVideoTrack();
    destroyAudioRenderers();
    m_listener->invalidate();

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    if (RefPtr session = m_session.get())
        session->removeRenderer(*this);
#endif
}

void AudioVideoRendererAVFObjC::setPreferences(VideoRendererPreferences preferences)
{
    m_preferences = preferences;
    if (RefPtr videoRenderer = m_videoRenderer)
        videoRenderer->setPreferences(preferences);
}

void AudioVideoRendererAVFObjC::setHasProtectedVideoContent(bool protectedContent)
{
    ALWAYS_LOG(LOGIDENTIFIER, "protectedContent: ", protectedContent);

    if (std::exchange(m_hasProtectedVideoContent, protectedContent) != protectedContent)
        updateDisplayLayerIfNeeded();
}

TracksRendererManager::TrackIdentifier AudioVideoRendererAVFObjC::addTrack(TrackType type)
{
    auto identifier = TrackIdentifier::generate();
    m_trackTypes.add(identifier, type);
    ALWAYS_LOG(LOGIDENTIFIER, toString(identifier));

    switch (type) {
    case TrackType::Video:
        if (RefPtr videoRenderer = m_videoRenderer) {
            videoRenderer->stopRequestingMediaData();
            videoRenderer->flush();
        }
        m_enabledVideoTrackId = identifier;
        updateDisplayLayerIfNeeded();
        break;
    case TrackType::Audio:
        addAudioRenderer(identifier);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return identifier;
}

void AudioVideoRendererAVFObjC::removeTrack(TrackIdentifier trackId)
{
    ALWAYS_LOG(LOGIDENTIFIER, toString(trackId));
    auto type = typeOf(trackId);
    if (!type)
        return;

    switch (*type) {
    case TrackType::Video:
        if (isEnabledVideoTrackId(trackId))
            destroyVideoTrack();
        break;
    case TrackType::Audio:
        removeAudioRenderer(trackId);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    m_trackTypes.remove(trackId);
}

void AudioVideoRendererAVFObjC::enqueueSample(TrackIdentifier trackId, Ref<MediaSample>&& sample, std::optional<MediaTime> minimumUpcomingTime)
{
    auto type = typeOf(trackId);
    if (!type)
        return;

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    if (!canEnqueueSample(trackId, sample)) {
        DEBUG_LOG(LOGIDENTIFIER, "Can't enqueue sample: ", sample.get(), " for track: ", toString(trackId));
        m_blockedSamples.append({ trackId, sample });
        return;
    }
    attachContentKeyToSampleIfNeeded(sample);
#endif

    RetainPtr cmSampleBuffer = sample->platformSample().cmSampleBuffer();
    RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(cmSampleBuffer.get());
    ASSERT(formatDescription);
    if (!formatDescription) {
        ERROR_LOG(LOGIDENTIFIER, "Received sample with a null formatDescription. Bailing.");
        return;
    }
    auto mediaType = typeFromFormatDescription(formatDescription.get());

    switch (*type) {
    case TrackType::Video:
        ASSERT(mediaType == TrackType::Video);
        if (mediaType != TrackType::Video) {
            ERROR_LOG(LOGIDENTIFIER, "Expected sample of type: video got: '", mediaType, "'. Bailing.");
            return;
        }

        if (m_sizeChangedCallback) {
            FloatSize formatSize = presentationSizeFromFormatDescription(formatDescription.get());
            if (m_cachedSize != formatSize) {
                DEBUG_LOG(LOGIDENTIFIER, "size changed from: ", m_cachedSize.value_or(FloatSize()), " to: ", formatSize);
                if (!std::exchange(m_cachedSize, formatSize))
                    m_sizeChangedCallback(sample->presentationTime(), formatSize);
                else
                    sizeWillChangeAtTime(sample->presentationTime(), formatSize);
            }
        }

        ASSERT(m_videoRenderer);
        if (RefPtr videoRenderer = m_videoRenderer; videoRenderer && isEnabledVideoTrackId(trackId))
            videoRenderer->enqueueSample(sample, minimumUpcomingTime.value_or(sample->presentationTime()));
        break;

    case TrackType::Audio:
        ASSERT(mediaType == TrackType::Audio);
        if (mediaType != TrackType::Audio) {
            ERROR_LOG(LOGIDENTIFIER, "Expected sample of type: audio got: '", mediaType, "'. Bailing.");
            return;
        }
        if (RetainPtr audioRenderer = audioRendererFor(trackId)) {
            [audioRenderer enqueueSampleBuffer:cmSampleBuffer.get()];
            if (!allRenderersHaveAvailableSamples() && !sample->isNonDisplaying())
                setHasAvailableAudioSample(trackId, true);
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

bool AudioVideoRendererAVFObjC::isReadyForMoreSamples(TrackIdentifier trackId)
{
    auto type = typeOf(trackId);
    if (!type)
        return false;

    switch (*type) {
    case TrackType::Video:
        return m_readyToRequestVideoData && isEnabledVideoTrackId(trackId) && protectedVideoRenderer()->isReadyForMoreMediaData();
    case TrackType::Audio:
        if (!m_readyToRequestAudioData)
            return false;
        if (RetainPtr audioRenderer = audioRendererFor(trackId))
            return [audioRenderer isReadyForMoreMediaData];
        return false;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

Ref<AudioVideoRenderer::RequestPromise> AudioVideoRendererAVFObjC::requestMediaDataWhenReady(TrackIdentifier trackId)
{
    auto type = typeOf(trackId);
    if (!type)
        return RequestPromise::createAndReject(PlatformMediaError::LogicError);

    DEBUG_LOG(LOGIDENTIFIER, "trackId: ", toString(trackId), " isEnabledVideoTrackId: ", isEnabledVideoTrackId(trackId));

    switch (*type) {
    case TrackType::Video:
        ASSERT(m_videoRenderer);
        if (RefPtr videoRenderer = m_videoRenderer) {
            m_requestVideoPromise.emplace(PlatformMediaError::Cancelled);
            videoRenderer->requestMediaDataWhenReady([trackId, weakThis = WeakPtr { *this }] {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;
                if (!protectedThis->m_readyToRequestVideoData) {
                        DEBUG_LOG_WITH_THIS(protectedThis, LOGIDENTIFIER_WITH_THIS(protectedThis), "Not ready to request video data, ignoring");
                    return;
                }
                if (RefPtr videoRenderer = protectedThis->m_videoRenderer)
                    videoRenderer->stopRequestingMediaData();
                if (auto existingPromise = std::exchange(protectedThis->m_requestVideoPromise, std::nullopt))
                    existingPromise->resolve(trackId);
            });
            return m_requestVideoPromise->promise();
        }
        break;
    case TrackType::Audio:
        if (RetainPtr audioRenderer = audioRendererFor(trackId)) {
            auto& property = audioTrackPropertiesFor(trackId);
            property.requestPromise = makeUnique<RequestPromise::AutoRejectProducer>(PlatformMediaError::Cancelled);
            auto handler = makeBlockPtr([trackId, weakThis = WeakPtr { *this }] {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;
                if (!protectedThis->m_readyToRequestAudioData) {
                        DEBUG_LOG_WITH_THIS(protectedThis, LOGIDENTIFIER_WITH_THIS(protectedThis), "Not ready to request audio data, ignoring");
                    return;
                }

                RetainPtr audioRenderer = protectedThis->audioRendererFor(trackId);
                if (!audioRenderer)
                    return;
                [audioRenderer stopRequestingMediaData];
                auto& property = protectedThis->audioTrackPropertiesFor(trackId);
                if (auto existingPromise = std::exchange(property.requestPromise, nullptr))
                    existingPromise->resolve(trackId);
            });
            [audioRenderer requestMediaDataWhenReadyOnQueue:mainDispatchQueueSingleton() usingBlock:handler.get()];
            return property.requestPromise->promise();
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return RequestPromise::createAndReject(PlatformMediaError::LogicError);
}

void AudioVideoRendererAVFObjC::notifyTrackNeedsReenqueuing(TrackIdentifier trackId, Function<void(TrackIdentifier, const MediaTime&)>&& callback)
{
    auto type = typeOf(trackId);
    if (!type)
        return;

    switch (*type) {
    case TrackType::Video:
        break;
    case TrackType::Audio:
        ASSERT(m_audioTracksMap.contains(trackId));
        if (auto it = m_audioTracksMap.find(trackId); it != m_audioTracksMap.end())
            it->value.callbackForReenqueuing = WTFMove(callback);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void AudioVideoRendererAVFObjC::flush()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    cancelSeekingPromiseIfNeeded();
    if (m_seekState == RequiresFlush)
        m_seekState = Seeking;
    else {
        m_seekState = SeekCompleted;
        m_isSynchronizerSeeking = false;
    }

    flushVideo();
    flushAudio();
}

void AudioVideoRendererAVFObjC::flushTrack(TrackIdentifier trackId)
{
    DEBUG_LOG(LOGIDENTIFIER, toString(trackId));

    auto type = typeOf(trackId);
    if (!type)
        return;

    switch (*type) {
    case TrackType::Video:
        if (isEnabledVideoTrackId(trackId))
            flushVideo();
        break;
    case TrackType::Audio:
        flushAudioTrack(trackId);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void AudioVideoRendererAVFObjC::applicationWillResignActive()
{
    RefPtr videoRenderer = m_videoRenderer;
    if (!videoRenderer || !videoRenderer->isUsingDecompressionSession())
        return;

    if (!paused()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Playing; not invalidating VideoMediaSampleRenderer Decompression Session");
        return;
    }

    videoRenderer->invalidateDecompressionSession();
    ALWAYS_LOG(LOGIDENTIFIER, "Paused; invalidating VideoMediaSampleRenderer Decompression Session");
}

void AudioVideoRendererAVFObjC::notifyWhenErrorOccurs(Function<void(PlatformMediaError)>&& callback)
{
    m_errorCallback = WTFMove(callback);
}

// Synchronizer interface
void AudioVideoRendererAVFObjC::play(std::optional<MonotonicTime> hostTime)
{
    ALWAYS_LOG(LOGIDENTIFIER, "m_rate: ", m_rate, " seeking: ", seeking());
    m_isPlaying = true;
    if (!seeking())
        setSynchronizerRate(m_rate, hostTime);
}

void AudioVideoRendererAVFObjC::pause(std::optional<MonotonicTime> hostTime)
{
    ALWAYS_LOG(LOGIDENTIFIER, "m_rate: ", m_rate);
    m_isPlaying = false;
    setSynchronizerRate(0, hostTime);
}

bool AudioVideoRendererAVFObjC::paused() const
{
    return !m_isPlaying;
}

bool AudioVideoRendererAVFObjC::timeIsProgressing() const
{
    return m_isPlaying && [m_synchronizer rate];
}

MediaTime AudioVideoRendererAVFObjC::currentTime() const
{
    if (seeking())
        return m_lastSeekTime;

    // False positive see webkit.org/b/298024
    SUPPRESS_UNRETAINED_ARG MediaTime synchronizerTime = clampTimeToLastSeekTime(PAL::toMediaTime(PAL::CMTimebaseGetTime([m_synchronizer timebase])));
    if (synchronizerTime < MediaTime::zeroTime())
        return MediaTime::zeroTime();

    return synchronizerTime;
}

void AudioVideoRendererAVFObjC::setRate(double rate)
{
    ALWAYS_LOG(LOGIDENTIFIER, "m_rate: ", rate);

    if (m_rate == rate)
        return;
    m_rate = rate;
    RetainPtr algorithm = MediaSessionManagerCocoa::audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(m_pitchCorrectionAlgorithm.value_or(PitchCorrectionAlgorithm::BestAllAround), m_preservesPitch, m_rate).createNSString();
    applyOnAudioRenderers([&](auto* renderer) {
        setAudioTimePitchAlgorithm(renderer, algorithm.get());
    });

    if (shouldBePlaying())
        [m_synchronizer setRate:m_rate];
}

double AudioVideoRendererAVFObjC::effectiveRate() const
{
    // False positive see webkit.org/b/298024
    SUPPRESS_UNRETAINED_ARG return PAL::CMTimebaseGetRate([m_synchronizer timebase]);
}

void AudioVideoRendererAVFObjC::stall()
{
    ALWAYS_LOG(LOGIDENTIFIER, "playing: ", m_isPlaying, " rate: ", m_rate);
    setSynchronizerRate(0, { });
}

void AudioVideoRendererAVFObjC::notifyTimeReachedAndStall(const MediaTime& timeBoundary, Function<void(const MediaTime&)>&& callback)
{
    cancelTimeReachedAction();
    RetainPtr<NSArray> times = @[[NSValue valueWithCMTime:PAL::toCMTime(timeBoundary)]];

    auto logSiteIdentifier = LOGIDENTIFIER;
    DEBUG_LOG(logSiteIdentifier, timeBoundary);
    UNUSED_PARAM(logSiteIdentifier);

    m_currentTimeObserver = [m_synchronizer addBoundaryTimeObserverForTimes:times.get() queue:mainDispatchQueueSingleton() usingBlock:makeBlockPtr([weakThis = WeakPtr { *this }, timeBoundary, logSiteIdentifier, callback = WTFMove(callback)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        MediaTime now = protectedThis->currentTime();
        ALWAYS_LOG_WITH_THIS(protectedThis, logSiteIdentifier, "boundary time observer called, now: ", now);

        // Experimentation shows that between the time the boundary time observer is called, the time have progressed by a few milliseconds. Re-adjust time. This seek doesn't require re-enqueuing/flushing.
        [protectedThis->m_synchronizer setRate:0 time:PAL::toCMTime(timeBoundary)];

        callback(now);
    }).get()];
}

void AudioVideoRendererAVFObjC::cancelTimeReachedAction()
{
    if (RetainPtr observer = std::exchange(m_currentTimeObserver, nullptr))
        [m_synchronizer removeTimeObserver:observer.get()];
}

void AudioVideoRendererAVFObjC::performTaskAtTime(const MediaTime& time, Function<void(const MediaTime&)>&& task)
{
    if (m_performTaskObserver)
        [m_synchronizer removeTimeObserver:m_performTaskObserver.get()];

    RetainPtr<NSArray> times = @[[NSValue valueWithCMTime:PAL::toCMTime(time)]];

    auto logSiteIdentifier = LOGIDENTIFIER;
    DEBUG_LOG(logSiteIdentifier, time);
    UNUSED_PARAM(logSiteIdentifier);

    m_performTaskObserver = [m_synchronizer addBoundaryTimeObserverForTimes:times.get() queue:mainDispatchQueueSingleton() usingBlock:makeBlockPtr([weakThis = WeakPtr { *this }, task = WTFMove(task), logSiteIdentifier]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        MediaTime now = protectedThis->currentTime();
        ALWAYS_LOG_WITH_THIS(protectedThis, logSiteIdentifier, "boundary time observer called, now: ", now);

        task(now);
    }).get()];
}

void AudioVideoRendererAVFObjC::setTimeObserver(Seconds interval, Function<void(const MediaTime&)>&& callback)
{
    m_currentTimeDidChangeCallback = WTFMove(callback);

    cancelTimeObserver();

    if (m_currentTimeDidChangeCallback) {
        __block WeakPtr weakThis = *this;
        m_timeChangedObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::toCMTime(MediaTime::createWithSeconds(interval)) queue:mainDispatchQueueSingleton() usingBlock:^(CMTime time) {
            if (RefPtr protectedThis = weakThis.get()) {
                if (!protectedThis->m_currentTimeDidChangeCallback)
                    return;

                auto clampedTime = CMTIME_IS_NUMERIC(time) ? protectedThis->clampTimeToLastSeekTime(PAL::toMediaTime(time)) : MediaTime::zeroTime();
                protectedThis->m_currentTimeDidChangeCallback(clampedTime);
            }
        }];
    }
}

void AudioVideoRendererAVFObjC::cancelTimeObserver()
{
    if (RetainPtr observer = std::exchange(m_timeChangedObserver, { }))
        [m_synchronizer removeTimeObserver:observer.get()];
}

void AudioVideoRendererAVFObjC::prepareToSeek()
{
    ALWAYS_LOG(LOGIDENTIFIER, "state: ", toString(m_seekState));

    cancelSeekingPromiseIfNeeded();
    m_seekState = Preparing;
    stall();
}

Ref<MediaTimePromise> AudioVideoRendererAVFObjC::seekTo(const MediaTime& seekTime)
{
    ALWAYS_LOG(LOGIDENTIFIER, seekTime, "state: ", toString(m_seekState), " m_isSynchronizerSeeking: ", m_isSynchronizerSeeking, " hasAvailableVideoFrame: ", m_videoRenderer && allRenderersHaveAvailableSamples());

    cancelSeekingPromiseIfNeeded();
    if (m_seekState == RequiresFlush)
        return MediaTimePromise::createAndReject(PlatformMediaError::RequiresFlushToResume);

    m_lastSeekTime = seekTime;

    MediaTime synchronizerTime = PAL::toMediaTime([m_synchronizer currentTime]);

    bool isSynchronizerSeeking = m_isSynchronizerSeeking || std::abs((synchronizerTime - seekTime).toMicroseconds()) > 1000;

    if (!isSynchronizerSeeking && allRenderersHaveAvailableSamples()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Synchroniser doesn't require seeking current: ", synchronizerTime, " seeking: ", seekTime);
        // In cases where the destination seek time matches too closely the synchronizer's existing time
        // no time jumped notification will be issued. In this case, just notify the MediaPlayer that
        // the seek completed successfully.
        m_seekPromise.emplace();
        Ref promise = m_seekPromise->promise();
        maybeCompleteSeek();
        return promise;
    }

    m_isSynchronizerSeeking = isSynchronizerSeeking;
    [m_synchronizer setRate:0 time:PAL::toCMTime(seekTime)];

    if (m_seekState == SeekCompleted || m_seekState == Preparing) {
        m_seekState = RequiresFlush;
        m_readyToRequestAudioData = false;
        m_readyToRequestVideoData = false;
        ALWAYS_LOG(LOGIDENTIFIER, "Requesting Flush");
        return MediaTimePromise::createAndReject(PlatformMediaError::RequiresFlushToResume);
    }

    m_seekState = Seeking;

    m_seekPromise.emplace();
    return m_seekPromise->promise();
}

void AudioVideoRendererAVFObjC::notifyEffectiveRateChanged(Function<void(double)>&& callback)
{
    // False positive see webkit.org/b/298024
    SUPPRESS_UNRETAINED_ARG m_effectiveRateChangedListener = EffectiveRateChangedListener::create([callback = makeBlockPtr(WTFMove(callback))](double rate) mutable {
        callOnMainThread([callback, rate] {
            callback.get()(rate);
        });
    }, [m_synchronizer timebase]);
}

void AudioVideoRendererAVFObjC::setVolume(float volume)
{
    m_volume = volume;
    applyOnAudioRenderers([&](auto* renderer) {
        [renderer setVolume:volume];
    });
}

void AudioVideoRendererAVFObjC::setMuted(bool muted)
{
    m_muted = muted;
    applyOnAudioRenderers([&](auto* renderer) {
        [renderer setMuted:muted];
    });
}

void AudioVideoRendererAVFObjC::setPreservesPitchAndCorrectionAlgorithm(bool preservesPitch, std::optional<PitchCorrectionAlgorithm> pitchCorrectionAlgorithm)
{
    m_preservesPitch = preservesPitch;
    m_pitchCorrectionAlgorithm = pitchCorrectionAlgorithm;
    RetainPtr algorithm = MediaSessionManagerCocoa::audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(pitchCorrectionAlgorithm.value_or(PitchCorrectionAlgorithm::BestAllAround), preservesPitch, m_rate).createNSString();
    applyOnAudioRenderers([&](auto* renderer) {
        setAudioTimePitchAlgorithm(renderer, algorithm.get());
    });
}

void AudioVideoRendererAVFObjC::setAudioTimePitchAlgorithm(AVSampleBufferAudioRenderer *audioRenderer, NSString *algorithm) const
{
    [audioRenderer setAudioTimePitchAlgorithm:algorithm];
}

#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
void AudioVideoRendererAVFObjC::setOutputDeviceId(const String& outputDeviceId)
{
    m_audioOutputDeviceId = outputDeviceId;
    if (!outputDeviceId)
        return;
    applyOnAudioRenderers([&](auto* renderer) {
        setOutputDeviceIdOnRenderer(renderer);
    });
}

void AudioVideoRendererAVFObjC::setOutputDeviceIdOnRenderer(AVSampleBufferAudioRenderer *renderer)
{
    if (m_audioOutputDeviceId.isEmpty() || m_audioOutputDeviceId == AudioMediaStreamTrackRenderer::defaultDeviceID()) {
        // FIXME(rdar://155986053): Remove the @try/@catch when this exception is resolved.
        @try {
            [renderer setAudioOutputDeviceUniqueID:nil];
        } @catch(NSException *exception) {
            ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferAudioRenderer setAudioOutputDeviceUniqueID:] threw an exception: ", exception.name, ", reason : ", exception.reason);
        }
    } else
        [renderer setAudioOutputDeviceUniqueID:m_audioOutputDeviceId.createNSString().get()];
}
#endif

void AudioVideoRendererAVFObjC::setIsVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif
}

void AudioVideoRendererAVFObjC::setPresentationSize(const IntSize& newSize)
{
    m_presentationSize = newSize;
}

void AudioVideoRendererAVFObjC::setShouldMaintainAspectRatio(bool shouldMaintainAspectRatio)
{
    if (m_shouldMaintainAspectRatio == shouldMaintainAspectRatio)
        return;

    m_shouldMaintainAspectRatio = shouldMaintainAspectRatio;
    if (!m_sampleBufferDisplayLayer)
        return;

    [CATransaction begin];
    [CATransaction setAnimationDuration:0];
    [CATransaction setDisableActions:YES];

    // False positive see webkit.org/b/298035
    SUPPRESS_UNRETAINED_ARG [m_sampleBufferDisplayLayer setVideoGravity: (m_shouldMaintainAspectRatio ? AVLayerVideoGravityResizeAspect : AVLayerVideoGravityResize)];

    [CATransaction commit];
}

void AudioVideoRendererAVFObjC::renderingCanBeAcceleratedChanged(bool isAccelerated)
{
    m_renderingCanBeAccelerated = isAccelerated;
    if (isAccelerated)
        updateDisplayLayerIfNeeded();
}

void AudioVideoRendererAVFObjC::contentBoxRectChanged(const LayoutRect& newRect)
{
    if (!layerOrVideoRenderer() && !newRect.isEmpty())
        updateDisplayLayerIfNeeded();
}

void AudioVideoRendererAVFObjC::notifyFirstFrameAvailable(Function<void()>&& callback)
{
    m_firstFrameAvailableCallback = WTFMove(callback);
}

void AudioVideoRendererAVFObjC::notifyWhenHasAvailableVideoFrame(Function<void(const MediaTime&, double)>&& callback)
{
    m_hasAvailableVideoFrameCallback = WTFMove(callback);
    configureHasAvailableVideoFrameCallbackIfNeeded();
}

void AudioVideoRendererAVFObjC::notifyWhenRequiresFlushToResume(Function<void()>&& callback)
{
    m_notifyWhenRequiresFlushToResume = WTFMove(callback);
}

void AudioVideoRendererAVFObjC::notifyRenderingModeChanged(Function<void()>&& callback)
{
    m_renderingModeChangedCallback = WTFMove(callback);
}

void AudioVideoRendererAVFObjC::expectMinimumUpcomingPresentationTime(const MediaTime& presentationTime)
{
    if (!m_enabledVideoTrackId)
        return;
    if (RefPtr videoRenderer = m_videoRenderer)
        videoRenderer->expectMinimumUpcomingSampleBufferPresentationTime(presentationTime);
}

void AudioVideoRendererAVFObjC::notifySizeChanged(Function<void(const MediaTime&, FloatSize)>&& callback)
{
    m_sizeChangedCallback = WTFMove(callback);
}

void AudioVideoRendererAVFObjC::setShouldDisableHDR(bool shouldDisable)
{
    m_shouldDisableHDR = shouldDisable;
    if (![m_sampleBufferDisplayLayer respondsToSelector:@selector(setToneMapToStandardDynamicRange:)])
        return;

    ALWAYS_LOG(LOGIDENTIFIER, shouldDisable);
    [m_sampleBufferDisplayLayer setToneMapToStandardDynamicRange:shouldDisable];
}

void AudioVideoRendererAVFObjC::setPlatformDynamicRangeLimit(const PlatformDynamicRangeLimit& platformDynamicRangeLimit)
{
    m_dynamicRangeLimit = platformDynamicRangeLimit;

    if (!m_sampleBufferDisplayLayer)
        return;

    setLayerDynamicRangeLimit(m_sampleBufferDisplayLayer.get(), platformDynamicRangeLimit);
}

RefPtr<VideoFrame> AudioVideoRendererAVFObjC::currentVideoFrame() const
{
    RefPtr videoRenderer = m_videoRenderer;
    if (!videoRenderer)
        return nullptr;

    auto entry = videoRenderer->copyDisplayedPixelBuffer();
    if (!entry.pixelBuffer)
        return nullptr;

    return VideoFrameCV::create(entry.presentationTimeStamp, false, VideoFrame::Rotation::None, entry.pixelBuffer.get());
}

void AudioVideoRendererAVFObjC::paintCurrentVideoFrameInContext(GraphicsContext& context, const FloatRect& outputRect)
{
    if (context.paintingDisabled())
        return;

    auto image = currentNativeImage();
    if (!image)
        return;

    GraphicsContextStateSaver stateSaver(context);
    FloatRect imageRect { FloatPoint::zero(), image->size() };
    context.drawNativeImage(*image, outputRect, imageRect);
}

std::optional<VideoPlaybackQualityMetrics> AudioVideoRendererAVFObjC::videoPlaybackQualityMetrics()
{
    RefPtr videoRenderer = m_videoRenderer;
    if (!videoRenderer)
        return std::nullopt;

    return VideoPlaybackQualityMetrics {
        videoRenderer->totalVideoFrames(),
        videoRenderer->droppedVideoFrames(),
        videoRenderer->corruptedVideoFrames(),
        videoRenderer->totalFrameDelay().toDouble(),
        videoRenderer->totalDisplayedFrames()
    };
}

PlatformLayer* AudioVideoRendererAVFObjC::platformVideoLayer() const
{
    if (!m_videoRenderer)
        return nullptr;
    return m_videoLayerManager->videoInlineLayer();
}

void AudioVideoRendererAVFObjC::setVideoLayerSizeFenced(const FloatSize& newSize, WTF::MachSendRightAnnotated&&)
{
    if (!layerOrVideoRenderer() && !newSize.isEmpty())
        updateDisplayLayerIfNeeded();
}

void AudioVideoRendererAVFObjC::setVideoFullscreenLayer(PlatformLayer *videoFullscreenLayer, WTF::Function<void()>&& completionHandler)
{
    RefPtr currentImage = currentNativeImage();
    m_videoLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler), currentImage ? currentImage->platformImage() : nullptr);
}

void AudioVideoRendererAVFObjC::setVideoFullscreenFrame(const FloatRect& frame)
{
    m_videoLayerManager->setVideoFullscreenFrame(frame);
}

void AudioVideoRendererAVFObjC::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    RetainPtr representationLayer = representation ? representation->platformLayer() : nil;
    m_videoLayerManager->setTextTrackRepresentationLayer(representationLayer.get());
}

void AudioVideoRendererAVFObjC::syncTextTrackBounds()
{
    m_videoLayerManager->syncTextTrackBounds();
}

Ref<GenericPromise> AudioVideoRendererAVFObjC::setVideoTarget(const PlatformVideoTarget& videoTarget)
{
#if !ENABLE(LINEAR_MEDIA_PLAYER)
    UNUSED_PARAM(videoTarget);
    return GenericPromise::createAndReject();
#else
    m_videoTarget = videoTarget;
    return updateDisplayLayerIfNeeded();
#endif
}

void AudioVideoRendererAVFObjC::isInFullscreenOrPictureInPictureChanged(bool isInFullscreenOrPictureInPicture)
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    ALWAYS_LOG(LOGIDENTIFIER, isInFullscreenOrPictureInPicture);
    destroyExpiringVideoRenderersIfNeeded();
#else
    UNUSED_PARAM(isInFullscreenOrPictureInPicture);
#endif
}

RetainPtr<AVSampleBufferAudioRenderer> AudioVideoRendererAVFObjC::audioRendererFor(TrackIdentifier trackId) const
{
    auto itRenderer = m_audioRenderers.find(trackId);
    if (itRenderer == m_audioRenderers.end())
        return nil;
    return itRenderer->value;
}

void AudioVideoRendererAVFObjC::applyOnAudioRenderers(NOESCAPE Function<void(AVSampleBufferAudioRenderer *)>&& function) const
{
    for (auto& pair : m_audioRenderers) {
        RetainPtr renderer = pair.value;
        function(renderer.get());
    }
}

void AudioVideoRendererAVFObjC::addAudioRenderer(TrackIdentifier trackId)
{
    ASSERT(!m_audioRenderers.contains(trackId));

    if (!m_audioTracksMap.add(trackId, AudioTrackProperties()).isNewEntry)
        return;

    RetainPtr renderer = [adoptNS(PAL::allocAVSampleBufferAudioRendererInstance()) init];

    if (!renderer) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferAudioRenderer init] returned nil! bailing!");
        ASSERT_NOT_REACHED();

        notifyError(PlatformMediaError::AudioDecodingError);
        return;
    }

    [renderer setMuted:m_muted];
    [renderer setVolume:m_volume];
    RetainPtr algorithm = MediaSessionManagerCocoa::audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(m_pitchCorrectionAlgorithm.value_or(PitchCorrectionAlgorithm::BestAllAround), m_preservesPitch, m_rate).createNSString();
    setAudioTimePitchAlgorithm(renderer.get(), algorithm.get());

#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    if (!!m_audioOutputDeviceId)
        setOutputDeviceIdOnRenderer(renderer.get());
#endif

    @try {
        [m_synchronizer addRenderer:renderer.get()];
    } @catch(NSException *exception) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer addRenderer:] threw an exception: ", exception.name, ", reason : ", exception.reason);
        ASSERT_NOT_REACHED();

        notifyError(PlatformMediaError::AudioDecodingError);
        return;
    }

    m_audioRenderers.set(trackId, renderer);
    m_listener->beginObservingAudioRenderer(renderer.get());
}

void AudioVideoRendererAVFObjC::removeAudioRenderer(TrackIdentifier trackId)
{
    if (RetainPtr audioRenderer = audioRendererFor(trackId)) {
        destroyAudioRenderer(audioRenderer);
        m_audioRenderers.remove(trackId);
        ASSERT(m_audioTracksMap.contains(trackId));
        m_audioTracksMap.remove(trackId);
    }
}

void AudioVideoRendererAVFObjC::destroyAudioRenderer(RetainPtr<AVSampleBufferAudioRenderer> renderer)
{
    removeRendererFromSynchronizerIfNeeded(renderer.get());
    m_listener->stopObservingAudioRenderer(renderer.get());
    [renderer flush];
    [renderer stopRequestingMediaData];
}

void AudioVideoRendererAVFObjC::destroyAudioRenderers()
{
    for (auto& pair : m_audioRenderers) {
        auto renderer = pair.value;
        destroyAudioRenderer(renderer);
    }
    m_audioRenderers.clear();
}

bool AudioVideoRendererAVFObjC::seeking() const
{
    return m_seekState != SeekCompleted;
}

MediaTime AudioVideoRendererAVFObjC::clampTimeToLastSeekTime(const MediaTime& time) const
{
    if (m_lastSeekTime.isFinite() && time < m_lastSeekTime)
        return m_lastSeekTime;

    return time;
}

void AudioVideoRendererAVFObjC::maybeCompleteSeek()
{
    ALWAYS_LOG(LOGIDENTIFIER, "state: ", toString(m_seekState));

    if (m_seekState == SeekCompleted || !m_seekPromise)
        return;

    if (m_videoRenderer && !allRenderersHaveAvailableSamples()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Waiting for all frames");
        m_seekState = WaitingForAvailableFame;
        return;
    }
    m_seekState = Seeking;
    if (m_isSynchronizerSeeking) {
        ALWAYS_LOG(LOGIDENTIFIER, "Waiting on synchronizer to complete seeking");
        return;
    }
    m_seekState = SeekCompleted;
    if (shouldBePlaying())
        setSynchronizerRate(m_rate, { });
    else
        ALWAYS_LOG(LOGIDENTIFIER, "Not resuming playback, shouldBePlaying:false");

    if (auto promise = std::exchange(m_seekPromise, std::nullopt))
        promise->resolve(m_lastSeekTime);
    ALWAYS_LOG(LOGIDENTIFIER, "seek completed");
}

bool AudioVideoRendererAVFObjC::shouldBePlaying() const
{
    return m_isPlaying && !seeking() && allRenderersHaveAvailableSamples();
}

void AudioVideoRendererAVFObjC::setHasAvailableAudioSample(TrackIdentifier trackId, bool flag)
{
    if (flag && allRenderersHaveAvailableSamples())
        return;

    auto it = m_audioTracksMap.find(trackId);
    if (it == m_audioTracksMap.end())
        return;
    auto& properties = it->value;
    if (properties.hasAudibleSample == flag)
        return;
    ALWAYS_LOG(LOGIDENTIFIER, flag);
    properties.hasAudibleSample = flag;

    updateAllRenderersHaveAvailableSamples();
}

AudioVideoRendererAVFObjC::AudioTrackProperties& AudioVideoRendererAVFObjC::audioTrackPropertiesFor(TrackIdentifier trackId)
{
    auto it = m_audioTracksMap.find(trackId);
    RELEASE_ASSERT(it != m_audioTracksMap.end());
    return it->value;
}

void AudioVideoRendererAVFObjC::updateAllRenderersHaveAvailableSamples()
{
    bool allRenderersHaveAvailableSamples = [&] {
        if (m_enabledVideoTrackId && !m_hasAvailableVideoFrame)
            return false;
        for (auto& properties : m_audioTracksMap.values()) {
            if (!properties.hasAudibleSample)
                return false;
        }
        return true;
    }();

    if (m_allRenderersHaveAvailableSamples == allRenderersHaveAvailableSamples)
        return;

    DEBUG_LOG(LOGIDENTIFIER, allRenderersHaveAvailableSamples);
    m_allRenderersHaveAvailableSamples = allRenderersHaveAvailableSamples;

    if (allRenderersHaveAvailableSamples)
        maybeCompleteSeek();
    if (shouldBePlaying() && [m_synchronizer rate] != m_rate)
        [m_synchronizer setRate:m_rate];
    else if (!shouldBePlaying() && [m_synchronizer rate])
        stall();
}

void AudioVideoRendererAVFObjC::setHasAvailableVideoFrame(bool hasAvailableVideoFrame)
{
    if (std::exchange(m_hasAvailableVideoFrame, hasAvailableVideoFrame) == hasAvailableVideoFrame)
        return;
    if (hasAvailableVideoFrame) {
        if (m_firstFrameAvailableCallback)
            m_firstFrameAvailableCallback();
        setNeedsPlaceholderImage(false);
    }
    updateAllRenderersHaveAvailableSamples();
}

std::optional<TracksRendererManager::TrackType> AudioVideoRendererAVFObjC::typeOf(TrackIdentifier trackId) const
{
    if (auto it = m_trackTypes.find(trackId); it != m_trackTypes.end())
        return it->value;
    return { };
}

Ref<GenericPromise> AudioVideoRendererAVFObjC::updateDisplayLayerIfNeeded()
{
    if (!m_enabledVideoTrackId)
        return GenericPromise::createAndResolve();

    if (shouldEnsureLayerOrVideoRenderer())
        return ensureLayerOrVideoRenderer();

    // Using renderless video renderer.
    return setVideoRenderer(nil);
}

bool AudioVideoRendererAVFObjC::shouldEnsureLayerOrVideoRenderer() const
{
    if (!canUseDecompressionSession())
        return true;

    return m_renderingCanBeAccelerated;
}

WebSampleBufferVideoRendering *AudioVideoRendererAVFObjC::layerOrVideoRenderer() const
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    switch (acceleratedVideoMode()) {
    case AcceleratedVideoMode::Layer:
        return m_sampleBufferDisplayLayer.get();
    case AcceleratedVideoMode::VideoRenderer:
        return m_sampleBufferVideoRenderer.get();
    }
#else
    return m_sampleBufferDisplayLayer.get();
#endif
}

Ref<GenericPromise> AudioVideoRendererAVFObjC::ensureLayerOrVideoRenderer()
{
    switch (acceleratedVideoMode()) {
    case AcceleratedVideoMode::Layer:
        ensureLayer();
        break;
    case AcceleratedVideoMode::VideoRenderer:
        ensureVideoRenderer();
        break;
    }

    RetainPtr renderer = layerOrVideoRenderer();
    if (!renderer) {
        notifyError(PlatformMediaError::DecoderCreationError);
        return GenericPromise::createAndReject();
    }
    RefPtr videoRenderer = m_videoRenderer;

    bool needsRenderingModeChanged = (!videoRenderer && renderer) || (videoRenderer && videoRenderer->renderer() != renderer.get());

    ALWAYS_LOG(LOGIDENTIFIER, acceleratedVideoMode(), ", renderer=", !!renderer);

    Ref promise = setVideoRenderer(renderer.get());

    if (needsRenderingModeChanged && m_renderingModeChangedCallback)
        m_renderingModeChangedCallback();

    return promise;
}

void AudioVideoRendererAVFObjC::ensureLayer()
{
    if (m_sampleBufferDisplayLayer) {
        if (m_sampleBufferDisplayLayerState == SampleBufferLayerState::AddedToSynchronizer)
            return;
        configureLayerOrVideoRenderer(m_sampleBufferDisplayLayer.get());
        return;
    }

    m_sampleBufferDisplayLayer = [adoptNS(PAL::allocAVSampleBufferDisplayLayerInstance()) init];
    if (!m_sampleBufferDisplayLayer) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferDisplayLayer init] returned nil! bailing!");
        return;
    }
    [m_sampleBufferDisplayLayer setName:@"AudioVideoRendererAVFObjC AVSampleBufferDisplayLayer"];
    // False positive see webkit.org/b/298035
    SUPPRESS_UNRETAINED_ARG [m_sampleBufferDisplayLayer setVideoGravity:(m_shouldMaintainAspectRatio ? AVLayerVideoGravityResizeAspect : AVLayerVideoGravityResize)];

    configureLayerOrVideoRenderer(m_sampleBufferDisplayLayer.get());

    if ([m_sampleBufferDisplayLayer respondsToSelector:@selector(setToneMapToStandardDynamicRange:)])
        [m_sampleBufferDisplayLayer setToneMapToStandardDynamicRange:m_shouldDisableHDR];

    setLayerDynamicRangeLimit(m_sampleBufferDisplayLayer.get(), m_dynamicRangeLimit);

    m_videoLayerManager->setVideoLayer(m_sampleBufferDisplayLayer.get(), m_presentationSize);
}

void AudioVideoRendererAVFObjC::destroyLayer()
{
    if (!m_sampleBufferDisplayLayer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    removeRendererFromSynchronizerIfNeeded(m_sampleBufferDisplayLayer.get());

    m_videoLayerManager->didDestroyVideoLayer();

    m_sampleBufferDisplayLayer = nullptr;
}

void AudioVideoRendererAVFObjC::ensureVideoRenderer()
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    destroyExpiringVideoRenderersIfNeeded();

    if (m_sampleBufferVideoRenderer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_sampleBufferVideoRenderer = [adoptNS(PAL::allocAVSampleBufferVideoRendererInstance()) init];
    if (!m_sampleBufferVideoRenderer) {
        ERROR_LOG(LOGIDENTIFIER, "-[VSampleBufferVideoRenderer init] returned nil! bailing!");
        return;
    }

    [m_sampleBufferVideoRenderer addVideoTarget:m_videoTarget.get()];

    configureLayerOrVideoRenderer(m_sampleBufferVideoRenderer.get());
#endif // ENABLE(LINEAR_MEDIA_PLAYER)
}

void AudioVideoRendererAVFObjC::destroyVideoRenderer()
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (!m_sampleBufferVideoRenderer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    removeRendererFromSynchronizerIfNeeded(m_sampleBufferVideoRenderer.get());

    if ([m_sampleBufferVideoRenderer respondsToSelector:@selector(removeAllVideoTargets)])
        [m_sampleBufferVideoRenderer removeAllVideoTargets];
    m_sampleBufferVideoRenderer = nullptr;
#endif // ENABLE(LINEAR_MEDIA_PLAYER)
}

void AudioVideoRendererAVFObjC::destroyExpiringVideoRenderersIfNeeded()
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    for (RetainPtr renderer : std::exchange(m_expiringSampleBufferVideoRenderers, { }))
        [renderer removeAllVideoTargets];
#endif
}

Ref<GenericPromise> AudioVideoRendererAVFObjC::setVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
    ALWAYS_LOG(LOGIDENTIFIER, "!!renderer = ", !!renderer);

    if (m_videoRenderer)
        return stageVideoRenderer(renderer);

    if (!renderer) {
        destroyLayer();
        destroyVideoRenderer();
    }

    RefPtr videoRenderer = VideoMediaSampleRenderer::create(renderer, logger(), logIdentifier());
    m_videoRenderer = videoRenderer;

    videoRenderer->setPreferences(m_preferences);
    // False positive see webkit.org/b/298024
    SUPPRESS_UNRETAINED_ARG videoRenderer->setTimebase([m_synchronizer timebase]);
    videoRenderer->notifyWhenDecodingErrorOccurred([weakThis = WeakPtr { *this }](NSError *error) {
        if (RefPtr protectedThis = weakThis.get()) {
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
            if ([error code] == 'HDCP') {
                bool obscured = [[retainPtr([error userInfo]) valueForKey:@"obscured"] boolValue];
                if (RefPtr cdmInstance = protectedThis->m_cdmInstance)
                    cdmInstance->setHDCPStatus(obscured ? CDMInstance::HDCPStatus::OutputRestricted : CDMInstance::HDCPStatus::Valid);
                return;
            }
#else
            UNUSED_PARAM(error);
#endif
            protectedThis->notifyError(PlatformMediaError::VideoDecodingError);
        }
    });
    videoRenderer->notifyFirstFrameAvailable([weakThis = WeakPtr { *this }](const MediaTime&, double) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->setHasAvailableVideoFrame(true);
    });
    configureHasAvailableVideoFrameCallbackIfNeeded();
    videoRenderer->notifyWhenVideoRendererRequiresFlushToResumeDecoding([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->notifyRequiresFlushToResume();
    });
    videoRenderer->setResourceOwner(m_resourceOwner);

    return GenericPromise::createAndResolve();
}

void AudioVideoRendererAVFObjC::configureHasAvailableVideoFrameCallbackIfNeeded()
{
    if (RetainPtr observer = std::exchange(m_videoFrameMetadataGatheringObserver, nil))
        [m_synchronizer removeTimeObserver:observer.get()];

    if (!m_hasAvailableVideoFrameCallback)
        return;

    RefPtr videoRenderer = m_videoRenderer;
    if (videoRenderer) {
        videoRenderer->notifyWhenHasAvailableVideoFrame([weakThis = WeakPtr { *this }](const MediaTime& presentationTime, double displayTime) {
            if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_hasAvailableVideoFrameCallback)
                protectedThis->m_hasAvailableVideoFrameCallback(presentationTime, displayTime);
        });
    }
    if (isUsingDecompressionSession())
        return;

    m_preferences |= VideoRendererPreference::PrefersDecompressionSession;
    if (videoRenderer)
        videoRenderer->setPreferences(m_preferences);

    if (m_previousRendererConfiguration.hasVideoTrack) {
        // Activating AvailableVideoFrame callback may force the use of decompression session.
        updateDisplayLayerIfNeeded();
    }

    if (willUseDecompressionSessionIfNeeded())
        return;

    m_videoFrameMetadataGatheringObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::CMTimeMake(1, 60) queue:mainDispatchQueueSingleton() usingBlock:[weakThis = WeakPtr { *this }](CMTime currentCMTime) {
        ensureOnMainThread([weakThis, currentCMTime] {
            if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_hasAvailableVideoFrameCallback)
                protectedThis->m_hasAvailableVideoFrameCallback(PAL::toMediaTime(currentCMTime), (MonotonicTime::now() - protectedThis->m_startupTime).seconds());
        });
    }];
}

void AudioVideoRendererAVFObjC::configureLayerOrVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif

    renderer.preventsDisplaySleepDuringVideoPlayback = NO;

    if ([renderer respondsToSelector:@selector(setPreventsAutomaticBackgroundingDuringVideoPlayback:)])
        renderer.preventsAutomaticBackgroundingDuringVideoPlayback = NO;

    bool isAVSBDL = is_objc<AVSampleBufferDisplayLayer>(renderer);
    if (isAVSBDL && m_sampleBufferDisplayLayerState == SampleBufferLayerState::AddedToSynchronizer)
        return;

    @try {
        [m_synchronizer addRenderer:renderer];
    } @catch(NSException *exception) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer addRenderer:] threw an exception: ", exception.name, ", reason : ", exception.reason);
        ASSERT_NOT_REACHED();

        notifyError(PlatformMediaError::DecoderCreationError);
        return;
    }

    if (isAVSBDL)
        m_sampleBufferDisplayLayerState = SampleBufferLayerState::AddedToSynchronizer;
}

RefPtr<VideoMediaSampleRenderer> AudioVideoRendererAVFObjC::protectedVideoRenderer() const
{
    return m_videoRenderer;
}

void AudioVideoRendererAVFObjC::sizeWillChangeAtTime(const MediaTime& time, const FloatSize& size)
{
    if (!m_sizeChangedCallback)
        return;

    NSArray* times = @[[NSValue valueWithCMTime:PAL::toCMTime(time)]];
    RetainPtr<id> observer = [m_synchronizer addBoundaryTimeObserverForTimes:times queue:mainDispatchQueueSingleton() usingBlock:makeBlockPtr([weakThis = WeakPtr { *this }, time, size] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        ASSERT(!protectedThis->m_sizeChangeObservers.isEmpty());
        if (!protectedThis->m_sizeChangeObservers.isEmpty()) {
            RetainPtr<id> observer = protectedThis->m_sizeChangeObservers.takeFirst();
            [protectedThis->m_synchronizer removeTimeObserver:observer.get()];
        }
        if (protectedThis->m_sizeChangedCallback)
            protectedThis->m_sizeChangedCallback(time, size);
    }).get()];
    m_sizeChangeObservers.append(WTFMove(observer));

    if (currentTime() >= time && m_sizeChangedCallback)
        m_sizeChangedCallback(currentTime(), size);
}

void AudioVideoRendererAVFObjC::flushPendingSizeChanges()
{
    m_cachedSize.reset();
    while (!m_sizeChangeObservers.isEmpty()) {
        RetainPtr<id> observer = m_sizeChangeObservers.takeFirst();
        [m_synchronizer removeTimeObserver:observer.get()];
    }
}

bool AudioVideoRendererAVFObjC::canUseDecompressionSession() const
{
    if (!m_preferences.contains(VideoRendererPreference::PrefersDecompressionSession))
        return false;
    if (m_preferences.contains(VideoRendererPreference::UseDecompressionSessionForProtectedContent))
        return true;
    return !m_hasProtectedVideoContent;
}

bool AudioVideoRendererAVFObjC::isUsingDecompressionSession() const
{
    return m_videoRenderer && m_videoRenderer->isUsingDecompressionSession();
}

bool AudioVideoRendererAVFObjC::willUseDecompressionSessionIfNeeded() const
{
    if (!canUseDecompressionSession())
        return false;

    return m_preferences.contains(VideoRendererPreference::PrefersDecompressionSession) || m_hasAvailableVideoFrameCallback;
}

Ref<GenericPromise> AudioVideoRendererAVFObjC::stageVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
    ASSERT(m_videoRenderer);

    RefPtr videoRenderer = m_videoRenderer;
    RendererConfiguration newConfiguration {
        .canUseDecompressionSession = willUseDecompressionSessionIfNeeded(),
        .isProtected = m_hasProtectedVideoContent,
        .hasVideoTrack = m_enabledVideoTrackId.has_value()
    };
    if (renderer == videoRenderer->renderer()) {
        if (std::exchange(m_previousRendererConfiguration, newConfiguration) != newConfiguration && renderer)
            notifyRequiresFlushToResume();
        return GenericPromise::createAndResolve();
    }
    ASSERT(!renderer || hasSelectedVideo());

    RetainPtr<WebSampleBufferVideoRendering> rendererToExpire;
    if (renderer) {
        switch (acceleratedVideoMode()) {
        case AcceleratedVideoMode::Layer:
            m_expiringSampleBufferVideoRenderers.append(m_sampleBufferVideoRenderer);
            rendererToExpire = std::exchange(m_sampleBufferVideoRenderer, { });
            break;
        case AcceleratedVideoMode::VideoRenderer:
            // We only need to remove the AVSampleBufferDisplayLayer from the synchronizer.
            rendererToExpire = m_sampleBufferDisplayLayer;
            m_sampleBufferDisplayLayerState = SampleBufferLayerState::PendingRemovalFromSynchronizer;
            break;
        }
    } else {
        destroyLayer();
        destroyVideoRenderer();
    }

    bool videoTrackChangeOnly = !m_previousRendererConfiguration.hasVideoTrack && newConfiguration.hasVideoTrack;
    bool flushRequired = std::exchange(m_previousRendererConfiguration, newConfiguration) != newConfiguration && !videoTrackChangeOnly;
    m_readyToRequestVideoData = !flushRequired;
    ALWAYS_LOG(LOGIDENTIFIER, "renderer: ", !!renderer, " videoTrackChangeOnly: ", videoTrackChangeOnly, " flushRequired: ", flushRequired);

    return videoRenderer->changeRenderer(renderer)->whenSettled(RunLoop::mainSingleton(), [weakThis = WeakPtr { *this }, rendererToExpire = WTFMove(rendererToExpire), flushRequired]() {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return GenericPromise::createAndReject();
        if (rendererToExpire)
            protectedThis->removeRendererFromSynchronizerIfNeeded(rendererToExpire.get());
        if (flushRequired)
            protectedThis->notifyRequiresFlushToResume();
        return GenericPromise::createAndResolve();
    });
}

void AudioVideoRendererAVFObjC::destroyVideoTrack()
{
    if (RefPtr videoRenderer = std::exchange(m_videoRenderer, { }))
        videoRenderer->shutdown();
    destroyLayer();
    destroyVideoRenderer();
    m_enabledVideoTrackId.reset();
}

void AudioVideoRendererAVFObjC::removeRendererFromSynchronizerIfNeeded(id renderer)
{
    bool isAVSBDL = is_objc<AVSampleBufferDisplayLayer>(renderer);
    if (isAVSBDL && m_sampleBufferDisplayLayerState == SampleBufferLayerState::RemovedFromSynchronizer)
        return;
    // False positive see webkit.org/b/298024
    SUPPRESS_UNRETAINED_ARG CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:renderer atTime:currentTime completionHandler:nil];
    if (isAVSBDL)
        m_sampleBufferDisplayLayerState = SampleBufferLayerState::RemovedFromSynchronizer;
}

AudioVideoRendererAVFObjC::AcceleratedVideoMode AudioVideoRendererAVFObjC::acceleratedVideoMode() const
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (m_videoTarget)
        return AcceleratedVideoMode::VideoRenderer;
#endif // ENABLE(LINEAR_MEDIA_PLAYER)

    return AcceleratedVideoMode::Layer;
}

void AudioVideoRendererAVFObjC::notifyError(PlatformMediaError error)
{
    if (m_errorCallback)
        m_errorCallback(error);
}

void AudioVideoRendererAVFObjC::audioRendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError *)
{
    notifyError(PlatformMediaError::AudioDecodingError);
}

void AudioVideoRendererAVFObjC::audioRendererWasAutomaticallyFlushed(AVSampleBufferAudioRenderer *renderer, const CMTime& cmTime)
{
    std::optional<TrackIdentifier> trackId;
    for (auto& pair : m_audioRenderers) {
        if (pair.value.get() == renderer) {
            trackId = pair.key;
            break;
        }
    }
    if (!trackId) {
        ERROR_LOG(LOGIDENTIFIER, "Couldn't find track attached to Audio Renderer.");
        return;
    }
    auto it = m_audioTracksMap.find(*trackId);
    if (it == m_audioTracksMap.end())
        return;
    auto& properties = it->value;
    if (!properties.callbackForReenqueuing)
        return;
    properties.callbackForReenqueuing(*trackId, PAL::toMediaTime(cmTime));
}

#if HAVE(SPATIAL_TRACKING_LABEL)
void AudioVideoRendererAVFObjC::setSpatialTrackingInfo(bool prefersSpatialAudioExperience, SoundStageSize soundStage, const String& sceneIdentifier, const String& defaultLabel, const String& label)
{
    if (m_prefersSpatialAudioExperience == prefersSpatialAudioExperience
        && m_soundStage == soundStage
        && m_sceneIdentifier == sceneIdentifier
        && m_defaultSpatialTrackingLabel == defaultLabel
        && m_spatialTrackingLabel == label)
        return;

    m_prefersSpatialAudioExperience = prefersSpatialAudioExperience;
    m_soundStage = soundStage;
    m_sceneIdentifier = sceneIdentifier;
    m_defaultSpatialTrackingLabel = defaultLabel;
    m_spatialTrackingLabel = label;

    ALWAYS_LOG(LOGIDENTIFIER, "prefersSpatialAudioExperience(", prefersSpatialAudioExperience, "), soundStage(", soundStage, "), sceneIdentifier(", sceneIdentifier, "), defaultLabel(", defaultLabel, "), label(", label, ")");
    updateSpatialTrackingLabel();
}

void AudioVideoRendererAVFObjC::updateSpatialTrackingLabel()
{
    RetainPtr renderer = m_sampleBufferVideoRenderer ? m_sampleBufferVideoRenderer.get() : [m_sampleBufferDisplayLayer sampleBufferRenderer];

#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
    if (m_prefersSpatialAudioExperience) {
        RetainPtr experience = createSpatialAudioExperienceWithOptions({
            .hasLayer = !!renderer,
            .hasTarget = !!m_videoTarget,
            .isVisible = m_visible,
            .soundStageSize = m_soundStage,
            .sceneIdentifier = m_sceneIdentifier,
#if HAVE(SPATIAL_TRACKING_LABEL)
            .spatialTrackingLabel = m_spatialTrackingLabel,
#endif
        });
        ALWAYS_LOG(LOGIDENTIFIER, "Setting spatialAudioExperience: ", spatialAudioExperienceDescription(experience.get()));
        [m_synchronizer setIntendedSpatialAudioExperience:experience.get()];
        return;
    }
#endif

    if (!m_spatialTrackingLabel.isNull()) {
        INFO_LOG(LOGIDENTIFIER, "Explicitly set STSLabel: ", m_spatialTrackingLabel);
        [renderer setSTSLabel:m_spatialTrackingLabel.createNSString().get()];
        return;
    }

    if (renderer && m_visible) {
        // Let AVSBRS manage setting the spatial tracking label in its video renderer itself.
        INFO_LOG(LOGIDENTIFIER, "Has visible renderer, set STSLabel: nil");
        [renderer setSTSLabel:nil];
        return;
    }

    if (m_audioRenderers.isEmpty()) {
        // If there are no audio renderers, there's nothing to do.
        INFO_LOG(LOGIDENTIFIER, "No audio renderers - no-op");
        return;
    }

    // If there is no video renderer, use the default spatial tracking label if available, or
    // the session's spatial tracking label if not, and set the label directly on each audio
    // renderer.
    AVAudioSession *session = [PAL::getAVAudioSessionClassSingleton() sharedInstance];
    RetainPtr<NSString> defaultLabel;
    if (!m_defaultSpatialTrackingLabel.isNull()) {
        INFO_LOG(LOGIDENTIFIER, "Default STSLabel: ", m_defaultSpatialTrackingLabel);
        defaultLabel = m_defaultSpatialTrackingLabel.createNSString();
    } else {
        INFO_LOG(LOGIDENTIFIER, "AVAudioSession label: ", session.spatialTrackingLabel);
        defaultLabel = session.spatialTrackingLabel;
    }
    for (auto& pair : m_audioRenderers)
        [(__bridge AVSampleBufferAudioRenderer *)pair.value.get() setSTSLabel:defaultLabel.get()];
}
#endif

#if HAVE(AVCONTENTKEYSESSION)
#if ENABLE(ENCRYPTED_MEDIA)
void AudioVideoRendererAVFObjC::setCDMInstance(CDMInstance* instance)
{
    RefPtr fpsInstance = dynamicDowncast<CDMInstanceFairPlayStreamingAVFObjC>(instance);
    if (fpsInstance == m_cdmInstance)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);
    if (RefPtr cdmInstance = m_cdmInstance)
        cdmInstance->removeKeyStatusesChangedObserver(m_keyStatusesChangedObserver);

    m_cdmInstance = fpsInstance;
    if (fpsInstance)
        fpsInstance->addKeyStatusesChangedObserver(m_keyStatusesChangedObserver);

    attemptToDecrypt();
}

Ref<MediaPromise> AudioVideoRendererAVFObjC::setInitData(Ref<SharedBuffer> initData)
{
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    m_initData = initData.copyRef();
    if (RefPtr session = m_session.get()) {
        session->setInitData(initData);
        return MediaPromise::createAndResolve();
    }
#endif
    auto keyIDs = CDMPrivateFairPlayStreaming::extractKeyIDsSinf(initData);
#if HAVE(FAIRPLAYSTREAMING_MTPS_INITDATA)
    if (!keyIDs)
        keyIDs = CDMPrivateFairPlayStreaming::extractKeyIDsMpts(initData);
#endif
    if (!keyIDs)
        return MediaPromise::createAndResolve();

    if (RefPtr cdmInstance = m_cdmInstance) {
        if (RefPtr instanceSession = cdmInstance->sessionForKeyIDs(keyIDs.value()))
            return MediaPromise::createAndResolve();
    }

    m_keyIDs = WTFMove(keyIDs.value());
    return MediaPromise::createAndReject(PlatformMediaError::CDMInstanceKeyNeeded);
}

void AudioVideoRendererAVFObjC::attemptToDecrypt()
{
    if (m_blockedSamples.isEmpty())
        return;
    if (m_cdmInstance && m_keyIDs.isEmpty()) {
        ALWAYS_LOG(LOGIDENTIFIER, "CDMInstance set, but no keyIDs");
        return;
    }

    if (RefPtr cdmInstance = m_cdmInstance) {
        RefPtr instanceSession = cdmInstance->sessionForKeyIDs(m_keyIDs);
        if (!instanceSession)
            return;
    } else if (!m_session.get())
        return;

    tryToEnqueueBlockedSamples();
}

void AudioVideoRendererAVFObjC::tryToEnqueueBlockedSamples()
{
    while (!m_blockedSamples.isEmpty()) {
        auto& firstPair = m_blockedSamples.first();

        // If we still can't enqueue the sample, bail.
        if (!canEnqueueSample(firstPair.first, firstPair.second))
            return;

        auto firstPairTaken = m_blockedSamples.takeFirst();
        enqueueSample(firstPairTaken.first, WTFMove(firstPairTaken.second), { });
    }
}

bool AudioVideoRendererAVFObjC::canEnqueueSample(TrackIdentifier trackId, const MediaSample& sample)
{
    // if sample is unencrytped: enqueue sample
    if (!sample.isProtected())
        return true;

    // if sample is encrypted, but we are not attached to a CDM: do not enqueue sample.
    if (!m_cdmInstance && !m_session.get())
        return false;

    if (typeOf(trackId) == TrackType::Video && !isEnabledVideoTrackId(trackId))
        return false;

    Ref sampleAVFObjC = downcast<MediaSampleAVFObjC>(sample);

    // if sample is encrypted, and keyIDs match the current set of keyIDs: enqueue sample.
    if (auto findResult = m_currentTrackIds.find(trackId); findResult != m_currentTrackIds.end() && findResult->value == sampleAVFObjC->keyIDs())
        return true;

    // if sample's set of keyIDs does not match the current set of keyIDs, consult with the CDM
    // to determine if the keyIDs are usable; if so, update the current set of keyIDs and enqueue sample.
    if (RefPtr cdmInstance = m_cdmInstance; cdmInstance && cdmInstance->isAnyKeyUsable(sampleAVFObjC->keyIDs())) {
        m_currentTrackIds.add(trackId, sampleAVFObjC->keyIDs());
        return true;
    }

    if (RefPtr session = m_session.get(); session && session->isAnyKeyUsable(sampleAVFObjC->keyIDs())) {
        m_currentTrackIds.add(trackId, sampleAVFObjC->keyIDs());
        return true;
    }

    ALWAYS_LOG(LOGIDENTIFIER, "Can't enqueue sample: ", sample, " no suitable CDM");
    return false;
}

void AudioVideoRendererAVFObjC::attachContentKeyToSampleIfNeeded(const MediaSample& sample)
{
    if (RefPtr cdmInstance = m_cdmInstance)
        cdmInstance->attachContentKeyToSample(downcast<MediaSampleAVFObjC>(sample));
    else if (RefPtr session = m_session.get())
        session->attachContentKeyToSample(downcast<MediaSampleAVFObjC>(sample));
}
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
void AudioVideoRendererAVFObjC::setCDMSession(LegacyCDMSession* session)
{
    RefPtr oldSession = m_session.get();
    if (session == oldSession)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    if (RefPtr oldSession = m_session.get())
        oldSession->removeRenderer(*this);

    m_session = dynamicDowncast<CDMSessionAVContentKeySession>(session);

    if (RefPtr session = m_session.get()) {
        session->addRenderer(*this);
        if (RefPtr initData = m_initData)
            session->setInitData(*initData);
        attemptToDecrypt();
    }
}
#endif
#endif

void AudioVideoRendererAVFObjC::setSynchronizerRate(float rate, std::optional<MonotonicTime> hostTime)
{
    if (hostTime) {
        auto cmHostTime = PAL::CMClockMakeHostTimeFromSystemUnits(hostTime->toMachAbsoluteTime());
        ALWAYS_LOG(LOGIDENTIFIER, "setting rate to: ", m_rate, " at host time: ", PAL::CMTimeGetSeconds(cmHostTime));
        [m_synchronizer setRate:rate time:PAL::kCMTimeInvalid atHostTime:cmHostTime];
    } else
        [m_synchronizer setRate:rate];

    // If we are pausing the synchronizer, update the last image to ensure we have something
    // to display if and when the decoders are purged while in the background. And vice-versa,
    // purge our retained images and pixel buffers when playing the synchronizer, to release that
    // retained memory.
    if (!rate)
        updateLastPixelBuffer();
    else
        maybePurgeLastPixelBuffer();
}

RefPtr<NativeImage> AudioVideoRendererAVFObjC::currentNativeImage() const
{
    if (RefPtr videoFrame = currentVideoFrame()) {
        if (!m_rgbConformer) {
            auto attributes = @{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) };
            m_rgbConformer = makeUnique<PixelBufferConformerCV>((__bridge CFDictionaryRef)attributes);
        }
        RetainPtr pixelBuffer = videoFrame->pixelBuffer();
        return NativeImage::create(m_rgbConformer->createImageFromPixelBuffer(pixelBuffer.get()));
    }
    return nullptr;
}

bool AudioVideoRendererAVFObjC::updateLastPixelBuffer()
{
    RefPtr videoRenderer = m_videoRenderer;
    if (!videoRenderer)
        return false;
    auto entry = videoRenderer->copyDisplayedPixelBuffer();
    if (!entry.pixelBuffer)
        return false;

    INFO_LOG(LOGIDENTIFIER, "displayed pixelbuffer copied for time: ", entry.presentationTimeStamp);
    m_lastPixelBuffer = WTFMove(entry.pixelBuffer);
    return true;
}

void AudioVideoRendererAVFObjC::maybePurgeLastPixelBuffer()
{
    m_lastPixelBuffer = nullptr;
}

void AudioVideoRendererAVFObjC::setNeedsPlaceholderImage(bool needsPlaceholder)
{
    if (m_needsPlaceholderImage == needsPlaceholder)
        return;
    m_needsPlaceholderImage = needsPlaceholder;

    if (m_needsPlaceholderImage)
        [m_sampleBufferDisplayLayer setContents:(id)m_lastPixelBuffer.get()];
    else
        [m_sampleBufferDisplayLayer setContents:nil];
}

bool AudioVideoRendererAVFObjC::isEnabledVideoTrackId(TrackIdentifier trackId) const
{
    return m_enabledVideoTrackId == trackId;
}

bool AudioVideoRendererAVFObjC::hasSelectedVideo() const
{
    return !!m_enabledVideoTrackId;
}

void AudioVideoRendererAVFObjC::flushVideo()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    setHasAvailableVideoFrame(false);
    // Flush may call immediately requestMediaDataWhenReady. Must clear m_readyToRequestVideoData before flushing renderer.
    m_readyToRequestVideoData = true;
    if (RefPtr videoRenderer = m_videoRenderer)
        videoRenderer->flush();
    flushPendingSizeChanges();
}

void AudioVideoRendererAVFObjC::flushAudio()
{
    for (auto& properties : m_audioTracksMap.values())
        properties.hasAudibleSample = false;
    updateAllRenderersHaveAvailableSamples();

    applyOnAudioRenderers([&](auto *renderer) {
        [renderer flush];
    });
    m_readyToRequestAudioData = true;
}

void AudioVideoRendererAVFObjC::flushAudioTrack(TrackIdentifier trackId)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    RetainPtr audioRenderer = audioRendererFor(trackId);
    if (!audioRenderer)
        return;
    [audioRenderer flush];
    setHasAvailableAudioSample(trackId, false);
}

void AudioVideoRendererAVFObjC::notifyRequiresFlushToResume()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_readyToRequestVideoData = false;
    setNeedsPlaceholderImage(true);
    if (m_notifyWhenRequiresFlushToResume)
        m_notifyWhenRequiresFlushToResume();
}

void AudioVideoRendererAVFObjC::cancelSeekingPromiseIfNeeded()
{
    if (auto promise = std::exchange(m_seekPromise, std::nullopt))
        promise->reject(PlatformMediaError::Cancelled);
}

WTFLogChannel& AudioVideoRendererAVFObjC::logChannel() const
{
    return LogMedia;
}

String AudioVideoRendererAVFObjC::toString(TrackIdentifier trackId) const
{
    StringBuilder builder;

    builder.append('[');
    if (auto type = typeOf(trackId)) {
        switch (*type) {
        case TrackType::Video:
            builder.append("video:"_s);
            break;
        case TrackType::Audio:
            builder.append("audio:"_s);
            break;
        default:
            builder.append("unknown:"_s);
            break;
        }
    } else
        builder.append("NotFound:"_s);
    builder.append(trackId.loggingString());
    builder.append(']');
    return builder.toString();
}

String AudioVideoRendererAVFObjC::toString(SeekState state) const
{
    switch (state) {
    case Preparing:
        return "Preparing"_s;
    case RequiresFlush:
        return "RequiresFlush"_s;
    case Seeking:
        return "Seeking"_s;
    case WaitingForAvailableFame:
        return "WaitingForAvailableFame"_s;
    case SeekCompleted:
        return "SeekCompleted"_s;
    }
}

} // namespace WebCore
