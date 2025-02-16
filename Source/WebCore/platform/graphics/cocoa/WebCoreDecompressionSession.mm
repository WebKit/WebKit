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

#import "FormatDescriptionUtilities.h"
#import "IOSurface.h"
#import "Logging.h"
#import "MediaSampleAVFObjC.h"
#import "PixelBufferConformerCV.h"
#import "VideoDecoder.h"
#import "VideoFrame.h"
#import <CoreMedia/CMBufferQueue.h>
#import <CoreMedia/CMFormatDescription.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <wtf/BlockPtr.h>
#import <wtf/MainThread.h>
#import <wtf/MediaTime.h>
#import <wtf/NativePromise.h>
#import <wtf/RunLoop.h>
#import <wtf/StringPrintStream.h>
#import <wtf/Vector.h>
#import <wtf/WTFSemaphore.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cf/VectorCF.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

WebCoreDecompressionSession::WebCoreDecompressionSession(Mode mode)
    : m_mode(mode)
    , m_decompressionQueue(WorkQueue::create("WebCoreDecompressionSession Decompression Queue"_s))
{
}

WebCoreDecompressionSession::~WebCoreDecompressionSession() = default;

void WebCoreDecompressionSession::invalidate()
{
    assertIsMainThread();
    m_invalidated = true;
    Locker lock { m_lock };
    m_decompressionQueue->dispatch([decoder = WTFMove(m_videoDecoder)] {
        if (decoder)
            decoder->close();
    });
}

void WebCoreDecompressionSession::assignResourceOwner(CVImageBufferRef imageBuffer)
{
    if (!m_resourceOwner || !imageBuffer || CFGetTypeID(imageBuffer) != CVPixelBufferGetTypeID())
        return;
    if (auto surface = CVPixelBufferGetIOSurface((CVPixelBufferRef)imageBuffer))
        IOSurface::setOwnershipIdentity(surface, m_resourceOwner);
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
        auto videoDecoderSpecification = @{ (__bridge NSString *)kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder: @YES };

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
        }
    }

    return m_decompressionSession;
}

auto WebCoreDecompressionSession::decodeSample(CMSampleBufferRef sample, bool displaying) -> Ref<DecodingPromise>
{
    DecodingPromise::Producer producer;
    auto promise = producer.promise();
    m_decompressionQueue->dispatch([protectedThis = RefPtr { this }, producer = WTFMove(producer), sample = RetainPtr { sample }, displaying, flushId = m_flushId.load()]() mutable {
        if (flushId == protectedThis->m_flushId)
            protectedThis->decodeSampleInternal(sample.get(), displaying)->chainTo(WTFMove(producer));
        else
            producer.resolve(nullptr);
    });
    return promise;
}

