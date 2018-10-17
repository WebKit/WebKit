/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if USE(VIDEOTOOLBOX)

#import "Logging.h"
#import "MediaSampleAVFObjC.h"
#import <CoreMedia/CMFormatDescription.h>
#import <CoreMedia/CMSampleBuffer.h>
#import <pal/cf/CoreMediaSoftLink.h>

#if HAVE(IOSURFACE) && !PLATFORM(IOSMAC)
#include <pal/spi/cocoa/IOSurfaceSPI.h>
#endif

#import "CoreVideoSoftLink.h"

namespace WebCore {
using namespace PAL;

static inline CFStringRef cvPixelFormatOpenGLKey()
{
#if PLATFORM(IOS) && !PLATFORM(IOSMAC)
    return kCVPixelFormatOpenGLESCompatibility;
#else
    return kCVPixelBufferOpenGLCompatibilityKey;
#endif
}

ImageTransferSessionVT::ImageTransferSessionVT(uint32_t pixelFormat)
{
    VTPixelTransferSessionRef transferSession;
    VTPixelTransferSessionCreate(kCFAllocatorDefault, &transferSession);
    ASSERT(transferSession);
    m_transferSession = adoptCF(transferSession);

    auto status = VTSessionSetProperty(transferSession, kVTPixelTransferPropertyKey_ScalingMode, kVTScalingMode_Trim);
    if (status != kCVReturnSuccess)
        RELEASE_LOG(Media, "ImageTransferSessionVT::ImageTransferSessionVT: VTSessionSetProperty(kVTPixelTransferPropertyKey_ScalingMode) failed with error %d", static_cast<int>(status));

    status = VTSessionSetProperty(transferSession, kVTPixelTransferPropertyKey_EnableHighSpeedTransfer, @(YES));
    if (status != kCVReturnSuccess)
        RELEASE_LOG(Media, "ImageTransferSessionVT::ImageTransferSessionVT: VTSessionSetProperty(kVTPixelTransferPropertyKey_EnableHighSpeedTransfer) failed with error %d", static_cast<int>(status));

    status = VTSessionSetProperty(transferSession, kVTPixelTransferPropertyKey_RealTime, @(YES));
    if (status != kCVReturnSuccess)
        RELEASE_LOG(Media, "ImageTransferSessionVT::ImageTransferSessionVT: VTSessionSetProperty(kVTPixelTransferPropertyKey_RealTime) failed with error %d", static_cast<int>(status));

#if PLATFORM(IOS) && !PLATFORM(IOSMAC)
    status = VTSessionSetProperty(transferSession, kVTPixelTransferPropertyKey_EnableHardwareAcceleratedTransfer, @(YES));
    if (status != kCVReturnSuccess)
        RELEASE_LOG(Media, "ImageTransferSessionVT::ImageTransferSessionVT: VTSessionSetProperty(kVTPixelTransferPropertyKey_EnableHardwareAcceleratedTransfer) failed with error %d", static_cast<int>(status));
#endif

    m_pixelFormat = pixelFormat;
}

bool ImageTransferSessionVT::setSize(const IntSize& size)
{
    if (m_size == size && m_outputBufferPool)
        return true;

    CFDictionaryRef pixelBufferOptions = (__bridge CFDictionaryRef) @{
        (__bridge NSString *)kCVPixelBufferWidthKey :@(size.width()),
        (__bridge NSString *)kCVPixelBufferHeightKey:@(size.height()),
        (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey:@(m_pixelFormat),
        (__bridge NSString *)cvPixelFormatOpenGLKey() : @(YES),
        (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{ /*empty dictionary*/ },
    };

    CFDictionaryRef pixelBufferPoolOptions = (__bridge CFDictionaryRef) @{
        (__bridge NSString *)kCVPixelBufferPoolMinimumBufferCountKey : @(6)
    };

    CVPixelBufferPoolRef bufferPool;
    auto status = CVPixelBufferPoolCreate(kCFAllocatorDefault, pixelBufferPoolOptions, pixelBufferOptions, &bufferPool);
    ASSERT(!status);
    if (status != kCVReturnSuccess)
        return false;

    m_outputBufferPool = adoptCF(bufferPool);
    m_size = size;
    m_ioSurfaceBufferAttributes = nullptr;

    return true;
}

RetainPtr<CVPixelBufferRef> ImageTransferSessionVT::convertPixelBuffer(CVPixelBufferRef sourceBuffer, const IntSize& size)
{
    if (sourceBuffer && m_size == IntSize(CVPixelBufferGetWidth(sourceBuffer), CVPixelBufferGetHeight(sourceBuffer)) && m_pixelFormat == CVPixelBufferGetPixelFormatType(sourceBuffer))
        return retainPtr(sourceBuffer);

    if (!sourceBuffer || !setSize(size))
        return nullptr;

    CVPixelBufferRef outputBuffer = nullptr;
    auto status = CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, m_outputBufferPool.get(), &outputBuffer);
    if (status) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertPixelBuffer, CVPixelBufferPoolCreatePixelBuffer failed with error %d", static_cast<int>(status));
        return nullptr;
    }
    auto result = adoptCF(outputBuffer);

    auto err = VTPixelTransferSessionTransferImage(m_transferSession.get(), sourceBuffer, outputBuffer);
    if (err) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertPixelBuffer, VTPixelTransferSessionTransferImage failed with error %d", static_cast<int>(err));
        return nullptr;
    }

