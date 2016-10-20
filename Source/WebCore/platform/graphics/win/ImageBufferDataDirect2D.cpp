/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include <d2d1.h>
#include <runtime/JSCInlines.h>
#include <runtime/TypedArrayInlines.h>
#include <runtime/Uint8ClampedArray.h>
#include <wtf/Assertions.h>

namespace WebCore {

RefPtr<Uint8ClampedArray> ImageBufferData::getData(const IntRect& rect, const IntSize& size, bool /* accelerateRendering */, bool /* unmultiplied */, float /* resolutionScale */) const
{
    auto platformContext = context->platformContext();

    Checked<unsigned, RecordOverflow> area = 4 * rect.area();
    if (area.hasOverflowed())
        return nullptr;

    auto result = Uint8ClampedArray::createUninitialized(area.unsafeGet());
    unsigned char* resultData = result ? result->data() : nullptr;
    if (!resultData)
        return nullptr;

    BitmapInfo bitmapInfo = BitmapInfo::createBottomUp(size);

    void* pixels = nullptr;
    auto bitmap = adoptGDIObject(::CreateDIBSection(0, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0));

    HWndDC windowDC(nullptr);
    auto bitmapDC = adoptGDIObject(::CreateCompatibleDC(windowDC));
    HGDIOBJ oldBitmap = ::SelectObject(bitmapDC.get(), bitmap.get());

    HRESULT hr;

    COMPtr<ID2D1GdiInteropRenderTarget> gdiRenderTarget;
    hr = platformContext->QueryInterface(__uuidof(ID2D1GdiInteropRenderTarget), (void**)&gdiRenderTarget);
    if (FAILED(hr))
        return nullptr;

    platformContext->BeginDraw();

    HDC hdc = nullptr;
    hr = gdiRenderTarget->GetDC(D2D1_DC_INITIALIZE_MODE_COPY, &hdc);

    BOOL ok = ::BitBlt(bitmapDC.get(), 0, 0, rect.width(), rect.height(), hdc, rect.x(), rect.y(), SRCCOPY);

    hr = gdiRenderTarget->ReleaseDC(nullptr);

    hr = platformContext->EndDraw();

    if (!ok)
        return nullptr;

    memcpy(result->data(), pixels, 4 * rect.area());

    return result;
}

void ImageBufferData::putData(Uint8ClampedArray*& /* source */, const IntSize& /* sourceSize */, const IntRect& /* sourceRect */, const IntPoint& /* destPoint */, const IntSize& /* size */, bool /* accelerateRendering */, bool /* unmultiplied */, float /* resolutionScale */)
{
    notImplemented();
}

} // namespace WebCore

#endif
