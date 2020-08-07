/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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
#include "ImageBufferDirect2DBackend.h"

#if USE(DIRECT2D)

#include "BitmapImage.h"
#include "COMPtr.h"
#include "Direct2DUtilities.h"
#include "GraphicsContext.h"
#include "ImageData.h"
#include "ImageDecoderDirect2D.h"
#include "IntRect.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "PlatformContextDirect2D.h"
#include <d2d1_1.h>
#include <math.h>
#include <wincodec.h>
#include <wtf/Assertions.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferDirect2DBackend);

std::unique_ptr<ImageBufferDirect2DBackend> ImageBufferDirect2DBackend::create(const FloatSize& size, float resolutionScale, ColorSpace colorSpace, const HostWindow* hostWindow)
{
    IntSize backendSize = calculateBackendSize(size, resolutionScale);
    if (backendSize.isEmpty())
        return nullptr;

    auto* platformContext = targetContext ? targetContext->platformContext() : nullptr;
    auto* renderTarget = platformContext ? platformContext->renderTarget() : nullptr;

    if (!renderTarget)
        renderTarget = GraphicsContext::defaultRenderTarget();

    auto bitmapContext = Direct2D::createBitmapRenderTargetOfSize(size, renderTarget, resolutionScale);
    if (!bitmapContext)
        return;

    NativeImagePtr bitmap;
    HRESULT hr = bitmapContext->GetBitmap(&bitmap);
    if (!SUCCEEDED(hr))
        return;

    auto platformContext = makeUnique<PlatformContextDirect2D>(bitmapContext.get(), [this]() {
        m_data.loadDataToBitmapIfNeeded();
    }, [this]() {
        m_data.markBufferOutOfSync();
    });

    if (!platformContext)
        return nullptr;

    auto context = makeUnique<GraphicsContext>(platformContext.get(), GraphicsContext::BitmapRenderingContextType::GPUMemory);
    if (!context)
        return nullptr;

    return std::unique_ptr<ImageBufferDirect2DBackend>(new ImageBufferDirect2DBackend(size, backendSize, resolutionScale, colorSpace, WTFMove(platformContext), WTFMove(context), WTFMove(bitmap)));
}

std::unique_ptr<ImageBufferDirect2DBackend> ImageBufferDirect2DBackend::create(const FloatSize& size, const GraphicsContext& context)
{
    return ImageBufferDirect2DBackend::create(size, 1, ColorSpace::SRGB, nullptr);
}

ImageBufferDirect2DBackend::ImageBufferDirect2DBackend(const FloatSize& logicalSize, const IntSize& backendSize, float resolutionScale, ColorSpace colorSpace, std::unique_ptr<PlatformContextDirect2D>&& platformContext, std::unique_ptr<GraphicsContext>&& context, NativeImagePtr&& bitmap)
    : ImageBufferCGBackend(logicalSize, backendSize, resolutionScale, colorSpace)
    , m_platformContext(WTFMove(platformContext))
    , m_context(WTFMove(context))
    , m_bitmap(WTFMove(bitmap))
{
}

GraphicsContext& ImageBufferDirect2DBackend::context() const
{
    return *m_context;
}

void ImageBufferDirect2DBackend::flushContext()
{
    context().flush();
}

NativeImagePtr ImageBufferDirect2DBackend::copyNativeImage(BackingStoreCopy copyBehavior) const
{
    COMPtr<ID2D1BitmapRenderTarget> bitmapTarget;
    HRESULT hr = context().platformContext()->renderTarget()->QueryInterface(&bitmapTarget);
    if (!SUCCEEDED(hr))
        return nullptr;

    COMPtr<ID2D1Bitmap> image;
    hr = bitmapTarget->GetBitmap(&image);
    ASSERT(SUCCEEDED(hr));

    // FIXME: m_data.data is nullptr even when asking to copy backing store leading to test failures.
    if (copyBehavior == CopyBackingStore && m_data.data.isEmpty())
        copyBehavior = DontCopyBackingStore;

    CheckedSize numBytes = Checked<unsigned, RecordOverflow>(m_data.backingStoreSize.height()) * m_data.bytesPerRow;
    if (numBytes.hasOverflowed())
        return nullptr;

    if (copyBehavior == CopyBackingStore) {
        D2D1_RECT_U backingStoreDimenstions = IntRect(IntPoint(), m_data.backingStoreSize);
        image->CopyFromMemory(&backingStoreDimenstions, m_data.data.data(), 32);

    }

    return image;
}

