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
#import "LayoutRect.h"
#import "Logging.h"
#import "PixelBufferConformerCV.h"
#import "PlatformDynamicRangeLimitCocoa.h"
#import "SpatialAudioExperienceHelper.h"
#import "TextTrackRepresentation.h"
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

#pragma mark - Soft Linking
#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

@interface AVSampleBufferDisplayLayer (Staging_100128644)
@property (assign, nonatomic) BOOL preventsAutomaticBackgroundingDuringVideoPlayback;
@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AudioVideoRendererAVFObjC);

AudioVideoRendererAVFObjC::AudioVideoRendererAVFObjC(const Logger& originalLogger, uint64_t logSiteIdentifier)
    : m_logger(originalLogger)
    , m_logIdentifier(logSiteIdentifier)
    , m_videoLayerManager(makeUniqueRef<VideoLayerManagerObjC>(originalLogger, logSiteIdentifier))
    , m_synchronizer(adoptNS([PAL::allocAVSampleBufferRenderSynchronizerInstanceSingleton() init]))
    , m_listener(WebAVSampleBufferListener::create(*this))
{
    // addPeriodicTimeObserverForInterval: throws an exception if you pass a non-numeric CMTime, so just use
    // an arbitrarily large time value of once an hour:
    __block WeakPtr weakThis { *this };
    // False positive webkit.org/b/298037
    SUPPRESS_UNRETAINED_ARG m_timeJumpedObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::toCMTime(MediaTime::createWithDouble(3600)) queue:dispatch_get_main_queue() usingBlock:^(CMTime time) {
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
    [m_synchronizer setRate:0];
}

AudioVideoRendererAVFObjC::~AudioVideoRendererAVFObjC()
{
    cancelSeekingPromiseIfNeeded();

    if (m_durationObserver)
        [m_synchronizer removeTimeObserver:m_durationObserver.get()];
    if (m_timeJumpedObserver)
        [m_synchronizer removeTimeObserver:m_timeJumpedObserver.get()];

    destroyLayer();
    destroyAudioRenderers();
    m_listener->invalidate();
}

TracksRendererManager::TrackIdentifier AudioVideoRendererAVFObjC::addTrack(TrackType type)
{
    auto identifier = TrackIdentifier::generate();
    m_trackTypes.add(identifier, type);

    switch (type) {
    case TrackType::Video:
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
    auto type = typeOf(trackId);
    if (!type)
        return;

    switch (*type) {
    case TrackType::Video:
        if (isEnabledVideoTrackId(trackId)) {
            m_enabledVideoTrackId.reset();
            if (RefPtr videoRenderer = m_videoRenderer)
                videoRenderer->stopRequestingMediaData();
        }
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

    switch (*type) {
    case TrackType::Video:
        ASSERT(m_videoRenderer);
        if (RefPtr videoRenderer = m_videoRenderer; videoRenderer && isEnabledVideoTrackId(trackId))
            videoRenderer->enqueueSample(sample, minimumUpcomingTime.value_or(sample->presentationTime()));
        break;
    case TrackType::Audio:
        if (RetainPtr audioRenderer = audioRendererFor(trackId)) {
            RetainPtr cmSampleBuffer = sample->platformSample().sample.cmSampleBuffer;
            [audioRenderer enqueueSampleBuffer:cmSampleBuffer.get()];
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
        return isEnabledVideoTrackId(trackId) && protectedVideoRenderer()->isReadyForMoreMediaData();
    case TrackType::Audio:
        if (RetainPtr audioRenderer = audioRendererFor(trackId))
            return [audioRenderer isReadyForMoreMediaData];
        return false;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

void AudioVideoRendererAVFObjC::requestMediaDataWhenReady(TrackIdentifier trackId, Function<void(TrackIdentifier)>&& callback)
{
    auto type = typeOf(trackId);
    if (!type)
        return;

    DEBUG_LOG(LOGIDENTIFIER, "trackId: ", toString(trackId), " isEnabledVideoTrackId: ", isEnabledVideoTrackId(trackId));

    switch (*type) {
    case TrackType::Video:
        ASSERT(m_videoRenderer);
        if (RefPtr videoRenderer = m_videoRenderer) {
            videoRenderer->requestMediaDataWhenReady([trackId, weakThis = WeakPtr { *this }, callback = WTFMove(callback)]() mutable {
                if (RefPtr protectedThis = weakThis.get()) {
                    if (protectedThis->m_readyToRequestVideoData)
                        callback(trackId);
                    else
                        DEBUG_LOG_WITH_THIS(protectedThis, LOGIDENTIFIER_WITH_THIS(protectedThis), "Not ready to request video data, ignoring");
                }
            });
        }
        break;
    case TrackType::Audio:
        if (RetainPtr audioRenderer = audioRendererFor(trackId)) {
            auto handler = makeBlockPtr([trackId, weakThis = WeakPtr { *this }, callback = WTFMove(callback)]() mutable {
                if (RefPtr protectedThis = weakThis.get()) {
                    if (protectedThis->m_readyToRequestAudioData)
                        callback(trackId);
                    else
                        DEBUG_LOG_WITH_THIS(protectedThis, LOGIDENTIFIER_WITH_THIS(protectedThis), "Not ready to request audio data, ignoring");
                }
            });
            // False positive webkit.org/b/298037
            SUPPRESS_UNRETAINED_ARG [audioRenderer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:handler.get()];
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void AudioVideoRendererAVFObjC::stopRequestingMediaData(TrackIdentifier trackId)
{
    auto type = typeOf(trackId);
    if (!type)
        return;

    switch (*type) {
    case TrackType::Video:
        if (RefPtr videoRenderer = m_videoRenderer; videoRenderer && isEnabledVideoTrackId(trackId))
            videoRenderer->stopRequestingMediaData();
        break;
    case TrackType::Audio:
        if (RetainPtr audioRenderer = audioRendererFor(trackId))
            [audioRenderer stopRequestingMediaData];
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

void AudioVideoRendererAVFObjC::notifyWhenErrorOccurs(Function<void(PlatformMediaError)>&& callback)
{
    m_errorCallback = WTFMove(callback);
}

// Synchronizer interface
void AudioVideoRendererAVFObjC::play()
{
    ALWAYS_LOG(LOGIDENTIFIER, "m_rate: ", m_rate, " seeking: ", seeking());
    m_isPlaying = true;
    if (!seeking())
        [m_synchronizer setRate:m_rate];
}

void AudioVideoRendererAVFObjC::pause()
{
    ALWAYS_LOG(LOGIDENTIFIER, "m_rate: ", m_rate);
    m_isPlaying = false;
    [m_synchronizer setRate:0];
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
    if (shouldBePlaying())
        [m_synchronizer setRate:m_rate];
}

double AudioVideoRendererAVFObjC::effectiveRate() const
{
    // False positive see webkit.org/b/298024
    SUPPRESS_UNRETAINED_ARG return PAL::CMTimebaseGetRate([m_synchronizer timebase]);
}

void AudioVideoRendererAVFObjC::setDuration(MediaTime duration)
{
    if (m_durationObserver)
        [m_synchronizer removeTimeObserver:m_durationObserver.get()];

    NSArray* times = @[[NSValue valueWithCMTime:PAL::toCMTime(duration)]];

    auto logSiteIdentifier = LOGIDENTIFIER;
    DEBUG_LOG(logSiteIdentifier, duration);
    UNUSED_PARAM(logSiteIdentifier);

    // False positive webkit.org/b/298037
    SUPPRESS_UNRETAINED_ARG m_durationObserver = [m_synchronizer addBoundaryTimeObserverForTimes:times queue:dispatch_get_main_queue() usingBlock:[weakThis = WeakPtr { *this }, duration, logSiteIdentifier] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        MediaTime now = protectedThis->currentTime();
        ALWAYS_LOG_WITH_THIS(protectedThis, logSiteIdentifier, "boundary time observer called, now = ", now);

        protectedThis->pause();
        if (now < duration) {
            ERROR_LOG_WITH_THIS(protectedThis, logSiteIdentifier, "ERROR: boundary time observer called before duration");
            [protectedThis->m_synchronizer setRate:0 time:PAL::toCMTime(duration)];
        }
        if (protectedThis->m_durationReachedCallback)
            protectedThis->m_durationReachedCallback(now);
    }];
}

void AudioVideoRendererAVFObjC::notifyDurationReached(Function<void(const MediaTime&)>&& callback)
{
    m_durationReachedCallback = WTFMove(callback);
}

void AudioVideoRendererAVFObjC::prepareToSeek()
{
    ALWAYS_LOG(LOGIDENTIFIER, "state: ", toString(m_seekState));

    cancelSeekingPromiseIfNeeded();
    m_seekState = Preparing;
    [m_synchronizer setRate:0];
}

Ref<MediaTimePromise> AudioVideoRendererAVFObjC::seekTo(const MediaTime& seekTime)
{
    ALWAYS_LOG(LOGIDENTIFIER, seekTime, "state: ", toString(m_seekState), " m_isSynchronizerSeeking: ", m_isSynchronizerSeeking, " m_hasAvailableVideoFrame: ", m_hasAvailableVideoFrame);

    cancelSeekingPromiseIfNeeded();
    if (m_seekState == RequiresFlush)
        return MediaTimePromise::createAndReject(PlatformMediaError::RequiresFlushToResume);

    m_lastSeekTime = seekTime;

    MediaTime synchronizerTime = PAL::toMediaTime([m_synchronizer currentTime]);

    bool isSynchronizerSeeking = m_isSynchronizerSeeking || std::abs((synchronizerTime - seekTime).toMicroseconds()) > 1000;

    if (!isSynchronizerSeeking) {
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

void AudioVideoRendererAVFObjC::setPreservesPitch(bool preservePitch)
{
    m_preservePitch = preservePitch;
    applyOnAudioRenderers([&](auto* renderer) {
        // False positive see webkit.org/b/298035
        SUPPRESS_UNRETAINED_ARG [renderer setAudioTimePitchAlgorithm:preservePitch ? AVAudioTimePitchAlgorithmSpectral : AVAudioTimePitchAlgorithmVarispeed];
    });
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
    updateDisplayLayerIfNeeded();
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

void AudioVideoRendererAVFObjC::acceleratedRenderingStateChanged(bool isAccelerated)
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
    RefPtr videoRenderer = m_videoRenderer;
    if (m_hasAvailableVideoFrameCallback && videoRenderer) {
        videoRenderer->notifyWhenHasAvailableVideoFrame([weakThis = WeakPtr { *this }](const MediaTime& presentationTime, double displayTime) {
            if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_hasAvailableVideoFrameCallback)
                protectedThis->m_hasAvailableVideoFrameCallback(presentationTime, displayTime);
        });
    }
}

void AudioVideoRendererAVFObjC::notifyWhenRequiresFlushToResume(Function<void()>&& callback)
{
    m_notifyWhenRequiresFlushToResume = WTFMove(callback);
}

void AudioVideoRendererAVFObjC::notifyRenderingModeChanged(Function<void()>&& callback)
{
    m_renderingModeChangedCallback = WTFMove(callback);
}

void AudioVideoRendererAVFObjC::setMinimumUpcomingPresentationTime(const MediaTime&)
{
    // TODO: need to pass it to the videoRenderer. Not needed yet for webm or when decompression session is always in use
    ASSERT_NOT_REACHED();
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

PlatformLayerContainer AudioVideoRendererAVFObjC::platformVideoLayer() const
{
    if (!m_videoRenderer)
        return nullptr;
    return m_videoLayerManager->videoInlineLayer();
}

void AudioVideoRendererAVFObjC::setVideoFullscreenLayer(PlatformLayer *videoFullscreenLayer, WTF::Function<void()>&& completionHandler)
{
    RefPtr videoFrame = currentVideoFrame();

    if (!m_rgbConformer) {
        auto attributes = @{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) };
        m_rgbConformer = makeUnique<PixelBufferConformerCV>((__bridge CFDictionaryRef)attributes);
    }
    RefPtr<NativeImage> currentFrameImage = videoFrame ? RefPtr { NativeImage::create(m_rgbConformer->createImageFromPixelBuffer(videoFrame->protectedPixelBuffer().get())) } : nullptr;
    m_videoLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler), currentFrameImage ? currentFrameImage->platformImage() : nullptr);
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
    if (acceleratedVideoMode() == AcceleratedVideoMode::VideoRenderer)
        destroyVideoLayerIfNeeded();
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

    RetainPtr renderer = adoptNS([PAL::allocAVSampleBufferAudioRendererInstanceSingleton() init]);

    if (!renderer) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferAudioRenderer init] returned nil! bailing!");
        ASSERT_NOT_REACHED();

        notifyError(PlatformMediaError::AudioDecodingError);
        return;
    }

    [renderer setMuted:m_muted];
    [renderer setVolume:m_volume];
    // False positive see webkit.org/b/298035
    SUPPRESS_UNRETAINED_ARG [renderer setAudioTimePitchAlgorithm:(m_preservePitch ? AVAudioTimePitchAlgorithmSpectral : AVAudioTimePitchAlgorithmVarispeed)];

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
    }
}

void AudioVideoRendererAVFObjC::destroyAudioRenderer(RetainPtr<AVSampleBufferAudioRenderer> renderer)
{
    // False positive see webkit.org/b/298024
    SUPPRESS_UNRETAINED_ARG CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:renderer.get() atTime:currentTime completionHandler:nil];

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

    if (m_videoRenderer && !m_hasAvailableVideoFrame) {
        ALWAYS_LOG(LOGIDENTIFIER, "Waiting for first video frame");
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
        [m_synchronizer setRate:m_rate];
    else
        ALWAYS_LOG(LOGIDENTIFIER, "Not resuming playback, shouldBePlaying:false");

    if (auto promise = std::exchange(m_seekPromise, std::nullopt))
        promise->resolve();
    ALWAYS_LOG(LOGIDENTIFIER, "seek completed");
}

bool AudioVideoRendererAVFObjC::shouldBePlaying() const
{
    return m_isPlaying && !seeking();
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
    return ((m_sampleBufferDisplayLayer && !CGRectIsEmpty([m_sampleBufferDisplayLayer bounds])) || (!m_presentationSize.isEmpty() && m_renderingCanBeAccelerated));
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
    if (m_sampleBufferDisplayLayer)
        return;

    destroyVideoLayerIfNeeded();

    m_sampleBufferDisplayLayer = adoptNS([PAL::allocAVSampleBufferDisplayLayerInstanceSingleton() init]);
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

    // False positive see webkit.org/b/298024
    SUPPRESS_UNRETAINED_ARG CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:m_sampleBufferDisplayLayer.get() atTime:currentTime completionHandler:nil];

    m_videoLayerManager->didDestroyVideoLayer();
    m_sampleBufferDisplayLayer = nullptr;
    m_needsDestroyVideoLayer = false;
}

void AudioVideoRendererAVFObjC::destroyVideoLayerIfNeeded()
{
    if (!m_needsDestroyVideoLayer)
        return;
    m_needsDestroyVideoLayer = false;
    m_videoLayerManager->didDestroyVideoLayer();
}

void AudioVideoRendererAVFObjC::ensureVideoRenderer()
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (m_sampleBufferVideoRenderer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_sampleBufferVideoRenderer = adoptNS([PAL::allocAVSampleBufferVideoRendererInstanceSingleton() init]);
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

    // False positive see webkit.org/b/298024
    SUPPRESS_UNRETAINED_ARG CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:m_sampleBufferVideoRenderer.get() atTime:currentTime completionHandler:nil];

    if ([m_sampleBufferVideoRenderer respondsToSelector:@selector(removeAllVideoTargets)])
        [m_sampleBufferVideoRenderer removeAllVideoTargets];
    m_sampleBufferVideoRenderer = nullptr;
#endif // ENABLE(LINEAR_MEDIA_PLAYER)
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

    RefPtr videoRenderer = VideoMediaSampleRenderer::create(renderer);
    m_videoRenderer = videoRenderer;
    videoRenderer->setPreferences(VideoMediaSampleRendererPreference::PrefersDecompressionSession);
    // False positive see webkit.org/b/298024
    SUPPRESS_UNRETAINED_ARG videoRenderer->setTimebase([m_synchronizer timebase]);
    videoRenderer->notifyWhenDecodingErrorOccurred([weakThis = WeakPtr { *this }](NSError *) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->notifyError(PlatformMediaError::VideoDecodingError);
    });
    videoRenderer->notifyFirstFrameAvailable([weakThis = WeakPtr { *this }](const MediaTime&, double) {
        if (RefPtr protectedThis = weakThis.get()) {
            protectedThis->m_hasAvailableVideoFrame = true;
            if (protectedThis->m_firstFrameAvailableCallback)
                protectedThis->m_firstFrameAvailableCallback();
            protectedThis->maybeCompleteSeek();
        }
    });
    if (m_hasAvailableVideoFrameCallback) {
        videoRenderer->notifyWhenHasAvailableVideoFrame([weakThis = WeakPtr { *this }](const MediaTime& presentationTime, double displayTime) {
            if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_hasAvailableVideoFrameCallback)
                protectedThis->m_hasAvailableVideoFrameCallback(presentationTime, displayTime);
        });
    }
    videoRenderer->notifyWhenVideoRendererRequiresFlushToResumeDecoding([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get()) {
            ALWAYS_LOG_WITH_THIS(protectedThis, LOGIDENTIFIER_WITH_THIS(protectedThis), "VideoRendererRequiresFlushToResumeDecoding");
            protectedThis->m_readyToRequestVideoData = false;
            if (protectedThis->m_notifyWhenRequiresFlushToResume)
                protectedThis->m_notifyWhenRequiresFlushToResume();
        }
    });
    videoRenderer->setResourceOwner(m_resourceOwner);

    return GenericPromise::createAndResolve();
}

void AudioVideoRendererAVFObjC::configureLayerOrVideoRenderer(WebSampleBufferVideoRendering *renderer)
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

        notifyError(PlatformMediaError::DecoderCreationError);
        return;
    }
}

