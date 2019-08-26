/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Brent Fulgham <bfulgham@webkit.org>
 * Copyright (C) 2011 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ShareableBitmap.h"

#include <WebCore/BitmapImage.h>
#include <WebCore/DIBPixelData.h>
#include <WebCore/Direct2DOperations.h>
#include <WebCore/Direct2DUtilities.h>
#include <WebCore/GraphicsContextImplDirect2D.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformContextDirect2D.h>
#include <d2d1_1.h>
#include <wincodec.h>
#include <wtf/ProcessID.h>


namespace WebKit {
using namespace WebCore;

static unsigned strideForWidth(unsigned width)
{
    static unsigned bitsPerPixel = Direct2D::bitsPerPixel(Direct2D::wicBitmapFormat());
    return bitsPerPixel * width / 8;
}

Checked<unsigned, RecordOverflow> ShareableBitmap::calculateBytesPerRow(WebCore::IntSize size, const Configuration& configuration)
{
    return calculateBytesPerPixel(configuration) * size.width();
}

unsigned ShareableBitmap::calculateBytesPerPixel(const Configuration&)
{
    return 4;
}

static inline COMPtr<IWICBitmap> createSurfaceFromData(void* data, const WebCore::IntSize& size)
{
    const unsigned stride = strideForWidth(size.width());
    return Direct2D::createDirect2DImageSurfaceWithData(data, size, stride);
}

std::unique_ptr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    auto bitmapContext = Direct2D::createBitmapRenderTargetOfSize(m_size);
    if (!bitmapContext)
        return nullptr;

    HRESULT hr = bitmapContext->GetBitmap(&m_bitmap);
    RELEASE_ASSERT(SUCCEEDED(hr));
    return makeUnique<GraphicsContext>(GraphicsContextImplDirect2D::createFactory(bitmapContext.get()));
}

void ShareableBitmap::paint(GraphicsContext& context, const IntPoint& dstPoint, const IntRect& srcRect)
{
    paint(context, 1, dstPoint, srcRect);
}

void ShareableBitmap::paint(GraphicsContext& context, float scaleFactor, const IntPoint& dstPoint, const IntRect& srcRect)
{
    auto surface = createDirect2DSurface(context.platformContext()->renderTarget());

#ifndef _NDEBUG
    auto bitmapSize = surface->GetPixelSize();
    ASSERT(bitmapSize.width == m_size.width());
    ASSERT(bitmapSize.height == m_size.height());
#endif

    FloatRect destRect(dstPoint, srcRect.size());
    FloatRect srcRectScaled(srcRect);
    srcRectScaled.scale(scaleFactor);

    ASSERT(context.hasPlatformContext());
    auto& state = context.state();
    auto& platformContext = *context.platformContext();

    Direct2D::drawNativeImage(platformContext, surface.get(), m_size, destRect, srcRectScaled, state.compositeOperator, state.blendMode, ImageOrientation(), state.imageInterpolationQuality, state.alpha, Direct2D::ShadowState(state));
}

COMPtr<ID2D1Bitmap> ShareableBitmap::createDirect2DSurface(ID2D1RenderTarget* renderTarget)
{
    auto bitmapProperties = Direct2D::bitmapProperties();

    COMPtr<ID2D1Bitmap> bitmap;
    uint32_t stride = 4 * m_size.width();
    HRESULT hr = renderTarget->CreateBitmap(m_size, data(), stride, &bitmapProperties, &bitmap);
    if (!SUCCEEDED(hr))
        return nullptr;

    return bitmap;
}

RefPtr<Image> ShareableBitmap::createImage()
{
    auto surface = createDirect2DSurface(GraphicsContext::defaultRenderTarget());
    if (!surface)
        return nullptr;

    return BitmapImage::create(WTFMove(surface));
}

void ShareableBitmap::sync(GraphicsContext& graphicsContext)
{
    ASSERT(m_bitmap);

    graphicsContext.endDraw();

    COMPtr<ID2D1DeviceContext> d2dDeviceContext;
    HRESULT hr = graphicsContext.platformContext()->renderTarget()->QueryInterface(__uuidof(ID2D1DeviceContext), reinterpret_cast<void**>(&d2dDeviceContext));
    ASSERT(SUCCEEDED(hr));

    const unsigned stride = strideForWidth(m_size.width());

    COMPtr<ID2D1Bitmap1> cpuBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, Direct2D::pixelFormat());
    hr = d2dDeviceContext->CreateBitmap(m_size, nullptr, stride, bitmapProperties, &cpuBitmap);
    if (!SUCCEEDED(hr))
        return;

    hr = cpuBitmap->CopyFromBitmap(nullptr, m_bitmap.get(), nullptr);
    if (!SUCCEEDED(hr))
        return;

    D2D1_MAPPED_RECT mappedData;
    hr = cpuBitmap->Map(D2D1_MAP_OPTIONS_READ, &mappedData);
    if (!SUCCEEDED(hr))
        return;

    if (mappedData.pitch == stride)
        memcpy(data(), reinterpret_cast<char*>(mappedData.bits), stride * m_size.height());
    else {
        // Stride is different, so must do a rowwise copy:
        Checked<int> height = m_size.height();
        Checked<int> width = m_size.width();

        const uint8_t* srcRows = mappedData.bits;
        uint8_t* row = reinterpret_cast<uint8_t*>(data());

        for (int y = 0; y < height.unsafeGet(); ++y) {
            for (int x = 0; x < width.unsafeGet(); x++) {
                int basex = x * 4;
                reinterpret_cast<uint32_t*>(row + basex)[0] = reinterpret_cast<const uint32_t*>(srcRows + basex)[0];
            }

            srcRows += mappedData.pitch;
            row += stride;
        }
    }

    hr = cpuBitmap->Unmap();
    ASSERT(SUCCEEDED(hr));
}

} // namespace WebKit