static NativeImagePtr createCroppedImageIfNecessary(ID2D1BitmapRenderTarget* bitmapTarget, ID2D1Bitmap* image, const IntSize& bounds)
{
    FloatSize imageSize = image ? nativeImageSize(image) : FloatSize();

    if (image && (static_cast<size_t>(imageSize.width()) != static_cast<size_t>(bounds.width()) || static_cast<size_t>(imageSize.height()) != static_cast<size_t>(bounds.height()))) {
        COMPtr<ID2D1Bitmap> croppedBitmap = Direct2D::createBitmap(bitmapTarget, bounds);
        if (croppedBitmap) {
            auto sourceRect = D2D1::RectU(0, 0, bounds.width(), bounds.height());
            HRESULT hr = croppedBitmap->CopyFromBitmap(nullptr, image, &sourceRect);
            if (SUCCEEDED(hr))
                return croppedBitmap;
        }
    }

    return image;
}

static RefPtr<Image> createBitmapImageAfterScalingIfNeeded(ID2D1BitmapRenderTarget* bitmapTarget, COMPtr<ID2D1Bitmap>&& image, IntSize internalSize, float resolutionScale, PreserveResolution preserveResolution)
{
    if (resolutionScale == 1 || preserveResolution == PreserveResolution::Yes)
        image = createCroppedImageIfNecessary(bitmapTarget, image.get(), internalSize);
    else {
        // FIXME: Need to implement scaled version
        notImplemented();
    }

    if (!image)
        return nullptr;

    return BitmapImage::create(WTFMove(image));
}

RefPtr<Image> ImageBufferDirect2DBackend::copyImage(BackingStoreCopy copyBehavior, PreserveResolution preserveResolution) const
{
    NativeImagePtr image;
    if (m_resolutionScale == 1 || preserveResolution == PreserveResolution::Yes)
        image = copyNativeImage(copyBehavior);
    else
        image = copyNativeImage(DontCopyBackingStore);

    auto bitmapTarget = reinterpret_cast<ID2D1BitmapRenderTarget*>(context().platformContext());
    return createBitmapImageAfterScalingIfNeeded(bitmapTarget, WTFMove(image), internalSize(), resolutionScale(), preserveResolution);
}

RefPtr<Image> ImageBufferDirect2DBackend::sinkIntoImage(PreserveResolution preserveResolution)
{
    COMPtr<ID2D1BitmapRenderTarget> bitmapTarget;
    HRESULT hr = context().platformContext()->renderTarget()->QueryInterface(&bitmapTarget);
    if (!SUCCEEDED(hr))
        return nullptr;

    NativeImagePtr image = copyNativeImage(DontCopyBackingStore);
    return createBitmapImageAfterScalingIfNeeded(bitmapTarget.get(), WTFMove(image), internalSize(), resolutionScale(), preserveResolution);
}

