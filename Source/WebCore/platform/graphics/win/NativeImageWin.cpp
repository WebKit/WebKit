/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "NativeImageWin.h"

#include "HWndDC.h"
#include "NativeImage.h"
#include <windows.h>
#include <wtf/win/GDIObject.h>

#if USE(CAIRO)
#include "GraphicsContextCairo.h"
#endif

namespace WebCore {

void drawNativeImageIntoHBITMAP(const NativeImage& image, HBITMAP bitmap)
{
#if USE(CAIRO)
    BITMAP bitmapInfo;
    if (!::GetObject(bitmap, sizeof(bitmapInfo), &bitmapInfo))
        return;

    HWndDC screenDC(0);
    GDIObject dc = adoptGDIObject(::CreateCompatibleDC(screenDC));
    HGDIOBJ oldBitmap = ::SelectObject(dc.get(), bitmap);
    {
        GraphicsContextCairo context(dc.get());
        context.drawNativeImage(image, FloatRect(0, 0, bitmapInfo.bmWidth, bitmapInfo.bmHeight), { CompositeOperator::Copy });
    }
    ::SelectObject(dc.get(), oldBitmap);
#else
    // FIXME: Implement for Skia.
    UNUSED_PARAM(image);
    UNUSED_PARAM(bitmap);
#endif
}

} // namespace WebCore
