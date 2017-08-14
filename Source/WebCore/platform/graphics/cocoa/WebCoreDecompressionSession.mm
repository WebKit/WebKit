/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "WebCoreDecompressionSession.h"

#if USE(VIDEOTOOLBOX)

#import "Logging.h"
#import "MediaTimeAVFoundation.h"
#import "PixelBufferConformerCV.h"
#import <CoreMedia/CMBufferQueue.h>
#import <CoreMedia/CMFormatDescription.h>
#import <wtf/MainThread.h>
#import <wtf/MediaTime.h>
#import <wtf/StringPrintStream.h>
#import <wtf/Vector.h>

#import "CoreMediaSoftLink.h"
#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"

namespace WebCore {

WebCoreDecompressionSession::WebCoreDecompressionSession()
    : m_decompressionQueue(adoptOSObject(dispatch_queue_create("WebCoreDecompressionSession Decompression Queue", DISPATCH_QUEUE_SERIAL)))
    , m_enqueingQueue(adoptOSObject(dispatch_queue_create("WebCoreDecompressionSession Enqueueing Queue", DISPATCH_QUEUE_SERIAL)))
    , m_hasAvailableImageSemaphore(adoptOSObject(dispatch_semaphore_create(0)))
{
}

void WebCoreDecompressionSession::invalidate()
{
    m_invalidated = true;
    m_notificationCallback = nullptr;
    m_hasAvailableFrameCallback = nullptr;
    setTimebase(nullptr);
    if (m_timerSource)
        dispatch_source_cancel(m_timerSource.get());
}

void WebCoreDecompressionSession::setTimebase(CMTimebaseRef timebase)
{
    if (m_timebase == timebase)
        return;

    if (m_timebase)
        CMTimebaseRemoveTimerDispatchSource(m_timebase.get(), m_timerSource.get());

    m_timebase = timebase;

    if (m_timebase) {
        if (!m_timerSource) {
            m_timerSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue()));
            dispatch_source_set_event_handler(m_timerSource.get(), [this] {
                automaticDequeue();
            });
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200) || PLATFORM(IOS)
            dispatch_activate(m_timerSource.get());
#endif
        }
        CMTimebaseAddTimerDispatchSource(m_timebase.get(), m_timerSource.get());
    }
}

void WebCoreDecompressionSession::maybeBecomeReadyForMoreMediaDataCallback(void* refcon, CMBufferQueueTriggerToken)
{
    WebCoreDecompressionSession* session = static_cast<WebCoreDecompressionSession*>(refcon);
    session->maybeBecomeReadyForMoreMediaData();
}

void WebCoreDecompressionSession::maybeBecomeReadyForMoreMediaData()
{
    LOG(Media, "WebCoreDecompressionSession::maybeBecomeReadyForMoreMediaData(%p) - isReadyForMoreMediaData(%d), hasCallback(%d)", this, isReadyForMoreMediaData(), !!m_notificationCallback);
    if (!isReadyForMoreMediaData() || !m_notificationCallback)
        return;

    if (isMainThread()) {
        m_notificationCallback();
        return;
    }

    RefPtr<WebCoreDecompressionSession> protectedThis { this };
    dispatch_async(dispatch_get_main_queue(), [protectedThis] {
        if (protectedThis->m_notificationCallback)
            protectedThis->m_notificationCallback();
    });
}

