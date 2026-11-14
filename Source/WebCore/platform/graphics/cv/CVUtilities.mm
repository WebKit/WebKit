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
#import "RealtimeVideoUtilities.h"
#import <array>
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

namespace {

struct YCbCrBiPlanarLayout {
    const bool isTenBit { false };
    const bool isFullRange { false };
    const bool isChroma422 { false };
};

}

PixelBufferLock::PixelBufferLock(CVPixelBufferRef pixelBuffer, CVPixelBufferLockFlags flags)
    : m_pixelBuffer(pixelBuffer)
    , m_flags(flags)
    , m_isLocked(CVPixelBufferLockBaseAddress(pixelBuffer, flags) == kCVReturnSuccess)
{
}

PixelBufferLock::~PixelBufferLock()
{
    if (m_isLocked)
        CVPixelBufferUnlockBaseAddress(m_pixelBuffer, m_flags);
}

static constexpr OSType pixelFormat422YpCbCr8VideoRange8ATriPlanar = 'v2a8';
static constexpr OSType pixelFormat420YpCbCr10VideoRange8ATriPlanar = 'x0a8';
static constexpr OSType pixelFormat422YpCbCr10VideoRange8ATriPlanar = 'x2a8';

static constexpr std::array<OSType, 4> alphaPlanePixelFormats {
    kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar,
    pixelFormat420YpCbCr10VideoRange8ATriPlanar,
    pixelFormat422YpCbCr8VideoRange8ATriPlanar,
    pixelFormat422YpCbCr10VideoRange8ATriPlanar,
};

// VideoToolbox tags the frames it decodes into these formats as opaque.
static void setStraightAlpha(CVPixelBufferRef pixelBuffer)
{
    CVBufferSetAttachment(pixelBuffer, kCVImageBufferAlphaChannelModeKey, kCVImageBufferAlphaChannelMode_StraightAlpha, kCVAttachmentMode_ShouldPropagate);
    CVBufferRemoveAttachment(pixelBuffer, kCVImageBufferAlphaChannelIsOpaque);
}

static std::optional<YCbCrBiPlanarLayout> yCbCrBiPlanarLayout(OSType format)
{
    switch (format) {
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        return YCbCrBiPlanarLayout { };
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
        return YCbCrBiPlanarLayout { .isFullRange = true };
    case kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange:
        return YCbCrBiPlanarLayout { .isChroma422 = true };
    case kCVPixelFormatType_422YpCbCr8BiPlanarFullRange:
        return YCbCrBiPlanarLayout { .isFullRange = true, .isChroma422 = true };
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
        return YCbCrBiPlanarLayout { .isTenBit = true };
    case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
        return YCbCrBiPlanarLayout { .isTenBit = true, .isFullRange = true };
    case kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange:
        return YCbCrBiPlanarLayout { .isTenBit = true, .isChroma422 = true };
    case kCVPixelFormatType_422YpCbCr10BiPlanarFullRange:
        return YCbCrBiPlanarLayout { .isTenBit = true, .isFullRange = true, .isChroma422 = true };
    default:
        return { };
    }
}

std::optional<OSType> pixelFormatWithAlpha(OSType colorFormat)
{
    auto layout = yCbCrBiPlanarLayout(colorFormat);
    if (!layout)
        return { };
    // These formats are video range only; 8-bit full range is widened to 10 bits to keep every level distinct.
    bool isTenBitOutput = layout->isTenBit || layout->isFullRange;
    if (layout->isChroma422)
        return isTenBitOutput ? pixelFormat422YpCbCr10VideoRange8ATriPlanar : pixelFormat422YpCbCr8VideoRange8ATriPlanar;
    return isTenBitOutput ? pixelFormat420YpCbCr10VideoRange8ATriPlanar : kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar;
}

std::span<const OSType> pixelFormatsWithAlphaPlane()
{
    return alphaPlanePixelFormats;
}

