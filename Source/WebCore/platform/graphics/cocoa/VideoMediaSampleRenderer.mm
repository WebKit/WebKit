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
    if (auto *displayLayer = dynamic_objc_cast<AVSampleBufferDisplayLayer>(renderering, PAL::getAVSampleBufferDisplayLayerClass()))
        m_displayLayer = displayLayer;
    else if (auto *renderer = dynamic_objc_cast<AVSampleBufferVideoRenderer>(renderering, PAL::getAVSampleBufferVideoRendererClass()))
        m_renderer = renderer;
}

VideoMediaSampleRenderer::~VideoMediaSampleRenderer()
{
    assertIsMainThread();

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

    setTimebase(nullptr);
}

bool VideoMediaSampleRenderer::isReadyForMoreMediaData() const
{
    assertIsMainThread();

    if (m_compressedSampleQueue && PAL::CMBufferQueueGetBufferCount(m_compressedSampleQueue.get()) >= CompressedSampleQueueHighWaterMark)
        return false;

    return [renderer() isReadyForMoreMediaData];
}

void VideoMediaSampleRenderer::maybeBecomeReadyForMoreMediaData()
{
    assertIsMainThread();

    if (![renderer() isReadyForMoreMediaData])
        return;

    if (m_compressedSampleQueue && PAL::CMBufferQueueGetBufferCount(m_compressedSampleQueue.get()) >= CompressedSampleQueueLowWaterMark)
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
    if (m_timebase) {
        PAL::CMTimebaseRemoveTimerDispatchSource(m_timebase.get(), m_timerSource.get());
        dispatch_source_cancel(m_timerSource.get());
        m_timerSource = nullptr;
    }

    m_timebase = WTFMove(timebase);

    if (m_timebase) {
        m_timerSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, m_workQueue->dispatchQueue()));
        dispatch_source_set_event_handler(m_timerSource.get(), [weakThis = ThreadSafeWeakPtr { *this }] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->purgeDecodedSampleQueue();
        });
        dispatch_activate(m_timerSource.get());
        PAL::CMTimebaseAddTimerDispatchSource(m_timebase.get(), m_timerSource.get());
    }
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

    PAL::CMBufferQueueEnqueue(ensureCompressedSampleQueue(), cmSampleBuffer);
    m_workQueue->dispatch([weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->decodeNextSample();
    });
}

