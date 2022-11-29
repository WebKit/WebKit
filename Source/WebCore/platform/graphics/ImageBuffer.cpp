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
#include "Filter.h"
#include "FilterImage.h"
#include "FilterResults.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "MIMETypeRegistry.h"
#include "PlatformImageBuffer.h"
#include "ProcessCapabilities.h"
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

RefPtr<ImageBuffer> ImageBuffer::create(const FloatSize& size, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, OptionSet<ImageBufferOptions> options, const CreationContext& creationContext)
{
    RefPtr<ImageBuffer> imageBuffer;

    // Give UseDisplayList a higher precedence since it is a debug option.
    if (options.contains(ImageBufferOptions::UseDisplayList)) {
        if (options.contains(ImageBufferOptions::Accelerated))
            imageBuffer = DisplayList::ImageBuffer::create<AcceleratedImageBufferBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, creationContext);

        if (!imageBuffer)
            imageBuffer = DisplayList::ImageBuffer::create<UnacceleratedImageBufferBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, creationContext);
    }

    if (creationContext.graphicsClient && !imageBuffer) {
        auto renderingMode = options.contains(ImageBufferOptions::Accelerated) ? RenderingMode::Accelerated : RenderingMode::Unaccelerated;
        imageBuffer = creationContext.graphicsClient->createImageBuffer(size, renderingMode, purpose, resolutionScale, colorSpace, pixelFormat, creationContext.avoidIOSurfaceSizeCheckInWebProcessForTesting);
    }

    if (imageBuffer)
        return imageBuffer;

    if (options.contains(ImageBufferOptions::Accelerated) && ProcessCapabilities::canUseAcceleratedBuffers()) {
#if HAVE(IOSURFACE)
        imageBuffer = IOSurfaceImageBuffer::create(size, resolutionScale, colorSpace, pixelFormat, purpose, creationContext);
#endif
    }

    if (!imageBuffer)
        imageBuffer = create<UnacceleratedImageBufferBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, creationContext);

    return imageBuffer;
}

ImageBuffer::ImageBuffer(const ImageBufferBackend::Parameters& parameters, const ImageBufferBackend::Info& backendInfo, std::unique_ptr<ImageBufferBackend>&& backend, RenderingResourceIdentifier renderingResourceIdentifier)
    : m_parameters(parameters)
    , m_backendInfo(backendInfo)
    , m_backend(WTFMove(backend))
    , m_renderingResourceIdentifier(renderingResourceIdentifier)
{
}

ImageBuffer::~ImageBuffer() = default;

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

static RefPtr<NativeImage> copyImageBufferToNativeImage(Ref<ImageBuffer> source, BackingStoreCopy copyBehavior, PreserveResolution preserveResolution)
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

static RefPtr<NativeImage> copyImageBufferToOpaqueNativeImage(Ref<ImageBuffer> source, PreserveResolution preserveResolution)
{
    // Composite this ImageBuffer on top of opaque black, because JPEG does not have an alpha channel.
    auto copyBuffer = copyImageBuffer(WTFMove(source), preserveResolution);
    if (!copyBuffer)
        return { };
    // We composite the copy on top of black by drawing black under the copy.
    copyBuffer->context().fillRect({ { }, copyBuffer->logicalSize() }, Color::black, CompositeOperator::DestinationOver);
    return ImageBuffer::sinkIntoNativeImage(WTFMove(copyBuffer));
}

RefPtr<ImageBuffer> ImageBuffer::clone() const
{
    return copyImageBuffer(const_cast<ImageBuffer&>(*this), PreserveResolution::Yes);
}

RefPtr<ImageBuffer> ImageBuffer::cloneForDifferentThread()
{
    return clone();
}

GraphicsContext& ImageBuffer::context() const
{
    ASSERT(m_backend);
    ASSERT(volatilityState() == VolatilityState::NonVolatile);
    return m_backend->context();
}

void ImageBuffer::flushContext()
{
    if (auto* backend = ensureBackendCreated()) {
        flushDrawingContext();
        backend->flushContext();
    }
}

IntSize ImageBuffer::backendSize() const
{
    if (auto* backend = ensureBackendCreated())
        return backend->backendSize();
    return { };
}

