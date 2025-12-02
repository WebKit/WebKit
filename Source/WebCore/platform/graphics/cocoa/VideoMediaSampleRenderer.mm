/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#import "VideoMediaSampleRenderer.h"

#import "EffectiveRateChangedListener.h"
#import "FourCC.h"
#import "IOSurface.h"
#import "Logging.h"
#import "MediaSampleAVFObjC.h"
#import "VP9UtilitiesCocoa.h"
#import "WebCoreDecompressionSession.h"
#import "WebSampleBufferVideoRendering.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CMFormatDescription.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/Locker.h>
#import <wtf/MainThreadDispatcher.h>
#import <wtf/MonotonicTime.h>
#import <wtf/NativePromise.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/darwin/DispatchExtras.h>

#if PLATFORM(VISION)
#import "FormatDescriptionUtilities.h"
#import "SpatialVideoMetadata.h"
#import "VideoProjectionMetadata.h"
#endif

#pragma mark - Soft Linking

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

@interface AVSampleBufferDisplayLayer (WebCoreAVSampleBufferDisplayLayerQueueManagementPrivate)
- (void)expectMinimumUpcomingSampleBufferPresentationTime:(CMTime)minimumUpcomingPresentationTime;
@end

#if ENABLE(LINEAR_MEDIA_PLAYER)
@interface AVSampleBufferVideoRenderer (Staging_127455709)
- (void)removeAllVideoTargets;
@end
#endif

#if HAVE(RECOMMENDED_PIXEL_ATTRIBUTES_API)
@interface AVSampleBufferVideoRenderer (Staging_152246223)
@property (readonly, nonnull) NSDictionary<NSString*, NS_SWIFT_SENDABLE id> *recommendedPixelBufferAttributes;
@end
#endif

namespace WebCore {

static constexpr CMItemCount SampleQueueHighWaterMark = 30;
static constexpr CMItemCount SampleQueueLowWaterMark = 15;

static constexpr MediaTime DecodeLowWaterMark { 100, 1000 };
static constexpr MediaTime DecodeHighWaterMark { 133, 1000 };

static bool isRendererThreadSafe(WebSampleBufferVideoRendering *renderering)
{
    if (!renderering)
        return true;
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    return true;
#else
    return false;
#endif
}

#define LogPerformance(...) RELEASE_LOG_DEBUG_IF(LOG_CHANNEL(MediaPerformance).level >= WTFLogLevel::Debug, MediaPerformance, __VA_ARGS__)

WorkQueue& VideoMediaSampleRenderer::queueSingleton()
{
    static std::once_flag onceKey;
    static LazyNeverDestroyed<Ref<WorkQueue>> workQueue;
    std::call_once(onceKey, [] {
        workQueue.construct(WorkQueue::create("VideoMediaSampleRenderer Queue"_s, WorkQueue::QOS::UserInteractive));
    });
    return workQueue.get();
}

VideoMediaSampleRenderer::VideoMediaSampleRenderer(WebSampleBufferVideoRendering *renderer, const Logger& logger, uint64_t logIdentifier)
    : m_rendererIsThreadSafe(isRendererThreadSafe(renderer))
    , m_displayLayer(dynamic_objc_cast<AVSampleBufferDisplayLayer>(renderer))
    , m_listener(WebAVSampleBufferListener::create(*this))
    , m_frameRateMonitor([](auto info) {
        RELEASE_LOG_ERROR(MediaPerformance, "VideoMediaSampleRenderer Frame decoding allowance exceeded:%f expected:%f", Seconds { info.lastFrameTime - info.frameTime }.value(), 1 / info.observedFrameRate);
    })
#if !RELEASE_LOG_DISABLED
    , m_logger(logger)
    , m_logIdentifier(logIdentifier)
#endif
{
    if (!renderer)
        return;

    m_listener->beginObservingVideoRenderer(renderer);

#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    if (RetainPtr videoRenderer = videoRendererFor(renderer)) {
        m_mainRenderer = videoRenderer;
        m_renderer = WTFMove(videoRenderer);
    }
    ASSERT(m_renderer);
#else
    ASSERT(is_objc<AVSampleBufferDisplayLayer>(renderer));
#endif

#if RELEASE_LOG_DISABLED
    UNUSED_PARAM(logger);
    UNUSED_PARAM(logIdentifier);
#endif
}

VideoMediaSampleRenderer::~VideoMediaSampleRenderer()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (RetainPtr renderer = this->renderer())
        m_listener->stopObservingVideoRenderer(renderer.get());
    shutdown();
}

#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
AVSampleBufferVideoRenderer *VideoMediaSampleRenderer::videoRendererFor(WebSampleBufferVideoRendering *renderer)
{
    ASSERT(renderer);
    if (auto *displayLayer = dynamic_objc_cast<AVSampleBufferDisplayLayer>(renderer))
        return [displayLayer sampleBufferRenderer];
    return checked_objc_cast<AVSampleBufferVideoRenderer>(renderer);
}

Ref<GenericPromise> VideoMediaSampleRenderer::changeRenderer(WebSampleBufferVideoRendering *renderer)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    assertIsMainThread();

    RetainPtr previousRenderer = this->renderer();

    RetainPtr videoRenderer = videoRendererFor(renderer);
    if (std::exchange(m_mainRenderer, videoRenderer) == videoRenderer)
        return GenericPromise::createAndResolve();

    if (previousRenderer)
        m_listener->stopObservingVideoRenderer(previousRenderer.get());
    if (renderer)
        m_listener->beginObservingVideoRenderer(renderer);

    m_displayLayer = dynamic_objc_cast<AVSampleBufferDisplayLayer>(renderer);

    if (!isUsingDecompressionSession() && previousRenderer) {
        [previousRenderer flush];
        [previousRenderer stopRequestingMediaData];
    }

    return invokeAsync(dispatcher(), [weakThis = ThreadSafeWeakPtr { *this }, renderer = WTFMove(videoRenderer)] {
        if (RefPtr protectedThis = weakThis.get()) {
            assertIsCurrent(protectedThis->dispatcher().get());
            if (RetainPtr previousRenderer = std::exchange(protectedThis->m_renderer, renderer)) {
                [previousRenderer flush];
                [previousRenderer stopRequestingMediaData];
            }
            if (!protectedThis->isUsingDecompressionSession())
                return GenericPromise::createAndResolve();

            protectedThis->purgeDecodedSampleQueue(protectedThis->m_flushId);
            for (Ref sample : protectedThis->m_decodedSampleQueue)
                [renderer enqueueSampleBuffer:sample->platformSample().cmSampleBuffer()];
        }
        return GenericPromise::createAndResolve();
    });
}
#endif

RefPtr<WebCoreDecompressionSession> VideoMediaSampleRenderer::decompressionSession() const
{
    Locker lock { m_lock };
    return m_decompressionSession;
}

