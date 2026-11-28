/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "PixelBufferConverterCV.h"

#import "CMUtilities.h"
#import "CVUtilities.h"
#import "Logging.h"
#import "PlanarImageConverterCV.h"
#import <wtf/TZoneMallocInlines.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PixelBufferConverter);

PixelBufferConverter::~PixelBufferConverter() = default;

static RetainPtr<VTPixelTransferSessionRef> createPixelTransferSession(bool shouldUseHardwareAcceleratedTransfer)
{
    VTPixelTransferSessionRef rawTransferSession = nullptr;
    auto status = VTPixelTransferSessionCreate(kCFAllocatorDefault, &rawTransferSession);
    if (status != kCVReturnSuccess)
        return nullptr;
    RetainPtr transferSession = adoptCF(rawTransferSession);
    auto setProperty = [&](CFStringRef key, CFTypeRef value) {
        auto status = VTSessionSetProperty(transferSession, key, value);
        RELEASE_LOG_ERROR_IF(status != kCVReturnSuccess, Media, "VTSessionSetProperty(%{public}@) error: %d", key, static_cast<int>(status));
    };
    setProperty(kVTPixelTransferPropertyKey_ScalingMode, kVTScalingMode_Trim);
    setProperty(kVTPixelTransferPropertyKey_EnableHighSpeedTransfer, @YES);
    setProperty(kVTPixelTransferPropertyKey_RealTime, @YES);
#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    if (shouldUseHardwareAcceleratedTransfer)
        setProperty(kVTPixelTransferPropertyKey_EnableHardwareAcceleratedTransfer, @YES);
#else
    UNUSED_PARAM(shouldUseHardwareAcceleratedTransfer);
#endif
    return transferSession;
}

OSType PixelBufferConverter::nearestCVPixelFormat(OSType pixelFormat)
{
    auto description = PlanarImageConverter::pixelFormatDescription(pixelFormat);
    if (!description || !description->alpha || (description->color == PlanarImageConverter::eightBit && !description->isFullRange))
        return pixelFormat;

    // CoreVideo has no 4:2:0 format with > 8-bit alpha.
    auto subsampling = description->subsampling == PlanarImageConverter::ChromaSubsampling::HorizontalAndVertical ? PlanarImageConverter::ChromaSubsampling::Horizontal : description->subsampling;
    auto nearestFormat = PlanarImageConverter::pixelFormat(subsampling, PlanarImageConverter::tenBitMSB, false, PlanarImageConverter::sixteenBit);
    ASSERT(nearestFormat);
    return nearestFormat.value_or(pixelFormat);
}

std::optional<OSType> PixelBufferConverter::pixelFormatWithAlpha(OSType pixelFormat)
{
    auto description = PlanarImageConverter::pixelFormatDescription(pixelFormat);
    if (!description)
        return { };
    if (description->alpha)
        return pixelFormat;

    if (auto format = PlanarImageConverter::pixelFormat(description->subsampling, description->color, description->isFullRange, description->color))
        return format;
    return PlanarImageConverter::pixelFormat(description->subsampling, description->color, description->isFullRange, PlanarImageConverter::sixteenBit);
}

std::optional<OSType> PixelBufferConverter::pixelFormatWithoutAlpha(OSType pixelFormat)
{
    auto description = PlanarImageConverter::pixelFormatDescription(pixelFormat);
    if (!description)
        return { };
    return PlanarImageConverter::pixelFormat(description->subsampling, description->color, description->isFullRange);
}

static bool hasAlphaPlane(OSType pixelFormat)
{
    auto description = PlanarImageConverter::pixelFormatDescription(pixelFormat);
    return description && description->alpha;
}

template<typename Function>
static void forEachColorProperty(const PlatformVideoColorSpace& colorSpace, NOESCAPE Function&& function)
{
    if (RetainPtr primaries = colorSpace.primaries ? convertToCMColorPrimaries(*colorSpace.primaries) : nullptr)
        function(kCVImageBufferColorPrimariesKey, kVTPixelTransferPropertyKey_DestinationColorPrimaries, primaries);
    if (RetainPtr transfer = colorSpace.transfer ? convertToCMTransferFunction(*colorSpace.transfer) : nullptr)
        function(kCVImageBufferTransferFunctionKey, kVTPixelTransferPropertyKey_DestinationTransferFunction, transfer);
    if (RetainPtr matrix = colorSpace.matrix ? convertToCMYCbCRMatrix(*colorSpace.matrix) : nullptr)
        function(kCVImageBufferYCbCrMatrixKey, kVTPixelTransferPropertyKey_DestinationYCbCrMatrix, matrix);
}

static bool hasColorSpace(CVPixelBufferRef pixelBuffer, const PlatformVideoColorSpace& colorSpace)
{
    bool matches = true;
    forEachColorProperty(colorSpace, [&](CFStringRef attachmentKey, CFStringRef, CFStringRef value) {
        if (!matches)
            return;
        RetainPtr attachment = CVBufferGetAttachment(pixelBuffer, attachmentKey, nullptr);
        matches = attachment && CFEqual(attachment, value);
    });
    return matches;
}

static bool canCopyPlanes(OSType sourceFormat, OSType destinationFormat)
{
    auto source = PlanarImageConverter::pixelFormatDescription(sourceFormat);
    auto destination = PlanarImageConverter::pixelFormatDescription(destinationFormat);
    return source && destination && source->subsampling == destination->subsampling && (source->alpha || !destination->alpha);
}

