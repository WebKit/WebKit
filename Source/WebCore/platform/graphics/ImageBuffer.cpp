/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "FilterStyleTargetSwitcher.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "ImageBufferPlatformBackend.h"
#include "MIMETypeRegistry.h"
#include "ProcessCapabilities.h"
#include <wtf/text/Base64.h>

#if USE(CG)
#include "ImageBufferUtilitiesCG.h"
#endif

#if USE(CAIRO)
#include "ImageBufferUtilitiesCairo.h"
#endif

#if HAVE(IOSURFACE)
#include "ImageBufferIOSurfaceBackend.h"
#endif

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
#include "DynamicContentScalingDisplayList.h"
#endif

namespace WebCore {

static const float MaxClampedLength = 4096;
static const float MaxClampedArea = MaxClampedLength * MaxClampedLength;

RefPtr<ImageBuffer> ImageBuffer::create(const FloatSize& size, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, OptionSet<ImageBufferOptions> options, GraphicsClient* graphicsClient)
{
    RefPtr<ImageBuffer> imageBuffer;

    if (graphicsClient) {
        if (auto imageBuffer = graphicsClient->createImageBuffer(size, purpose, resolutionScale, colorSpace, pixelFormat, options))
            return imageBuffer;
    }

#if HAVE(IOSURFACE)
    if (options.contains(ImageBufferOptions::Accelerated) && ProcessCapabilities::canUseAcceleratedBuffers()) {
        ImageBufferCreationContext creationContext;
        if (graphicsClient)
            creationContext.displayID = graphicsClient->displayID();
        if (auto imageBuffer = ImageBuffer::create<ImageBufferIOSurfaceBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, creationContext))
            return imageBuffer;
    }
#endif

    return create<ImageBufferPlatformBitmapBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, { });
}

ImageBuffer::ImageBuffer(Parameters parameters, const ImageBufferBackend::Info& backendInfo, const WebCore::ImageBufferCreationContext&, std::unique_ptr<ImageBufferBackend>&& backend, RenderingResourceIdentifier renderingResourceIdentifier)
    : m_parameters(parameters)
    , m_backendInfo(backendInfo)
    , m_backend(WTFMove(backend))
    , m_renderingResourceIdentifier(renderingResourceIdentifier)
{
}

ImageBuffer::~ImageBuffer() = default;

IntSize ImageBuffer::calculateBackendSize(FloatSize logicalSize, float resolutionScale)
{
    FloatSize scaledSize = { ceilf(resolutionScale * logicalSize.width()), ceilf(resolutionScale * logicalSize.height()) };
    if (scaledSize.isEmpty() || !scaledSize.isExpressibleAsIntSize())
        return { };
    return IntSize { scaledSize };
}

ImageBufferBackendParameters ImageBuffer::backendParameters(const ImageBufferParameters& parameters)
{
    return { calculateBackendSize(parameters.logicalSize, parameters.resolutionScale), parameters.resolutionScale, parameters.colorSpace, parameters.pixelFormat, parameters.purpose };
}

bool ImageBuffer::sizeNeedsClamping(const FloatSize& size)
{
    if (size.isEmpty())
        return false;

    return floorf(size.height()) * floorf(size.width()) > MaxClampedArea;
}

RefPtr<ImageBuffer> SerializedImageBuffer::sinkIntoImageBuffer(std::unique_ptr<SerializedImageBuffer> buffer, GraphicsClient* graphicsClient)
{
    if (graphicsClient)
        return graphicsClient->sinkIntoImageBuffer(WTFMove(buffer));
    return buffer->sinkIntoImageBuffer();
}

// The default serialization of an ImageBuffer just assumes that we can
// pass it as-is, as long as this is the only reference.
class DefaultSerializedImageBuffer : public SerializedImageBuffer {
public:
    DefaultSerializedImageBuffer(ImageBuffer* image)
        : m_buffer(image)
    { }

    RefPtr<ImageBuffer> sinkIntoImageBuffer() final
    {
        return m_buffer;
    }

    size_t memoryCost() const final
    {
        return m_buffer->memoryCost();
    }

private:
    RefPtr<ImageBuffer> m_buffer;
};

std::unique_ptr<SerializedImageBuffer> ImageBuffer::sinkIntoSerializedImageBuffer()
{
    ASSERT(hasOneRef());
    ASSERT(!controlBlock().weakReferenceCount());
    return makeUnique<DefaultSerializedImageBuffer>(this);
}

