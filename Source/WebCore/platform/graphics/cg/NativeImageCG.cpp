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
#include "GeometryUtilities.h"
#include "GraphicsContextCG.h"
#include "ImageBuffer.h"
#include "ImageRotationSessionVT.h"
#include "ImageUtilities.h"
#include "PixelBuffer.h"
#include <limits>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/Scope.h>

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

static CGImageAlphaInfo alphaInfoForAlphaLast(bool isPremultiplied)
{
    return isPremultiplied ? kCGImageAlphaPremultipliedLast : kCGImageAlphaLast;
}

static CGImageAlphaInfo alphaInfoForAlphaFirst(bool isPremultiplied)
{
    return isPremultiplied ? kCGImageAlphaPremultipliedFirst : kCGImageAlphaFirst;
}

RefPtr<NativeImage> NativeImage::create(Ref<PixelBuffer>&& pixelBuffer)
{
    if (pixelBuffer->size().isEmpty())
        return nullptr;
    auto format = pixelBuffer->format();
    bool isPremultiplied = format.alphaFormat == AlphaPremultiplication::Premultiplied;
    // RGBA == kCGBitmapByteOrder32Big | kCGImageAlpha*Last
    // BGRA == kCGBitmapByteOrder32Little | kCGImageAlpha*First
    CGBitmapInfo bitmapInfo;
    switch (format.pixelFormat) {
    case PixelFormat::RGBX8:
        bitmapInfo = static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big) | static_cast<CGBitmapInfo>(kCGImageAlphaNoneSkipLast);
        break;
    case PixelFormat::RGBA8:
        bitmapInfo = static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big) | static_cast<CGBitmapInfo>(alphaInfoForAlphaLast(isPremultiplied));
        break;
    case PixelFormat::BGRX8:
        bitmapInfo = static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaNoneSkipFirst);
        break;
    case PixelFormat::BGRA8:
        bitmapInfo = static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(alphaInfoForAlphaFirst(isPremultiplied));
        break;
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case PixelFormat::RGBA16F:
        bitmapInfo = static_cast<CGBitmapInfo>(kCGBitmapByteOrder16Host) | static_cast<CGBitmapInfo>(kCGBitmapFloatComponents) | static_cast<CGBitmapInfo>(alphaInfoForAlphaLast(isPremultiplied));
        break;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10)
    case PixelFormat::RGB10:
        ASSERT(!PixelBuffer::supportedPixelFormat(format.pixelFormat));
        return nullptr;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case PixelFormat::RGB10A8:
        ASSERT(!PixelBuffer::supportedPixelFormat(format.pixelFormat));
        return nullptr;
#endif
    }

    auto imageSize = pixelBuffer->size();
    RetainPtr colorSpace = format.colorSpace.platformColorSpace();
    auto data = pixelBuffer->bytes();
    auto bytesPerPixel = PixelBuffer::bytesPerPixel(format.pixelFormat);
    auto bitsPerComponent = PixelBuffer::bytesPerPixelComponent(format.pixelFormat) * 8;

    verifyImageBufferIsBigEnough(data);

    // On success, the data provider owns the pixel buffer reference.
    RetainPtr dataProvider = adoptCF(CGDataProviderCreateWithData(pixelBuffer.ptr(), data.data(), data.size(), [] (void* context, const void*, size_t) {
        static_cast<PixelBuffer*>(context)->deref();
    }));
    if (!dataProvider)
        return nullptr;
    SUPPRESS_RETAINPTR_CTOR_ADOPT (void) pixelBuffer.leakRef(); // NOLINT

    return NativeImage::create(adoptCF(CGImageCreate(imageSize.width(), imageSize.height(), bitsPerComponent, bytesPerPixel * 8, bytesPerPixel * imageSize.width(), colorSpace.get(), bitmapInfo, dataProvider.get(), 0, false, kCGRenderingIntentDefault)));
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

ColorSpace NativeImage::colorSpace() const
{
    Locker locker { m_lock };
    return ColorSpace(CGImageGetColorSpace(m_platformImage.get()));
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

std::optional<Color> NativeImage::singlePixelSolidColor() const
{
    if (size() != IntSize(1, 1))
        return std::nullopt;

    std::array<uint8_t, 4> pixel; // RGBA
    auto bitmapContext = adoptCF(CGBitmapContextCreate(pixel.data(), 1, 1, 8, pixel.size(), sRGBColorSpaceSingleton(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));

    if (!bitmapContext)
        return std::nullopt;

    CGContextSetBlendMode(bitmapContext.get(), kCGBlendModeCopy);
    CGContextDrawImage(bitmapContext.get(), CGRectMake(0, 0, 1, 1), platformImage().get());

    if (!pixel[3])
        return Color::transparentBlack;

    return makeFromComponentsClampingExceptAlpha<SRGBA<uint8_t>>(pixel[0] * 255 / pixel[3], pixel[1] * 255 / pixel[3], pixel[2] * 255 / pixel[3], pixel[3]);
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
