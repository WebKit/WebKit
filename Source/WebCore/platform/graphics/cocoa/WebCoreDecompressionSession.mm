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
#import "PixelBufferConformerCV.h"
#import <CoreMedia/CMBufferQueue.h>
#import <CoreMedia/CMFormatDescription.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/cf/CoreMediaSoftLink.h>
#import <wtf/MainThread.h>
#import <wtf/MediaTime.h>
#import <wtf/MonotonicTime.h>
#import <wtf/StringPrintStream.h>
#import <wtf/Vector.h>
#import <wtf/WTFSemaphore.h>
#import <wtf/cf/TypeCastsCF.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"

using namespace PAL;

WTF_DECLARE_CF_TYPE_TRAIT(CMSampleBuffer);

namespace WebCore {

WebCoreDecompressionSession::WebCoreDecompressionSession(Mode mode)
    : m_mode(mode)
    , m_decompressionQueue(adoptOSObject(dispatch_queue_create("WebCoreDecompressionSession Decompression Queue", DISPATCH_QUEUE_SERIAL)))
    , m_enqueingQueue(adoptOSObject(dispatch_queue_create("WebCoreDecompressionSession Enqueueing Queue", DISPATCH_QUEUE_SERIAL)))
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
            dispatch_activate(m_timerSource.get());
        }
        CMTimebaseAddTimerDispatchSource(m_timebase.get(), m_timerSource.get());
    }
}

void WebCoreDecompressionSession::maybeBecomeReadyForMoreMediaData()
{
    if (!isReadyForMoreMediaData() || !m_notificationCallback)
        return;

    LOG(Media, "WebCoreDecompressionSession::maybeBecomeReadyForMoreMediaData(%p) - isReadyForMoreMediaData(%d), hasCallback(%d)", this, isReadyForMoreMediaData(), !!m_notificationCallback);

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

bool WebCoreDecompressionSession::shouldDecodeSample(CMSampleBufferRef sample, bool displaying)
{
    if (!displaying)
        return true;

    if (!m_timebase)
        return true;

    auto currentTime = CMTimebaseGetTime(m_timebase.get());
    auto presentationStartTime = CMSampleBufferGetPresentationTimeStamp(sample);
    auto duration = CMSampleBufferGetDuration(sample);
    auto presentationEndTime = CMTimeAdd(presentationStartTime, duration);
    if (CMTimeCompare(presentationEndTime, currentTime) >= 0)
        return true;

    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sample, false);
    if (!attachments)
        return true;

    for (CFIndex index = 0, count = CFArrayGetCount(attachments); index < count; ++index) {
        CFDictionaryRef attachmentDict = (CFDictionaryRef)CFArrayGetValueAtIndex(attachments, index);
        CFBooleanRef dependedOn = (CFBooleanRef)CFDictionaryGetValue(attachmentDict, kCMSampleAttachmentKey_IsDependedOnByOthers);
        if (dependedOn && !CFBooleanGetValue(dependedOn))
            return false;
    }

    return true;
}

void WebCoreDecompressionSession::ensureDecompressionSessionForSample(CMSampleBufferRef sample)
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
        auto videoDecoderSpecification = @{ (__bridge NSString *)kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder: @YES };

        NSDictionary *attributes;
        if (m_mode == OpenGL) {
            attributes = nil;
        } else {
            ASSERT(m_mode == RGB);
            attributes = @{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) };
        }
        VTDecompressionSessionRef decompressionSessionOut = nullptr;
        if (noErr == VTDecompressionSessionCreate(kCFAllocatorDefault, videoFormatDescription, (__bridge CFDictionaryRef)videoDecoderSpecification, (__bridge CFDictionaryRef)attributes, nullptr, &decompressionSessionOut)) {
            m_decompressionSession = adoptCF(decompressionSessionOut);
            CFArrayRef rawSuggestedQualityOfServiceTiers = nullptr;
            VTSessionCopyProperty(decompressionSessionOut, kVTDecompressionPropertyKey_SuggestedQualityOfServiceTiers, kCFAllocatorDefault, &rawSuggestedQualityOfServiceTiers);
            m_qosTiers = adoptCF(rawSuggestedQualityOfServiceTiers);
            m_currentQosTier = 0;
            resetQosTier();
        }
    }
}

