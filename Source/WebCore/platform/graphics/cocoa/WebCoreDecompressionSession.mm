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

#import "Logging.h"
#import "PixelBufferConformerCV.h"
#import "VideoDecoder.h"
#import "VideoFrame.h"
#import <CoreMedia/CMBufferQueue.h>
#import <CoreMedia/CMFormatDescription.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <wtf/MainThread.h>
#import <wtf/MediaTime.h>
#import <wtf/NativePromise.h>
#import <wtf/RunLoop.h>
#import <wtf/StringPrintStream.h>
#import <wtf/Vector.h>
#import <wtf/WTFSemaphore.h>
#import <wtf/cf/TypeCastsCF.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

// Equivalent to WTF_DECLARE_CF_TYPE_TRAIT(CMSampleBuffer);
// Needed due to requirement of specifying the PAL namespace.
template <>
struct WTF::CFTypeTrait<CMSampleBufferRef> {
    static inline CFTypeID typeID(void) { return PAL::CMSampleBufferGetTypeID(); }
};

namespace WebCore {

WebCoreDecompressionSession::WebCoreDecompressionSession(Mode mode)
    : m_mode(mode)
    , m_decompressionQueue(WorkQueue::create("WebCoreDecompressionSession Decompression Queue"))
    , m_enqueingQueue(WorkQueue::create("WebCoreDecompressionSession Enqueueing Queue"))
{
}

WebCoreDecompressionSession::~WebCoreDecompressionSession() = default;

void WebCoreDecompressionSession::invalidate()
{
    assertIsMainThread();
    m_invalidated = true;
    Locker lock { m_lock };
    m_notificationCallback = nullptr;
    m_hasAvailableFrameCallback = nullptr;
    setTimebaseWithLockHeld(nullptr);
    if (m_timerSource) {
        dispatch_source_cancel(m_timerSource.get());
        m_timerSource = nullptr;
    }
    m_decompressionQueue->dispatch([decoder = WTFMove(m_videoDecoder)] {
        if (decoder)
            decoder->close();
    });
    removeErrorListener();
}

void WebCoreDecompressionSession::setTimebase(CMTimebaseRef timebase)
{
    Locker lock { m_lock };
    setTimebaseWithLockHeld(timebase);
}

void WebCoreDecompressionSession::setTimebaseWithLockHeld(CMTimebaseRef timebase)
{
    assertIsHeld(m_lock);

    if (m_timebase == timebase)
        return;

    if (m_timebase)
        PAL::CMTimebaseRemoveTimerDispatchSource(m_timebase.get(), m_timerSource.get());

    m_timebase = timebase;

    if (m_timebase) {
        if (!m_timerSource) {
            m_timerSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue()));
            dispatch_source_set_event_handler(m_timerSource.get(), [this] {
                automaticDequeue();
            });
            dispatch_activate(m_timerSource.get());
        }
        PAL::CMTimebaseAddTimerDispatchSource(m_timebase.get(), m_timerSource.get());
    }
}

RetainPtr<CMTimebaseRef> WebCoreDecompressionSession::timebase() const
{
    Locker lock { m_lock };
    return m_timebase;
}

void WebCoreDecompressionSession::maybeBecomeReadyForMoreMediaData()
{
    if (!isReadyForMoreMediaData())
        return;

    ensureOnMainThread([protectedThis = Ref { *this }, this] {
        assertIsMainThread();
        LOG(Media, "WebCoreDecompressionSession::maybeBecomeReadyForMoreMediaData(%p) - isReadyForMoreMediaData(1), hasCallback(%d)", this, !!m_notificationCallback);
        if (m_notificationCallback)
            m_notificationCallback();
    });
}