    return result;
}

RetainPtr<CVPixelBufferRef> ImageTransferSessionVT::createPixelBuffer(CMSampleBufferRef sourceBuffer, const IntSize& size)
{
    return convertPixelBuffer(CMSampleBufferGetImageBuffer(sourceBuffer), size);
}

RetainPtr<CMSampleBufferRef> ImageTransferSessionVT::convertCMSampleBuffer(CMSampleBufferRef sourceBuffer, const IntSize& size)
{
    if (!sourceBuffer)
        return nullptr;

    auto description = CMSampleBufferGetFormatDescription(sourceBuffer);
    auto sourceSize = FloatSize(CMVideoFormatDescriptionGetPresentationDimensions(description, true, true));
    auto pixelBuffer = static_cast<CVPixelBufferRef>(CMSampleBufferGetImageBuffer(sourceBuffer));
    if (size == roundedIntSize(sourceSize) && m_pixelFormat == CVPixelBufferGetPixelFormatType(pixelBuffer))
        return retainPtr(sourceBuffer);

    if (!setSize(size))
        return nullptr;

    auto convertedPixelBuffer = createPixelBuffer(sourceBuffer, size);
    if (!convertedPixelBuffer)
        return nullptr;

    CMItemCount itemCount = 0;
    auto status = CMSampleBufferGetSampleTimingInfoArray(sourceBuffer, 1, nullptr, &itemCount);
    if (status != noErr) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertCMSampleBuffer: CMSampleBufferGetSampleTimingInfoArray failed with error code: %d", static_cast<int>(status));
        return nullptr;
    }
    Vector<CMSampleTimingInfo> timingInfoArray;
    CMSampleTimingInfo* timeingInfoPtr = nullptr;
    if (itemCount) {
        timingInfoArray.grow(itemCount);
        status = CMSampleBufferGetSampleTimingInfoArray(sourceBuffer, itemCount, timingInfoArray.data(), nullptr);
        if (status != noErr) {
            RELEASE_LOG(Media, "ImageTransferSessionVT::convertCMSampleBuffer: CMSampleBufferGetSampleTimingInfoArray failed with error code: %d", static_cast<int>(status));
            return nullptr;
        }
        timeingInfoPtr = timingInfoArray.data();
    }

    CMVideoFormatDescriptionRef formatDescription = nullptr;
    status = CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, convertedPixelBuffer.get(), &formatDescription);
    if (status != noErr) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertCMSampleBuffer: CMVideoFormatDescriptionCreateForImageBuffer returned: %d", static_cast<int>(status));
        return nullptr;
    }

    CMSampleBufferRef resizedSampleBuffer;
    status = CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, convertedPixelBuffer.get(), formatDescription, timeingInfoPtr, &resizedSampleBuffer);
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
    auto context = CGBitmapContextCreate(data, imageSize.width(), imageSize.height(), 8, CVPixelBufferGetBytesPerRow(rgbBuffer), sRGBColorSpaceRef(), (CGBitmapInfo) kCGImageAlphaNoneSkipFirst);
    if (!context) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::createPixelBuffer: CGBitmapContextCreate returned nullptr");
        return nullptr;
    }

    auto retainedContext = adoptCF(context);
    CGContextDrawImage(context, CGRectMake(0, 0, imageSize.width(), imageSize.height()), image);
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
    auto status = CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)inputBuffer.get(), &formatDescription);
    if (status) {
        RELEASE_LOG(Media, "ImageTransferSessionVT::convertPixelBuffer: failed to initialize CMVideoFormatDescription with error code: %d", static_cast<int>(status));
        return nullptr;
    }

    CMSampleBufferRef sampleBuffer;
    auto cmTime = toCMTime(sampleTime);
    CMSampleTimingInfo timingInfo = { kCMTimeInvalid, cmTime, cmTime };
    status = CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)inputBuffer.get(), formatDescription, &timingInfo, &sampleBuffer);
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

#if HAVE(IOSURFACE) && !PLATFORM(IOSMAC)

#if PLATFORM(MAC)
static int32_t roundUpToMacroblockMultiple(int32_t size)
{
    return (size + 15) & ~15;
}
#endif

