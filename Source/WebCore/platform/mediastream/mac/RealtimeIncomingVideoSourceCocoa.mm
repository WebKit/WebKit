/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RealtimeIncomingVideoSourceCocoa.h"

#if USE(LIBWEBRTC)

#include "Logging.h"
#include "MediaSampleAVFObjC.h"
#include "RealtimeVideoUtilities.h"
#include <pal/cf/CoreMediaSoftLink.h>

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/sdk/WebKit/WebKitUtilities.h>

ALLOW_UNUSED_PARAMETERS_END

#include <wtf/cf/TypeCastsCF.h>

#include "CoreVideoSoftLink.h"

namespace WebCore {
using namespace PAL;

Ref<RealtimeIncomingVideoSource> RealtimeIncomingVideoSource::create(rtc::scoped_refptr<webrtc::VideoTrackInterface>&& videoTrack, String&& trackId)
{
    auto source = RealtimeIncomingVideoSourceCocoa::create(WTFMove(videoTrack), WTFMove(trackId));
    source->start();
    return WTFMove(source);
}

Ref<RealtimeIncomingVideoSourceCocoa> RealtimeIncomingVideoSourceCocoa::create(rtc::scoped_refptr<webrtc::VideoTrackInterface>&& videoTrack, String&& trackId)
{
    return adoptRef(*new RealtimeIncomingVideoSourceCocoa(WTFMove(videoTrack), WTFMove(trackId)));
}

RealtimeIncomingVideoSourceCocoa::RealtimeIncomingVideoSourceCocoa(rtc::scoped_refptr<webrtc::VideoTrackInterface>&& videoTrack, String&& videoTrackId)
    : RealtimeIncomingVideoSource(WTFMove(videoTrack), WTFMove(videoTrackId))
{
}

RetainPtr<CVPixelBufferRef> createBlackPixelBuffer(size_t width, size_t height)
{
    OSType format = preferedPixelBufferFormat();
    ASSERT(format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange || format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);

    CVPixelBufferRef pixelBuffer = nullptr;
    auto status = CVPixelBufferCreate(kCFAllocatorDefault, width, height, format, nullptr, &pixelBuffer);
    ASSERT_UNUSED(status, status == noErr);

    status = CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    ASSERT(status == noErr);

    auto* yPlane = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));
    size_t yStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
    for (unsigned i = 0; i < height; ++i)
        memset(&yPlane[i * yStride], 0, width);

    auto* uvPlane = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1));
    size_t uvStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);
    for (unsigned i = 0; i < height / 2; ++i)
        memset(&uvPlane[i * uvStride], 128, width);

    status = CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    ASSERT(!status);
    return adoptCF(pixelBuffer);
}

