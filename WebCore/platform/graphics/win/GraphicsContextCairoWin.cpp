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

#include "AffineTransform.h"
#include "Path.h"

#include <cairo-win32.h>
#include "GraphicsContextPlatformPrivateCairo.h"

using namespace std;

namespace WebCore {

static cairo_t* createCairoContextWithHDC(HDC hdc, bool hasAlpha)
{
    // Put the HDC In advanced mode so it will honor affine transforms.
    SetGraphicsMode(hdc, GM_ADVANCED);

    cairo_surface_t* surface = 0;

    HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(hdc, OBJ_BITMAP));

    BITMAP info;
    if (!GetObject(bitmap, sizeof(info), &info))
        surface = cairo_win32_surface_create(hdc);
    else {
        ASSERT(info.bmBitsPixel == 32);

        surface = cairo_image_surface_create_for_data((unsigned char*)info.bmBits,
                                               CAIRO_FORMAT_ARGB32,
                                               info.bmWidth,
                                               info.bmHeight,
                                               info.bmWidthBytes);
    }

    cairo_t* context = cairo_create(surface);
    cairo_surface_destroy(surface);

    return context;
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
        setPlatformFillColor(fillColor(), fillColorSpace());
        setPlatformStrokeColor(strokeColor(), strokeColorSpace());
    }
}

static void setRGBABitmapAlpha(unsigned char* bytes, size_t length, unsigned char level)
{
    for (size_t i = 0; i < length; i += 4)
        bytes[i + 3] = level;
}

void GraphicsContext::releaseWindowsContext(HDC hdc, const IntRect& dstRect, bool supportAlphaBlend, bool mayCreateBitmap)
{
    bool createdBitmap = mayCreateBitmap && (!m_data->m_hdc || inTransparencyLayer());
    if (!hdc || !createdBitmap) {
        m_data->restore();
        return;
    }

    if (dstRect.isEmpty())
        return;

    HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(hdc, OBJ_BITMAP));

    BITMAP info;
    GetObject(bitmap, sizeof(info), &info);
    ASSERT(info.bmBitsPixel == 32);

    // If this context does not support alpha blending, then it may have
    // been drawn with GDI functions which always set the alpha channel
    // to zero. We need to manually set the bitmap to be fully opaque.
    unsigned char* bytes = reinterpret_cast<unsigned char*>(info.bmBits);
    if (!supportAlphaBlend)
        setRGBABitmapAlpha(bytes, info.bmHeight * info.bmWidthBytes, 255);

    // Need to make a cairo_surface_t out of the bitmap's pixel buffer and then draw
    // it into our context.
    cairo_surface_t* image = cairo_image_surface_create_for_data(bytes,
                                            CAIRO_FORMAT_ARGB32,
                                            info.bmWidth,
                                            info.bmHeight,
                                            info.bmWidthBytes);

    // Scale the target surface to the new image size, and flip it
    // so that when we set the srcImage as the surface it will draw
    // right-side-up.
    cairo_save(m_data->cr);
    cairo_translate(m_data->cr, dstRect.x(), dstRect.height() + dstRect.y());
    cairo_scale(m_data->cr, 1.0, -1.0);
    cairo_set_source_surface(m_data->cr, image, 0, 0);

    if (m_data->layers.size())
        cairo_paint_with_alpha(m_data->cr, m_data->layers.last());
    else
        cairo_paint(m_data->cr);
     
    // Delete all our junk.
    cairo_surface_destroy(image);
    ::DeleteDC(hdc);
    ::DeleteObject(bitmap);
    cairo_restore(m_data->cr);
}

void GraphicsContextPlatformPrivate::syncContext(PlatformGraphicsContext* cr)
{
    if (!cr)
       return;

    cairo_surface_t* surface = cairo_get_target(cr);
    m_hdc = cairo_win32_surface_get_dc(surface);   

    SetGraphicsMode(m_hdc, GM_ADVANCED); // We need this call for themes to honor world transforms.
}

void GraphicsContextPlatformPrivate::flush()
{
    cairo_surface_t* surface = cairo_win32_surface_create(m_hdc);
    cairo_surface_flush(surface);
    cairo_surface_destroy(surface);
}

}