bool isPixelFormatWithAlphaPlane(OSType format)
{
    return std::ranges::find(alphaPlanePixelFormats, format) != alphaPlanePixelFormats.end();
}

// ITU-T H.273 full to video range scaling.
template<size_t levels>
static constexpr std::array<uint16_t, levels> fullToTenBitVideoRangeTable(bool isChroma)
{
    std::array<uint16_t, levels> table { };
    constexpr int maximum = levels - 1;
    constexpr int center = levels / 2;
    for (int value = 0; value < static_cast<int>(levels); ++value) {
        int mapped;
        if (isChroma) {
            int scaled = (value - center) * 896;
            mapped = 512 + (scaled >= 0 ? (scaled + maximum / 2) / maximum : (scaled - maximum / 2) / maximum);
        } else
            mapped = 64 + (value * 876 + maximum / 2) / maximum;
        table[value] = static_cast<uint16_t>(std::clamp(mapped, 0, 1023) << 6);
    }
    return table;
}

static constexpr auto eightBitFullToTenBitVideoLuma = fullToTenBitVideoRangeTable<256>(false);
static constexpr auto eightBitFullToTenBitVideoChroma = fullToTenBitVideoRangeTable<256>(true);
static constexpr auto tenBitFullToTenBitVideoLuma = fullToTenBitVideoRangeTable<1024>(false);
static constexpr auto tenBitFullToTenBitVideoChroma = fullToTenBitVideoRangeTable<1024>(true);

static void copyRow(std::span<uint8_t> destination, std::span<const uint8_t> source, size_t sampleCount, const YCbCrBiPlanarLayout& layout, bool isChroma)
{
    if (!layout.isFullRange) {
        size_t byteCount = sampleCount * (layout.isTenBit ? 2 : 1);
        memcpySpan(destination.first(byteCount), source.first(byteCount));
        return;
    }
    auto destinationSamples = spanReinterpretCast<uint16_t>(destination.first(2 * sampleCount));
    if (layout.isTenBit) {
        auto& table = isChroma ? tenBitFullToTenBitVideoChroma : tenBitFullToTenBitVideoLuma;
        auto sourceSamples = spanReinterpretCast<const uint16_t>(source.first(2 * sampleCount));
        for (size_t index = 0; index < sampleCount; ++index)
            destinationSamples[index] = table[sourceSamples[index] >> 6];
        return;
    }
    auto& table = isChroma ? eightBitFullToTenBitVideoChroma : eightBitFullToTenBitVideoLuma;
    auto sourceSamples = source.first(sampleCount);
    for (size_t index = 0; index < sampleCount; ++index)
        destinationSamples[index] = table[sourceSamples[index]];
}

static bool copyAlphaPlane(std::span<uint8_t> destination, size_t destinationBytesPerRow, CVPixelBufferRef alphaBuffer, size_t width, size_t height)
{
    auto alphaLayout = yCbCrBiPlanarLayout(CVPixelBufferGetPixelFormatType(alphaBuffer));
    if (!alphaLayout)
        return false;

    auto alphaPlane = CVPixelBufferGetSpanOfPlane(alphaBuffer, 0);
    size_t alphaBytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(alphaBuffer, 0);
    size_t alphaBytesPerSample = alphaLayout->isTenBit ? 2 : 1;
    if (alphaPlane.size() < height * alphaBytesPerRow || alphaBytesPerRow < width * alphaBytesPerSample)
        return false;

    // Alpha coverage spans the whole sample range whatever range the plane is tagged with.
    for (size_t y = 0; y < height; ++y) {
        auto source = alphaPlane.subspan(y * alphaBytesPerRow, width * alphaBytesPerSample);
        auto row = destination.subspan(y * destinationBytesPerRow, width);
        if (!alphaLayout->isTenBit) {
            memcpySpan(row, source);
            continue;
        }
        for (size_t x = 0; x < width; ++x)
            row[x] = source[2 * x + 1];
    }
    return true;
}

