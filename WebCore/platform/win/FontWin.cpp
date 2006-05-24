/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "Font.h"

#include <cairo-win32.h>
#include "FontData.h"
#include "FontFallbackList.h"
#include "GraphicsContext.h"
#include "IntRect.h"

namespace WebCore {

static void notImplemented() { puts("Not yet implemented"); _CrtDbgBreak(); }

void Font::drawGlyphs(GraphicsContext* graphicsContext, const FontData* font, const GlyphBuffer& glyphBuffer, 
                      int from, int numGlyphs, const FloatPoint& point) const
{
    cairo_t* context = graphicsContext->platformContext();

    // Set the text color to use for drawing.
    float red, green, blue, alpha;
    Color penColor = graphicsContext->pen().color();
    penColor.getRGBA(red, green, blue, alpha);
    cairo_set_source_rgba(context, red, green, blue, alpha);
    
    cairo_surface_t* surface = cairo_get_target(context);
    HDC dc = cairo_win32_surface_get_dc(surface);

    // Cairo alters this but we need to preserve it.
    XFORM savedxform;
    GetWorldTransform(dc, &savedxform);

    // Select our font face/size.
    cairo_set_font_face(context, font->m_font.fontFace());
    cairo_set_font_size(context, font->m_font.size());

    SaveDC(dc);

    // Select the scaled font.
    cairo_scaled_font_t* scaledFont = font->m_font.scaledFont();
    cairo_win32_scaled_font_select_font(scaledFont, dc);

    // Restore the original transform.
    SetWorldTransform(dc, &savedxform);

    GlyphBufferGlyph* glyphs = glyphBuffer.glyphs(from);

    float offset = point.x();

    for (unsigned i = 0; i < numGlyphs; i++) {
        glyphs[i].x = offset;
        glyphs[i].y = point.y();
        offset += glyphBuffer.advanceAt(from + i);
    }

    cairo_show_glyphs(context, glyphs, numGlyphs);

    cairo_win32_scaled_font_done_font(scaledFont);
    RestoreDC(dc, -1);
}

FloatRect Font::selectionRectForComplexText(const TextRun& run, const TextStyle& style, const IntPoint& point, int h) const
{
    notImplemented();
    return FloatRect();
}

void Font::drawComplexText(GraphicsContext* graphicsContext, const TextRun& run, const TextStyle& style, const FloatPoint& point) const
{
    notImplemented();
}

float Font::floatWidthForComplexText(const TextRun& run, const TextStyle& style) const
{
    notImplemented();
    return 0;
}

int Font::offsetForPositionForComplexText(const TextRun& run, const TextStyle& style, int x, bool includePartialGlyphs) const
{
    notImplemented();
    return 0;
}

}
