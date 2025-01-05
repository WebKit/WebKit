/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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
#include "ImmutableImageBuffer.h"

#include "Filter.h"
#include "FilterImage.h"
#include "FilterResults.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "MIMETypeRegistry.h"
#include "TransparencyLayerContextSwitcher.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/Base64.h>
#include <wtf/text/MakeString.h>

#if USE(CG)
#include "ImageBufferUtilitiesCG.h"
#elif USE(CAIRO)
#include "ImageBufferUtilitiesCairo.h"
#elif USE(SKIA)
#include "ImageBufferUtilitiesSkia.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ImmutableImageBuffer);

static const float MaxClampedLength = 4096;
static const float MaxClampedArea = MaxClampedLength * MaxClampedLength;

ImmutableImageBuffer::ImmutableImageBuffer(Parameters parameters, const ImageBufferBackend::Info& backendInfo, const WebCore::ImageBufferCreationContext&, std::unique_ptr<ImageBufferBackend>&& backend, RenderingResourceIdentifier renderingResourceIdentifier)
    : m_parameters(parameters)
    , m_backendInfo(backendInfo)
    , m_backend(WTFMove(backend))
    , m_renderingResourceIdentifier(renderingResourceIdentifier)
{
}

ImmutableImageBuffer::~ImmutableImageBuffer() = default;

IntSize ImmutableImageBuffer::calculateBackendSize(FloatSize logicalSize, float resolutionScale)
{
    FloatSize scaledSize = { ceilf(resolutionScale * logicalSize.width()), ceilf(resolutionScale * logicalSize.height()) };
    if (scaledSize.isEmpty() || !scaledSize.isExpressibleAsIntSize())
        return { };
    return IntSize { scaledSize };
}

ImageBufferBackendParameters ImmutableImageBuffer::backendParameters(const ImageBufferParameters& parameters)
{
    return { calculateBackendSize(parameters.logicalSize, parameters.resolutionScale), parameters.resolutionScale, parameters.colorSpace, parameters.pixelFormat, parameters.purpose };
}

bool ImmutableImageBuffer::sizeNeedsClamping(const FloatSize& size)
{
    if (size.isEmpty())
        return false;

    return floorf(size.height()) * floorf(size.width()) > MaxClampedArea;
}

bool ImmutableImageBuffer::sizeNeedsClamping(const FloatSize& size, FloatSize& scale)
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

FloatSize ImmutableImageBuffer::clampedSize(const FloatSize& size)
{
    return size.shrunkTo(FloatSize(MaxClampedLength, MaxClampedLength));
}

FloatSize ImmutableImageBuffer::clampedSize(const FloatSize& size, FloatSize& scale)
{
    if (size.isEmpty())
        return size;

    FloatSize clampedSize = ImmutableImageBuffer::clampedSize(size);
    scale = clampedSize / size;
    ASSERT(!sizeNeedsClamping(clampedSize));
    ASSERT(!sizeNeedsClamping(size, scale));
    return clampedSize;
}

FloatRect ImmutableImageBuffer::clampedRect(const FloatRect& rect)
{
    return FloatRect(rect.location(), clampedSize(rect.size()));
}

IntSize ImmutableImageBuffer::backendSize() const
{
    return calculateBackendSize(m_parameters.logicalSize, m_parameters.resolutionScale);
}

GraphicsContext& ImmutableImageBuffer::context() const
{
    ASSERT(m_backend);
    ASSERT(volatilityState() == VolatilityState::NonVolatile);
    return m_backend->context();
}

#if HAVE(IOSURFACE)
IOSurface* ImmutableImageBuffer::surface()
{
    return m_backend ? m_backend->surface() : nullptr;
}
#endif

#if USE(CAIRO)
RefPtr<cairo_surface_t> ImmutableImageBuffer::createCairoSurface()
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

#if USE(SKIA)
const GrDirectContext* ImmutableImageBuffer::skiaGrContext() const
{
    auto* backend = ensureBackend();
    if (!backend)
        return nullptr;
    return backend->skiaGrContext();
}
#endif

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
std::optional<DynamicContentScalingDisplayList> ImmutableImageBuffer::dynamicContentScalingDisplayList()
{
    return std::nullopt;
}
#endif

