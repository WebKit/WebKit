/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

static bool copyRectFromSourceToDest(const IntRect& sourceRect, const IntSize& sourceBufferSize, const uint8_t* source, const IntSize& destBufferSize, uint8_t* dest, const IntPoint& destBufferPosition)
{
    if (!IntRect({ }, destBufferSize).contains(IntRect(destBufferPosition, sourceRect.size())))
        return false;

    if (!IntRect({ }, sourceBufferSize).contains(sourceRect))
        return false;

    unsigned srcStride = sourceBufferSize.width() * 4;
    unsigned destStride = destBufferSize.width() * 4;

    unsigned srcRowOffset = sourceRect.location().x() * 4;
    unsigned destRowOffset = destBufferPosition.x() * 4;

    const uint8_t* srcRows = source + srcStride * sourceRect.location().y();
    uint8_t* destRows = dest + destStride * destBufferPosition.y();

    const unsigned bytesPerRow = sourceRect.width() * 4;
    for (unsigned y = 0; y < sourceRect.height(); ++y) {
        const uint8_t* srcRow = srcRows + srcStride * y + srcRowOffset;
        uint8_t* destRow = destRows + destStride * y + destRowOffset;
        memcpy(destRow, srcRow, bytesPerRow);
    }

    return true;
}

// Note: Assumes that bytes are already in RGBA format
template <AlphaPremultiplication desiredFormat>
bool copyRectFromSourceToDestAndSetPremultiplication(const IntRect& sourceRect, const IntSize& sourceBufferSize, const uint8_t* source, const IntSize& destBufferSize, uint8_t* dest, const IntPoint& destBufferPosition)
{
    if (!IntRect({ }, destBufferSize).contains(IntRect(destBufferPosition, sourceRect.size())))
        return false;

    if (!IntRect({ }, sourceBufferSize).contains(sourceRect))
        return false;

    unsigned srcStride = sourceBufferSize.width() * 4;
    unsigned destStride = destBufferSize.width() * 4;

    unsigned srcRowOffset = sourceRect.location().x() * 4;
    unsigned destRowOffset = destBufferPosition.x() * 4;

    const uint8_t* srcRows = source + srcStride * sourceRect.location().y();
    uint8_t* destRows = dest + destStride * destBufferPosition.y();

    const unsigned bytesPerRow = sourceRect.width() * 4;
    for (unsigned y = 0; y < sourceRect.height(); ++y) {
        const uint8_t* srcRow = srcRows + srcStride * y + srcRowOffset;
        uint8_t* destRow = destRows + destStride * y + destRowOffset;

        for (size_t x = 0; x < sourceRect.width(); ++x) {
            unsigned pos = x * 4;

            unsigned red = srcRow[x];
            unsigned green = srcRow[x + 1];
            unsigned blue = srcRow[x + 2];
            unsigned alpha = srcRow[x + 3];

            if (desiredFormat == AlphaPremultiplication::Premultiplied) {
                if (alpha != 255) {
                    red = (red * alpha + 254) / 255;
                    green = (green * alpha + 254) / 255;
                    blue = (blue * alpha + 254) / 255;
                }
            } else {
                if (alpha && alpha != 255) {
                    red = red * 255 / alpha;
                    green = green * 255 / alpha;
                    blue = blue * 255 / alpha;
                }
            }

            uint32_t* pixel = reinterpret_cast<uint32_t*>(destRow) + x;
            *pixel = (alpha << 24) | red  << 16 | green  << 8 | blue;
        }
    }

    return true;
}

bool ImageBufferData::copyRectFromData(const IntRect& rect, RefPtr<Uint8ClampedArray>& result) const
{
    auto rawBitmapSize = bitmap->GetSize();

    float scaleFactor = backingStoreSize.width() / rawBitmapSize.width;

    IntRect scaledRect = rect;
    scaledRect.scale(scaleFactor);

    if (!IntRect(IntPoint(), backingStoreSize).contains(scaledRect)) {
        // Requested rect is outside the buffer. Return zero-filled buffer.
        result->zeroFill();
        return true;
    }

    return copyRectFromSourceToDest(scaledRect, backingStoreSize, data.data(), rect.size(), result->data(), IntPoint());
}