RetainPtr<CVPixelBufferRef> PixelBufferConverter::createPixelBuffer(Pool& pool, OSType pixelFormat, CVPixelBufferRef source)
{
    IntSize size { static_cast<int>(CVPixelBufferGetWidth(source)), static_cast<int>(CVPixelBufferGetHeight(source)) };
    ASSERT(m_useIOSurface == UseIOSurface::No || !PlanarImageConverter::isRegisteredPixelFormat(pixelFormat));
    bool usesIOSurface = m_useIOSurface == UseIOSurface::Yes && !PlanarImageConverter::isRegisteredPixelFormat(pixelFormat);
    if (m_usePixelBufferPool == UsePixelBufferPool::No) {
        NSDictionary *attributes = usesIOSurface ? @{ (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{ } } : nil;
        CVPixelBufferRef rawPixelBuffer = nullptr;
        if (CVPixelBufferCreate(kCFAllocatorDefault, size.width(), size.height(), pixelFormat, (__bridge CFDictionaryRef)attributes, &rawPixelBuffer) != kCVReturnSuccess || !rawPixelBuffer)
            return nullptr;
        return adoptCF(rawPixelBuffer);
    }

    if (!pool.pool || pool.pixelFormat != pixelFormat || pool.size != size) {
        auto newPool = createCVPixelBufferPool(size.width(), size.height(), pixelFormat, 0u, false, usesIOSurface);
        if (!newPool) {
            pool = { };
            return nullptr;
        }
        pool = { WTF::move(*newPool), pixelFormat, size };
    }

    auto pixelBuffer = createCVPixelBufferFromPool(pool.pool);
    if (!pixelBuffer)
        return nullptr;
    return WTF::move(*pixelBuffer);
}

RetainPtr<CVPixelBufferRef> PixelBufferConverter::copyPlanes(CVPixelBufferRef source, OSType destinationFormat, Pool& pool)
{
    RetainPtr destination = createPixelBuffer(pool, destinationFormat, source);
    if (!destination || !PlanarImageConverter::copyColorPlanes(source, destination))
        return nullptr;

    CVBufferPropagateAttachments(source, destination);
    if (!hasAlphaPlane(destinationFormat)) {
        CVBufferRemoveAttachment(destination, kCVImageBufferAlphaChannelModeKey);
        return destination;
    }
    if (!PlanarImageConverter::copyAlphaPlane(source, destination))
        return nullptr;
    return destination;
}

RetainPtr<VTPixelTransferSessionRef> PixelBufferConverter::transferSession(const PlatformVideoColorSpace& colorSpace)
{
    if (m_transferSession && m_transferSessionColorSpace == colorSpace)
        return m_transferSession;

    RetainPtr session = createPixelTransferSession(m_useIOSurface == UseIOSurface::Yes);
    m_transferSession = session;
    m_transferSessionColorSpace = colorSpace;
    if (!session)
        return nullptr;

    forEachColorProperty(colorSpace, [&](CFStringRef, CFStringRef transferSessionKey, CFStringRef value) {
        VTSessionSetProperty(session, transferSessionKey, value);
    });
    return session;
}

RetainPtr<CVPixelBufferRef> PixelBufferConverter::convertWithVideoToolbox(CVPixelBufferRef source, OSType destinationFormat, const PlatformVideoColorSpace& colorSpace)
{
    // VideoToolbox can't read the formats WebKit registers with CoreVideo, misreads CoreVideo's 10-bit Y'CbCr with 8-bit
    // alpha (rdar://193713488) and rounds alpha down (rdar://192541461): it is given the colour planes only.
    RetainPtr<CVPixelBufferRef> colorSource = source;
    bool sourceHasAlphaPlane = hasAlphaPlane(CVPixelBufferGetPixelFormatType(source));
    if (sourceHasAlphaPlane) {
        auto colorFormat = pixelFormatWithoutAlpha(CVPixelBufferGetPixelFormatType(source));
        colorSource = colorFormat ? copyPlanes(source, *colorFormat, m_colorPlanesPool) : nullptr;
        if (!colorSource)
            return nullptr;
    }

    RetainPtr destination = createPixelBuffer(m_outputPool, destinationFormat, source);
    if (!destination)
        return nullptr;
    RetainPtr session = transferSession(colorSpace);
    if (!session)
        return nullptr;

    auto status = VTPixelTransferSessionTransferImage(session, colorSource, destination);
    if (status != noErr) {
        RELEASE_LOG_ERROR(Media, "PixelBufferConverter::convert: VTPixelTransferSessionTransferImage failed with error %d", static_cast<int>(status));
        return nullptr;
    }

    if (sourceHasAlphaPlane && hasAlphaPlane(destinationFormat) && !PlanarImageConverter::copyAlphaPlane(source, destination))
        return nullptr;
    return destination;
}

RetainPtr<CVPixelBufferRef> PixelBufferConverter::convert(CVPixelBufferRef source, OSType destinationFormat, const PlatformVideoColorSpace& colorSpace)
{
    if (!source)
        return nullptr;

    auto sourceFormat = CVPixelBufferGetPixelFormatType(source);
    bool hasRequestedColorSpace = hasColorSpace(source, colorSpace);
    if (sourceFormat == destinationFormat && hasRequestedColorSpace)
        return source;

    if (hasRequestedColorSpace && canCopyPlanes(sourceFormat, destinationFormat)) {
        if (RetainPtr destination = copyPlanes(source, destinationFormat, m_outputPool))
            return destination;
    }
    return convertWithVideoToolbox(source, destinationFormat, colorSpace);
}

} // namespace WebCore