RefPtr<GraphicsLayerContentsDisplayDelegate> ImmutableImageBuffer::layerContentsDisplayDelegate()
{
    if (auto* backend = ensureBackend())
        return backend->layerContentsDisplayDelegate();
    return nullptr;
}

void ImmutableImageBuffer::setBackend(std::unique_ptr<ImageBufferBackend>&& backend)
{
    if (m_backend.get() == backend.get())
        return;

    m_backend = WTFMove(backend);
    ++m_backendGeneration;
}

RefPtr<ImageBuffer> ImmutableImageBuffer::copyImageBuffer(Ref<ImmutableImageBuffer> source, PreserveResolution preserveResolution, std::optional<RenderingMode> renderingMode)
{
    if (source->resolutionScale() == 1 || preserveResolution == PreserveResolution::Yes) {
        if (source->hasOneRef()
#if USE(SKIA)
            && (!renderingMode || *renderingMode == source->renderingMode())
#endif
        )
            return RefPtr { reinterpret_cast<ImageBuffer*>(source.ptr()) };
    }
    auto copySize = source->logicalSize();
    auto copyScale = preserveResolution == PreserveResolution::Yes ? source->resolutionScale() : 1.f;
    auto copyBuffer = source->context().createImageBuffer(copySize, copyScale, source->colorSpace(), renderingMode);
    if (!copyBuffer)
        return nullptr;
    if (source->hasOneRef())
        copyBuffer->context().drawConsumingImageBuffer(WTFMove(source), FloatRect { { }, copySize }, FloatRect { 0, 0, -1, -1 }, { CompositeOperator::Copy });
    else
        copyBuffer->context().drawImageBuffer(source, FloatPoint { }, { CompositeOperator::Copy });
    return copyBuffer;
}

static RefPtr<NativeImage> copyImageBufferToNativeImage(Ref<ImmutableImageBuffer> source, BackingStoreCopy copyBehavior, PreserveResolution preserveResolution)
{
    if (source->resolutionScale() == 1 || preserveResolution == PreserveResolution::Yes) {
        if (source->hasOneRef())
            return ImmutableImageBuffer::sinkIntoNativeImage(WTFMove(source));
        if (copyBehavior == CopyBackingStore)
            return source->copyNativeImage();
        return source->createNativeImageReference();
    }
    auto copyBuffer = ImmutableImageBuffer::copyImageBuffer(WTFMove(source), preserveResolution);
    if (!copyBuffer)
        return nullptr;
    return ImmutableImageBuffer::sinkIntoNativeImage(WTFMove(copyBuffer));
}

static RefPtr<NativeImage> copyImageBufferToOpaqueNativeImage(Ref<ImmutableImageBuffer> source, PreserveResolution preserveResolution)
{
    // Composite this ImageBuffer on top of opaque black, because JPEG does not have an alpha channel.
    auto copyBuffer = ImmutableImageBuffer::copyImageBuffer(WTFMove(source), preserveResolution);
    if (!copyBuffer)
        return { };
    // We composite the copy on top of black by drawing black under the copy.
    copyBuffer->context().fillRect({ { }, copyBuffer->logicalSize() }, Color::black, CompositeOperator::DestinationOver);
    return ImmutableImageBuffer::sinkIntoNativeImage(WTFMove(copyBuffer));
}

RefPtr<ImageBuffer> ImmutableImageBuffer::clone() const
{
    return copyImageBuffer(const_cast<ImmutableImageBuffer&>(*this), PreserveResolution::Yes);
}

RefPtr<NativeImage> ImmutableImageBuffer::copyNativeImage() const
{
    if (auto* backend = ensureBackend())
        return backend->copyNativeImage();
    return nullptr;
}

RefPtr<NativeImage> ImmutableImageBuffer::createNativeImageReference() const
{
    if (auto* backend = ensureBackend())
        return backend->createNativeImageReference();
    return nullptr;
}

RefPtr<NativeImage> ImmutableImageBuffer::nativeImageForDrawing(GraphicsContext& context) const
{
    if (context.isDeferred() == GraphicsContext::IsDeferred::Yes || &this->context() == &context)
        return copyNativeImage();
    return createNativeImageReference();
}

