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
#include "ImageBufferData.h"

#if USE(DIRECT2D)

#include "BitmapInfo.h"
#include "Direct2DUtilities.h"
#include "GraphicsContext.h"
#include "HWndDC.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "PlatformContextDirect2D.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/Uint8ClampedArray.h>
#include <d2d1.h>
#include <wtf/Assertions.h>
#include <wtf/UniqueArray.h>

namespace WebCore {

// Swizzle the red and blue bytes of the pixels in a buffer
template <AlphaPremultiplication desiredFormat>
void swizzleAndPremultiply(const uint8_t* srcRows, unsigned rowCount, unsigned colCount, unsigned srcStride, unsigned destStride, uint8_t* destRows)
{
    for (unsigned y = 0; y < rowCount; ++y) {
        // Source data may be power-of-two sized, so we need to only copy the bits that
        // correspond to the rectangle supplied by the caller.
        const uint32_t* srcRow = reinterpret_cast<const uint32_t*>(srcRows + srcStride * y);
        uint8_t* destRow = destRows + destStride * y;
        for (unsigned x = 0; x < colCount; ++x) {
            unsigned bytePosition = x * 4;
            const uint32_t* srcPixel = srcRow + x;

            // Software filters expect (P)RGBA bytes. We need to swizzle from Direct2D's PBGRA to be compatible.
            uint32_t alpha = (*srcPixel & 0xFF000000) >> 24;
            uint32_t red = (*srcPixel & 0x00FF0000) >> 16;
            uint32_t green = (*srcPixel & 0x0000FF00) >> 8;
            uint32_t blue = (*srcPixel & 0x000000FF);

            if (desiredFormat == AlphaPremultiplication::Unpremultiplied) {
                if (alpha && alpha != 255) {
                    red = red * 255 / alpha;
                    green = green * 255 / alpha;
                    blue = blue * 255 / alpha;
                }
            }

            destRow[bytePosition]     = red;
            destRow[bytePosition + 1] = green;
            destRow[bytePosition + 2] = blue;
            destRow[bytePosition + 3] = alpha;
        }
    }
}

RefPtr<Uint8ClampedArray> ImageBufferData::getData(AlphaPremultiplication desiredFormat, const IntRect& rect, const IntSize& size, bool /* accelerateRendering */, float /* resolutionScale */) const
{
    auto numBytes = rect.area<RecordOverflow>() * 4;
    if (numBytes.hasOverflowed())
        return nullptr;

    auto result = Uint8ClampedArray::tryCreateUninitialized(numBytes.unsafeGet());
    unsigned char* resultData = result ? result->data() : nullptr;
    if (!resultData)
        return nullptr;

    if (!bitmap)
        return result;

    context->endDraw();

    COMPtr<ID2D1DeviceContext> d2dDeviceContext;
    HRESULT hr = platformContext->renderTarget()->QueryInterface(__uuidof(ID2D1DeviceContext), reinterpret_cast<void**>(&d2dDeviceContext));
    ASSERT(SUCCEEDED(hr));

    auto bytesPerRowInData = size.width() * 4;

    COMPtr<ID2D1Bitmap1> cpuBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties2 = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, Direct2D::pixelFormat());
    hr = d2dDeviceContext->CreateBitmap(size, nullptr, bytesPerRowInData, bitmapProperties2, &cpuBitmap);
    if (!SUCCEEDED(hr))
        return nullptr;

    D2D1_POINT_2U targetPos = D2D1::Point2U();
    D2D1_RECT_U dataRect = rect;
    hr = cpuBitmap->CopyFromBitmap(&targetPos, bitmap.get(), &dataRect);
    if (!SUCCEEDED(hr))
        return nullptr;

    D2D1_MAPPED_RECT mappedData;
    hr = cpuBitmap->Map(D2D1_MAP_OPTIONS_READ, &mappedData);
    if (!SUCCEEDED(hr))
        return nullptr;

    // Software filters expect RGBA bytes. We need to swizzle from Direct2D's BGRA to be compatible.
    Checked<int> height = rect.height();
    Checked<int> width = rect.width();

    if (desiredFormat == AlphaPremultiplication::Unpremultiplied)
        swizzleAndPremultiply<AlphaPremultiplication::Unpremultiplied>(mappedData.bits, height.unsafeGet(), width.unsafeGet(), mappedData.pitch, bytesPerRowInData, resultData);
    else
        swizzleAndPremultiply<AlphaPremultiplication::Premultiplied>(mappedData.bits, height.unsafeGet(), width.unsafeGet(), mappedData.pitch, bytesPerRowInData, resultData);

    hr = cpuBitmap->Unmap();
    ASSERT(SUCCEEDED(hr));

    context->beginDraw();

    return result;
}

// Swizzle the red and blue bytes of the pixels in a buffer
template <AlphaPremultiplication sourceFormat>
void inPlaceSwizzle(uint8_t* byteData, unsigned byteCount)
{
    size_t pixelCount = byteCount / 4;
    auto* pixelData = reinterpret_cast<uint32_t*>(byteData);

    for (size_t i = 0; i < pixelCount; ++i) {
        uint32_t pixel = *pixelData;
        size_t bytePosition = i * 4;

        uint32_t alpha = (pixel & 0xFF000000) >> 24;
        uint32_t red = (pixel & 0x00FF0000) >> 16;
        uint32_t green = (pixel & 0x0000FF00) >> 8;
        uint32_t blue = (pixel & 0x000000FF);

        // (P)RGBA -> PBGRA
        if (sourceFormat == AlphaPremultiplication::Unpremultiplied) {
            if (alpha != 255) {
                red = (red * alpha + 254) / 255;
                green = (green * alpha + 254) / 255;
                blue = (blue * alpha + 254) / 255;
            }
        }

        *pixelData = (alpha << 24) | red  << 16 | green  << 8 | blue;
        ++pixelData;
    }
}

