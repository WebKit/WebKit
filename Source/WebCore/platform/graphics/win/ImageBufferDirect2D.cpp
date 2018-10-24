/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#if USE(DIRECT2D)

#include "BitmapImage.h"
#include "COMPtr.h"
#include "GraphicsContext.h"
#include "ImageData.h"
#include "IntRect.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include <d2d1.h>
#include <math.h>
#include <wtf/Assertions.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/MainThread.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/Base64.h>
#include <wtf/text/WTFString.h>


namespace WebCore {

static FloatSize scaleSizeToUserSpace(const FloatSize& logicalSize, const IntSize& backingStoreSize, const IntSize& internalSize)
{
    float xMagnification = static_cast<float>(backingStoreSize.width()) / internalSize.width();
    float yMagnification = static_cast<float>(backingStoreSize.height()) / internalSize.height();
    return FloatSize(logicalSize.width() * xMagnification, logicalSize.height() * yMagnification);
}

std::unique_ptr<ImageBuffer> ImageBuffer::createCompatibleBuffer(const FloatSize& size, const GraphicsContext& context)
{
    if (size.isEmpty())
        return nullptr;

    RenderingMode renderingMode = context.renderingMode();
    IntSize scaledSize = ImageBuffer::compatibleBufferSize(size, context);
    bool success = false;
    std::unique_ptr<ImageBuffer> buffer(new ImageBuffer(scaledSize, 1, ColorSpaceSRGB, renderingMode, nullptr, &context, success));

    if (!success)
        return nullptr;

    // Set up a corresponding scale factor on the graphics context.
    buffer->context().scale(FloatSize(scaledSize.width() / size.width(), scaledSize.height() / size.height()));
    return buffer;
}

ImageBuffer::ImageBuffer(const FloatSize& size, float resolutionScale, ColorSpace /*colorSpace*/, RenderingMode renderingMode, const HostWindow*, const GraphicsContext* targetContext, bool& success)
    : m_logicalSize(size)
    , m_resolutionScale(resolutionScale)
{
    success = false; // Make early return mean failure.
    float scaledWidth = std::ceil(resolutionScale * size.width());
    float scaledHeight = std::ceil(resolutionScale * size.height());

    // FIXME: Should we automatically use a lower resolution?
    if (!FloatSize(scaledWidth, scaledHeight).isExpressibleAsIntSize())
        return;

    m_size = IntSize(scaledWidth, scaledHeight);
    m_data.backingStoreSize = m_size;

    bool accelerateRendering = renderingMode == Accelerated;
    if (m_size.width() <= 0 || m_size.height() <= 0)
        return;

    // Prevent integer overflows
    m_data.bytesPerRow = 4 * Checked<unsigned, RecordOverflow>(m_data.backingStoreSize.width());
    Checked<size_t, RecordOverflow> numBytes = Checked<unsigned, RecordOverflow>(m_data.backingStoreSize.height()) * m_data.bytesPerRow;
    if (numBytes.hasOverflowed())
        return;

    auto renderTarget = targetContext ? targetContext->platformContext() : nullptr;
    if (!renderTarget)
        renderTarget = GraphicsContext::defaultRenderTarget();
    RELEASE_ASSERT(renderTarget);

    COMPtr<ID2D1BitmapRenderTarget> bitmapContext;
    D2D1_SIZE_F desiredSize = FloatSize(m_logicalSize);
    D2D1_SIZE_U pixelSize = IntSize(m_logicalSize);
    HRESULT hr = renderTarget->CreateCompatibleRenderTarget(&desiredSize, &pixelSize, nullptr, D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_GDI_COMPATIBLE, &bitmapContext);
    if (!bitmapContext || !SUCCEEDED(hr))
        return;

    m_data.context = std::make_unique<GraphicsContext>(bitmapContext.get());
    m_data.m_compatibleTarget = renderTarget;

    success = true;
}

ImageBuffer::ImageBuffer(const FloatSize& size, float resolutionScale, ColorSpace imageColorSpace, RenderingMode renderingMode, const HostWindow*, bool& success)
    : ImageBuffer(size, resolutionScale, imageColorSpace, renderingMode, nullptr, nullptr, success)
{
}

ImageBuffer::~ImageBuffer() = default;

FloatSize ImageBuffer::sizeForDestinationSize(FloatSize destinationSize) const
{
    return scaleSizeToUserSpace(destinationSize, m_data.backingStoreSize, internalSize());
}

GraphicsContext& ImageBuffer::context() const
{
    return *m_data.context;
}

void ImageBuffer::flushContext() const
{
    context().flush();
}

RefPtr<Image> ImageBuffer::copyImage(BackingStoreCopy copyBehavior, PreserveResolution) const
{
    notImplemented();
    return nullptr;
}

RefPtr<Image> ImageBuffer::sinkIntoImage(std::unique_ptr<ImageBuffer> imageBuffer, PreserveResolution)
{
    IntSize internalSize = imageBuffer->internalSize();
    IntSize logicalSize = imageBuffer->logicalSize();
    IntSize backingStoreSize = imageBuffer->m_data.backingStoreSize;
    float resolutionScale = imageBuffer->m_resolutionScale;

    notImplemented();
    return nullptr;
}

BackingStoreCopy ImageBuffer::fastCopyImageMode()
{
    return DontCopyBackingStore;
}

void ImageBuffer::drawConsuming(std::unique_ptr<ImageBuffer> imageBuffer, GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator op, BlendMode blendMode)
{
    imageBuffer->draw(destContext, destRect, srcRect, op, blendMode);
}

void ImageBuffer::draw(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator op, BlendMode blendMode)
{
    auto bitmapTarget = reinterpret_cast<ID2D1BitmapRenderTarget*>(context().platformContext());
    auto outputTarget = destContext.platformContext();

    COMPtr<ID2D1Bitmap> image;
    HRESULT hr = bitmapTarget->GetBitmap(&image);
    if (!SUCCEEDED(hr))
        return;

    // If the render targets for the source and destination contexts do not match, move the image over.
    if (destContext.platformContext() != m_data.m_compatibleTarget) {
        COMPtr<ID2D1Bitmap> sourceImage = image;
        image = nullptr;

        auto bitmapProperties = D2D1::BitmapProperties();
        GraphicsContext::systemFactory()->GetDesktopDpi(&bitmapProperties.dpiX, &bitmapProperties.dpiY);
        hr = outputTarget->CreateSharedBitmap(__uuidof(ID2D1Bitmap), sourceImage.get(), &bitmapProperties, &image);
        if (!SUCCEEDED(hr))
            return;
    }

    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(m_resolutionScale, m_resolutionScale);

    destContext.drawNativeImage(image, image->GetSize(), destRect, adjustedSrcRect, op, blendMode);

    destContext.flush();
}

void ImageBuffer::drawPattern(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator op, BlendMode blendMode)
{
    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(m_resolutionScale, m_resolutionScale);

    if (!context().isAcceleratedContext()) {
        if (&destContext == &context() || destContext.isAcceleratedContext()) {
            if (RefPtr<Image> copy = copyImage(CopyBackingStore)) // Drawing into our own buffer, need to deep copy.
                copy->drawPattern(destContext, destRect, adjustedSrcRect, patternTransform, phase, spacing, op, blendMode);
        } else {
            if (RefPtr<Image> imageForRendering = copyImage(DontCopyBackingStore))
                imageForRendering->drawPattern(destContext, destRect, adjustedSrcRect, patternTransform, phase, spacing, op, blendMode);
        }
    } else {
        if (RefPtr<Image> copy = copyImage(CopyBackingStore))
            copy->drawPattern(destContext, destRect, adjustedSrcRect, patternTransform, phase, spacing, op, blendMode);
    }
}

RefPtr<Uint8ClampedArray> ImageBuffer::getUnmultipliedImageData(const IntRect& rect, IntSize* pixelArrayDimensions, CoordinateSystem coordinateSystem) const
{
    if (context().isAcceleratedContext())
        flushContext();

    IntRect srcRect = rect;
    if (coordinateSystem == LogicalCoordinateSystem)
        srcRect.scale(m_resolutionScale);

    if (pixelArrayDimensions)
        *pixelArrayDimensions = srcRect.size();

    return m_data.getData(AlphaPremultiplication::Unpremultiplied, srcRect, internalSize(), context().isAcceleratedContext(), 1);
}

RefPtr<Uint8ClampedArray> ImageBuffer::getPremultipliedImageData(const IntRect& rect, IntSize* pixelArrayDimensions, CoordinateSystem coordinateSystem) const
{
    if (context().isAcceleratedContext())
        flushContext();

    IntRect srcRect = rect;
    if (coordinateSystem == LogicalCoordinateSystem)
        srcRect.scale(m_resolutionScale);

    if (pixelArrayDimensions)
        *pixelArrayDimensions = srcRect.size();

    return m_data.getData(AlphaPremultiplication::Premultiplied, srcRect, internalSize(), context().isAcceleratedContext(), 1);
}

void ImageBuffer::putByteArray(const Uint8ClampedArray& source, AlphaPremultiplication bufferFormat, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, CoordinateSystem coordinateSystem)
{
    if (context().isAcceleratedContext())
        flushContext();

    IntRect scaledSourceRect = sourceRect;
    IntSize scaledSourceSize = sourceSize;
    if (coordinateSystem == LogicalCoordinateSystem) {
        scaledSourceRect.scale(m_resolutionScale);
        scaledSourceSize.scale(m_resolutionScale);
    }

    m_data.putData(source, bufferFormat, scaledSourceSize, scaledSourceRect, destPoint, internalSize(), context().isAcceleratedContext(), 1);
}

String ImageBuffer::toDataURL(const String&, std::optional<double>, PreserveResolution) const
{
    notImplemented();
    return "data:,"_s;
}

Vector<uint8_t> ImageBuffer::toData(const String& mimeType, std::optional<double> quality) const
{
    notImplemented();
    return { };
}

String ImageDataToDataURL(const ImageData& source, const String& mimeType, const double* quality)
{
    notImplemented();
    return "data:,"_s;
}

void ImageBuffer::transformColorSpace(ColorSpace, ColorSpace)
{
    notImplemented();
}

} // namespace WebCore

#endif
