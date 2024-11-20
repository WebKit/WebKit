/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "MediaSampleAVFObjC.h"
#import "WebCoreDecompressionSession.h"
#import "WebSampleBufferVideoRendering.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CMFormatDescription.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/Locker.h>
#import <wtf/MainThreadDispatcher.h>

#pragma mark - Soft Linking

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

@interface AVSampleBufferDisplayLayer (WebCoreAVSampleBufferDisplayLayerQueueManagementPrivate)
- (void)expectMinimumUpcomingSampleBufferPresentationTime:(CMTime)minimumUpcomingPresentationTime;
- (void)resetUpcomingSampleBufferPresentationTimeExpectations;
@end

namespace WebCore {

static constexpr CMItemCount SampleQueueHighWaterMark = 30;
static constexpr CMItemCount SampleQueueLowWaterMark = 15;

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

VideoMediaSampleRenderer::VideoMediaSampleRenderer(WebSampleBufferVideoRendering *renderer)
    : m_workQueue(isRendererThreadSafe(renderer) ? RefPtr { WorkQueue::create("VideoMediaSampleRenderer Queue"_s) } : nullptr)
{
    if (!renderer)
        return;

    if (auto *displayLayer = dynamic_objc_cast<AVSampleBufferDisplayLayer>(renderer)) {
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
        m_renderer = [displayLayer sampleBufferRenderer];
#endif
        m_displayLayer = displayLayer;
        return;
    }
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    ASSERT(dynamic_objc_cast<AVSampleBufferVideoRenderer>(renderer));
    m_renderer = dynamic_objc_cast<AVSampleBufferVideoRenderer>(renderer);
#endif
}

VideoMediaSampleRenderer::~VideoMediaSampleRenderer()
{
    assertIsMainThread();

    flushCompressedSampleQueue();

    if (auto renderer = rendererOrDisplayLayer()) {
        [renderer flush];
        [renderer stopRequestingMediaData];
    }

    if (m_decompressionSession) {
        m_decompressionSession->invalidate();
        m_decompressionSession = nullptr;
    }

    clearTimebase();
}

size_t VideoMediaSampleRenderer::decodedSamplesCount() const
{
    return m_decodedSampleQueue ? PAL::CMBufferQueueGetBufferCount(m_decodedSampleQueue.get()) : 0;
}

bool VideoMediaSampleRenderer::isReadyForMoreMediaData() const
{
    assertIsMainThread();

    return areSamplesQueuesReadyForMoreMediaData(SampleQueueHighWaterMark) && (!renderer() || [renderer() isReadyForMoreMediaData]);
}

bool VideoMediaSampleRenderer::areSamplesQueuesReadyForMoreMediaData(size_t waterMark) const
{
    return m_framesBeingDecoded + decodedSamplesCount() <= waterMark;
}

void VideoMediaSampleRenderer::maybeBecomeReadyForMoreMediaData()
{
    RetainPtr renderer = rendererOrDisplayLayer();
    if (renderer && ![renderer isReadyForMoreMediaData]) {
        if (m_decompressionSession) {
            ThreadSafeWeakPtr weakThis { *this };
            [renderer requestMediaDataWhenReadyOnQueue:dispatchQueue() usingBlock:^{
                if (RefPtr protectedThis = weakThis.get()) {
                    [protectedThis->rendererOrDisplayLayer() stopRequestingMediaData];
                    protectedThis->maybeBecomeReadyForMoreMediaData();
                }
            }];
        }
        return;
    }

    if (!areSamplesQueuesReadyForMoreMediaData(SampleQueueLowWaterMark))
        return;

    callOnMainThread([weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_readyForMoreSampleFunction)
            protectedThis->m_readyForMoreSampleFunction();
    });
}

void VideoMediaSampleRenderer::stopRequestingMediaData()
{
    assertIsMainThread();

    m_readyForMoreSampleFunction = nil;

    auto stopRequestingMediaData = [weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            [protectedThis->rendererOrDisplayLayer() stopRequestingMediaData];
    };

    if (m_decompressionSession)
        dispatcher()->dispatch(WTFMove(stopRequestingMediaData)); // stopRequestingMediaData may deadlock if used on the main thread while enqueuing on the workqueue
    else
        stopRequestingMediaData();
}

