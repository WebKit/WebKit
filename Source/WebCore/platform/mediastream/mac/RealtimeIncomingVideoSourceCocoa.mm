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

#import "config.h"
#import "RealtimeIncomingVideoSourceCocoa.h"

#if USE(LIBWEBRTC)

#import "Logging.h"
#import "MediaSampleAVFObjC.h"
#import "RealtimeVideoUtilities.h"
#import <wtf/cf/TypeCastsCF.h>

ALLOW_UNUSED_PARAMETERS_BEGIN
#import <webrtc/sdk/WebKit/WebKitUtilities.h>
ALLOW_UNUSED_PARAMETERS_END

#import <pal/cf/CoreMediaSoftLink.h>

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
        m_pixelBufferPool = createPixelBufferPool(width, height);
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
            ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "Failed creating a pixel buffer with error ", status);
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
    ALWAYS_LOG_IF(loggerPtr() && !(++m_numberOfFrames % 60), LOGIDENTIFIER, "frame ", m_numberOfFrames);
#endif

    auto pixelBuffer = pixelBufferFromVideoFrame(frame);
    if (!pixelBuffer) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "Failed to get a pixel buffer from a frame");
        return;
    }

    CMSampleTimingInfo timingInfo;
    timingInfo.presentationTimeStamp = CMTimeMake(frame.timestamp_us(), 1000000);
    timingInfo.decodeTimeStamp = kCMTimeInvalid;
    timingInfo.duration = kCMTimeInvalid;

    CMVideoFormatDescriptionRef formatDescription;
    OSStatus ostatus = CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)pixelBuffer, &formatDescription);
    if (ostatus != noErr) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "Failed to initialize CMVideoFormatDescription with error ", static_cast<int>(ostatus));
        return;
    }

    CMSampleBufferRef sampleBuffer;
    ostatus = CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)pixelBuffer, formatDescription, &timingInfo, &sampleBuffer);
    CFRelease(formatDescription);
    if (ostatus != noErr) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "Failed to create the sample buffer with error ", static_cast<int>(ostatus));
        return;
    }

    auto sample = adoptCF(sampleBuffer);

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

    setIntrinsicSize(IntSize(width, height));
    videoSampleAvailable(MediaSampleAVFObjC::create(sample.get(), rotation));
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
