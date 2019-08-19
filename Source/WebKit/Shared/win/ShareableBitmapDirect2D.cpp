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
#include <wincodec.h>
#include <wtf/ProcessID.h>


namespace WebKit {
using namespace WebCore;

static const auto bitmapFormat = GUID_WICPixelFormat32bppPBGRA;

static unsigned strideForWidth(unsigned width)
{
    static unsigned bitsPerPixel = Direct2D::bitsPerPixel(bitmapFormat);
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
    m_bitmap = createDirect2DSurface();
    COMPtr<ID2D1RenderTarget> bitmapContext = Direct2D::createRenderTargetFromWICBitmap(m_bitmap.get());
    return makeUnique<GraphicsContext>(GraphicsContextImplDirect2D::createFactory(bitmapContext.get()));
}

void ShareableBitmap::paint(GraphicsContext& context, const IntPoint& dstPoint, const IntRect& srcRect)
{
    paint(context, 1, dstPoint, srcRect);
}

void ShareableBitmap::paint(GraphicsContext& context, float scaleFactor, const IntPoint& dstPoint, const IntRect& srcRect)
{
    auto surface = createDirect2DSurface();

    FloatRect destRect(dstPoint, srcRect.size());
    FloatRect srcRectScaled(srcRect);
    srcRectScaled.scale(scaleFactor);

    ASSERT(context.hasPlatformContext());
    auto& state = context.state();
    auto& platformContext = *context.platformContext();

    Direct2D::drawNativeImage(platformContext, surface.get(), m_size, destRect, srcRectScaled, state.compositeOperator, state.blendMode, ImageOrientation(), state.imageInterpolationQuality, state.alpha, Direct2D::ShadowState(state));
}

COMPtr<IWICBitmap> ShareableBitmap::createDirect2DSurface()
{
    m_bitmap = createSurfaceFromData(data(), m_size);
    return m_bitmap;
}

RefPtr<Image> ShareableBitmap::createImage()
{
    auto surface = createDirect2DSurface();
    if (!surface)
        return nullptr;

    return BitmapImage::create(WTFMove(surface));
}

void ShareableBitmap::sync(GraphicsContext& graphicsContext)
{
    ASSERT(m_bitmap);

    graphicsContext.endDraw();

    WICRect rcLock = { 0, 0, m_size.width(), m_size.height() };

    COMPtr<IWICBitmapLock> bitmapDataLock;
    HRESULT hr = m_bitmap->Lock(&rcLock, WICBitmapLockRead, &bitmapDataLock);
    if (SUCCEEDED(hr)) {
        UINT bufferSize = 0;
        WICInProcPointer dataPtr = nullptr;
        hr = bitmapDataLock->GetDataPointer(&bufferSize, &dataPtr);
        if (SUCCEEDED(hr))
            memcpy(data(), reinterpret_cast<char*>(dataPtr), bufferSize);
    }

    // Once we are done modifying the data, unlock the bitmap
    bitmapDataLock = nullptr;
}

} // namespace WebKit
