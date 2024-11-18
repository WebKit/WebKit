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

static constexpr CMItemCount CompressedSampleQueueHighWaterMark = 30;
static constexpr CMItemCount CompressedSampleQueueLowWaterMark = 15;

VideoMediaSampleRenderer::VideoMediaSampleRenderer(WebSampleBufferVideoRendering *renderering)
    : m_workQueue(WorkQueue::create("VideoMediaSampleRenderer Queue"_s))
{
    if (auto *displayLayer = dynamic_objc_cast<AVSampleBufferDisplayLayer>(renderering))
        m_displayLayer = displayLayer;
    else if (auto *renderer = dynamic_objc_cast<AVSampleBufferVideoRenderer>(renderering))
        m_renderer = renderer;
}

VideoMediaSampleRenderer::~VideoMediaSampleRenderer()
{
    assertIsMainThread();

    flushCompressedSampleQueue();

    if (m_displayLayer) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [m_displayLayer flush];
        [m_displayLayer stopRequestingMediaData];
        ALLOW_DEPRECATED_DECLARATIONS_END
        m_displayLayer = nil;
    }

    if (m_renderer) {
        [m_renderer flush];
        [m_renderer stopRequestingMediaData];
        m_renderer = nil;
    }

    if (m_decompressionSession) {
        m_decompressionSession->invalidate();
        m_decompressionSession = nullptr;
    }

    clearTimebase();
}

bool VideoMediaSampleRenderer::isReadyForMoreMediaData() const
{
    assertIsMainThread();

    if (m_framesBeingDecoded >= CompressedSampleQueueHighWaterMark)
        return false;

    return [renderer() isReadyForMoreMediaData];
}

void VideoMediaSampleRenderer::maybeBecomeReadyForMoreMediaData()
{
    assertIsMainThread();

    if (![renderer() isReadyForMoreMediaData])
        return;

    if (m_framesBeingDecoded >= CompressedSampleQueueLowWaterMark)
        return;

    if (m_readyForMoreSampleFunction)
        m_readyForMoreSampleFunction();
}

void VideoMediaSampleRenderer::stopRequestingMediaData()
{
    assertIsMainThread();
    [renderer() stopRequestingMediaData];

    m_readyForMoreSampleFunction = nil;
}

void VideoMediaSampleRenderer::setPrefersDecompressionSession(bool prefers)
{
    if (m_prefersDecompressionSession == prefers)
        return;

    m_prefersDecompressionSession = prefers;
    if (!m_prefersDecompressionSession && m_decompressionSession) {
        m_decompressionSession->invalidate();
        m_decompressionSession = nullptr;
    }
}

void VideoMediaSampleRenderer::setTimebase(RetainPtr<CMTimebaseRef>&& timebase)
{
    if (!timebase)
        return;

    Locker locker { m_lock };

    ASSERT(!m_timebase);

    m_timebase = WTFMove(timebase);

    m_timerSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, m_workQueue->dispatchQueue()));
    dispatch_source_set_event_handler(m_timerSource.get(), [weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get()) {
            assertIsCurrent(protectedThis->m_workQueue.get());
            if (!protectedThis->m_decodedSampleQueue.get())
                return;
            protectedThis->purgeDecodedSampleQueue();
            if (RetainPtr nextSample = (CMSampleBufferRef)const_cast<void*>(PAL::CMBufferQueueGetHead(protectedThis->m_decodedSampleQueue.get())))
                protectedThis->maybeQueueFrameForDisplay(nextSample.get());
        }
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

    if (!m_decompressionSession && prefersDecompressionSession() && !sample.isProtected())
        initializeDecompressionSession();

    if (!m_decompressionSession) {
        [renderer() enqueueSampleBuffer:cmSampleBuffer];
        return;
    }

    ++m_framesBeingDecoded;
    m_workQueue->dispatch([weakThis = ThreadSafeWeakPtr { *this }, sample = RetainPtr { cmSampleBuffer }, flushId = m_flushId.load()]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        assertIsCurrent(protectedThis->m_workQueue.get());

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
    assertIsCurrent(m_workQueue.get());

    if (m_isDecodingSample || m_gotDecodingError)
        return;

    if (m_compressedSampleQueue.isEmpty())
        return;

    auto [sample, flushId] = m_compressedSampleQueue.takeFirst();
    if (flushId != m_flushId)
        return decodeNextSample();

    --m_framesBeingDecoded;
    ASSERT(m_framesBeingDecoded >= 0);

    bool displaying = !MediaSampleAVFObjC::isCMSampleBufferNonDisplaying(sample.get());
    auto decodePromise = m_decompressionSession->decodeSample(sample.get(), displaying);
    m_isDecodingSample = true;
    decodePromise->whenSettled(m_workQueue, [weakThis = ThreadSafeWeakPtr { *this }, displaying, flushId = flushId](auto&& result) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        assertIsCurrent(protectedThis->m_workQueue.get());

        protectedThis->m_isDecodingSample = false;
        if (flushId != protectedThis->m_flushId)
            return;

        if (!result) {
            protectedThis->m_gotDecodingError = true;

            callOnMainThread([protectedThis = WTFMove(protectedThis), status = result.error()] {
                assertIsMainThread();

                // Simulate AVSBDL decoding error.
                RetainPtr error = [NSError errorWithDomain:@"com.apple.WebKit" code:status userInfo:nil];
                NSDictionary *userInfoDict = @{ (__bridge NSString *)AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey: (__bridge NSError *)error.get() };
                [NSNotificationCenter.defaultCenter postNotificationName:AVSampleBufferDisplayLayerFailedToDecodeNotification object:protectedThis->renderer() userInfo:userInfoDict];
                [NSNotificationCenter.defaultCenter postNotificationName:AVSampleBufferVideoRendererDidFailToDecodeNotification object:protectedThis->renderer() userInfo:userInfoDict];
            });
            return;
        }

        if (displaying && *result) {
            RetainPtr<CMSampleBufferRef> retainedResult = *result;
            protectedThis->decodedFrameAvailable(WTFMove(retainedResult));
        }

        protectedThis->decodeNextSample();
    });

    callOnMainThread([weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->maybeBecomeReadyForMoreMediaData();
    });
}