void WebCoreDecompressionSession::enqueueSample(CMSampleBufferRef sampleBuffer, bool displaying)
{
    CMItemCount itemCount = 0;
    if (noErr != PAL::CMSampleBufferGetSampleTimingInfoArray(sampleBuffer, 0, nullptr, &itemCount))
        return;

    Vector<CMSampleTimingInfo> timingInfoArray;
    timingInfoArray.grow(itemCount);
    if (noErr != PAL::CMSampleBufferGetSampleTimingInfoArray(sampleBuffer, itemCount, timingInfoArray.data(), nullptr))
        return;

    // CMBufferCallbacks contains 64-bit pointers that aren't 8-byte aligned. To suppress the linker
    // warning about this, we prepend 4 bytes of padding when building.
    const size_t padSize = 4;

    if (!m_producerQueue) {
        CMBufferQueueRef outQueue { nullptr };
#pragma pack(push, 4)
        struct BufferCallbacks { uint8_t pad[padSize]; CMBufferCallbacks callbacks; } callbacks { { }, {
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
        static_assert(sizeof(callbacks.callbacks.version) == sizeof(uint32_t), "Version field must be 4 bytes");
        static_assert(alignof(BufferCallbacks) == 4, "CMBufferCallbacks struct must have 4 byte alignment");

        PAL::CMBufferQueueCreate(kCFAllocatorDefault, kMaximumCapacity, &callbacks.callbacks, &outQueue);
        m_producerQueue = adoptCF(outQueue);
    }

    if (!m_consumerQueue) {
        CMBufferQueueRef outQueue { nullptr };
#pragma pack(push, 4)
        struct BufferCallbacks { uint8_t pad[padSize]; CMBufferCallbacks callbacks; } callbacks { { }, {
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
        static_assert(sizeof(callbacks.callbacks.version) == sizeof(uint32_t), "Version field must be 4 bytes");
        static_assert(alignof(BufferCallbacks) == 4, "CMBufferCallbacks struct alignment must be 4");

        PAL::CMBufferQueueCreate(kCFAllocatorDefault, kMaximumCapacity, &callbacks.callbacks, &outQueue);
        m_consumerQueue = adoptCF(outQueue);
    }

    ++m_framesBeingDecoded;

    LOG(Media, "WebCoreDecompressionSession::enqueueSample(%p) - framesBeingDecoded(%d)", this, int(m_framesBeingDecoded));

    m_decompressionQueue->dispatch([protectedThis = Ref { *this }, strongBuffer = retainPtr(sampleBuffer), displaying] {
        protectedThis->enqueueCompressedSample(strongBuffer.get(), displaying);
    });
}

bool WebCoreDecompressionSession::shouldDecodeSample(CMSampleBufferRef sample, bool displaying)
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

RetainPtr<VTDecompressionSessionRef> WebCoreDecompressionSession::ensureDecompressionSessionForSample(CMSampleBufferRef sample)
{
    Locker lock { m_lock };
    if (isInvalidated() || m_videoDecoder)
        return nullptr;

    CMVideoFormatDescriptionRef videoFormatDescription = PAL::CMSampleBufferGetFormatDescription(sample);
    if (m_decompressionSession && !VTDecompressionSessionCanAcceptFormatDescription(m_decompressionSession.get(), videoFormatDescription)) {
        VTDecompressionSessionWaitForAsynchronousFrames(m_decompressionSession.get());
        m_decompressionSession = nullptr;
    }

    if (!m_decompressionSession) {
        auto videoDecoderSpecification = @{ (__bridge NSString *)kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder: @( bool(m_hardwareDecoderEnabled) ) };

        NSDictionary *attributes;
        if (m_mode == OpenGL)
            attributes = @{ (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey: @{ /* empty dictionary */ } };
        else {
            attributes = @{
                (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
                (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey: @{ /* empty dictionary */ }
            };
        }

        VTDecompressionSessionRef decompressionSessionOut = nullptr;
        auto result = VTDecompressionSessionCreate(kCFAllocatorDefault, videoFormatDescription, (__bridge CFDictionaryRef)videoDecoderSpecification, (__bridge CFDictionaryRef)attributes, nullptr, &decompressionSessionOut);
        if (noErr == result) {
            m_decompressionSession = adoptCF(decompressionSessionOut);
            CFArrayRef rawSuggestedQualityOfServiceTiers = nullptr;
            VTSessionCopyProperty(decompressionSessionOut, kVTDecompressionPropertyKey_SuggestedQualityOfServiceTiers, kCFAllocatorDefault, &rawSuggestedQualityOfServiceTiers);
            m_qosTiers = adoptCF(rawSuggestedQualityOfServiceTiers);
            m_currentQosTier = 0;
            resetQosTier();
            // Decoder creation succeeded, we'll never fallback to using a VideoDecoder.
            m_isUsingVideoDecoder = false;
        }
    }

    return m_decompressionSession;
}

void WebCoreDecompressionSession::enqueueCompressedSample(CMSampleBufferRef sample, bool displaying)
{
    assertIsCurrent(m_decompressionQueue.get());

    m_pendingSamples.append(std::make_pair(sample, displaying));

    maybeDecodeNextSample();
}

void WebCoreDecompressionSession::maybeDecodeNextSample()
{
    assertIsCurrent(m_decompressionQueue.get());

    if (m_pendingSamples.isEmpty() || m_isDecodingSample)
        return;

    auto pair = m_pendingSamples.takeFirst();
    decodeSample(pair.first.get(), pair.second);
}

void WebCoreDecompressionSession::decodeSample(CMSampleBufferRef sample, bool displaying)
{
    assertIsCurrent(m_decompressionQueue.get());

    m_isDecodingSample = true;

    VTDecodeInfoFlags flags { kVTDecodeFrame_EnableTemporalProcessing };
    if (!displaying)
        flags |= kVTDecodeFrame_DoNotOutputFrame;

    if (!shouldDecodeSample(sample, displaying)) {
        ++m_totalVideoFrames;
        ++m_droppedVideoFrames;
        finishCurrentDecodingAndDecodeNextSample();
        return;
    }

    RetainPtr decompressionSession = ensureDecompressionSessionForSample(sample);
    if (!decompressionSession && !m_videoDecoderCreationFailed) {
        Locker lock { m_lock };
        if (!m_videoDecoder) {
            if (isInvalidated())
                return;
            CMVideoFormatDescriptionRef videoFormatDescription = PAL::CMSampleBufferGetFormatDescription(sample);
            auto fourCC = PAL::CMFormatDescriptionGetMediaSubType(videoFormatDescription);
            initializeVideoDecoder(fourCC)->whenSettled(m_decompressionQueue, [protectedThis = Ref { *this }, this, sample = RetainPtr { sample }, displaying](auto&& result) {
                if (!result) {
                    m_videoDecoderCreationFailed = true;
                    finishCurrentDecodingAndReportError(kVTVideoDecoderNotAvailableNowErr);
                    return;
                }
                decodeSample(sample.get(), displaying);
            });
            return;
        }
        MediaTime presentationTimestamp = PAL::toMediaTime(PAL::CMSampleBufferGetPresentationTimeStamp(sample));
        MediaTime duration = PAL::toMediaTime(PAL::CMSampleBufferGetDuration(sample));
        CMBlockBufferRef rawBuffer = PAL::CMSampleBufferGetDataBuffer(sample);
        ASSERT(rawBuffer);
        RetainPtr buffer = rawBuffer;
        // Make sure block buffer is contiguous.
        if (!PAL::CMBlockBufferIsRangeContiguous(rawBuffer, 0, 0)) {
            CMBlockBufferRef contiguousBuffer;
            if (auto status = PAL::CMBlockBufferCreateContiguous(nullptr, rawBuffer, nullptr, nullptr, 0, 0, 0, &contiguousBuffer); status != kCMBlockBufferNoErr) {
                finishCurrentDecodingAndReportError(status);
                return;
            }
            buffer = adoptCF(contiguousBuffer);
        }
        auto size = PAL::CMBlockBufferGetDataLength(buffer.get());
        char* data = nullptr;
        if (auto status = PAL::CMBlockBufferGetDataPointer(buffer.get(), 0, nullptr, nullptr, &data); status != noErr) {
            finishCurrentDecodingAndReportError(status);
            return;
        }
        m_pendingDecodeData = { MonotonicTime::now(), displaying };
        auto presentationTimeInUs = presentationTimestamp.toTimeScale(1000000);
        auto durationInUs = duration.toTimeScale(1000000);
        m_videoDecoder->decode({ { reinterpret_cast<uint8_t*>(data), size }, true, presentationTimeInUs.timeValue(), durationInUs.timeValue() }, [weakThis = ThreadSafeWeakPtr { *this }, this, duration = PAL::toCMTime(duration)](String&&) {
            if (RefPtr protectedThis = weakThis.get()) {
                assertIsCurrent(m_decompressionQueue.get());
                if (m_lastDecodingError != noErr)
                    finishCurrentDecodingAndReportError(m_lastDecodingError);
                else
                    finishCurrentDecodingAndDecodeNextSample();
                if (!m_pendingDecodeData)
                    return;
                double deltaRatio = (MonotonicTime::now() - m_pendingDecodeData->startTime).seconds() / PAL::CMTimeGetSeconds(duration);
                updateQosWithDecodeTimeStatistics(deltaRatio);
                m_pendingDecodeData.reset();
            };
        });
        return;
    }

    if (!decompressionSession)
        return;
    MonotonicTime startTime = MonotonicTime::now();
    VTDecompressionSessionDecodeFrameWithOutputHandler(decompressionSession.get(), sample, flags, nullptr, [weakThis = ThreadSafeWeakPtr { *this }, this, displaying, startTime](OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer, CMTime presentationTimeStamp, CMTime presentationDuration) {
        if (RefPtr protectedThis = weakThis.get()) {
            m_decompressionQueue->dispatch([protectedThis = WTFMove(protectedThis), this, displaying, startTime, status, infoFlags, imageBuffer = RetainPtr { imageBuffer }, presentationTimeStamp, presentationDuration]() {
                assertIsCurrent(m_decompressionQueue.get());
                double deltaRatio = (MonotonicTime::now() - startTime).seconds() / PAL::CMTimeGetSeconds(presentationDuration);

                updateQosWithDecodeTimeStatistics(deltaRatio);
                handleDecompressionOutput(displaying, status, infoFlags, imageBuffer.get(), presentationTimeStamp, presentationDuration);
                if (m_lastDecodingError != noErr)
                    finishCurrentDecodingAndReportError(m_lastDecodingError);
                else
                    finishCurrentDecodingAndDecodeNextSample();
            });
        }
    });
}

void WebCoreDecompressionSession::setErrorListener(Function<void(OSStatus)>&& listener)
{
    assertIsMainThread();
    m_errorListener = WTFMove(listener);
}

void WebCoreDecompressionSession::removeErrorListener()
{
    assertIsMainThread();
    m_errorListener = { };
}

void WebCoreDecompressionSession::finishCurrentDecodingAndReportError(OSStatus status)
{
    assertIsCurrent(m_decompressionQueue.get());

    --m_framesBeingDecoded;
    m_isDecodingSample = false;
    ensureOnMainThread([protectedThis = Ref { *this }, this, status] {
        assertIsMainThread();
        if (!m_errorListener)
            return;
        m_errorListener(status);
    });
}

void WebCoreDecompressionSession::finishCurrentDecodingAndDecodeNextSample()
{
    assertIsCurrent(m_decompressionQueue.get());

    --m_framesBeingDecoded;
    m_isDecodingSample = false;
    m_decompressionQueue->dispatch([protectedThis = Ref { *this }] {
        protectedThis->maybeDecodeNextSample();
    });
    maybeBecomeReadyForMoreMediaData();
}

RetainPtr<CVPixelBufferRef> WebCoreDecompressionSession::decodeSampleSync(CMSampleBufferRef sample)
{
    RetainPtr decompressionSession = ensureDecompressionSessionForSample(sample);
    if (!decompressionSession)
        return nullptr;

    RetainPtr<CVPixelBufferRef> pixelBuffer;
    VTDecodeInfoFlags flags { 0 };
    WTF::Semaphore syncDecompressionOutputSemaphore { 0 };
    VTDecompressionSessionDecodeFrameWithOutputHandler(decompressionSession.get(), sample, flags, nullptr, [&] (OSStatus, VTDecodeInfoFlags, CVImageBufferRef imageBuffer, CMTime, CMTime) mutable {
        if (imageBuffer && CFGetTypeID(imageBuffer) == CVPixelBufferGetTypeID())
            pixelBuffer = (CVPixelBufferRef)imageBuffer;
        syncDecompressionOutputSemaphore.signal();
    });
    syncDecompressionOutputSemaphore.wait();
    return pixelBuffer;
}

void WebCoreDecompressionSession::handleDecompressionOutput(bool displaying, OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef rawImageBuffer, CMTime presentationTimeStamp, CMTime presentationDuration)
{
    assertIsCurrent(m_decompressionQueue.get());

    ++m_totalVideoFrames;
    if (infoFlags & kVTDecodeInfo_FrameDropped) {
        ++m_droppedVideoFrames;
        return;
    }

    if (status != noErr) {
        ++m_corruptedVideoFrames;
        m_lastDecodingError = status;
        return;
    }

    if (!displaying)
        return;

    CMVideoFormatDescriptionRef rawImageBufferDescription = nullptr;
    if (auto status = PAL::CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, rawImageBuffer, &rawImageBufferDescription); status != noErr) {
        ++m_corruptedVideoFrames;
        m_lastDecodingError = status;
        return;
    }
    RetainPtr<CMVideoFormatDescriptionRef> imageBufferDescription = adoptCF(rawImageBufferDescription);

    CMSampleTimingInfo imageBufferTiming {
        presentationDuration,
        presentationTimeStamp,
        presentationTimeStamp,
    };

    CMSampleBufferRef rawImageSampleBuffer = nullptr;
    if (auto status = PAL::CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, rawImageBuffer, imageBufferDescription.get(), &imageBufferTiming, &rawImageSampleBuffer); status != noErr) {
        ++m_corruptedVideoFrames;
        m_lastDecodingError = status;
        return;
    }

    if (!m_deliverDecodedFrames) {
        m_enqueingQueue->dispatch([protectedThis = Ref { *this }, imageSampleBuffer = adoptCF(rawImageSampleBuffer)] {
            protectedThis->enqueueDecodedSample(imageSampleBuffer.get());
        });
        return;
    }

    RunLoop::main().dispatch([protectedThis = Ref { *this }, this, imageSampleBuffer = adoptCF(rawImageSampleBuffer)]() mutable {
        assertIsMainThread();
        LOG(Media, "WebCoreDecompressionSession::handleDecompressionOutput(%p) - returning frame: presentationTime(%s)", this, toString(PAL::toMediaTime(PAL::CMSampleBufferGetPresentationTimeStamp(imageSampleBuffer.get()))).utf8().data());
        if (m_newDecodedFrameCallback)
            m_newDecodedFrameCallback(WTFMove(imageSampleBuffer));
    });
}

RetainPtr<CVPixelBufferRef> WebCoreDecompressionSession::getFirstVideoFrame()
{
    if (!m_producerQueue || PAL::CMBufferQueueIsEmpty(m_producerQueue.get()))
        return nullptr;

    RetainPtr<CMSampleBufferRef> currentSample = adoptCF(checked_cf_cast<CMSampleBufferRef>(PAL::CMBufferQueueDequeueAndRetain(m_producerQueue.get())));
    RetainPtr<CVPixelBufferRef> imageBuffer = (CVPixelBufferRef)PAL::CMSampleBufferGetImageBuffer(currentSample.get());
    ASSERT(CFGetTypeID(imageBuffer.get()) == CVPixelBufferGetTypeID());

    maybeBecomeReadyForMoreMediaData();

    return imageBuffer;
}

void WebCoreDecompressionSession::automaticDequeue()
{
    assertIsMainThread();
    RetainPtr timebase = this->timebase();
    if (!timebase)
        return;

    auto time = PAL::toMediaTime(PAL::CMTimebaseGetTime(timebase.get()));
    LOG(Media, "WebCoreDecompressionSession::automaticDequeue(%p) - purging all samples before time(%s)", this, toString(time).utf8().data());

    MediaTime nextFireTime = MediaTime::positiveInfiniteTime();
    bool releasedImageBuffers = false;

    while (CMSampleBufferRef firstSample = checked_cf_cast<CMSampleBufferRef>(PAL::CMBufferQueueGetHead(m_producerQueue.get()))) {
        MediaTime presentationTimestamp = PAL::toMediaTime(PAL::CMSampleBufferGetPresentationTimeStamp(firstSample));
        MediaTime duration = PAL::toMediaTime(PAL::CMSampleBufferGetDuration(firstSample));
        MediaTime presentationEndTimestamp = presentationTimestamp + duration;
        if (time > presentationEndTimestamp) {
            CFRelease(PAL::CMBufferQueueDequeueAndRetain(m_producerQueue.get()));
            releasedImageBuffers = true;
            continue;
        }

#if !LOG_DISABLED
        auto begin = PAL::toMediaTime(PAL::CMBufferQueueGetFirstPresentationTimeStamp(m_producerQueue.get()));
        auto end = PAL::toMediaTime(PAL::CMBufferQueueGetEndPresentationTimeStamp(m_producerQueue.get()));
        LOG(Media, "WebCoreDecompressionSession::automaticDequeue(%p) - queue(%s -> %s)", this, toString(begin).utf8().data(), toString(end).utf8().data());
#endif

        nextFireTime = presentationEndTimestamp;
        break;
    }

    if (releasedImageBuffers)
        maybeBecomeReadyForMoreMediaData();

    LOG(Media, "WebCoreDecompressionSession::automaticDequeue(%p) - queue empty", this);

    Locker lock { m_lock };
    if (isInvalidated())
        return;
    PAL::CMTimebaseSetTimerDispatchSourceNextFireTime(m_timebase.get(), m_timerSource.get(), PAL::toCMTime(nextFireTime), 0);
}

void WebCoreDecompressionSession::enqueueDecodedSample(CMSampleBufferRef sample)
{
    assertIsCurrent(m_enqueingQueue);

    if (isInvalidated())
        return;

    bool shouldNotify = true;

    RetainPtr timebase = this->timebase();

    if (timebase) {
        auto currentRate = PAL::CMTimebaseGetRate(timebase.get());
        auto currentTime = PAL::toMediaTime(PAL::CMTimebaseGetTime(timebase.get()));
        auto presentationStartTime = PAL::toMediaTime(PAL::CMSampleBufferGetPresentationTimeStamp(sample));
        auto presentationEndTime = presentationStartTime + PAL::toMediaTime(PAL::CMSampleBufferGetDuration(sample));
        if (currentTime < presentationStartTime || currentTime >= presentationEndTime)
            shouldNotify = false;

        if (currentRate > 0 && presentationEndTime < currentTime) {
#if !LOG_DISABLED
            auto begin = PAL::toMediaTime(PAL::CMBufferQueueGetFirstPresentationTimeStamp(m_producerQueue.get()));
            auto end = PAL::toMediaTime(PAL::CMBufferQueueGetEndPresentationTimeStamp(m_producerQueue.get()));
            LOG(Media, "WebCoreDecompressionSession::enqueueDecodedSample(%p) - dropping frame late by %s, framesBeingDecoded(%d), producerQueue(%s -> %s)", this, toString(presentationEndTime - currentTime).utf8().data(), int(m_framesBeingDecoded), toString(begin).utf8().data(), toString(end).utf8().data());
#endif
            ++m_droppedVideoFrames;
            return;
        }
    }

    PAL::CMBufferQueueEnqueue(m_producerQueue.get(), sample);

#if !LOG_DISABLED
    auto begin = PAL::toMediaTime(PAL::CMBufferQueueGetFirstPresentationTimeStamp(m_producerQueue.get()));
    auto end = PAL::toMediaTime(PAL::CMBufferQueueGetEndPresentationTimeStamp(m_producerQueue.get()));
    auto presentationTime = PAL::toMediaTime(PAL::CMSampleBufferGetPresentationTimeStamp(sample));
    LOG(Media, "WebCoreDecompressionSession::enqueueDecodedSample(%p) - presentationTime(%s), framesBeingDecoded(%d), producerQueue(%s -> %s)", this, toString(presentationTime).utf8().data(), int(m_framesBeingDecoded), toString(begin).utf8().data(), toString(end).utf8().data());
#endif

    {
        Locker lock { m_lock };
        if (m_timebase)
            PAL::CMTimebaseSetTimerDispatchSourceToFireImmediately(m_timebase.get(), m_timerSource.get());
    }

    if (!shouldNotify)
        return;

    RunLoop::main().dispatch([protectedThis = Ref { *this }] {
        assertIsMainThread();
        if (auto callback = std::exchange(protectedThis->m_hasAvailableFrameCallback, { }))
            callback();
    });
}

bool WebCoreDecompressionSession::isReadyForMoreMediaData() const
{
    CMItemCount producerCount = m_producerQueue ? PAL::CMBufferQueueGetBufferCount(m_producerQueue.get()) : 0;
    return (!m_isUsingVideoDecoder || !m_framesBeingDecoded) && (m_framesBeingDecoded + producerCount <= kHighWaterMark);
}

void WebCoreDecompressionSession::requestMediaDataWhenReady(Function<void()>&& notificationCallback)
{
    assertIsMainThread();
    LOG(Media, "WebCoreDecompressionSession::requestMediaDataWhenReady(%p), hasNotificationCallback(%d)", this, !!notificationCallback);
    m_notificationCallback = WTFMove(notificationCallback);

    if (m_notificationCallback && isReadyForMoreMediaData()) {
        RefPtr<WebCoreDecompressionSession> protectedThis { this };
        RunLoop::main().dispatch([protectedThis] {
            assertIsMainThread();
            if (protectedThis->m_notificationCallback)
                protectedThis->m_notificationCallback();
        });
    }
}

void WebCoreDecompressionSession::decodedFrameWhenAvailable(Function<void(RetainPtr<CMSampleBufferRef>&&)>&& callback)
{
    assertIsMainThread();
    LOG(Media, "WebCoreDecompressionSession::decodedFrameWhenAvailable(%p), hasDecodedFrameWhenAvailable(%d)", this, !!callback);
    m_newDecodedFrameCallback = WTFMove(callback);

    m_deliverDecodedFrames = !!m_newDecodedFrameCallback;
}

void WebCoreDecompressionSession::stopRequestingMediaData()
{
    assertIsMainThread();
    LOG(Media, "WebCoreDecompressionSession::stopRequestingMediaData(%p)", this);
    m_notificationCallback = nullptr;
}

void WebCoreDecompressionSession::notifyWhenHasAvailableVideoFrame(Function<void()>&& callback)
{
    assertIsMainThread();
    if (m_producerQueue && !PAL::CMBufferQueueIsEmpty(m_producerQueue.get())) {
        RunLoop::main().dispatch(WTFMove(callback));
        return;
    }
    m_hasAvailableFrameCallback = WTFMove(callback);
}

RetainPtr<CVPixelBufferRef> WebCoreDecompressionSession::imageForTime(const MediaTime& time, ImageForTimeFlags flags)
{
    if (PAL::CMBufferQueueIsEmpty(m_producerQueue.get())) {
        LOG(Media, "WebCoreDecompressionSession::imageForTime(%p) - time(%s), queue empty", this, toString(time).utf8().data());
        return nullptr;
    }

    bool allowEarlier = flags == WebCoreDecompressionSession::AllowEarlier;
    bool allowLater = flags == WebCoreDecompressionSession::AllowLater;

    MediaTime startTime = PAL::toMediaTime(PAL::CMBufferQueueGetFirstPresentationTimeStamp(m_producerQueue.get()));
#if !LOG_DISABLED
    MediaTime endTime = PAL::toMediaTime(PAL::CMBufferQueueGetEndPresentationTimeStamp(m_producerQueue.get()));
#endif
    if (!allowLater && time < startTime) {
        LOG(Media, "WebCoreDecompressionSession::imageForTime(%p) - time(%s) too early for queue(%s -> %s)", this, toString(time).utf8().data(), toString(startTime).utf8().data(), toString(endTime).utf8().data());
        return nullptr;
    }

    bool releasedImageBuffers = false;

    while (CMSampleBufferRef firstSample = checked_cf_cast<CMSampleBufferRef>(PAL::CMBufferQueueGetHead(m_producerQueue.get()))) {
        MediaTime presentationTimestamp = PAL::toMediaTime(PAL::CMSampleBufferGetPresentationTimeStamp(firstSample));
        MediaTime duration = PAL::toMediaTime(PAL::CMSampleBufferGetDuration(firstSample));
        MediaTime presentationEndTimestamp = presentationTimestamp + duration;
        if (!allowLater && presentationTimestamp > time)
            return nullptr;
        if (!allowEarlier && presentationEndTimestamp < time) {
            CFRelease(PAL::CMBufferQueueDequeueAndRetain(m_producerQueue.get()));
            releasedImageBuffers = true;
            continue;
        }

        RetainPtr<CMSampleBufferRef> currentSample = adoptCF(checked_cf_cast<CMSampleBufferRef>(PAL::CMBufferQueueDequeueAndRetain(m_producerQueue.get())));
        RetainPtr<CVPixelBufferRef> imageBuffer = (CVPixelBufferRef)PAL::CMSampleBufferGetImageBuffer(currentSample.get());
        ASSERT(CFGetTypeID(imageBuffer.get()) == CVPixelBufferGetTypeID());

        {
            Locker lock { m_lock };
            if (m_timebase)
                PAL::CMTimebaseSetTimerDispatchSourceToFireImmediately(m_timebase.get(), m_timerSource.get());
        }
        maybeBecomeReadyForMoreMediaData();

        LOG(Media, "WebCoreDecompressionSession::imageForTime(%p) - found sample for time(%s) in queue(%s -> %s)", this, toString(time).utf8().data(), toString(startTime).utf8().data(), toString(endTime).utf8().data());
        return imageBuffer;
    }

    {
        Locker lock { m_lock };
        if (m_timebase)
            PAL::CMTimebaseSetTimerDispatchSourceToFireImmediately(m_timebase.get(), m_timerSource.get());
    }
    if (releasedImageBuffers)
        maybeBecomeReadyForMoreMediaData();

    LOG(Media, "WebCoreDecompressionSession::imageForTime(%p) - no matching sample for time(%s) in queue(%s -> %s)", this, toString(time).utf8().data(), toString(startTime).utf8().data(), toString(endTime).utf8().data());
    return nullptr;
}

void WebCoreDecompressionSession::flush()
{
    m_decompressionQueue->dispatchSync([this, protectedThis = Ref { *this }]() mutable {
        PAL::CMBufferQueueReset(protectedThis->m_producerQueue.get());
        m_enqueingQueue->dispatchSync([protectedThis = WTFMove(protectedThis)] {
            PAL::CMBufferQueueReset(protectedThis->m_consumerQueue.get());
            Locker lock { protectedThis->m_lock };
            protectedThis->m_framesSinceLastQosCheck = 0;
            protectedThis->m_currentQosTier = 0;
            protectedThis->resetQosTier();
        });
    });
}

CMTime WebCoreDecompressionSession::getDecodeTime(CMBufferRef buf, void*)
{
    CMSampleBufferRef sample = checked_cf_cast<CMSampleBufferRef>(buf);
    return PAL::CMSampleBufferGetDecodeTimeStamp(sample);
}

CMTime WebCoreDecompressionSession::getPresentationTime(CMBufferRef buf, void*)
{
    CMSampleBufferRef sample = checked_cf_cast<CMSampleBufferRef>(buf);
    return PAL::CMSampleBufferGetPresentationTimeStamp(sample);
}

CMTime WebCoreDecompressionSession::getDuration(CMBufferRef buf, void*)
{
    CMSampleBufferRef sample = checked_cf_cast<CMSampleBufferRef>(buf);
    return PAL::CMSampleBufferGetDuration(sample);
}

CFComparisonResult WebCoreDecompressionSession::compareBuffers(CMBufferRef buf1, CMBufferRef buf2, void* refcon)
{
    return (CFComparisonResult)PAL::CMTimeCompare(getPresentationTime(buf1, refcon), getPresentationTime(buf2, refcon));
}

void WebCoreDecompressionSession::resetQosTier()
{
    assertIsHeld(m_lock);

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
    Locker lock { m_lock };
    if (!m_qosTiers)
        return;

    if (m_currentQosTier + 1 >= CFArrayGetCount(m_qosTiers.get()))
        return;

    ++m_currentQosTier;
    resetQosTier();
}

void WebCoreDecompressionSession::decreaseQosTier()
{
    Locker lock { m_lock };
    if (!m_qosTiers)
        return;

    if (m_currentQosTier <= 0)
        return;

    --m_currentQosTier;
    resetQosTier();
}

void WebCoreDecompressionSession::updateQosWithDecodeTimeStatistics(double ratio)
{
    assertIsCurrent(m_decompressionQueue.get());

    static const double kMovingAverageAlphaValue = 0.1;
    static const unsigned kNumberOfFramesBeforeSwitchingTiers = 60;
    static const double kHighWaterDecodeRatio = 1.;
    static const double kLowWaterDecodeRatio = 0.5;

    auto timebase = this->timebase();
    if (!timebase)
        return;

    double rate = PAL::CMTimebaseGetRate(timebase.get());
    if (!rate)
        rate = 1;

    m_decodeRatioMovingAverage += kMovingAverageAlphaValue * (ratio - m_decodeRatioMovingAverage) * rate;
    unsigned framesSinceLastQosCheck = ++m_framesSinceLastQosCheck;
    if (framesSinceLastQosCheck < kNumberOfFramesBeforeSwitchingTiers)
        return;

    LOG(Media, "WebCoreDecompressionSession::updateQosWithDecodeTimeStatistics(%p) - framesSinceLastQosCheck(%ld), decodeRatioMovingAverage(%g)", this, framesSinceLastQosCheck, m_decodeRatioMovingAverage);
    if (m_decodeRatioMovingAverage > kHighWaterDecodeRatio)
        increaseQosTier();
    else if (m_decodeRatioMovingAverage < kLowWaterDecodeRatio)
        decreaseQosTier();
    m_framesSinceLastQosCheck = 0;
}

Ref<MediaPromise> WebCoreDecompressionSession::initializeVideoDecoder(FourCharCode codec)
{
    VideoDecoder::Config config { { }, 0, 0, VideoDecoder::HardwareAcceleration::Yes, VideoDecoder::HardwareBuffer::Yes };
    MediaPromise::Producer producer;
    auto promise = producer.promise();
    VideoDecoder::create(VideoDecoder::fourCCToCodecString(codec), config, [protectedThis = Ref { *this }, this, producer = WTFMove(producer)](VideoDecoder::CreateResult&& result) {
        assertIsCurrent(m_decompressionQueue.get());
        if (!result || isInvalidated()) {
            producer.reject(PlatformMediaError::DecoderCreationError);
            return;
        }
        Locker lock { m_lock };
        m_videoDecoder = result.value().moveToUniquePtr();
        producer.resolve();
    }, [weakThis = ThreadSafeWeakPtr { *this }, this](Expected<VideoDecoder::DecodedFrame, String>&& result) {
        if (RefPtr protectedThis = weakThis.get()) {
            assertIsCurrent(m_decompressionQueue.get());
            if (isInvalidated() || !m_pendingDecodeData)
                return;

            if (!result) {
                handleDecompressionOutput(false, -1, 0, nullptr, PAL::kCMTimeInvalid, PAL::kCMTimeInvalid);
                return;
            }

            auto presentationTime = PAL::toCMTime(MediaTime(result->timestamp, 1000000));
            auto presentationDuration = PAL::toCMTime(MediaTime(result->duration.value_or(0), 1000000));
            handleDecompressionOutput(m_pendingDecodeData->displaying, noErr, 0, result->frame->pixelBuffer(), presentationTime, presentationDuration);
        }
    }, [queue = m_decompressionQueue](Function<void()>&& function) {
        queue->dispatch(WTFMove(function));
    });
    return promise;
}

} // namespace WebCore