void VideoMediaSampleRenderer::setPrefersDecompressionSession(bool prefers)
{
    if (m_prefersDecompressionSession == prefers || m_decompressionSession)
        return;

    m_prefersDecompressionSession = prefers;
}

void VideoMediaSampleRenderer::setTimebase(RetainPtr<CMTimebaseRef>&& timebase)
{
    if (!timebase)
        return;

    Locker locker { m_lock };

    ASSERT(!m_timebase);

    m_timebase = WTFMove(timebase);

    m_timerSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatchQueue()));
    dispatch_source_set_event_handler(m_timerSource.get(), [weakThis = ThreadSafeWeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        protectedThis->ensureOnDispatcher([weakThis] {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !protectedThis->decodedSamplesCount())
                return;
            bool samplesPurged = protectedThis->purgeDecodedSampleQueue();
            if (RetainPtr nextSample = (CMSampleBufferRef)const_cast<void*>(PAL::CMBufferQueueGetHead(protectedThis->m_decodedSampleQueue.get())))
                protectedThis->maybeQueueFrameForDisplay(nextSample.get());
            if (samplesPurged)
                protectedThis->maybeBecomeReadyForMoreMediaData();
        });
    });
    dispatch_activate(m_timerSource.get());
    PAL::CMTimebaseAddTimerDispatchSource(m_timebase.get(), m_timerSource.get());
}

void VideoMediaSampleRenderer::clearTimebase()
{
    Locker locker { m_lock };

    if (!m_timebase)
        return;

    PAL::CMTimebaseRemoveTimerDispatchSource(m_timebase.get(), m_timerSource.get());
    dispatch_source_cancel(m_timerSource.get());
    m_timerSource = nullptr;
}

RetainPtr<CMTimebaseRef> VideoMediaSampleRenderer::timebase() const
{
    Locker locker { m_lock };

    return m_timebase;
}

void VideoMediaSampleRenderer::enqueueSample(const MediaSample& sample)
{
    ASSERT(sample.platformSampleType() == PlatformSample::Type::CMSampleBufferType);
    if (sample.platformSampleType() != PlatformSample::Type::CMSampleBufferType)
        return;

    auto cmSampleBuffer = sample.platformSample().sample.cmSampleBuffer;

#if PLATFORM(IOS_FAMILY)
    if (!m_decompressionSession && !m_currentCodec) {
        // Only use a decompression session for vp8 or vp9 when software decoded.
        CMVideoFormatDescriptionRef videoFormatDescription = PAL::CMSampleBufferGetFormatDescription(cmSampleBuffer);
        auto fourCC = PAL::CMFormatDescriptionGetMediaSubType(videoFormatDescription);
        if (fourCC == 'vp08' || (fourCC == 'vp09' && !(canLoad_VideoToolbox_VTIsHardwareDecodeSupported() && VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9))))
            initializeDecompressionSession();
        m_currentCodec = fourCC;
    }
#endif

    if (!m_decompressionSession && (!renderer() || (prefersDecompressionSession() && !sample.isProtected())))
        initializeDecompressionSession();

    if (!m_decompressionSession) {
        [renderer() enqueueSampleBuffer:cmSampleBuffer];
        return;
    }

    ++m_framesBeingDecoded;
    dispatcher()->dispatch([weakThis = ThreadSafeWeakPtr { *this }, sample = RetainPtr { cmSampleBuffer }, flushId = m_flushId.load()]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        assertIsCurrent(protectedThis->dispatcher().get());

        if (flushId != protectedThis->m_flushId) {
            protectedThis->m_compressedSampleQueue.clear();
            return;
        }
        protectedThis->m_compressedSampleQueue.append({ WTFMove(sample), flushId });
        protectedThis->decodeNextSample();
    });
}