void VideoMediaSampleRenderer::initializeDecompressionSession()
{
    assertIsMainThread();

    ASSERT(!m_decompressionSession);
    if (m_decompressionSession)
        return;

    m_decompressionSession = WebCoreDecompressionSession::createOpenGL();

    resetReadyForMoreSample();
}

void VideoMediaSampleRenderer::decodedFrameAvailable(RetainPtr<CMSampleBufferRef>&& sample)
{
    assertIsCurrent(m_workQueue);

    assignResourceOwner(sample.get());

    purgeDecodedSampleQueue();

    if (RetainPtr timebase = this->timebase())
        PAL::CMBufferQueueEnqueue(ensureDecodedSampleQueue(), sample.get());

    maybeQueueFrameForDisplay(sample.get());
}

void VideoMediaSampleRenderer::maybeQueueFrameForDisplay(CMSampleBufferRef sample)
{
    assertIsCurrent(m_workQueue.get());

    if (RetainPtr timebase = this->timebase()) {
        CMTime currentTime = PAL::CMTimebaseGetTime(timebase.get());
        CMTime presentationTime = PAL::CMSampleBufferGetOutputPresentationTimeStamp(sample);

        // Always display the first video frame available if we aren't displaying any yet, regardless of its time as it's always either:
        // 1- The first frame of the video.
        // 2- The first visible frame after a seek.

        if (m_isDisplayingSample && PAL::CMTimeCompare(presentationTime, currentTime) > 0)
            return; // Too early to display.

        CMTime duration = PAL::CMSampleBufferGetOutputDuration(sample);
        CMTime presentationEndTime = PAL::CMTimeAdd(presentationTime, duration);
        if (m_isDisplayingSample && PAL::CMTimeCompare(presentationTime, presentationEndTime) && PAL::CMTimeCompare(presentationEndTime, currentTime) < 0)
            return; // Too late to be displayed..
    }

    m_isDisplayingSample = true;

    if (m_renderer)
        [m_renderer enqueueSampleBuffer:sample];
    else if (m_displayLayer) {
        callOnMainThread([weakThis = ThreadSafeWeakPtr { *this }, sample = RetainPtr { sample }] {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (RefPtr protectedThis = weakThis.get())
                [protectedThis->m_displayLayer enqueueSampleBuffer:sample.get()];
            ALLOW_DEPRECATED_DECLARATIONS_END
        });
    }
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
    assertIsCurrent(m_workQueue.get());
    if (!m_decodedSampleQueue)
        return;

    PAL::CMBufferQueueReset(m_decodedSampleQueue.get());
    m_isDisplayingSample = false;
}

void VideoMediaSampleRenderer::cancelTimer()
{
    Locker locker { m_lock };
    PAL::CMTimebaseSetTimerDispatchSourceNextFireTime(m_timebase.get(), m_timerSource.get(), PAL::kCMTimeInvalid, 0);
}