bool VideoMediaSampleRenderer::useDecompressionSessionForProtectedFallback() const
{
    return useDecompressionSessionForProtectedContent() || m_preferences.contains(VideoRendererPreference::ProtectedFallbackDisabled);
}

bool VideoMediaSampleRenderer::useDecompressionSessionForProtectedContent() const
{
    return m_preferences.contains(VideoRendererPreference::UseDecompressionSessionForProtectedContent);
}

bool VideoMediaSampleRenderer::useStereoDecoding() const
{
    return m_preferences.contains(VideoRendererPreference::UseStereoDecoding);
}

size_t VideoMediaSampleRenderer::decodedSamplesCount() const
{
    assertIsCurrent(dispatcher().get());

    return m_decodedSampleQueue.size();
}

RefPtr<const MediaSample> VideoMediaSampleRenderer::nextDecodedSample() const
{
    assertIsCurrent(dispatcher().get());

    if (!decodedSamplesCount())
        return nullptr;
    return m_decodedSampleQueue.first();
}

MediaTime VideoMediaSampleRenderer::nextDecodedSampleEndTime() const
{
    assertIsCurrent(dispatcher().get());

    if (m_decodedSampleQueue.size() <= 1)
        return MediaTime::positiveInfiniteTime();
    Ref secondSample = *std::next(m_decodedSampleQueue.begin());
    return secondSample->presentationTime();
}

MediaTime VideoMediaSampleRenderer::lastDecodedSampleTime() const
{
    assertIsCurrent(dispatcher().get());

    if (m_decodedSampleQueue.isEmpty())
        return MediaTime::invalidTime();
    Ref lastSample = m_decodedSampleQueue.last();
    return lastSample->presentationTime();
}

void VideoMediaSampleRenderer::enqueueDecodedSample(Ref<const MediaSample>&& sample)
{
    assertIsCurrent(dispatcher().get());

    m_decodedSampleQueue.append(WTFMove(sample));
}

bool VideoMediaSampleRenderer::isReadyForMoreMediaData() const
{
    assertIsMainThread();

    RetainPtr renderer = this->renderer();
    return areSamplesQueuesReadyForMoreMediaData(SampleQueueHighWaterMark) && (!renderer || [renderer isReadyForMoreMediaData]);
}

bool VideoMediaSampleRenderer::areSamplesQueuesReadyForMoreMediaData(size_t waterMark) const
{
    return compressedSamplesCount() <= waterMark;
}

size_t VideoMediaSampleRenderer::compressedSamplesCount() const
{
    return m_compressedSamplesCount + m_pendingSamplesCount;
}

void VideoMediaSampleRenderer::maybeBecomeReadyForMoreMediaData()
{
    assertIsCurrent(dispatcher().get());

    RetainPtr renderer = rendererOrDisplayLayer();
    if (renderer && ![renderer isReadyForMoreMediaData]) {
        if (m_waitingForMoreMediaData)
            return;
        m_waitingForMoreMediaData = true;
        ThreadSafeWeakPtr weakThis { *this };
        [renderer requestMediaDataWhenReadyOnQueue:dispatchQueue() usingBlock:^{
            if (RefPtr protectedThis = weakThis.get()) {
                assertIsCurrent(protectedThis->dispatcher().get());
                protectedThis->m_waitingForMoreMediaData = false;
                [protectedThis->rendererOrDisplayLayer() stopRequestingMediaData];
                protectedThis->maybeBecomeReadyForMoreMediaData();
            }
        }];
        return;
    }

    if (!areSamplesQueuesReadyForMoreMediaData(SampleQueueLowWaterMark))
        return;

    if (m_waitingForMoreMediaDataPending.exchange(true))
        return;

    callOnMainThread([weakThis = ThreadSafeWeakPtr { *this }] {
        assertIsMainThread();
        if (RefPtr protectedThis = weakThis.get()) {
            protectedThis->m_waitingForMoreMediaDataPending = false;
            if (protectedThis->m_readyForMoreMediaDataFunction)
                protectedThis->m_readyForMoreMediaDataFunction();
        }
    });
}

void VideoMediaSampleRenderer::stopRequestingMediaData()
{
    assertIsMainThread();

    m_readyForMoreMediaDataFunction = nullptr;

    if (isUsingDecompressionSession()) {
        // stopRequestingMediaData may deadlock if used on the main thread while enqueuing on the workqueue
        dispatcher()->dispatch([weakThis = ThreadSafeWeakPtr { *this }] {
            if (RefPtr protectedThis = weakThis.get()) {
                assertIsCurrent(protectedThis->dispatcher().get());
                protectedThis->m_waitingForMoreMediaData = false;
                [protectedThis->rendererOrDisplayLayer() stopRequestingMediaData];
            }
        });
        return;
    }
    [renderer() stopRequestingMediaData];
}

bool VideoMediaSampleRenderer::prefersDecompressionSession() const
{
    assertIsMainThread();

    return m_preferences.contains(VideoRendererPreference::PrefersDecompressionSession);
}

void VideoMediaSampleRenderer::setPreferences(Preferences preferences)
{
    assertIsMainThread();

    if (m_preferences == preferences || isUsingDecompressionSession())
        return;

    m_preferences = preferences;
}

void VideoMediaSampleRenderer::setTimebase(RetainPtr<CMTimebaseRef>&& timebase)
{
    if (!timebase)
        return;

    Locker locker { m_lock };

    ASSERT(!m_timebaseAndTimerSource.first);

    auto timerSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatchQueue()));
    dispatch_source_set_event_handler(timerSource.get(), [weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->purgeDecodedSampleQueue(protectedThis->m_flushId);
    });
    dispatch_activate(timerSource.get());
    PAL::CMTimebaseAddTimerDispatchSource(timebase.get(), timerSource.get());
    m_effectiveRateChangedListener = EffectiveRateChangedListener::create([weakThis = ThreadSafeWeakPtr { *this }, dispatcher = dispatcher()](double rate) {
        dispatcher->dispatch([weakThis, rate] {
            if (RefPtr protectedThis = weakThis.get(); protectedThis && rate)
                    protectedThis->purgeDecodedSampleQueue(protectedThis->m_flushId);
        });
    }, timebase.get());
    m_timebaseAndTimerSource = { WTFMove(timebase), WTFMove(timerSource) };
}

void VideoMediaSampleRenderer::clearTimebase()
{
    Locker locker { m_lock };

    auto [timebase, timerSource] = std::exchange(m_timebaseAndTimerSource, { });
    if (!timebase)
        return;

    PAL::CMTimebaseRemoveTimerDispatchSource(timebase.get(), timerSource.get());
    dispatch_source_cancel(timerSource.get());
    m_effectiveRateChangedListener = nullptr;
}