RefPtr<NativeImage> ImmutableImageBuffer::filteredNativeImage(Filter& filter)
{
    ASSERT(!filter.filterRenderingModes().contains(FilterRenderingMode::GraphicsContext));

    auto* backend = ensureBackend();
    if (!backend)
        return nullptr;

    FilterResults results;
    auto result = filter.apply(this, { { }, logicalSize() }, results);
    if (!result)
        return nullptr;

    RefPtr imageBuffer = result->immutableImageBuffer();
    if (!imageBuffer)
        return nullptr;

    return ImmutableImageBuffer::sinkIntoNativeImage(imageBuffer.releaseNonNull());
}

RefPtr<NativeImage> ImmutableImageBuffer::filteredNativeImage(Filter& filter, Function<void(GraphicsContext&)> drawCallback)
{
    std::unique_ptr<GraphicsContextSwitcher> targetSwitcher;

    if (filter.filterRenderingModes().contains(FilterRenderingMode::GraphicsContext)) {
        targetSwitcher = makeUnique<TransparencyLayerContextSwitcher>(context(), FloatRect { { }, logicalSize() }, &filter);
        if (!targetSwitcher)
            return nullptr;
        targetSwitcher->beginDrawSourceImage(context());
    }

    drawCallback(context());

    if (filter.filterRenderingModes().contains(FilterRenderingMode::GraphicsContext)) {
        ASSERT(targetSwitcher);
        targetSwitcher->endDrawSourceImage(context(), colorSpace());
        return copyNativeImage();
        return copyImageBufferToNativeImage(*this, CopyBackingStore, PreserveResolution::No);
    }

    return filteredNativeImage(filter);
}

RefPtr<PixelBuffer> ImmutableImageBuffer::getPixelBuffer(const PixelBufferFormat& destinationFormat, const IntRect& sourceRect, const ImageBufferAllocator& allocator) const
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

bool ImmutableImageBuffer::isInUse() const
{
    if (auto* backend = ensureBackend())
        return backend->isInUse();
    return false;
}

void ImmutableImageBuffer::releaseGraphicsContext()
{
    if (auto* backend = ensureBackend())
        return backend->releaseGraphicsContext();
}

bool ImmutableImageBuffer::setVolatile()
{
    if (auto* backend = ensureBackend())
        return backend->setVolatile();

    return true; // Just claim we succeedded.
}

SetNonVolatileResult ImmutableImageBuffer::setNonVolatile()
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

VolatilityState ImmutableImageBuffer::volatilityState() const
{
    if (auto* backend = ensureBackend())
        return backend->volatilityState();
    return VolatilityState::NonVolatile;
}

void ImmutableImageBuffer::setVolatilityState(VolatilityState volatilityState)
{
    if (auto* backend = ensureBackend())
        backend->setVolatilityState(volatilityState);
}

void ImmutableImageBuffer::setVolatileAndPurgeForTesting()
{
    releaseGraphicsContext();
    context().clearRect(FloatRect(FloatPoint::zero(), logicalSize()));
    releaseGraphicsContext();
    setVolatile();
    m_hasForcedPurgeForTesting = true;
}

std::unique_ptr<ThreadSafeImageBufferFlusher> ImmutableImageBuffer::createFlusher()
{
    if (auto* backend = ensureBackend())
        return backend->createFlusher();
    return nullptr;
}

unsigned ImmutableImageBuffer::backendGeneration() const
{
    return m_backendGeneration;
}

ImageBufferBackendSharing* ImmutableImageBuffer::toBackendSharing()
{
    if (auto* backend = ensureBackend())
        return backend->toBackendSharing();
    return nullptr;
}

void ImmutableImageBuffer::transferToNewContext(const ImageBufferCreationContext& context)
{
    backend()->transferToNewContext(context);
}

RefPtr<NativeImage> ImmutableImageBuffer::sinkIntoNativeImage()
{
    if (auto* backend = ensureBackend())
        return backend->sinkIntoNativeImage();
    return nullptr;
}

RefPtr<NativeImage> ImmutableImageBuffer::sinkIntoNativeImage(RefPtr<ImmutableImageBuffer> source)
{
    if (!source)
        return nullptr;
    return source->sinkIntoNativeImage();
}

String ImmutableImageBuffer::toDataURL(const String& mimeType, std::optional<double> quality, PreserveResolution preserveResolution) const
{
    return toDataURL(Ref { const_cast<ImmutableImageBuffer&>(*this) }, mimeType, quality, preserveResolution);
}

