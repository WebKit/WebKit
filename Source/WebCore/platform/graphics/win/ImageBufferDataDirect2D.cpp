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
#include "GraphicsContext.h"
#include "HWndDC.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/Uint8ClampedArray.h>
#include <d2d1.h>
#include <wtf/Assertions.h>
#include <wtf/UniqueArray.h>

namespace WebCore {

RefPtr<Uint8ClampedArray> ImageBufferData::getData(AlphaPremultiplication, const IntRect& rect, const IntSize& size, bool /* accelerateRendering */, float /* resolutionScale */) const
{
    auto platformContext = context->platformContext();

    auto numBytes = rect.area<RecordOverflow>() * 4;
    if (numBytes.hasOverflowed())
        return nullptr;

    auto result = Uint8ClampedArray::tryCreateUninitialized(numBytes.unsafeGet());
    unsigned char* resultData = result ? result->data() : nullptr;
    if (!resultData)
        return nullptr;

    BitmapInfo bitmapInfo = BitmapInfo::createBottomUp(size);

    void* pixels = nullptr;
    auto bitmap = adoptGDIObject(::CreateDIBSection(0, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0));

    HWndDC windowDC(nullptr);
    auto bitmapDC = adoptGDIObject(::CreateCompatibleDC(windowDC));
    HGDIOBJ oldBitmap = ::SelectObject(bitmapDC.get(), bitmap.get());

    COMPtr<ID2D1GdiInteropRenderTarget> gdiRenderTarget;
    HRESULT hr = platformContext->QueryInterface(__uuidof(ID2D1GdiInteropRenderTarget), (void**)&gdiRenderTarget);
    if (FAILED(hr))
        return nullptr;

    HDC hdc = nullptr;
    hr = gdiRenderTarget->GetDC(D2D1_DC_INITIALIZE_MODE_COPY, &hdc);

    BOOL ok = ::BitBlt(bitmapDC.get(), 0, 0, rect.width(), rect.height(), hdc, rect.x(), rect.y(), SRCCOPY);

    RECT updateRect = { 0, 0, 0, 0 };
    hr = gdiRenderTarget->ReleaseDC(&updateRect);

    if (!ok)
        return nullptr;

    memcpy(result->data(), pixels, numBytes.unsafeGet());

    return result;
}

void ImageBufferData::putData(const Uint8ClampedArray& source, AlphaPremultiplication sourceFormat, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, const IntSize& size, bool /* accelerateRendering */, float resolutionScale)
{
    auto platformContext = context->platformContext();
    COMPtr<ID2D1BitmapRenderTarget> renderTarget(Query, platformContext);
    if (!renderTarget)
        return;

    COMPtr<ID2D1Bitmap> bitmap;
    HRESULT hr = renderTarget->GetBitmap(&bitmap);
    ASSERT(SUCCEEDED(hr));

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

    unsigned srcBytesPerRow = 4 * sourceSize.width();
    const uint8_t* srcRows = source.data() + (originy * srcBytesPerRow + originx * 4).unsafeGet();

    auto row = makeUniqueArray<uint8_t>(srcBytesPerRow);

    for (int y = 0; y < height.unsafeGet(); ++y) {
        for (int x = 0; x < width.unsafeGet(); x++) {
            int basex = x * 4;
            uint8_t alpha = srcRows[basex + 3];
            if (sourceFormat == AlphaPremultiplication::Unpremultiplied && alpha != 255) {
                row[basex] = (srcRows[basex] * alpha + 254) / 255;
                row[basex + 1] = (srcRows[basex + 1] * alpha + 254) / 255;
                row[basex + 2] = (srcRows[basex + 2] * alpha + 254) / 255;
                row[basex + 3] = alpha;
            } else
                reinterpret_cast<uint32_t*>(row.get() + basex)[0] = reinterpret_cast<const uint32_t*>(srcRows + basex)[0];
        }

        D2D1_RECT_U dstRect = D2D1::RectU(destPoint.x(), destPoint.y() + y, destPoint.x() + size.width(), destPoint.y() + y + 1);
        hr = bitmap->CopyFromMemory(&dstRect, row.get(), srcBytesPerRow);
        ASSERT(SUCCEEDED(hr));

        srcRows += srcBytesPerRow;
    }
}

} // namespace WebCore

#endif