VideoMediaSampleRenderer::TimebaseAndTimerSource VideoMediaSampleRenderer::timebaseAndTimerSource() const
{
    Locker locker { m_lock };

    return m_timebaseAndTimerSource;
}

RetainPtr<CMTimebaseRef> VideoMediaSampleRenderer::timebase() const
{
    Locker locker { m_lock };

    return m_timebaseAndTimerSource.first;
}

MediaTime VideoMediaSampleRenderer::currentTime() const
{
    if (RetainPtr timebase = this->timebase())
        return PAL::toMediaTime(PAL::CMTimebaseGetTime(timebase.get()));
    return MediaTime::invalidTime();
}

void VideoMediaSampleRenderer::enqueueSample(const MediaSample& sample, const MediaTime& minimumUpcomingTime)
{
    assertIsMainThread();
    DEBUG_LOG(LOGIDENTIFIER, "sample: ", sample, " minimumUpcomingTime: ", minimumUpcomingTime);

    ASSERT(sample.type() == MediaSample::Type::CMSampleBuffer);
    if (sample.type() != MediaSample::Type::CMSampleBuffer)
        return;

    ASSERT(!m_needsFlushing);
    RetainPtr cmSampleBuffer = sample.platformSample().cmSampleBuffer();

    bool needsDecompressionSession = false;
#if ENABLE(VP9)
    if (!isUsingDecompressionSession() && !m_currentCodec) {
        // Only use a decompression session for vp8 or vp9 when software decoded.
        CMVideoFormatDescriptionRef videoFormatDescription = PAL::CMSampleBufferGetFormatDescription(cmSampleBuffer.get());
        auto fourCC = PAL::CMFormatDescriptionGetMediaSubType(videoFormatDescription);
        needsDecompressionSession = fourCC == 'vp08' || (fourCC == 'vp09' && !vp9HardwareDecoderAvailableInProcess());
        m_currentCodec = fourCC;
    }
#endif

    RetainPtr renderer = this->renderer();
    if (!m_decompressionSessionBlocked && !decompressionSession() && (!renderer || ((prefersDecompressionSession() || needsDecompressionSession || isUsingDecompressionSession()) && (!sample.isProtected() || useDecompressionSessionForProtectedContent()))))
        initializeDecompressionSession();

    if (!isUsingDecompressionSession()) {
        [renderer enqueueSampleBuffer:cmSampleBuffer.get()];
        return;
    }

    if (!useDecompressionSessionForProtectedFallback() && !m_decompressionSessionBlocked && sample.isProtected()) {
        m_decompressionSessionBlocked = true;
#if !PLATFORM(WATCHOS)
        auto numberOfDroppedVideoFrames = [renderer videoPerformanceMetrics].numberOfDroppedVideoFrames;
        if (m_droppedVideoFrames >= numberOfDroppedVideoFrames)
            m_droppedVideoFramesOffset = m_droppedVideoFrames - numberOfDroppedVideoFrames;
#endif
    }
    ++m_pendingSamplesCount;
    dispatcher()->dispatch([weakThis = ThreadSafeWeakPtr { *this }, sample = Ref { sample }, minimumUpcomingTime, flushId = m_flushId.load(), decompressionSessionBlocked = m_decompressionSessionBlocked]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        assertIsCurrent(protectedThis->dispatcher().get());
        --protectedThis->m_pendingSamplesCount;
        if (flushId != protectedThis->m_flushId) {
            protectedThis->m_compressedSamplesCount = 0;
            protectedThis->m_compressedSampleQueue.clear();
            protectedThis->maybeBecomeReadyForMoreMediaData();
            return;
        }
        protectedThis->m_compressedSampleQueue.append({ WTFMove(sample), minimumUpcomingTime, flushId, decompressionSessionBlocked });
        protectedThis->decodeNextSampleIfNeeded();
        protectedThis->m_compressedSamplesCount = protectedThis->m_compressedSampleQueue.size();
    });
}