void VideoMediaSampleRenderer::decodeNextSample()
{
    assertIsCurrent(dispatcher().get());

    if (m_isDecodingSample || m_gotDecodingError)
        return;

    if (m_compressedSampleQueue.isEmpty())
        return;

    auto [sample, flushId] = m_compressedSampleQueue.takeFirst();
    if (flushId != m_flushId)
        return decodeNextSample();

    bool displaying = !MediaSampleAVFObjC::isCMSampleBufferNonDisplaying(sample.get());

    if (!shouldDecodeSample(sample.get(), displaying)) {
        --m_framesBeingDecoded;
        ASSERT(m_framesBeingDecoded >= 0);
        ++m_totalVideoFrames;
        ++m_droppedVideoFrames;

        maybeBecomeReadyForMoreMediaData();

        decodeNextSample();
        return;
    }

    auto decodePromise = m_decompressionSession->decodeSample(sample.get(), displaying);
    m_isDecodingSample = true;
    decodePromise->whenSettled(dispatcher(), [weakThis = ThreadSafeWeakPtr { *this }, this, displaying, flushId = flushId](auto&& result) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        assertIsCurrent(dispatcher().get());

        m_isDecodingSample = false;

        if (flushId != m_flushId)
            return;

        --m_framesBeingDecoded;
        ASSERT(m_framesBeingDecoded >= 0);
        ++m_totalVideoFrames;

        if (!result) {
            m_gotDecodingError = true;
            ++m_corruptedVideoFrames;

            callOnMainThread([protectedThis, status = result.error()] {
                assertIsMainThread();

                if (protectedThis->renderer()) {
                    // Simulate AVSBDL decoding error.
                    RetainPtr error = [NSError errorWithDomain:@"com.apple.WebKit" code:status userInfo:nil];
                    NSDictionary *userInfoDict = @{ (__bridge NSString *)AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey: (__bridge NSError *)error.get() };
                    [NSNotificationCenter.defaultCenter postNotificationName:AVSampleBufferDisplayLayerFailedToDecodeNotification object:protectedThis->renderer() userInfo:userInfoDict];
                    [NSNotificationCenter.defaultCenter postNotificationName:AVSampleBufferVideoRendererDidFailToDecodeNotification object:protectedThis->renderer() userInfo:userInfoDict];
                    return;
                }
                protectedThis->notifyErrorHasOccurred(status);
            });
            return;
        }

        if (displaying && *result)
            decodedFrameAvailable(WTFMove(*result));

        decodeNextSample();
    });

    maybeBecomeReadyForMoreMediaData();
}

bool VideoMediaSampleRenderer::shouldDecodeSample(CMSampleBufferRef sample, bool displaying)
{
    if (!displaying)
        return true;

    RetainPtr timebase = this->timebase();
    if (!timebase)
        return true;

    auto currentTime = PAL::CMTimebaseGetTime(timebase.get());
    auto presentationStartTime = PAL::CMSampleBufferGetPresentationTimeStamp(sample);
    auto duration = PAL::CMSampleBufferGetDuration(sample);
    auto presentationEndTime = PAL::CMTimeAdd(presentationStartTime, duration);
    if (PAL::CMTimeCompare(presentationEndTime, currentTime) >= 0)
        return true;

    CFArrayRef attachments = PAL::CMSampleBufferGetSampleAttachmentsArray(sample, false);
    if (!attachments)
        return true;

    for (CFIndex index = 0, count = CFArrayGetCount(attachments); index < count; ++index) {
        CFDictionaryRef attachmentDict = (CFDictionaryRef)CFArrayGetValueAtIndex(attachments, index);
        if (CFDictionaryGetValue(attachmentDict, PAL::kCMSampleAttachmentKey_IsDependedOnByOthers) == kCFBooleanFalse)
            return false;
    }

    return true;
}

void VideoMediaSampleRenderer::initializeDecompressionSession()
{
    assertIsMainThread();

    ASSERT(!m_decompressionSession && !m_decodedSampleQueue);
    if (m_decompressionSession)
        return;

    m_decodedSampleQueue = WebCoreDecompressionSession::createBufferQueue();
    m_decompressionSession = WebCoreDecompressionSession::createOpenGL();

    resetReadyForMoreSample();
}

void VideoMediaSampleRenderer::decodedFrameAvailable(RetainPtr<CMSampleBufferRef>&& sample)
{
    assertIsCurrent(dispatcher());

    assignResourceOwner(sample.get());

    if (maybeQueueFrameForDisplay(sample.get()))
        m_droppedVideoFrames++;
    else
        PAL::CMBufferQueueEnqueue(m_decodedSampleQueue.get(), sample.get());

    purgeDecodedSampleQueue();
}