void WebCoreDecompressionSession::enqueueSample(CMSampleBufferRef sampleBuffer, bool displaying)
{
    CMItemCount itemCount = 0;
    if (noErr != CMSampleBufferGetSampleTimingInfoArray(sampleBuffer, 0, nullptr, &itemCount))
        return;

    Vector<CMSampleTimingInfo> timingInfoArray;
    timingInfoArray.grow(itemCount);
    if (noErr != CMSampleBufferGetSampleTimingInfoArray(sampleBuffer, itemCount, timingInfoArray.data(), nullptr))
        return;

    if (!m_decompressionQueue)
        m_decompressionQueue = adoptOSObject(dispatch_queue_create("SourceBufferPrivateAVFObjC Decompression Queue", DISPATCH_QUEUE_SERIAL));

    // CMBufferCallbacks contains 64-bit pointers that aren't 8-byte aligned. To suppress the linker
    // warning about this, we prepend 4 bytes of padding when building for macOS.
#if PLATFORM(MAC)
    const size_t padSize = 4;
#else
    const size_t padSize = 0;
#endif

    if (!m_producerQueue) {
        CMBufferQueueRef outQueue { nullptr };
#pragma pack(push, 4)
        struct { uint8_t pad[padSize]; CMBufferCallbacks callbacks; } callbacks { { }, {
            0,
            nullptr,
            &getDecodeTime,
            &getPresentationTime,
            &getDuration,
            nullptr,
            &compareBuffers,
            nullptr,
            nullptr,
        } };
#pragma pack(pop)
        CMBufferQueueCreate(kCFAllocatorDefault, kMaximumCapacity, &callbacks.callbacks, &outQueue);
        m_producerQueue = adoptCF(outQueue);

        CMBufferQueueInstallTriggerWithIntegerThreshold(m_producerQueue.get(), maybeBecomeReadyForMoreMediaDataCallback, this, kCMBufferQueueTrigger_WhenBufferCountBecomesLessThan, kLowWaterMark, &m_didBecomeReadyTrigger);
    }

    if (!m_consumerQueue) {
        CMBufferQueueRef outQueue { nullptr };
#pragma pack(push, 4)
        struct { uint8_t pad[padSize]; CMBufferCallbacks callbacks; } callbacks { { }, {
            0,
            nullptr,
            &getDecodeTime,
            &getPresentationTime,
            &getDuration,
            nullptr,
            &compareBuffers,
            nullptr,
            nullptr,
        } };
#pragma pack(pop)
        CMBufferQueueCreate(kCFAllocatorDefault, kMaximumCapacity, &callbacks.callbacks, &outQueue);
        m_consumerQueue = adoptCF(outQueue);
    }

    ++m_framesBeingDecoded;

    LOG(Media, "WebCoreDecompressionSession::enqueueSample(%p) - framesBeingDecoded(%d)", this, m_framesBeingDecoded);

    dispatch_async(m_decompressionQueue.get(), [protectedThis = makeRefPtr(*this), strongBuffer = retainPtr(sampleBuffer), displaying] {
        protectedThis->decodeSample(strongBuffer.get(), displaying);
    });
}

void WebCoreDecompressionSession::decodeSample(CMSampleBufferRef sample, bool displaying)
{
    if (isInvalidated())
        return;

    CMVideoFormatDescriptionRef videoFormatDescription = CMSampleBufferGetFormatDescription(sample);
    if (m_decompressionSession && !VTDecompressionSessionCanAcceptFormatDescription(m_decompressionSession.get(), videoFormatDescription)) {
        VTDecompressionSessionWaitForAsynchronousFrames(m_decompressionSession.get());
        m_decompressionSession = nullptr;
    }

    if (!m_decompressionSession) {
        CMVideoFormatDescriptionRef videoFormatDescription = CMSampleBufferGetFormatDescription(sample);
        NSDictionary* videoDecoderSpecification = @{ (NSString *)kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder: @YES };
#if PLATFORM(IOS)
        NSDictionary* attributes = @{(NSString *)kCVPixelBufferIOSurfaceOpenGLESFBOCompatibilityKey: @YES};
#else
        NSDictionary* attributes = @{(NSString *)kCVPixelBufferIOSurfaceOpenGLFBOCompatibilityKey: @YES};
#endif
        VTDecompressionSessionRef decompressionSessionOut = nullptr;
        VTDecompressionOutputCallbackRecord callback {
            &decompressionOutputCallback,
            this,
        };
        if (noErr == VTDecompressionSessionCreate(kCFAllocatorDefault, videoFormatDescription, (CFDictionaryRef)videoDecoderSpecification, (CFDictionaryRef)attributes, &callback, &decompressionSessionOut))
            m_decompressionSession = adoptCF(decompressionSessionOut);
    }

    VTDecodeInfoFlags flags { kVTDecodeFrame_EnableTemporalProcessing };
    if (!displaying)
        flags |= kVTDecodeFrame_DoNotOutputFrame;

    VTDecompressionSessionDecodeFrame(m_decompressionSession.get(), sample, flags, reinterpret_cast<void*>(displaying), nullptr);
}