Ref<WebCoreDecompressionSession::DecodingPromise> WebCoreDecompressionSession::decodeSampleInternal(CMSampleBufferRef sample, bool displaying)
{
    assertIsCurrent(m_decompressionQueue.get());

    m_lastDecodingError = noErr;
    m_lastDecodedSample = nullptr;

    VTDecodeInfoFlags flags { kVTDecodeFrame_EnableTemporalProcessing };
    if (!displaying)
        flags |= kVTDecodeFrame_DoNotOutputFrame;

    RetainPtr decompressionSession = ensureDecompressionSessionForSample(sample);
    if (!decompressionSession && !m_videoDecoderCreationFailed) {
        RefPtr<MediaPromise> initPromise;

        {
            Locker lock { m_lock };
            if (!m_videoDecoder) {
                if (isInvalidated())
                    return DecodingPromise::createAndReject(0);
                RetainPtr videoFormatDescription = PAL::CMSampleBufferGetFormatDescription(sample);
                auto fourCC = PAL::CMFormatDescriptionGetMediaSubType(videoFormatDescription.get());

                RetainPtr extensions = PAL::CMFormatDescriptionGetExtensions(videoFormatDescription.get());
                if (!extensions)
                    return DecodingPromise::createAndReject(0);

                RetainPtr extensionAtoms = dynamic_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(extensions.get(), PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms));
                if (!extensionAtoms)
                    return DecodingPromise::createAndReject(0);

                // We should only hit this code path for VP8 and SW VP9 decoder, look for the vpcC path.
                RetainPtr configurationRecord = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(extensionAtoms.get(), CFSTR("vpcC")));
                if (!configurationRecord)
                    return DecodingPromise::createAndReject(0);

                auto colorSpace = colorSpaceFromFormatDescription(videoFormatDescription.get());

                initPromise = initializeVideoDecoder(fourCC, span(configurationRecord.get()), colorSpace);
            }
        }
        auto decode = [protectedThis = Ref { *this }, this, cmSamples = RetainPtr { sample }, displaying] {
            Locker lock { m_lock };
            RefPtr videoDecoder = m_videoDecoder;
            if (!videoDecoder)
                return DecodingPromise::createAndReject(0);

            assertIsCurrent(m_decompressionQueue.get());

            m_pendingDecodeData = { displaying };
            MediaTime totalDuration = PAL::toMediaTime(PAL::CMSampleBufferGetDuration(cmSamples.get()));

            Vector<Ref<VideoDecoder::DecodePromise>> promises;
            for (Ref sample : MediaSampleAVFObjC::create(cmSamples.get(), 0)->divide()) {
                auto cmSample = sample->platformSample().sample.cmSampleBuffer;
                MediaTime presentationTimestamp = PAL::toMediaTime(PAL::CMSampleBufferGetPresentationTimeStamp(cmSample));
                CMBlockBufferRef rawBuffer = PAL::CMSampleBufferGetDataBuffer(cmSample);
                ASSERT(rawBuffer);
                RetainPtr buffer = rawBuffer;
                // Make sure block buffer is contiguous.
                if (!PAL::CMBlockBufferIsRangeContiguous(rawBuffer, 0, 0)) {
                    CMBlockBufferRef contiguousBuffer;
                    if (auto status = PAL::CMBlockBufferCreateContiguous(nullptr, rawBuffer, nullptr, nullptr, 0, 0, 0, &contiguousBuffer))
                        return DecodingPromise::createAndReject(status);
                    buffer = adoptCF(contiguousBuffer);
                }
                auto data = PAL::CMBlockBufferGetDataSpan(buffer.get());
                if (!data.data())
                    return DecodingPromise::createAndReject(-1);
                promises.append(videoDecoder->decode({ data, true, presentationTimestamp.toMicroseconds(), 0 }));
            }
            DecodingPromise::Producer producer;
            auto promise = producer.promise();
            VideoDecoder::DecodePromise::all(promises)->whenSettled(m_decompressionQueue.get(), [weakThis = ThreadSafeWeakPtr { *this }, totalDuration = PAL::toCMTime(totalDuration), producer = WTFMove(producer)] (auto&& result) {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis || protectedThis->isInvalidated()) {
                    producer.reject(0);
                    return;
                }
                assertIsCurrent(protectedThis->m_decompressionQueue.get());
                if (!result)
                    producer.reject(kVTVideoDecoderBadDataErr);
                else
                    producer.resolve(std::exchange(protectedThis->m_lastDecodedSample, { }));
                if (!protectedThis->m_pendingDecodeData)
                    return;
                protectedThis->m_pendingDecodeData.reset();
            });
            return promise;
        };
        if (initPromise) {
            return initPromise->then(m_decompressionQueue, WTFMove(decode), [] {
                return DecodingPromise::createAndReject(kVTVideoDecoderNotAvailableNowErr);
            });
        }
        return decode();
    }

    if (!decompressionSession)
        return DecodingPromise::createAndReject(kVTVideoDecoderNotAvailableNowErr);
    ASSERT(!m_lastDecodedSample);
    DecodingPromise::Producer producer;
    auto promise = producer.promise();
    auto handler = makeBlockPtr([weakThis = ThreadSafeWeakPtr { *this }, displaying, producer = WTFMove(producer), numberOfTimesCalled = 0](OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer, CMTime presentationTimeStamp, CMTime presentationDuration) mutable {
        if (++numberOfTimesCalled > 1)
            return;
        if (RefPtr protectedThis = weakThis.get()) {
            protectedThis->m_decompressionQueue->dispatch([protectedThis, producer = WTFMove(producer), displaying, status, infoFlags, imageBuffer = RetainPtr { imageBuffer }, presentationTimeStamp, presentationDuration]() {
                assertIsCurrent(protectedThis->m_decompressionQueue.get());
                protectedThis->handleDecompressionOutput(displaying, status, infoFlags, imageBuffer.get(), presentationTimeStamp, presentationDuration);
                if (protectedThis->m_lastDecodingError != noErr)
                    producer.reject(protectedThis->m_lastDecodingError);
                else
                    producer.resolve(std::exchange(protectedThis->m_lastDecodedSample, { }));
            });
        } else
            producer.reject(0);
    });

    if (auto result = VTDecompressionSessionDecodeFrameWithOutputHandler(decompressionSession.get(), sample, flags, nullptr, handler.get()); result != noErr)
        handler(result, 0, nullptr, PAL::kCMTimeInvalid, PAL::kCMTimeInvalid);

    return promise;
}