std::unique_ptr<SerializedImageBuffer> ImageBuffer::sinkIntoSerializedImageBuffer(RefPtr<ImageBuffer>&& image)
{
    ASSERT(image->hasOneRef());
    RefPtr<ImageBuffer> move = WTFMove(image);
    return move->sinkIntoSerializedImageBuffer();
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
        copyBuffer->context().drawConsumingImageBuffer(WTFMove(source), FloatRect { { }, copySize }, FloatRect { 0, 0, -1, -1 }, { CompositeOperator::Copy });
    else
        copyBuffer->context().drawImageBuffer(source, FloatPoint { }, { CompositeOperator::Copy });
    return copyBuffer;
}

static RefPtr<NativeImage> copyImageBufferToNativeImage(Ref<ImageBuffer> source, BackingStoreCopy copyBehavior, PreserveResolution preserveResolution)
{
    if (source->resolutionScale() == 1 || preserveResolution == PreserveResolution::Yes) {
        if (source->hasOneRef())
            return ImageBuffer::sinkIntoNativeImage(WTFMove(source));
        if (copyBehavior == CopyBackingStore)
            return source->copyNativeImage();
        return source->createNativeImageReference();
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

GraphicsContext& ImageBuffer::context() const
{
    ASSERT(m_backend);
    ASSERT(volatilityState() == VolatilityState::NonVolatile);
    return m_backend->context();
}

void ImageBuffer::flushDrawingContext()
{
    // FIXME: this will be removed and flushDrawingContext will be renamed as flushContext().
    // The direct backend context flush is not part of ImageBuffer abstraction semantics,
    // rather implementation detail of the ImageBufferBackends that need separate management
    // of their context lifetime for purposes of drawing from the image buffer.
    if (auto* backend = ensureBackend())
        backend->flushContext();
}

bool ImageBuffer::flushDrawingContextAsync()
{
    // This function is only really useful for the Remote subclass.
    flushDrawingContext();
    return true;
}

void ImageBuffer::setBackend(std::unique_ptr<ImageBufferBackend>&& backend)
{
    if (m_backend.get() == backend.get())
        return;

    m_backend = WTFMove(backend);
    ++m_backendGeneration;
}

IntSize ImageBuffer::backendSize() const
{
    return calculateBackendSize(m_parameters.logicalSize, m_parameters.resolutionScale);
}

RefPtr<NativeImage> ImageBuffer::copyNativeImage() const
{
    if (auto* backend = ensureBackend())
        return backend->copyNativeImage();
    return nullptr;
}

RefPtr<NativeImage> ImageBuffer::createNativeImageReference() const
{
    if (auto* backend = ensureBackend())
        return backend->createNativeImageReference();
    return nullptr;
}

RefPtr<NativeImage> ImageBuffer::sinkIntoNativeImage()
{
    if (auto* backend = ensureBackend())
        return backend->sinkIntoNativeImage();
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

RefPtr<NativeImage> ImageBuffer::filteredNativeImage(Filter& filter)
{
    ASSERT(!filter.filterRenderingModes().contains(FilterRenderingMode::GraphicsContext));

    auto* backend = ensureBackend();
    if (!backend)
        return nullptr;

    FilterResults results;
    auto result = filter.apply(this, { { }, logicalSize() }, results);
    if (!result)
        return nullptr;

    RefPtr imageBuffer = result->imageBuffer();
    if (!imageBuffer)
        return nullptr;

    return copyImageBufferToNativeImage(*imageBuffer, CopyBackingStore, PreserveResolution::No);
}

RefPtr<NativeImage> ImageBuffer::filteredNativeImage(Filter& filter, Function<void(GraphicsContext&)> drawCallback)
{
    std::unique_ptr<FilterTargetSwitcher> targetSwitcher;

    if (filter.filterRenderingModes().contains(FilterRenderingMode::GraphicsContext)) {
        targetSwitcher = makeUnique<FilterStyleTargetSwitcher>(filter, FloatRect { { }, logicalSize() });
        if (!targetSwitcher)
            return nullptr;
        targetSwitcher->beginDrawSourceImage(context());
    }

    drawCallback(context());

    if (filter.filterRenderingModes().contains(FilterRenderingMode::GraphicsContext)) {
        ASSERT(targetSwitcher);
        targetSwitcher->endDrawSourceImage(context(), colorSpace());
        return copyImageBufferToNativeImage(*this, CopyBackingStore, PreserveResolution::No);
    }

    return filteredNativeImage(filter);
}

#if HAVE(IOSURFACE)
IOSurface* ImageBuffer::surface()
{
    return m_backend ? m_backend->surface() : nullptr;
}
#endif

#if USE(CAIRO)
RefPtr<cairo_surface_t> ImageBuffer::createCairoSurface()
{
    auto* backend = ensureBackend();
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

void ImageBuffer::convertToLuminanceMask()
{
    if (auto* backend = ensureBackend())
        backend->convertToLuminanceMask();
}

void ImageBuffer::transformToColorSpace(const DestinationColorSpace& newColorSpace)
{
    if (auto* backend = ensureBackend()) {
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

RefPtr<PixelBuffer> ImageBuffer::getPixelBuffer(const PixelBufferFormat& destinationFormat, const IntRect& sourceRect, const ImageBufferAllocator& allocator) const
{
    ASSERT(PixelBuffer::supportedPixelFormat(destinationFormat.pixelFormat));
    auto sourceRectScaled = sourceRect;
    sourceRectScaled.scale(resolutionScale());
    auto destination = allocator.createPixelBuffer(destinationFormat, sourceRectScaled.size());
    if (!destination)
        return nullptr;
    if (auto* backend = ensureBackend())
        backend->getPixelBuffer(sourceRectScaled, *destination);
    else
        destination->zeroFill();
    return destination;
}

void ImageBuffer::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& sourceRect, const IntPoint& destinationPoint, AlphaPremultiplication destinationFormat)
{
    ASSERT(resolutionScale() == 1);
    auto* backend = ensureBackend();
    if (!backend)
        return;
    auto sourceRectScaled = sourceRect;
    sourceRectScaled.scale(resolutionScale());
    auto destinationPointScaled = destinationPoint;
    destinationPointScaled.scale(resolutionScale());
    backend->putPixelBuffer(pixelBuffer, sourceRectScaled, destinationPointScaled, destinationFormat);
}

bool ImageBuffer::isInUse() const
{
    if (auto* backend = ensureBackend())
        return backend->isInUse();
    return false;
}

void ImageBuffer::releaseGraphicsContext()
{
    if (auto* backend = ensureBackend())
        return backend->releaseGraphicsContext();
}

bool ImageBuffer::setVolatile()
{
    if (auto* backend = ensureBackend())
        return backend->setVolatile();

    return true; // Just claim we succeedded.
}

SetNonVolatileResult ImageBuffer::setNonVolatile()
{
    auto result = SetNonVolatileResult::Valid;
    if (auto* backend = ensureBackend())
        result = backend->setNonVolatile();

    if (m_hasForcedPurgeForTesting) {
        result = SetNonVolatileResult::Empty;
        m_hasForcedPurgeForTesting = false;
    }

    return result;
}

VolatilityState ImageBuffer::volatilityState() const
{
    if (auto* backend = ensureBackend())
        return backend->volatilityState();
    return VolatilityState::NonVolatile;
}

void ImageBuffer::setVolatilityState(VolatilityState volatilityState)
{
    if (auto* backend = ensureBackend())
        backend->setVolatilityState(volatilityState);
}

void ImageBuffer::setVolatileAndPurgeForTesting()
{
    releaseGraphicsContext();
    context().clearRect(FloatRect(FloatPoint::zero(), logicalSize()));
    releaseGraphicsContext();
    setVolatile();
    m_hasForcedPurgeForTesting = true;
}

std::unique_ptr<ThreadSafeImageBufferFlusher> ImageBuffer::createFlusher()
{
    if (auto* backend = ensureBackend())
        return backend->createFlusher();
    return nullptr;
}

unsigned ImageBuffer::backendGeneration() const
{
    return m_backendGeneration;
}

ImageBufferBackendSharing* ImageBuffer::toBackendSharing()
{
    if (auto* backend = ensureBackend())
        return backend->toBackendSharing();
    return nullptr;
}

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
std::optional<DynamicContentScalingDisplayList> ImageBuffer::dynamicContentScalingDisplayList()
{
    return std::nullopt;
}
#endif

void ImageBuffer::transferToNewContext(const ImageBufferCreationContext& context)
{
    backend()->transferToNewContext(context);
}

String ImageBuffer::debugDescription() const
{
    TextStream stream;
    stream << "ImageBuffer " << this << " " << renderingResourceIdentifier() << " " << logicalSize() << " " << resolutionScale() << "x " << renderingMode() << " backend " << ValueOrNull(m_backend.get());
    return stream.release();
}

TextStream& operator<<(TextStream& ts, const ImageBuffer& imageBuffer)
{
    ts << imageBuffer.debugDescription();
    return ts;
}

} // namespace WebCore