void WebCoreDecompressionSession::decodeSample(CMSampleBufferRef sample, bool displaying)
{
    if (isInvalidated())
        return;

    ensureDecompressionSessionForSample(sample);

    VTDecodeInfoFlags flags { kVTDecodeFrame_EnableTemporalProcessing };
    if (!displaying)
        flags |= kVTDecodeFrame_DoNotOutputFrame;

    if (!shouldDecodeSample(sample, displaying)) {
        ++m_totalVideoFrames;
        ++m_droppedVideoFrames;
        --m_framesBeingDecoded;
        maybeBecomeReadyForMoreMediaData();
        return;
    }

    MonotonicTime startTime = MonotonicTime::now();
    VTDecompressionSessionDecodeFrameWithOutputHandler(m_decompressionSession.get(), sample, flags, nullptr, [this, displaying, startTime](OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer, CMTime presentationTimeStamp, CMTime presentationDuration) {
        double deltaRatio = (MonotonicTime::now() - startTime).seconds() / CMTimeGetSeconds(presentationDuration);

        updateQosWithDecodeTimeStatistics(deltaRatio);
        handleDecompressionOutput(displaying, status, infoFlags, imageBuffer, presentationTimeStamp, presentationDuration);
    });
}

RetainPtr<CVPixelBufferRef> WebCoreDecompressionSession::decodeSampleSync(CMSampleBufferRef sample)
{
    if (isInvalidated())
        return nullptr;

    ensureDecompressionSessionForSample(sample);

    RetainPtr<CVPixelBufferRef> pixelBuffer;
    VTDecodeInfoFlags flags { 0 };
    WTF::Semaphore syncDecompressionOutputSemaphore { 0 };
    VTDecompressionSessionDecodeFrameWithOutputHandler(m_decompressionSession.get(), sample, flags, nullptr, [&] (OSStatus, VTDecodeInfoFlags, CVImageBufferRef imageBuffer, CMTime, CMTime) mutable {
        if (imageBuffer && CFGetTypeID(imageBuffer) == CVPixelBufferGetTypeID())
            pixelBuffer = (CVPixelBufferRef)imageBuffer;
        syncDecompressionOutputSemaphore.signal();
    });
    syncDecompressionOutputSemaphore.wait();
    return pixelBuffer;
}

void WebCoreDecompressionSession::handleDecompressionOutput(bool displaying, OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef rawImageBuffer, CMTime presentationTimeStamp, CMTime presentationDuration)
{
    ++m_totalVideoFrames;
    if (infoFlags & kVTDecodeInfo_FrameDropped)
        ++m_droppedVideoFrames;

    CMVideoFormatDescriptionRef rawImageBufferDescription = nullptr;
    if (status != noErr || noErr != CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, rawImageBuffer, &rawImageBufferDescription)) {
        ++m_corruptedVideoFrames;
        --m_framesBeingDecoded;
        maybeBecomeReadyForMoreMediaData();
        return;
    }
    RetainPtr<CMVideoFormatDescriptionRef> imageBufferDescription = adoptCF(rawImageBufferDescription);

    CMSampleTimingInfo imageBufferTiming {
        presentationDuration,
        presentationTimeStamp,
        presentationTimeStamp,
    };

    CMSampleBufferRef rawImageSampleBuffer = nullptr;
    if (noErr != CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, rawImageBuffer, imageBufferDescription.get(), &imageBufferTiming, &rawImageSampleBuffer)) {
        ++m_corruptedVideoFrames;
        --m_framesBeingDecoded;
        maybeBecomeReadyForMoreMediaData();
        return;
    }

    dispatch_async(m_enqueingQueue.get(), [protectedThis = makeRefPtr(this), imageSampleBuffer = adoptCF(rawImageSampleBuffer), displaying] {
        protectedThis->enqueueDecodedSample(imageSampleBuffer.get(), displaying);
    });
}