bool ImageBufferData::ensureBackingStore(const IntSize& size) const
{
    if (size == backingStoreSize && !data.isEmpty())
        return true;

    auto numBytes = size.area<RecordOverflow>() * 4;
    if (numBytes.hasOverflowed())
        return false;

    backingStoreSize = size;
    data.resizeToFit(numBytes.unsafeGet());
    return data.capacity() == numBytes.unsafeGet();
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

// Note: Assumes that bytes are already in RGBA format
template <AlphaPremultiplication desiredFormat>
void inPlaceChangePremultiplication(uint8_t* byteData, unsigned byteCount)
{
    size_t pixelCount = byteCount / 4;

    for (size_t i = 0; i < pixelCount; ++i) {
        unsigned x = pixelCount * 4;

        unsigned red = byteData[x];
        unsigned green = byteData[x + 1];
        unsigned blue = byteData[x + 2];
        unsigned alpha = byteData[x + 3];

        if (desiredFormat == AlphaPremultiplication::Premultiplied) {
            if (alpha != 255) {
                red = (red * alpha + 254) / 255;
                green = (green * alpha + 254) / 255;
                blue = (blue * alpha + 254) / 255;
            }
        } else {
            if (alpha && alpha != 255) {
                red = red * 255 / alpha;
                green = green * 255 / alpha;
                blue = blue * 255 / alpha;
            }
        }

        uint32_t* pixel = reinterpret_cast<uint32_t*>(byteData) + i;
        *pixel = (alpha << 24) | red  << 16 | green  << 8 | blue;
    }
}

RefPtr<Uint8ClampedArray> ImageBufferData::getData(AlphaPremultiplication desiredFormat, const IntRect& rect, const IntSize& size, bool /* accelerateRendering */, float /* resolutionScale */) const
{
    auto numBytes = rect.area<RecordOverflow>() * 4;
    if (numBytes.hasOverflowed())
        return nullptr;

    auto result = Uint8ClampedArray::tryCreateUninitialized(numBytes.unsafeGet());
    if (!result)
        return nullptr;

    if (!bitmap)
        return result;

    if (!readDataFromBitmapIfNeeded(desiredFormat))
        return nullptr;

    auto rawBitmapSize = bitmap->GetSize();
    IntSize bitmapSize(clampTo<int>(rawBitmapSize.width), clampTo<int>(rawBitmapSize.height));

    if (rect.location() == IntPoint() && rect.size() == bitmapSize) {
        RELEASE_ASSERT(numBytes.unsafeGet() == data.size());
        result->setRange(data.data(), data.size(), 0);
    } else {
        // Only want part of the bitmap.
        if (!copyRectFromData(rect, result))
            return nullptr;
    }

    if (byteFormat != desiredFormat) {
        // In-memory data does not match desired format. Need to swizzle the results.
        if (desiredFormat == AlphaPremultiplication::Unpremultiplied)
            inPlaceChangePremultiplication<AlphaPremultiplication::Unpremultiplied>(result->data(), result->length());
        else
            inPlaceChangePremultiplication<AlphaPremultiplication::Premultiplied>(result->data(), result->length());
    }

    return result;
}

bool ImageBufferData::readDataFromBitmapIfNeeded(AlphaPremultiplication desiredFormat) const
{
    if (bitmapBufferSync != BitmapBufferSync::BufferOutOfSync)
        return true;

    IntSize pixelSize = bitmap->GetPixelSize();

    auto numBytes = pixelSize.area<RecordOverflow>() * 4;
    if (numBytes.hasOverflowed())
        return false;

    context->endDraw();

    COMPtr<ID2D1DeviceContext> d2dDeviceContext = platformContext->deviceContext();
    ASSERT(!!d2dDeviceContext);

    auto bytesPerRowInData = pixelSize.width() * 4;

    // Copy GPU data from the ID2D1Bitmap to a CPU-backed ID2D1Bitmap1
    COMPtr<ID2D1Bitmap1> cpuBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties2 = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, Direct2D::pixelFormat());
    HRESULT hr = d2dDeviceContext->CreateBitmap(pixelSize, nullptr, bytesPerRowInData, bitmapProperties2, &cpuBitmap);
    if (!SUCCEEDED(hr))
        return false;

    auto targetPos = D2D1::Point2U();
    D2D1_RECT_U dataRect = D2D1::RectU(0, 0, pixelSize.width(), pixelSize.height());
    hr = cpuBitmap->CopyFromBitmap(&targetPos, bitmap.get(), &dataRect);
    if (!SUCCEEDED(hr))
        return false;

    D2D1_MAPPED_RECT mappedData;
    hr = cpuBitmap->Map(D2D1_MAP_OPTIONS_READ, &mappedData);
    if (!SUCCEEDED(hr))
        return false;

    if (!mappedData.bits)
        return false;

    if (!ensureBackingStore(pixelSize))
        return false;

    // Software filters expect RGBA bytes. We need to swizzle from Direct2D's BGRA to be compatible.
    if (desiredFormat == AlphaPremultiplication::Premultiplied)
        swizzleAndPremultiply<AlphaPremultiplication::Premultiplied>(mappedData.bits, pixelSize.height(), pixelSize.width(), mappedData.pitch, bytesPerRowInData, data.data());
    else
        swizzleAndPremultiply<AlphaPremultiplication::Unpremultiplied>(mappedData.bits, pixelSize.height(), pixelSize.width(), mappedData.pitch, bytesPerRowInData, data.data());

    byteFormat = desiredFormat;

    bitmapBufferSync = BitmapBufferSync::InSync;

    hr = cpuBitmap->Unmap();
    ASSERT(SUCCEEDED(hr));

    context->beginDraw();

    return true;
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

#if !ASSERT_DISABLED
    if (bitmap) {
        auto pixelSize = bitmap->GetPixelSize();
        ASSERT(pixelSize.width >= sourceSize.width());
        ASSERT(pixelSize.width >= size.width());
        ASSERT(pixelSize.height >= sourceSize.height());
        ASSERT(pixelSize.height >= size.height());
    }
#endif

    if (!ensureBackingStore(size))
        return;

    if (sourceSize == size) {
        memcpy(data.data(), source.data(), source.length());
        byteFormat = sourceFormat;
    } else {
        readDataFromBitmapIfNeeded(byteFormat);
        if (!copyRectFromSourceToData(sourceRect, source, sourceFormat))
            return;
    }

    bitmapBufferSync = BitmapBufferSync::BitmapOutOfSync;
}