void VideoMediaSampleRenderer::decodeNextSampleIfNeeded()
{
    assertIsCurrent(dispatcher().get());

    RefPtr decompressionSession = this->decompressionSession();

    if (m_gotDecodingError || !decompressionSession)
        return;

    RetainPtr timebase = this->timebase();
    MediaTime currentTime = timebase ? PAL::toMediaTime(PAL::CMTimebaseGetTime(timebase.get())) : MediaTime::invalidTime();
    double playbackRate = timebase ? PAL::CMTimebaseGetRate(timebase.get()) : 0;
    RefPtr nextDecodedSample = this->nextDecodedSample();
    auto nextDecodedSampleTime = nextDecodedSample ? nextDecodedSample->presentationTime() : MediaTime::zeroTime();
    auto lowWaterMarkTime = currentTime.isValid() ? std::max(currentTime, nextDecodedSampleTime) + DecodeLowWaterMark : MediaTime::invalidTime();
    auto highWaterMarkTime = currentTime.isValid() ? std::max(currentTime, nextDecodedSampleTime) + DecodeHighWaterMark : MediaTime::invalidTime();

    DEBUG_LOG(LOGIDENTIFIER, "currentTime: ", currentTime, " rate: ", playbackRate, " nextDecodedSampleTime: ", nextDecodedSampleTime, " lowWaterMarkTime: ", lowWaterMarkTime, " highWaterMarkTime: ", highWaterMarkTime, " compressedSampleQueue: ", m_compressedSampleQueue.size(), " m_isDecodingSample:", m_isDecodingSample);

    while (!m_compressedSampleQueue.isEmpty() && !m_isDecodingSample) {
        WebCoreDecompressionSession::DecodingFlags decodingFlags;

        if (currentTime.isValid() && !m_wasProtected && !m_decompressionSessionWasBlocked) {
            auto endTime = lastDecodedSampleTime();
            if (endTime.isValid() && endTime > highWaterMarkTime) {
                auto [sample, upcomingMinimum, flushId, blocked] = m_compressedSampleQueue.first();
                upcomingMinimum = std::min(sample->presentationTime(), upcomingMinimum.isValid() ? upcomingMinimum : MediaTime::positiveInfiniteTime());

                if (endTime < upcomingMinimum) {
                    if (m_lastMinimumUpcomingPresentationTime.isInvalid() || upcomingMinimum != m_lastMinimumUpcomingPresentationTime) {
                        ASSERT(m_lastMinimumUpcomingPresentationTime.isInvalid() || m_lastMinimumUpcomingPresentationTime < upcomingMinimum);
                        m_lastMinimumUpcomingPresentationTime = upcomingMinimum;
                        LogPerformance("VideoMediaSampleRenderer::decodeNextSampleIfNeeded currentTime:%0.2f expectMinimumUpcomingSampleBufferPresentationTime:%0.2f decoded queued:%zu upcoming:%zu high watermark reached", currentTime.toDouble(), m_lastMinimumUpcomingPresentationTime.toDouble(), decodedSamplesCount(), compressedSamplesCount());
                        [rendererOrDisplayLayer() expectMinimumUpcomingSampleBufferPresentationTime:PAL::toCMTime(m_lastMinimumUpcomingPresentationTime)];
                    }
                    return;
                }
                DEBUG_LOG(LOGIDENTIFIER, "Out of order frames detected, forcing extra decode");
            }
            if (endTime.isValid() && endTime >= lowWaterMarkTime && playbackRate > 0.9 && playbackRate < 1.1) {
                LogPerformance("VideoMediaSampleRenderer::decodeNextSampleIfNeeded expectMinimumUpcomingSampleBufferPresentationTime:%0.2f decoded queued:%zu upcoming:%zu currentTime:%0.2f endTime:%0.2f low:%0.2f high:%0.2f low watermark reached", m_lastMinimumUpcomingPresentationTime.toDouble(), decodedSamplesCount(), compressedSamplesCount(), currentTime.toDouble(), endTime.toDouble(), lowWaterMarkTime.toDouble(), highWaterMarkTime.toDouble());
                decodingFlags.add(WebCoreDecompressionSession::DecodingFlag::RealTime);
            }
        }

        auto [sample, upcomingMinimum, flushId, blocked] = m_compressedSampleQueue.takeFirst();
        m_compressedSamplesCount = m_compressedSampleQueue.size();
        maybeBecomeReadyForMoreMediaData();

        if (flushId != m_flushId) {
            DEBUG_LOG(LOGIDENTIFIER, "Flush detected");
            continue;
        }

        if (!shouldDecodeSample(sample)) {
            DEBUG_LOG(LOGIDENTIFIER, "shouldDecodeSample: false");
            ++m_totalVideoFrames;
            ++m_droppedVideoFrames;
            continue;
        }

        ASSERT(m_lastMinimumUpcomingPresentationTime.isInvalid() || sample->isNonDisplaying() || sample->presentationTime() >= std::min(sample->presentationTime(), m_lastMinimumUpcomingPresentationTime));

        if (!useDecompressionSessionForProtectedFallback() && m_wasProtected != sample->isProtected()) {
            ASSERT(sample->isSync());
            ALWAYS_LOG(LOGIDENTIFIER, "Changing protection type (was: ", m_wasProtected, ") content at:", sample->presentationTime());
            m_wasProtected = sample->isProtected();
        }

        m_decompressionSessionWasBlocked = blocked;
        if (blocked) {
            decodedFrameAvailable(WTFMove(sample), flushId);
            continue;
        }

        if (sample->isNonDisplaying())
            decodingFlags.add(WebCoreDecompressionSession::DecodingFlag::NonDisplaying);
        if (useStereoDecoding())
            decodingFlags.add(WebCoreDecompressionSession::DecodingFlag::EnableStereo);

        RetainPtr cmSample = sample->platformSample().cmSampleBuffer();
        auto decodePromise = decompressionSession->decodeSample(cmSample.get(), decodingFlags);

        m_isDecodingSample = true;

        decodePromise->whenSettled(dispatcher(), [weakThis = ThreadSafeWeakPtr { *this }, this, decodingFlags, flushId = flushId, startTime = MonotonicTime::now(), numberOfSamples = PAL::CMSampleBufferGetNumSamples(cmSample.get())](auto&& result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            assertIsCurrent(dispatcher().get());

            m_isDecodingSample = false;

            if (flushId != m_flushId || (!result && result.error() == noErr)) {
                ALWAYS_LOG(LOGIDENTIFIER, "Decoder was flushed");
                decodeNextSampleIfNeeded();
                return;
            }

            m_totalVideoFrames += numberOfSamples;

            if (!result) {
                if (result.error() == kVTInvalidSessionErr) {
                    ALWAYS_LOG(LOGIDENTIFIER, "VTDecompressionSession got invalidated, requesting flush");
                    invalidateDecompressionSession();
                    return;
                }

                m_gotDecodingError = true;
                ++m_corruptedVideoFrames;

                callOnMainThread([protectedThis, status = result.error()] {
                    assertIsMainThread();
                    RetainPtr error = [NSError errorWithDomain:@"com.apple.WebKit" code:status userInfo:nil];
                    protectedThis->notifyErrorHasOccurred(error.get());
                });
                return;
            }

            if (LOG_CHANNEL(MediaPerformance).level >= WTFLogLevel::Debug) {
                auto now = MonotonicTime::now();
                m_frameRateMonitor.update();
                OSType format = '----';
                MediaTime presentationTime = MediaTime::invalidTime();
                if (RetainPtr firstFrame = result->isEmpty() ? nullptr : (*result)[0]) {
                    RetainPtr imageBuffer = imageForSample(static_cast<CMSampleBufferRef>(firstFrame.get()));
                    format = CVPixelBufferGetPixelFormatType(imageBuffer.get());
                    presentationTime = PAL::toMediaTime(PAL::CMSampleBufferGetOutputPresentationTimeStamp(firstFrame.get()));
                }
                LogPerformance("VideoMediaSampleRenderer pts:%0.2f minimum upcoming:%0.2f decoding rate:%0.1fHz rolling:%0.1f decoder rate:%0.1fHz compressed queue:%zu decoded queue:%zu hw:%d format:%s", presentationTime.toDouble(), m_lastMinimumUpcomingPresentationTime.toDouble(), 1.0f / Seconds { now - std::exchange(m_timeSinceLastDecode, now) }.value(), m_frameRateMonitor.observedFrameRate(), 1.0f / Seconds { now - startTime }.value(), compressedSamplesCount(), decodedSamplesCount(), protectedThis->decompressionSession()->isHardwareAccelerated(), &FourCC(format).string()[0]);
            }

            if (!decodingFlags.contains(WebCoreDecompressionSession::DecodingFlag::NonDisplaying)) {
                for (auto& decodedFrame : *result) {
                    if (decodedFrame)
                        decodedFrameAvailable(MediaSampleAVFObjC::create(decodedFrame.get(), 0), flushId);
                }
            }

            decodeNextSampleIfNeeded();
        });
    }
}

bool VideoMediaSampleRenderer::shouldDecodeSample(const MediaSample& sample)
{
    if (sample.isNonDisplaying())
        return true;

    auto currentTime = this->currentTime();
    if (currentTime.isInvalid())
        return true;

    if (sample.presentationEndTime() >= currentTime)
        return true;

    RetainPtr attachments = PAL::CMSampleBufferGetSampleAttachmentsArray(sample.platformSample().cmSampleBuffer(), false);
    if (!attachments)
        return true;

    for (CFIndex index = 0, count = CFArrayGetCount(attachments.get()); index < count; ++index) {
        CFDictionaryRef attachmentDict = (CFDictionaryRef)CFArrayGetValueAtIndex(attachments.get(), index);
        if (CFDictionaryGetValue(attachmentDict, PAL::kCMSampleAttachmentKey_IsDependedOnByOthers) == kCFBooleanFalse)
            return false;
    }

    return true;
}

