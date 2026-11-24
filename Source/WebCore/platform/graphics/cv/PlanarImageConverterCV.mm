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
#import "PlanarImageConverterCV.h"

#import "FourCC.h"
#import "Logging.h"
#import <array>
#import <pal/spi/cf/CoreVideoSPI.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#import "CoreVideoSoftLink.h"

namespace WebCore {

PixelBufferLock::PixelBufferLock(CVPixelBufferRef pixelBuffer, CVPixelBufferLockFlags flags)
    : m_pixelBuffer(pixelBuffer)
    , m_flags(flags)
    , m_isLocked(pixelBuffer && CVPixelBufferLockBaseAddress(pixelBuffer, flags) == kCVReturnSuccess)
{
}

PixelBufferLock::~PixelBufferLock()
{
    if (m_isLocked)
        CVPixelBufferUnlockBaseAddress(m_pixelBuffer.get(), m_flags);
}

namespace PlanarImageConverter {

static constexpr auto subsampled420 = ChromaSubsampling::HorizontalAndVertical;
static constexpr auto subsampled422 = ChromaSubsampling::Horizontal;
static constexpr auto subsampled444 = ChromaSubsampling::None;

static constexpr std::array pixelFormatDescriptions {
    YUVPixelFormatDescription { kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, subsampled420, eightBit, false, { } },
    YUVPixelFormatDescription { kCVPixelFormatType_420YpCbCr8BiPlanarFullRange, subsampled420, eightBit, true, { } },
    YUVPixelFormatDescription { kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange, subsampled420, tenBitMSB, false, { } },
    YUVPixelFormatDescription { kCVPixelFormatType_420YpCbCr10BiPlanarFullRange, subsampled420, tenBitMSB, true, { } },
    YUVPixelFormatDescription { kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange, subsampled422, eightBit, false, { } },
    YUVPixelFormatDescription { kCVPixelFormatType_422YpCbCr8BiPlanarFullRange, subsampled422, eightBit, true, { } },
    YUVPixelFormatDescription { kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange, subsampled422, tenBitMSB, false, { } },
    YUVPixelFormatDescription { kCVPixelFormatType_422YpCbCr10BiPlanarFullRange, subsampled422, tenBitMSB, true, { } },
    YUVPixelFormatDescription { kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange, subsampled444, eightBit, false, { } },
    YUVPixelFormatDescription { kCVPixelFormatType_444YpCbCr8BiPlanarFullRange, subsampled444, eightBit, true, { } },
    YUVPixelFormatDescription { kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange, subsampled444, tenBitMSB, false, { } },
    YUVPixelFormatDescription { kCVPixelFormatType_444YpCbCr10BiPlanarFullRange, subsampled444, tenBitMSB, true, { } },
    YUVPixelFormatDescription { kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar, subsampled420, eightBit, false, eightBit },
    YUVPixelFormatDescription { kCVPixelFormatType_422YpCbCr8VideoRange_8A_TriPlanar, subsampled422, eightBit, false, eightBit },
    YUVPixelFormatDescription { kCVPixelFormatType_444YpCbCr8VideoRange_8A_TriPlanar, subsampled444, eightBit, false, eightBit },
    YUVPixelFormatDescription { kCVPixelFormatType_420YpCbCr10VideoRange_8A_TriPlanar, subsampled420, tenBitMSB, false, eightBit },
    YUVPixelFormatDescription { kCVPixelFormatType_422YpCbCr10VideoRange_8A_TriPlanar, subsampled422, tenBitMSB, false, eightBit },
    YUVPixelFormatDescription { kCVPixelFormatType_444YpCbCr10VideoRange_8A_TriPlanar, subsampled444, tenBitMSB, false, eightBit },
    YUVPixelFormatDescription { kCVPixelFormatType_422YpCbCr10VideoRange_16A_TriPlanar, subsampled422, tenBitMSB, false, sixteenBit },
    YUVPixelFormatDescription { kCVPixelFormatType_444YpCbCr10VideoRange_16A_TriPlanar, subsampled444, tenBitMSB, false, sixteenBit },
};

static constexpr std::array registeredPixelFormatDescriptions {
    YUVPixelFormatDescription { pixelFormatWK420YpCbCr8FullRange8ATriPlanar, subsampled420, eightBit, true, eightBit },
    YUVPixelFormatDescription { pixelFormatWK422YpCbCr8FullRange8ATriPlanar, subsampled422, eightBit, true, eightBit },
    YUVPixelFormatDescription { pixelFormatWK444YpCbCr8FullRange8ATriPlanar, subsampled444, eightBit, true, eightBit },
    YUVPixelFormatDescription { pixelFormatWK420YpCbCr10VideoRange16ATriPlanar, subsampled420, tenBitMSB, false, tenBitMSB },
    YUVPixelFormatDescription { pixelFormatWK420YpCbCr10FullRange16ATriPlanar, subsampled420, tenBitMSB, true, tenBitMSB },
    YUVPixelFormatDescription { pixelFormatWK422YpCbCr10FullRange16ATriPlanar, subsampled422, tenBitMSB, true, tenBitMSB },
    YUVPixelFormatDescription { pixelFormatWK444YpCbCr10FullRange16ATriPlanar, subsampled444, tenBitMSB, true, tenBitMSB },
};

// A sample of the given format holding eightBitValue scaled to its bit depth.
static uint16_t sampleValue(SampleFormat format, unsigned eightBitValue)
{
    unsigned value = eightBitValue << (format.bitDepth - 8);
    if (format.isMSBAligned)
        value <<= 16 - format.bitDepth;
    return static_cast<uint16_t>(value);
}

static uint16_t maximumSampleValue(SampleFormat format)
{
    unsigned value = (1u << format.bitDepth) - 1;
    if (format.isMSBAligned)
        value <<= 16 - format.bitDepth;
    return static_cast<uint16_t>(value);
}

static RetainPtr<NSDictionary> planeDescription(SampleFormat format, std::span<const uint16_t> blackSamples, size_t horizontalSubsampling, size_t verticalSubsampling)
{
    Vector<uint8_t, 4> blackBlock;
    for (auto sample : blackSamples) {
        blackBlock.append(static_cast<uint8_t>(sample));
        if (format.sampleSize() > 1)
            blackBlock.append(static_cast<uint8_t>(sample >> 8));
    }
    return @{
        (__bridge NSString *)kCVPixelFormatBitsPerBlock: @(8 * format.sampleSize() * blackSamples.size()),
        (__bridge NSString *)kCVPixelFormatBlackBlock: WTF::toNSData(borrow(blackBlock)->span()).get(),
        (__bridge NSString *)kCVPixelFormatHorizontalSubsampling: @(horizontalSubsampling),
        (__bridge NSString *)kCVPixelFormatVerticalSubsampling: @(verticalSubsampling),
    };
}

static RetainPtr<NSDictionary> coreVideoDescription(const YUVPixelFormatDescription& description)
{
    std::array<uint16_t, 1> lumaBlack { sampleValue(description.color, description.isFullRange ? 0 : 16) };
    std::array<uint16_t, 2> chromaBlack { sampleValue(description.color, 128), sampleValue(description.color, 128) };
    std::array<uint16_t, 1> opaqueAlpha { maximumSampleValue(*description.alpha) };
    return @{
        (__bridge NSString *)kCVPixelFormatConstant: @(description.pixelFormat),
        (__bridge NSString *)kCVPixelFormatComponentRange: (__bridge NSString *)(description.isFullRange ? kCVPixelFormatComponentRange_FullRange : kCVPixelFormatComponentRange_VideoRange),
        (__bridge NSString *)kCVPixelFormatContainsYCbCr: @YES,
        (__bridge NSString *)kCVPixelFormatContainsRGB: @NO,
        (__bridge NSString *)kCVPixelFormatContainsAlpha: @YES,
        (__bridge NSString *)kCVPixelFormatBitsPerComponent: @(description.color.bitDepth),
        (__bridge NSString *)kCVPixelFormatPlanes: @[
            planeDescription(description.color, lumaBlack, 1, 1).get(),
            planeDescription(description.color, chromaBlack, horizontalChromaFactor(description.subsampling), verticalChromaFactor(description.subsampling)).get(),
            planeDescription(*description.alpha, opaqueAlpha, 1, 1).get(),
        ],
    };
}

// Registration only applies to the calling process, and a FourCC CoreVideo already knows keeps its description.
// A format whose registered description isn't ours is not used.
static std::array<bool, registeredPixelFormatDescriptions.size()> registerPixelFormats()
{
    std::array<bool, registeredPixelFormatDescriptions.size()> isAvailable { };
    @autoreleasepool {
        for (size_t index = 0; index < registeredPixelFormatDescriptions.size(); ++index) {
            auto pixelFormat = registeredPixelFormatDescriptions[index].pixelFormat;
            RetainPtr<CFDictionaryRef> description = bridge_cast(coreVideoDescription(registeredPixelFormatDescriptions[index]));
            CVPixelFormatDescriptionRegisterDescriptionWithPixelFormatType(description, pixelFormat);
            RetainPtr registered = adoptCF(CVPixelFormatDescriptionCreateWithPixelFormatType(kCFAllocatorDefault, pixelFormat));
            isAvailable[index] = registered && CFEqual(registered, description);
            if (!isAvailable[index])
                RELEASE_LOG_ERROR(Media, "PlanarImageConverter: pixel format '%{public}s' is already registered with another description", FourCC { pixelFormat }.string().data());
        }
    }
    return isAvailable;
}

static const std::array<bool, registeredPixelFormatDescriptions.size()>& registeredPixelFormatAvailability()
{
    static const std::array<bool, registeredPixelFormatDescriptions.size()> isAvailable = registerPixelFormats();
    return isAvailable;
}

template<typename Predicate>
static std::optional<YUVPixelFormatDescription> findPixelFormatDescription(NOESCAPE const Predicate& predicate)
{
    for (auto& description : pixelFormatDescriptions) {
        if (predicate(description))
            return description;
    }
    auto& isAvailable = registeredPixelFormatAvailability();
    for (size_t index = 0; index < registeredPixelFormatDescriptions.size(); ++index) {
        if (isAvailable[index] && predicate(registeredPixelFormatDescriptions[index]))
            return registeredPixelFormatDescriptions[index];
    }
    return { };
}

std::optional<YUVPixelFormatDescription> pixelFormatDescription(OSType pixelFormat)
{
    return findPixelFormatDescription([&](auto& description) {
        return description.pixelFormat == pixelFormat;
    });
}

std::optional<OSType> pixelFormat(ChromaSubsampling subsampling, SampleFormat color, bool isFullRange, std::optional<SampleFormat> alpha)
{
    auto match = findPixelFormatDescription([&](auto& description) {
        return description.subsampling == subsampling && description.color == color && description.isFullRange == isFullRange && description.alpha == alpha;
    });
    if (!match)
        return { };
    return match->pixelFormat;
}

std::optional<SampleFormat> pixelBufferSampleFormat(SampleFormat format)
{
    if (format.bitDepth == 8)
        return eightBit;
    if (format.bitDepth == 10 || format.bitDepth == 12)
        return tenBitMSB;
    return { };
}

static std::optional<DestinationPlane> pixelBufferPlane(CVPixelBufferRef pixelBuffer, size_t index, SampleFormat format, uint8_t channels)
{
    return makePlane(CVPixelBufferGetSpanOfPlane(pixelBuffer, index), 0, CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, index), CVPixelBufferGetWidthOfPlane(pixelBuffer, index), CVPixelBufferGetHeightOfPlane(pixelBuffer, index), format, channels);
}

// The pixel buffer is locked.
static std::optional<YUVPixelBufferPlanes<uint8_t>> planesOfLockedPixelBuffer(CVPixelBufferRef pixelBuffer)
{
    auto description = pixelFormatDescription(CVPixelBufferGetPixelFormatType(pixelBuffer));
    if (!description || CVPixelBufferGetPlaneCount(pixelBuffer) != (description->alpha ? 3u : 2u))
        return { };

    auto y = pixelBufferPlane(pixelBuffer, 0, description->color, 1);
    auto uv = pixelBufferPlane(pixelBuffer, 1, description->color, 2);
    if (!y || !uv || uv->width != chromaWidth(description->subsampling, y->width) || uv->height != chromaHeight(description->subsampling, y->height))
        return { };

    std::optional<DestinationPlane> alpha;
    if (description->alpha) {
        alpha = pixelBufferPlane(pixelBuffer, 2, *description->alpha, 1);
        if (!alpha || alpha->width != y->width || alpha->height != y->height)
            return { };
    }
    return YUVPixelBufferPlanes<uint8_t> { *description, *y, *uv, alpha };
}

WritableYUVPixelBufferPlanes::WritableYUVPixelBufferPlanes(CVPixelBufferRef pixelBuffer)
    : m_lock(pixelBuffer, 0)
{
    if (m_lock)
        m_planes = planesOfLockedPixelBuffer(pixelBuffer);
}

ReadOnlyYUVPixelBufferPlanes::ReadOnlyYUVPixelBufferPlanes(CVPixelBufferRef pixelBuffer)
    : m_lock(pixelBuffer, kCVPixelBufferLock_ReadOnly)
{
    if (!m_lock)
        return;
    auto planes = planesOfLockedPixelBuffer(pixelBuffer);
    if (!planes)
        return;
    std::optional<SourcePlane> alpha;
    if (planes->alpha)
        alpha = asSource(*planes->alpha);
    m_planes = YUVPixelBufferPlanes<const uint8_t> { planes->description, asSource(planes->y), asSource(planes->uv), alpha };
}

static RangeConversion rangeConversion(bool isSourceFullRange, bool isDestinationFullRange)
{
    if (isSourceFullRange == isDestinationFullRange)
        return RangeConversion::None;
    return isSourceFullRange ? RangeConversion::FullToVideo : RangeConversion::VideoToFull;
}

bool copyFrame(const SourceFrame& frame, CVPixelBufferRef destination)
{
    WritableYUVPixelBufferPlanes lockedPlanes { destination };
    if (!lockedPlanes)
        return false;
    Borrow planes = lockedPlanes;
    if (planes->description().subsampling != frame.subsampling || frame.alpha.has_value() != planes->alpha().has_value())
        return false;

    auto conversion = rangeConversion(frame.isFullRange, planes->description().isFullRange);
    if (!copyPlane(frame.y, planes->y(), conversion, Component::Luma) || !interleave(frame.u, frame.v, planes->uv(), conversion))
        return false;
    return !frame.alpha || copyPlane(*frame.alpha, *planes->alpha());
}

static bool isAtLeastAsSubsampled(ChromaSubsampling a, ChromaSubsampling b)
{
    return horizontalChromaFactor(a) >= horizontalChromaFactor(b) && verticalChromaFactor(a) >= verticalChromaFactor(b);
}

static bool copyChroma(const SourcePlane& uv, ChromaSubsampling sourceSubsampling, const DestinationFrame& frame, RangeConversion conversion)
{
    if (sourceSubsampling == frame.subsampling)
        return deinterleave(uv, frame.u, frame.v, conversion);

    size_t stride = uv.width * frame.u.format.sampleSize();
    Vector<uint8_t> uStorage(stride * uv.height);
    Vector<uint8_t> vStorage(stride * uv.height);
    Borrow borrowedUStorage = uStorage;
    Borrow borrowedVStorage = vStorage;
    auto u = makePlane(borrowedUStorage->mutableSpan(), 0, stride, uv.width, uv.height, frame.u.format);
    auto v = makePlane(borrowedVStorage->mutableSpan(), 0, stride, uv.width, uv.height, frame.u.format);
    return u && v && deinterleave(uv, *u, *v, conversion) && downsample(asSource(*u), frame.u) && downsample(asSource(*v), frame.v);
}

bool copyFrame(CVPixelBufferRef source, size_t left, size_t top, const DestinationFrame& frame, const std::optional<SourcePlane>& alpha)
{
    ReadOnlyYUVPixelBufferPlanes lockedPlanes { source };
    if (!lockedPlanes)
        return false;
    Borrow planes = lockedPlanes;

    auto sourceSubsampling = planes->description().subsampling;
    if (!isAtLeastAsSubsampled(frame.subsampling, sourceSubsampling) || left % horizontalChromaFactor(sourceSubsampling) || top % verticalChromaFactor(sourceSubsampling))
        return false;

    auto conversion = rangeConversion(planes->description().isFullRange, frame.isFullRange);
    auto y = subplane(planes->y(), left, top, frame.y.width, frame.y.height);
    auto uv = subplane(planes->uv(), left / horizontalChromaFactor(sourceSubsampling), top / verticalChromaFactor(sourceSubsampling), chromaWidth(sourceSubsampling, frame.y.width), chromaHeight(sourceSubsampling, frame.y.height));
    if (!y || !uv || !copyPlane(*y, frame.y, conversion, Component::Luma) || !copyChroma(*uv, sourceSubsampling, frame, conversion))
        return false;

    if (!frame.alpha)
        return true;
    auto alphaSource = alpha ? alpha : planes->alpha();
    if (!alphaSource || alphaSource->width != planes->y().width || alphaSource->height != planes->y().height)
        return false;
    auto alphaRect = subplane(*alphaSource, left, top, frame.alpha->width, frame.alpha->height);
    return alphaRect && copyPlane(*alphaRect, *frame.alpha);
}

bool copyFrame(const SourceBiPlanarFrame& frame, CVPixelBufferRef destination)
{
    WritableYUVPixelBufferPlanes lockedPlanes { destination };
    if (!lockedPlanes)
        return false;
    Borrow planes = lockedPlanes;
    if (planes->description().subsampling != frame.subsampling || planes->alpha())
        return false;

    auto conversion = rangeConversion(frame.isFullRange, planes->description().isFullRange);
    return copyPlane(frame.y, planes->y(), conversion, Component::Luma) && copyPlane(frame.uv, planes->uv(), conversion, Component::Chroma);
}

bool copyFrame(CVPixelBufferRef source, size_t left, size_t top, const DestinationBiPlanarFrame& frame)
{
    ReadOnlyYUVPixelBufferPlanes lockedPlanes { source };
    if (!lockedPlanes)
        return false;
    Borrow planes = lockedPlanes;

    auto subsampling = planes->description().subsampling;
    if (subsampling != frame.subsampling || left % horizontalChromaFactor(subsampling) || top % verticalChromaFactor(subsampling))
        return false;

    auto conversion = rangeConversion(planes->description().isFullRange, frame.isFullRange);
    auto y = subplane(planes->y(), left, top, frame.y.width, frame.y.height);
    auto uv = subplane(planes->uv(), left / horizontalChromaFactor(subsampling), top / verticalChromaFactor(subsampling), frame.uv.width, frame.uv.height);
    return y && uv && copyPlane(*y, frame.y, conversion, Component::Luma) && copyPlane(*uv, frame.uv, conversion, Component::Chroma);
}

bool copyColorPlanes(CVPixelBufferRef source, CVPixelBufferRef destination)
{
    ReadOnlyYUVPixelBufferPlanes lockedSourcePlanes { source };
    WritableYUVPixelBufferPlanes lockedDestinationPlanes { destination };
    if (!lockedSourcePlanes || !lockedDestinationPlanes)
        return false;
    Borrow sourcePlanes = lockedSourcePlanes;
    Borrow destinationPlanes = lockedDestinationPlanes;
    if (sourcePlanes->description().subsampling != destinationPlanes->description().subsampling)
        return false;

    auto conversion = rangeConversion(sourcePlanes->description().isFullRange, destinationPlanes->description().isFullRange);
    return copyPlane(sourcePlanes->y(), destinationPlanes->y(), conversion, Component::Luma)
        && copyPlane(sourcePlanes->uv(), destinationPlanes->uv(), conversion, Component::Chroma);
}

bool copyLumaToAlpha(CVPixelBufferRef alphaSource, const DestinationPlane& destination)
{
    ReadOnlyYUVPixelBufferPlanes lockedPlanes { alphaSource };
    return lockedPlanes && copyPlane(borrow(lockedPlanes)->y(), destination);
}

bool copyLumaToAlphaPlane(CVPixelBufferRef alphaSource, CVPixelBufferRef destination)
{
    WritableYUVPixelBufferPlanes lockedPlanes { destination };
    if (!lockedPlanes)
        return false;
    Borrow planes = lockedPlanes;
    return planes->alpha() && copyLumaToAlpha(alphaSource, *planes->alpha());
}

// The pixel buffer is locked.
static std::optional<DestinationPlane> packedPixelBufferPlane(CVPixelBufferRef pixelBuffer)
{
    switch (CVPixelBufferGetPixelFormatType(pixelBuffer)) {
    case kCVPixelFormatType_32ARGB:
    case kCVPixelFormatType_32BGRA:
    case kCVPixelFormatType_32ABGR:
    case kCVPixelFormatType_32RGBA:
        return makePlane(CVPixelBufferGetSpan(pixelBuffer), 0, CVPixelBufferGetBytesPerRow(pixelBuffer), CVPixelBufferGetWidth(pixelBuffer), CVPixelBufferGetHeight(pixelBuffer), eightBit, 4);
    default:
        return { };
    }
}

bool copyAlphaPlaneToBGRA(CVPixelBufferRef source, CVPixelBufferRef destination)
{
    if (CVPixelBufferGetPixelFormatType(destination) != kCVPixelFormatType_32BGRA)
        return false;

    ReadOnlyYUVPixelBufferPlanes lockedPlanes { source };
    PixelBufferLock destinationLock { destination, 0 };
    if (!lockedPlanes || !destinationLock)
        return false;
    Borrow planes = lockedPlanes;
    auto& alpha = planes->alpha();
    if (!alpha || alpha->format != eightBit)
        return false;

    auto pixels = packedPixelBufferPlane(destination);
    // Alpha is the last byte of a 32BGRA pixel.
    constexpr uint8_t alphaChannel = 3;
    return pixels && copyPlaneToChannel(*alpha, *pixels, alphaChannel);
}

bool copyToPixelBuffer(const SourcePlane& source, CVPixelBufferRef destination, std::array<uint8_t, 4> channelMap)
{
    PixelBufferLock lock { destination, 0 };
    if (!lock)
        return false;
    auto pixels = packedPixelBufferPlane(destination);
    return pixels && permuteChannels(source, *pixels, channelMap);
}

bool copyFromPixelBuffer(CVPixelBufferRef source, size_t left, size_t top, const DestinationPlane& destination, std::array<uint8_t, 4> channelMap, std::optional<uint8_t> opaqueChannel)
{
    PixelBufferLock lock { source, kCVPixelBufferLock_ReadOnly };
    if (!lock)
        return false;
    auto pixels = packedPixelBufferPlane(source);
    if (!pixels)
        return false;
    auto rect = subplane(asSource(*pixels), left, top, destination.width, destination.height);
    return rect && permuteChannels(*rect, destination, channelMap, opaqueChannel);
}

} // namespace PlanarImageConverter

} // namespace WebCore