Vector<uint8_t> ImmutableImageBuffer::toData(const String& mimeType, std::optional<double> quality, PreserveResolution preserveResolution) const
{
    return toData(Ref { const_cast<ImmutableImageBuffer&>(*this) }, mimeType, quality, preserveResolution);
}

String ImmutableImageBuffer::toDataURL(Ref<ImmutableImageBuffer> source, const String& mimeType, std::optional<double> quality, PreserveResolution preserveResolution)
{
    auto encodedData = toData(WTFMove(source), mimeType, quality, preserveResolution);
    if (encodedData.isEmpty())
        return "data:,"_s;
    return makeString("data:"_s, mimeType, ";base64,"_s, base64Encoded(encodedData));
}

Vector<uint8_t> ImmutableImageBuffer::toData(Ref<ImmutableImageBuffer> source, const String& mimeType, std::optional<double> quality, PreserveResolution preserveResolution)
{
    RefPtr<NativeImage> image = MIMETypeRegistry::isJPEGMIMEType(mimeType) ? copyImageBufferToOpaqueNativeImage(WTFMove(source), preserveResolution) : copyImageBufferToNativeImage(WTFMove(source), DontCopyBackingStore, preserveResolution);
    if (!image)
        return { };
    return encodeData(image->platformImage().get(), mimeType, quality);
}

#if USE(SKIA)
void ImmutableImageBuffer::finishAcceleratedRenderingAndCreateFence()
{
    if (auto* backend = ensureBackend())
        backend->finishAcceleratedRenderingAndCreateFence();
}

void ImmutableImageBuffer::waitForAcceleratedRenderingFenceCompletion()
{
    if (auto* backend = ensureBackend())
        backend->waitForAcceleratedRenderingFenceCompletion();
}

RefPtr<ImageBuffer> ImmutableImageBuffer::copyAcceleratedImageBufferBorrowingBackendRenderTarget() const
{
    ASSERT(renderingMode() == RenderingMode::Accelerated);

    auto* backend = ensureBackend();
    if (!backend)
        return nullptr;
    return backend->copyAcceleratedImageBufferBorrowingBackendRenderTarget(*this);
}

RefPtr<ImageBuffer> ImmutableImageBuffer::sinkIntoImageBufferForCrossThreadTransfer(RefPtr<ImmutableImageBuffer> buffer)
{
    if (!buffer || buffer->renderingMode() != RenderingMode::Accelerated)
        return RefPtr { reinterpret_cast<ImageBuffer*>(buffer.get()) };
    if (buffer->hasOneRef()) {
        buffer->finishAcceleratedRenderingAndCreateFence();
        return RefPtr { reinterpret_cast<ImageBuffer*>(buffer.get()) };
    }
    auto bufferCopy = copyImageBuffer(*buffer, PreserveResolution::Yes, RenderingMode::Accelerated);
    bufferCopy->finishAcceleratedRenderingAndCreateFence();
    return bufferCopy;
}

RefPtr<ImageBuffer> ImmutableImageBuffer::sinkIntoImageBufferAfterCrossThreadTransfer(RefPtr<ImmutableImageBuffer> buffer)
{
    if (!buffer || buffer->renderingMode() != RenderingMode::Accelerated)
        return RefPtr { reinterpret_cast<ImageBuffer*>(buffer.get()) };
    buffer->waitForAcceleratedRenderingFenceCompletion();
    return buffer->copyAcceleratedImageBufferBorrowingBackendRenderTarget();
}
#endif

RefPtr<SharedBuffer> ImmutableImageBuffer::sinkIntoPDFDocument()
{
    if (auto* backend = ensureBackend())
        return backend->sinkIntoPDFDocument();
    return nullptr;
}

String ImmutableImageBuffer::debugDescription() const
{
    TextStream stream;
    stream << "ImageBuffer " << this << " " << renderingResourceIdentifier() << " " << logicalSize() << " " << resolutionScale() << "x " << renderingMode() << " backend " << ValueOrNull(m_backend.get());
    return stream.release();
}

TextStream& operator<<(TextStream& ts, const ImmutableImageBuffer& imageBuffer)
{
    ts << imageBuffer.debugDescription();
    return ts;
}

} // namespace WebCore
