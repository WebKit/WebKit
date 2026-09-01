/*
 * Copyright (C) 2016-2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NativeImage.h"

#if USE(CG)

#include "CGSubimageCacheWithTimer.h"
#include "CGUtilities.h"
#include "GeometryUtilities.h"
#include "GraphicsContextCG.h"
#include "ImageBuffer.h"
#include "ImageRotationSessionVT.h"
#include "Logging.h"
#include "PixelBuffer.h"
#include "PixelBufferConversion.h"
#include <limits>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/Scope.h>
#include <wtf/cf/VectorCF.h>

#include "CoreVideoSoftLink.h"

namespace WebCore {

RefPtr<NativeImage> NativeImage::create(PlatformImagePtr&& image, std::optional<GainMap>&& gainMap)
{
    if (!image)
        return nullptr;
    if (CGImageGetWidth(image.get()) > std::numeric_limits<int>::max() || CGImageGetHeight(image.get()) > std::numeric_limits<int>::max())
        return nullptr;
    return adoptRef(*new NativeImage(WTF::move(image), WTF::move(gainMap)));
}

RefPtr<NativeImage> NativeImage::create(PlatformImagePtr&& image)
{
    return create(WTF::move(image), std::nullopt);
}

RefPtr<NativeImage> NativeImage::createTransient(PlatformImagePtr&& image)
{
    if (!image)
        return nullptr;
    // FIXME: GraphicsContextCG caching should be made better and this should be the default mode
    // for NativeImage, as we cannot guarantee all the places that draw images to not cache unwanted
    // images.
    RetainPtr<CGImage> transientImage = adoptCF(CGImageCreateCopy(image.get())); // Make a shallow copy so the metadata change doesn't affect the caller.
    if (!transientImage)
        return nullptr;
    image = nullptr;
    CGImageSetCachingFlags(transientImage.get(), kCGImageCachingTransient);
    return create(WTF::move(transientImage));
}

static void releaseImageBlock(void* info, CGImageBlockRef)
{
    SUPPRESS_RETAINPTR_CTOR_ADOPT RetainPtr pixelBuffer = adoptCF(static_cast<CVPixelBufferRef>(info));
    CVPixelBufferUnlockBaseAddress(pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
}

static const CGImageBlockCallbacks blockCallbacks = { 0, releaseImageBlock };

static bool canSatisfyBlockSetOptions(CFDictionaryRef options)
{
    CFIndex satisfiedRequests = 0;
    if (CFDictionaryContainsKey(options, kCGImageBlockSingletonRequest))
        ++satisfiedRequests;
    if (CFDictionaryContainsKey(options, kCGImageBlockMarkAsReadOnlyRequest))
        ++satisfiedRequests;
    return CFDictionaryGetCount(options) == satisfiedRequests;
}

static CGImageBlockSetRef copyImageBlockSet(void* info, CGImageProviderRef provider, CGRect, CGSize, CFDictionaryRef options)
{
    if (options && !canSatisfyBlockSetOptions(options))
        return nullptr;

    RetainPtr pixelBuffer = static_cast<CVPixelBufferRef>(info);
    if (CVPixelBufferLockBaseAddress(pixelBuffer.get(), kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess)
        return nullptr;
    auto lockCleanup = makeScopeExit([&] {
        CVPixelBufferUnlockBaseAddress(pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    });

    auto size = CGImageProviderGetSize(provider);
    auto rect = CGRectMake(0, 0, size.width, size.height);
    auto* data = CVPixelBufferGetBaseAddress(pixelBuffer.get());
    auto bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer.get());
    // Image blocks and image block sets are not CF types, so they cannot be held in a RetainPtr:
    // they are released with CGImageBlockRelease() and CGImageBlockSetRelease(), not CFRelease().
    SUPPRESS_UNRETAINED_LOCAL CGImageBlockRef block = CGImageBlockCreate(data, rect, bytesPerRow, pixelBuffer.get(), &blockCallbacks);
    if (!block)
        return nullptr;
    // The block owns a reference to the buffer and its read lock, released via releaseImageBlock.
    lockCleanup.release();
    SUPPRESS_RETAINPTR_CTOR_ADOPT (void) pixelBuffer.leakRef(); // NOLINT
    SUPPRESS_UNRETAINED_LOCAL CGImageBlockSetRef blockSet = CGImageBlockSetCreate(provider, rect.size, rect, 1, &block, nullptr, nullptr);
    if (!blockSet) {
        // CGImageBlockSetCreate() takes ownership of the block only on success.
        CGImageBlockRelease(block);
        return nullptr;
    }
    // The blockSet took ownership of the block; the caller takes ownership of the blockSet.
    return blockSet;
}

static IOSurfaceRef copyIOSurface(void* info, CGImageProviderRef, CFDictionaryRef)
{
    RetainPtr result = CVPixelBufferGetIOSurface(static_cast<CVPixelBufferRef>(info));
    // FIXME(rdar://162218496): SaferCPP should notice that our API is a create.
    SUPPRESS_RETAINPTR_CTOR_ADOPT return result.leakRef();
}

static void releaseProviderInfo(void* info)
{
    CFRelease(info);
}

static const CGImageProviderCallbacksVersion1 blockSetOnlyCallbacks = { 1, copyImageBlockSet, releaseProviderInfo };
static const CGImageProviderCallbacksVersion2 surfaceCallbacks = { 2, copyImageBlockSet, copyIOSurface, releaseProviderInfo };

RefPtr<NativeImage> NativeImage::create(RetainPtr<CVPixelBufferRef> pixelBuffer, CGImageAlphaInfo alphaInfo, RetainPtr<CGColorSpaceRef> colorSpace)
{
    if (!pixelBuffer)
        return nullptr;
    if (CVPixelBufferGetPixelFormatType(pixelBuffer.get()) != kCVPixelFormatType_32BGRA)
        return nullptr;
    IntSize size { static_cast<int>(CVPixelBufferGetWidth(pixelBuffer.get())), static_cast<int>(CVPixelBufferGetHeight(pixelBuffer.get())) };
    if (size.isEmpty() || !colorSpace)
        return nullptr;

    // Currently CA uses the IOSurface if there is one, but is not able to draw with unpremultiplied correctly.
    RetainPtr surface = CVPixelBufferGetIOSurface(pixelBuffer.get());
    bool contentsArePremultiplied = alphaInfo != kCGImageAlphaFirst && alphaInfo != kCGImageAlphaLast;
    const void* callbacks = surface && contentsArePremultiplied ? static_cast<const void*>(&surfaceCallbacks) : static_cast<const void*>(&blockSetOnlyCallbacks);

    auto bitmapInfo = static_cast<int32_t>(static_cast<CGBitmapInfo>(alphaInfo) | static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little));
    RetainPtr bitmapInfoNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &bitmapInfo));
    CFTypeRef keys[] = { kCGImageProviderBitmapInfo };
    CFTypeRef values[] = { bitmapInfoNumber.get() };
    RetainPtr auxiliaryInfo = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, std::size(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    RetainPtr provider = adoptCF(CGImageProviderCreate(CGSizeMake(size.width(), size.height()), kCGImageComponent8BitInteger, colorSpace.get(), pixelBuffer.get(), callbacks, auxiliaryInfo.get()));
    if (!provider)
        return nullptr;
    // On success, the provider owns the pixel buffer, and releases it via releaseProviderInfo.
    SUPPRESS_RETAINPTR_CTOR_ADOPT (void) pixelBuffer.leakRef(); // NOLINT

    RetainPtr<CGImageRef> image = adoptCF(CGImageCreateWithImageProvider(provider.get(), nullptr, false, kCGRenderingIntentDefault));
    if (!image)
        return nullptr;
    // CoreAnimation draws the surface directly when the image describes it, instead of
    // reading the contents back and uploading them to a texture of its own.
    if (surface && contentsArePremultiplied)
        CGImageSetProperty(image.get(), kCGImagePropertyIOSurface, surface.get());
    // For historical reasons, CoreAnimation will adjust certain video color spaces when
    // displaying the video. If the image is drawn to an accelerated image buffer, e.g. for a
    // canvas, CA may not do this same adjustment, resulting in the canvas pixels not matching
    // the source video. Setting this property, even for an image that is not IOSurface
    // backed, avoids this non-adjustment of the image color space. rdar://88804270
    CGImageSetProperty(image.get(), CFSTR("CA_IOSURFACE_IMAGE"), kCFBooleanTrue);
    return NativeImage::create(WTF::move(image));
}

IntSize NativeImage::size() const
{
    Locker locker { m_lock };
    return IntSize(CGImageGetWidth(m_platformImage.get()), CGImageGetHeight(m_platformImage.get()));
}

bool NativeImage::hasAlpha() const
{
    Locker locker { m_lock };
    CGImageAlphaInfo info = CGImageGetAlphaInfo(m_platformImage.get());
    return (info >= kCGImageAlphaPremultipliedLast) && (info <= kCGImageAlphaFirst);
}

size_t NativeImage::sizeInBytes() const
{
    Locker locker { m_lock };
    CheckedSize height = CGImageGetHeight(m_platformImage);
    CheckedSize sizeInBytes = height * CGImageGetBytesPerRow(m_platformImage);
    if (m_gainMap)
        sizeInBytes += CVPixelBufferGetDataSize(m_gainMap->gainMapPixelBuffer);
    return sizeInBytes;
}

DestinationColorSpace NativeImage::colorSpace() const
{
    Locker locker { m_lock };
    return DestinationColorSpace(CGImageGetColorSpace(m_platformImage.get()));
}

void NativeImage::computeHeadroom() const
{
    constexpr float whiteLevel = 203.0; // Default reference white 203 nits
    constexpr float peakLevel = 1000.0; // Default to 1000 nits
    constexpr auto gainMapImageHeadroom = Headroom(peakLevel / whiteLevel);

#if HAVE(SUPPORT_HDR_DISPLAY)
    float headroom = CGImageGetContentHeadroom(m_platformImage.get());
    m_baseImageHeadroom = Headroom(std::max<float>(headroom, Headroom::None));
#else
    m_baseImageHeadroom = Headroom::None;
#endif

    if (hasHDRGainMap()) {
        m_headroom = gainMapImageHeadroom;
        ASSERT(m_baseImageHeadroom == Headroom::None);
    } else
        m_headroom = m_baseImageHeadroom;
}

// Maps a CGImage's layout onto a PixelBufferFormat. nullopt for layouts PixelBufferFormat
// cannot name: indexed or non-RGB color spaces, 16-bits-per-component integer, and fewer
// than four components. Those images reach withPixels() through readPixels() instead.
//
// Deliberately not exported. NativeImage is meant to be the only place that maps a CGImage
// layout to a pixel format; a shared helper would invite callers to bypass it, which is how
// GraphicsContextGLCG, GPUQueue and ShareableBitmapCG each grew a copy of this switch.
static std::optional<PixelBufferFormat> pixelBufferFormat(CGImageRef image)
{
    RetainPtr colorSpace = CGImageGetColorSpace(image);
    if (!colorSpace || CGColorSpaceGetModel(colorSpace.get()) != kCGColorSpaceModelRGB)
        return std::nullopt;

    auto bitmapInfo = CGImageGetBitmapInfo(image);
    auto alphaInfo = static_cast<CGImageAlphaInfo>(bitmapInfo & kCGBitmapAlphaInfoMask);
    auto byteOrder = bitmapInfo & kCGBitmapByteOrderMask;
    auto bitsPerComponent = CGImageGetBitsPerComponent(image);
    auto bitsPerPixel = CGImageGetBitsPerPixel(image);

    if (bitmapInfo & kCGBitmapFloatComponents) {
#if ENABLE(PIXEL_FORMAT_RGBA16F)
        if (bitsPerComponent != 16 || bitsPerPixel != 64 || byteOrder != static_cast<CGBitmapInfo>(kCGBitmapByteOrder16Host))
            return std::nullopt;
        switch (alphaInfo) {
        case kCGImageAlphaPremultipliedLast:
            return PixelBufferFormat { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA16F, DestinationColorSpace { colorSpace.get() } };
        case kCGImageAlphaLast:
            return PixelBufferFormat { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA16F, DestinationColorSpace { colorSpace.get() } };
        default:
            return std::nullopt;
        }
#else
        return std::nullopt;
#endif
    }

    if (bitsPerComponent != 8 || bitsPerPixel != 32)
        return std::nullopt;

    // kCGBitmapByteOrderDefault is big-endian in practice, even on little-endian
    // architectures. This matches what the CG WebGL image extractor has always assumed.
    bool isLittleEndian = byteOrder == static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little);

    switch (alphaInfo) {
    case kCGImageAlphaPremultipliedLast:
        if (isLittleEndian)
            return std::nullopt; // ABGR8: not nameable.
        return PixelBufferFormat { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, DestinationColorSpace { colorSpace.get() } };
    case kCGImageAlphaLast:
        if (isLittleEndian)
            return std::nullopt; // ABGR8: not nameable.
        return PixelBufferFormat { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, DestinationColorSpace { colorSpace.get() } };
    case kCGImageAlphaPremultipliedFirst:
        if (!isLittleEndian)
            return std::nullopt; // ARGB8: not nameable.
        return PixelBufferFormat { AlphaPremultiplication::Premultiplied, PixelFormat::BGRA8, DestinationColorSpace { colorSpace.get() } };
    case kCGImageAlphaFirst:
        if (!isLittleEndian)
            return std::nullopt; // ARGB8: not nameable.
        return PixelBufferFormat { AlphaPremultiplication::Unpremultiplied, PixelFormat::BGRA8, DestinationColorSpace { colorSpace.get() } };
    case kCGImageAlphaNoneSkipFirst:
        if (!isLittleEndian)
            return std::nullopt; // XRGB8: not nameable.
        // BGRX8 is opaque, so its alpha format is immaterial; convertImagePixels() treats
        // an opaque source as already premultiplied.
        return PixelBufferFormat { AlphaPremultiplication::Premultiplied, PixelFormat::BGRX8, DestinationColorSpace { colorSpace.get() } };
    case kCGImageAlphaNone:
    case kCGImageAlphaNoneSkipLast:
    case kCGImageAlphaOnly:
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<NativeImage::PixelSourceInfo> NativeImage::pixelSourceInfo() const
{
    RetainPtr image = platformImage();
    if (!image)
        return std::nullopt;

    // A sub-image shares its parent's data provider, so it is the parent's layout that
    // describes the pixels. Only the size, which size() reports, is the sub-image's own.
    CGImageRef parent = nullptr;
    CGPoint parentOrigin { };
    if (CGImageIsSubimage(image.get(), &parent, &parentOrigin) && parent)
        image = parent;

    if (!CGImageGetDataProvider(image.get()))
        return std::nullopt;
    auto format = pixelBufferFormat(image.get());
    if (!format)
        return std::nullopt;

    auto bytesPerRow = CGImageGetBytesPerRow(image.get());
    if (bytesPerRow > std::numeric_limits<unsigned>::max())
        return std::nullopt;
    return PixelSourceInfo { *format, static_cast<unsigned>(bytesPerRow) };
}

bool NativeImage::withBorrowedPixels(NOESCAPE const PixelSourceFunctor& functor) const
{
    RetainPtr image = platformImage();
    if (!image)
        return false;

    // Resolve sub-images to an offset within the parent's rows. `parent` is +0 and owned by
    // the sub-image, so `subimage` must stay retained for as long as `image` is used.
    RetainPtr subimage = image;
    IntPoint origin;
    CGImageRef parent = nullptr;
    CGPoint parentOrigin { };
    if (CGImageIsSubimage(image.get(), &parent, &parentOrigin) && parent) {
        image = parent;
        origin = IntPoint(parentOrigin);
    }

    auto format = pixelBufferFormat(image.get());
    if (!format)
        return false;

    RetainPtr provider = CGImageGetDataProvider(image.get());
    if (!provider)
        return false;

    RetainPtr<CFDataRef> data;
    @try {
        data = adoptCF(CGDataProviderCopyData(provider.get()));
    } @catch (id exception) {
        LOG_WITH_STREAM(Images, stream << "NativeImage::withBorrowedPixels() CGDataProviderCopyData raised for a "
            << cgImageRect(image.get()).size() << " image with bytesPerRow " << CGImageGetBytesPerRow(image.get()));
    }
    if (!data)
        return false;

    auto bytesPerRow = CGImageGetBytesPerRow(image.get());
    if (bytesPerRow > std::numeric_limits<unsigned>::max())
        return false;

    // Validate against the parent's geometry, then narrow to the sub-image rect. create()
    // accepts a data provider larger than the image needs and rejects one that is too small.
    auto parentView = validatedConversionView(*format, cgImageRect(image.get()).size(), static_cast<unsigned>(bytesPerRow), WTF::span(data.get()));
    if (!parentView)
        return false;
    auto view = conversionSubview(*parentView, cgImageRect(image.get()).size(), { origin, cgImageRect(subimage.get()).size() });
    if (!view)
        return false;

    functor(*view);
    return true;
}

bool NativeImage::readPixels(const PixelBufferFormat& format, std::span<uint8_t> destination, unsigned bytesPerRow) const
{
    auto size = this->size();
    if (size.isEmpty()) {
        ASSERT_NOT_REACHED();
        return false;
    }

    // CGBitmapContext writes the whole stride, including any padding on the last row, so the
    // destination has to hold every byte of every row rather than PixelBuffer::minimumBufferSize().
    auto tightlyPackedBytesPerRow = PixelBuffer::tightlyPackedBytesPerRow(format.pixelFormat, size.width());
    if (tightlyPackedBytesPerRow.hasOverflowed() || bytesPerRow < tightlyPackedBytesPerRow.value()) {
        ASSERT_NOT_REACHED();
        return false;
    }
    auto requiredBytes = CheckedSize { bytesPerRow } * static_cast<size_t>(size.height());
    if (requiredBytes.hasOverflowed() || destination.size() < requiredBytes.value()) {
        ASSERT_NOT_REACHED();
        return false;
    }

    // CGBitmapContext always premultiplies, so draw premultiplied and let the caller's
    // conversion unpremultiply if it asked for that.
    auto bitmapInfo = [&]() -> std::optional<CGBitmapInfo> {
        switch (format.pixelFormat) {
        case PixelFormat::RGBA8:
            return static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big) | static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast);
        case PixelFormat::BGRA8:
            return static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst);
        case PixelFormat::BGRX8:
            return static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaNoneSkipFirst);
#if ENABLE(PIXEL_FORMAT_RGBA16F)
        case PixelFormat::RGBA16F:
            return static_cast<CGBitmapInfo>(kCGBitmapByteOrder16Host) | static_cast<CGBitmapInfo>(kCGBitmapFloatComponents) | static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast);
#endif
        default:
            return std::nullopt;
        }
    }();
    if (!bitmapInfo)
        return false;

    auto bitsPerComponent = PixelBuffer::bytesPerPixelComponent(format.pixelFormat) * 8;
    RetainPtr bitmapContext = adoptCF(CGBitmapContextCreate(destination.data(), size.width(), size.height(), bitsPerComponent, bytesPerRow, protect(format.colorSpace.platformColorSpace()).get(), *bitmapInfo));
    if (!bitmapContext)
        return false;

    CGContextSetBlendMode(bitmapContext.get(), kCGBlendModeCopy);
    CGContextSetInterpolationQuality(bitmapContext.get(), kCGInterpolationNone);
    CGContextDrawImage(bitmapContext.get(), CGRectMake(0, 0, size.width(), size.height()), platformImage().get());

    // The alpha the draw produced is premultiplied. Tell withPixels() the truth by
    // converting in place when the caller asked for unpremultiplied.
    if (format.alphaFormat == AlphaPremultiplication::Unpremultiplied && !pixelFormatIsOpaque(format.pixelFormat)) {
        PixelBufferFormat premultiplied { AlphaPremultiplication::Premultiplied, format.pixelFormat, format.colorSpace };
        ConstPixelBufferConversionView source { premultiplied, bytesPerRow, destination };
        PixelBufferConversionView unpremultiplied { format, bytesPerRow, destination };
        convertImagePixels(source, unpremultiplied, size);
    }
    return true;
}

RefPtr<NativeImage> NativeImage::rotatedImage(ImageOrientation orientation)
{
    ImageRotationSessionVT rotationSession(ImageRotationSessionVT::ShouldUseIOSurface::No);
    return rotationSession.applyRotation(*this, orientation, ImageRotationSessionVT::IsCGImageCompatible::Yes);
}

void NativeImage::clearSubimages()
{
#if CACHE_SUBIMAGES
    CGSubimageCacheWithTimer::clearImage(platformImage().get());
#endif
}

} // namespace WebCore

#endif // USE(CG)
