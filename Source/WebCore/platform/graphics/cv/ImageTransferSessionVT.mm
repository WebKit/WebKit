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
#import <wtf/cocoa/TypeCastsCocoa.h>

#if !PLATFORM(MACCATALYST)
#import <wtf/spi/cocoa/IOSurfaceSPI.h>
#endif

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

static RetainPtr<VTPixelTransferSessionRef> createTransferSession(bool shouldUseHWAcceleratedTransfer)
{
    VTPixelTransferSessionRef rawTransferSession = nullptr;
    auto status = VTPixelTransferSessionCreate(kCFAllocatorDefault, &rawTransferSession);
    if (status != kCVReturnSuccess)
        return nullptr;
    RetainPtr transferSession = adoptCF(rawTransferSession);
    auto setProperty = [&](CFStringRef key, CFTypeRef value) {
        auto status = VTSessionSetProperty(transferSession.get(), key, value);
        RELEASE_LOG_ERROR_IF(status != kCVReturnSuccess, Media, "VTSessionSetProperty(%{public}@) error: %d", key, static_cast<int>(status));
    };
    setProperty(kVTPixelTransferPropertyKey_ScalingMode, kVTScalingMode_Trim);
    setProperty(kVTPixelTransferPropertyKey_EnableHighSpeedTransfer, @YES);
    setProperty(kVTPixelTransferPropertyKey_RealTime, @YES);
#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    if (shouldUseHWAcceleratedTransfer)
        setProperty(kVTPixelTransferPropertyKey_EnableHardwareAcceleratedTransfer, @YES);
#else
    UNUSED_PARAM(shouldUseHWAcceleratedTransfer);
#endif
    return transferSession;
}

// FIXME: maybe we should use createTransferSession(shouldUseIOSurface). Clarify what
// kVTPixelTransferPropertyKey_EnableHardwareAcceleratedTransfer does, why is it not enabled
// on all platforms.
ImageTransferSessionVT::ImageTransferSessionVT(uint32_t pixelFormat, bool shouldUseIOSurface)
    : m_transferSession(createTransferSession(true))
    , m_shouldUseIOSurface(shouldUseIOSurface)
    , m_pixelFormat(pixelFormat)
{
}

void ImageTransferSessionVT::setCroppingRectangle(std::optional<FloatRect> rectangle, FloatSize size)
{
    if (!rectangle) {
        m_sourceCroppingDictionary = { };
        return;
    }

    auto width = rectangle->width();
    auto height = rectangle->height();
    // Offsets are relative from center of image buffer
    auto horizontalOffset = rectangle->x() - size.width() / 2 + width / 2;
    auto verticalOffset = rectangle->y() - size.height() / 2 + height / 2;

    FloatRect apertureRectangle {
        FloatPoint { horizontalOffset, verticalOffset },
        FloatSize { width, height }
    };

    if (m_croppingRectangle == apertureRectangle)
        return;

    m_croppingRectangle = apertureRectangle;

    m_sourceCroppingDictionary = @{
        (__bridge NSString *)kCVImageBufferCleanApertureWidthKey: @(apertureRectangle.width()),
        (__bridge NSString *)kCVImageBufferCleanApertureHeightKey: @(apertureRectangle.height()),
        (__bridge NSString *)kCVImageBufferCleanApertureVerticalOffsetKey: @(apertureRectangle.y()),
        (__bridge NSString *)kCVImageBufferCleanApertureHorizontalOffsetKey: @(apertureRectangle.x()),
    };
}

bool ImageTransferSessionVT::setSize(const IntSize& size)
{
    if (m_size == size && m_outputBufferPool)
        return true;
    auto bufferPool = createCVPixelBufferPool(size.width(), size.height(), m_pixelFormat, 6, false, m_shouldUseIOSurface);
    if (!bufferPool)
        return false;
    m_outputBufferPool = WTF::move(*bufferPool);
    m_size = size;
    return true;
}

RetainPtr<CVPixelBufferRef> ImageTransferSessionVT::convertPixelBuffer(CVPixelBufferRef sourceBuffer, const IntSize& size)
{
    if (!m_sourceCroppingDictionary && sourceBuffer && m_size == IntSize(CVPixelBufferGetWidth(sourceBuffer), CVPixelBufferGetHeight(sourceBuffer)) && m_pixelFormat == CVPixelBufferGetPixelFormatType(sourceBuffer))
        return retainPtr(sourceBuffer);

    if (m_sourceCroppingDictionary)
        CVBufferSetAttachment(sourceBuffer, kCVImageBufferCleanApertureKey, m_sourceCroppingDictionary.get(), kCVAttachmentMode_ShouldPropagate);

    if (!sourceBuffer || !setSize(size))
        return nullptr;

    auto result = createCVPixelBufferFromPool(m_outputBufferPool.get(), m_maxBufferPoolSize);
    if (!result) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertPixelBuffer, createCVPixelBufferFromPool failed with error %d", static_cast<int>(result.error()));
        return nullptr;
    }
    auto outputBuffer = WTF::move(*result);

    auto err = VTPixelTransferSessionTransferImage(m_transferSession.get(), sourceBuffer, outputBuffer.get());
    if (err) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertPixelBuffer, VTPixelTransferSessionTransferImage failed with error %d", static_cast<int>(err));
        return nullptr;
    }

    if (m_sourceCroppingDictionary)
        CVBufferRemoveAttachment(sourceBuffer, kCVImageBufferCleanApertureKey);

    return outputBuffer;
}

