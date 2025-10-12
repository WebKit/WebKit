/*
 * Copyright (C) 2008-2024 Apple Inc. All rights reserved.
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
#include "ImageAdapter.h"

#if PLATFORM(WIN) && USE(CAIRO)

#include "GraphicsContextCairo.h"
#include <cairo-win32.h>

namespace WebCore {

RefPtr<NativeImage> ImageAdapter::nativeImageOfHBITMAP(HBITMAP bmp)
{
    DIBSECTION dibSection;
    if (!GetObject(bmp, sizeof(DIBSECTION), &dibSection))
        return nullptr;

    ASSERT(dibSection.dsBm.bmBitsPixel == 32);
    if (dibSection.dsBm.bmBitsPixel != 32)
        return nullptr;

    ASSERT(dibSection.dsBm.bmBits);
    if (!dibSection.dsBm.bmBits)
        return nullptr;

    auto surface = adoptRef(cairo_win32_surface_create_with_dib(CAIRO_FORMAT_ARGB32, dibSection.dsBm.bmWidth, dibSection.dsBm.bmHeight));
    return NativeImage::create(WTFMove(surface));
}

bool ImageAdapter::getHBITMAPOfSize(HBITMAP bmp, const IntSize* size)
{
    ASSERT(bmp);

    BITMAP bmpInfo;
    GetObject(bmp, sizeof(BITMAP), &bmpInfo);

    // If this is a 32bpp bitmap, which it always should be, we'll clear it so alpha-wise it will be visible
    if (bmpInfo.bmBitsPixel == 32 && bmpInfo.bmBits) {
        int bufferSize = bmpInfo.bmWidthBytes * bmpInfo.bmHeight;
        memset(bmpInfo.bmBits, 255, bufferSize);
    }

    unsigned char* bmpdata = (unsigned char*)bmpInfo.bmBits + bmpInfo.bmWidthBytes * (bmpInfo.bmHeight - 1);
    auto platformImage = adoptRef(cairo_image_surface_create_for_data(bmpdata, CAIRO_FORMAT_ARGB32, bmpInfo.bmWidth, bmpInfo.bmHeight, -bmpInfo.bmWidthBytes));

    GraphicsContextCairo gc(platformImage.get());

    auto imageSize = image().size();
    auto destinationRect = FloatRect(0.0f, 0.0f, bmpInfo.bmWidth, bmpInfo.bmHeight);

    if (auto nativeImage = size ? nativeImageOfSize(*size) : nullptr) {
        auto sourceRect = FloatRect { { }, *size };
        gc.drawNativeImage(*nativeImage, destinationRect, sourceRect, { CompositeOperator::Copy });
        return true;
    }

    auto sourceRect = FloatRect { { }, imageSize };
    gc.drawImage(image(), destinationRect, sourceRect, { CompositeOperator::Copy });
    return true;
}

} // namespace WebCore

#endif // PLATFORM(WIN) && USE(CAIRO)
