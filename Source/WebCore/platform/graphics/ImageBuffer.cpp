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
#include <wtf/text/Base64.h>

#if USE(CG)
#include "ImageBufferUtilitiesCG.h"
#endif
#if USE(CAIRO)
#include "ImageBufferUtilitiesCairo.h"
#endif

namespace WebCore {

static const float MaxClampedLength = 4096;
static const float MaxClampedArea = MaxClampedLength * MaxClampedLength;

static RefPtr<ImageBuffer> copyImageBuffer(Ref<ImageBuffer> source, PreserveResolution preserveResolution)
{
    if (source->resolutionScale() == 1 || preserveResolution == PreserveResolution::Yes) {
        if (source->hasOneRef())
            return source;
    }
    auto copySize = source->logicalSize();
    auto copyScale = preserveResolution == PreserveResolution::Yes ? source->resolutionScale() : 1.f;
    auto copyBuffer = source->context().createImageBuffer(copySize, copyScale, source->colorSpace());
    if (!copyBuffer)
        return nullptr;
    if (source->hasOneRef())
        ImageBuffer::drawConsuming(WTFMove(source), copyBuffer->context(), FloatRect { { }, copySize }, FloatRect { 0, 0, -1, -1 }, CompositeOperator::Copy);
    else
        copyBuffer->context().drawImageBuffer(source, FloatPoint { }, CompositeOperator::Copy);
    return copyBuffer;
}

static RefPtr<NativeImage> copyNativeImageImpl(Ref<ImageBuffer> source, BackingStoreCopy copyBehavior, PreserveResolution preserveResolution)
{
    if (source->resolutionScale() == 1 || preserveResolution == PreserveResolution::Yes) {
        if (source->hasOneRef())
            return ImageBuffer::sinkIntoNativeImage(WTFMove(source));
        return source->copyNativeImage(copyBehavior);
    }
    auto copyBuffer = copyImageBuffer(WTFMove(source), preserveResolution);
    if (!copyBuffer)
        return nullptr;
    return ImageBuffer::sinkIntoNativeImage(WTFMove(copyBuffer));
}

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
    auto image = copyNativeImageImpl(const_cast<ImageBuffer&>(*this), copyBehavior, preserveResolution);
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

String ImageBuffer::toDataURL(const String& mimeType, std::optional<double> quality, PreserveResolution preserveResolution) const
{
    return toDataURL(Ref { const_cast<ImageBuffer&>(*this) }, mimeType, quality, preserveResolution);
}

Vector<uint8_t> ImageBuffer::toData(const String& mimeType, std::optional<double> quality, PreserveResolution preserveResolution) const
{
    return toData(Ref { const_cast<ImageBuffer&>(*this) }, mimeType, quality, preserveResolution);
}

String ImageBuffer::toDataURL(Ref<ImageBuffer> source, const String& mimeType, std::optional<double> quality, PreserveResolution preserveResolution)
{
    auto encodedData = toData(WTFMove(source), mimeType, quality, preserveResolution);
    if (encodedData.isEmpty())
        return "data:,"_s;
    return makeString("data:", mimeType, ";base64,", base64Encoded(encodedData));
}

Vector<uint8_t> ImageBuffer::toData(Ref<ImageBuffer> source, const String& mimeType, std::optional<double> quality, PreserveResolution preserveResolution)
{
    RefPtr<NativeImage> image;
    if (!platformMIMETypeIsJPEG(mimeType))
        image = copyNativeImageImpl(WTFMove(source), DontCopyBackingStore, preserveResolution);
    else {
        // Composite this ImageBuffer on top of opaque black, because JPEG does not have an alpha channel.
        auto copyBuffer = copyImageBuffer(WTFMove(source), preserveResolution);
        if (!copyBuffer)
            return { };
        // We composite the copy on top of black by drawing black under the copy.
        copyBuffer->context().fillRect({ { }, copyBuffer->logicalSize() }, Color::black, CompositeOperator::DestinationOver);
        image = sinkIntoNativeImage(WTFMove(copyBuffer));
    }
    if (!image)
        return { };
    return data(image->platformImage().get(), mimeType, quality);
}

} // namespace WebCore