void ImageBufferData::putData(const Uint8ClampedArray& source, AlphaPremultiplication sourceFormat, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, const IntSize& size, bool /* accelerateRendering */, float resolutionScale)
{
    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    Checked<int> originx = sourceRect.x();
    Checked<int> destx = (Checked<int>(destPoint.x()) + sourceRect.x());
    destx *= resolutionScale;
    ASSERT(destx.unsafeGet() >= 0);
    ASSERT(destx.unsafeGet() < size.width());
    ASSERT(originx.unsafeGet() >= 0);
    ASSERT(originx.unsafeGet() <= sourceRect.maxX());

    Checked<int> endx = (Checked<int>(destPoint.x()) + sourceRect.maxX());
    endx *= resolutionScale;
    ASSERT(endx.unsafeGet() <= size.width());

    Checked<int> width = sourceRect.width();
    Checked<int> destw = endx - destx;

    Checked<int> originy = sourceRect.y();
    Checked<int> desty = (Checked<int>(destPoint.y()) + sourceRect.y());
    desty *= resolutionScale;
    ASSERT(desty.unsafeGet() >= 0);
    ASSERT(desty.unsafeGet() < size.height());
    ASSERT(originy.unsafeGet() >= 0);
    ASSERT(originy.unsafeGet() <= sourceRect.maxY());

    Checked<int> endy = (Checked<int>(destPoint.y()) + sourceRect.maxY());
    endy *= resolutionScale;
    ASSERT(endy.unsafeGet() <= size.height());

    Checked<int> height = sourceRect.height();
    Checked<int> desth = endy - desty;

    if (width <= 0 || height <= 0)
        return;

    context->endDraw();

    auto pixelSize = bitmap->GetPixelSize();
    ASSERT(pixelSize.width >= sourceSize.width());
    ASSERT(pixelSize.width >= size.width());
    ASSERT(pixelSize.height >= sourceSize.height());
    ASSERT(pixelSize.height >= size.height());

    // Software generated bitmap data is in RGBA. We need to swizzle to premultiplied BGRA to be compatible
    // with the HWND/HDC render backing we use.
    if (sourceFormat == AlphaPremultiplication::Unpremultiplied)
        inPlaceSwizzle<AlphaPremultiplication::Unpremultiplied>(source.data(), source.length()); // RGBA -> PBGRA
    else
        inPlaceSwizzle<AlphaPremultiplication::Premultiplied>(source.data(), source.length()); // PRGBA -> PBGRA

    COMPtr<ID2D1BitmapRenderTarget> bitmapRenderTarget;
    HRESULT hr = platformContext->renderTarget()->QueryInterface(__uuidof(ID2D1BitmapRenderTarget), reinterpret_cast<void**>(&bitmapRenderTarget));
    ASSERT(SUCCEEDED(hr));

    auto bytesPerRowInData = sourceRect.width() * 4;

    COMPtr<ID2D1Bitmap> swizzledBitmap;
    D2D1_BITMAP_PROPERTIES bitmapProperties = D2D1::BitmapProperties(Direct2D::pixelFormat());
    hr = bitmapRenderTarget->CreateBitmap(sourceSize, source.data(), bytesPerRowInData, bitmapProperties, &swizzledBitmap);
    if (!SUCCEEDED(hr))
        return;

    D2D1_POINT_2U destPointD2D = destPoint;
    D2D1_RECT_U srcRect = sourceRect;
    hr = bitmap->CopyFromMemory(&srcRect, source.data(), bytesPerRowInData);
    ASSERT(SUCCEEDED(hr));

    context->beginDraw();
}

COMPtr<ID2D1Bitmap> ImageBufferData::compatibleBitmap(ID2D1RenderTarget* renderTarget)
{
    if (!renderTarget)
        return bitmap;

    if (platformContext->renderTarget() == renderTarget)
        return bitmap;

    auto size = bitmap->GetPixelSize();

    Checked<unsigned, RecordOverflow> numBytes = size.width * size.height * 4;
    if (numBytes.hasOverflowed())
        return nullptr;

    // Copy the bits from current renderTarget to the output target.
    // We cannot access the data backing an IWICBitmap while an active draw session is open.
    context->endDraw();

    COMPtr<ID2D1DeviceContext> sourceDeviceContext;
    HRESULT hr = platformContext->renderTarget()->QueryInterface(__uuidof(ID2D1DeviceContext), reinterpret_cast<void**>(&sourceDeviceContext));
    ASSERT(SUCCEEDED(hr));

    if (!sourceDeviceContext)
        return nullptr;

    COMPtr<ID2D1Bitmap1> sourceCPUBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, Direct2D::pixelFormat());
    hr = sourceDeviceContext->CreateBitmap(bitmap->GetPixelSize(), nullptr, bytesPerRow.unsafeGet(), bitmapProperties, &sourceCPUBitmap);
    if (!SUCCEEDED(hr))
        return nullptr;

    if (!sourceCPUBitmap)
        return nullptr;

    hr = sourceCPUBitmap->CopyFromBitmap(nullptr, bitmap.get(), nullptr);
    if (!SUCCEEDED(hr))
        return nullptr;

    D2D1_MAPPED_RECT mappedSourceData;
    hr = sourceCPUBitmap->Map(D2D1_MAP_OPTIONS_READ, &mappedSourceData);
    if (!SUCCEEDED(hr))
        return nullptr;

    COMPtr<ID2D1DeviceContext> targetDeviceContext;
    hr = renderTarget->QueryInterface(__uuidof(ID2D1DeviceContext), reinterpret_cast<void**>(&targetDeviceContext));
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

} // namespace WebCore

#endif