void VideoMediaSampleRenderer::initializeDecompressionSession()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    assertIsMainThread();

    {
        Locker locker { m_lock };

        ASSERT(!m_decompressionSession);
        if (m_decompressionSession)
            return;

#if HAVE(RECOMMENDED_PIXEL_ATTRIBUTES_API)
        if (m_mainRenderer) {
            if ([m_mainRenderer respondsToSelector:@selector(recommendedPixelBufferAttributes)])
                m_decompressionSession = WebCoreDecompressionSession::create([m_mainRenderer recommendedPixelBufferAttributes], m_rendererIsThreadSafe ? dispatcher().ptr() : nullptr);
        }
#endif
        if (!m_decompressionSession)
            m_decompressionSession = WebCoreDecompressionSession::createOpenGL(m_rendererIsThreadSafe ? dispatcher().ptr() : nullptr);
        m_isUsingDecompressionSession = true;
    }
    if (!m_startupTime)
        m_startupTime = MonotonicTime::now();

    resetReadyForMoreMediaData();
}

void VideoMediaSampleRenderer::decodedFrameAvailable(Ref<const MediaSample>&& sample, FlushId flushId)
{
    assertIsCurrent(dispatcher().get());

    assignResourceOwner(sample);

    [rendererOrDisplayLayer() enqueueSampleBuffer:sample->platformSample().cmSampleBuffer()];

    if (auto timebase = this->timebase()) {
        enqueueDecodedSample(WTFMove(sample));
        maybeReschedulePurge(flushId);
    } else
        maybeQueueFrameForDisplay(MediaTime::invalidTime(), sample, flushId);
}

VideoMediaSampleRenderer::DecodedFrameResult VideoMediaSampleRenderer::maybeQueueFrameForDisplay(const MediaTime& currentTime, const MediaSample& sample, FlushId flushId)
{
    assertIsCurrent(dispatcher().get());

    auto presentationTime = sample.presentationTime();

    if (currentTime.isValid()) {
        // Always display the first video frame available if we aren't displaying any yet, regardless of its time as it's always either:
        // 1- The first frame of the video.
        // 2- The first visible frame after a seek.

        if (m_lastDisplayedSample) {
            if (presentationTime < *m_lastDisplayedSample)
                return DecodedFrameResult::TooLate;
            if (presentationTime == *m_lastDisplayedSample)
                return DecodedFrameResult::AlreadyDisplayed;
        }
        if (!m_forceLateSampleToBeDisplayed && m_isDisplayingSample && m_lastDisplayedTime && presentationTime < *m_lastDisplayedTime)
            return DecodedFrameResult::TooLate;

        if (m_isDisplayingSample && presentationTime > currentTime)
            return DecodedFrameResult::TooEarly;

        m_lastDisplayedTime = currentTime;
        m_lastDisplayedSample = presentationTime;
    }

    ++m_presentedVideoFrames;
    m_isDisplayingSample = true;
    m_forceLateSampleToBeDisplayed = false;

    notifyHasAvailableVideoFrame(presentationTime, (MonotonicTime::now() - m_startupTime).seconds(), flushId);

    return DecodedFrameResult::Displayed;
}

void VideoMediaSampleRenderer::flushCompressedSampleQueue()
{
    assertIsMainThread();

    ++m_flushId;
    m_compressedSamplesCount = 0;
    m_gotDecodingError = false;
}

void VideoMediaSampleRenderer::flushDecodedSampleQueue()
{
    assertIsCurrent(dispatcher().get());

    m_decodedSampleQueue.clear();
    m_nextScheduledPurge.reset();
    m_lastDisplayedSample.reset();
    m_lastDisplayedTime.reset();
    m_isDisplayingSample = false;
    m_lastMinimumUpcomingPresentationTime = MediaTime::invalidTime();
}

void VideoMediaSampleRenderer::cancelTimer()
{
    schedulePurgeAtTime(MediaTime::invalidTime());
}

void VideoMediaSampleRenderer::purgeDecodedSampleQueue(FlushId flushId)
{
    assertIsCurrent(dispatcher().get());

    if (!decompressionSession())
        return;

    if (!decodedSamplesCount()) {
        decodeNextSampleIfNeeded();
        return;
    }

    bool samplesPurged = false;

    auto timebase = this->timebase();
    if (timebase) {
        auto currentTime = PAL::toMediaTime(PAL::CMTimebaseGetTime(timebase.get()));

        samplesPurged = purgeDecodedSampleQueueUntilTime(currentTime);
        if (RefPtr nextSample = nextDecodedSample()) {
            auto result = maybeQueueFrameForDisplay(currentTime, *nextSample, flushId);
#if !LOG_DISABLED
            if (LOG_CHANNEL(Media).level >= WTFLogLevel::Debug) {
                auto presentationTime = nextSample->presentationTime();
                auto presentationEndTime = nextDecodedSampleEndTime();
                auto resultLiteral = [](DecodedFrameResult result) {
                    switch (result) {
                    case DecodedFrameResult::TooEarly: return "tooEarly"_s;
                    case DecodedFrameResult::TooLate: return "tooLate"_s;
                    case DecodedFrameResult::AlreadyDisplayed: return "alreadyDisplayed"_s;
                    case DecodedFrameResult::Displayed: return "displayed"_s;
                    };
                }(result);
                DEBUG_LOG(LOGIDENTIFIER, "maybeQueueFrameForDisplay: currentTime:", currentTime, " start:", presentationTime, " end:", presentationEndTime, " result:", resultLiteral);
            }
#else
            UNUSED_VARIABLE(result);
#endif
        }
    }
    if (samplesPurged)
        maybeBecomeReadyForMoreMediaData();
    if (samplesPurged || decodedSamplesCount() == 1)
        decodeNextSampleIfNeeded();
}