RefPtr<NativeImage> ImageBuffer::copyNativeImage(BackingStoreCopy copyBehavior) const
{
    if (auto* backend = ensureBackendCreated()) {
        const_cast<ImageBuffer&>(*this).flushDrawingContext();
        return backend->copyNativeImage(copyBehavior);
    }
    return nullptr;
}

RefPtr<NativeImage> ImageBuffer::copyNativeImageForDrawing(BackingStoreCopy copyBehavior) const
{
    if (auto* backend = ensureBackendCreated()) {
        const_cast<ImageBuffer&>(*this).flushDrawingContext();
        return backend->copyNativeImageForDrawing(copyBehavior);
    }
    return nullptr;
}

RefPtr<NativeImage> ImageBuffer::sinkIntoNativeImage()
{
    if (auto* backend = ensureBackendCreated()) {
        flushDrawingContext();
        return backend->sinkIntoNativeImage();
    }
    return nullptr;
}

RefPtr<ImageBuffer> ImageBuffer::sinkIntoBufferForDifferentThread(RefPtr<ImageBuffer> buffer)
{
    if (!buffer)
        return nullptr;
    ASSERT(buffer->hasOneRef());
    return buffer->sinkIntoBufferForDifferentThread();
}

RefPtr<ImageBuffer> ImageBuffer::sinkIntoBufferForDifferentThread()
{
    ASSERT(hasOneRef());
    return this;
}

RefPtr<Image> ImageBuffer::filteredImage(Filter& filter)
{
    auto* backend = ensureBackendCreated();
    if (!backend)
        return nullptr;

    const_cast<ImageBuffer&>(*this).flushDrawingContext();

    FilterResults results;
    auto result = filter.apply(this, { { }, logicalSize() }, results);
    if (!result)
        return nullptr;

    auto imageBuffer = result->imageBuffer();
    if (!imageBuffer)
        return nullptr;

    return imageBuffer->copyImage();
}

#if USE(CAIRO)
RefPtr<cairo_surface_t> ImageBuffer::createCairoSurface()
{
    auto* backend = ensureBackendCreated();
    if (!backend)
        return nullptr;

    auto surface = backend->createCairoSurface();

    ref(); // Balanced by deref below.

    static cairo_user_data_key_t dataKey;
    cairo_surface_set_user_data(surface.get(), &dataKey, this, [](void *buffer) {
        static_cast<ImageBuffer*>(buffer)->deref();
    });

    return surface;
}
#endif

RefPtr<NativeImage> ImageBuffer::sinkIntoNativeImage(RefPtr<ImageBuffer> source)
{
    if (!source)
        return nullptr;
    return source->sinkIntoNativeImage();
}

RefPtr<Image> ImageBuffer::copyImage(BackingStoreCopy copyBehavior, PreserveResolution preserveResolution) const
{
    auto image = copyImageBufferToNativeImage(const_cast<ImageBuffer&>(*this), copyBehavior, preserveResolution);
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
        auto copyBuffer = source->context().createImageBuffer(source->logicalSize(), 1.f, source->colorSpace());
        if (!copyBuffer)
            return nullptr;
        copyBuffer->context().drawConsumingImageBuffer(WTFMove(source), FloatRect { { }, copyBuffer->logicalSize() }, CompositeOperator::Copy);
        image = ImageBuffer::sinkIntoNativeImage(WTFMove(copyBuffer));
    }
    if (!image)
        return nullptr;
    return BitmapImage::create(image.releaseNonNull());
}

void ImageBuffer::draw(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    FloatRect srcRectScaled = srcRect;
    srcRectScaled.scale(resolutionScale());

    if (auto* backend = ensureBackendCreated()) {
        if (auto image = copyNativeImageForDrawing(&destContext == &context() ? CopyBackingStore : DontCopyBackingStore))
            destContext.drawNativeImage(*image, backendSize(), destRect, srcRectScaled, options);
        backend->finalizeDrawIntoContext(destContext);
    }
}

void ImageBuffer::drawPattern(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(resolutionScale());

    if (ensureBackendCreated()) {
        if (auto image = copyImage(&destContext == &context() ? CopyBackingStore : DontCopyBackingStore))
            image->drawPattern(destContext, destRect, adjustedSrcRect, patternTransform, phase, spacing, options);
    }
}