static bool copyColorPlanes(CVPixelBufferRef source, CVPixelBufferRef destination, const YCbCrBiPlanarLayout& layout, size_t width, size_t height)
{
    size_t sourceSampleBytes = layout.isTenBit ? 2 : 1;
    size_t destinationSampleBytes = layout.isTenBit || layout.isFullRange ? 2 : 1;
    size_t chromaSamplesPerRow = 2 * ((width + 1) / 2);
    size_t chromaHeight = layout.isChroma422 ? height : (height + 1) / 2;

    struct Plane {
        size_t samplesPerRow;
        size_t rows;
        bool isChroma;
    };
    for (auto [index, plane] : std::array { std::pair { 0, Plane { width, height, false } }, std::pair { 1, Plane { chromaSamplesPerRow, chromaHeight, true } } }) {
        auto sourcePlane = CVPixelBufferGetSpanOfPlane(source, index);
        size_t sourceBytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(source, index);
        auto destinationPlane = CVPixelBufferGetSpanOfPlane(destination, index);
        size_t destinationBytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(destination, index);
        if (sourcePlane.size() < plane.rows * sourceBytesPerRow || sourceBytesPerRow < plane.samplesPerRow * sourceSampleBytes
            || destinationPlane.size() < plane.rows * destinationBytesPerRow || destinationBytesPerRow < plane.samplesPerRow * destinationSampleBytes)
            return false;
        for (size_t y = 0; y < plane.rows; ++y)
            copyRow(destinationPlane.subspan(y * destinationBytesPerRow), sourcePlane.subspan(y * sourceBytesPerRow), plane.samplesPerRow, layout, plane.isChroma);
    }
    return true;
}

std::expected<RetainPtr<CVPixelBufferRef>, CVReturn> createPixelBufferWithAlpha(CVPixelBufferRef colorBuffer, CVPixelBufferRef alphaBuffer, CVPixelBufferPoolRef pool)
{
    if (!colorBuffer || !alphaBuffer || !pool)
        return RetainPtr<CVPixelBufferRef> { };

    auto colorFormat = CVPixelBufferGetPixelFormatType(colorBuffer);
    auto layout = yCbCrBiPlanarLayout(colorFormat);
    if (!layout) {
        RELEASE_LOG_ERROR(Media, "createPixelBufferWithAlpha: unsupported colour format %" PUBLIC_LOG_STRING, FourCC(colorFormat).string().data());
        return RetainPtr<CVPixelBufferRef> { };
    }

    auto outputBuffer = createCVPixelBufferFromPool(pool);
    if (!outputBuffer) {
        RELEASE_LOG_ERROR(Media, "createPixelBufferWithAlpha: CVPixelBufferPool allocation failed (status=%d)", int(outputBuffer.error()));
        return makeUnexpected(outputBuffer.error());
    }
    RetainPtr output = WTF::move(*outputBuffer);

    {
        PixelBufferLock colorLock { colorBuffer, kCVPixelBufferLock_ReadOnly };
        PixelBufferLock outputLock { output, 0 };
        if (!colorLock || !outputLock || !copyColorPlanes(colorBuffer, output, *layout, CVPixelBufferGetWidth(colorBuffer), CVPixelBufferGetHeight(colorBuffer))) {
            RELEASE_LOG_ERROR(Media, "createPixelBufferWithAlpha: plane copy failed for colour format %" PUBLIC_LOG_STRING, FourCC(colorFormat).string().data());
            return RetainPtr<CVPixelBufferRef> { };
        }
    }

    CVBufferPropagateAttachments(colorBuffer, output);
    if (!copyAlphaIntoAlphaPlane(output, alphaBuffer))
        return RetainPtr<CVPixelBufferRef> { };
    return output;
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

    PixelBufferLock alphaLock { alphaBuffer, kCVPixelBufferLock_ReadOnly };
    PixelBufferLock destinationLock { destination, 0 };
    if (!alphaLock || !destinationLock)
        return false;

    auto alphaPlane = CVPixelBufferGetSpanOfPlane(destination, 2);
    size_t alphaBytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(destination, 2);
    if (alphaPlane.size() < height * alphaBytesPerRow || alphaBytesPerRow < width)
        return false;
    if (!copyAlphaPlane(alphaPlane, alphaBytesPerRow, alphaBuffer, width, height)) {
        RELEASE_LOG_ERROR(Media, "copyAlphaIntoAlphaPlane: unsupported alpha format %" PUBLIC_LOG_STRING, FourCC(CVPixelBufferGetPixelFormatType(alphaBuffer)).string().data());
        return false;
    }

    setStraightAlpha(destination);
    return true;
}