void VideoMediaSampleRenderer::decodeNextSample()
{
    assertIsCurrent(m_workQueue);

    if (!m_compressedSampleQueue)
        return;

    if (m_isDecodingSample)
        return;

    auto sample = adoptCF(checked_cf_cast<CMSampleBufferRef>(PAL::CMBufferQueueDequeueAndRetain(m_compressedSampleQueue.get())));
    if (!sample)
        return;

    bool displaying = !MediaSampleAVFObjC::isCMSampleBufferNonDisplaying(sample.get());
    auto decodePromise = m_decompressionSession->decodeSample(sample.get(), displaying);
    m_isDecodingSample = true;
    decodePromise->whenSettled(m_workQueue, [weakThis = ThreadSafeWeakPtr { *this }, displaying] (auto&& result) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->m_isDecodingSample = false;

        if (!result) {
            ensureOnMainThread([protectedThis = WTFMove(protectedThis), status = result.error()] {
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
    if (m_decompressionSession)
        m_decompressionSession->invalidate();

    m_decompressionSession = WebCoreDecompressionSession::createOpenGL();
    m_decompressionSession->setTimebase(m_timebase.get());
    m_decompressionSession->setResourceOwner(m_resourceOwner);

    resetReadyForMoreSample();
}

void VideoMediaSampleRenderer::decodedFrameAvailable(RetainPtr<CMSampleBufferRef>&& sample)
{
    assertIsCurrent(m_workQueue);

    assignResourceOwner(sample.get());

    if (m_timebase) {
        purgeDecodedSampleQueue();
        PAL::CMBufferQueueEnqueue(ensureDecodedSampleQueue(), sample.get());
    }

    if (m_renderer)
        [m_renderer enqueueSampleBuffer:sample.get()];
    else if (m_displayLayer) {
        callOnMainThread([weakThis = ThreadSafeWeakPtr { *this }, sample = sample] {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (RefPtr protectedThis = weakThis.get())
                [protectedThis->m_displayLayer enqueueSampleBuffer:sample.get()];
            ALLOW_DEPRECATED_DECLARATIONS_END
        });
    }
}

void VideoMediaSampleRenderer::flushCompressedSampleQueue()
{
    assertIsCurrent(m_workQueue);
    if (!m_compressedSampleQueue)
        return;

    PAL::CMBufferQueueReset(m_compressedSampleQueue.get());
    PAL::CMTimebaseSetTimerDispatchSourceNextFireTime(m_timebase.get(), m_timerSource.get(), PAL::kCMTimeInvalid, 0);
}

void VideoMediaSampleRenderer::flushDecodedSampleQueue()
{
    assertIsCurrent(m_workQueue);
    if (!m_decodedSampleQueue)
        return;

    PAL::CMBufferQueueReset(m_decodedSampleQueue.get());
    PAL::CMTimebaseSetTimerDispatchSourceNextFireTime(m_timebase.get(), m_timerSource.get(), PAL::kCMTimeInvalid, 0);
}

void VideoMediaSampleRenderer::purgeDecodedSampleQueue()
{
    assertIsCurrent(m_workQueue);
    if (!m_decodedSampleQueue)
        return;

    if (!m_timebase)
        return;

    CMTime currentTime = PAL::CMTimebaseGetTime(m_timebase.get());
    CMTime nextPurgeTime = PAL::kCMTimeInvalid;

    while (RetainPtr nextSample = (CMSampleBufferRef)const_cast<void*>(PAL::CMBufferQueueGetHead(m_decodedSampleQueue.get()))) {
        CMTime presentationTime = PAL::CMSampleBufferGetOutputPresentationTimeStamp(nextSample.get());
        CMTime duration = PAL::CMSampleBufferGetOutputDuration(nextSample.get());
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

    PAL::CMTimebaseSetTimerDispatchSourceNextFireTime(m_timebase.get(), m_timerSource.get(), nextPurgeTime, 0);
}

CMBufferQueueRef VideoMediaSampleRenderer::ensureCompressedSampleQueue()
{
    assertIsMainThread();
    if (!m_compressedSampleQueue)
        m_compressedSampleQueue = WebCoreDecompressionSession::createBufferQueue();
    return m_compressedSampleQueue.get();
}

CMBufferQueueRef VideoMediaSampleRenderer::ensureDecodedSampleQueue()
{
    assertIsCurrent(m_workQueue);
    if (!m_decodedSampleQueue)
        m_decodedSampleQueue = WebCoreDecompressionSession::createBufferQueue();
    return m_decodedSampleQueue.get();
}

void VideoMediaSampleRenderer::flush()
{
    assertIsMainThread();
    [renderer() flush];

    if (m_decompressionSession)
        m_decompressionSession->flush();

    m_workQueue->dispatch([weakThis = ThreadSafeWeakPtr { *this }] () mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->flushCompressedSampleQueue();
        protectedThis->flushDecodedSampleQueue();

        callOnMainThread([weakThis = WTFMove(weakThis)] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->maybeBecomeReadyForMoreMediaData();
        });
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

RetainPtr<CVPixelBufferRef> VideoMediaSampleRenderer::copyDisplayedPixelBuffer()
{
    assertIsMainThread();

#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
    if (auto buffer = adoptCF([renderer() copyDisplayedPixelBuffer]))
        return buffer;
#endif

    RetainPtr<CVPixelBufferRef> imageBuffer;

    m_workQueue->dispatchSync([&] {
        if (!m_timebase)
            return;

        purgeDecodedSampleQueue();

        CMTime currentTime = PAL::CMTimebaseGetTime(m_timebase.get());
        auto nextSample = (CMSampleBufferRef)const_cast<void*>(PAL::CMBufferQueueGetHead(m_decodedSampleQueue.get()));
        CMTime presentationTime = PAL::CMSampleBufferGetOutputPresentationTimeStamp(nextSample);

        if (PAL::CMTimeCompare(presentationTime, currentTime) > 0)
            return;

        RetainPtr sampleToBePurged = adoptCF((CMSampleBufferRef)const_cast<void*>(PAL::CMBufferQueueDequeueAndRetain(m_decodedSampleQueue.get())));
        imageBuffer = (CVPixelBufferRef)PAL::CMSampleBufferGetImageBuffer(sampleToBePurged.get());
    });

    if (!imageBuffer)
        return nullptr;

    ASSERT(CFGetTypeID(imageBuffer.get()) == CVPixelBufferGetTypeID());
    if (CFGetTypeID(imageBuffer.get()) != CVPixelBufferGetTypeID())
        return nullptr;
    return imageBuffer;
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
