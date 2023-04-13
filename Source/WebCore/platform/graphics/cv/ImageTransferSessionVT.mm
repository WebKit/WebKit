/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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
#import "ImageTransferSessionVT.h"

#import "CVUtilities.h"
#import "GraphicsContextCG.h"
#import "Logging.h"
#import "VideoFrameCV.h"
#import <CoreMedia/CMFormatDescription.h>
#import <CoreMedia/CMSampleBuffer.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>

#if !PLATFORM(MACCATALYST)
#import <wtf/spi/cocoa/IOSurfaceSPI.h>
#endif

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

ImageTransferSessionVT::ImageTransferSessionVT(uint32_t pixelFormat, bool shouldUseIOSurface)
    : m_shouldUseIOSurface(shouldUseIOSurface)
{
    VTPixelTransferSessionRef transferSession;
    VTPixelTransferSessionCreate(kCFAllocatorDefault, &transferSession);
    ASSERT(transferSession);
    m_transferSession = adoptCF(transferSession);

    auto status = VTSessionSetProperty(transferSession, kVTPixelTransferPropertyKey_ScalingMode, kVTScalingMode_Trim);
    if (status != kCVReturnSuccess)
        RELEASE_LOG(Media, "ImageTransferSessionVT::ImageTransferSessionVT: VTSessionSetProperty(kVTPixelTransferPropertyKey_ScalingMode) failed with error %d", static_cast<int>(status));

    status = VTSessionSetProperty(transferSession, kVTPixelTransferPropertyKey_EnableHighSpeedTransfer, @YES);
    if (status != kCVReturnSuccess)
        RELEASE_LOG(Media, "ImageTransferSessionVT::ImageTransferSessionVT: VTSessionSetProperty(kVTPixelTransferPropertyKey_EnableHighSpeedTransfer) failed with error %d", static_cast<int>(status));

    status = VTSessionSetProperty(transferSession, kVTPixelTransferPropertyKey_RealTime, @YES);
    if (status != kCVReturnSuccess)
        RELEASE_LOG(Media, "ImageTransferSessionVT::ImageTransferSessionVT: VTSessionSetProperty(kVTPixelTransferPropertyKey_RealTime) failed with error %d", static_cast<int>(status));

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    status = VTSessionSetProperty(transferSession, kVTPixelTransferPropertyKey_EnableHardwareAcceleratedTransfer, @YES);
    if (status != kCVReturnSuccess)
        RELEASE_LOG(Media, "ImageTransferSessionVT::ImageTransferSessionVT: VTSessionSetProperty(kVTPixelTransferPropertyKey_EnableHardwareAcceleratedTransfer) failed with error %d", static_cast<int>(status));
#endif

    m_pixelFormat = pixelFormat;
}

bool ImageTransferSessionVT::setSize(const IntSize& size)
{
    if (m_size == size && m_outputBufferPool)
        return true;
    auto bufferPool = createCVPixelBufferPool(size.width(), size.height(), m_pixelFormat, 6, false, m_shouldUseIOSurface);
    if (!bufferPool)
        return false;
    m_outputBufferPool = WTFMove(*bufferPool);
    m_size = size;
    return true;
}

RetainPtr<CVPixelBufferRef> ImageTransferSessionVT::convertPixelBuffer(CVPixelBufferRef sourceBuffer, const IntSize& size)
{
    if (sourceBuffer && m_size == IntSize(CVPixelBufferGetWidth(sourceBuffer), CVPixelBufferGetHeight(sourceBuffer)) && m_pixelFormat == CVPixelBufferGetPixelFormatType(sourceBuffer))
        return retainPtr(sourceBuffer);

    if (!sourceBuffer || !setSize(size))
        return nullptr;

    auto result = createCVPixelBufferFromPool(m_outputBufferPool.get(), m_maxBufferPoolSize);
    if (!result) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertPixelBuffer, createCVPixelBufferFromPool failed with error %d", static_cast<int>(result.error()));
        return nullptr;
    }
    auto outputBuffer = WTFMove(*result);

    auto err = VTPixelTransferSessionTransferImage(m_transferSession.get(), sourceBuffer, outputBuffer.get());
    if (err) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertPixelBuffer, VTPixelTransferSessionTransferImage failed with error %d", static_cast<int>(err));
        return nullptr;
    }

    return outputBuffer;
}