bool VideoMediaSampleRenderer::maybeQueueFrameForDisplay(CMSampleBufferRef sample)
{
    assertIsCurrent(dispatcher().get());

    if (RetainPtr timebase = this->timebase()) {
        CMTime currentTime = PAL::CMTimebaseGetTime(timebase.get());
        CMTime presentationTime = PAL::CMSampleBufferGetOutputPresentationTimeStamp(sample);

        // Always display the first video frame available if we aren't displaying any yet, regardless of its time as it's always either:
        // 1- The first frame of the video.
        // 2- The first visible frame after a seek.

        if (m_isDisplayingSample && PAL::CMTimeCompare(presentationTime, currentTime) > 0)
            return false; // Too early to display.

        CMTime duration = PAL::CMSampleBufferGetOutputDuration(sample);
        CMTime presentationEndTime = PAL::CMTimeAdd(presentationTime, duration);
        if (m_isDisplayingSample && PAL::CMTimeCompare(presentationTime, presentationEndTime) && PAL::CMTimeCompare(presentationEndTime, currentTime) < 0)
            return true; // Too late to be displayed..

        m_lastDisplayedTime = currentTime;
    }

    [rendererOrDisplayLayer() enqueueSampleBuffer:sample];
    m_isDisplayingSample = true;

    notifyHasAvailableVideoFrame();

    return false;
}

void VideoMediaSampleRenderer::flushCompressedSampleQueue()
{
    assertIsMainThread();

    ++m_flushId;
    m_framesBeingDecoded = 0;
    m_gotDecodingError = false;
}

void VideoMediaSampleRenderer::flushDecodedSampleQueue()
{
    assertIsCurrent(dispatcher().get());
    ASSERT(m_decodedSampleQueue);

    PAL::CMBufferQueueReset(m_decodedSampleQueue.get());
    m_isDisplayingSample = false;
}

void VideoMediaSampleRenderer::cancelTimer()
{
    Locker locker { m_lock };
    PAL::CMTimebaseSetTimerDispatchSourceNextFireTime(m_timebase.get(), m_timerSource.get(), PAL::kCMTimeInvalid, 0);
}

bool VideoMediaSampleRenderer::purgeDecodedSampleQueue()
{
    assertIsCurrent(dispatcher().get());

    if (!m_decodedSampleQueue)
        return false;

    auto [timebase, timerSource] = [&] {
        Locker locker { m_lock };
        return std::make_pair(m_timebase, m_timerSource);
    }();
    if (!timebase || !timerSource)
        return false;

    CMTime currentTime = PAL::CMTimebaseGetTime(timebase.get());
    CMTime nextPurgeTime = PAL::kCMTimeInvalid;
    CMTime lastPresentationEndTime = PAL::kCMTimeInvalid;
    bool samplesPurged = false;

    while (RetainPtr nextSample = (CMSampleBufferRef)const_cast<void*>(PAL::CMBufferQueueGetHead(m_decodedSampleQueue.get()))) {
        CMTime duration = PAL::CMSampleBufferGetOutputDuration(nextSample.get());

        // Always leave the last sample if it doesn't have a duration. It is valid until the next one.
        if (decodedSamplesCount() == 1 && !PAL::CMTimeCompare(duration, PAL::kCMTimeZero))
            break;

        CMTime presentationTime = PAL::CMSampleBufferGetOutputPresentationTimeStamp(nextSample.get());
        CMTime presentationEndTime = PAL::CMTimeAdd(presentationTime, duration);
        if (PAL::CMTimeCompare(presentationEndTime, currentTime) >= 0) {
            nextPurgeTime = CMTIME_IS_VALID(lastPresentationEndTime) && PAL::CMTimeCompare(presentationTime, lastPresentationEndTime) > 0 ? presentationTime : presentationEndTime;
            break;
        }

        if (m_lastDisplayedTime && PAL::CMTimeCompare(*m_lastDisplayedTime, presentationTime) <= 0)
            m_droppedVideoFrames++; // This frame was never displayed.

        RetainPtr sampleToBePurged = adoptCF(PAL::CMBufferQueueDequeueAndRetain(m_decodedSampleQueue.get()));
        sampleToBePurged = nil;
        lastPresentationEndTime = presentationEndTime;
        samplesPurged = true;
    }

    if (!CMTIME_IS_VALID(nextPurgeTime))
        return samplesPurged;

    PAL::CMTimebaseSetTimerDispatchSourceNextFireTime(timebase.get(), timerSource.get(), nextPurgeTime, 0);

    return samplesPurged;
}

