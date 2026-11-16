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

// WebKit replacement for VK-GL-CTS framework/common/tcuImageIO.cpp. It
// implements the tcu::ImageIO PNG paths with Apple's ImageIO / CoreGraphics
// instead of libpng, so the dEQP runners need no vendored libpng (or the zlib
// it would pull in). The Apple Xcode targets compile this file in place of the
// upstream tcuImageIO.cpp. The PKM path is libpng-free and ported verbatim.

#include "tcuImageIO.hpp"
#include "tcuResource.hpp"
#include "tcuSurface.hpp"
#include "tcuTexture.hpp"
#include "tcuCompressedTexture.hpp"
#include "deFilePath.hpp"
#include "deUniquePtr.hpp"

#include <cstring>
#include <string>
#include <vector>

#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>

namespace tcu
{
namespace ImageIO
{

using std::string;
using std::vector;

namespace
{
// Minimal scope guard for CoreFoundation objects: releases on scope exit so the
// (exception-throwing) decode paths below never leak a CF object.
template <typename T>
class ScopedCF
{
  public:
    explicit ScopedCF(T ref) : m_ref(ref) {}
    ~ScopedCF()
    {
        if (m_ref)
            CFRelease(m_ref);
    }
    T get() const { return m_ref; }
    explicit operator bool() const { return m_ref != nullptr; }

    ScopedCF(const ScopedCF &)            = delete;
    ScopedCF &operator=(const ScopedCF &) = delete;