void WebCoreDecompressionSession::decompressionOutputCallback(void* decompressionOutputRefCon, void* sourceFrameRefCon, OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer, CMTime presentationTimeStamp, CMTime presentationDuration)
{
    WebCoreDecompressionSession* session = static_cast<WebCoreDecompressionSession*>(decompressionOutputRefCon);
    bool displaying = sourceFrameRefCon;
    session->handleDecompressionOutput(displaying, status, infoFlags, imageBuffer, presentationTimeStamp, presentationDuration);
}

void WebCoreDecompressionSession::handleDecompressionOutput(bool displaying, OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef rawImageBuffer, CMTime presentationTimeStamp, CMTime presentationDuration)
{
    UNUSED_PARAM(status);
    UNUSED_PARAM(infoFlags);

    CMVideoFormatDescriptionRef rawImageBufferDescription = nullptr;
    if (noErr != CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, rawImageBuffer, &rawImageBufferDescription))
        return;
    RetainPtr<CMVideoFormatDescriptionRef> imageBufferDescription = adoptCF(rawImageBufferDescription);

    CMSampleTimingInfo imageBufferTiming {
        presentationDuration,
        presentationTimeStamp,
        presentationTimeStamp,
    };

    CMSampleBufferRef rawImageSampleBuffer = nullptr;
    if (noErr != CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, rawImageBuffer, imageBufferDescription.get(), &imageBufferTiming, &rawImageSampleBuffer))
        return;
    RefPtr<WebCoreDecompressionSession> protectedThis { this };
    RetainPtr<CMSampleBufferRef> imageSampleBuffer = adoptCF(rawImageSampleBuffer);
    dispatch_async(m_enqueingQueue.get(), [protectedThis, imageSampleBuffer, displaying] {
        protectedThis->enqueueDecodedSample(imageSampleBuffer.get(), displaying);
    });
}

RetainPtr<CVPixelBufferRef> WebCoreDecompressionSession::getFirstVideoFrame()
{
    if (!m_producerQueue || CMBufferQueueIsEmpty(m_producerQueue.get()))
        return nullptr;

    RetainPtr<CMSampleBufferRef> currentSample = adoptCF((CMSampleBufferRef)CMBufferQueueDequeueAndRetain(m_producerQueue.get()));
    RetainPtr<CVPixelBufferRef> imageBuffer = (CVPixelBufferRef)CMSampleBufferGetImageBuffer(currentSample.get());
    ASSERT(CFGetTypeID(imageBuffer.get()) == CVPixelBufferGetTypeID());
    return imageBuffer;
}

void WebCoreDecompressionSession::automaticDequeue()
{
    if (!m_timebase)
        return;

    auto time = toMediaTime(CMTimebaseGetTime(m_timebase.get()));
    LOG(Media, "WebCoreDecompressionSession::automaticDequeue(%p) - purging all samples before time(%s)", this, toString(time).utf8().data());

    while (CMSampleBufferRef firstSample = (CMSampleBufferRef)CMBufferQueueGetHead(m_producerQueue.get())) {
        MediaTime presentationTimestamp = toMediaTime(CMSampleBufferGetPresentationTimeStamp(firstSample));
        MediaTime duration = toMediaTime(CMSampleBufferGetDuration(firstSample));
        MediaTime presentationEndTimestamp = presentationTimestamp + duration;
        if (time > presentationEndTimestamp) {
            CFRelease(CMBufferQueueDequeueAndRetain(m_producerQueue.get()));
            continue;
        }

#if !LOG_DISABLED
        auto begin = toMediaTime(CMBufferQueueGetFirstPresentationTimeStamp(m_producerQueue.get()));
        auto end = toMediaTime(CMBufferQueueGetEndPresentationTimeStamp(m_producerQueue.get()));
        LOG(Media, "WebCoreDecompressionSession::automaticDequeue(%p) - queue(%s -> %s)", this, toString(begin).utf8().data(), toString(end).utf8().data());
#endif
        CMTimebaseSetTimerDispatchSourceNextFireTime(m_timebase.get(), m_timerSource.get(), toCMTime(presentationEndTimestamp), 0);
        return;
    }

    LOG(Media, "WebCoreDecompressionSession::automaticDequeue(%p) - queue empty", this, toString(time).utf8().data());
    CMTimebaseSetTimerDispatchSourceNextFireTime(m_timebase.get(), m_timerSource.get(), kCMTimePositiveInfinity, 0);
}