void ImageBuffer::drawConsuming(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(resolutionScale());

    ASSERT(&destContext != &context());
    if (auto* backend = ensureBackendCreated()) {
        auto backendSize = backend->backendSize();
        if (auto image = sinkIntoNativeImage())
            destContext.drawNativeImage(*image, backendSize, destRect, adjustedSrcRect, options);
    }
}

void ImageBuffer::drawConsuming(RefPtr<ImageBuffer> imageBuffer, GraphicsContext& context, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    imageBuffer->drawConsuming(context, destRect, srcRect, options);
}

void ImageBuffer::clipToMask(GraphicsContext& destContext, const FloatRect& destRect)
{
    if (auto* backend = ensureBackendCreated()) {
        flushContext();
        backend->clipToMask(destContext, destRect);
    }
}

void ImageBuffer::convertToLuminanceMask()
{
    if (auto* backend = ensureBackendCreated()) {
        flushContext();
        backend->convertToLuminanceMask();
    }
}

void ImageBuffer::transformToColorSpace(const DestinationColorSpace& newColorSpace)
{
    if (auto* backend = ensureBackendCreated()) {
        flushDrawingContext();
        backend->transformToColorSpace(newColorSpace);
        m_parameters.colorSpace = newColorSpace;
    }
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
    RefPtr<NativeImage> image = MIMETypeRegistry::isJPEGMIMEType(mimeType) ? copyImageBufferToOpaqueNativeImage(WTFMove(source), preserveResolution) : copyImageBufferToNativeImage(WTFMove(source), DontCopyBackingStore, preserveResolution);
    if (!image)
        return { };
    return encodeData(image->platformImage().get(), mimeType, quality);
}

RefPtr<PixelBuffer> ImageBuffer::getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& srcRect, const ImageBufferAllocator& allocator) const
{
    if (auto* backend = ensureBackendCreated()) {
        const_cast<ImageBuffer&>(*this).flushContext();
        return backend->getPixelBuffer(outputFormat, srcRect, allocator);
    }
    return nullptr;
}

void ImageBuffer::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    if (auto* backend = ensureBackendCreated()) {
        flushContext();
        backend->putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat);
    }
}

PlatformLayer* ImageBuffer::platformLayer() const
{
    if (auto* backend = ensureBackendCreated())
        return backend->platformLayer();
    return nullptr;
}

bool ImageBuffer::copyToPlatformTexture(GraphicsContextGL& context, GCGLenum target, PlatformGLObject destinationTexture, GCGLenum internalformat, bool premultiplyAlpha, bool flipY) const
{
    if (auto* backend = ensureBackendCreated())
        return backend->copyToPlatformTexture(context, target, destinationTexture, internalformat, premultiplyAlpha, flipY);
    return false;
}

bool ImageBuffer::isInUse() const
{
    if (auto* backend = ensureBackendCreated())
        return backend->isInUse();
    return false;
}

void ImageBuffer::releaseGraphicsContext()
{
    if (auto* backend = ensureBackendCreated())
        return backend->releaseGraphicsContext();
}

bool ImageBuffer::setVolatile()
{
    if (auto* backend = ensureBackendCreated())
        return backend->setVolatile();

    return true; // Just claim we succeedded.
}

SetNonVolatileResult ImageBuffer::setNonVolatile()
{
    if (auto* backend = ensureBackendCreated())
        return backend->setNonVolatile();
    return SetNonVolatileResult::Valid;
}

VolatilityState ImageBuffer::volatilityState() const
{
    if (auto* backend = ensureBackendCreated())
        return backend->volatilityState();
    return VolatilityState::NonVolatile;
}

void ImageBuffer::setVolatilityState(VolatilityState volatilityState)
{
    if (auto* backend = ensureBackendCreated())
        backend->setVolatilityState(volatilityState);
}

void ImageBuffer::clearContents()
{
    if (auto* backend = ensureBackendCreated())
        backend->clearContents();
}

std::unique_ptr<ThreadSafeImageBufferFlusher> ImageBuffer::createFlusher()
{
    if (auto* backend = ensureBackendCreated())
        return backend->createFlusher();
    return nullptr;
}

} // namespace WebCore
