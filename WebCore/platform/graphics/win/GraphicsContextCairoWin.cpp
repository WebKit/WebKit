/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "GraphicsContext.h"

#include "TransformationMatrix.h"
#include "Path.h"

#include <cairo-win32.h>
#include "GraphicsContextPlatformPrivateCairo.h"

using namespace std;

namespace WebCore {

static cairo_t* createCairoContextWithHDC(HDC hdc, bool hasAlpha)
{
    // Put the HDC In advanced mode so it will honor affine transforms.
    SetGraphicsMode(hdc, GM_ADVANCED);
    
    HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(hdc, OBJ_BITMAP));

    BITMAP info;
    GetObject(bitmap, sizeof(info), &info);
    ASSERT(info.bmBitsPixel == 32);

    cairo_surface_t* image = cairo_image_surface_create_for_data((unsigned char*)info.bmBits,
                                               CAIRO_FORMAT_ARGB32,
                                               info.bmWidth,
                                               info.bmHeight,
                                               info.bmWidthBytes);

    cairo_t* context = cairo_create(image);
    cairo_surface_destroy(image);

    return context;
}

static BITMAPINFO bitmapInfoForSize(const IntSize& size)
{
    BITMAPINFO bitmapInfo;
    bitmapInfo.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth         = size.width(); 
    bitmapInfo.bmiHeader.biHeight        = size.height();
    bitmapInfo.bmiHeader.biPlanes        = 1;
    bitmapInfo.bmiHeader.biBitCount      = 32;
    bitmapInfo.bmiHeader.biCompression   = BI_RGB;
    bitmapInfo.bmiHeader.biSizeImage     = 0;
    bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biClrUsed       = 0;
    bitmapInfo.bmiHeader.biClrImportant  = 0;

    return bitmapInfo;
}

static void fillWithClearColor(HBITMAP bitmap)
{
    BITMAP bmpInfo;
    GetObject(bitmap, sizeof(bmpInfo), &bmpInfo);
    int bufferSize = bmpInfo.bmWidthBytes * bmpInfo.bmHeight;
    memset(bmpInfo.bmBits, 0, bufferSize);
}

GraphicsContext::GraphicsContext(HDC dc, bool hasAlpha)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate)
{
    if (dc) {
        m_data->cr = createCairoContextWithHDC(dc, hasAlpha);
        m_data->m_hdc = dc;
    } else {
        setPaintingDisabled(true);
        m_data->cr = 0;
        m_data->m_hdc = 0;
    }

    if (m_data->cr) {
        // Make sure the context starts in sync with our state.
        setPlatformFillColor(fillColor());
        setPlatformStrokeColor(strokeColor());
    }
}

HDC GraphicsContext::getWindowsContext(const IntRect& dstRect, bool supportAlphaBlend, bool mayCreateBitmap)
{
    // FIXME: Should a bitmap be created also when a shadow is set?
    if (mayCreateBitmap && inTransparencyLayer()) {
        if (dstRect.isEmpty())
            return 0;

        // Create a bitmap DC in which to draw.
        BITMAPINFO bitmapInfo = bitmapInfoForSize(dstRect.size());

        void* pixels = 0;
        HBITMAP bitmap = ::CreateDIBSection(NULL, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0);
        if (!bitmap)
            return 0;

        HDC bitmapDC = ::CreateCompatibleDC(m_data->m_hdc);
        ::SelectObject(bitmapDC, bitmap);

        // Fill our buffer with clear if we're going to alpha blend.
        if (supportAlphaBlend)
           fillWithClearColor(bitmap);

        // Make sure we can do world transforms.
        SetGraphicsMode(bitmapDC, GM_ADVANCED);

        // Apply a translation to our context so that the drawing done will be at (0,0) of the bitmap.
        XFORM xform;
        xform.eM11 = 1.0f;
        xform.eM12 = 0.0f;
        xform.eM21 = 0.0f;
        xform.eM22 = 1.0f;
        xform.eDx = -dstRect.x();
        xform.eDy = -dstRect.y();
        ::SetWorldTransform(bitmapDC, &xform);

        return bitmapDC;
    }

    cairo_surface_t* surface = cairo_win32_surface_create(m_data->m_hdc);
    cairo_surface_flush(surface);
    cairo_surface_destroy(surface);

    m_data->save();

    return m_data->m_hdc;
}

void GraphicsContext::releaseWindowsContext(HDC hdc, const IntRect& dstRect, bool supportAlphaBlend, bool mayCreateBitmap)
{
    if (!mayCreateBitmap || !hdc || !inTransparencyLayer()) {
        m_data->restore();
        return;
    }

    if (dstRect.isEmpty())
        return;

    HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(hdc, OBJ_BITMAP));

    BITMAP info;
    GetObject(bitmap, sizeof(info), &info);
    ASSERT(info.bmBitsPixel == 32);

    // Need to make a cairo_surface_t out of the bitmap's pixel buffer and then draw
    // it into our context.
    cairo_surface_t* image = cairo_image_surface_create_for_data((unsigned char*)info.bmBits,
                                            CAIRO_FORMAT_ARGB32,
                                            info.bmWidth,
                                            info.bmHeight,
                                            info.bmWidthBytes);

    // Scale the target surface to the new image size, and flip it
    // so that when we set the srcImage as the surface it will draw
    // right-side-up.
    cairo_translate(m_data->cr, 0, dstRect.height());
    cairo_scale(m_data->cr, dstRect.width(), -dstRect.height());
    cairo_set_source_surface (m_data->cr, image, dstRect.x(), dstRect.y());

    if (m_data->layers.size())
        cairo_paint_with_alpha(m_data->cr, m_data->layers.last());
    else
        cairo_paint(m_data->cr);
     
    // Delete all our junk.
    cairo_surface_destroy(image);
    ::DeleteDC(hdc);
    ::DeleteObject(bitmap);
}

void GraphicsContextPlatformPrivate::concatCTM(const TransformationMatrix& transform)
{
    const cairo_matrix_t* matrix = reinterpret_cast<const cairo_matrix_t*>(&transform);

    XFORM xform;
    xform.eM11 = matrix->xx;
    xform.eM12 = matrix->xy;
    xform.eM21 = matrix->yx;
    xform.eM22 = matrix->yy;
    xform.eDx = matrix->x0;
    xform.eDy = matrix->y0;

    ModifyWorldTransform(m_hdc, &xform, MWT_LEFTMULTIPLY);
}

void GraphicsContextPlatformPrivate::syncContext(PlatformGraphicsContext* cr)
{
    if (!cr)
       return;

    cairo_surface_t* surface = cairo_get_target(cr);
    m_hdc = cairo_win32_surface_get_dc(surface);   

    SetGraphicsMode(m_hdc, GM_ADVANCED); // We need this call for themes to honor world transforms.
}

}