bool VideoMediaSampleRenderer::purgeDecodedSampleQueueUntilTime(const MediaTime& currentTime)
{
    assertIsCurrent(dispatcher().get());

    if (m_decodedSampleQueue.isEmpty())
        return false;

    MediaTime nextPurgeTime = MediaTime::invalidTime();
    bool samplesPurged = false;

    while (RefPtr nextSample = nextDecodedSample()) {
        auto presentationTime = nextSample->presentationTime();
        auto presentationEndTime = nextDecodedSampleEndTime();

        if (presentationEndTime >= currentTime) {
            nextPurgeTime = presentationEndTime;
            break;
        }

        if (m_lastDisplayedSample && *m_lastDisplayedSample < presentationTime) {
            ++m_droppedVideoFrames; // This frame was never displayed.
            DEBUG_LOG(LOGIDENTIFIER, "purgeDecodedSampleQueueUntilTime: currentTime:", currentTime, " start:", presentationTime, " end:", presentationEndTime, " result:tooLate scheduled:", m_nextScheduledPurge.value_or(MediaTime::positiveInfiniteTime()), " (dropped:", m_droppedVideoFrames.load(), ")");
        }

        m_decodedSampleQueue.removeFirst();
        samplesPurged = true;
    }

    if (nextPurgeTime.isInvalid())
        return samplesPurged;

    schedulePurgeAtTime(nextPurgeTime);

    return samplesPurged;
}

void VideoMediaSampleRenderer::schedulePurgeAtTime(const MediaTime& nextPurgeTime)
{
    auto [timebase, timerSource] = timebaseAndTimerSource();
    if (!timebase)
        return;

    PAL::CMTimebaseSetTimerDispatchSourceNextFireTime(timebase.get(), timerSource.get(), PAL::toCMTime(nextPurgeTime), 0);

    if (nextPurgeTime.isValid()) {
        assertIsCurrent(dispatcher().get());
        m_nextScheduledPurge = nextPurgeTime;
    }
}

void VideoMediaSampleRenderer::maybeReschedulePurge(FlushId flushId)
{
    assertIsCurrent(dispatcher().get());

    if (!decodedSamplesCount())
        return;
    auto presentationEndTime = nextDecodedSampleEndTime();
    if (m_nextScheduledPurge && *m_nextScheduledPurge <= presentationEndTime)
        return;

    if ((m_nextScheduledPurge && m_nextScheduledPurge->isPositiveInfinite()) || presentationEndTime.isPositiveInfinite()) {
        purgeDecodedSampleQueue(flushId);
        return;
    }
    schedulePurgeAtTime(presentationEndTime);
}

void VideoMediaSampleRenderer::flush()
{
    assertIsMainThread();
    [renderer() flush];

    if (!isUsingDecompressionSession()) {
        resetReadyForMoreMediaData();
        return;
    }

    m_needsFlushing = false;
    cancelTimer();
    flushCompressedSampleQueue();

    if (RefPtr decompressionSession = this->decompressionSession())
        decompressionSession->flush();
    dispatcher()->dispatch([weakThis = ThreadSafeWeakPtr { *this }]() mutable {
        if (RefPtr protectedThis = weakThis.get()) {
            assertIsCurrent(protectedThis->dispatcher().get());
            protectedThis->flushDecodedSampleQueue();
            protectedThis->m_notifiedFirstFrameAvailable = false;
            protectedThis->m_waitingForMoreMediaData = false;
            protectedThis->maybeBecomeReadyForMoreMediaData();
        }
    });
}

void VideoMediaSampleRenderer::shutdown()
{
    assertIsMainThread();

    clearTimebase();
    [renderer() flush];
    cancelTimer();
    flushCompressedSampleQueue();
    RefPtr<WebCoreDecompressionSession> decompressionSession = [&] {
        Locker lock { m_lock };
        return std::exchange(m_decompressionSession, nullptr);
    }();
    if (decompressionSession)
        decompressionSession->invalidate();

    if (RetainPtr renderer = this->renderer()) {
        [renderer flush];
        [renderer stopRequestingMediaData];
    }
}

void VideoMediaSampleRenderer::requestMediaDataWhenReady(Function<void()>&& function)
{
    assertIsMainThread();
    m_readyForMoreMediaDataFunction = WTFMove(function);
    resetReadyForMoreMediaData();
}

void VideoMediaSampleRenderer::resetReadyForMoreMediaData()
{
    assertIsMainThread();

    if (!renderer() || isUsingDecompressionSession()) {
        dispatcher()->dispatch([weakThis = ThreadSafeWeakPtr { *this }] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->maybeBecomeReadyForMoreMediaData();
        });
        return;
    }

    ThreadSafeWeakPtr weakThis { *this };
    [renderer() requestMediaDataWhenReadyOnQueue:mainDispatchQueueSingleton() usingBlock:^{
        assertIsMainThread();
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (![protectedThis->renderer() isReadyForMoreMediaData])
            return;
        if (protectedThis->m_readyForMoreMediaDataFunction)
            protectedThis->m_readyForMoreMediaDataFunction();
    }];
}

void VideoMediaSampleRenderer::expectMinimumUpcomingSampleBufferPresentationTime(const MediaTime& time)
{
    assertIsMainThread();
    if (isUsingDecompressionSession() || ![PAL::getAVSampleBufferDisplayLayerClassSingleton() instancesRespondToSelector:@selector(expectMinimumUpcomingSampleBufferPresentationTime:)])
        return;

    [renderer() expectMinimumUpcomingSampleBufferPresentationTime:PAL::toCMTime(time)];
}

WebSampleBufferVideoRendering *VideoMediaSampleRenderer::renderer() const
{
    assertIsMainThread();

    if (m_displayLayer)
        return m_displayLayer.get();
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    return m_mainRenderer.get();
#else
    return nil;
#endif
}

template <>
AVSampleBufferVideoRenderer *VideoMediaSampleRenderer::as() const
{
    assertIsMainThread();

#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    return m_mainRenderer.get();
#else
    return nil;
#endif
}

WebSampleBufferVideoRendering *VideoMediaSampleRenderer::rendererOrDisplayLayer() const
{
    assertIsCurrent(dispatcher().get());
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    return m_renderer.get();
#else
    if (m_displayLayer) {
        assertIsMainThread(); // if we access this object, dispatcher is identical to main thread.
        return m_displayLayer.get();
    }
    return nil;
#endif
}

