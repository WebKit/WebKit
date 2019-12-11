/*
 * Copyright (C) 2007, 2013 Apple, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(CG)
#include "PixelDumpSupportCG.h"
#elif USE(CAIRO)
#include "PixelDumpSupportCairo.h"
#elif USE(DIRECT2D)
#include "PixelDumpSupportDirect2D.h"
#endif

#include "DumpRenderTree.h"

#if USE(CG)
// Note: Must be included *after* DumpRenderTree.h to avoid compile error.
#include <CoreGraphics/CGBitmapContext.h>
#endif

#include <wtf/Assertions.h>
#include <wtf/RetainPtr.h>
#include <wtf/win/GDIObject.h>

static void makeAlphaChannelOpaque(void* argbBits, LONG width, LONG height)
{
    unsigned* pixel = static_cast<unsigned*>(argbBits);
    for (LONG row = 0; row < height; ++row) {
        for (LONG column = 0; column < width; ++column)
            *pixel++ |= 0xff000000;
    }
}

RefPtr<BitmapContext> createBitmapContextFromWebView(bool onscreen, bool incrementalRepaint, bool sweepHorizontally, bool drawSelectionRect)
{
    RECT frame;
    if (!GetWindowRect(webViewWindow, &frame))
        return nullptr;

    BITMAPINFO bmp { };
    bmp.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmp.bmiHeader.biWidth = frame.right - frame.left;
    bmp.bmiHeader.biHeight = -(frame.bottom - frame.top);
    bmp.bmiHeader.biPlanes = 1;
    bmp.bmiHeader.biBitCount = 32;
    bmp.bmiHeader.biCompression = BI_RGB;

    void* bits = 0;
    HBITMAP bitmap = ::CreateDIBSection(0, &bmp, DIB_RGB_COLORS, &bits, 0, 0);
    if (!bitmap)
        return nullptr;

    auto memoryDC = adoptGDIObject(::CreateCompatibleDC(0));
    ::SelectObject(memoryDC.get(), bitmap);
    SendMessage(webViewWindow, WM_PRINT, reinterpret_cast<WPARAM>(memoryDC.get()), PRF_CLIENT | PRF_CHILDREN | PRF_OWNED);

    BITMAP info { };
    GetObject(bitmap, sizeof(info), &info);
    ASSERT(info.bmBitsPixel == 32);

    // We create a context that has an alpha channel below so that the PNGs we generate will also
    // have an alpha channel. But WM_PRINT doesn't necessarily write anything into the alpha
    // channel, so we set the alpha channel to constant full opacity to make sure the resulting image is opaque.
    makeAlphaChannelOpaque(info.bmBits, info.bmWidth, info.bmHeight);

#if USE(CG)
    RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
    CGContextRef context = CGBitmapContextCreate(info.bmBits, info.bmWidth, info.bmHeight, 8,
                                                info.bmWidthBytes, colorSpace.get(), kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
#elif USE(CAIRO) 
    cairo_surface_t* image = cairo_image_surface_create_for_data((unsigned char*)info.bmBits, CAIRO_FORMAT_ARGB32, 
                                                      info.bmWidth, info.bmHeight, info.bmWidthBytes); 
    cairo_t* context = cairo_create(image); 
    cairo_surface_destroy(image);
#elif USE(DIRECT2D)
    auto targetProperties = D2D1::RenderTargetProperties();
    targetProperties.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);

    COMPtr<ID2D1DCRenderTarget> renderTarget;
    HRESULT hr = pixelDumpSystemFactory()->CreateDCRenderTarget(&targetProperties, &renderTarget);
    if (!SUCCEEDED(hr))
        return nullptr;

    hr = renderTarget->BindDC(memoryDC.get(), &frame);
    if (!SUCCEEDED(hr))
        return nullptr;
    auto context = renderTarget.get();
#endif 

    return BitmapContext::createByAdoptingBitmapAndContext(bitmap, context);
}