void VideoMediaSampleRenderer::purgeDecodedSampleQueue()
{
    assertIsCurrent(m_workQueue.get());

    if (!m_decodedSampleQueue)
        return;

    RetainPtr timebase = this->timebase();
    if (!timebase)
        return;

    CMTime currentTime = PAL::CMTimebaseGetTime(timebase.get());
    CMTime nextPurgeTime = PAL::kCMTimeInvalid;

    while (RetainPtr nextSample = (CMSampleBufferRef)const_cast<void*>(PAL::CMBufferQueueGetHead(m_decodedSampleQueue.get()))) {
        CMTime duration = PAL::CMSampleBufferGetOutputDuration(nextSample.get());

        // Always leave the last sample if it doesn't have a duration. It is valid until the next one.
        if (PAL::CMBufferQueueGetBufferCount(m_decodedSampleQueue.get()) == 1 && !PAL::CMTimeCompare(duration, PAL::kCMTimeZero))
            break;

        CMTime presentationTime = PAL::CMSampleBufferGetOutputPresentationTimeStamp(nextSample.get());
        CMTime presentationEndTime = PAL::CMTimeAdd(presentationTime, duration);
        if (PAL::CMTimeCompare(presentationEndTime, currentTime) >= 0) {
            nextPurgeTime = presentationEndTime;
            break;
        }

        RetainPtr sampleToBePurged = adoptCF(PAL::CMBufferQueueDequeueAndRetain(m_decodedSampleQueue.get()));
        sampleToBePurged = nil;
    }

    if (!CMTIME_IS_VALID(nextPurgeTime))
        return;

    Locker locker { m_lock };
    if (!m_timebase)
        return;
    PAL::CMTimebaseSetTimerDispatchSourceNextFireTime(m_timebase.get(), m_timerSource.get(), nextPurgeTime, 0);
}

CMBufferQueueRef VideoMediaSampleRenderer::ensureDecodedSampleQueue()
{
    assertIsCurrent(m_workQueue.get());
    if (!m_decodedSampleQueue)
        m_decodedSampleQueue = WebCoreDecompressionSession::createBufferQueue();
    return m_decodedSampleQueue.get();
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
    m_workQueue->dispatch([weakThis = ThreadSafeWeakPtr { *this }]() mutable {
        if (RefPtr protectedThis = weakThis.get()) {
            protectedThis->flushDecodedSampleQueue();
            callOnMainThread([weakThis = WTFMove(weakThis)] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->maybeBecomeReadyForMoreMediaData();
            });
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
    ThreadSafeWeakPtr weakThis { *this };
    [renderer() requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->maybeBecomeReadyForMoreMediaData();
    }];
}

void VideoMediaSampleRenderer::expectMinimumUpcomingSampleBufferPresentationTime(const MediaTime& time)
{
    assertIsMainThread();
    if (![PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(expectMinimumUpcomingSampleBufferPresentationTime:)])
        return;

    if (!m_decompressionSession)
        [renderer() expectMinimumUpcomingSampleBufferPresentationTime:PAL::toCMTime(time)];
}

void VideoMediaSampleRenderer::resetUpcomingSampleBufferPresentationTimeExpectations()
{
    assertIsMainThread();
    if (![PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(resetUpcomingSampleBufferPresentationTimeExpectations)])
        return;

    [renderer() resetUpcomingSampleBufferPresentationTimeExpectations];
}

WebSampleBufferVideoRendering *VideoMediaSampleRenderer::renderer() const
{
    assertIsMainThread();
    if (m_displayLayer)
        return m_displayLayer.get();
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    return m_renderer.get();
#else
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
        return { WTFMove(buffer), MediaTime::invalidTime() };
    }

    RetainPtr<CVPixelBufferRef> imageBuffer;
    MediaTime presentationTimeStamp;

    m_workQueue->dispatchSync([&] {
        assertIsCurrent(m_workQueue.get());

        RetainPtr timebase = this->timebase();
        if (!timebase || !m_decodedSampleQueue)
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

CGRect VideoMediaSampleRenderer::bounds() const
{
    return [displayLayer() bounds];
}

unsigned VideoMediaSampleRenderer::totalVideoFrames() const
{
    if (m_decompressionSession)
        return m_decompressionSession->totalVideoFrames();
#if PLATFORM(WATCHOS)
    return 0;
#else
    return [renderer() videoPerformanceMetrics].totalNumberOfVideoFrames;
#endif
}

unsigned VideoMediaSampleRenderer::droppedVideoFrames() const
{
    if (m_decompressionSession)
        return m_decompressionSession->droppedVideoFrames();
#if PLATFORM(WATCHOS)
    return 0;
#else
    return [renderer() videoPerformanceMetrics].numberOfDroppedVideoFrames;
#endif
}

unsigned VideoMediaSampleRenderer::corruptedVideoFrames() const
{
    if (m_decompressionSession)
        return m_decompressionSession->corruptedVideoFrames();
#if PLATFORM(WATCHOS)
    return 0;
#else
    return [renderer() videoPerformanceMetrics].numberOfCorruptedVideoFrames;
#endif
}

MediaTime VideoMediaSampleRenderer::totalFrameDelay() const
{
    if (m_decompressionSession)
        return m_decompressionSession->totalFrameDelay();
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
    assertIsCurrent(m_workQueue);
    if (!m_resourceOwner || !sampleBuffer)
        return;

    RetainPtr<CVPixelBufferRef> imageBuffer = (CVPixelBufferRef)PAL::CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!imageBuffer || CFGetTypeID(imageBuffer.get()) != CVPixelBufferGetTypeID())
        return;

    if (auto surface = CVPixelBufferGetIOSurface(imageBuffer.get()))
        IOSurface::setOwnershipIdentity(surface, m_resourceOwner);
}


} // namespace WebCore