void WebCoreDecompressionSession::enqueueDecodedSample(CMSampleBufferRef sample, bool displaying)
{
    if (isInvalidated())
        return;

    --m_framesBeingDecoded;

    if (!displaying) {
        maybeBecomeReadyForMoreMediaData();
        return;
    }

    CMBufferQueueEnqueue(m_producerQueue.get(), sample);

#if !LOG_DISABLED
    auto begin = toMediaTime(CMBufferQueueGetFirstPresentationTimeStamp(m_producerQueue.get()));
    auto end = toMediaTime(CMBufferQueueGetEndPresentationTimeStamp(m_producerQueue.get()));
    auto presentationTime = toMediaTime(CMSampleBufferGetPresentationTimeStamp(sample));
    LOG(Media, "WebCoreDecompressionSession::enqueueDecodedSample(%p) - presentationTime(%s), framesBeingDecoded(%d), producerQueue(%s -> %s)", this, toString(presentationTime).utf8().data(), m_framesBeingDecoded, toString(begin).utf8().data(), toString(end).utf8().data());
#endif

    if (m_timebase)
        CMTimebaseSetTimerDispatchSourceToFireImmediately(m_timebase.get(), m_timerSource.get());

    if (m_hasAvailableFrameCallback) {
        std::function<void()> callback { m_hasAvailableFrameCallback };
        m_hasAvailableFrameCallback = nullptr;
        RefPtr<WebCoreDecompressionSession> protectedThis { this };
        dispatch_async(dispatch_get_main_queue(), [protectedThis, callback] {
            callback();
        });
    }
}

bool WebCoreDecompressionSession::isReadyForMoreMediaData() const
{
    CMItemCount producerCount = m_producerQueue ? CMBufferQueueGetBufferCount(m_producerQueue.get()) : 0;
    return m_framesBeingDecoded + producerCount <= kHighWaterMark;
}

void WebCoreDecompressionSession::requestMediaDataWhenReady(std::function<void()> notificationCallback)
{
    LOG(Media, "WebCoreDecompressionSession::requestMediaDataWhenReady(%p), hasNotificationCallback(%d)", this, !!notificationCallback);
    m_notificationCallback = notificationCallback;

    if (notificationCallback && isReadyForMoreMediaData()) {
        RefPtr<WebCoreDecompressionSession> protectedThis { this };
        dispatch_async(dispatch_get_main_queue(), [protectedThis] {
            if (protectedThis->m_notificationCallback)
                protectedThis->m_notificationCallback();
        });
    }
}

void WebCoreDecompressionSession::stopRequestingMediaData()
{
    LOG(Media, "WebCoreDecompressionSession::stopRequestingMediaData(%p)", this);
    m_notificationCallback = nullptr;
}

void WebCoreDecompressionSession::notifyWhenHasAvailableVideoFrame(std::function<void()> callback)
{
    if (callback && m_producerQueue && !CMBufferQueueIsEmpty(m_producerQueue.get())) {
        dispatch_async(dispatch_get_main_queue(), [callback] {
            callback();
        });
        return;
    }
    m_hasAvailableFrameCallback = callback;
}

