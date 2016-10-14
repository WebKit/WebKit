/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "ImageBuffer.h"

#include "GraphicsContext.h"
#include "IntRect.h"
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const float MaxClampedLength = 4096;
static const float MaxClampedArea = MaxClampedLength * MaxClampedLength;

std::unique_ptr<ImageBuffer> ImageBuffer::create(const FloatSize& size, RenderingMode renderingMode, float resolutionScale, ColorSpace colorSpace)
{
    bool success = false;
    std::unique_ptr<ImageBuffer> buffer(new ImageBuffer(size, resolutionScale, colorSpace, renderingMode, success));
    if (!success)
        return nullptr;
    return buffer;
}

#if USE(DIRECT2D)
std::unique_ptr<ImageBuffer> ImageBuffer::create(const FloatSize& size, RenderingMode renderingMode, const GraphicsContext* targetContext, float resolutionScale, ColorSpace colorSpace)
{
    bool success = false;
    std::unique_ptr<ImageBuffer> buffer(new ImageBuffer(size, resolutionScale, colorSpace, renderingMode, targetContext, success));
    if (!success)
        return nullptr;
    return buffer;
}
#endif

bool ImageBuffer::sizeNeedsClamping(const FloatSize& size)
{
    if (size.isEmpty())
        return false;

    return floorf(size.height()) * floorf(size.width()) > MaxClampedArea;
}

bool ImageBuffer::sizeNeedsClamping(const FloatSize& size, FloatSize& scale)
{
    FloatSize scaledSize(size);
    scaledSize.scale(scale.width(), scale.height());

    if (!sizeNeedsClamping(scaledSize))
        return false;

    // The area of scaled size is bigger than the upper limit, adjust the scale to fit.
    scale.scale(sqrtf(MaxClampedArea / (scaledSize.width() * scaledSize.height())));
    ASSERT(!sizeNeedsClamping(size, scale));
    return true;
}

FloatSize ImageBuffer::clampedSize(const FloatSize& size)
{
    return size.shrunkTo(FloatSize(MaxClampedLength, MaxClampedLength));
}

FloatSize ImageBuffer::clampedSize(const FloatSize& size, FloatSize& scale)
{
    if (size.isEmpty())
        return size;

    FloatSize clampedSize = ImageBuffer::clampedSize(size);
    scale = FloatSize(clampedSize.width() / size.width(), clampedSize.height() / size.height());
    ASSERT(!sizeNeedsClamping(clampedSize));
    ASSERT(!sizeNeedsClamping(size, scale));
    return clampedSize;
}

FloatRect ImageBuffer::clampedRect(const FloatRect& rect)
{
    return FloatRect(rect.location(), clampedSize(rect.size()));
}

#if !(USE(CG) || USE(DIRECT2D))
FloatSize ImageBuffer::sizeForDestinationSize(FloatSize size) const
{
    return size;
}

void ImageBuffer::transformColorSpace(ColorSpace srcColorSpace, ColorSpace dstColorSpace)
{
    static NeverDestroyed<Vector<int>> deviceRgbLUT;
    static NeverDestroyed<Vector<int>> linearRgbLUT;

    if (srcColorSpace == dstColorSpace)
        return;

    // only sRGB <-> linearRGB are supported at the moment
    if ((srcColorSpace != ColorSpaceLinearRGB && srcColorSpace != ColorSpaceDeviceRGB)
        || (dstColorSpace != ColorSpaceLinearRGB && dstColorSpace != ColorSpaceDeviceRGB))
        return;

    if (dstColorSpace == ColorSpaceLinearRGB) {
        if (linearRgbLUT.get().isEmpty()) {
            for (unsigned i = 0; i < 256; i++) {
                float color = i  / 255.0f;
                color = (color <= 0.04045f ? color / 12.92f : pow((color + 0.055f) / 1.055f, 2.4f));
                color = std::max(0.0f, color);
                color = std::min(1.0f, color);
                linearRgbLUT.get().append(static_cast<int>(round(color * 255)));
            }
        }
        platformTransformColorSpace(linearRgbLUT.get());
    } else if (dstColorSpace == ColorSpaceDeviceRGB) {
        if (deviceRgbLUT.get().isEmpty()) {
            for (unsigned i = 0; i < 256; i++) {
                float color = i / 255.0f;
                color = (powf(color, 1.0f / 2.4f) * 1.055f) - 0.055f;
                color = std::max(0.0f, color);
                color = std::min(1.0f, color);
                deviceRgbLUT.get().append(static_cast<int>(round(color * 255)));
            }
        }
        platformTransformColorSpace(deviceRgbLUT.get());
    }
}
#endif // USE(CG)