bool ImageBufferData::copyRectFromSourceToData(const IntRect& sourceRect, const Uint8ClampedArray& source, AlphaPremultiplication sourceFormat)
{
    IntSize pixelSize = bitmap->GetPixelSize();

    IntRect sourceRectToCopy = sourceRect;
    if (!IntRect(IntPoint(), pixelSize).contains(sourceRect))
        return false;

    auto destBufferPosition = sourceRect.location();

    if (sourceFormat == byteFormat)
        return copyRectFromSourceToDest(sourceRect, sourceRect.size(), source.data(), backingStoreSize, data.data(), destBufferPosition);

    if (byteFormat == AlphaPremultiplication::Unpremultiplied)
        return copyRectFromSourceToDestAndSetPremultiplication<AlphaPremultiplication::Unpremultiplied>(sourceRect, sourceRect.size(), source.data(), backingStoreSize, data.data(), destBufferPosition);

    return copyRectFromSourceToDestAndSetPremultiplication<AlphaPremultiplication::Premultiplied>(sourceRect, sourceRect.size(), source.data(), backingStoreSize, data.data(), destBufferPosition);
}

void ImageBufferData::loadDataToBitmapIfNeeded()
{
    if (bitmapBufferSync != BitmapBufferSync::BitmapOutOfSync || data.isEmpty())
        return;

    auto pixelSize = bitmap->GetPixelSize();
    ASSERT(pixelSize.width >= backingStoreSize.width());
    ASSERT(pixelSize.height >= backingStoreSize.height());

    Checked<unsigned, RecordOverflow> numBytes = pixelSize.width * pixelSize.height * 4;
    if (numBytes.hasOverflowed())
        return;

    // Software generated bitmap data is in RGBA. We need to swizzle to premultiplied BGRA to be compatible
    // with the HWND/HDC render backing we use.
    if (byteFormat == AlphaPremultiplication::Unpremultiplied)
        inPlaceSwizzle<AlphaPremultiplication::Unpremultiplied>(data.data(), data.size()); // RGBA -> PBGRA
    else
        inPlaceSwizzle<AlphaPremultiplication::Premultiplied>(data.data(), data.size()); // PRGBA -> PBGRA

    auto bytesPerRowInData = backingStoreSize.width() * 4;

    HRESULT hr = bitmap->CopyFromMemory(nullptr, data.data(), bytesPerRowInData);
    ASSERT(SUCCEEDED(hr));

    bitmapBufferSync = BitmapBufferSync::InSync;
}

COMPtr<ID2D1Bitmap> ImageBufferData::compatibleBitmap(ID2D1RenderTarget* renderTarget)
{
    loadDataToBitmapIfNeeded();

    if (!renderTarget)
        return bitmap;

    if (platformContext->renderTarget() == renderTarget) {
        COMPtr<ID2D1BitmapRenderTarget> bitmapTarget(Query, renderTarget);
        if (bitmapTarget) {
            COMPtr<ID2D1Bitmap> backingBitmap;
            if (SUCCEEDED(bitmapTarget->GetBitmap(&backingBitmap))) {
                if (backingBitmap != bitmap)
                    return bitmap;

                // We can't draw an ID2D1Bitmap to itself. Must return a copy.
                COMPtr<ID2D1Bitmap> copiedBitmap;
                if (SUCCEEDED(renderTarget->CreateBitmap(bitmap->GetPixelSize(), Direct2D::bitmapProperties(), &copiedBitmap))) {
                    if (SUCCEEDED(copiedBitmap->CopyFromBitmap(nullptr, bitmap.get(), nullptr)))
                        return copiedBitmap;
                }
            }
        }
    }

    auto size = bitmap->GetPixelSize();
    ASSERT(size.height && size.width);

    Checked<unsigned, RecordOverflow> numBytes = size.width * size.height * 4;
    if (numBytes.hasOverflowed())
        return nullptr;

    // Copy the bits from current renderTarget to the output target.
    // We cannot access the data backing an IWICBitmap while an active draw session is open.
    context->endDraw();

    COMPtr<ID2D1DeviceContext> sourceDeviceContext = platformContext->deviceContext();
    if (!sourceDeviceContext)
        return nullptr;

    COMPtr<ID2D1Bitmap1> sourceCPUBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, Direct2D::pixelFormat());
    HRESULT hr = sourceDeviceContext->CreateBitmap(bitmap->GetPixelSize(), nullptr, bytesPerRow.unsafeGet(), bitmapProperties, &sourceCPUBitmap);
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

} // namespace WebCore

#endif
