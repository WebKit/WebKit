/*
 * Copyright (C) 2008, 2013 Apple Inc. All rights reserved.
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
#include "GraphicsContext.h"

#include "AffineTransform.h"
#include "DIBPixelData.h"
#include "GraphicsContextCairo.h"
#include "Path.h"
#include "RefPtrCairo.h"

#include <cairo-win32.h>

namespace WebCore {

#if PLATFORM(WIN)
static RefPtr<cairo_t> createCairoContextWithHDC(HDC hdc, bool hasAlpha)
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

    auto context = adoptRef(cairo_create(surface));
    cairo_surface_destroy(surface);

    return context;
}

GraphicsContextCairo::GraphicsContextCairo(HDC dc, bool hasAlpha)
    : GraphicsContextCairo(createCairoContextWithHDC(dc, hasAlpha))
{
}

GraphicsContextCairo::GraphicsContextCairo(GraphicsContextCairo* platformContext)
    : GraphicsContextCairo(platformContext->cr())
{
}
#endif

static void setRGBABitmapAlpha(unsigned char* bytes, size_t length, unsigned char level)
{
    for (size_t i = 0; i < length; i += 4)
        bytes[i + 3] = level;
}

static void drawBitmapToContext(GraphicsContextCairo& platformContext, const DIBPixelData& pixelData, const IntSize& translate)
{
    // Need to make a cairo_surface_t out of the bitmap's pixel buffer and then draw
    // it into our context.
    cairo_surface_t* surface = cairo_image_surface_create_for_data(pixelData.buffer(),
                                                                   CAIRO_FORMAT_ARGB32,
                                                                   pixelData.size().width(),
                                                                   pixelData.size().height(),
                                                                   pixelData.bytesPerRow());

    // Flip the target surface so that when we set the srcImage as
    // the surface it will draw right-side-up.
    cairo_t* cr = platformContext.cr();
    cairo_save(cr);
    cairo_translate(cr, static_cast<double>(translate.width()), static_cast<double>(translate.height()));
    cairo_scale(cr, 1, -1);
    cairo_set_source_surface(cr, surface, 0, 0);

    if (platformContext.layers().size())
        cairo_paint_with_alpha(cr, platformContext.layers().last());
    else
        cairo_paint(cr);
     
    // Delete all our junk.
    cairo_surface_destroy(surface);
    cairo_restore(cr);
}

void GraphicsContext::releaseWindowsContext(HDC hdc, const IntRect& dstRect, bool supportAlphaBlend)
{
    ASSERT(hdc);

    if (dstRect.isEmpty())
        return;

    auto bitmap = adoptGDIObject(static_cast<HBITMAP>(::GetCurrentObject(hdc, OBJ_BITMAP)));

    DIBPixelData pixelData(bitmap.get());
    ASSERT(pixelData.bitsPerPixel() == 32);

    // If this context does not support alpha blending, then it may have
    // been drawn with GDI functions which always set the alpha channel
    // to zero. We need to manually set the bitmap to be fully opaque.
    unsigned char* bytes = reinterpret_cast<unsigned char*>(pixelData.buffer());
    if (!supportAlphaBlend)
        setRGBABitmapAlpha(bytes, pixelData.size().height() * pixelData.bytesPerRow(), 255);

    drawBitmapToContext(*platformContext(), pixelData, IntSize(dstRect.x(), dstRect.height() + dstRect.y()));

    ::DeleteDC(hdc);
}

}