RetainPtr<CVPixelBufferRef> createBiPlanarPixelBufferFromColorPlanes(CVPixelBufferRef source)
{
    auto sourceFormat = CVPixelBufferGetPixelFormatType(source);
    if (sourceFormat != pixelFormat420YpCbCr10VideoRange8ATriPlanar && sourceFormat != pixelFormat422YpCbCr10VideoRange8ATriPlanar)
        return nullptr;
    YCbCrBiPlanarLayout layout { .isTenBit = true, .isChroma422 = sourceFormat == pixelFormat422YpCbCr10VideoRange8ATriPlanar };
    OSType colorFormat = layout.isChroma422 ? kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange : kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;

    size_t width = CVPixelBufferGetWidth(source);
    size_t height = CVPixelBufferGetHeight(source);
    NSDictionary *attributes = CVPixelBufferGetIOSurface(source) ? @{ (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{ } } : nil;
    CVPixelBufferRef rawOutput = nullptr;
    if (CVPixelBufferCreate(kCFAllocatorDefault, width, height, colorFormat, (__bridge CFDictionaryRef)attributes, &rawOutput) != kCVReturnSuccess || !rawOutput)
        return nullptr;
    RetainPtr output = adoptCF(rawOutput);

    PixelBufferLock sourceLock { source, kCVPixelBufferLock_ReadOnly };
    PixelBufferLock outputLock { output, 0 };
    if (!sourceLock || !outputLock || !copyColorPlanes(source, output, layout, width, height))
        return nullptr;

    CVBufferPropagateAttachments(source, output);
    CVBufferRemoveAttachment(output, kCVImageBufferAlphaChannelModeKey);
    return output;
}

bool copyAlphaPlaneToBGRAPixelBuffer(CVPixelBufferRef source, CVPixelBufferRef destination)
{
    if (!isPixelFormatWithAlphaPlane(CVPixelBufferGetPixelFormatType(source)) || CVPixelBufferGetPixelFormatType(destination) != kCVPixelFormatType_32BGRA)
        return false;
    size_t width = CVPixelBufferGetWidth(source);
    size_t height = CVPixelBufferGetHeight(source);
    if (width != CVPixelBufferGetWidth(destination) || height != CVPixelBufferGetHeight(destination))
        return false;

    PixelBufferLock sourceLock { source, kCVPixelBufferLock_ReadOnly };
    PixelBufferLock destinationLock { destination, 0 };
    if (!sourceLock || !destinationLock)
        return false;

    auto alphaPlane = CVPixelBufferGetSpanOfPlane(source, 2);
    size_t alphaBytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(source, 2);
    auto pixels = CVPixelBufferGetSpan(destination);
    size_t pixelBytesPerRow = CVPixelBufferGetBytesPerRow(destination);
    if (alphaPlane.size() < height * alphaBytesPerRow || alphaBytesPerRow < width || pixels.size() < height * pixelBytesPerRow || pixelBytesPerRow < 4 * width)
        return false;

    for (size_t y = 0; y < height; ++y) {
        auto alpha = alphaPlane.subspan(y * alphaBytesPerRow, width);
        auto row = pixels.subspan(y * pixelBytesPerRow, 4 * width);
        for (size_t x = 0; x < width; ++x)
            row[4 * x + 3] = alpha[x];
    }
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