CFDictionaryRef ImageTransferSessionVT::ioSurfacePixelBufferCreationOptions(IOSurfaceRef surface)
{
    if (m_ioSurfaceBufferAttributes)
        return m_ioSurfaceBufferAttributes.get();

    m_ioSurfaceBufferAttributes = (__bridge CFDictionaryRef) @{
        (__bridge NSString *)cvPixelFormatOpenGLKey() : @(YES),
    };

#if PLATFORM(MAC)
    auto format = IOSurfaceGetPixelFormat(surface);
    auto width = IOSurfaceGetWidth(surface);
    auto height = IOSurfaceGetHeight(surface);
    auto extendedRight = roundUpToMacroblockMultiple(width) - width;
    auto extendedBottom = roundUpToMacroblockMultiple(height) - height;

    if ((format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange || format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)
        && (IOSurfaceGetBytesPerRowOfPlane(surface, 0) >= width + extendedRight)
        && (IOSurfaceGetBytesPerRowOfPlane(surface, 1) >= width + extendedRight)
        && (IOSurfaceGetAllocSize(surface) >= (height + extendedBottom) * IOSurfaceGetBytesPerRowOfPlane(surface, 0) * 3 / 2)) {
            m_ioSurfaceBufferAttributes = (__bridge CFDictionaryRef) @{
                (__bridge NSString *)kCVPixelBufferOpenGLCompatibilityKey : @(YES),
                (__bridge NSString *)kCVPixelBufferExtendedPixelsRightKey : @(extendedRight),
                (__bridge NSString *)kCVPixelBufferExtendedPixelsBottomKey : @(extendedBottom)
            };
    }
#else
    UNUSED_PARAM(surface);
#endif

    return m_ioSurfaceBufferAttributes.get();
}

RetainPtr<CVPixelBufferRef> ImageTransferSessionVT::createPixelBuffer(IOSurfaceRef surface, const IntSize& size)
{
    if (!surface || !setSize(size))
        return nullptr;

    CVPixelBufferRef pixelBuffer;
    auto status = CVPixelBufferCreateWithIOSurface(kCFAllocatorDefault, surface, ioSurfacePixelBufferCreationOptions(surface), &pixelBuffer);
    if (status) {
        RELEASE_LOG(Media, "CVPixelBufferCreateWithIOSurface failed with error code: %d", static_cast<int>(status));
        return nullptr;
    }

    auto retainedBuffer = adoptCF(pixelBuffer);
    if (m_size == IntSize(CVPixelBufferGetWidth(pixelBuffer), CVPixelBufferGetHeight(pixelBuffer)) && m_pixelFormat == CVPixelBufferGetPixelFormatType(pixelBuffer))
        return retainedBuffer;

    return convertPixelBuffer(pixelBuffer, size);
}

RetainPtr<CMSampleBufferRef> ImageTransferSessionVT::createCMSampleBuffer(IOSurfaceRef surface, const MediaTime& sampleTime, const IntSize& size)
{
    auto pixelBuffer = createPixelBuffer(surface, size);
    if (!pixelBuffer)
        return nullptr;

    return createCMSampleBuffer(pixelBuffer.get(), sampleTime, size);
}
#endif

RefPtr<MediaSample> ImageTransferSessionVT::convertMediaSample(MediaSample& sample, const IntSize& size)
{
    ASSERT(sample.platformSample().type == PlatformSample::CMSampleBufferType);

    if (size == roundedIntSize(sample.presentationSize()))
        return &sample;

    auto resizedBuffer = convertCMSampleBuffer(sample.platformSample().sample.cmSampleBuffer, size);
    if (!resizedBuffer)
        return nullptr;

    return MediaSampleAVFObjC::create(resizedBuffer.get(), sample.videoRotation(), sample.videoMirrored());
}

#if HAVE(IOSURFACE) && !PLATFORM(IOSMAC)
RefPtr<MediaSample> ImageTransferSessionVT::createMediaSample(IOSurfaceRef surface, const MediaTime& sampleTime, const IntSize& size, MediaSample::VideoRotation rotation, bool mirrored)
{
    auto sampleBuffer = createCMSampleBuffer(surface, sampleTime, size);
    if (!sampleBuffer)
        return nullptr;

    return MediaSampleAVFObjC::create(sampleBuffer.get(), rotation, mirrored);
}
#endif

RefPtr<MediaSample> ImageTransferSessionVT::createMediaSample(CGImageRef image, const MediaTime& sampleTime, const IntSize& size, MediaSample::VideoRotation rotation, bool mirrored)
{
    auto sampleBuffer = createCMSampleBuffer(image, sampleTime, size);
    if (!sampleBuffer)
        return nullptr;

    return MediaSampleAVFObjC::create(sampleBuffer.get(), rotation, mirrored);
}

RefPtr<MediaSample> ImageTransferSessionVT::createMediaSample(CMSampleBufferRef buffer, const IntSize& size, MediaSample::VideoRotation rotation, bool mirrored)
{
    auto sampleBuffer = convertCMSampleBuffer(buffer, size);
    if (!sampleBuffer)
        return nullptr;

    return MediaSampleAVFObjC::create(sampleBuffer.get(), rotation, mirrored);
}

} // namespace WebCore

#endif // USE(VIDEOTOOLBOX)