RetainPtr<CMSampleBufferRef> ImageTransferSessionVT::convertCMSampleBuffer(CMSampleBufferRef sourceBuffer, const IntSize& size, const MediaTime* sampleTime)
{
    if (!sourceBuffer)
        return nullptr;

    RetainPtr description = PAL::CMSampleBufferGetFormatDescription(sourceBuffer);
    auto sourceSize = FloatSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(description.get(), true, true));
    RetainPtr pixelBuffer = static_cast<CVPixelBufferRef>(PAL::CMSampleBufferGetImageBuffer(sourceBuffer));
    if (size == expandedIntSize(sourceSize) && m_pixelFormat == CVPixelBufferGetPixelFormatType(pixelBuffer.get()))
        return retainPtr(sourceBuffer);

    if (!setSize(size))
        return nullptr;

    auto convertedPixelBuffer = convertPixelBuffer(pixelBuffer.get(), size);
    if (!convertedPixelBuffer)
        return nullptr;

    CMItemCount itemCount = 0;
    auto status = PAL::CMSampleBufferGetSampleTimingInfoArray(sourceBuffer, 1, nullptr, &itemCount);
    if (status != noErr) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertCMSampleBuffer: CMSampleBufferGetSampleTimingInfoArray failed with error code: %d", static_cast<int>(status));
        return nullptr;
    }
    Vector<CMSampleTimingInfo> timingInfoArray;
    std::span<CMSampleTimingInfo> timeingInfoSpan;
    if (itemCount) {
        timingInfoArray.grow(itemCount);
        status = PAL::CMSampleBufferGetSampleTimingInfoArray(sourceBuffer, itemCount, timingInfoArray.mutableSpan().data(), nullptr);
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
        timeingInfoSpan = timingInfoArray.mutableSpan();
    }

    CMVideoFormatDescriptionRef formatDescription = nullptr;
    status = PAL::CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, convertedPixelBuffer.get(), &formatDescription);
    if (status != noErr) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertCMSampleBuffer: CMVideoFormatDescriptionCreateForImageBuffer returned: %d", static_cast<int>(status));
        return nullptr;
    }

    CMSampleBufferRef resizedSampleBuffer;
    status = PAL::CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, convertedPixelBuffer.get(), formatDescription, timeingInfoSpan.data(), &resizedSampleBuffer);
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
    auto context = adoptCF(CGBitmapContextCreate(data, imageSize.width(), imageSize.height(), 8, CVPixelBufferGetBytesPerRow(rgbBuffer), sRGBColorSpaceSingleton(), (CGBitmapInfo) kCGImageAlphaNoneSkipFirst));
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

    auto resizedBuffer = convertPixelBuffer(protect(videoFrame.pixelBuffer()).get(), size);
    if (!resizedBuffer)
        return nullptr;

    return VideoFrameCV::create(videoFrame.presentationTime(), videoFrame.isMirrored(), videoFrame.rotation(), WTF::move(resizedBuffer));
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

RetainPtr<CVPixelBufferRef> ImageTransferSessionVT::convertPixelBuffer(CVPixelBufferRef source, uint32_t targetPixelFormat)
{
    if (!source)
        return nullptr;
    if (CVPixelBufferGetPixelFormatType(source) == targetPixelFormat)
        return source;
    NSDictionary *targetOptions = nullptr;
    const bool shouldUseIOSurface = CVPixelBufferGetIOSurface(source);
    if (shouldUseIOSurface)
        targetOptions = @{ bridge_cast(kCVPixelBufferIOSurfacePropertiesKey) : @{ } };
    RetainPtr transferSession = createTransferSession(shouldUseIOSurface);
    CVPixelBufferRef rawTarget = nullptr;
    auto status = CVPixelBufferCreate(kCFAllocatorDefault, CVPixelBufferGetWidth(source), CVPixelBufferGetHeight(source), targetPixelFormat, bridge_cast(targetOptions), &rawTarget);
    if (status != kCVReturnSuccess)
        return nullptr;
    RetainPtr target = adoptCF(rawTarget);
    status = VTPixelTransferSessionTransferImage(transferSession.get(), source, target.get());
    if (status != kCVReturnSuccess)
        return nullptr;
    return target;
}

} // namespace WebCore
