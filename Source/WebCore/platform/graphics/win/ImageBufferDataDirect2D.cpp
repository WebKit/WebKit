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
    auto numBytes = rect.area<RecordOverflow>() * 4;
    if (numBytes.hasOverflowed())
        return nullptr;

    auto result = Uint8ClampedArray::tryCreateUninitialized(numBytes.unsafeGet());
    unsigned char* resultData = result ? result->data() : nullptr;
    if (!resultData)
        return nullptr;

    WICRect rcLock = { 0, 0, rect.width(), rect.height() };

    // We cannot access the data backing an IWICBitmap while an active draw session is open.
    context->endDraw();

    COMPtr<IWICBitmapLock> bitmapDataLock;
    HRESULT hr = bitmapSource->Lock(&rcLock, WICBitmapLockRead, &bitmapDataLock);
    if (SUCCEEDED(hr)) {
        UINT bufferSize = 0;
        WICInProcPointer dataPtr = nullptr;
        hr = bitmapDataLock->GetDataPointer(&bufferSize, &dataPtr);
        if (SUCCEEDED(hr))
            memcpy(result->data(), reinterpret_cast<char*>(dataPtr), numBytes.unsafeGet());
    }

    // Once we are done modifying the data, unlock the bitmap
    bitmapDataLock = nullptr;

    context->beginDraw();

    return result;
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

    // We cannot access the data backing an IWICBitmap while an active draw session is open.
    context->endDraw();

    WICRect rcLock = { 0, 0, sourceSize.width(), sourceSize.height() };

    COMPtr<IWICBitmapLock> bitmapDataLock;
    HRESULT hr = bitmapSource->Lock(&rcLock, WICBitmapLockWrite, &bitmapDataLock);
    if (!SUCCEEDED(hr))
        return;

    UINT stride = 0;
    hr = bitmapDataLock->GetStride(&stride);
    if (!SUCCEEDED(hr))
        return;

    UINT bufferSize = 0;
    WICInProcPointer dataPtr = nullptr;
    hr = bitmapDataLock->GetDataPointer(&bufferSize, &dataPtr);
    if (!SUCCEEDED(hr))
        return;

    ASSERT(bufferSize == source.byteLength());

    unsigned srcBytesPerRow = 4 * sourceSize.width();

    ASSERT(srcBytesPerRow == stride);

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

        memcpy(reinterpret_cast<char*>(dataPtr + y * srcBytesPerRow), row.get(), srcBytesPerRow);

        srcRows += srcBytesPerRow;
    }

    // Once we are done modifying the data, unlock the bitmap
    bitmapDataLock = nullptr;

    context->beginDraw();
}

} // namespace WebCore

#endif