RetainPtr<CVPixelBufferRef> WebCoreDecompressionSession::imageForTime(const MediaTime& time, ImageForTimeFlags flags)
{
    if (CMBufferQueueIsEmpty(m_producerQueue.get())) {
        LOG(Media, "WebCoreDecompressionSession::imageForTime(%p) - time(%s), queue empty", this, toString(time).utf8().data());
        return nullptr;
    }

    bool allowEarlier = flags == WebCoreDecompressionSession::AllowEarlier;
    bool allowLater = flags == WebCoreDecompressionSession::AllowLater;

    MediaTime startTime = toMediaTime(CMBufferQueueGetFirstPresentationTimeStamp(m_producerQueue.get()));
    MediaTime endTime = toMediaTime(CMBufferQueueGetEndPresentationTimeStamp(m_producerQueue.get()));
    if (!allowLater && time < startTime) {
        LOG(Media, "WebCoreDecompressionSession::imageForTime(%p) - time(%s) too early for queue(%s -> %s)", this, toString(time).utf8().data(), toString(startTime).utf8().data(), toString(endTime).utf8().data());
        return nullptr;
    }

    while (CMSampleBufferRef firstSample = (CMSampleBufferRef)CMBufferQueueGetHead(m_producerQueue.get())) {
        MediaTime presentationTimestamp = toMediaTime(CMSampleBufferGetPresentationTimeStamp(firstSample));
        MediaTime duration = toMediaTime(CMSampleBufferGetDuration(firstSample));
        MediaTime presentationEndTimestamp = presentationTimestamp + duration;
        if (!allowLater && presentationTimestamp > time)
            return nullptr;
        if (!allowEarlier && presentationEndTimestamp < time) {
            CFRelease(CMBufferQueueDequeueAndRetain(m_producerQueue.get()));
            continue;
        }

        RetainPtr<CMSampleBufferRef> currentSample = adoptCF((CMSampleBufferRef)CMBufferQueueDequeueAndRetain(m_producerQueue.get()));
        RetainPtr<CVPixelBufferRef> imageBuffer = (CVPixelBufferRef)CMSampleBufferGetImageBuffer(currentSample.get());
        ASSERT(CFGetTypeID(imageBuffer.get()) == CVPixelBufferGetTypeID());

        if (m_timebase)
            CMTimebaseSetTimerDispatchSourceToFireImmediately(m_timebase.get(), m_timerSource.get());

        LOG(Media, "WebCoreDecompressionSession::imageForTime(%p) - found sample for time(%s) in queue(%s -> %s)", this, toString(time).utf8().data(), toString(startTime).utf8().data(), toString(endTime).utf8().data());
        return imageBuffer;
    }

    if (m_timebase)
        CMTimebaseSetTimerDispatchSourceToFireImmediately(m_timebase.get(), m_timerSource.get());

    LOG(Media, "WebCoreDecompressionSession::imageForTime(%p) - no matching sample for time(%s) in queue(%s -> %s)", this, toString(time).utf8().data(), toString(startTime).utf8().data(), toString(endTime).utf8().data());
    return nullptr;
}

void WebCoreDecompressionSession::flush()
{
    dispatch_sync(m_decompressionQueue.get(), [protectedThis = RefPtr<WebCoreDecompressionSession>(this)] {
        CMBufferQueueReset(protectedThis->m_producerQueue.get());
        dispatch_sync(protectedThis->m_enqueingQueue.get(), [protectedThis] {
            CMBufferQueueReset(protectedThis->m_consumerQueue.get());
        });
    });
}

CMTime WebCoreDecompressionSession::getDecodeTime(CMBufferRef buf, void*)
{
    ASSERT(CFGetTypeID(buf) == CMSampleBufferGetTypeID());
    CMSampleBufferRef sample = (CMSampleBufferRef)(buf);
    return CMSampleBufferGetDecodeTimeStamp(sample);
}

CMTime WebCoreDecompressionSession::getPresentationTime(CMBufferRef buf, void*)
{
    ASSERT(CFGetTypeID(buf) == CMSampleBufferGetTypeID());
    CMSampleBufferRef sample = (CMSampleBufferRef)(buf);
    return CMSampleBufferGetPresentationTimeStamp(sample);
}

CMTime WebCoreDecompressionSession::getDuration(CMBufferRef buf, void*)
{
    ASSERT(CFGetTypeID(buf) == CMSampleBufferGetTypeID());
    CMSampleBufferRef sample = (CMSampleBufferRef)(buf);
    return CMSampleBufferGetDuration(sample);
}

CFComparisonResult WebCoreDecompressionSession::compareBuffers(CMBufferRef buf1, CMBufferRef buf2, void* refcon)
{
    return (CFComparisonResult)CMTimeCompare(getPresentationTime(buf1, refcon), getPresentationTime(buf2, refcon));
}

}

#endif
