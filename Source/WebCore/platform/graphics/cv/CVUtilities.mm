/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "CVUtilities.h"

#import "ColorSpaceCG.h"
#import "IOSurface.h"
#import "ImageUtilities.h"
#import "Logging.h"
#import "RealtimeVideoUtilities.h"
#import <wtf/CheckedArithmetic.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cf/TypeCastsCF.h>

#import "CoreVideoSoftLink.h"

namespace WebCore {

static Expected<RetainPtr<CVPixelBufferPoolRef>, CVReturn> createBufferPool(unsigned minimumBufferCount, NSDictionary *pixelAttributes)
{
    NSDictionary *poolOptions = nullptr;
    if (minimumBufferCount) {
        poolOptions = @{
            (__bridge NSString *)kCVPixelBufferPoolMinimumBufferCountKey : @(minimumBufferCount)
        };
    }
    CVPixelBufferPoolRef pool = nullptr;
    auto status = CVPixelBufferPoolCreate(kCFAllocatorDefault, (__bridge CFDictionaryRef)poolOptions, (__bridge CFDictionaryRef)pixelAttributes, &pool);
    if (status != kCVReturnSuccess || !pool)
        return makeUnexpected(status);
    return adoptCF(pool);
}

Expected<RetainPtr<CVPixelBufferPoolRef>, CVReturn> createIOSurfaceCVPixelBufferPool(size_t width, size_t height, OSType format, unsigned minimumBufferCount, bool isCGImageCompatible)
{
    return createBufferPool(minimumBufferCount, @{
        (__bridge NSString *)kCVPixelBufferWidthKey : @(width),
        (__bridge NSString *)kCVPixelBufferHeightKey : @(height),
        (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey : @(format),
        (__bridge NSString *)kCVPixelBufferCGImageCompatibilityKey : isCGImageCompatible ? @YES : @NO,
#if PLATFORM(MAC)
        (__bridge NSString *)kCVPixelBufferOpenGLCompatibilityKey : @YES,
#endif
        (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{ }
    });
}

Expected<RetainPtr<CVPixelBufferPoolRef>, CVReturn> createInMemoryCVPixelBufferPool(size_t width, size_t height, OSType format, unsigned minimumBufferCount, bool isCGImageCompatible)
{
    return createBufferPool(minimumBufferCount, @{
        (__bridge NSString *)kCVPixelBufferWidthKey : @(width),
        (__bridge NSString *)kCVPixelBufferHeightKey : @(height),
        (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey : @(format),
        (__bridge NSString *)kCVPixelBufferCGImageCompatibilityKey : isCGImageCompatible ? @YES : @NO,
#if PLATFORM(MAC)
        (__bridge NSString *)kCVPixelBufferOpenGLCompatibilityKey : @YES,
#endif
    });
}

Expected<RetainPtr<CVPixelBufferPoolRef>, CVReturn> createCVPixelBufferPool(size_t width, size_t height, OSType format, unsigned minimumBufferCount, bool isCGImageCompatible, bool shouldUseIOSurfacePool)
{
    return shouldUseIOSurfacePool ? createIOSurfaceCVPixelBufferPool(width, height, format, minimumBufferCount, isCGImageCompatible) : createInMemoryCVPixelBufferPool(width, height, format, minimumBufferCount, isCGImageCompatible);
}

Expected<RetainPtr<CVPixelBufferRef>, CVReturn> createCVPixelBufferFromPool(CVPixelBufferPoolRef pixelBufferPool, unsigned maxBufferSize)
{
    CVPixelBufferRef pixelBuffer = nullptr;
    CVReturn status;
    if (!maxBufferSize)
        status = CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, pixelBufferPool, &pixelBuffer);
    else {
        auto *auxiliaryAttributes = @{ (__bridge NSString *)kCVPixelBufferPoolAllocationThresholdKey : @(maxBufferSize) };
        status = CVPixelBufferPoolCreatePixelBufferWithAuxAttributes(kCFAllocatorDefault, pixelBufferPool, (__bridge CFDictionaryRef)auxiliaryAttributes, &pixelBuffer);
    }
    if (status != kCVReturnSuccess || !pixelBuffer)
        return makeUnexpected(status);
    return adoptCF(pixelBuffer);
}

static CFDictionaryRef pixelBufferCreationOptions(IOSurfaceRef surface)
{
#if PLATFORM(MAC)
    auto format = IOSurfaceGetPixelFormat(surface);
    if (format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange || format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
        // YUV formats might contain extra pixels due to block size. Define the picture position
        // in the buffer as the default top-left position. The caller might use the pixel buffer
        // with APIs that expect the properties.
        constexpr size_t macroblockSize = 16;
        auto width = IOSurfaceGetWidth(surface);
        auto height = IOSurfaceGetHeight(surface);
        auto extendedRight = roundUpToMultipleOf(macroblockSize, width) - width;
        auto extendedBottom = roundUpToMultipleOf(macroblockSize, height) - height;
        if ((IOSurfaceGetBytesPerRowOfPlane(surface, 0) >= width + extendedRight)
            && (IOSurfaceGetBytesPerRowOfPlane(surface, 1) >= width + extendedRight)
            && (IOSurfaceGetAllocSize(surface) >= (height + extendedBottom) * IOSurfaceGetBytesPerRowOfPlane(surface, 0) * 3 / 2)) {
            return (__bridge CFDictionaryRef) @{
                (__bridge NSString *)kCVPixelBufferOpenGLCompatibilityKey : @YES,
                (__bridge NSString *)kCVPixelBufferExtendedPixelsRightKey : @(extendedRight),
                (__bridge NSString *)kCVPixelBufferExtendedPixelsBottomKey : @(extendedBottom)
            };
        }
    }
#else
    UNUSED_PARAM(surface);
#endif
    return (__bridge CFDictionaryRef) @{
#if PLATFORM(MAC)
        (__bridge NSString *)kCVPixelBufferOpenGLCompatibilityKey : @YES
#endif
    };
}

Expected<RetainPtr<CVPixelBufferRef>, CVReturn> createCVPixelBuffer(IOSurfaceRef surface)
{
    CVPixelBufferRef pixelBuffer = nullptr;
    auto status = CVPixelBufferCreateWithIOSurface(kCFAllocatorDefault, surface, RetainPtr { pixelBufferCreationOptions(surface) }.get(), &pixelBuffer);
    if (status != kCVReturnSuccess || !pixelBuffer) {
        RELEASE_LOG_ERROR(WebRTC, "createCVPixelBuffer failed with IOSurface status=%d, pixelBuffer=%p", (int)status, pixelBuffer);
        return makeUnexpected(status);
    }
    return adoptCF(pixelBuffer);
}

RetainPtr<CGColorSpaceRef> createCGColorSpaceForCVPixelBuffer(CVPixelBufferRef buffer)
{
    if (RetainPtr colorSpace = dynamic_cf_cast<CGColorSpaceRef>(CVBufferGetAttachment(buffer, kCVImageBufferCGColorSpaceKey, nullptr)))
        return colorSpace;

    RetainPtr<CFDictionaryRef> attachments;
#if HAVE(CVBUFFERCOPYATTACHMENTS)
    attachments = adoptCF(CVBufferCopyAttachments(buffer, kCVAttachmentMode_ShouldPropagate));
#else
    attachments = CVBufferGetAttachments(buffer, kCVAttachmentMode_ShouldPropagate);
#endif
    if (auto colorSpace = adoptCF(CVImageBufferCreateColorSpaceFromAttachments(attachments.get())))
        return colorSpace;

    // We should only get here with content that has a broken embedded ICC
    // profile; in all other cases VideoToolbox should have put either known
    // accurate or guessed color space attachments on the pixel buffer. Content
    // that requires an embedded ICC profile is unlikely to be presented
    // correctly with any particular fallback color space we choose, so we
    // choose sRGB for ease.
    return sRGBColorSpaceSingleton();
}

void setOwnershipIdentityForCVPixelBuffer(CVPixelBufferRef pixelBuffer, const ProcessIdentity& owner)
{
    RetainPtr surface = CVPixelBufferGetIOSurface(pixelBuffer);
    ASSERT(surface);
    IOSurface::setOwnershipIdentity(surface.get(), owner);
}

RetainPtr<CVPixelBufferRef> createBlackPixelBuffer(size_t width, size_t height, bool shouldUseIOSurface)
{
    OSType format = preferedPixelBufferFormat();
    ASSERT(format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange || format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);

    NSDictionary *pixelAttributes = @{ (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{ } };

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=300264 - Creating very large CVPixelBuffers should terminate the IPC connection
    size_t widthTimesHeight;
    constexpr size_t maxIOSurfaceWidth = 1 << 15;
    if (!WTF::safeMultiply(width, height, widthTimesHeight) || widthTimesHeight > maxIOSurfaceWidth * maxIOSurfaceWidth)
        return nullptr;

    CVPixelBufferRef pixelBuffer = nullptr;
    auto status = CVPixelBufferCreate(kCFAllocatorDefault, width, height, format, shouldUseIOSurface ? (__bridge CFDictionaryRef)pixelAttributes : nullptr, &pixelBuffer);
    if (status != noErr || !pixelBuffer)
        return nullptr;

    status = CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    ASSERT(status == noErr);

    auto yPlane = CVPixelBufferGetSpanOfPlane(pixelBuffer, 0);
    size_t yStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
    for (unsigned i = 0; i < height; ++i)
        zeroSpan(yPlane.subspan(i * yStride, width));

    auto uvPlane = CVPixelBufferGetSpanOfPlane(pixelBuffer, 1);
    size_t uvStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);
    for (unsigned i = 0; i < height / 2; ++i)
        memsetSpan(uvPlane.subspan(i * uvStride, width), 128);

    status = CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    ASSERT(!status);
    return adoptCF(pixelBuffer);
}

namespace {

class CVPixelBufferDataProviderInfo {
    WTF_MAKE_NONCOPYABLE(CVPixelBufferDataProviderInfo);
public:
    static RetainPtr<CGDataProviderRef> createDataProvider(RetainPtr<CVPixelBufferRef>&&);

private:
    CVPixelBufferDataProviderInfo(RetainPtr<CVPixelBufferRef>&& pixelBuffer)
        : m_pixelBuffer(WTF::move(pixelBuffer))
    {
    }
    ~CVPixelBufferDataProviderInfo();
    const void* getBytePointer();
    void releaseBytePointer();

    static const void* getBytePointerCallback(void* info) { RELEASE_ASSERT(info); return static_cast<CVPixelBufferDataProviderInfo*>(info)->getBytePointer(); }
    static void releaseBytePointerCallback(void* info, const void*) { RELEASE_ASSERT(info); static_cast<CVPixelBufferDataProviderInfo*>(info)->releaseBytePointer(); }
    static void releaseInfoCallback(void* info) { RELEASE_ASSERT(info); delete static_cast<CVPixelBufferDataProviderInfo*>(info); }

    const RetainPtr<CVPixelBufferRef> m_pixelBuffer;
    unsigned m_lockCount { 0 };
};

RetainPtr<CGDataProviderRef> CVPixelBufferDataProviderInfo::createDataProvider(RetainPtr<CVPixelBufferRef>&& pixelBuffer)
{
    if (CVPixelBufferGetPixelFormatType(pixelBuffer.get()) != kCVPixelFormatType_32BGRA)
        return nullptr;
    auto dataSize = CVPixelBufferGetDataSize(pixelBuffer.get());
    if (!dataSize)
        return nullptr;
    CVPixelBufferDataProviderInfo* info = new CVPixelBufferDataProviderInfo(WTF::move(pixelBuffer));
    CGDataProviderDirectCallbacks providerCallbacks = { 0, getBytePointerCallback, releaseBytePointerCallback, 0, releaseInfoCallback };
    return adoptCF(CGDataProviderCreateDirect(info, dataSize, &providerCallbacks));
}

CVPixelBufferDataProviderInfo::~CVPixelBufferDataProviderInfo()
{
    if (!m_lockCount)
        return;
    RELEASE_LOG_ERROR(Media, "lockCount != 0: %d", m_lockCount);
    ASSERT_NOT_REACHED();
    // To avoid UAF, we do not unlock the pixel buffer.
}

const void* CVPixelBufferDataProviderInfo::getBytePointer()
{
    auto result = CVPixelBufferLockBaseAddress(m_pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    if (result != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferLockBaseAddress() error: %d", result);
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    ++m_lockCount;
    auto bytes = CVPixelBufferGetSpan(m_pixelBuffer.get());
    if (!bytes.data()) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferGetSpan() null");
        return nullptr;
    }
    verifyImageBufferIsBigEnough(bytes);
    return bytes.data();
}

void CVPixelBufferDataProviderInfo::releaseBytePointer()
{
    auto result = CVPixelBufferUnlockBaseAddress(m_pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    if (result != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferUnlockBaseAddress() error: %d", result);
        ASSERT_NOT_REACHED();
        return;
    }
    if (!m_lockCount) {
        RELEASE_LOG_ERROR(Media, "invalid releaseBytePointer()");
        ASSERT_NOT_REACHED();
        return;
    }
    --m_lockCount;
}

}

RetainPtr<CGImageRef> createImageFrom32BGRAPixelBuffer(RetainPtr<CVPixelBufferRef>&& buffer, CGColorSpaceRef colorSpace)
{
    if (!buffer)
        return nullptr;
    const CGBitmapInfo bitmapInfo = static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaFirst);
    const size_t bitsPerComponent = 8;
    const size_t bitsPerPixel = 32;
    const size_t width = CVPixelBufferGetWidth(buffer.get());
    const size_t height = CVPixelBufferGetHeight(buffer.get());
    const size_t bytesPerRow = CVPixelBufferGetBytesPerRow(buffer.get());
    RetainPtr provider = CVPixelBufferDataProviderInfo::createDataProvider(WTF::move(buffer));
    if (!provider)
        return nullptr;
    RetainPtr<CGImageRef> image = adoptCF(CGImageCreate(width, height, bitsPerComponent, bitsPerPixel, bytesPerRow, colorSpace, bitmapInfo, provider.get(), nullptr, false, kCGRenderingIntentDefault));
    if (!image)
        return nullptr;
    // For historical reasons, CoreAnimation will adjust certain video color
    // spaces when displaying the video. If the video frame derived image we
    // create here is drawn to an accelerated image buffer (e.g. for a canvas),
    // CA may not do this same adjustment, resulting in the canvas pixels not
    // matching the source video. Setting this CGImage property (despite the
    // image not being IOSurface backed), avoids this non-adjustment of the
    // image color space. <rdar://88804270>
    CGImageSetProperty(image.get(), CFSTR("CA_IOSURFACE_IMAGE"), kCFBooleanTrue);
    return image;
}

}