  private:
    T m_ref;
};

// Copy the decoded pixels straight out of the CGImage's backing store (the
// approach WebCore's ShareableBitmap::createFromImagePixels uses), avoiding a
// redraw and the premultiply->unpremultiply round-trip, and skipping color
// management so the stored sample values reach tcu unchanged (like libpng).
// Only the exact 8-bit RGB / straight-RGBA byte layouts tcu expects are
// accepted; anything else (premultiplied, BGRA/ARGB order, 16-bit, a cropped
// sub-image) returns false so the caller falls back to rasterization.
bool copyImagePixelsDirect(CGImageRef image, bool hasAlpha, TextureLevel &dst)
{
    CGColorSpaceRef colorSpace = CGImageGetColorSpace(image);
    if (!colorSpace || CGColorSpaceGetModel(colorSpace) != kCGColorSpaceModelRGB)
        return false;
    if (CGImageGetBitsPerComponent(image) != 8)
        return false;

    // Byte order must be the natural R,G,B[,A] layout (reject 32Little/16-bit).
    const uint32_t byteOrder = CGImageGetBitmapInfo(image) & kCGBitmapByteOrderMask;
    if (byteOrder != kCGBitmapByteOrderDefault && byteOrder != kCGBitmapByteOrder32Big)
        return false;

    const CGImageAlphaInfo alpha = CGImageGetAlphaInfo(image);
    if (hasAlpha)
    {
        // Need straight (non-premultiplied) trailing alpha; premultiplied or
        // leading-alpha layouts go to the rasterization fallback.
        if (alpha != kCGImageAlphaLast)
            return false;
    }
    else if (alpha != kCGImageAlphaNone && alpha != kCGImageAlphaNoneSkipLast)
    {
        return false;  // RGB or RGBX only
    }

    CGDataProviderRef provider = CGImageGetDataProvider(image);
    if (!provider)
        return false;

    CFDataRef rawPixels = CGDataProviderCopyData(provider);
    if (!rawPixels)
        return false;
    ScopedCF<CFDataRef> pixels(rawPixels);

    const size_t width        = CGImageGetWidth(image);
    const size_t height       = CGImageGetHeight(image);
    const size_t srcPixelSize = CGImageGetBitsPerPixel(image) / 8;
    const size_t srcRowBytes  = CGImageGetBytesPerRow(image);

    // Bail (rasterize) unless the provider holds exactly this image's pixels --
    // a cropped CGImage shares its parent's larger buffer, so direct indexing
    // would read the wrong rows.
    if ((size_t)CFDataGetLength(pixels.get()) != height * srcRowBytes)
        return false;
    if (srcPixelSize < (hasAlpha ? 4u : 3u))
        return false;

    const uint8_t *srcData = CFDataGetBytePtr(pixels.get());
    uint8_t *dstData       = (uint8_t *)dst.getAccess().getDataPtr();
    const int dstRowPitch  = dst.getAccess().getRowPitch();
    const int dstPixelSize = hasAlpha ? 4 : 3;

    // CGImage backing data is top-row-first, matching tcu's row order.
    for (size_t y = 0; y < height; y++)
    {
        const uint8_t *srcRow = srcData + y * srcRowBytes;
        uint8_t *dstRow       = dstData + y * dstRowPitch;
        for (size_t x = 0; x < width; x++)
        {
            const uint8_t *s = srcRow + x * srcPixelSize;
            uint8_t *d       = dstRow + x * dstPixelSize;
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
            if (hasAlpha)
                d[3] = s[3];
        }
    }
    return true;
}
}  // namespace

/*--------------------------------------------------------------------*//*!
 * \brief Load image from resource
 *
 * TextureLevel storage is set to match image data. Only PNG format is
 * currently supported.
 *//*--------------------------------------------------------------------*/
void loadImage(TextureLevel &dst, const tcu::Archive &archive, const char *fileName)
{
    string ext = de::FilePath(fileName).getFileExtension();

    if (ext == "png" || ext == "PNG")
        loadPNG(dst, archive, fileName);
    else
        throw InternalError("Unrecognized image file extension", fileName, __FILE__, __LINE__);
}

/*--------------------------------------------------------------------*//*!
 * \brief Load PNG image from resource
 *
 * TextureLevel storage is set to match image data. Matches the upstream
 * libpng path: only 8-bit RGB and RGBA images are supported, with rows stored
 * top-to-bottom and straight (non-premultiplied) alpha.
 *//*--------------------------------------------------------------------*/
void loadPNG(TextureLevel &dst, const tcu::Archive &archive, const char *fileName)
{
    de::UniquePtr<Resource> resource(archive.getResource(fileName));

    const uint32_t numBytes = resource->getSize();
    vector<uint8_t> fileData(numBytes);
    resource->read(&fileData[0], (int)numBytes);

    ScopedCF<CFDataRef> data(
        CFDataCreate(kCFAllocatorDefault, &fileData[0], (CFIndex)numBytes));
    if (!data)
        throw InternalError("Failed to wrap PNG data", fileName, __FILE__, __LINE__);

    ScopedCF<CGImageSourceRef> source(
        CGImageSourceCreateWithData(data.get(), nullptr));
    if (!source)
        throw InternalError("Failed to create image source", fileName, __FILE__, __LINE__);

    ScopedCF<CGImageRef> image(
        CGImageSourceCreateImageAtIndex(source.get(), 0, nullptr));
    if (!image)
        throw InternalError("Failed to decode PNG", fileName, __FILE__, __LINE__);

    const size_t width  = CGImageGetWidth(image.get());
    const size_t height = CGImageGetHeight(image.get());

    const CGImageAlphaInfo alphaInfo = CGImageGetAlphaInfo(image.get());
    const bool hasAlpha = !(alphaInfo == kCGImageAlphaNone ||
                            alphaInfo == kCGImageAlphaNoneSkipLast ||
                            alphaInfo == kCGImageAlphaNoneSkipFirst);

    const TextureFormat textureFormat(hasAlpha ? TextureFormat::RGBA : TextureFormat::RGB,
                                      TextureFormat::UNORM_INT8);
    dst.setStorage(textureFormat, (int)width, (int)height);

    // Fast path: copy the decoded samples straight from the CGImage when its
    // layout already matches tcu's (8-bit RGB / straight RGBA). This is exact
    // (no color management, no premultiply round-trip). Otherwise fall back to
    // rasterizing into a staging buffer below.
    if (copyImagePixelsDirect(image.get(), hasAlpha, dst))
        return;

    // CoreGraphics cannot render to a 24-bit RGB bitmap and cannot produce
    // straight (non-premultiplied) alpha, so render into a tightly packed,
    // premultiplied RGBA8 staging buffer and convert. The vertical flip makes
    // memory row 0 the top of the image, matching tcu's expectation.
    vector<uint8_t> staging(width * height * 4);
    {
        ScopedCF<CGColorSpaceRef> colorSpace(CGColorSpaceCreateDeviceRGB());
        ScopedCF<CGContextRef> ctx(CGBitmapContextCreate(
            &staging[0], width, height, 8, width * 4, colorSpace.get(),
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big));
        if (!ctx)
            throw InternalError("Failed to create bitmap context", fileName, __FILE__, __LINE__);

        CGContextSetBlendMode(ctx.get(), kCGBlendModeCopy);
        CGContextTranslateCTM(ctx.get(), 0, (CGFloat)height);
        CGContextScaleCTM(ctx.get(), 1, -1);
        CGContextDrawImage(ctx.get(), CGRectMake(0, 0, (CGFloat)width, (CGFloat)height), image.get());
    }

    uint8_t *dstData       = (uint8_t *)dst.getAccess().getDataPtr();
    const int dstRowPitch  = dst.getAccess().getRowPitch();
    const int dstPixelSize = hasAlpha ? 4 : 3;

    for (size_t y = 0; y < height; y++)
    {
        const uint8_t *srcRow = &staging[y * width * 4];
        uint8_t *dstRow       = dstData + y * dstRowPitch;
        for (size_t x = 0; x < width; x++)
        {
            const uint8_t *s = srcRow + x * 4;
            uint8_t *d       = dstRow + x * dstPixelSize;
            uint8_t r = s[0], g = s[1], b = s[2];
            const uint8_t a = s[3];
            // Undo premultiplication to recover the straight colors libpng
            // would have produced (no-op for the opaque reference images).
            if (hasAlpha && a != 0 && a != 255)
            {
                r = (uint8_t)((r * 255 + a / 2) / a);
                g = (uint8_t)((g * 255 + a / 2) / a);
                b = (uint8_t)((b * 255 + a / 2) / a);
            }
            d[0] = r;
            d[1] = g;
            d[2] = b;
            if (hasAlpha)
                d[3] = a;
        }
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Write image to file in PNG format
 *
 * Provided for debugging and development purposes only.
 * \note Only RGB/RGBA, UNORM_INT8 formats are supported.
 *//*--------------------------------------------------------------------*/
void savePNG(const ConstPixelBufferAccess &src, const char *fileName)
{
    const TextureFormat &format = src.getFormat();
    const bool hasAlpha = (format == TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8));
    if (!hasAlpha && !(format == TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8)))
        throw InternalError("Unsupported texture format", nullptr, __FILE__, __LINE__);

    const int width      = src.getWidth();
    const int height     = src.getHeight();
    const int pixelSize  = hasAlpha ? 4 : 3;
    const int srcPitch   = src.getRowPitch();
    const uint8_t *srcPtr = (const uint8_t *)src.getDataPtr();

    // Pack into a tight top-to-bottom buffer for a CGImage.
    vector<uint8_t> packed((size_t)width * height * pixelSize);
    for (int y = 0; y < height; y++)
        memcpy(&packed[(size_t)y * width * pixelSize], srcPtr + y * srcPitch,
               (size_t)width * pixelSize);

    ScopedCF<CGDataProviderRef> provider(CGDataProviderCreateWithData(
        nullptr, &packed[0], packed.size(), nullptr));
    ScopedCF<CGColorSpaceRef> colorSpace(CGColorSpaceCreateDeviceRGB());
    if (!provider || !colorSpace)
        throw InternalError("Failed to create PNG image", fileName, __FILE__, __LINE__);

    const CGBitmapInfo bitmapInfo =
        (hasAlpha ? (CGBitmapInfo)kCGImageAlphaLast : (CGBitmapInfo)kCGImageAlphaNone) |
        kCGBitmapByteOrderDefault;

    ScopedCF<CGImageRef> image(CGImageCreate(
        width, height, 8, pixelSize * 8, (size_t)width * pixelSize, colorSpace.get(),
        bitmapInfo, provider.get(), nullptr, false, kCGRenderingIntentDefault));
    if (!image)
        throw InternalError("Failed to create PNG image", fileName, __FILE__, __LINE__);

    ScopedCF<CFStringRef> path(CFStringCreateWithCString(
        kCFAllocatorDefault, fileName, kCFStringEncodingUTF8));
    ScopedCF<CFURLRef> url(CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault, path.get(), kCFURLPOSIXPathStyle, false));
    if (!url)
        throw InternalError("Failed to build PNG path", fileName, __FILE__, __LINE__);

