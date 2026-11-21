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
#import "FourCC.h"
#import "IOSurface.h"
#import "ImageUtilities.h"
#import "Logging.h"
#import "PlanarImageConverterCV.h"
#import "RealtimeVideoUtilities.h"
#import <array>
#import <pal/spi/cf/CoreVideoSPI.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <wtf/CheckedArithmetic.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cf/TypeCastsCF.h>

#import "CoreVideoSoftLink.h"

namespace WebCore {

static std::expected<RetainPtr<CVPixelBufferPoolRef>, CVReturn> createBufferPool(unsigned minimumBufferCount, NSDictionary *pixelAttributes)
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

std::expected<RetainPtr<CVPixelBufferPoolRef>, CVReturn> createIOSurfaceCVPixelBufferPool(size_t width, size_t height, OSType format, unsigned minimumBufferCount, bool isCGImageCompatible)
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

std::expected<RetainPtr<CVPixelBufferPoolRef>, CVReturn> createInMemoryCVPixelBufferPool(size_t width, size_t height, OSType format, unsigned minimumBufferCount, bool isCGImageCompatible)
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

std::expected<RetainPtr<CVPixelBufferPoolRef>, CVReturn> createCVPixelBufferPool(size_t width, size_t height, OSType format, unsigned minimumBufferCount, bool isCGImageCompatible, bool shouldUseIOSurfacePool)
{
    return shouldUseIOSurfacePool ? createIOSurfaceCVPixelBufferPool(width, height, format, minimumBufferCount, isCGImageCompatible) : createInMemoryCVPixelBufferPool(width, height, format, minimumBufferCount, isCGImageCompatible);
}

std::expected<RetainPtr<CVPixelBufferRef>, CVReturn> createCVPixelBufferFromPool(CVPixelBufferPoolRef pixelBufferPool, unsigned maxBufferSize)
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

std::expected<RetainPtr<CVPixelBufferRef>, CVReturn> createCVPixelBuffer(IOSurfaceRef surface)
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

CGImageAlphaInfo alphaInfoForCVPixelBuffer(CVPixelBufferRef buffer)
{
    RetainPtr alphaMode = dynamic_cf_cast<CFStringRef>(CVBufferGetAttachment(buffer, kCVImageBufferAlphaChannelModeKey, nullptr));
    if (alphaMode && CFEqual(alphaMode, kCVImageBufferAlphaChannelMode_PremultipliedAlpha))
        return kCGImageAlphaPremultipliedFirst;
    return kCGImageAlphaFirst;
}

void setAlphaInfoForCVPixelBuffer(CVPixelBufferRef pixelBuffer, CGImageAlphaInfo alphaInfo)
{
    CFStringRef alphaMode = nullptr;
    bool isOpaque = false;
    switch (alphaInfo) {
    case kCGImageAlphaFirst:
    case kCGImageAlphaLast:
        alphaMode = kCVImageBufferAlphaChannelMode_StraightAlpha;
        break;
    case kCGImageAlphaPremultipliedFirst:
    case kCGImageAlphaPremultipliedLast:
        alphaMode = kCVImageBufferAlphaChannelMode_PremultipliedAlpha;
        break;
    case kCGImageAlphaNone:
    case kCGImageAlphaNoneSkipFirst:
    case kCGImageAlphaNoneSkipLast:
        isOpaque = true;
        break;
    case kCGImageAlphaOnly:
        break;
    }

    if (alphaMode)
        CVBufferSetAttachment(pixelBuffer, kCVImageBufferAlphaChannelModeKey, alphaMode, kCVAttachmentMode_ShouldPropagate);
    else
        CVBufferRemoveAttachment(pixelBuffer, kCVImageBufferAlphaChannelModeKey);

    if (isOpaque)
        CVBufferSetAttachment(pixelBuffer, kCVImageBufferAlphaChannelIsOpaque, kCFBooleanTrue, kCVAttachmentMode_ShouldPropagate);
    else
        CVBufferRemoveAttachment(pixelBuffer, kCVImageBufferAlphaChannelIsOpaque);
}

static constexpr std::array<OSType, 4> alphaPlanePixelFormats {
    kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar,
    kCVPixelFormatType_420YpCbCr10VideoRange_8A_TriPlanar,
    kCVPixelFormatType_422YpCbCr8VideoRange_8A_TriPlanar,
    kCVPixelFormatType_422YpCbCr10VideoRange_8A_TriPlanar,
};

// VideoToolbox tags the frames it decodes into these formats as opaque.
static void setStraightAlpha(CVPixelBufferRef pixelBuffer)
{
    CVBufferSetAttachment(pixelBuffer, kCVImageBufferAlphaChannelModeKey, kCVImageBufferAlphaChannelMode_StraightAlpha, kCVAttachmentMode_ShouldPropagate);
    CVBufferRemoveAttachment(pixelBuffer, kCVImageBufferAlphaChannelIsOpaque);
}

std::optional<OSType> pixelFormatWithAlpha(OSType colorFormat)
{
    auto description = PlanarImageConverter::pixelFormatDescription(colorFormat);
    if (!description || description->alpha || description->subsampling == PlanarImageConverter::ChromaSubsampling::None)
        return { };
    // These formats are video range only; 8-bit full range is widened to 10 bits to keep every level distinct.
    bool isTenBitOutput = description->color.bitDepth > 8 || description->isFullRange;
    return PlanarImageConverter::pixelFormat(description->subsampling, isTenBitOutput ? PlanarImageConverter::tenBitMSB : PlanarImageConverter::eightBit, false, PlanarImageConverter::eightBit);
}

// pixelFormatWithAlpha() widens 8-bit full range samples to the 10-bit alpha formats.
static bool shouldTreatI420AsFullRange(OSType destinationFormat)
{
    auto description = PlanarImageConverter::pixelFormatDescription(destinationFormat);
    return description && (description->isFullRange || (description->alpha && description->color.bitDepth > 8));
}

std::span<const OSType> pixelFormatsWithAlphaPlane()
{
    return alphaPlanePixelFormats;
}

bool isPixelFormatWithAlphaPlane(OSType format)
{
    return std::ranges::find(alphaPlanePixelFormats, format) != alphaPlanePixelFormats.end();
}

std::optional<int> bitDepthOfPixelFormatWithAlphaPlane(OSType format)
{
    if (!isPixelFormatWithAlphaPlane(format))
        return { };
    if (auto description = PlanarImageConverter::pixelFormatDescription(format))
        return description->color.bitDepth;
    return { };
}

std::expected<RetainPtr<CVPixelBufferRef>, CVReturn> createPixelBufferWithAlpha(CVPixelBufferRef colorBuffer, CVPixelBufferRef alphaBuffer, CVPixelBufferPoolRef pool)
{
    if (!colorBuffer || !alphaBuffer || !pool)
        return RetainPtr<CVPixelBufferRef> { };

    auto colorFormat = CVPixelBufferGetPixelFormatType(colorBuffer);
    if (!pixelFormatWithAlpha(colorFormat)) {
        RELEASE_LOG_ERROR(Media, "createPixelBufferWithAlpha: unsupported colour format %" PUBLIC_LOG_STRING, FourCC(colorFormat).string().data());
        return RetainPtr<CVPixelBufferRef> { };
    }

    auto outputBuffer = createCVPixelBufferFromPool(pool);
    if (!outputBuffer) {
        RELEASE_LOG_ERROR(Media, "createPixelBufferWithAlpha: CVPixelBufferPool allocation failed (status=%d)", int(outputBuffer.error()));
        return makeUnexpected(outputBuffer.error());
    }
    RetainPtr output = WTF::move(*outputBuffer);

    if (!PlanarImageConverter::copyColorPlanes(colorBuffer, output)) {
        RELEASE_LOG_ERROR(Media, "createPixelBufferWithAlpha: plane copy failed for colour format %" PUBLIC_LOG_STRING, FourCC(colorFormat).string().data());
        return RetainPtr<CVPixelBufferRef> { };
    }

    CVBufferPropagateAttachments(colorBuffer, output);
    if (!copyAlphaIntoAlphaPlane(output, alphaBuffer))
        return RetainPtr<CVPixelBufferRef> { };
    return output;
}

bool copyI420ToPixelBuffer(std::span<const uint8_t> source, size_t width, size_t height, CVPixelBufferRef destination)
{
    auto format = CVPixelBufferGetPixelFormatType(destination);
    bool hasAlpha = format == kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar || format == kCVPixelFormatType_420YpCbCr10VideoRange_8A_TriPlanar;
    if (!hasAlpha && format != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange && format != kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)
        return false;

    size_t chromaWidth = (width + 1) / 2;
    size_t chromaHeight = (height + 1) / 2;
    size_t lumaSize = width * height;
    size_t chromaSize = chromaWidth * chromaHeight;
    if (!width || !height || source.size() != lumaSize + 2 * chromaSize + (hasAlpha ? lumaSize : 0))
        return false;

    auto y = PlanarImageConverter::makePlane(source, 0, width, width, height, PlanarImageConverter::eightBit);
    auto u = PlanarImageConverter::makePlane(source, lumaSize, chromaWidth, chromaWidth, chromaHeight, PlanarImageConverter::eightBit);
    auto v = PlanarImageConverter::makePlane(source, lumaSize + chromaSize, chromaWidth, chromaWidth, chromaHeight, PlanarImageConverter::eightBit);
    std::optional<PlanarImageConverter::SourcePlane> alpha;
    if (hasAlpha)
        alpha = PlanarImageConverter::makePlane(source, lumaSize + 2 * chromaSize, width, width, height, PlanarImageConverter::eightBit);
    if (!y || !u || !v || (hasAlpha && !alpha))
        return false;

    if (!PlanarImageConverter::copyFrame({ PlanarImageConverter::ChromaSubsampling::HorizontalAndVertical, shouldTreatI420AsFullRange(format), *y, *u, *v, alpha }, destination))
        return false;

    if (hasAlpha)
        setStraightAlpha(destination);
    return true;
}

bool copyAlphaIntoAlphaPlane(CVPixelBufferRef destination, CVPixelBufferRef alphaBuffer)
{
    if (!destination || !alphaBuffer || !isPixelFormatWithAlphaPlane(CVPixelBufferGetPixelFormatType(destination)))
        return false;

    size_t width = CVPixelBufferGetWidth(destination);
    size_t height = CVPixelBufferGetHeight(destination);
    if (width != CVPixelBufferGetWidth(alphaBuffer) || height != CVPixelBufferGetHeight(alphaBuffer)) {
        RELEASE_LOG_ERROR(Media, "copyAlphaIntoAlphaPlane: colour (%zux%zu) and alpha (%zux%zu) dimensions differ", width, height, CVPixelBufferGetWidth(alphaBuffer), CVPixelBufferGetHeight(alphaBuffer));
        return false;
    }

    if (!PlanarImageConverter::copyLumaToAlphaPlane(alphaBuffer, destination)) {
        RELEASE_LOG_ERROR(Media, "copyAlphaIntoAlphaPlane: unsupported alpha format %" PUBLIC_LOG_STRING, FourCC(CVPixelBufferGetPixelFormatType(alphaBuffer)).string().data());
        return false;
    }

    setStraightAlpha(destination);
    return true;
}

bool copyAlphaFromLumaPlane(CVPixelBufferRef alphaBuffer, std::span<uint8_t> destination, size_t destinationBytesPerRow)
{
    if (!alphaBuffer)
        return false;

    auto alphaPlane = PlanarImageConverter::makePlane(destination, 0, destinationBytesPerRow, CVPixelBufferGetWidth(alphaBuffer), CVPixelBufferGetHeight(alphaBuffer), PlanarImageConverter::eightBit);
    if (!alphaPlane)
        return false;

    if (!PlanarImageConverter::copyLumaToAlpha(alphaBuffer, *alphaPlane)) {
        RELEASE_LOG_ERROR(Media, "copyAlphaFromLumaPlane: unsupported alpha format %" PUBLIC_LOG_STRING, FourCC(CVPixelBufferGetPixelFormatType(alphaBuffer)).string().data());
        return false;
    }
    return true;
}

RetainPtr<CVPixelBufferRef> createBiPlanarPixelBufferFromColorPlanes(CVPixelBufferRef source)
{
    auto sourceFormat = CVPixelBufferGetPixelFormatType(source);
    if (sourceFormat != kCVPixelFormatType_420YpCbCr10VideoRange_8A_TriPlanar && sourceFormat != kCVPixelFormatType_422YpCbCr10VideoRange_8A_TriPlanar)
        return nullptr;
    auto description = PlanarImageConverter::pixelFormatDescription(sourceFormat);
    if (!description)
        return nullptr;
    auto colorFormat = PlanarImageConverter::pixelFormat(description->subsampling, description->color, description->isFullRange);
    if (!colorFormat)
        return nullptr;

    size_t width = CVPixelBufferGetWidth(source);
    size_t height = CVPixelBufferGetHeight(source);
    NSDictionary *attributes = CVPixelBufferGetIOSurface(source) ? @{ (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{ } } : nil;
    CVPixelBufferRef rawOutput = nullptr;
    if (CVPixelBufferCreate(kCFAllocatorDefault, width, height, *colorFormat, (__bridge CFDictionaryRef)attributes, &rawOutput) != kCVReturnSuccess || !rawOutput)
        return nullptr;
    RetainPtr output = adoptCF(rawOutput);

    if (!PlanarImageConverter::copyColorPlanes(source, output))
        return nullptr;

    CVBufferPropagateAttachments(source, output);
    CVBufferRemoveAttachment(output, kCVImageBufferAlphaChannelModeKey);
    return output;
}

bool copyAlphaPlaneToBGRAPixelBuffer(CVPixelBufferRef source, CVPixelBufferRef destination)
{
    if (!isPixelFormatWithAlphaPlane(CVPixelBufferGetPixelFormatType(source)) || !PlanarImageConverter::copyAlphaPlaneToBGRA(source, destination))
        return false;
    setStraightAlpha(destination);
    return true;
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

    // Full-range black is Y=0; video-range black is the luma floor Y=16.
    uint8_t blackLuma = format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ? 16 : 0;
    auto yPlane = CVPixelBufferGetSpanOfPlane(pixelBuffer, 0);
    size_t yStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
    for (unsigned i = 0; i < height; ++i)
        memsetSpan(yPlane.subspan(i * yStride, width), blackLuma);

    auto uvPlane = CVPixelBufferGetSpanOfPlane(pixelBuffer, 1);
    size_t uvStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);
    for (unsigned i = 0; i < height / 2; ++i)
        memsetSpan(uvPlane.subspan(i * uvStride, width), 128);

    status = CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    ASSERT(!status);
    return adoptCF(pixelBuffer);
}

}