RetainPtr<CMSampleBufferRef> ImageTransferSessionVT::convertCMSampleBuffer(CMSampleBufferRef sourceBuffer, const IntSize& size, const MediaTime* sampleTime)
{
    if (!sourceBuffer)
        return nullptr;

    auto description = PAL::CMSampleBufferGetFormatDescription(sourceBuffer);
    auto sourceSize = FloatSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(description, true, true));
    auto pixelBuffer = static_cast<CVPixelBufferRef>(PAL::CMSampleBufferGetImageBuffer(sourceBuffer));
    if (size == expandedIntSize(sourceSize) && m_pixelFormat == CVPixelBufferGetPixelFormatType(pixelBuffer))
        return retainPtr(sourceBuffer);

    if (!setSize(size))
        return nullptr;

    auto convertedPixelBuffer = convertPixelBuffer(pixelBuffer, size);
    if (!convertedPixelBuffer)
        return nullptr;

    CMItemCount itemCount = 0;
    auto status = PAL::CMSampleBufferGetSampleTimingInfoArray(sourceBuffer, 1, nullptr, &itemCount);
    if (status != noErr) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertCMSampleBuffer: CMSampleBufferGetSampleTimingInfoArray failed with error code: %d", static_cast<int>(status));
        return nullptr;
    }
    Vector<CMSampleTimingInfo> timingInfoArray;
    CMSampleTimingInfo* timeingInfoPtr = nullptr;
    if (itemCount) {
        timingInfoArray.grow(itemCount);
        status = PAL::CMSampleBufferGetSampleTimingInfoArray(sourceBuffer, itemCount, timingInfoArray.data(), nullptr);
        if (status != noErr) {
            RELEASE_LOG(Media, "ImageTransferSessionVT::convertCMSampleBuffer: CMSampleBufferGetSampleTimingInfoArray failed with error code: %d", static_cast<int>(status));
            return nullptr;
        }

        if (sampleTime) {
            auto cmTime = PAL::toCMTime(*sampleTime);
            for (auto& timing : timingInfoArray) {
                timing.presentationTimeStamp = cmTime;
                timing.decodeTimeStamp = cmTime;
            }
        }
        timeingInfoPtr = timingInfoArray.data();
    }

    CMVideoFormatDescriptionRef formatDescription = nullptr;
    status = PAL::CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, convertedPixelBuffer.get(), &formatDescription);
    if (status != noErr) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertCMSampleBuffer: CMVideoFormatDescriptionCreateForImageBuffer returned: %d", static_cast<int>(status));
        return nullptr;
    }

    CMSampleBufferRef resizedSampleBuffer;
    status = PAL::CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, convertedPixelBuffer.get(), formatDescription, timeingInfoPtr, &resizedSampleBuffer);
    CFRelease(formatDescription);
    if (status != noErr) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertCMSampleBuffer: failed to create CMSampleBuffer with error code: %d", static_cast<int>(status));
        return nullptr;
    }

    return adoptCF(resizedSampleBuffer);
}

RetainPtr<CVPixelBufferRef> ImageTransferSessionVT::createPixelBuffer(CGImageRef image, const IntSize& size)
{
    if (!image || !setSize(size))
        return nullptr;

    CVPixelBufferRef rgbBuffer;
    auto imageSize = IntSize(CGImageGetWidth(image), CGImageGetHeight(image));
    auto status = CVPixelBufferCreate(kCFAllocatorDefault, imageSize.width(), imageSize.height(), kCVPixelFormatType_32ARGB, nullptr, &rgbBuffer);
    if (status != kCVReturnSuccess) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::createPixelBuffer: CVPixelBufferCreate failed with error code: %d", static_cast<int>(status));
        return nullptr;
    }

    CVPixelBufferLockBaseAddress(rgbBuffer, 0);
    void* data = CVPixelBufferGetBaseAddress(rgbBuffer);
    auto retainedRGBBuffer = adoptCF(rgbBuffer);
    auto context = adoptCF(CGBitmapContextCreate(data, imageSize.width(), imageSize.height(), 8, CVPixelBufferGetBytesPerRow(rgbBuffer), sRGBColorSpaceRef(), (CGBitmapInfo) kCGImageAlphaNoneSkipFirst));
    if (!context) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::createPixelBuffer: CGBitmapContextCreate returned nullptr");
        return nullptr;
    }

    CGContextDrawImage(context.get(), CGRectMake(0, 0, imageSize.width(), imageSize.height()), image);
    CVPixelBufferUnlockBaseAddress(rgbBuffer, 0);

    return convertPixelBuffer(rgbBuffer, size);
}