RetainPtr<CVPixelBufferRef> WebCoreDecompressionSession::getFirstVideoFrame()
{
    if (!m_producerQueue || CMBufferQueueIsEmpty(m_producerQueue.get()))
        return nullptr;

    RetainPtr<CMSampleBufferRef> currentSample = adoptCF(checked_cf_cast<CMSampleBufferRef>(CMBufferQueueDequeueAndRetain(m_producerQueue.get())));
    RetainPtr<CVPixelBufferRef> imageBuffer = (CVPixelBufferRef)CMSampleBufferGetImageBuffer(currentSample.get());
    ASSERT(CFGetTypeID(imageBuffer.get()) == CVPixelBufferGetTypeID());

    maybeBecomeReadyForMoreMediaData();

    return imageBuffer;
}

void WebCoreDecompressionSession::automaticDequeue()
{
    if (!m_timebase)
        return;

    auto time = PAL::toMediaTime(CMTimebaseGetTime(m_timebase.get()));
    LOG(Media, "WebCoreDecompressionSession::automaticDequeue(%p) - purging all samples before time(%s)", this, toString(time).utf8().data());

    MediaTime nextFireTime = MediaTime::positiveInfiniteTime();
    bool releasedImageBuffers = false;

    while (CMSampleBufferRef firstSample = checked_cf_cast<CMSampleBufferRef>(CMBufferQueueGetHead(m_producerQueue.get()))) {
        MediaTime presentationTimestamp = PAL::toMediaTime(CMSampleBufferGetPresentationTimeStamp(firstSample));
        MediaTime duration = PAL::toMediaTime(CMSampleBufferGetDuration(firstSample));
        MediaTime presentationEndTimestamp = presentationTimestamp + duration;
        if (time > presentationEndTimestamp) {
            CFRelease(CMBufferQueueDequeueAndRetain(m_producerQueue.get()));
            releasedImageBuffers = true;
            continue;
        }

#if !LOG_DISABLED
        auto begin = PAL::toMediaTime(CMBufferQueueGetFirstPresentationTimeStamp(m_producerQueue.get()));
        auto end = PAL::toMediaTime(CMBufferQueueGetEndPresentationTimeStamp(m_producerQueue.get()));
        LOG(Media, "WebCoreDecompressionSession::automaticDequeue(%p) - queue(%s -> %s)", this, toString(begin).utf8().data(), toString(end).utf8().data());
#endif

        nextFireTime = presentationEndTimestamp;
        break;
    }

    if (releasedImageBuffers)
        maybeBecomeReadyForMoreMediaData();

    LOG(Media, "WebCoreDecompressionSession::automaticDequeue(%p) - queue empty", this);
    CMTimebaseSetTimerDispatchSourceNextFireTime(m_timebase.get(), m_timerSource.get(), PAL::toCMTime(nextFireTime), 0);
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

    bool shouldNotify = true;

    if (displaying && m_timebase) {
        auto currentRate = CMTimebaseGetRate(m_timebase.get());
        auto currentTime = PAL::toMediaTime(CMTimebaseGetTime(m_timebase.get()));
        auto presentationStartTime = PAL::toMediaTime(CMSampleBufferGetPresentationTimeStamp(sample));
        auto presentationEndTime = presentationStartTime + PAL::toMediaTime(CMSampleBufferGetDuration(sample));
        if (currentTime < presentationStartTime || currentTime >= presentationEndTime)
            shouldNotify = false;

        if (currentRate > 0 && presentationEndTime < currentTime) {
#if !LOG_DISABLED
            auto begin = PAL::toMediaTime(CMBufferQueueGetFirstPresentationTimeStamp(m_producerQueue.get()));
            auto end = PAL::toMediaTime(CMBufferQueueGetEndPresentationTimeStamp(m_producerQueue.get()));
            LOG(Media, "WebCoreDecompressionSession::enqueueDecodedSample(%p) - dropping frame late by %s, framesBeingDecoded(%d), producerQueue(%s -> %s)", this, toString(presentationEndTime - currentTime).utf8().data(), m_framesBeingDecoded, toString(begin).utf8().data(), toString(end).utf8().data());
#endif
            ++m_droppedVideoFrames;
            return;
        }
    }

    CMBufferQueueEnqueue(m_producerQueue.get(), sample);

#if !LOG_DISABLED
    auto begin = PAL::toMediaTime(CMBufferQueueGetFirstPresentationTimeStamp(m_producerQueue.get()));
    auto end = PAL::toMediaTime(CMBufferQueueGetEndPresentationTimeStamp(m_producerQueue.get()));
    auto presentationTime = PAL::toMediaTime(CMSampleBufferGetPresentationTimeStamp(sample));
    LOG(Media, "WebCoreDecompressionSession::enqueueDecodedSample(%p) - presentationTime(%s), framesBeingDecoded(%d), producerQueue(%s -> %s)", this, toString(presentationTime).utf8().data(), m_framesBeingDecoded, toString(begin).utf8().data(), toString(end).utf8().data());
#endif

    if (m_timebase)
        CMTimebaseSetTimerDispatchSourceToFireImmediately(m_timebase.get(), m_timerSource.get());

    if (!m_hasAvailableFrameCallback)
        return;

    if (!shouldNotify)
        return;

    dispatch_async(dispatch_get_main_queue(), [protectedThis = makeRefPtr(this), callback = WTFMove(m_hasAvailableFrameCallback)] {
        callback();
    });
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

    MediaTime startTime = PAL::toMediaTime(CMBufferQueueGetFirstPresentationTimeStamp(m_producerQueue.get()));
    MediaTime endTime = PAL::toMediaTime(CMBufferQueueGetEndPresentationTimeStamp(m_producerQueue.get()));
    if (!allowLater && time < startTime) {
        LOG(Media, "WebCoreDecompressionSession::imageForTime(%p) - time(%s) too early for queue(%s -> %s)", this, toString(time).utf8().data(), toString(startTime).utf8().data(), toString(endTime).utf8().data());
        return nullptr;
    }

    bool releasedImageBuffers = false;

    while (CMSampleBufferRef firstSample = checked_cf_cast<CMSampleBufferRef>(CMBufferQueueGetHead(m_producerQueue.get()))) {
        MediaTime presentationTimestamp = PAL::toMediaTime(CMSampleBufferGetPresentationTimeStamp(firstSample));
        MediaTime duration = PAL::toMediaTime(CMSampleBufferGetDuration(firstSample));
        MediaTime presentationEndTimestamp = presentationTimestamp + duration;
        if (!allowLater && presentationTimestamp > time)
            return nullptr;
        if (!allowEarlier && presentationEndTimestamp < time) {
            CFRelease(CMBufferQueueDequeueAndRetain(m_producerQueue.get()));
            releasedImageBuffers = true;
            continue;
        }

        RetainPtr<CMSampleBufferRef> currentSample = adoptCF(checked_cf_cast<CMSampleBufferRef>(CMBufferQueueDequeueAndRetain(m_producerQueue.get())));
        RetainPtr<CVPixelBufferRef> imageBuffer = (CVPixelBufferRef)CMSampleBufferGetImageBuffer(currentSample.get());
        ASSERT(CFGetTypeID(imageBuffer.get()) == CVPixelBufferGetTypeID());

        if (m_timebase)
            CMTimebaseSetTimerDispatchSourceToFireImmediately(m_timebase.get(), m_timerSource.get());

        maybeBecomeReadyForMoreMediaData();

        LOG(Media, "WebCoreDecompressionSession::imageForTime(%p) - found sample for time(%s) in queue(%s -> %s)", this, toString(time).utf8().data(), toString(startTime).utf8().data(), toString(endTime).utf8().data());
        return imageBuffer;
    }

    if (m_timebase)
        CMTimebaseSetTimerDispatchSourceToFireImmediately(m_timebase.get(), m_timerSource.get());

    if (releasedImageBuffers)
        maybeBecomeReadyForMoreMediaData();

    LOG(Media, "WebCoreDecompressionSession::imageForTime(%p) - no matching sample for time(%s) in queue(%s -> %s)", this, toString(time).utf8().data(), toString(startTime).utf8().data(), toString(endTime).utf8().data());
    return nullptr;
}