inline void ImageBuffer::genericConvertToLuminanceMask()
{
    IntRect luminanceRect(IntPoint(), internalSize());
    RefPtr<Uint8ClampedArray> srcPixelArray = getUnmultipliedImageData(luminanceRect);
    
    unsigned pixelArrayLength = srcPixelArray->length();
    for (unsigned pixelOffset = 0; pixelOffset < pixelArrayLength; pixelOffset += 4) {
        unsigned char a = srcPixelArray->item(pixelOffset + 3);
        if (!a)
            continue;
        unsigned char r = srcPixelArray->item(pixelOffset);
        unsigned char g = srcPixelArray->item(pixelOffset + 1);
        unsigned char b = srcPixelArray->item(pixelOffset + 2);
        
        double luma = (r * 0.2125 + g * 0.7154 + b * 0.0721) * ((double)a / 255.0);
        srcPixelArray->set(pixelOffset + 3, luma);
    }
    putByteArray(Unmultiplied, srcPixelArray.get(), luminanceRect.size(), luminanceRect, IntPoint());
}

void ImageBuffer::convertToLuminanceMask()
{
    // Add platform specific functions with platformConvertToLuminanceMask here later.
    genericConvertToLuminanceMask();
}

#if !USE(CAIRO)
PlatformLayer* ImageBuffer::platformLayer() const
{
    return 0;
}

bool ImageBuffer::copyToPlatformTexture(GraphicsContext3D&, GC3Denum, Platform3DObject, GC3Denum, bool, bool)
{
    return false;
}
#endif

std::unique_ptr<ImageBuffer> ImageBuffer::createCompatibleBuffer(const FloatSize& size, ColorSpace colorSpace, const GraphicsContext& context)
{
    if (size.isEmpty())
        return nullptr;

    IntSize scaledSize = ImageBuffer::compatibleBufferSize(size, context);

    auto buffer = ImageBuffer::createCompatibleBuffer(scaledSize, 1, colorSpace, context);
    if (!buffer)
        return nullptr;

    // Set up a corresponding scale factor on the graphics context.
    buffer->context().scale(FloatSize(scaledSize.width() / size.width(), scaledSize.height() / size.height()));
    return buffer;
}

std::unique_ptr<ImageBuffer> ImageBuffer::createCompatibleBuffer(const FloatSize& size, float resolutionScale, ColorSpace colorSpace, const GraphicsContext& context)
{
    return create(size, context.renderingMode(), resolutionScale, colorSpace);
}

IntSize ImageBuffer::compatibleBufferSize(const FloatSize& size, const GraphicsContext& context)
{
    // Enlarge the buffer size if the context's transform is scaling it so we need a higher
    // resolution than one pixel per unit.
    return expandedIntSize(size * context.scaleFactor());
}

bool ImageBuffer::isCompatibleWithContext(const GraphicsContext& context) const
{
    return areEssentiallyEqual(context.scaleFactor(), this->context().scaleFactor());
}

#if !USE(IOSURFACE_CANVAS_BACKING_STORE)
size_t ImageBuffer::memoryCost() const
{
    return 4 * internalSize().width() * internalSize().height();
}

size_t ImageBuffer::externalMemoryCost() const
{
    return 0;
}
#endif

}