RetainPtr<CVPixelBufferRef> VideoMediaSampleRenderer::imageForSample(CMSampleBufferRef sample) const
{
    RetainPtr videoFormatDescription = PAL::CMSampleBufferGetFormatDescription(sample);
    auto type = PAL::CMFormatDescriptionGetMediaType(videoFormatDescription.get());
    if (type == kCMMediaType_TaggedBufferGroup) {
        RetainPtr group = PAL::CMSampleBufferGetTaggedBufferGroup(sample);
        if (RetainPtr heroEye = dynamic_cf_cast<CFStringRef>(PAL::CMFormatDescriptionGetExtension(videoFormatDescription.get(), PAL::kCMFormatDescriptionExtension_HeroEye))) {
            for (CFIndex index = 0; index < PAL::CMTaggedBufferGroupGetCount(group.get()); ++index) {
                RetainPtr tagCollection = PAL::CMTaggedBufferGroupGetTagCollectionAtIndex(group.get(), index);
                if (PAL::CMTagCollectionContainsTag(tagCollection.get(), heroEye.get() == PAL::kCMFormatDescriptionHeroEye_Left ? PAL::kCMTagStereoLeftEye : PAL::kCMTagStereoRightEye))
                    return PAL::CMTaggedBufferGroupGetCVPixelBufferAtIndex(group.get(), index);
            }
        }

        // Hero eye not defined or not found, use the one with LayerID=0
        for (CFIndex index = 0; index < PAL::CMTaggedBufferGroupGetCount(group.get()); ++index) {
            RetainPtr tagCollection = PAL::CMTaggedBufferGroupGetTagCollectionAtIndex(group.get(), index);
            CMTag videoLayerIDTag = PAL::kCMTagInvalid;
            CMItemCount numberOfTagsCopied = 0;
            if (!PAL::CMTagCollectionGetTagsWithCategory(tagCollection.get(), kCMTagCategory_VideoLayerID, &videoLayerIDTag, 1, &numberOfTagsCopied) && numberOfTagsCopied == 1 && !PAL::CMTagGetSInt64Value(videoLayerIDTag))
                return PAL::CMTaggedBufferGroupGetCVPixelBufferAtIndex(group.get(), index);
        }

        // None found.
        return nullptr;
    }

    return PAL::CMSampleBufferGetImageBuffer(sample);
}

auto VideoMediaSampleRenderer::copyDisplayedPixelBuffer() -> DisplayedPixelBufferEntry
{
    assertIsMainThread();

    if (!isUsingDecompressionSession()) {
        RetainPtr buffer = adoptCF([renderer() copyDisplayedPixelBuffer]);
        if (auto surface = CVPixelBufferGetIOSurface(buffer.get()); surface && m_resourceOwner)
            IOSurface::setOwnershipIdentity(surface, m_resourceOwner);
        return { WTFMove(buffer), MediaTime::invalidTime() };
    }

    RetainPtr<CVPixelBufferRef> imageBuffer;
    MediaTime presentationTimeStamp;

    ensureOnDispatcherSync([&] {
        assertIsCurrent(dispatcher().get());

        RetainPtr timebase = this->timebase();
        if (!timebase)
            return;

        m_forceLateSampleToBeDisplayed = true;
        purgeDecodedSampleQueue(m_flushId);

        RefPtr nextSample = nextDecodedSample();
        if (!nextSample)
            return;
        auto currentTime = PAL::toMediaTime(PAL::CMTimebaseGetTime(timebase.get()));
        auto presentationTime = nextSample->presentationTime();

        if (presentationTime > currentTime && (!m_lastDisplayedSample || presentationTime > *m_lastDisplayedSample))
            return;

        imageBuffer = imageForSample(nextSample->platformSample().cmSampleBuffer());
        presentationTimeStamp = presentationTime;
    });

    if (!imageBuffer)
        return { nullptr, MediaTime::invalidTime() };

    ASSERT(CFGetTypeID(imageBuffer.get()) == CVPixelBufferGetTypeID());
    if (CFGetTypeID(imageBuffer.get()) != CVPixelBufferGetTypeID())
        return { nullptr, MediaTime::invalidTime() };
    return { WTFMove(imageBuffer), presentationTimeStamp };
}

unsigned VideoMediaSampleRenderer::totalDisplayedFrames() const
{
    assertIsMainThread();

    return isUsingDecompressionSession() ? m_presentedVideoFrames.load() : ++m_sampleCount;
}

unsigned VideoMediaSampleRenderer::totalVideoFrames() const
{
    assertIsMainThread();

    if (isUsingDecompressionSession() && !m_decompressionSessionBlocked)
        return m_totalVideoFrames;
#if PLATFORM(WATCHOS)
    return 0;
#else
    return [renderer() videoPerformanceMetrics].totalNumberOfVideoFrames;
#endif
}

unsigned VideoMediaSampleRenderer::droppedVideoFrames() const
{
    assertIsMainThread();

    if (isUsingDecompressionSession() && !m_decompressionSessionBlocked)
        return m_droppedVideoFrames;
#if PLATFORM(WATCHOS)
    return 0;
#else
    return [renderer() videoPerformanceMetrics].numberOfDroppedVideoFrames + m_droppedVideoFramesOffset;
#endif
}

unsigned VideoMediaSampleRenderer::corruptedVideoFrames() const
{
    assertIsMainThread();

    if (isUsingDecompressionSession() && !m_decompressionSessionBlocked)
        return m_corruptedVideoFrames;
#if PLATFORM(WATCHOS)
    return 0;
#else
    return [renderer() videoPerformanceMetrics].numberOfCorruptedVideoFrames + m_corruptedVideoFrames;
#endif
}

MediaTime VideoMediaSampleRenderer::totalFrameDelay() const
{
    assertIsMainThread();

    if (isUsingDecompressionSession() && !m_decompressionSessionBlocked)
        return m_totalFrameDelay;
#if PLATFORM(WATCHOS)
    return MediaTime::invalidTime();
#else
    return MediaTime::createWithDouble([renderer() videoPerformanceMetrics].totalFrameDelay);
#endif
}

void VideoMediaSampleRenderer::setResourceOwner(const ProcessIdentity& resourceOwner)
{
    m_resourceOwner = resourceOwner;
}

void VideoMediaSampleRenderer::invalidateDecompressionSession()
{
    RefPtr decompressionSession = [&] {
        Locker lock { m_lock };
        return std::exchange(m_decompressionSession, nullptr);
    }();

    if (decompressionSession)
        decompressionSession->invalidate();

    ensureOnMainThread([weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->notifyVideoRendererRequiresFlushToResumeDecoding();
    });
}

void VideoMediaSampleRenderer::assignResourceOwner(const MediaSample& sample)
{
    if (!m_resourceOwner)
        return;

    auto assignImageBuffer = [&](CVPixelBufferRef imageBuffer) {
        if (!imageBuffer || CFGetTypeID(imageBuffer) != CVPixelBufferGetTypeID())
            return;

        if (auto surface = CVPixelBufferGetIOSurface(imageBuffer))
            IOSurface::setOwnershipIdentity(surface, m_resourceOwner);
    };

    RetainPtr cmSample = sample.platformSample().cmSampleBuffer();
    RetainPtr videoFormatDescription = PAL::CMSampleBufferGetFormatDescription(cmSample.get());
    if (PAL::CMFormatDescriptionGetMediaType(videoFormatDescription.get()) == kCMMediaType_TaggedBufferGroup) {
        RetainPtr group = PAL::CMSampleBufferGetTaggedBufferGroup(cmSample.get());

        for (CFIndex index = 0; index < PAL::CMTaggedBufferGroupGetCount(group.get()); ++index)
            assignImageBuffer(PAL::CMTaggedBufferGroupGetCVPixelBufferAtIndex(group.get(), index));
        return;
    }
    assignImageBuffer(PAL::CMSampleBufferGetImageBuffer(cmSample.get()));
}

