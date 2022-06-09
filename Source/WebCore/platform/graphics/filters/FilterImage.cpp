/*
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
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
#include "FilterImage.h"

#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PixelBuffer.h"
#include "PixelBufferConversion.h"

#if HAVE(ARM_NEON_INTRINSICS)
#include <arm_neon.h>
#endif

namespace WebCore {

RefPtr<FilterImage> FilterImage::create(const FloatRect& primitiveSubregion, const FloatRect& imageRect, const IntRect& absoluteImageRect, bool isAlphaImage, bool isValidPremultiplied, RenderingMode renderingMode, const DestinationColorSpace& colorSpace)
{
    ASSERT(!ImageBuffer::sizeNeedsClamping(absoluteImageRect.size()));
    return adoptRef(new FilterImage(primitiveSubregion, imageRect, absoluteImageRect, isAlphaImage, isValidPremultiplied, renderingMode, colorSpace));
}

RefPtr<FilterImage> FilterImage::create(const FloatRect& primitiveSubregion, const FloatRect& imageRect, const IntRect& absoluteImageRect, Ref<ImageBuffer>&& imageBuffer)
{
    return adoptRef(*new FilterImage(primitiveSubregion, imageRect, absoluteImageRect, WTFMove(imageBuffer)));
}

FilterImage::FilterImage(const FloatRect& primitiveSubregion, const FloatRect& imageRect, const IntRect& absoluteImageRect, bool isAlphaImage, bool isValidPremultiplied, RenderingMode renderingMode, const DestinationColorSpace& colorSpace)
    : m_primitiveSubregion(primitiveSubregion)
    , m_imageRect(imageRect)
    , m_absoluteImageRect(absoluteImageRect)
    , m_isAlphaImage(isAlphaImage)
    , m_isValidPremultiplied(isValidPremultiplied)
    , m_renderingMode(renderingMode)
    , m_colorSpace(colorSpace)
{
}

FilterImage::FilterImage(const FloatRect& primitiveSubregion, const FloatRect& imageRect, const IntRect& absoluteImageRect, Ref<ImageBuffer>&& imageBuffer)
    : m_primitiveSubregion(primitiveSubregion)
    , m_imageRect(imageRect)
    , m_absoluteImageRect(absoluteImageRect)
    , m_renderingMode(imageBuffer->renderingMode())
    , m_colorSpace(imageBuffer->colorSpace())
    , m_imageBuffer(WTFMove(imageBuffer))
{
}

FloatRect FilterImage::maxEffectRect(const Filter& filter) const
{
    return filter.maxEffectRect(m_primitiveSubregion);
}

IntRect FilterImage::absoluteImageRectRelativeTo(const FilterImage& origin) const
{
    return m_absoluteImageRect - origin.absoluteImageRect().location();
}

FloatPoint FilterImage::mappedAbsolutePoint(const FloatPoint& point) const
{
    return FloatPoint(point - m_absoluteImageRect.location());
}

ImageBuffer* FilterImage::imageBuffer()
{
#if USE(CORE_IMAGE)
    if (m_ciImage)
        return imageBufferFromCIImage();
#endif
    return imageBufferFromPixelBuffer();
}

ImageBuffer* FilterImage::imageBufferFromPixelBuffer()
{
    if (m_imageBuffer)
        return m_imageBuffer.get();

    m_imageBuffer = ImageBuffer::create(m_absoluteImageRect.size(), m_renderingMode, 1, m_colorSpace, PixelFormat::BGRA8);
    if (!m_imageBuffer)
        return nullptr;

    auto imageBufferRect = IntRect { { }, m_absoluteImageRect.size() };

    if (pixelBufferSlot(AlphaPremultiplication::Premultiplied))
        m_imageBuffer->putPixelBuffer(*pixelBufferSlot(AlphaPremultiplication::Premultiplied), imageBufferRect);
    else if (pixelBufferSlot(AlphaPremultiplication::Unpremultiplied))
        m_imageBuffer->putPixelBuffer(*pixelBufferSlot(AlphaPremultiplication::Unpremultiplied), imageBufferRect);

    return m_imageBuffer.get();
}

static void copyImageBytes(const PixelBuffer& sourcePixelBuffer, PixelBuffer& destinationPixelBuffer)
{
    ASSERT(sourcePixelBuffer.size() == destinationPixelBuffer.size());

    auto destinationSize = destinationPixelBuffer.size();
    unsigned rowBytes = destinationSize.width() * 4;

    ConstPixelBufferConversionView source { sourcePixelBuffer.format(), rowBytes, sourcePixelBuffer.data().data() };
    PixelBufferConversionView destination { destinationPixelBuffer.format(), rowBytes, destinationPixelBuffer.data().data() };

    convertImagePixels(source, destination, destinationSize);
}

static void copyImageBytes(const PixelBuffer& sourcePixelBuffer, PixelBuffer& destinationPixelBuffer, const IntRect& sourceRect)
{
    auto& source = sourcePixelBuffer.data();
    auto& destination = destinationPixelBuffer.data();

    auto sourcePixelBufferRect = IntRect { { }, sourcePixelBuffer.size() };
    auto destinationPixelBufferRect = IntRect { { }, destinationPixelBuffer.size() };

    auto sourceRectClipped = intersection(sourcePixelBufferRect, sourceRect);
    auto destinationRect = IntRect { { }, sourceRectClipped.size() };

    if (sourceRect.x() < 0)
        destinationRect.setX(-sourceRect.x());

    if (sourceRect.y() < 0)
        destinationRect.setY(-sourceRect.y());

    destinationRect.intersect(destinationPixelBufferRect);
    sourceRectClipped.setSize(destinationRect.size());

    // Initialize the destination to transparent black, if not entirely covered by the source.
    if (destinationRect.size() != destinationPixelBufferRect.size())
        destination.zeroFill();

    // Early return if the rect does not intersect with the source.
    if (destinationRect.isEmpty())
        return;

    int size = sourceRectClipped.width() * 4;
    int destinationBytesPerRow = destinationPixelBufferRect.width() * 4;
    int sourceBytesPerRow = sourcePixelBufferRect.width() * 4;
    uint8_t* destinationPixel = destination.data() + destinationRect.y() * destinationBytesPerRow + destinationRect.x() * 4;
    const uint8_t* sourcePixel = source.data() + sourceRectClipped.y() * sourceBytesPerRow + sourceRectClipped.x() * 4;

    for (int y = 0; y < sourceRectClipped.height(); ++y) {
        memcpy(destinationPixel, sourcePixel, size);
        destinationPixel += destinationBytesPerRow;
        sourcePixel += sourceBytesPerRow;
    }
}

static std::optional<PixelBuffer> getConvertedPixelBuffer(ImageBuffer& imageBuffer, AlphaPremultiplication alphaFormat, const IntRect& sourceRect, DestinationColorSpace colorSpace)
{
    auto clampedSize = ImageBuffer::clampedSize(sourceRect.size());
    auto convertedImageBuffer = ImageBuffer::create(clampedSize, RenderingMode::Unaccelerated, 1, colorSpace, PixelFormat::BGRA8);
    
    if (!convertedImageBuffer)
        return std::nullopt;

    // Color space conversion happens internally when drawing from one image buffer to another
    convertedImageBuffer->context().drawImageBuffer(imageBuffer, sourceRect);
    PixelBufferFormat format { alphaFormat, PixelFormat::RGBA8, colorSpace };
    return convertedImageBuffer->getPixelBuffer(format, sourceRect);
}

static std::optional<PixelBuffer> getConvertedPixelBuffer(PixelBuffer& sourcePixelBuffer, AlphaPremultiplication alphaFormat, DestinationColorSpace colorSpace)
{
    auto sourceRect = IntRect { { } , sourcePixelBuffer.size() };
    auto clampedSize = ImageBuffer::clampedSize(sourceRect.size());

    auto& sourceColorSpace = sourcePixelBuffer.format().colorSpace;
    auto imageBuffer = ImageBuffer::create(clampedSize, RenderingMode::Unaccelerated, 1, sourceColorSpace, PixelFormat::BGRA8);
    if (!imageBuffer)
        return std::nullopt;

    imageBuffer->putPixelBuffer(sourcePixelBuffer, sourceRect);
    return getConvertedPixelBuffer(*imageBuffer, alphaFormat, sourceRect, colorSpace);
}

bool FilterImage::requiresPixelBufferColorSpaceConversion(std::optional<DestinationColorSpace> colorSpace) const
{
#if USE(CG)
    // This function determines whether we need the step of an extra color space conversion
    // We only need extra color conversion when 1) color space is different in the input
    // AND 2) the filter is manipulating raw pixels
    return colorSpace && m_colorSpace != *colorSpace;
#else
    // Additional color space conversion is not needed on non-CG
    UNUSED_PARAM(colorSpace);
    return false;
#endif
}

std::optional<PixelBuffer>& FilterImage::pixelBufferSlot(AlphaPremultiplication alphaFormat)
{
    return alphaFormat == AlphaPremultiplication::Unpremultiplied ? m_unpremultipliedPixelBuffer : m_premultipliedPixelBuffer;
}

PixelBuffer* FilterImage::pixelBuffer(AlphaPremultiplication alphaFormat)
{
    auto& pixelBuffer = pixelBufferSlot(alphaFormat);
    if (pixelBuffer)
        return &pixelBuffer.value();

    PixelBufferFormat format { alphaFormat, PixelFormat::RGBA8, m_colorSpace };

    if (m_imageBuffer) {
        pixelBuffer = m_imageBuffer->getPixelBuffer(format, { { }, m_absoluteImageRect.size() });
        if (!pixelBuffer)
            return nullptr;
        return &pixelBuffer.value();
    }

    IntSize logicalSize(m_absoluteImageRect.size());
    ASSERT(!ImageBuffer::sizeNeedsClamping(logicalSize));

    pixelBuffer = PixelBuffer::tryCreate(format, logicalSize);
    if (!pixelBuffer)
        return nullptr;

    if (alphaFormat == AlphaPremultiplication::Unpremultiplied) {
        if (auto& sourcePixelBuffer = pixelBufferSlot(AlphaPremultiplication::Premultiplied))
            copyImageBytes(*sourcePixelBuffer, *pixelBuffer);
    } else {
        if (auto& sourcePixelBuffer = pixelBufferSlot(AlphaPremultiplication::Unpremultiplied))
            copyImageBytes(*sourcePixelBuffer, *pixelBuffer);
    }

    return &pixelBuffer.value();
}

std::optional<PixelBuffer> FilterImage::getPixelBuffer(AlphaPremultiplication alphaFormat, const IntRect& sourceRect, std::optional<DestinationColorSpace> colorSpace)
{
    ASSERT(!ImageBuffer::sizeNeedsClamping(sourceRect.size()));

    PixelBufferFormat format { alphaFormat, PixelFormat::RGBA8, colorSpace? *colorSpace : m_colorSpace };

    auto pixelBuffer = PixelBuffer::tryCreate(format, sourceRect.size());
    if (!pixelBuffer)
        return std::nullopt;

    copyPixelBuffer(*pixelBuffer, sourceRect);
    return pixelBuffer;
}

void FilterImage::copyPixelBuffer(PixelBuffer& destinationPixelBuffer, const IntRect& sourceRect)
{
    auto alphaFormat = destinationPixelBuffer.format().alphaFormat;
    auto& colorSpace = destinationPixelBuffer.format().colorSpace;

    auto* sourcePixelBuffer = pixelBufferSlot(alphaFormat) ? &pixelBufferSlot(alphaFormat).value() : nullptr;

    if (!sourcePixelBuffer) {
        if (requiresPixelBufferColorSpaceConversion(colorSpace)) {
            // We prefer a conversion from the image buffer.
            if (m_imageBuffer) {
                IntRect rect { { }, m_absoluteImageRect.size() };
                if (auto convertedPixelBuffer = getConvertedPixelBuffer(*m_imageBuffer, alphaFormat, rect, colorSpace))
                    copyImageBytes(*convertedPixelBuffer, destinationPixelBuffer, sourceRect);
                return;
            }
        }

        sourcePixelBuffer = this->pixelBuffer(alphaFormat);
    }

    if (!sourcePixelBuffer)
        return;

    if (requiresPixelBufferColorSpaceConversion(colorSpace)) {
        if (auto convertedPixelBuffer = getConvertedPixelBuffer(*sourcePixelBuffer, alphaFormat, colorSpace))
            copyImageBytes(*convertedPixelBuffer, destinationPixelBuffer, sourceRect);
        return;
    }

    copyImageBytes(*sourcePixelBuffer, destinationPixelBuffer, sourceRect);
}

void FilterImage::correctPremultipliedPixelBuffer()
{
    // Must operate on pre-multiplied results; other formats cannot have invalid pixels.
    if (!m_premultipliedPixelBuffer || m_isValidPremultiplied)
        return;

    Uint8ClampedArray& imageArray = m_premultipliedPixelBuffer->data();
    uint8_t* pixelData = imageArray.data();
    int pixelArrayLength = imageArray.length();

    // We must have four bytes per pixel, and complete pixels
    ASSERT(!(pixelArrayLength % 4));

#if HAVE(ARM_NEON_INTRINSICS)
    if (pixelArrayLength >= 64) {
        uint8_t* lastPixel = pixelData + (pixelArrayLength & ~0x3f);
        do {
            // Increments pixelData by 64.
            uint8x16x4_t sixteenPixels = vld4q_u8(pixelData);
            sixteenPixels.val[0] = vminq_u8(sixteenPixels.val[0], sixteenPixels.val[3]);
            sixteenPixels.val[1] = vminq_u8(sixteenPixels.val[1], sixteenPixels.val[3]);
            sixteenPixels.val[2] = vminq_u8(sixteenPixels.val[2], sixteenPixels.val[3]);
            vst4q_u8(pixelData, sixteenPixels);
            pixelData += 64;
        } while (pixelData < lastPixel);

        pixelArrayLength &= 0x3f;
        if (!pixelArrayLength)
            return;
    }
#endif

    int numPixels = pixelArrayLength / 4;

    // Iterate over each pixel, checking alpha and adjusting color components if necessary
    while (--numPixels >= 0) {
        // Alpha is the 4th byte in a pixel
        uint8_t a = *(pixelData + 3);
        // Clamp each component to alpha, and increment the pixel location
        for (int i = 0; i < 3; ++i) {
            if (*pixelData > a)
                *pixelData = a;
            ++pixelData;
        }
        // Increment for alpha
        ++pixelData;
    }
}

void FilterImage::transformToColorSpace(const DestinationColorSpace& colorSpace)
{
#if USE(CG)
    // CG handles color space adjustments internally.
    UNUSED_PARAM(colorSpace);
#else
    if (colorSpace == m_colorSpace)
        return;

    // FIXME: We can avoid this potentially unnecessary ImageBuffer conversion by adding
    // color space transform support for the {pre,un}multiplied arrays.
    if (auto imageBuffer = this->imageBuffer())
        imageBuffer->transformToColorSpace(colorSpace);

    m_colorSpace = colorSpace;
    m_unpremultipliedPixelBuffer = std::nullopt;
    m_premultipliedPixelBuffer = std::nullopt;
#endif
}

} // namespace WebCore
