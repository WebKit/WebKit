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
#include "ImageBuffer.h"

#include "BitmapImage.h"
#include "Filter.h"
#include "FilterImage.h"
#include "FilterResults.h"
#include "GraphicsClient.h"
#include "GraphicsContext.h"
#include "ImageBufferPlatformBackend.h"
#include "ProcessCapabilities.h"
#include "TransparencyLayerContextSwitcher.h"
#include <wtf/TZoneMallocInlines.h>

#if USE(CG)
#include "ImageBufferCGPDFDocumentBackend.h"
#include "ImageBufferUtilitiesCG.h"
#endif

#if USE(CAIRO)
#include "ImageBufferUtilitiesCairo.h"
#endif

#if USE(SKIA)
#include "ImageBufferSkiaAcceleratedBackend.h"
#include "ImageBufferUtilitiesSkia.h"
#endif

#if HAVE(IOSURFACE)
#include "ImageBufferIOSurfaceBackend.h"
#endif

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
#include "DynamicContentScalingDisplayList.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ImageBuffer);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SerializedImageBuffer);

RefPtr<ImageBuffer> ImageBuffer::create(const FloatSize& size, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, ImageBufferPixelFormat pixelFormat, GraphicsClient* graphicsClient)
{
    RefPtr<ImageBuffer> imageBuffer;

    if (graphicsClient) {
        if (auto imageBuffer = graphicsClient->createImageBuffer(size, renderingMode, purpose, resolutionScale, colorSpace, pixelFormat))
            return imageBuffer;
    }

    switch (renderingMode) {
    case RenderingMode::Accelerated:
#if HAVE(IOSURFACE)
        if (ProcessCapabilities::canUseAcceleratedBuffers()) {
            ImageBufferCreationContext creationContext;
            if (graphicsClient)
                creationContext.displayID = graphicsClient->displayID();
            if (auto imageBuffer = ImageBuffer::create<ImageBufferIOSurfaceBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, creationContext))
                return imageBuffer;
        }
#elif USE(SKIA)
        if (auto imageBuffer = ImageBuffer::create<ImageBufferSkiaAcceleratedBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, { }))
            return imageBuffer;
#endif
        [[fallthrough]];

    case RenderingMode::Unaccelerated:
        return create<ImageBufferPlatformBitmapBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, { });

    case RenderingMode::PDFDocument:
#if USE(CG)
        return ImageBuffer::create<ImageBufferCGPDFDocumentBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, { });
#else
        return nullptr;
#endif

    case RenderingMode::DisplayList:
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

ImageBuffer::ImageBuffer(Parameters parameters, const ImageBufferBackend::Info& backendInfo, const WebCore::ImageBufferCreationContext& context, std::unique_ptr<ImageBufferBackend>&& backend, RenderingResourceIdentifier renderingResourceIdentifier)
    : ImmutableImageBuffer(parameters, backendInfo, context, WTFMove(backend), renderingResourceIdentifier)
{
}

ImageBuffer::~ImageBuffer() = default;

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

// The default serialization of an ImageBuffer just assumes that we can
// pass it as-is, as long as this is the only reference.
class DefaultSerializedImageBuffer : public SerializedImageBuffer {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(DefaultSerializedImageBuffer);
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
    ASSERT(!controlBlock().weakRefCount());
    return makeUnique<DefaultSerializedImageBuffer>(this);
}

std::unique_ptr<SerializedImageBuffer> ImageBuffer::sinkIntoSerializedImageBuffer(RefPtr<ImageBuffer>&& image)
{
    ASSERT(image->hasOneRef());
    RefPtr<ImageBuffer> move = WTFMove(image);
    return move->sinkIntoSerializedImageBuffer();
}

RefPtr<ImageBuffer> SerializedImageBuffer::sinkIntoImageBuffer(std::unique_ptr<SerializedImageBuffer> buffer, GraphicsClient* graphicsClient)
{
    if (graphicsClient)
        return graphicsClient->sinkIntoImageBuffer(WTFMove(buffer));
    return buffer->sinkIntoImageBuffer();
}

} // namespace WebCore