    ScopedCF<CGImageDestinationRef> dest(CGImageDestinationCreateWithURL(
        url.get(), CFSTR("public.png"), 1, nullptr));
    if (!dest)
        throw InternalError("Failed to create PNG destination", fileName, __FILE__, __LINE__);

    CGImageDestinationAddImage(dest.get(), image.get(), nullptr);
    if (!CGImageDestinationFinalize(dest.get()))
        throw InternalError("Failed to write PNG", fileName, __FILE__, __LINE__);
}

enum PkmImageFormat
{
    ETC1_RGB_NO_MIPMAPS  = 0,
    ETC1_RGBA_NO_MIPMAPS = 1,
    ETC1_RGB_MIPMAPS     = 2,
    ETC1_RGBA_MIPMAPS    = 3
};

static inline uint16_t readBigEndianShort(tcu::Resource *resource)
{
    uint16_t val;
    resource->read((uint8_t *)&val, sizeof(val));
    return (uint16_t)(((val >> 8) & 0xFF) | ((val << 8) & 0xFF00));
}

/*--------------------------------------------------------------------*//*!
 * \brief Load compressed image data from PKM file
 *
 * \note Only ETC1_RGB8_NO_MIPMAPS format is supported.
 *//*--------------------------------------------------------------------*/
void loadPKM(CompressedTexture &dst, const tcu::Archive &archive, const char *fileName)
{
    de::UniquePtr<Resource> resource(archive.getResource(fileName));

    // Check magic and version.
    uint8_t refMagic[] = {'P', 'K', 'M', ' ', '1', '0'};
    uint8_t magic[6];
    resource->read(magic, DE_LENGTH_OF_ARRAY(magic));

    if (memcmp(refMagic, magic, sizeof(magic)) != 0)
        throw InternalError("Signature doesn't match PKM signature", resource->getName().c_str(), __FILE__, __LINE__);

    uint16_t type = readBigEndianShort(resource.get());
    if (type != ETC1_RGB_NO_MIPMAPS)
        throw InternalError("Unsupported PKM type", resource->getName().c_str(), __FILE__, __LINE__);

    uint16_t width        = readBigEndianShort(resource.get());
    uint16_t height       = readBigEndianShort(resource.get());
    uint16_t activeWidth  = readBigEndianShort(resource.get());
    uint16_t activeHeight = readBigEndianShort(resource.get());

    DE_UNREF(width && height);

    dst.setStorage(COMPRESSEDTEXFORMAT_ETC1_RGB8, (int)activeWidth, (int)activeHeight);
    resource->read((uint8_t *)dst.getData(), dst.getDataSize());
}

}  // namespace ImageIO
}  // namespace tcu