RefPtr<VideoMediaSampleRenderer> AudioVideoRendererAVFObjC::protectedVideoRenderer() const
{
    return m_videoRenderer;
}

Ref<GenericPromise> AudioVideoRendererAVFObjC::stageVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
    ASSERT(m_videoRenderer);

    RefPtr videoRenderer = m_videoRenderer;
    if (renderer == videoRenderer->renderer())
        return GenericPromise::createAndResolve();

    ALWAYS_LOG(LOGIDENTIFIER, "renderer: ", !!renderer);
    ASSERT(!renderer || hasSelectedVideo());

    Vector<RetainPtr<WebSampleBufferVideoRendering>> renderersToExpire { 2u };
    if (renderer) {
        switch (acceleratedVideoMode()) {
        case AcceleratedVideoMode::Layer:
            renderersToExpire.append(std::exchange(m_sampleBufferVideoRenderer, { }));
            break;
        case AcceleratedVideoMode::VideoRenderer:
            m_needsDestroyVideoLayer = true;
            renderersToExpire.append(std::exchange(m_sampleBufferDisplayLayer, { }));
            break;
        }
    } else {
        destroyLayer();
        destroyVideoRenderer();
    }

    return videoRenderer->changeRenderer(renderer)->whenSettled(RunLoop::mainSingleton(), [weakThis = WeakPtr { *this }, renderersToExpire = WTFMove(renderersToExpire)]() mutable {
        RefPtr protectedThis = weakThis.get();
        for (auto& rendererToExpire : renderersToExpire) {
            if (!rendererToExpire)
                continue;
            if (protectedThis) {
                // False positive see webkit.org/b/298024
                SUPPRESS_UNRETAINED_ARG CMTime currentTime = PAL::CMTimebaseGetTime([protectedThis->m_synchronizer timebase]);
                [protectedThis->m_synchronizer removeRenderer:rendererToExpire.get() atTime:currentTime completionHandler:nil];
            }
        }
        if (protectedThis)
            return GenericPromise::createAndResolve();
        return GenericPromise::createAndReject();
    });
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

#if HAVE(SPATIAL_TRACKING_LABEL)
void AudioVideoRendererAVFObjC::setSpatialTrackingInfo(bool prefersSpatialAudioExperience, SoundStageSize soundStage, const String& sceneIdentifier, const String& defaultLabel, const String& label)
{
    m_prefersSpatialAudioExperience = prefersSpatialAudioExperience;
    m_soundStage = soundStage;
    m_sceneIdentifier = sceneIdentifier;
    m_defaultSpatialTrackingLabel = defaultLabel;
    m_spatialTrackingLabel = label;

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
    AVAudioSession *session = [PAL::getAVAudioSessionClass() sharedInstance];
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
    if (RefPtr videoRenderer = m_videoRenderer)
        videoRenderer->flush();
    m_hasAvailableVideoFrame = false;
    m_readyToRequestVideoData = true;
}

void AudioVideoRendererAVFObjC::flushAudio()
{
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