void WebCoreDecompressionSession::flush()
{
    dispatch_sync(m_decompressionQueue.get(), [protectedThis = RefPtr<WebCoreDecompressionSession>(this)] {
        CMBufferQueueReset(protectedThis->m_producerQueue.get());
        dispatch_sync(protectedThis->m_enqueingQueue.get(), [protectedThis] {
            CMBufferQueueReset(protectedThis->m_consumerQueue.get());
            protectedThis->m_framesSinceLastQosCheck = 0;
            protectedThis->m_currentQosTier = 0;
            protectedThis->resetQosTier();
        });
    });
}

CMTime WebCoreDecompressionSession::getDecodeTime(CMBufferRef buf, void*)
{
    CMSampleBufferRef sample = checked_cf_cast<CMSampleBufferRef>(buf);
    return CMSampleBufferGetDecodeTimeStamp(sample);
}

CMTime WebCoreDecompressionSession::getPresentationTime(CMBufferRef buf, void*)
{
    CMSampleBufferRef sample = checked_cf_cast<CMSampleBufferRef>(buf);
    return CMSampleBufferGetPresentationTimeStamp(sample);
}

CMTime WebCoreDecompressionSession::getDuration(CMBufferRef buf, void*)
{
    CMSampleBufferRef sample = checked_cf_cast<CMSampleBufferRef>(buf);
    return CMSampleBufferGetDuration(sample);
}

