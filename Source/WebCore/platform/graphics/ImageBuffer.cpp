/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include "BitmapImage.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "PlatformImageBuffer.h"

namespace WebCore {

static const float MaxClampedLength = 4096;
static const float MaxClampedArea = MaxClampedLength * MaxClampedLength;

RefPtr<ImageBuffer> ImageBuffer::create(const FloatSize& size, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, OptionSet<ImageBufferOptions> options, const CreationContext& creationContext)
{
    RefPtr<ImageBuffer> imageBuffer;
    
    // Give UseDisplayList a higher precedence since it is a debug option.
    if (options.contains(ImageBufferOptions::UseDisplayList)) {
        if (options.contains(ImageBufferOptions::Accelerated))
            imageBuffer = DisplayListAcceleratedImageBuffer::create(size, resolutionScale, colorSpace, pixelFormat, purpose, creationContext);
        
        if (!imageBuffer)
            imageBuffer = DisplayListUnacceleratedImageBuffer::create(size, resolutionScale, colorSpace, pixelFormat, purpose, creationContext);
    }
    
    if (creationContext.hostWindow && !imageBuffer) {
        auto renderingMode = options.contains(ImageBufferOptions::Accelerated) ? RenderingMode::Accelerated : RenderingMode::Unaccelerated;
        imageBuffer = creationContext.hostWindow->createImageBuffer(size, renderingMode, purpose, resolutionScale, colorSpace, pixelFormat);
    }

    if (imageBuffer)
        return imageBuffer;

    if (options.contains(ImageBufferOptions::Accelerated))
        imageBuffer = AcceleratedImageBuffer::create(size, resolutionScale, colorSpace, pixelFormat, purpose, creationContext);
    
    if (!imageBuffer)
        imageBuffer = UnacceleratedImageBuffer::create(size, resolutionScale, colorSpace, pixelFormat, purpose, creationContext);

    return imageBuffer;
}

RefPtr<ImageBuffer> ImageBuffer::clone() const
{
    auto clone = context().createAlignedImageBuffer(logicalSize(), colorSpace());
    if (!clone)
        return nullptr;

    clone->context().drawImageBuffer(const_cast<ImageBuffer&>(*this), FloatPoint());
    return clone;
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

RefPtr<NativeImage> ImageBuffer::sinkIntoNativeImage(RefPtr<ImageBuffer> source)
{
    if (!source)
        return nullptr;
    return source->sinkIntoNativeImage();
}

RefPtr<Image> ImageBuffer::copyImage(BackingStoreCopy copyBehavior, PreserveResolution preserveResolution) const
{
    RefPtr<NativeImage> image;
    if (resolutionScale() == 1 || preserveResolution == PreserveResolution::Yes)
        image = copyNativeImage(copyBehavior);
    else {
        auto copyBuffer = context().createImageBuffer(logicalSize(), 1.f, colorSpace());
        if (!copyBuffer)
            return nullptr;
        copyBuffer->context().drawImageBuffer(const_cast<ImageBuffer&>(*this), FloatPoint { }, CompositeOperator::Copy);
        image = ImageBuffer::sinkIntoNativeImage(WTFMove(copyBuffer));
    }
    if (!image)
        return nullptr;
    return BitmapImage::create(image.releaseNonNull());
}

RefPtr<Image> ImageBuffer::sinkIntoImage(RefPtr<ImageBuffer> source, PreserveResolution preserveResolution)
{
    if (!source)
        return nullptr;
    RefPtr<NativeImage> image;
    if (source->resolutionScale() == 1 || preserveResolution == PreserveResolution::Yes)
        image = sinkIntoNativeImage(WTFMove(source));
    else {
        auto copySize = source->logicalSize();
        auto copyBuffer = source->context().createImageBuffer(copySize, 1.f, source->colorSpace());
        if (!copyBuffer)
            return nullptr;
        drawConsuming(WTFMove(source), copyBuffer->context(), FloatRect { { }, copySize }, FloatRect { 0, 0, -1, -1 }, CompositeOperator::Copy);
        image = ImageBuffer::sinkIntoNativeImage(WTFMove(copyBuffer));
    }
    if (!image)
        return nullptr;
    return BitmapImage::create(image.releaseNonNull());
}

void ImageBuffer::drawConsuming(RefPtr<ImageBuffer> imageBuffer, GraphicsContext& context, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    imageBuffer->drawConsuming(context, destRect, srcRect, options);
}

} // namespace WebCore