void VideoMediaSampleRenderer::flush()
{
    assertIsMainThread();
    [renderer() flush];

    if (!m_decompressionSession)
        return;

    flushCompressedSampleQueue();
    cancelTimer();

    m_decompressionSession->flush();
    dispatcher()->dispatch([weakThis = ThreadSafeWeakPtr { *this }]() mutable {
        if (RefPtr protectedThis = weakThis.get()) {
            protectedThis->flushDecodedSampleQueue();
            protectedThis->maybeBecomeReadyForMoreMediaData();
        }
    });
}

void VideoMediaSampleRenderer::requestMediaDataWhenReady(Function<void()>&& function)
{
    assertIsMainThread();
    m_readyForMoreSampleFunction = WTFMove(function);
    resetReadyForMoreSample();
}

void VideoMediaSampleRenderer::resetReadyForMoreSample()
{
    assertIsMainThread();

    if (!rendererOrDisplayLayer() || m_decompressionSession) {
        maybeBecomeReadyForMoreMediaData();
        return;
    }

    ThreadSafeWeakPtr weakThis { *this };
    [rendererOrDisplayLayer() requestMediaDataWhenReadyOnQueue:dispatchQueue() usingBlock:^{
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->maybeBecomeReadyForMoreMediaData();
    }];
}

void VideoMediaSampleRenderer::expectMinimumUpcomingSampleBufferPresentationTime(const MediaTime& time)
{
    assertIsMainThread();
    if (m_decompressionSession || ![PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(expectMinimumUpcomingSampleBufferPresentationTime:)])
        return;

    [renderer() expectMinimumUpcomingSampleBufferPresentationTime:PAL::toCMTime(time)];
}

void VideoMediaSampleRenderer::resetUpcomingSampleBufferPresentationTimeExpectations()
{
    assertIsMainThread();
    if (m_decompressionSession || ![PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(resetUpcomingSampleBufferPresentationTimeExpectations)])
        return;

    [renderer() resetUpcomingSampleBufferPresentationTimeExpectations];
}

WebSampleBufferVideoRendering *VideoMediaSampleRenderer::renderer() const
{
    if (m_displayLayer) {
        assertIsMainThread();
        return m_displayLayer.get();
    }
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    return m_renderer.get();
#else
    return nil;
#endif
}

WebSampleBufferVideoRendering *VideoMediaSampleRenderer::rendererOrDisplayLayer() const
{
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    return m_renderer.get();
#else
    if (m_displayLayer) {
        assertIsMainThread();
        return m_displayLayer.get();
    }
    return nil;
#endif
}

AVSampleBufferDisplayLayer *VideoMediaSampleRenderer::displayLayer() const
{
    assertIsMainThread();
    return m_displayLayer.get();
}

auto VideoMediaSampleRenderer::copyDisplayedPixelBuffer() -> DisplayedPixelBufferEntry
{
    assertIsMainThread();

    if (!m_decompressionSession) {
        RetainPtr buffer = adoptCF([renderer() copyDisplayedPixelBuffer]);
        if (auto surface = CVPixelBufferGetIOSurface(buffer.get()); surface && m_resourceOwner)
            IOSurface::setOwnershipIdentity(surface, m_resourceOwner);
        return { WTFMove(buffer), MediaTime::invalidTime() };
    }

    RetainPtr<CVPixelBufferRef> imageBuffer;
    MediaTime presentationTimeStamp;

    ensureOnDispatcherSync([&] {
        RetainPtr timebase = this->timebase();
        if (!timebase)
            return;

        purgeDecodedSampleQueue();

        CMTime currentTime = PAL::CMTimebaseGetTime(timebase.get());
        auto nextSample = (CMSampleBufferRef)const_cast<void*>(PAL::CMBufferQueueGetHead(m_decodedSampleQueue.get()));
        CMTime presentationTime = PAL::CMSampleBufferGetOutputPresentationTimeStamp(nextSample);

        if (PAL::CMTimeCompare(presentationTime, currentTime) > 0)
            return;

        RetainPtr sampleToBePurged = adoptCF((CMSampleBufferRef)const_cast<void*>(PAL::CMBufferQueueDequeueAndRetain(m_decodedSampleQueue.get())));
        imageBuffer = (CVPixelBufferRef)PAL::CMSampleBufferGetImageBuffer(sampleToBePurged.get());
        presentationTimeStamp = PAL::toMediaTime(presentationTime);
    });

    if (!imageBuffer)
        return { nullptr, MediaTime::invalidTime() };

    ASSERT(CFGetTypeID(imageBuffer.get()) == CVPixelBufferGetTypeID());
    if (CFGetTypeID(imageBuffer.get()) != CVPixelBufferGetTypeID())
        return { nullptr, MediaTime::invalidTime() };
    return { WTFMove(imageBuffer), presentationTimeStamp };
}