void VideoMediaSampleRenderer::notifyFirstFrameAvailable(Function<void(const MediaTime&, double)>&& callback)
{
    assertIsMainThread();

    m_hasFirstFrameAvailableCallback = WTFMove(callback);
}

void VideoMediaSampleRenderer::notifyWhenHasAvailableVideoFrame(Function<void(const MediaTime&, double)>&& callback)
{
    assertIsMainThread();

    m_hasAvailableFrameCallback = WTFMove(callback);
    m_notifyWhenHasAvailableVideoFrame = !!m_hasAvailableFrameCallback;
}

void VideoMediaSampleRenderer::notifyHasAvailableVideoFrame(const MediaTime& presentationTime, double displayTime, FlushId flushId)
{
    assertIsCurrent(dispatcher().get());

    if (!m_notifiedFirstFrameAvailable) {
        DEBUG_LOG(LOGIDENTIFIER, "First Frame Available");
        callOnMainThread([weakThis = ThreadSafeWeakPtr { *this }, flushId, presentationTime, displayTime] {
            if (RefPtr protectedThis = weakThis.get(); protectedThis && flushId == protectedThis->m_flushId) {
                assertIsMainThread();
                if (protectedThis->m_hasFirstFrameAvailableCallback)
                    protectedThis->m_hasFirstFrameAvailableCallback(presentationTime, displayTime);
            }
        });
        m_notifiedFirstFrameAvailable = true;
    }
    if (!m_notifyWhenHasAvailableVideoFrame)
        return;
    DEBUG_LOG(LOGIDENTIFIER, "Has Available Video Frame");
    callOnMainThread([weakThis = ThreadSafeWeakPtr { *this }, flushId, presentationTime, displayTime] {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && flushId == protectedThis->m_flushId) {
            assertIsMainThread();
            if (protectedThis->m_hasAvailableFrameCallback)
                protectedThis->m_hasAvailableFrameCallback(presentationTime, displayTime);
        }
    });
}

void VideoMediaSampleRenderer::notifyWhenDecodingErrorOccurred(Function<void(NSError *)>&& callback)
{
    assertIsMainThread();
    m_errorOccurredFunction = WTFMove(callback);
}

void VideoMediaSampleRenderer::notifyWhenVideoRendererRequiresFlushToResumeDecoding(Function<void()>&& callback)
{
    assertIsMainThread();
    m_rendererNeedsFlushFunction = WTFMove(callback);
}

void VideoMediaSampleRenderer::notifyErrorHasOccurred(NSError *error)
{
    ALWAYS_LOG(LOGIDENTIFIER, "error: ", [error description]);
    assertIsMainThread();

    if (m_errorOccurredFunction)
        m_errorOccurredFunction(error);
}

void VideoMediaSampleRenderer::notifyVideoRendererRequiresFlushToResumeDecoding()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    assertIsMainThread();

    m_needsFlushing = true;
    if (m_rendererNeedsFlushFunction)
        m_rendererNeedsFlushFunction();
}

Ref<GuaranteedSerialFunctionDispatcher> VideoMediaSampleRenderer::dispatcher() const
{
    return m_rendererIsThreadSafe ? queueSingleton() : static_cast<GuaranteedSerialFunctionDispatcher&>(MainThreadDispatcher::singleton());
}

dispatch_queue_t VideoMediaSampleRenderer::dispatchQueue() const
{
    return m_rendererIsThreadSafe ? queueSingleton().dispatchQueue() : WorkQueue::mainSingleton().dispatchQueue();
}

void VideoMediaSampleRenderer::ensureOnDispatcher(Function<void()>&& function) const
{
    if (dispatcher()->isCurrent()) {
        function();
        return;
    }

    if (m_rendererIsThreadSafe)
        return queueSingleton().dispatch(WTFMove(function));
    callOnMainThread(WTFMove(function));
}

void VideoMediaSampleRenderer::ensureOnDispatcherSync(Function<void()>&& function) const
{
    if (dispatcher()->isCurrent()) {
        function();
        return;
    }

    if (m_rendererIsThreadSafe)
        return queueSingleton().dispatchSync(WTFMove(function));
    callOnMainThreadAndWait(WTFMove(function));
}

void VideoMediaSampleRenderer::videoRendererDidReceiveError(WebSampleBufferVideoRendering *renderer, NSError *error)
{
    assertIsMainThread();
    if (renderer != this->renderer())
        return;
#if PLATFORM(IOS_FAMILY)
    if ((renderer.status == AVQueuedSampleBufferRenderingStatusFailed || renderer.status == AVQueuedSampleBufferRenderingStatusUnknown) && [error.domain isEqualToString:@"AVFoundationErrorDomain"] && error.code == AVErrorOperationInterrupted) {
        notifyVideoRendererRequiresFlushToResumeDecoding();
        return;
    }
    ALWAYS_LOG(LOGIDENTIFIER, "status: ", renderer.status, " domain: ", [error.domain UTF8String], " errorCode: ", error.code);
#endif
    notifyErrorHasOccurred(error);
}

void VideoMediaSampleRenderer::videoRendererRequiresFlushToResumeDecodingChanged(WebSampleBufferVideoRendering *renderer, bool requiresFlush)
{
    ALWAYS_LOG(LOGIDENTIFIER, "requiresFlush:", requiresFlush);
    assertIsMainThread();
    if (renderer != this->renderer() || !requiresFlush)
        return;
    notifyVideoRendererRequiresFlushToResumeDecoding();
}

void VideoMediaSampleRenderer::videoRendererReadyForDisplayChanged(WebSampleBufferVideoRendering *renderer, bool isReadyForDisplay)
{
    ALWAYS_LOG(LOGIDENTIFIER, "isReadyForDisplay:", isReadyForDisplay);
    assertIsMainThread();
    if (renderer != this->renderer() || !isReadyForDisplay)
        return;
    if (!isUsingDecompressionSession() && m_hasFirstFrameAvailableCallback)
        m_hasFirstFrameAvailableCallback(currentTime(), (MonotonicTime::now() - m_startupTime).seconds());
}

void VideoMediaSampleRenderer::outputObscuredDueToInsufficientExternalProtectionChanged(bool obscured)
{
    RetainPtr error = [NSError errorWithDomain:@"com.apple.WebKit" code:'HDCP' userInfo:@{
        @"obscured": obscured ? @YES : @NO
    }];
    notifyErrorHasOccurred(error.get());
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& VideoMediaSampleRenderer::logChannel() const
{
    return LogMedia;
}
#endif

#undef LogPerformance

} // namespace WebCore