RetainPtr<CVPixelBufferRef> WebCoreDecompressionSession::decodeSampleSync(CMSampleBufferRef sample)
{
    RetainPtr decompressionSession = ensureDecompressionSessionForSample(sample);
    if (!decompressionSession)
        return nullptr;

    RetainPtr<CVPixelBufferRef> pixelBuffer;
    VTDecodeInfoFlags flags { 0 };
    WTF::Semaphore syncDecompressionOutputSemaphore { 0 };
    Ref protectedThis { *this };
    VTDecompressionSessionDecodeFrameWithOutputHandler(decompressionSession.get(), sample, flags, nullptr, [&protectedThis, &pixelBuffer, &syncDecompressionOutputSemaphore] (OSStatus, VTDecodeInfoFlags, CVImageBufferRef imageBuffer, CMTime, CMTime) mutable {
        protectedThis->assignResourceOwner(imageBuffer);
        if (imageBuffer && CFGetTypeID(imageBuffer) == CVPixelBufferGetTypeID())
            pixelBuffer = (CVPixelBufferRef)imageBuffer;
        syncDecompressionOutputSemaphore.signal();
    });
    syncDecompressionOutputSemaphore.wait();
    return pixelBuffer;
}

void WebCoreDecompressionSession::handleDecompressionOutput(bool displaying, OSStatus status, VTDecodeInfoFlags, CVImageBufferRef rawImageBuffer, CMTime presentationTimeStamp, CMTime presentationDuration)
{
    assertIsCurrent(m_decompressionQueue.get());

    if (status != noErr) {
        m_lastDecodingError = status;
        return;
    }

    if (!displaying)
        return;

    CMVideoFormatDescriptionRef rawImageBufferDescription = nullptr;
    if (auto status = PAL::CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, rawImageBuffer, &rawImageBufferDescription); status != noErr) {
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
        m_lastDecodingError = status;
        return;
    }

    m_lastDecodedSample = adoptCF(rawImageSampleBuffer);
}

void WebCoreDecompressionSession::flush()
{
    m_flushId++;
}

Ref<MediaPromise> WebCoreDecompressionSession::initializeVideoDecoder(FourCharCode codec, std::span<const uint8_t> description, const std::optional<PlatformVideoColorSpace>& colorSpace)
{
    VideoDecoder::Config config {
        .description = description,
        .colorSpace = colorSpace,
        .decoding = VideoDecoder::HardwareAcceleration::Yes,
        .pixelBuffer = VideoDecoder::HardwareBuffer::Yes,
        .noOutputAsError = VideoDecoder::TreatNoOutputAsError::No
    };
    MediaPromise::Producer producer;
    auto promise = producer.promise();

    VideoDecoder::create(VideoDecoder::fourCCToCodecString(codec), config, [weakThis = ThreadSafeWeakPtr { *this }, queue = m_decompressionQueue] (auto&& result) {
        queue->dispatch([weakThis, result = WTFMove(result)] () {
            if (RefPtr protectedThis = weakThis.get()) {
                assertIsCurrent(protectedThis->m_decompressionQueue.get());
                if (protectedThis->isInvalidated() || !protectedThis->m_pendingDecodeData)
                    return;

                if (!result) {
                    protectedThis->handleDecompressionOutput(false, -1, 0, nullptr, PAL::kCMTimeInvalid, PAL::kCMTimeInvalid);
                    return;
                }

                auto presentationTime = PAL::toCMTime(MediaTime(result->timestamp, 1000000));
                auto presentationDuration = PAL::toCMTime(MediaTime(result->duration.value_or(0), 1000000));
                protectedThis->handleDecompressionOutput(protectedThis->m_pendingDecodeData->displaying, noErr, 0, result->frame->pixelBuffer(), presentationTime, presentationDuration);
            }
        });
    })->whenSettled(m_decompressionQueue, [protectedThis = Ref { *this }, this, producer = WTFMove(producer), queue = m_decompressionQueue] (auto&& result) mutable {
        assertIsCurrent(m_decompressionQueue.get());
        if (!result || isInvalidated()) {
            producer.reject(PlatformMediaError::DecoderCreationError);
            return;
        }
        Locker lock { m_lock };
        m_videoDecoder = WTFMove(*result);
        producer.resolve();
    });

    return promise;
}

} // namespace WebCore