CVPixelBufferPoolRef RealtimeIncomingVideoSourceCocoa::pixelBufferPool(size_t width, size_t height)
{
    if (!m_pixelBufferPool || m_pixelBufferPoolWidth != width || m_pixelBufferPoolHeight != height) {
        const OSType videoCaptureFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
        auto pixelAttributes = @{
            (__bridge NSString *)kCVPixelBufferWidthKey: @(width),
            (__bridge NSString *)kCVPixelBufferHeightKey: @(height),
            (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(videoCaptureFormat),
            (__bridge NSString *)kCVPixelBufferCGImageCompatibilityKey: @(NO),
#if PLATFORM(IOS)
            (__bridge NSString *)kCVPixelFormatOpenGLESCompatibility : @(YES),
#else
            (__bridge NSString *)kCVPixelBufferOpenGLCompatibilityKey : @(YES),
#endif
            (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{ }
        };

        CVPixelBufferPoolRef pool = nullptr;
        auto status = CVPixelBufferPoolCreate(kCFAllocatorDefault, nullptr, (__bridge CFDictionaryRef)pixelAttributes, &pool);

        if (status != kCVReturnSuccess) {
            RELEASE_LOG(MediaStream, "RealtimeIncomingVideoSourceCocoa::pixelBufferFromVideoFrame failed creating a pixel buffer pool with error %d", status);
            return nullptr;
        }
        m_pixelBufferPool = adoptCF(pool);
        m_pixelBufferPoolWidth = width;
        m_pixelBufferPoolHeight = height;
    }
    return m_pixelBufferPool.get();
}

RetainPtr<CVPixelBufferRef> RealtimeIncomingVideoSourceCocoa::pixelBufferFromVideoFrame(const webrtc::VideoFrame& frame)
{
    if (muted()) {
        if (!m_blackFrame || m_blackFrameWidth != frame.width() || m_blackFrameHeight != frame.height()) {
            m_blackFrameWidth = frame.width();
            m_blackFrameHeight = frame.height();
            m_blackFrame = createBlackPixelBuffer(m_blackFrameWidth, m_blackFrameHeight);
        }
        return m_blackFrame.get();
    }

    RetainPtr<CVPixelBufferRef> newPixelBuffer;
    return webrtc::pixelBufferFromFrame(frame, [this, &newPixelBuffer](size_t width, size_t height) -> CVPixelBufferRef {
        auto pixelBufferPool = this->pixelBufferPool(width, height);
        if (!pixelBufferPool)
            return nullptr;

        CVPixelBufferRef pixelBuffer = nullptr;
        auto status = CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, m_pixelBufferPool.get(), &pixelBuffer);

        if (status != kCVReturnSuccess) {
            RELEASE_LOG(MediaStream, "RealtimeIncomingVideoSourceCocoa::pixelBufferFromVideoFrame failed creating a pixel buffer with error %d", status);
            return nullptr;
        }
        newPixelBuffer = adoptCF(pixelBuffer);
        return newPixelBuffer.get();
    });
}

void RealtimeIncomingVideoSourceCocoa::OnFrame(const webrtc::VideoFrame& frame)
{
    if (!isProducingData())
        return;

#if !RELEASE_LOG_DISABLED
    if (!(++m_numberOfFrames % 30))
        RELEASE_LOG(MediaStream, "RealtimeIncomingVideoSourceCocoa::OnFrame %zu frame", m_numberOfFrames);
#endif

    auto pixelBuffer = pixelBufferFromVideoFrame(frame);
    if (!pixelBuffer) {
        LOG_ERROR("Failed to get a pixel buffer from a frame");
        return;
    }

    // FIXME: Convert timing information from VideoFrame to CMSampleTimingInfo.
    // For the moment, we will pretend that frames should be rendered asap.
    CMSampleTimingInfo timingInfo;
    timingInfo.presentationTimeStamp = kCMTimeInvalid;
    timingInfo.decodeTimeStamp = kCMTimeInvalid;
    timingInfo.duration = kCMTimeInvalid;

    CMVideoFormatDescriptionRef formatDescription;
    OSStatus ostatus = CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)pixelBuffer, &formatDescription);
    if (ostatus != noErr) {
        LOG_ERROR("Failed to initialize CMVideoFormatDescription: %d", static_cast<int>(ostatus));
        return;
    }

    CMSampleBufferRef sampleBuffer;
    ostatus = CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)pixelBuffer, formatDescription, &timingInfo, &sampleBuffer);
    CFRelease(formatDescription);
    if (ostatus != noErr) {
        LOG_ERROR("Failed to create the sample buffer: %d", static_cast<int>(ostatus));
        return;
    }

    auto sample = adoptCF(sampleBuffer);

    CFArrayRef attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, true);
    for (CFIndex i = 0; i < CFArrayGetCount(attachmentsArray); ++i) {
        CFMutableDictionaryRef attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
        CFDictionarySetValue(attachments, kCMSampleAttachmentKey_DisplayImmediately, kCFBooleanTrue);
    }

    unsigned width = frame.width();
    unsigned height = frame.height();

    MediaSample::VideoRotation rotation;
    switch (frame.rotation()) {
    case webrtc::kVideoRotation_0:
        rotation = MediaSample::VideoRotation::None;
        break;
    case webrtc::kVideoRotation_180:
        rotation = MediaSample::VideoRotation::UpsideDown;
        break;
    case webrtc::kVideoRotation_90:
        rotation = MediaSample::VideoRotation::Right;
        std::swap(width, height);
        break;
    case webrtc::kVideoRotation_270:
        rotation = MediaSample::VideoRotation::Left;
        std::swap(width, height);
        break;
    }

    callOnMainThread([protectedThis = makeRef(*this), sample = WTFMove(sample), width, height, rotation] {
        protectedThis->processNewSample(sample.get(), width, height, rotation);
    });
}

void RealtimeIncomingVideoSourceCocoa::processNewSample(CMSampleBufferRef sample, unsigned width, unsigned height, MediaSample::VideoRotation rotation)
{
    m_buffer = sample;
    auto size = this->size();
    if (WTF::safeCast<int>(width) != size.width() || WTF::safeCast<int>(height) != size.height())
        setSize(IntSize(width, height));

    videoSampleAvailable(MediaSampleAVFObjC::create(sample, rotation));
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