NativeImagePtr ImageBufferDirect2DBackend::compatibleBitmap(ID2D1RenderTarget* renderTarget)
{
    loadDataToBitmapIfNeeded();

    if (!renderTarget)
        return bitmap;

    if (m_platformContext->renderTarget() == renderTarget) {
        COMPtr<ID2D1BitmapRenderTarget> bitmapTarget(Query, renderTarget);
        if (bitmapTarget) {
            COMPtr<ID2D1Bitmap> backingBitmap;
            if (SUCCEEDED(bitmapTarget->GetBitmap(&backingBitmap))) {
                if (backingBitmap != m_bitmap)
                    return bitmap;

                // We can't draw an ID2D1Bitmap to itself. Must return a copy.
                COMPtr<ID2D1Bitmap> copiedBitmap;
                if (SUCCEEDED(renderTarget->CreateBitmap(bitmap->GetPixelSize(), Direct2D::bitmapProperties(), &copiedBitmap))) {
                    if (SUCCEEDED(copiedBitmap->CopyFromBitmap(nullptr, m_bitmap.get(), nullptr)))
                        return copiedBitmap;
                }
            }
        }
    }

    auto size = m_bitmap->GetPixelSize();
    ASSERT(size.height && size.width);

    Checked<unsigned, RecordOverflow> numBytes = size.width * size.height * 4;
    if (numBytes.hasOverflowed())
        return nullptr;

    // Copy the bits from current renderTarget to the output target.
    // We cannot access the data backing an IWICBitmap while an active draw session is open.
    context->endDraw();

    COMPtr<ID2D1DeviceContext> sourceDeviceContext = m_platformContext->deviceContext();
    if (!sourceDeviceContext)
        return nullptr;

    COMPtr<ID2D1Bitmap1> sourceCPUBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, Direct2D::pixelFormat());
    HRESULT hr = sourceDeviceContext->CreateBitmap(m_bitmap->GetPixelSize(), nullptr, bytesPerRow.unsafeGet(), bitmapProperties, &sourceCPUBitmap);
    if (!SUCCEEDED(hr))
        return nullptr;

    if (!sourceCPUBitmap)
        return nullptr;

    hr = sourceCPUBitmap->CopyFromBitmap(nullptr, m_bitmap.get(), nullptr);
    if (!SUCCEEDED(hr))
        return nullptr;

    D2D1_MAPPED_RECT mappedSourceData;
    hr = sourceCPUBitmap->Map(D2D1_MAP_OPTIONS_READ, &mappedSourceData);
    if (!SUCCEEDED(hr))
        return nullptr;

    COMPtr<ID2D1DeviceContext> targetDeviceContext;
    hr = renderTarget->QueryInterface(&targetDeviceContext);
    ASSERT(SUCCEEDED(hr));

    COMPtr<ID2D1Bitmap> compatibleBitmap;
    hr = targetDeviceContext->CreateBitmap(bitmap->GetPixelSize(), mappedSourceData.bits, mappedSourceData.pitch, Direct2D::bitmapProperties(), &compatibleBitmap);
    if (!SUCCEEDED(hr))
        return nullptr;

    hr = sourceCPUBitmap->Unmap();
    ASSERT(SUCCEEDED(hr));

    context->beginDraw();

    return compatibleBitmap;
}

void ImageBufferDirect2DBackend::draw(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(m_resolutionScale, m_resolutionScale);

    auto compatibleBitmap = compatibleBitmap(destContext.platformContext()->renderTarget());

    FloatSize currentImageSize = nativeImageSize(compatibleBitmap);
    if (currentImageSize.isZero())
        return;

    destContext.drawNativeImage(compatibleBitmap, currentImageSize, destRect, adjustedSrcRect, options);
}

void ImageBufferDirect2DBackend::drawPattern(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(m_resolutionScale);

    if (auto image = copyImage(&destContext == &context() ? CopyBackingStore : DontCopyBackingStore))
        image->drawPattern(destContext, destRect, adjustedSrcRect, patternTransform, phase, spacing, options);
}

String ImageBufferDirect2DBackend::toDataURL(const String&, Optional<double>, PreserveResolution) const
{
    notImplemented();
    return "data:,"_s;
}

Vector<uint8_t> ImageBufferDirect2DBackend::toData(const String& mimeType, Optional<double> quality) const
{
    notImplemented();
    return { };
}

Vector<uint8_t> ImageBufferDirect2DBackend::toBGRAData() const
{
    notImplemented();
    return { };
}

RefPtr<ImageData> ImageBufferDirect2DBackend::getImageData(AlphaPremultiplication outputFormat, const IntRect& srcRect) const
{
    notImplemented();
    return ImageBufferBackend::getImageData(outputFormat, srcRect, nullptr);
}

void ImageBufferDirect2DBackend::putImageData(AlphaPremultiplication inputFormat, const ImageData& imageData, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    notImplemented();
    ImageBufferBackend::putImageData(inputFormat, imageData, srcRect, destPoint, destFormat, nullptr);
}

} // namespace WebCore

#endif // USE(DIRECT2D)