RetainPtr<CMSampleBufferRef> ImageTransferSessionVT::createCMSampleBuffer(CVPixelBufferRef sourceBuffer, const MediaTime& sampleTime, const IntSize& size)
{
    if (!sourceBuffer || !setSize(size))
        return nullptr;

    auto bufferSize = IntSize(CVPixelBufferGetWidth(sourceBuffer), CVPixelBufferGetHeight(sourceBuffer));
    RetainPtr<CVPixelBufferRef> inputBuffer = sourceBuffer;
    if (bufferSize != m_size || m_pixelFormat != CVPixelBufferGetPixelFormatType(sourceBuffer)) {
        inputBuffer = convertPixelBuffer(sourceBuffer, m_size);
        if (!inputBuffer)
            return nullptr;
    }

    CMVideoFormatDescriptionRef formatDescription = nullptr;
    auto status = PAL::CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)inputBuffer.get(), &formatDescription);
    if (status) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertPixelBuffer: failed to initialize CMVideoFormatDescription with error code: %d", static_cast<int>(status));
        return nullptr;
    }

    CMSampleBufferRef sampleBuffer;
    auto cmTime = PAL::toCMTime(sampleTime);
    CMSampleTimingInfo timingInfo = { PAL::kCMTimeInvalid, cmTime, cmTime };
    status = PAL::CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)inputBuffer.get(), formatDescription, &timingInfo, &sampleBuffer);
    CFRelease(formatDescription);
    if (status) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertPixelBuffer: failed to initialize CMSampleBuffer with error code: %d", static_cast<int>(status));
        return nullptr;
    }

    return adoptCF(sampleBuffer);
}

RetainPtr<CMSampleBufferRef> ImageTransferSessionVT::createCMSampleBuffer(CGImageRef image, const MediaTime& sampleTime, const IntSize& size)
{
    auto pixelBuffer = createPixelBuffer(image, size);
    if (!pixelBuffer)
        return nullptr;

    return createCMSampleBuffer(pixelBuffer.get(), sampleTime, size);
}

#if !PLATFORM(MACCATALYST)

RetainPtr<CMSampleBufferRef> ImageTransferSessionVT::createCMSampleBuffer(IOSurfaceRef surface, const MediaTime &sampleTime, const IntSize &size)
{
    if (!surface || !setSize(size))
        return nullptr;
    auto pixelBuffer = createCVPixelBuffer(surface).value_or(nullptr);
    if (!pixelBuffer)
        return nullptr;

    return createCMSampleBuffer(pixelBuffer.get(), sampleTime, size);
}
#endif

RefPtr<VideoFrame> ImageTransferSessionVT::convertVideoFrame(VideoFrame& videoFrame, const IntSize& size)
{
    if (size == expandedIntSize(videoFrame.presentationSize()))
        return &videoFrame;

    auto resizedBuffer = convertPixelBuffer(videoFrame.pixelBuffer(), size);
    if (!resizedBuffer)
        return nullptr;

    return VideoFrameCV::create(videoFrame.presentationTime(), videoFrame.isMirrored(), videoFrame.rotation(), WTFMove(resizedBuffer));
}

RefPtr<VideoFrame> ImageTransferSessionVT::createVideoFrame(CGImageRef image, const WTF::MediaTime& time, const IntSize& size)
{
    return createVideoFrame(image, time, size, VideoFrame::Rotation::None);
}
RefPtr<VideoFrame> ImageTransferSessionVT::createVideoFrame(CMSampleBufferRef image, const WTF::MediaTime& time, const IntSize& size)
{
    return createVideoFrame(image, time, size, VideoFrame::Rotation::None);
}

#if !PLATFORM(MACCATALYST)
RefPtr<VideoFrame> ImageTransferSessionVT::createVideoFrame(IOSurfaceRef surface, const MediaTime& sampleTime, const IntSize& size)
{
    return createVideoFrame(surface, sampleTime, size, VideoFrame::Rotation::None);
}

RefPtr<VideoFrame> ImageTransferSessionVT::createVideoFrame(IOSurfaceRef surface, const MediaTime& sampleTime, const IntSize& size, VideoFrame::Rotation rotation, bool mirrored)
{
    auto sampleBuffer = createCMSampleBuffer(surface, sampleTime, size);
    if (!sampleBuffer)
        return nullptr;

    return VideoFrameCV::create(sampleBuffer.get(), mirrored, rotation);
}
#endif

RefPtr<VideoFrame> ImageTransferSessionVT::createVideoFrame(CGImageRef image, const MediaTime& sampleTime, const IntSize& size, VideoFrame::Rotation rotation, bool mirrored)
{
    auto sampleBuffer = createCMSampleBuffer(image, sampleTime, size);
    if (!sampleBuffer)
        return nullptr;

    return VideoFrameCV::create(sampleBuffer.get(), mirrored, rotation);
}

RefPtr<VideoFrame> ImageTransferSessionVT::createVideoFrame(CMSampleBufferRef buffer, const MediaTime& sampleTime, const IntSize& size, VideoFrame::Rotation rotation, bool mirrored)
{
    auto sampleBuffer = convertCMSampleBuffer(buffer, size, &sampleTime);
    if (!sampleBuffer)
        return nullptr;

    return VideoFrameCV::create(sampleBuffer.get(), mirrored, rotation);
}

} // namespace WebCore
