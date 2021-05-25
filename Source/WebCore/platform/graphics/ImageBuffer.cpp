/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#include "HostWindow.h"
#include "PlatformImageBuffer.h"

namespace WebCore {

static const float MaxClampedLength = 4096;
static const float MaxClampedArea = MaxClampedLength * MaxClampedLength;

RefPtr<ImageBuffer> ImageBuffer::create(const FloatSize& size, RenderingMode renderingMode, ShouldUseDisplayList shouldUseDisplayList, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, const HostWindow* hostWindow)
{
    RefPtr<ImageBuffer> imageBuffer;
    
    // Give ShouldUseDisplayList a higher precedence since it is a debug option.
    if (shouldUseDisplayList == ShouldUseDisplayList::Yes) {
        if (renderingMode == RenderingMode::Accelerated)
            imageBuffer = DisplayListAcceleratedImageBuffer::create(size, resolutionScale, colorSpace, pixelFormat, hostWindow);
        
        if (!imageBuffer)
            imageBuffer = DisplayListUnacceleratedImageBuffer::create(size, resolutionScale, colorSpace, pixelFormat, hostWindow);
    }
    
    if (hostWindow && !imageBuffer)
        imageBuffer = hostWindow->createImageBuffer(size, renderingMode, purpose, resolutionScale, colorSpace, pixelFormat);

    if (!imageBuffer)
        imageBuffer = ImageBuffer::create(size, renderingMode, resolutionScale, colorSpace, pixelFormat, hostWindow);

    return imageBuffer;
}

RefPtr<ImageBuffer> ImageBuffer::create(const FloatSize& size, RenderingMode renderingMode, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, const HostWindow* hostWindow)
{
    RefPtr<ImageBuffer> imageBuffer;
    
    if (renderingMode == RenderingMode::Accelerated)
        imageBuffer = AcceleratedImageBuffer::create(size, resolutionScale, colorSpace, pixelFormat, hostWindow);
    
    if (!imageBuffer)
        imageBuffer = UnacceleratedImageBuffer::create(size, resolutionScale, colorSpace, pixelFormat, hostWindow);

    return imageBuffer;
}

RefPtr<ImageBuffer> ImageBuffer::createCompatibleBuffer(const FloatSize& size, const GraphicsContext& context)
{
    if (size.isEmpty())
        return nullptr;

    IntSize scaledSize = ImageBuffer::compatibleBufferSize(size, context);

    RefPtr<ImageBuffer> imageBuffer;

    if (context.renderingMode() == RenderingMode::Accelerated)
        imageBuffer = AcceleratedImageBuffer::create(scaledSize, context);
    
    if (!imageBuffer)
        imageBuffer = UnacceleratedImageBuffer::create(scaledSize, context);

    if (!imageBuffer)
        return nullptr;

    // Set up a corresponding scale factor on the graphics context.
    imageBuffer->context().scale(scaledSize / size);
    return imageBuffer;
}

RefPtr<ImageBuffer> ImageBuffer::createCompatibleBuffer(const FloatSize& size, const DestinationColorSpace& colorSpace, const GraphicsContext& context)
{
    if (size.isEmpty())
        return nullptr;

    IntSize scaledSize = ImageBuffer::compatibleBufferSize(size, context);

    auto imageBuffer = ImageBuffer::createCompatibleBuffer(scaledSize, 1, colorSpace, context);
    if (!imageBuffer)
        return nullptr;

    // Set up a corresponding scale factor on the graphics context.
    imageBuffer->context().scale(scaledSize / size);
    return imageBuffer;
}

RefPtr<ImageBuffer> ImageBuffer::createCompatibleBuffer(const FloatSize& size, float resolutionScale, const DestinationColorSpace& colorSpace, const GraphicsContext& context)
{
    return ImageBuffer::create(size, context.renderingMode(), resolutionScale, colorSpace, PixelFormat::BGRA8);
}

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
    scale = clampedSize / size;
    ASSERT(!sizeNeedsClamping(clampedSize));
    ASSERT(!sizeNeedsClamping(size, scale));
    return clampedSize;
}

FloatRect ImageBuffer::clampedRect(const FloatRect& rect)
{
    return FloatRect(rect.location(), clampedSize(rect.size()));
}

IntSize ImageBuffer::compatibleBufferSize(const FloatSize& size, const GraphicsContext& context)
{
    // Enlarge the buffer size if the context's transform is scaling it so we need a higher
    // resolution than one pixel per unit.
    return expandedIntSize(size * context.scaleFactor());
}

RefPtr<ImageBuffer> ImageBuffer::copyRectToBuffer(const FloatRect& rect, const DestinationColorSpace& colorSpace, const GraphicsContext& context)
{
    if (rect.isEmpty())
        return nullptr;

    IntSize scaledSize = ImageBuffer::compatibleBufferSize(rect.size(), context);

    auto buffer = ImageBuffer::createCompatibleBuffer(scaledSize, 1, colorSpace, context);
    if (!buffer)
        return nullptr;

    buffer->context().drawImageBuffer(*this, -rect.location());
    return buffer;
}

RefPtr<NativeImage> ImageBuffer::sinkIntoNativeImage(RefPtr<ImageBuffer> imageBuffer)
{
    return imageBuffer->sinkIntoNativeImage();
}

RefPtr<Image> ImageBuffer::sinkIntoImage(RefPtr<ImageBuffer> imageBuffer, PreserveResolution preserveResolution)
{
    return imageBuffer->sinkIntoImage(preserveResolution);
}

void ImageBuffer::drawConsuming(RefPtr<ImageBuffer> imageBuffer, GraphicsContext& context, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    imageBuffer->drawConsuming(context, destRect, srcRect, options);
}

} // namespace WebCore