CFComparisonResult WebCoreDecompressionSession::compareBuffers(CMBufferRef buf1, CMBufferRef buf2, void* refcon)
{
    return (CFComparisonResult)CMTimeCompare(getPresentationTime(buf1, refcon), getPresentationTime(buf2, refcon));
}

void WebCoreDecompressionSession::resetQosTier()
{
    if (!m_qosTiers || !m_decompressionSession)
        return;

    if (m_currentQosTier < 0 || m_currentQosTier >= CFArrayGetCount(m_qosTiers.get()))
        return;

    auto tier = (CFDictionaryRef)CFArrayGetValueAtIndex(m_qosTiers.get(), m_currentQosTier);
    LOG(Media, "WebCoreDecompressionSession::resetQosTier(%p) - currentQosTier(%ld), tier(%@)", this, m_currentQosTier, [(__bridge NSDictionary *)tier description]);

    VTSessionSetProperties(m_decompressionSession.get(), tier);
    m_framesSinceLastQosCheck = 0;
}

void WebCoreDecompressionSession::increaseQosTier()
{
    if (!m_qosTiers)
        return;

    if (m_currentQosTier + 1 >= CFArrayGetCount(m_qosTiers.get()))
        return;

    ++m_currentQosTier;
    resetQosTier();
}

void WebCoreDecompressionSession::decreaseQosTier()
{
    if (!m_qosTiers)
        return;

    if (m_currentQosTier <= 0)
        return;

    --m_currentQosTier;
    resetQosTier();
}

void WebCoreDecompressionSession::updateQosWithDecodeTimeStatistics(double ratio)
{
    static const double kMovingAverageAlphaValue = 0.1;
    static const unsigned kNumberOfFramesBeforeSwitchingTiers = 60;
    static const double kHighWaterDecodeRatio = 1.;
    static const double kLowWaterDecodeRatio = 0.5;

    if (!m_timebase)
        return;

    double rate = CMTimebaseGetRate(m_timebase.get());
    if (!rate)
        rate = 1;

    m_decodeRatioMovingAverage += kMovingAverageAlphaValue * (ratio - m_decodeRatioMovingAverage) * rate;
    if (++m_framesSinceLastQosCheck < kNumberOfFramesBeforeSwitchingTiers)
        return;

    LOG(Media, "WebCoreDecompressionSession::updateQosWithDecodeTimeStatistics(%p) - framesSinceLastQosCheck(%ld), decodeRatioMovingAverage(%g)", this, m_framesSinceLastQosCheck, m_decodeRatioMovingAverage);
    if (m_decodeRatioMovingAverage > kHighWaterDecodeRatio)
        increaseQosTier();
    else if (m_decodeRatioMovingAverage < kLowWaterDecodeRatio)
        decreaseQosTier();
    m_framesSinceLastQosCheck = 0;
}

}

#endif