unsigned VideoMediaSampleRenderer::totalVideoFrames() const
{
    if (m_decompressionSession)
        return m_totalVideoFrames;
#if PLATFORM(WATCHOS)
    return 0;
#else
    return [renderer() videoPerformanceMetrics].totalNumberOfVideoFrames;
#endif
}

unsigned VideoMediaSampleRenderer::droppedVideoFrames() const
{
    if (m_decompressionSession)
        return m_droppedVideoFrames;
#if PLATFORM(WATCHOS)
    return 0;
#else
    return [renderer() videoPerformanceMetrics].numberOfDroppedVideoFrames;
#endif
}

unsigned VideoMediaSampleRenderer::corruptedVideoFrames() const
{
    if (m_decompressionSession)
        return m_corruptedVideoFrames;
#if PLATFORM(WATCHOS)
    return 0;
#else
    return [renderer() videoPerformanceMetrics].numberOfCorruptedVideoFrames;
#endif
}

MediaTime VideoMediaSampleRenderer::totalFrameDelay() const
{
    if (m_decompressionSession)
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

void VideoMediaSampleRenderer::assignResourceOwner(CMSampleBufferRef sampleBuffer)
{
    assertIsCurrent(dispatcher());
    if (!m_resourceOwner || !sampleBuffer)
        return;

    RetainPtr<CVPixelBufferRef> imageBuffer = (CVPixelBufferRef)PAL::CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!imageBuffer || CFGetTypeID(imageBuffer.get()) != CVPixelBufferGetTypeID())
        return;

    if (auto surface = CVPixelBufferGetIOSurface(imageBuffer.get()))
        IOSurface::setOwnershipIdentity(surface, m_resourceOwner);
}

void VideoMediaSampleRenderer::notifyWhenHasAvailableVideoFrame(Function<void()>&& callback)
{
    assertIsMainThread();

    m_hasAvailableFrameCallback = WTFMove(callback);
}

void VideoMediaSampleRenderer::notifyHasAvailableVideoFrame()
{
    assertIsCurrent(dispatcher());

    callOnMainThread([weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get()) {
            assertIsMainThread();
            if (auto callback = std::exchange(protectedThis->m_hasAvailableFrameCallback, { }))
                callback();
        }
    });
}

void VideoMediaSampleRenderer::notifyWhenDecodingErrorOccurred(Function<void(OSStatus)>&& callback)
{
    assertIsMainThread();
    m_errorOccurredFunction = WTFMove(callback);
}

void VideoMediaSampleRenderer::notifyErrorHasOccurred(OSStatus status)
{
    assertIsMainThread();

    if (m_errorOccurredFunction)
        m_errorOccurredFunction(status);
}

Ref<GuaranteedSerialFunctionDispatcher> VideoMediaSampleRenderer::dispatcher() const
{
    return m_workQueue ? *m_workQueue : static_cast<GuaranteedSerialFunctionDispatcher&>(MainThreadDispatcher::singleton());
}

dispatch_queue_t VideoMediaSampleRenderer::dispatchQueue() const
{
    return m_workQueue ? m_workQueue->dispatchQueue() : WorkQueue::main().dispatchQueue();
}

void VideoMediaSampleRenderer::ensureOnDispatcher(Function<void()>&& function) const
{
    if (dispatcher()->isCurrent()) {
        function();
        return;
    }

    if (m_workQueue)
        return m_workQueue->dispatch(WTFMove(function));
    callOnMainThread(WTFMove(function));
}

void VideoMediaSampleRenderer::ensureOnDispatcherSync(Function<void()>&& function) const
{
    if (dispatcher()->isCurrent()) {
        function();
        return;
    }

    if (m_workQueue)
        return m_workQueue->dispatchSync(WTFMove(function));
    callOnMainThreadAndWait(WTFMove(function));
}

} // namespace WebCore
